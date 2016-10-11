
// Underwater Acoustic CommunicationDlg.cpp : implementation file
//

#include "stdafx.h"
#include "Underwater Acoustic Communication.h"
#include "Underwater Acoustic CommunicationDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// XBee AT commands
#define ATDH						"ATDH"
#define ATDL						"ATDL"
#define ATID						"ATID"
#define ATCM						"ATCM"
#define ATPL						"ATPL"
// Power up/down at command
#define PWRRESERVED_CMD				"D0"
#define PWRLIGHT_CMD				"D1"
#define PWRSELECTS_CMD				"D2"
#define PWRBBB_CMD					"D3"
// Color for LED flashing in UI
#define COM_LEDGRAYON				RGB(0, 250, 0)
#define COM_LEDGRAYOFF				RGB(0, 150, 0)
// Return value code
#define CHECK_ERR					1
#define CHECK_OK					0
// File format for downloading
#define EXECUTE_FORMAT				1
#define MODEM_FORMAT				2

// The main commands
#define COMMON_CMD					0x01
#define DOWNLOAD_CMD				0x02
#define UPLOAD_CMD					0x03
#define ACK_CMD						0x04
#define MONITOR_CMD					0x05
// The sub-commands
#define TIMESYNC_CMD				0x11
#define RUNSTOP_CMD					0x12
#define MODEMADDR_CMD				0x13
#define MODEMRUN_CMD				0x01
#define MODEMSTOP_CMD				0x02

#define DOWNLOAD_CMDLEN				20
#define PACKET_CMDLEN				128
#define ACK_CMDLEN					20
#define TIMESYNC_CMDLEN				26
#define RUNSTOP_CMDLEN				28

#define XBEE_MYADDR_LEN				2
#define XBEE_SHADDR_LEN				4
#define XBEE_SLADDR_LEN				4
#define MONITOR_LENGTH				100

// Packet size for downloading/uploading
#define PACKETSIZE					116

#define TIMEOUT_CNT					40


#define MAX_NUM_OF_NODES		128

#define SYNC_OK						0
#define SYNC_ERR					1
#define MODEM_RUN					1
#define MODEM_STOP					0
#define MONITOR_RUN					0
#define MONITOR_STOP				1
#define POWER_UP					0x5
#define POWER_DOWN					0x4

#define TREE_MODEMADDR_CODE			101
#define TREE_TIMESYNC_CODE			102

// State Machine for thread running....
#define ST_THREAD_STOP				0
#define ST_THREAD_DOWNLOAD			1
#define ST_THREAD_SCAN				2
#define ST_THREAD_MONITOR			3
#define ST_THREAD_WAKEUP			4

// State Machine for application
#define ST_APP_ZERO					1
#define ST_APP_CONNECTED			2
#define ST_APP_NETWORKSCAN			3


// XBee info from each Node
struct XBeeInfo {
	char myAddr[2];
	char shAddr[4];
	char slAddr[4];
	char nIdenti[20];
	char devType;
	char profileID[2];
	char manufactID[2];
	char rssi[2];
};

// Node info from BeagleBone Black
struct ModemInfo {
	char addr;
	char timeSync;
	char runstop;
	char listOffset;
};

// Monitor data format from each node
struct MonitorInfo {
	char addr;
	char reserved_1;
	char reserved_2;
	char reserved_3;
	char data[96];
};

// Power control info from XBee for each node
struct PowerInfo {
	char light;
	char selects;
	char bbb;
	char reserved;
};

static struct XBeeInfo g_xbeeInfo[MAX_NUM_OF_NODES];
static struct ModemInfo g_modemInfo[MAX_NUM_OF_NODES];
static struct MonitorInfo g_monitorInfo[MAX_NUM_OF_NODES];
static struct PowerInfo g_powerInfo[MAX_NUM_OF_NODES];
static int g_xbeeOffset;
static int g_endScan;
static int g_endMonitor;
static int g_stThread;

CRITICAL_SECTION g_csThread;

// CAboutDlg dialog used for App About

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// Dialog Data
	enum { IDD = IDD_ABOUTBOX };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
protected:
	DECLARE_MESSAGE_MAP()
public:
};

CAboutDlg::CAboutDlg() : CDialogEx(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CUnderwaterAcousticCommunicationDlg dialog



CUnderwaterAcousticCommunicationDlg::CUnderwaterAcousticCommunicationDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(CUnderwaterAcousticCommunicationDlg::IDD, pParent)
	, m_xbeesh(_T(""))
	, m_xbeesl(_T(""))
	, m_xbeedh(_T(""))
	, m_xbeedl(_T(""))
	, m_xbeeid(_T(""))
	, m_xbeecm(_T(""))
	, m_scantime(0)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);

	g_xbeeOffset = 0xFFFFFFFF;
	m_stApp = ST_APP_ZERO;
	g_stThread = ST_THREAD_STOP;
}

void CUnderwaterAcousticCommunicationDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_COMBO_COMPORT, m_comport);
	DDX_Control(pDX, IDC_COMBO_BAUDRATE, m_baudrate);
	DDX_Control(pDX, IDC_COMBO_XBEEMODE, m_xbeeMode);
	DDX_Control(pDX, IDC_COMBO_RFPOWER, m_rfPower);
	DDX_Control(pDX, IDC_COMBO_DOWNLOAD_FILETYPE, m_dlType);
	DDX_Control(pDX, IDC_COMBO_UPLOAD_FILETYPE, m_ulType);
	DDX_Control(pDX, IDC_PROGRESS_TRANSFER, m_progress);
	DDX_Control(pDX, IDC_LIST_ADDRESSBOOK, m_List);
	DDX_Control(pDX, IDC_BUTTON_CONNECT, m_connect);
	DDX_Control(pDX, IDC_DYN_TXLED, m_txLed);
	DDX_Control(pDX, IDC_DYN_RXLED, m_rxLed);
	DDX_Text(pDX, IDC_EDIT_SH, m_xbeesh);
	DDX_Text(pDX, IDC_EDIT_SL, m_xbeesl);
	DDX_Text(pDX, IDC_EDIT_DH, m_xbeedh);
	DDX_Text(pDX, IDC_EDIT_DL, m_xbeedl);
	DDX_Text(pDX, IDC_EDIT_ID, m_xbeeid);
	DDX_Text(pDX, IDC_EDIT_CH, m_xbeecm);
	DDX_Control(pDX, IDC_EDIT_FILEPATH, m_filepath);
	DDX_Control(pDX, IDC_TREE_UAN, m_uan);
	DDX_Text(pDX, IDC_EDIT_SCANTIME, m_scantime);
	DDX_Control(pDX, IDC_CHECK_LIGHT, m_checkLight);
	DDX_Control(pDX, IDC_CHECK_Selects, m_checkSelects);
	DDX_Control(pDX, IDC_CHECK_BEAGLEBONE, m_checkBBB);
	DDX_Control(pDX, IDC_CHECK_RESERVED, m_checkReserved);
}

BEGIN_MESSAGE_MAP(CUnderwaterAcousticCommunicationDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BUTTON_CONNECT, &CUnderwaterAcousticCommunicationDlg::OnBnClickedButtonConnect)
	ON_CBN_SELCHANGE(IDC_COMBO_XBEEMODE, &CUnderwaterAcousticCommunicationDlg::OnCbnSelchangeComboXbeemode)
	ON_BN_CLICKED(IDC_BUTTON_READINFO2, &CUnderwaterAcousticCommunicationDlg::OnBnClickedButtonReadinfo2)
	ON_BN_CLICKED(IDC_BUTTON_WRITEINFO, &CUnderwaterAcousticCommunicationDlg::OnBnClickedButtonWriteinfo)
	ON_BN_CLICKED(IDC_BUTTON_TIMESYNC, &CUnderwaterAcousticCommunicationDlg::OnBnClickedButtonTimesync)
	ON_BN_CLICKED(IDC_BUTTON_DOWNLOAD, &CUnderwaterAcousticCommunicationDlg::OnBnClickedButtonDownload)
	ON_BN_CLICKED(IDC_BUTTON_SCAN, &CUnderwaterAcousticCommunicationDlg::OnBnClickedButtonScan)
	ON_NOTIFY(TVN_BEGINLABELEDIT, IDC_TREE_UAN, &CUnderwaterAcousticCommunicationDlg::OnBeginlabeleditTreeUan)
	ON_NOTIFY(TVN_ENDLABELEDIT, IDC_TREE_UAN, &CUnderwaterAcousticCommunicationDlg::OnEndlabeleditTreeUan)
	ON_BN_CLICKED(IDC_BUTTON_UPDATECONFIGURE, &CUnderwaterAcousticCommunicationDlg::OnBnClickedButtonUpdateconfigure)
	ON_NOTIFY(TVN_SELCHANGED, IDC_TREE_UAN, &CUnderwaterAcousticCommunicationDlg::OnTvnSelchangedTreeUan)
	ON_NOTIFY(NM_RCLICK, IDC_TREE_UAN, &CUnderwaterAcousticCommunicationDlg::OnRclickTreeUan)
	ON_COMMAND(ID_TIMESYCHRONIZE_RUN, &CUnderwaterAcousticCommunicationDlg::OnTimesychronizeRun)
	ON_COMMAND(ID_MENU_RUN, CUnderwaterAcousticCommunicationDlg::OnMenuRun)
	ON_COMMAND(ID_MENU_STOP, CUnderwaterAcousticCommunicationDlg::OnMenuStop)
	ON_BN_CLICKED(IDC_MONITORSTART, &CUnderwaterAcousticCommunicationDlg::OnBnClickedMonitorstart)
	ON_BN_CLICKED(IDC_BUTTON_STOPSCAN, &CUnderwaterAcousticCommunicationDlg::OnBnClickedButtonStopscan)
	ON_NOTIFY(NM_CUSTOMDRAW, IDC_TREE_UAN, &CUnderwaterAcousticCommunicationDlg::OnNMCustomdrawTreeUan)
	ON_BN_CLICKED(IDC_MONITORSTOP, &CUnderwaterAcousticCommunicationDlg::OnBnClickedMonitorstop)
	ON_BN_CLICKED(IDC_CHECK_LIGHT, &CUnderwaterAcousticCommunicationDlg::OnClickedCheckLight)
	ON_BN_CLICKED(IDC_CHECK_Selects, &CUnderwaterAcousticCommunicationDlg::OnClickedCheckSelects)
	ON_BN_CLICKED(IDC_CHECK_BEAGLEBONE, &CUnderwaterAcousticCommunicationDlg::OnClickedCheckBeaglebone)
	ON_BN_CLICKED(IDC_CHECK_RESERVED, &CUnderwaterAcousticCommunicationDlg::OnClickedCheckReserved)
	ON_BN_CLICKED(IDC_BUTTON_REMOTEWAKEUP, &CUnderwaterAcousticCommunicationDlg::OnBnClickedButtonRemotewakeup)
	ON_BN_CLICKED(IDC_BUTTON_REMOTSLEEP, &CUnderwaterAcousticCommunicationDlg::OnBnClickedButtonRemotsleep)
END_MESSAGE_MAP()



void CUnderwaterAcousticCommunicationDlg::AnsiToUnicode(WCHAR* str, const char* szStr)
{
	int nLen = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, szStr, -1, NULL, 0);
	MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, szStr, -1, str, nLen);

}


void CUnderwaterAcousticCommunicationDlg::UnicodeToAnsi(wchar_t* wszString, char* szAnsi)
{
	int ansiLen = ::WideCharToMultiByte(CP_ACP, NULL, wszString, wcslen(wszString), NULL, 0, NULL, NULL);
	::WideCharToMultiByte(CP_ACP, NULL, wszString, wcslen(wszString), szAnsi, ansiLen, NULL, NULL);
}

int CUnderwaterAcousticCommunicationDlg::XbeeSendMsg(char* cmd, CString data, int data_length)
{
	int length, err;
	WCHAR wch[128];
	char rbuffer[8], message[32];


	memset(rbuffer, 0, sizeof(rbuffer));
	memset(message, 0, sizeof(message));
	length = data.GetLength();
	memcpy(message, cmd, 4);
	wcscpy_s(wch, CT2CW(data));
	UnicodeToAnsi(wch, (message + 4));
	message[4 + length] = '\r';
	m_txLed.SetOnOff(TRUE);
	m_serialport.Write(message, 5 + length);
	Sleep(40);
	m_txLed.SetOnOff(FALSE);
	m_rxLed.SetOnOff(TRUE);
	Sleep(20);
	m_serialport.Read(rbuffer, 3);
	m_rxLed.SetOnOff(FALSE);
	if ((rbuffer[0] != 'O') || (rbuffer[1] != 'K') || (rbuffer[2] != '\r')) {
		err = 1;
	}
	else {
		err = 0;
	}

	return err;
}


int CUnderwaterAcousticCommunicationDlg::XbeeAddrConverter(char* SH, char* SL, char* addrConverted)
{
	int i;
	char low4bit, high4bit;
	
	for (i = 0; i < 4; i++) {
		if ((SH[i * 2] >= 0x30) && (SH[i * 2] <= 0x39)) {
			high4bit = ((SH[i * 2] - 0x30) << 4) & 0xF0;
		}
		else if ((SH[i * 2] >= 0x61) && (SH[i * 2] <= 0x66)) {
			high4bit = ((SH[i * 2] - 0x57) << 4) & 0xF0;
		}
		else {
			high4bit = ((SH[i * 2] - 0x37) << 4) & 0xF0;
		}
		if ((SH[i * 2 + 1] >= 0x30) && (SH[i * 2 + 1] <= 0x39)) {
			low4bit = (SH[i * 2 + 1] - 0x30) & 0x0F;
		}
		else if ((SH[i * 2 + 1] >= 0x61) && (SH[i * 2 + 1] <= 0x66)) {
			low4bit = (SH[i * 2 + 1] - 0x57) & 0x0F;
		}
		else {
			low4bit = (SH[i * 2 + 1] - 0x37) & 0x0F;
		}
		addrConverted[i] = high4bit + low4bit;
	}
	for (i = 0; i < 4; i++) {
		if ((SL[i * 2] >= 0x30) && (SL[i * 2] <= 0x39)) {
			high4bit = ((SL[i * 2] - 0x30) << 4) & 0xF0;
		}
		else if ((SL[i * 2] >= 0x61) && (SL[i * 2] <= 0x66)) {
			high4bit = ((SL[i * 2] - 0x57) << 4) & 0xF0;
		}
		else {
			high4bit = ((SL[i * 2] - 0x37) << 4) & 0xF0;
		}
		if ((SL[i * 2 + 1] >= 0x30) && (SL[i * 2 + 1] <= 0x39)) {
			low4bit = (SL[i * 2 + 1] - 0x30) & 0x0F;
		}
		else if ((SL[i * 2 + 1] >= 0x61) && (SL[i * 2 + 1] <= 0x66)) {
			low4bit = (SL[i * 2 + 1] - 0x57) & 0x0F;
		}
		else {
			low4bit = (SL[i * 2 + 1] - 0x37) & 0x0F;
		}
		addrConverted[4 + i] = high4bit + low4bit;
	}

	return 0;
}


int CUnderwaterAcousticCommunicationDlg::RemoteXbeeMsg(char* destinationAddr, char* atCmd, char* para, int paraLen)
{
	int i, num, err;
	char sum, msg[64], rbuffer[64];

	err = 1;
	memset(msg, 0, sizeof(msg));
	// Header
	msg[0] = 0x7E;
	// Length
	msg[1] = 0x00;
	msg[2] = 0x0F + paraLen;
	// Frame Type
	msg[3] = 0x17;
	// Frame ID
	msg[4] = 0x01;
	// Destination Address
	memcpy((msg + 5), destinationAddr, 8);
	// Reserved
	msg[13] = (char)0xFF;
	msg[14] = (char)0xFE;
	// Remote Command Option
	msg[15] = 0x02;
	// AT Command
	memcpy((msg + 16), atCmd, 2);
	// Command Parameter
	memcpy((msg + 18), para, paraLen);
	// Checksum
	sum = 0;
	for (i = 0; i < (0x0F + paraLen); i++) {
		sum += msg[3 + i];
	}
	msg[18 + paraLen] = 0xFF - sum;
	// Send data...
	m_txLed.SetOnOff(TRUE);
	m_serialport.Write(msg, (0x0F + paraLen + 4));
	Sleep(50);
	m_txLed.SetOnOff(FALSE);
	memset(rbuffer, 0, sizeof(rbuffer));
	m_rxLed.SetOnOff(TRUE);
	Sleep(50);
	num = m_serialport.Read(rbuffer, 64);
	if ((num == 19) && (rbuffer[17] == 0x00)) {
		err = 0;
	}
	m_rxLed.SetOnOff(FALSE);


	return err;
}


int CUnderwaterAcousticCommunicationDlg::LocalXbeeMsg(char* atCmd, char* para, int paraLen)
{
	int i, num, err;
	char sum, msg[64], rbuffer[64];

	err = 1;
	memset(msg, 0, sizeof(msg));
	// Header
	msg[0] = 0x7E;
	// Length
	msg[1] = 0x00;
	msg[2] = 0x04 + paraLen;
	// Frame Type
	msg[3] = 0x08;
	// Frame ID
	msg[4] = 0x01;
	// AT Command
	memcpy((msg + 5), atCmd, 2);
	// Command Parameter
	memcpy((msg + 7), para, paraLen);
	// Checksum
	sum = 0;
	for (i = 0; i < (0x04 + paraLen); i++) {
		sum += msg[3 + i];
	}
	msg[7 + paraLen] = 0xFF - sum;
	// Send data...
	m_txLed.SetOnOff(TRUE);
	m_serialport.Write(msg, (0x04 + paraLen + 4));
	Sleep(50);
	m_txLed.SetOnOff(FALSE);
	memset(rbuffer, 0, sizeof(rbuffer));
	m_rxLed.SetOnOff(TRUE);
	Sleep(50);
	num = m_serialport.Read(rbuffer, 16);
	if ((num == 9) && (rbuffer[7] == 0x00)) {
		err = 0;
	}
	m_rxLed.SetOnOff(FALSE);


	return err;
}


int CUnderwaterAcousticCommunicationDlg::TxXbeeMsg(char* data, int len, char* dAddr, char* recvBuf)
{
	int i, cnt, err;
	char sum, msg[256], rbuffer[512];

	err = 1;
	memset(msg, 0, sizeof(msg));
	// Header
	msg[0] = 0x7E;
	// Length
	msg[1] = 0x00;
	msg[2] = 14 + len;
	// Frame Type
	msg[3] = 0x10;
	// Frame ID
	msg[4] = 0x00;
	// Address of remote XBee
	memcpy((msg + 5), dAddr, 8);
	// Reserved
	msg[13] = (char)0xFF;
	msg[14] = (char)0xFE;
	// Broadcast radius
	msg[15] = 0x0;
	// Transmit option
	msg[16] = 0x0;
	// Data
	memcpy((msg + 17), data, len);
	// Checksum
	sum = 0;
	for (i = 0; i < (14 + len); i++) {
		sum += msg[3 + i];
	}
	msg[17 + len] = 0xFF - sum;
	// Send data...
	m_txLed.SetOnOff(TRUE);
	m_serialport.Write(msg, (14 + len + 4));
	Sleep(50);
	m_txLed.SetOnOff(FALSE);
	memset(rbuffer, 0, sizeof(rbuffer));
	cnt = 0;
	do {
		m_rxLed.SetOnOff(TRUE);
		Sleep(50);
		m_serialport.Read(rbuffer, 1);
		m_rxLed.SetOnOff(FALSE);
		Sleep(30);
	} while ((rbuffer[0] != 0x7E) && (++ cnt < 3));
	if (rbuffer[0] == 0x7E) {
		m_rxLed.SetOnOff(TRUE);
		Sleep(10);
		m_serialport.Read((rbuffer + 1), 2);
		Sleep(20);
		len = (((int)rbuffer[1] << 8) & 0x0000FF00) + ((int)rbuffer[2] & 0x000000FF);
		m_serialport.Read((rbuffer + 3), len + 1);
		memcpy(recvBuf, (rbuffer + 15), len + 4 - 16);
		err = 0;
		m_rxLed.SetOnOff(FALSE);
	}


	return err;
}


int CUnderwaterAcousticCommunicationDlg::XbeeConfigure(char* remote_sh, char* remote_sl)
{
	// Configure Remote Xbee module
	int i, err;
	char dAddr[8], para[8];
	char sAddr[8], local_sh[8], local_sl[8];
	wchar_t wch[16];
	CString addr8B;

	err = 0;
	memset(dAddr, 0, sizeof(dAddr));
	memset(para, 0, sizeof(para));
	// Convert Address
	//err += XbeeAddrConverter(remote_sh, remote_sl, dAddr);
	memcpy(dAddr, remote_sh, XBEE_SHADDR_LEN);
	memcpy((dAddr + XBEE_SHADDR_LEN), remote_sl, XBEE_SLADDR_LEN);
	// Enter transparent mode
	para[0] = 0x00;
	err += RemoteXbeeMsg(dAddr, "AP", para, 1);
	// Baudrate should be 115200
	para[0] = 0x07;
	err += RemoteXbeeMsg(dAddr, "BD", para, 1);
	// Configure destination address
	addr8B = "";
	for (i = 0; i < (8 - m_xbeesh.GetLength()); i++) {
		addr8B += "0";
	}
	addr8B += m_xbeesh;
	wcscpy_s(wch, CT2CW(addr8B));
	UnicodeToAnsi(wch, local_sh);
	addr8B = "";
	for (i = 0; i < (8 - m_xbeesl.GetLength()); i++) {
		addr8B += "0";
	}
	addr8B += m_xbeesl;
	wcscpy_s(wch, CT2CW(addr8B));
	UnicodeToAnsi(wch, local_sl);
	XbeeAddrConverter(local_sh, local_sl, sAddr);
	err += RemoteXbeeMsg(dAddr, "DH", sAddr, 4);
	err += RemoteXbeeMsg(dAddr, "DL", (sAddr + 4), 4);
	// Store in non-volatile memory
	err += RemoteXbeeMsg(dAddr, "WR", NULL, 0);
	err += RemoteXbeeMsg(dAddr, "AC", NULL, 0);
	Sleep(50);


	return err;
}


int CUnderwaterAcousticCommunicationDlg::RssiXbeeMsg(char* remote_sh, char* remote_sl, char* rssi)
{
	int i, num, err;
	char sum, msg[64], rbuffer[64];

	err = 1;
	memset(msg, 0, sizeof(msg));
	// Header
	msg[0] = 0x7E;
	// Length
	msg[1] = 0x00;
	msg[2] = 0x0F;
	// Frame Type
	msg[3] = 0x17;
	// Frame ID
	msg[4] = 0x01;
	// Remote Xbee module address
	memcpy((msg + 5), remote_sh, XBEE_SHADDR_LEN);
	memcpy((msg + 5 + XBEE_SHADDR_LEN), remote_sl, XBEE_SLADDR_LEN);
	msg[13] = (char)0xFF;
	msg[14] = (char)0xFE;
	// Remote Option
	msg[15] = 0x02;
	// AT Command
	msg[16] = 'D';
	msg[17] = 'B';
	// Checksum
	sum = 0;
	for (i = 0; i < 0x0F; i++) {
		sum += msg[3 + i];
	}
	msg[18] = 0xFF - sum;
	// Send data...
	m_txLed.SetOnOff(TRUE);
	m_serialport.Write(msg, 19);
	Sleep(50);
	m_txLed.SetOnOff(FALSE);
	memset(rbuffer, 0, sizeof(rbuffer));
	m_rxLed.SetOnOff(TRUE);
	Sleep(50);
	num = m_serialport.Read(rbuffer, 20);
	if ((rbuffer[15] == 'D') && (rbuffer[16] == 'B')) {
		memcpy(rssi, (rbuffer + 17), 2);
	}
	m_rxLed.SetOnOff(FALSE);


	return err;
}



int CUnderwaterAcousticCommunicationDlg::CheckACK(char* message, int subCmd)
{
	int crc_cal, crc_recv, err, cmd;
	unsigned int header, sub, err_code;
	char* ptr;


	err = 1;
	ptr = message;
	header = (((unsigned int)ptr[0] << 24) & 0xFF000000) + (((unsigned int)ptr[1] << 16) & 0x00FF0000) + (((unsigned int)ptr[2] << 8) & 0x0000FF00) + ((unsigned int)ptr[3] & 0x000000FF);
	if (header == 0xFFFF0000) {
		cmd = ((int)ptr[4] << 8) + (int)ptr[5];
		if (cmd == ACK_CMD) {
			crc_recv = (((int)ptr[16] << 24) & 0xFF000000) + (((int)ptr[17] << 16) & 0x00FF0000) + (((int)ptr[18] << 8) & 0x0000FF00) + ((int)ptr[19] & 0x000000FF);
			crc_cal = m_serialport.CRC32((unsigned char*)(ptr + 8), 8);
			if (crc_cal == crc_recv) {
				sub = (((int)ptr[10] << 8) & 0x0000FF00) + ((int)ptr[11] & 0x000000FF);
				err_code = ((int)ptr[14] << 8) + (int)ptr[15];
				if ((sub == subCmd) && (err_code == 0)) {
					err = 0;
				}
			}
		}
	}

	return err;
}


int CUnderwaterAcousticCommunicationDlg::TimeSync(char* remote_sh, char* remote_sl)
{
	char wbuffer[32], rbuffer[32], addr[8];
	int crc, err;
	SYSTEMTIME systime;

	err = 1;
	memset(wbuffer, 0, sizeof(wbuffer));
	GetLocalTime(&systime);
	wbuffer[0] = wbuffer[1] = (char)0xFF;
	wbuffer[2] = wbuffer[3] = 0x00;
	wbuffer[4] = 0x00;
	wbuffer[5] = COMMON_CMD;
	wbuffer[6] = 0x00;
	wbuffer[7] = 14;
	wbuffer[8] = (char)(TIMESYNC_CMD >> 8);
	wbuffer[9] = (char)TIMESYNC_CMD;
	wbuffer[10] = wbuffer[11] = 0x00;
	// Month/Day/Year
	wbuffer[12] = (char)systime.wMonth;
	wbuffer[13] = (char)systime.wDay;
	wbuffer[14] = (char)(systime.wYear >> 8);
	wbuffer[15] = (char)systime.wYear;
	// HH:MM:SS
	wbuffer[16] = (char)systime.wHour;
	wbuffer[17] = (char)systime.wMinute;
	wbuffer[18] = (char)systime.wSecond;
	wbuffer[19] = 0x00;
	// Milliseconds
	wbuffer[20] = (char)(systime.wMilliseconds >> 8);
	wbuffer[21] = (char)systime.wMilliseconds;
	crc = m_serialport.CRC32((unsigned char*)(wbuffer + 8), 14);
	wbuffer[22] = (char)(crc >> 24);
	wbuffer[23] = (char)(crc >> 16);
	wbuffer[24] = (char)(crc >> 8);
	wbuffer[25] = (char)crc;
	memcpy(addr, remote_sh, 4);
	memcpy((addr + 4), remote_sl, 4);
	// Here we send Tx request frame and get Rx indicator frame in API mode
	TxXbeeMsg(wbuffer, 26, addr, rbuffer);
	if (CheckACK(rbuffer, TIMESYNC_CMD) == 0) {
		err = 0;
	}

	return err;
}


int CUnderwaterAcousticCommunicationDlg::GetModemAddr(char* remote_sh, char* remote_sl, char* modemAddr)
{
	char wbuffer[32], rbuffer[32], addr[8];
	int crc, err;

	err = 1;
	memset(wbuffer, 0, sizeof(wbuffer));
	wbuffer[0] = wbuffer[1] = (char)0xFF;
	wbuffer[2] = wbuffer[3] = 0x00;
	wbuffer[4] = 0x00;
	wbuffer[5] = COMMON_CMD;
	wbuffer[6] = 0x00;
	wbuffer[7] = 4;
	wbuffer[8] = (char)(MODEMADDR_CMD >> 8);
	wbuffer[9] = (char)MODEMADDR_CMD;
	wbuffer[10] = wbuffer[11] = 0x00;
	crc = m_serialport.CRC32((unsigned char*)(wbuffer + 8), 4);
	wbuffer[12] = (char)(crc >> 24);
	wbuffer[13] = (char)(crc >> 16);
	wbuffer[14] = (char)(crc >> 8);
	wbuffer[15] = (char)crc;
	memcpy(addr, remote_sh, 4);
	memcpy((addr + 4), remote_sl, 4);
	// Here we send Tx request frame and get Rx indicator frame in API mode
	TxXbeeMsg(wbuffer, 16, addr, rbuffer);
	if (CheckACK(rbuffer, MODEMADDR_CMD) == 0) {
		*modemAddr = rbuffer[12];
		err = 0;
	}
	else {
		*modemAddr = 0x0;
	}


	return err;
}


int CUnderwaterAcousticCommunicationDlg::Download(char* remote_addr)
{
	char sbuffer[256], rbuffer[64];
	int crc, flags, loop_cnt, type;
	int length, size_cnt, size_remind, i_err;
	unsigned char* puc_ptr;
	unsigned char* pBuffer;

	type = m_downloadType;
	length = m_downloadFilelen;
	pBuffer = m_downloadpBuffer;
	memset(sbuffer, 0, sizeof(sbuffer));
	memset(rbuffer, 0, sizeof(rbuffer));
	m_progress.SetRange(0, (length / PACKETSIZE + 1));
	m_progress.SetPos(0);
	// Packet Format
	// --------------------------------------------------------------------------------------------------------------------------------
	// | Header (0xFFFF0000) | DOWNLOAD_CMD (0x0002) | Packet Length (2B) | File Type (2B) | Reserved (2B) | File Size(4B) | CRC(4B) |
	// --------------------------------------------------------------------------------------------------------------------------------
	sbuffer[0] = sbuffer[1] = 0xFF;
	sbuffer[2] = sbuffer[3] = 0x00;
	sbuffer[4] = 0x00;
	sbuffer[5] = (unsigned char)DOWNLOAD_CMD;
	sbuffer[6] = 0x00;
	sbuffer[7] = 8;
	sbuffer[8] = 0x00;
	sbuffer[9] = (unsigned char)type;
	sbuffer[10] = sbuffer[11] = 0x00;
	sbuffer[12] = (unsigned char)(length >> 24);
	sbuffer[13] = (unsigned char)(length >> 16);
	sbuffer[14] = (unsigned char)(length >> 8);
	sbuffer[15] = (unsigned char)length;
	crc = m_serialport.CRC32((unsigned char*)(sbuffer + 8), 8);
	sbuffer[16] = (char)(crc >> 24);
	sbuffer[17] = (char)(crc >> 16);
	sbuffer[18] = (char)(crc >> 8);
	sbuffer[19] = (char)crc;
	// Send data...
	TxXbeeMsg(sbuffer, DOWNLOAD_CMDLEN, remote_addr, rbuffer);
	if (CheckACK((char*)rbuffer, 0xFFFF) == 0) {
		flags = 1;
	}
	// Download the data
	puc_ptr = pBuffer;
	size_cnt = length / PACKETSIZE;
	size_remind = length % PACKETSIZE;
	if ((flags == 1) && (size_cnt > 0)) {
		loop_cnt = 0;
		// Packet Format
		// ------------------------------------------------------------------------------------------------
		// | Header (0xFFFF0000) | DOWNLOAD_CMD (0x0002) | Packet Length (2B) | Payload (116B) | CRC(4B) |
		// ------------------------------------------------------------------------------------------------
		sbuffer[6] = 0x00;
		sbuffer[7] = PACKETSIZE;
		do {
			memcpy(sbuffer + 8, puc_ptr, PACKETSIZE);
			crc = m_serialport.CRC32((unsigned char*)(sbuffer + 8), PACKETSIZE);
			sbuffer[124] = (char)(crc >> 24);
			sbuffer[125] = (char)(crc >> 16);
			sbuffer[126] = (char)(crc >> 8);
			sbuffer[127] = (char)crc;
			// Send data...
			TxXbeeMsg(sbuffer, PACKET_CMDLEN, remote_addr, rbuffer);
			if (CheckACK((char*)rbuffer, 0xFFFF) == 0) {
				flags = 1;
			}
			else {
				flags = 0;
			}
			puc_ptr += PACKETSIZE;
			m_progress.SetPos(loop_cnt + 1);
		} while ((flags == 1) && (++ loop_cnt < size_cnt));
	}
	// Download the rest of the data
	if (flags == 1) {
		sbuffer[6] = 0x00;
		sbuffer[7] = size_remind;
		memcpy(sbuffer + 8, puc_ptr, size_remind);
		crc = m_serialport.CRC32((unsigned char*)(sbuffer + 8), size_remind);
		sbuffer[8 + size_remind] = (char)(crc >> 24);
		sbuffer[8 + size_remind + 1] = (char)(crc >> 16);
		sbuffer[8 + size_remind + 2] = (char)(crc >> 8);
		sbuffer[8 + size_remind + 3] = (char)crc;
		// Send data...
		TxXbeeMsg(sbuffer, (8 + size_remind + 4), remote_addr, rbuffer);
		if (CheckACK((char*)rbuffer, 0xFFF0) == 0) {
			flags = 1;
		}
		else {
			flags = 0;
		}
	}
	if (flags == 1) {
		m_progress.SetPos(length / PACKETSIZE + 1);
	}
	free(m_downloadpBuffer);
	i_err = (flags == 1) ? 0 : 1;

	return i_err;
}


DWORD WINAPI CUnderwaterAcousticCommunicationDlg::DownloadThread(LPVOID lpPara)
{
	CUnderwaterAcousticCommunicationDlg* p_class = static_cast<CUnderwaterAcousticCommunicationDlg*>(lpPara);
	struct XBeeInfo* pXbee;
	char addr[8];

	if ((g_xbeeOffset < 0) || (g_xbeeOffset > MAX_NUM_OF_NODES)) {
		p_class->MessageBox(L"Please select one node, and then download your file.", MB_OK);
		g_stThread = ST_THREAD_STOP;
		return 1;
	}

	pXbee = &g_xbeeInfo[g_xbeeOffset];
	memcpy(addr, pXbee->shAddr, 4);
	memcpy((addr + 4), pXbee->slAddr, 4);
	if (p_class->Download(addr) != 0) {
		p_class->MessageBox(L"Download File Error!", MB_OK);
	}
	else {
		p_class->MessageBox(L"Download File OK!", MB_OK);
	}
	g_stThread = ST_THREAD_STOP;

	return 0;
}


int CUnderwaterAcousticCommunicationDlg::ScanNetwork(void)
{
	int timer_cnt, scan_time, already_exist;
	int addr, i;
	struct XBeeInfo* pXbee;
	HTREEITEM hItem, hSubItem, h2ndItem;
	CString str;
	WCHAR message[32];
	char frameLen, sum, scanTime[2];
	char msg[32];
	char rbuffer[8192], hex[8];
	char* ptr;
	char* ptrFrom;
	char* ptrTo;
	char* ptrMove;


	m_serialport.ClearWriteBuffer();
	m_serialport.ClearReadBuffer();
	memset(rbuffer, 0, sizeof(rbuffer));

	// Configure Netwrok Discovery backoff
	memset(hex, 0, sizeof(hex));
	scan_time = m_scantime * 10;
	if (scan_time > 252) {
		scan_time = 252;
		m_scantime = 25;
	}
	else if (scan_time < 32) {
		scan_time = 32;
		m_scantime = 32;
	}
	scanTime[0] = (char)(scan_time >> 8);
	scanTime[1] = (char)scan_time;
	LocalXbeeMsg("NT", scanTime, 2);
	// Send ATND command to do network discovery
	// Header
	msg[0] = 0x7E;
	// Length
	msg[1] = 0x00;
	msg[2] = 0x04;
	// Frame Type
	msg[3] = 0x08;
	// Frame ID
	msg[4] = 0x01;
	// AT Command
	msg[5] = 'N';
	msg[6] = 'D';
	// Checksum
	sum = 0;
	for (i = 0; i < 0x04; i++) {
		sum += msg[3 + i];
	}
	msg[7] = 0xFF - sum;
	m_txLed.SetOnOff(TRUE);
	m_serialport.Write(msg, 8);
	Sleep(50);
	m_txLed.SetOnOff(FALSE);
	g_endScan = timer_cnt = 0;
	memset(rbuffer, 0, sizeof(rbuffer));
	ptr = rbuffer;
	do {
		m_rxLed.SetOnOff(TRUE);
		if (m_serialport.Read(ptr, 1) == 1) {
			if (*ptr == 0x7E) {
				Sleep(5);
				m_serialport.Read((ptr + 1), 2);
				frameLen = (char)(ptr[1] >> 8) + (char)ptr[2];
				Sleep(20);
				m_serialport.Read((ptr + 3), frameLen + 1);
				ptrFrom = ptr;
				ptr = ptrTo = ptrFrom + frameLen + 4;
				// Search it to see if it has been existed
				pXbee = g_xbeeInfo;
				already_exist = 0;
				for (i = 0; i < MAX_NUM_OF_NODES; i++) {
					if ((strncmp(pXbee->shAddr, (ptrFrom + 10), XBEE_SHADDR_LEN) == 0) &&
						(strncmp(pXbee->slAddr, (ptrFrom + 14), XBEE_SLADDR_LEN) == 0)){
						already_exist = 1;
						break;
					}
					++pXbee;
				}
				// Store the info after finding remote xbee module
				if (already_exist == 0) {
					pXbee = g_xbeeInfo;
					for (i = 0; i < MAX_NUM_OF_NODES; i++) {
						if ((pXbee->myAddr[0] == 0x00) && (pXbee->myAddr[1] == 0x00)) {
							ptrFrom += 8;
							// Copy My Address
							memcpy(pXbee->myAddr, ptrFrom, XBEE_MYADDR_LEN);
							ptrFrom += XBEE_MYADDR_LEN;
							// Copy SH Address
							memcpy(pXbee->shAddr, ptrFrom, XBEE_SHADDR_LEN);
							ptrFrom += XBEE_SHADDR_LEN;
							// Copy SL Address
							memcpy(pXbee->slAddr, ptrFrom, XBEE_SLADDR_LEN);
							ptrFrom += XBEE_SLADDR_LEN;
							// Copy Network Identifier
							ptrMove = ptrFrom;
							do {
								++ptrMove;
							} while (*ptrMove != 0x00);
							memcpy(pXbee->nIdenti, ptrFrom, (ptrMove - ptrFrom));
							ptrFrom = ptrMove + 1;
							// Parent network address
							ptrFrom += 2;
							// Device type
							pXbee->devType = ptrFrom[0];
							ptrFrom += 1;
							// Ignore the rest, and update the tree structure
							hItem = m_uan.GetRootItem();
							str.Format(L"XBee %d: Stop", (m_xbeeNodeNum + 1));
							hSubItem = m_uan.InsertItem(str, NULL, NULL, hItem);
							m_uan.SetItemData(hSubItem, (m_xbeeNodeNum + 1));
							addr = (((int)(pXbee->shAddr[0]) << 24) & 0xFF000000) + (((int)(pXbee->shAddr[1]) << 16) & 0x00FF0000) +
									(((int)(pXbee->shAddr[2]) << 8) & 0x0000FF00) + ((int)(pXbee->shAddr[3]) & 0x000000FF);
							str.Format(L"SH: %x", addr);
							h2ndItem = m_uan.InsertItem(str, NULL, NULL, hSubItem);
							m_uan.SetItemData(h2ndItem, 100);
							addr = (((int)(pXbee->slAddr[0]) << 24) & 0xFF000000) + (((int)(pXbee->slAddr[1]) << 16) & 0x00FF0000) +
								(((int)(pXbee->slAddr[2]) << 8) & 0x0000FF00) + ((int)(pXbee->slAddr[3]) & 0x000000FF);
							str.Format(L"SL: %x", addr);
							h2ndItem = m_uan.InsertItem(str, NULL, NULL, hSubItem);
							m_uan.SetItemData(h2ndItem, 100);
							memset(message, 0, sizeof(message));
							AnsiToUnicode(message, pXbee->nIdenti);
							str.Format(L"NI: %s", message);
							h2ndItem = m_uan.InsertItem(str, NULL, NULL, hSubItem);
							m_uan.SetItemData(h2ndItem, 100);
							++m_xbeeNodeNum;
							Invalidate();
							m_uan.Expand(hItem, TVE_EXPAND);
							break;
						}
						++pXbee;
					}
				}
			}
		}
		Sleep(100);
		m_rxLed.SetOnOff(FALSE);
		Sleep(100);
	} while ((++timer_cnt < (scan_time / 2 + 5)) && (g_endScan == 0));
	// Get RSSI value from each remote XBee module
	pXbee = g_xbeeInfo;
	hItem = m_uan.GetRootItem();
	if (m_xbeeNodeNum > 0) {
		hSubItem = m_uan.GetChildItem(hItem);
		for (i = 0; i < MAX_NUM_OF_NODES; i++) {
			if ((pXbee->myAddr[0] != 0x00) || (pXbee->myAddr[1] != 0x00)) {
				if ((pXbee->rssi[0] == 0) && (pXbee->rssi[1] == 0)) {
					RssiXbeeMsg(pXbee->shAddr, pXbee->slAddr, pXbee->rssi);
					if (hSubItem != NULL) {
						str.Format(L"RSSI(dBm): %d", pXbee->rssi[1]);
						h2ndItem = m_uan.InsertItem(str, NULL, NULL, hSubItem);
						m_uan.SetItemData(h2ndItem, 100);
						hSubItem = m_uan.GetNextSiblingItem(hSubItem);
					}
				}
			}
			++pXbee;
		}
	}
	m_uan.Expand(hItem, TVE_EXPAND);


	return 0;
}


DWORD WINAPI CUnderwaterAcousticCommunicationDlg::ScanThread(LPVOID lpPara)
{
	DWORD err;
	CUnderwaterAcousticCommunicationDlg* p_class = static_cast<CUnderwaterAcousticCommunicationDlg*>(lpPara);

	err = (DWORD)(p_class->ScanNetwork());
	g_stThread = ST_THREAD_STOP;
	p_class->m_stApp = ST_APP_NETWORKSCAN;

	return err;
}


int CUnderwaterAcousticCommunicationDlg::WakeupXbee(void)
{
	int err, length, i, val_h, val_l, temp, num, count;
	WCHAR wch[64];
	char message[64], dAddr[8], msg[64];
	char rbuffer[32], sum;

	err = val_h = val_l = 0;
	length = m_xbeedh.GetLength();
	wcscpy_s(wch, CT2CW(m_xbeedh));
	UnicodeToAnsi(wch, message);
	length = m_xbeedh.GetLength();
	for (i = 0; i < length; i++) {
		if ((message[i] >= 0x30) && (message[i] <= 0x39)) {
			temp = (int)(message[i] - 48);
		}
		else if ((message[i] >= 0x41) && (message[i] <= 0x46)) {
			temp = (int)(message[i] - 55);
		}
		else if ((message[i] >= 0x61) && (message[i] <= 0x66)) {
			temp = (int)(message[i] - 87);
		}
		else {
			err = 1;
			break;
		}
		val_h += (temp << (4 * (length - i - 1)));
	}
	length = m_xbeedl.GetLength();
	wcscpy_s(wch, CT2CW(m_xbeedl));
	UnicodeToAnsi(wch, message);
	for (i = 0; i < length; i++) {
		if ((message[i] >= 0x30) && (message[i] <= 0x39)) {
			temp = (int)(message[i] - 48);
		}
		else if ((message[i] >= 0x41) && (message[i] <= 0x46)) {
			temp = (int)(message[i] - 55);
		}
		else if ((message[i] >= 0x61) && (message[i] <= 0x66)) {
			temp = (int)(message[i] - 87);
		}
		else {
			err = 1;
			break;
		}
		val_l += (temp << (4 * (length - i - 1)));
	}
	if (err == 0) {
		for (i = 0; i < 4; i++) {
			dAddr[i] = val_h >> ((4 - i - 1) * 8);
			dAddr[4 + i] = val_l >> ((4 - i - 1) * 8);
		}

		err = 1;
		memset(msg, 0, sizeof(msg));
		// Header
		msg[0] = 0x7E;
		// Length
		msg[1] = 0x00;
		msg[2] = 0x0F + 1;
		// Frame Type
		msg[3] = 0x17;
		// Frame ID
		msg[4] = 0x01;
		// Destination Address
		memcpy((msg + 5), dAddr, 8);
		// Reserved
		msg[13] = (char)0xFF;
		msg[14] = (char)0xFE;
		// Remote Command Option
		msg[15] = 0x02;
		// AT Command
		msg[16] = 'S';
		msg[17] = 'M';
		// Command Parameter
		msg[18] = 0x0;
		// Checksum
		sum = 0;
		for (i = 0; i < (0x0F + 1); i++) {
			sum += msg[3 + i];
		}
		msg[18 + 1] = 0xFF - sum;
		// Send data...
		m_txLed.SetOnOff(TRUE);
		m_serialport.Write(msg, (0x0F + 1 + 4));
		Sleep(50);
		m_txLed.SetOnOff(FALSE);
		memset(rbuffer, 0, sizeof(rbuffer));
		m_rxLed.SetOnOff(TRUE);
		count = 0;
		do {
			num = m_serialport.Read(rbuffer, 1);
			m_rxLed.SetOnOff(TRUE);
			Sleep(250);
			m_rxLed.SetOnOff(FALSE);
			Sleep(250);
		} while ((num == 0) && (++count < 120));
		Sleep(50);
		num += m_serialport.Read(rbuffer + 1, 18);
		if ((num == 19) && (rbuffer[17] == 0x00)) {
			err = 0;
		}
		m_rxLed.SetOnOff(FALSE);

	}
	

	return err;
}


DWORD WINAPI CUnderwaterAcousticCommunicationDlg::WakeupThread(LPVOID lpPara)
{
	DWORD err;
	CUnderwaterAcousticCommunicationDlg* p_class = static_cast<CUnderwaterAcousticCommunicationDlg*>(lpPara);

	err = (DWORD)(p_class->WakeupXbee());
	if (err == 0) {
		AfxMessageBox(_T("The remote XBee module has been waken up."));
	}
	else {
		AfxMessageBox(_T("The remote XBee module wake up timeout, please check the destination address"));
	}
	g_stThread = ST_THREAD_STOP;

	return err;
}


DWORD WINAPI CUnderwaterAcousticCommunicationDlg::MonitorThread(LPVOID lpPara)
{
	CUnderwaterAcousticCommunicationDlg* p_class = static_cast<CUnderwaterAcousticCommunicationDlg*>(lpPara);
	int i, j, crc, crc_recv, header, converter, err;
	char wbuffer[32], rbuffer[128], addr[8];
	struct MonitorInfo* pInfo;
	struct ModemInfo* pMinfo;
	struct XBeeInfo* pXbee;
	CString strItem;
	LVITEM lvi;
	float temp;
	char* ptr;

	g_endMonitor = MONITOR_RUN;
	do {
		pXbee = g_xbeeInfo;
		pMinfo = g_modemInfo;
		pInfo = g_monitorInfo;
		for (i = 0; i < MAX_NUM_OF_NODES; i++) {
			if ((pMinfo->runstop == MODEM_RUN) && (g_endMonitor == MONITOR_RUN)) {
				err = p_class->LocalXbeeMsg("DH", pXbee->shAddr, 4);
				err += p_class->LocalXbeeMsg("DL", pXbee->slAddr, 4);
				if (err == 0) {
					wbuffer[0] = wbuffer[1] = (char)0xFF;
					wbuffer[2] = wbuffer[3] = 0x00;
					wbuffer[4] = 0x00;
					wbuffer[5] = MONITOR_CMD;
					wbuffer[6] = 0x00;
					wbuffer[7] = 4;
					wbuffer[8] = 0x00;
					wbuffer[9] = 0x00;
					wbuffer[10] = wbuffer[11] = 0x00;
					crc = p_class->m_serialport.CRC32((unsigned char*)(wbuffer + 8), 4);
					wbuffer[12] = (char)(crc >> 24);
					wbuffer[13] = (char)(crc >> 16);
					wbuffer[14] = (char)(crc >> 8);
					wbuffer[15] = (char)crc;
					memcpy(addr, pXbee->shAddr, 4);
					memcpy((addr + 4), pXbee->slAddr, 4);
					// Here we send Tx request frame and get Rx indicator frame in API mode
					memset(rbuffer, 0, sizeof(rbuffer));
					p_class->TxXbeeMsg(wbuffer, 16, addr, rbuffer);
					header = (((unsigned int)rbuffer[0] << 24) & 0xFF000000) + (((unsigned int)rbuffer[1] << 16) & 0x00FF0000) + 
								(((unsigned int)rbuffer[2] << 8) & 0x0000FF00) + ((unsigned int)rbuffer[3] & 0x000000FF);
					if (header == 0xFFFF0000) {
						crc_recv = (((int)rbuffer[8 + MONITOR_LENGTH] << 24) & 0xFF000000) + 
									(((int)rbuffer[8 + MONITOR_LENGTH + 1] << 16) & 0x00FF0000) + 
									(((int)rbuffer[8 + MONITOR_LENGTH + 2] << 8) & 0x0000FF00) + 
									((int)rbuffer[8 + MONITOR_LENGTH + 3] & 0x000000FF);
						crc = p_class->m_serialport.CRC32((unsigned char*)(rbuffer + 8), MONITOR_LENGTH);
						if (crc == crc_recv) {
							memcpy(pInfo, (rbuffer + 8), MONITOR_LENGTH);
							strItem = L"OK";
						}
						else {
							strItem = L"CRC Err";
						}
					}
					else {
						strItem = L"Data Err";
					}
					// Update monitor info in UI Table
					// State
					lvi.mask = LVIF_IMAGE | LVIF_TEXT;
					lvi.iItem = pMinfo->listOffset;
					lvi.iSubItem = 2;
					lvi.pszText = (LPTSTR)(LPCTSTR)(strItem);
					p_class->m_List.SetItem(&lvi);
					ptr = pInfo->data;
					for (j = 0; j < 5; j++) {
						converter = (((unsigned int)ptr[0] << 24) & 0xFF000000) + (((unsigned int)ptr[1] << 16) & 0x00FF0000) +
									(((unsigned int)ptr[2] << 8) & 0x0000FF00) + ((unsigned int)ptr[3] & 0x000000FF);
						temp = ((float)converter) / 10000;
						strItem.Format(L"%.4f", temp);
						lvi.iSubItem = 3 + j;
						lvi.pszText = (LPTSTR)(LPCTSTR)(strItem);
						p_class->m_List.SetItem(&lvi);
						ptr += 4;
					}
				}
			}
			++pXbee;
			++pMinfo;
			++pInfo;
		}
		Sleep(1000);
	} while (g_endMonitor == MONITOR_RUN);
	g_stThread = ST_THREAD_STOP;

	return 0;
}

// CUnderwaterAcousticCommunicationDlg message handlers

BOOL CUnderwaterAcousticCommunicationDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	// TODO: Add extra initialization here
	int i;

	InitializeCriticalSection(&g_csThread);

	m_comport.AddString(_T("COM1"));
	m_comport.AddString(_T("COM2"));
	m_comport.AddString(_T("COM3"));
	m_comport.AddString(_T("COM4"));
	m_comport.AddString(_T("COM5"));
	m_comport.AddString(_T("COM6"));
	m_comport.AddString(_T("COM7"));
	m_comport.AddString(_T("COM8"));
	m_comport.AddString(_T("COM9"));
	m_comport.AddString(_T("COM10"));
	m_comport.AddString(_T("COM11"));
	m_comport.AddString(_T("COM12"));
	m_comport.SetCurSel(0);
	m_baudrate.AddString(_T("2400"));
	m_baudrate.AddString(_T("4800"));
	m_baudrate.AddString(_T("9600"));
	m_baudrate.AddString(_T("19200"));
	m_baudrate.AddString(_T("38400"));
	m_baudrate.AddString(_T("57600"));
	m_baudrate.AddString(_T("115200"));
	m_baudrate.SetCurSel(2);
	m_xbeeMode.AddString(_T("Transparent Mode"));
	m_xbeeMode.AddString(_T("API Mode"));
	m_xbeeMode.SetCurSel(1);
	m_rfPower.AddString(_T("10 dBm"));
	m_rfPower.AddString(_T("12 dBm"));
	m_rfPower.AddString(_T("14 dBm"));
	m_rfPower.AddString(_T("16 dBm"));
	m_rfPower.AddString(_T("18 dBm"));
	m_rfPower.SetCurSel(4);
	m_dlType.AddString(_T("Executive File"));
	m_dlType.AddString(_T("Modem File"));
	m_dlType.SetCurSel(0);
	m_ulType.AddString(_T("Log File"));
	m_ulType.AddString(_T("Debug File"));
	m_ulType.AddString(_T("Modem File"));
	m_ulType.SetCurSel(0);

	m_progress.SetRange(0, 100);
	m_progress.SetPos(50);

	m_txLed.SetColor(COM_LEDGRAYON, COM_LEDGRAYOFF);
	m_txLed.SetBlink(0);
	m_rxLed.SetColor(COM_LEDGRAYON, COM_LEDGRAYOFF);
	m_rxLed.SetBlink(0);
	m_txLed.SetOnOff(FALSE);
	m_rxLed.SetOnOff(FALSE);

	LONG lStyle;
	lStyle = GetWindowLong(m_List.m_hWnd, GWL_STYLE);
	lStyle &= ~LVS_TYPEMASK; 
	lStyle |= LVS_REPORT;
	SetWindowLong(m_List.m_hWnd, GWL_STYLE, lStyle); 
	DWORD dwStyle = m_List.GetExtendedStyle();
	dwStyle |= LVS_EX_FULLROWSELECT; 
	dwStyle |= LVS_EX_GRIDLINES; 
	dwStyle |= LVS_EX_CHECKBOXES;
	m_List.SetExtendedStyle(dwStyle);
	m_List.DeleteAllItems();
	m_List.InsertColumn(0, _T("Name"));
	m_List.InsertColumn(1, _T("Modem Addr"));
	m_List.InsertColumn(2, _T("State"));
	m_List.InsertColumn(3, _T("Q Empty"));
	m_List.InsertColumn(4, _T("TARS I"));
	m_List.InsertColumn(5, _T("Avg Delay"));
	m_List.InsertColumn(6, _T("Ping State"));
	m_List.InsertColumn(7, _T("Tx State"));
	m_List.SetColumnWidth(0, 120);
	m_List.SetColumnWidth(1, 90);
	m_List.SetColumnWidth(2, 60);
	m_List.SetColumnWidth(3, 80);
	m_List.SetColumnWidth(4, 80);
	m_List.SetColumnWidth(5, 60);
	m_List.SetColumnWidth(6, 80);
	m_List.SetColumnWidth(7, 80);
	//m_List.SetRedraw(FALSE);

	HTREEITEM hItem = m_uan.InsertItem(L"Beautiful Shore", NULL, NULL);
	m_uan.SetItemData(hItem, 0);
	m_uan.ModifyStyle(NULL, TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_EDITLABELS | TVS_TRACKSELECT | TVS_SHOWSELALWAYS);

	m_scantime = 13;
	m_runstopFlag = 0;
	for (i = 0; i < MAX_NUM_OF_NODES; i++) {
		memset(&g_xbeeInfo[i], 0, sizeof(struct XBeeInfo));
		memset(&g_modemInfo[i], 0, sizeof(struct ModemInfo));
		memset(&g_powerInfo[i], POWER_DOWN, sizeof(struct PowerInfo));
	}
	m_xbeeNodeNum = 0;

	UpdateData(FALSE);



	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CUnderwaterAcousticCommunicationDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CUnderwaterAcousticCommunicationDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CUnderwaterAcousticCommunicationDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}



void CUnderwaterAcousticCommunicationDlg::OnBnClickedButtonConnect()
{
	// TODO: Add your control notification handler code here
	unsigned int ui_port, ui_cnt;
	int i_len, i_baudrate;
	CString str_baudrate;
	wchar_t wc_buffer[8];
	char c_buffer[8];

	if (g_stThread != ST_THREAD_STOP) {
		AfxMessageBox(_T("One thread is running, please wait for completion or stop it manually."));
		return;
	}

	ui_port = (unsigned int)(m_comport.GetCurSel() + 1);
	i_len = m_baudrate.GetLBTextLen(m_baudrate.GetCurSel());
	m_baudrate.GetLBText(m_baudrate.GetCurSel(), str_baudrate.GetBuffer(i_len));
	wcscpy_s(wc_buffer, str_baudrate.GetBuffer());
	ui_cnt = 0;
	do {
		c_buffer[ui_cnt] = (char)wc_buffer[ui_cnt];
	} while (++ui_cnt < (unsigned int)i_len);
	i_baudrate = atoi(c_buffer);
	str_baudrate.ReleaseBuffer();

	if (m_stApp == ST_APP_ZERO) {
		try {
			m_serialport.Open(ui_port, i_baudrate, CSerialPort::NoParity, 8, CSerialPort::OneStopBit, CSerialPort::NoFlowControl, NULL);
			m_serialport.Set0Timeout();
			m_serialport.Set0ReadTimeout();
			m_serialport.Setup(1024, 1024);
			m_connect.SetWindowTextW(_T("Disconnect"));
			m_stApp = ST_APP_CONNECTED;
			OnBnClickedButtonReadinfo2();
		}
		catch (CSerialException* pEx) {
			pEx->Delete();
			AfxMessageBox(_T("Open COM Port Error."));
		}
	}
	else {
		m_serialport.Close();
		m_xbeesh = "";
		m_xbeesl = "";
		m_xbeedh = "";
		m_xbeedl = "";
		m_xbeeid = "";
		m_xbeecm = "";
		m_connect.SetWindowTextW(_T("Connect"));
		m_stApp = ST_APP_ZERO;
		UpdateData(false);
	}
}


void CUnderwaterAcousticCommunicationDlg::OnCbnSelchangeComboXbeemode()
{
	// TODO: Add your control notification handler code here
	m_xbeeMode.SetCurSel(1);
	AfxMessageBox(_T("Please use API mode only for configuration...."));
}


void CUnderwaterAcousticCommunicationDlg::OnBnClickedButtonReadinfo2()
{
	// TODO: Add your control notification handler code here
	char initCmd[] = "+++";
	char at_atap[] = "ATAP1\r";
	char at_atsh[] = "ATSH\r";
	char at_atsl[] = "ATSL\r";
	char at_atdh[] = "ATDH\r";
	char at_atdl[] = "ATDL\r";
	char at_atid[] = "ATID\r";
	char at_atcm[] = "ATCM\r";
	char at_atpl[] = "ATPL\r";
	char at_atcn[] = "ATCN\r";
	char rbuffer[24];
	WCHAR message[24];
	DWORD count, i;
	CString str;

	if (g_stThread != ST_THREAD_STOP) {
		AfxMessageBox(_T("One thread is running, please wait for completion or stop it manually."));
		return;
	}

	if (m_stApp != ST_APP_ZERO) {
		m_serialport.ClearWriteBuffer();
		m_serialport.ClearReadBuffer();
		memset(rbuffer, 0, sizeof(rbuffer));
		// Send "+++" command to enter AT command mode
		m_txLed.SetOnOff(TRUE);
		m_serialport.Write(initCmd, 3);
		Sleep(20);
		m_txLed.SetOnOff(FALSE);
		for (i = 0; i < 3; i++) {
			m_rxLed.SetOnOff(TRUE);
			Sleep(200);
			m_rxLed.SetOnOff(FALSE);
		}
		m_serialport.Read(rbuffer, 3);
		//m_rxLed.SetOnOff(FALSE);
		if ((rbuffer[0] != 'O') || (rbuffer[1] != 'K') || (rbuffer[2] != '\r')) {
			AfxMessageBox(L"Cannot find XBee Module, please check the connection.");
		}
		else {
			// First of all, switch to API 1 mode
			m_serialport.Write(at_atap, 6);
			Sleep(40);
			m_serialport.Read(rbuffer, 3);

			// Send ATSH and ATSL command to get XBee source address
			memset(rbuffer, 0, sizeof(rbuffer));
			memset(message, 0, sizeof(message));
			m_txLed.SetOnOff(TRUE);
			m_serialport.Write(at_atsh, 5);
			Sleep(40);
			m_txLed.SetOnOff(FALSE);
			m_rxLed.SetOnOff(TRUE);
			count = m_serialport.Read(rbuffer, 9);
			m_rxLed.SetOnOff(FALSE);
			rbuffer[count - 1] = 0x00;
			AnsiToUnicode(message, rbuffer);
			m_xbeesh = message;
			memset(rbuffer, 0, sizeof(rbuffer));
			memset(message, 0, sizeof(message));
			m_txLed.SetOnOff(TRUE);
			m_serialport.Write(at_atsl, 5);
			Sleep(40);
			m_txLed.SetOnOff(FALSE);
			m_rxLed.SetOnOff(TRUE);
			count = m_serialport.Read(rbuffer, 9);
			m_rxLed.SetOnOff(FALSE);
			rbuffer[count - 1] = 0x00;
			AnsiToUnicode(message, rbuffer);
			m_xbeesl = message;
			// Send ATDH and ATDL command to get XBee destination address
			memset(rbuffer, 0, sizeof(rbuffer));
			memset(message, 0, sizeof(message));
			m_txLed.SetOnOff(TRUE);
			m_serialport.Write(at_atdh, 5);
			Sleep(40);
			m_txLed.SetOnOff(FALSE);
			m_rxLed.SetOnOff(TRUE);
			count = m_serialport.Read(rbuffer, 9);
			m_rxLed.SetOnOff(FALSE);
			rbuffer[count - 1] = 0x00;
			AnsiToUnicode(message, rbuffer);
			m_xbeedh = message;
			memset(rbuffer, 0, sizeof(rbuffer));
			memset(message, 0, sizeof(message));
			m_txLed.SetOnOff(TRUE);
			m_serialport.Write(at_atdl, 5);
			Sleep(40);
			m_txLed.SetOnOff(FALSE);
			m_rxLed.SetOnOff(TRUE);
			count = m_serialport.Read(rbuffer, 9);
			m_rxLed.SetOnOff(FALSE);
			rbuffer[count - 1] = 0x00;
			AnsiToUnicode(message, rbuffer);
			m_xbeedl = message;
			// Send ATID command to get XBee Network ID
			memset(rbuffer, 0, sizeof(rbuffer));
			memset(message, 0, sizeof(message));
			m_txLed.SetOnOff(TRUE);
			m_serialport.Write(at_atid, 5);
			Sleep(40);
			m_txLed.SetOnOff(FALSE);
			m_rxLed.SetOnOff(TRUE);
			count = m_serialport.Read(rbuffer, 8);
			m_rxLed.SetOnOff(FALSE);
			rbuffer[count - 1] = 0x00;
			AnsiToUnicode(message, rbuffer);
			m_xbeeid = message;
			// Send ATCM command to get XBee Channel Mask
			memset(rbuffer, 0, sizeof(rbuffer));
			memset(message, 0, sizeof(message));
			m_txLed.SetOnOff(TRUE);
			m_serialport.Write(at_atcm, 5);
			Sleep(60);
			m_txLed.SetOnOff(FALSE);
			m_rxLed.SetOnOff(TRUE);
			count = m_serialport.Read(rbuffer, 17);
			m_rxLed.SetOnOff(FALSE);
			rbuffer[count - 1] = 0x00;
			AnsiToUnicode(message, rbuffer);
			m_xbeecm = message;
			// Send ATPL command to get XBee Power Level
			memset(rbuffer, 0, sizeof(rbuffer));
			memset(message, 0, sizeof(message));
			m_txLed.SetOnOff(TRUE);
			m_serialport.Write(at_atpl, 5);
			Sleep(60);
			m_txLed.SetOnOff(FALSE);
			m_rxLed.SetOnOff(TRUE);
			count = m_serialport.Read(rbuffer, 2);
			m_rxLed.SetOnOff(FALSE);
			m_rfPower.SetCurSel((int)(rbuffer[0] - 0x30));
			// Send ATCN to quit AT command mode
			memset(rbuffer, 0, sizeof(rbuffer));
			memset(message, 0, sizeof(message));
			m_txLed.SetOnOff(TRUE);
			m_serialport.Write(at_atcn, 5);
			Sleep(60);
			m_txLed.SetOnOff(FALSE);
			m_rxLed.SetOnOff(TRUE);
			count = m_serialport.Read(rbuffer, 3);
			m_rxLed.SetOnOff(FALSE);
			if ((rbuffer[0] != 'O') || (rbuffer[1] != 'K')) {
				AfxMessageBox(L"Try to quit AT command error, but didn't get OK response.");
			}
		}

		UpdateData(false);
	}
	else {
		AfxMessageBox(L"Please Connect COM port at first.......", MB_OK);
	}

}


void CUnderwaterAcousticCommunicationDlg::OnBnClickedButtonWriteinfo()
{
	// TODO: Add your control notification handler code here
	int vl1, vl2;
	CString str;
	char initCmd[] = "+++";
	char at_atwr[] = "ATWR\r";
	char at_atcn[] = "ATCN\r";
	char rbuffer[8];

	if (g_stThread != ST_THREAD_STOP) {
		AfxMessageBox(_T("One thread is running, please wait for completion or stop it manually."));
		return;
	}


	if (m_stApp != ST_APP_ZERO) {
		m_serialport.ClearWriteBuffer();
		m_serialport.ClearReadBuffer();
		memset(rbuffer, 0, sizeof(rbuffer));
		// Send "+++" command to enter AT command mode
		m_txLed.SetOnOff(TRUE);
		m_serialport.Write(initCmd, 3);
		Sleep(20);
		m_txLed.SetOnOff(FALSE);
		m_rxLed.SetOnOff(TRUE);
		Sleep(600);
		m_serialport.Read(rbuffer, 3);
		m_rxLed.SetOnOff(FALSE);
		if ((rbuffer[0] != 'O') || (rbuffer[1] != 'K') || (rbuffer[2] != '\r')) {
			AfxMessageBox(L"Cannot find XBee Module, please check the connection.");
		}
		else {
			UpdateData(true);
			// Write destination address to XBee
			vl1 = XbeeSendMsg(ATDH, m_xbeedh, m_xbeedh.GetLength());
			vl2 = XbeeSendMsg(ATDL, m_xbeedl, m_xbeedl.GetLength());
			if ((vl1 != 0) || (vl2 != 0)) {
				AfxMessageBox(L"Configure Destination address error.");
			}
			else {
				// Write network ID and Channel Mask
				vl1 = XbeeSendMsg(ATID, m_xbeeid, m_xbeeid.GetLength());
				vl2 = XbeeSendMsg(ATCM, m_xbeecm, m_xbeecm.GetLength());
				if (vl1 != 0) {
					AfxMessageBox(L"Configure network ID error.");
				}
				else if (vl2 != 0) {
					AfxMessageBox(L"Configure Channel Mask error.");
				}
				else {
					// Write Power Level
					str.Format(L"%d", m_rfPower.GetCurSel());
					vl1 = XbeeSendMsg(ATPL, str, str.GetLength());
					if (vl1 != 0) {
						AfxMessageBox(L"Configure Power Level error.");
					}
					else {
						// Write the parameters to non-volatile memory
						memset(rbuffer, 0, sizeof(rbuffer));
						m_txLed.SetOnOff(TRUE);
						m_serialport.Write(at_atwr, 5);
						Sleep(60);
						m_txLed.SetOnOff(FALSE);
						m_rxLed.SetOnOff(TRUE);
						Sleep(50);
						m_serialport.Read(rbuffer, 3);
						m_rxLed.SetOnOff(FALSE);
						if ((rbuffer[0] != 'O') || (rbuffer[1] != 'K')) {
							AfxMessageBox(L"Store the parameters to XBee error.");
						}
						else {
							memset(rbuffer, 0, sizeof(rbuffer));
							m_txLed.SetOnOff(TRUE);
							m_serialport.Write(at_atcn, 5);
							Sleep(60);
							m_txLed.SetOnOff(FALSE);
							m_rxLed.SetOnOff(TRUE);
							Sleep(50);
							m_serialport.Read(rbuffer, 3);
							m_rxLed.SetOnOff(FALSE);
							if ((rbuffer[0] != 'O') || (rbuffer[1] != 'K')) {
								AfxMessageBox(L"Try to quit AT command error, but didn't get OK response.");
							}
							else {
								AfxMessageBox(L"Write parameter to XBee successfully.", MB_OK);
							}
						}
					}
				}
			}
		}
	}
	else {
		AfxMessageBox(L"Please Connect COM port at first.........", MB_OK);
	}
}


void CUnderwaterAcousticCommunicationDlg::OnBnClickedButtonTimesync()
{
	// TODO: Add your control notification handler code here
	int i, j, check, err, data;
	HTREEITEM hItem, hSubItem, h2ndItem, hPrevItem;
	struct XBeeInfo* pXbee;
	CString str;


	if (g_stThread != ST_THREAD_STOP) {
		AfxMessageBox(_T("One thread is running, please wait for completion or stop it manually."));
		return;
	}

	if ((m_stApp != ST_APP_ZERO) && (m_stApp != ST_APP_CONNECTED)) {
		pXbee = g_xbeeInfo;
		hItem = m_uan.GetRootItem();
		hSubItem = m_uan.GetChildItem(hItem);
		for (i = 0; i < MAX_NUM_OF_NODES; i++) {
			check = 0;
			for (j = 0; j < 4; j++) {
				if ((pXbee->shAddr[j] != 0x00) || (pXbee->slAddr[j] != 0x00)) {
					check = 1;
					break;
				}
			}
			if (check == 1) {
				// Do time synchronization
				err = LocalXbeeMsg("DH", pXbee->shAddr, 4);
				err += LocalXbeeMsg("DL", pXbee->slAddr, 4);
				if (err == 0) {
					if (TimeSync(pXbee->shAddr, pXbee->slAddr) == 0) {
						str = L"Time Sync OK";
					}
					else {
						str = L"Time Sync Failed";
					}
				}
				else {
					str = L"Time Sync Failed";
				}
				
				data = 0;
				hPrevItem = NULL;
				h2ndItem = m_uan.GetChildItem(hSubItem);
				if (h2ndItem != NULL) {
					do {
						data = m_uan.GetItemData(h2ndItem);
						hPrevItem = h2ndItem;
						h2ndItem = m_uan.GetNextSiblingItem(h2ndItem);
					} while ((h2ndItem != NULL) && (data != TREE_TIMESYNC_CODE));
				}
				if (data == TREE_TIMESYNC_CODE) {
					m_uan.DeleteItem(hPrevItem);
				}
				h2ndItem = m_uan.InsertItem(str, NULL, NULL, hSubItem);
				m_uan.SetItemData(h2ndItem, TREE_TIMESYNC_CODE);
			}
			else {
				break;
			}
			++pXbee;
			hSubItem = m_uan.GetNextSiblingItem(hSubItem);
		}
	}
	else {
		AfxMessageBox(_T("Please connect COM port at first, and do network scan..."));
	}
}



void CUnderwaterAcousticCommunicationDlg::OnBnClickedButtonDownload()
{
	// TODO: Add your control notification handler code here
	CString msg, filepath;
	CFileDialog dlg(TRUE);
	CFile uFile;
	CFileException ex;

	if (g_stThread != ST_THREAD_STOP) {
		AfxMessageBox(_T("One thread is running, please wait for completion or stop it manually."));
		return;
	}

	if (dlg.DoModal() == IDOK) {
		filepath = dlg.GetFolderPath();
		filepath += _T("\\");
		filepath += dlg.GetFileName();
		// Open file
		if (uFile.Open((LPCTSTR)filepath, CFile::modeRead, &ex)) {
			m_filepath.SetWindowTextW(filepath);
			m_downloadType = m_dlType.GetCurSel() + 1;
			m_downloadFilelen = (unsigned int)uFile.GetLength();
			m_downloadpBuffer = (unsigned char*)malloc(sizeof(char) * (m_downloadFilelen + 4));
			if (m_downloadpBuffer != NULL) {
				memset(m_downloadpBuffer, 0, m_downloadFilelen);
				uFile.Read(m_downloadpBuffer, m_downloadFilelen);
				uFile.Close();
				// Create Download thread...
				g_stThread = ST_THREAD_DOWNLOAD;
				CreateThread(NULL, 0, DownloadThread, this, 0, NULL);
			}
		}
	}
}


void CUnderwaterAcousticCommunicationDlg::OnBnClickedButtonScan()
{
	// TODO: Add your control notification handler code here
	int i, flag;
	struct ModemInfo* pModem;

	if (g_stThread != ST_THREAD_STOP) {
		AfxMessageBox(_T("One thread is running, please wait for completion or stop it manually."));
		return;
	}

	flag = 0;
	pModem = g_modemInfo;
	if (m_stApp != ST_APP_ZERO) {
		// Check if some modem is in running state
		for (i = 0; i < MAX_NUM_OF_NODES; i++) {
			if (pModem->runstop == MODEM_RUN) {
				flag = 1;
				break;
			}
		}
		if (flag == 0) {
			UpdateData(TRUE);
			g_stThread = ST_THREAD_SCAN;
			CreateThread(NULL, 0, ScanThread, this, 0, NULL);
		}
		else {
			MessageBox(L"One or more modem is in running state. Please stop it at first and then do network scan!");
		}
	}
	else {
		AfxMessageBox(L"How many times do I tell you to connect COM port at first?!", MB_OK);
	}
}


void CUnderwaterAcousticCommunicationDlg::OnBnClickedButtonStopscan()
{
	// TODO: Add your control notification handler code here
	g_endScan = 1;
}


void CUnderwaterAcousticCommunicationDlg::OnOK()
{
	// TODO: Add your specialized code here and/or call the base class


	//CDialogEx::OnOK();
}

void CUnderwaterAcousticCommunicationDlg::OnCancel()
{
	// TODO: Add your specialized code here and/or call the base class

	CDialogEx::OnCancel();
}


void CUnderwaterAcousticCommunicationDlg::OnBeginlabeleditTreeUan(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMTVDISPINFO pTVDispInfo = reinterpret_cast<LPNMTVDISPINFO>(pNMHDR);
	// TODO: Add your control notification handler code here
	if (pTVDispInfo->item.pszText == NULL) {
		m_uan.SetItemText(pTVDispInfo->item.hItem, pTVDispInfo->item.pszText);
	}

	*pResult = 0;
}


void CUnderwaterAcousticCommunicationDlg::OnEndlabeleditTreeUan(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMTVDISPINFO pTVDispInfo = reinterpret_cast<LPNMTVDISPINFO>(pNMHDR);
	// TODO: Add your control notification handler code here
	HTREEITEM hItem, hSubItem;
	struct ModemInfo* pModem;
	CString str;

	pModem = g_modemInfo;
	hItem = m_uan.GetRootItem();
	hSubItem = m_uan.GetChildItem(hItem);
	if (pTVDispInfo->item.lParam == 0) {
		*pResult = 1;
	}
	else {
		do {
			if (hSubItem == pTVDispInfo->item.hItem) {
				if (pModem->runstop == MODEM_RUN) {
					str.Format(L"%s%s", pTVDispInfo->item.pszText, L": Running");
				}
				else {
					str.Format(L"%s%s", pTVDispInfo->item.pszText, L": Stop");
				}
				m_uan.SetItemText(pTVDispInfo->item.hItem, str);
			}
			hSubItem = m_uan.GetNextSiblingItem(hSubItem);
			++pModem;
		} while (hSubItem != NULL);
		*pResult = 0;
	}


}


void CUnderwaterAcousticCommunicationDlg::OnBnClickedButtonUpdateconfigure()
{
	// TODO: Add your control notification handler code here
	int i, j, check, status, count, accum_status, data, err;
	char mAddr;
	HTREEITEM hItem, hSubItem, h2ndItem, hPrevItem;
	struct XBeeInfo* pXbee;
	struct ModemInfo* pModem;
	CString str;


	if (g_stThread != ST_THREAD_STOP) {
		AfxMessageBox(_T("One thread is running, please wait for completion or stop it manually."));
		return;
	}

	if ((m_stApp == ST_APP_ZERO) || (m_stApp == ST_APP_CONNECTED)) {
		AfxMessageBox(_T("Please connect COM port at first, and do network scan..."));
		return;
	}

	pXbee = g_xbeeInfo;
	pModem = g_modemInfo;
	accum_status = 0;
	hItem = m_uan.GetRootItem();
	hSubItem = m_uan.GetChildItem(hItem);
	for (i = 0; i < MAX_NUM_OF_NODES; i++) {
		check = 0;
		for (j = 0; j < 4; j++) {
			if ((pXbee->shAddr[j] != 0x00) || (pXbee->slAddr[j] != 0x00)) {
				check = 1;
				break;
			}
		}
		if (check == 1) {
			// Since it may be an error by calling XbeeConfigure sometimes, we call it in loop for at most 3 times
			count = 0;
			do {
				status = XbeeConfigure(pXbee->shAddr, pXbee->slAddr);
			} while ((status > 0) && (++ count < 3));
			accum_status += status;
			// Do something here to show the status after configuration of each remote Xbee module
			if ((accum_status == 0) && (pModem->addr == 0)) {
				err = LocalXbeeMsg("DH", pXbee->shAddr, 4);
				err += LocalXbeeMsg("DL", pXbee->slAddr, 4);
				if (err == 0) {
					if (GetModemAddr(pXbee->shAddr, pXbee->slAddr, &mAddr) == 0) {
						pModem->addr = mAddr;
						if (mAddr != 0) {
							str.Format(L"Modem Addr: 0x%x", mAddr);
						}
						else {
							str.Format(L"Modem Addr: modem failed");
						}
					}
					else {
						str.Format(L"Modem Addr: remote failed");
					}
				}
				else {
					str.Format(L"Modem Addr: local failed");
				}
				data = 0;
				hPrevItem = NULL;
				h2ndItem = m_uan.GetChildItem(hSubItem);
				if (h2ndItem != NULL) {
					do {
						data = m_uan.GetItemData(h2ndItem);
						hPrevItem = h2ndItem;
						h2ndItem = m_uan.GetNextSiblingItem(h2ndItem);
					} while ((h2ndItem != NULL) && (data != TREE_MODEMADDR_CODE));
				}
				if (data == TREE_MODEMADDR_CODE) {
					m_uan.DeleteItem(hPrevItem);
				}
				h2ndItem = m_uan.InsertItem(str, NULL, NULL, hSubItem);
				m_uan.SetItemData(h2ndItem, TREE_MODEMADDR_CODE);
			}
		}
		else {
			break;
		}
		++pXbee;
		++pModem;
		hSubItem = m_uan.GetNextSiblingItem(hSubItem);
	}

	if (accum_status == 0) {
		AfxMessageBox(L"All remote XBee module has been configured and updated.", MB_OK);
	}

}


void CUnderwaterAcousticCommunicationDlg::OnTvnSelchangedTreeUan(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMTREEVIEW pNMTreeView = reinterpret_cast<LPNMTREEVIEW>(pNMHDR);
	// TODO: Add your control notification handler code here
	struct XBeeInfo* pXbee;
	HTREEITEM hItem, hSubItem;
	int addr, err, offset;

	if (g_stThread != ST_THREAD_STOP) {
		return;
	}

	offset = 0;
	pXbee = g_xbeeInfo;
	hItem = m_uan.GetRootItem();
	hSubItem = m_uan.GetChildItem(hItem);
	do {
		if (hSubItem == pNMTreeView->itemNew.hItem) {
			// found it, and deal with it...
			addr = (((int)(pXbee->shAddr[0]) << 24) & 0xFF000000) + (((int)(pXbee->shAddr[1]) << 16) & 0x00FF0000) +
				(((int)(pXbee->shAddr[2]) << 8) & 0x0000FF00) + ((int)(pXbee->shAddr[3]) & 0x000000FF);
			m_xbeedh.Format(L"%x", addr);
			addr = (((int)(pXbee->slAddr[0]) << 24) & 0xFF000000) + (((int)(pXbee->slAddr[1]) << 16) & 0x00FF0000) +
				(((int)(pXbee->slAddr[2]) << 8) & 0x0000FF00) + ((int)(pXbee->slAddr[3]) & 0x000000FF);
			m_xbeedl.Format(L"%x", addr);
			err = LocalXbeeMsg("DH", pXbee->shAddr, 4);
			err += LocalXbeeMsg("DL", pXbee->slAddr, 4);
			if (err != 0) {
				AfxMessageBox(L"Update DH and DL address for local Address error.");
			}
			g_xbeeOffset = offset;
			// Update power info in UI
			if (g_powerInfo[g_xbeeOffset].light == POWER_DOWN) {
				m_checkLight.SetCheck(FALSE);
			}
			else {
				m_checkLight.SetCheck(TRUE);
			}
			if (g_powerInfo[g_xbeeOffset].bbb == POWER_DOWN) {
				m_checkBBB.SetCheck(FALSE);
			}
			else {
				m_checkBBB.SetCheck(TRUE);
			}
			if (g_powerInfo[g_xbeeOffset].selects == POWER_DOWN) {
				m_checkSelects.SetCheck(FALSE);
			}
			else {
				m_checkSelects.SetCheck(TRUE);
			}
			if (g_powerInfo[g_xbeeOffset].reserved == POWER_DOWN) {
				m_checkReserved.SetCheck(FALSE);
			}
			else {
				m_checkReserved.SetCheck(TRUE);
			}
			break;
		}
		hSubItem = m_uan.GetNextSiblingItem(hSubItem);
		++pXbee;
		++offset;
	} while (hSubItem != NULL);


	*pResult = 0;
	UpdateData(false);
}


void CUnderwaterAcousticCommunicationDlg::OnRclickTreeUan(NMHDR *pNMHDR, LRESULT *pResult)
{
	// TODO: Add your control notification handler code here
	HTREEITEM hItem;
	int data;
	CString str;
	CMenu menu;
	CMenu *pM;
	CPoint pt;
	UINT uFlag;

	uFlag = TVHT_ONITEM;
	GetCursorPos(&pt);
	m_uan.ScreenToClient(&pt);
	hItem = m_uan.HitTest(pt, &uFlag);
	data = m_uan.GetItemData(hItem);
	if ((data >= 1) && (data < 100)) {
		menu.LoadMenu(IDR_UAN_MENU);
		pM = menu.GetSubMenu(0);
		GetCursorPos(&pt);
		pM->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, this);
		m_uan.SelectItem(hItem);
		g_xbeeOffset = m_uan.GetItemData(hItem) - 1;
	}


	*pResult = 0;
}


void CUnderwaterAcousticCommunicationDlg::OnNMCustomdrawTreeUan(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMCUSTOMDRAW pNMCD = reinterpret_cast<LPNMCUSTOMDRAW>(pNMHDR);
	// TODO: Add your control notification handler code here
	*pResult = 0;
}



void CUnderwaterAcousticCommunicationDlg::OnTimesychronizeRun()
{
	// TODO: Add your command handler code here
	int i, data;
	struct XBeeInfo* pXbee;
	struct ModemInfo* pMinfo;
	HTREEITEM hItem, hSubItem, h2ndItem, hPrevItem;
	CString str;

	if (g_stThread != ST_THREAD_STOP) {
		AfxMessageBox(_T("One thread is running, please wait for completion or stop it manually."));
		return;
	}

	if ((g_xbeeOffset < 0) || (g_xbeeOffset > MAX_NUM_OF_NODES)) {
		AfxMessageBox(_T("Please select one node for time sychronization command"));
		return;
	}
	pXbee = &g_xbeeInfo[g_xbeeOffset];
	pMinfo = &g_modemInfo[g_xbeeOffset];
	if (TimeSync(pXbee->shAddr, pXbee->slAddr) == 0) {
		str = L"Time Sync OK";
		pMinfo->timeSync = SYNC_OK;
	}
	else {
		str = L"Time Sync Failed";
		pMinfo->timeSync = SYNC_ERR;
	}

	hItem = m_uan.GetRootItem();
	hSubItem = m_uan.GetChildItem(hItem);
	if (g_xbeeOffset) {
		for (i = 0; i < g_xbeeOffset; i++) {
			hSubItem = m_uan.GetNextSiblingItem(hSubItem);
		}
	}
	data = 0;
	hPrevItem = NULL;
	h2ndItem = m_uan.GetChildItem(hSubItem);
	if (h2ndItem != NULL) {
		do {
			data = m_uan.GetItemData(h2ndItem);
			hPrevItem = h2ndItem;
			h2ndItem = m_uan.GetNextSiblingItem(h2ndItem);
		} while ((h2ndItem != NULL) && (data != TREE_TIMESYNC_CODE));
	}
	if (data == TREE_TIMESYNC_CODE) {
		m_uan.DeleteItem(hPrevItem);
	}
	h2ndItem = m_uan.InsertItem(str, NULL, NULL, hSubItem);
	m_uan.SetItemData(h2ndItem, TREE_TIMESYNC_CODE);

}


void CUnderwaterAcousticCommunicationDlg::OnMenuRun()
{
	// TODO: Add your command handler code here
	struct XBeeInfo* pXbee;
	struct ModemInfo* pMinfo;
	char wbuffer[64], rbuffer[64], addr[8];
	int i, flag, crc, pktRate;
	HTREEITEM hItem, hSubItem;
	CString str;

	if (g_stThread != ST_THREAD_STOP) {
		AfxMessageBox(_T("One thread is running, please wait for completion or stop it manually."));
		return;
	}

	if ((g_xbeeOffset < 0) || (g_xbeeOffset > MAX_NUM_OF_NODES)) {
		AfxMessageBox(_T("Please select one node for run command"));
		return;
	}

	flag = 0;
	pXbee = &g_xbeeInfo[g_xbeeOffset];
	pMinfo = &g_modemInfo[g_xbeeOffset];
	if (pMinfo->runstop == MODEM_STOP) {
		if (m_dlgPara.DoModal() == IDOK) {
			memset(wbuffer, 0, sizeof(wbuffer));
			wbuffer[0] = wbuffer[1] = (char)0xFF;
			wbuffer[2] = wbuffer[3] = 0x00;
			wbuffer[4] = 0x00;
			wbuffer[5] = COMMON_CMD;
			wbuffer[6] = 0x00;
			wbuffer[7] = 16;
			wbuffer[8] = (char)(RUNSTOP_CMD >> 8);
			wbuffer[9] = (char)RUNSTOP_CMD;
			wbuffer[10] = wbuffer[11] = 0x00;
			// Run or Stop
			wbuffer[12] = MODEMRUN_CMD;
			// MAC Protocol
			wbuffer[13] = m_dlgPara.m_macSelect + 1;
			// XID
			wbuffer[14] = (char)m_dlgPara.m_xid;
			// Master Address
			wbuffer[15] = (char)m_dlgPara.m_masteraddr;
			// Packet rate
			pktRate = (int)(m_dlgPara.m_packetrate * 10000);
			wbuffer[16] = (char)(pktRate >> 24);
			wbuffer[17] = (char)(pktRate >> 16);
			wbuffer[18] = (char)(pktRate >> 8);
			wbuffer[19] = (char)pktRate;
			// Reserved
			wbuffer[20] = wbuffer[21] = 0x00;
			wbuffer[22] = wbuffer[23] = 0x00;
			// CRC
			crc = m_serialport.CRC32((unsigned char*)(wbuffer + 8), 16);
			wbuffer[24] = (char)(crc >> 24);
			wbuffer[25] = (char)(crc >> 16);
			wbuffer[26] = (char)(crc >> 8);
			wbuffer[27] = (char)crc;
			memcpy(addr, pXbee->shAddr, 4);
			memcpy((addr + 4), pXbee->slAddr, 4);
			// Here we send Tx request frame and get Rx indicator frame in API mode
			TxXbeeMsg(wbuffer, 28, addr, rbuffer);
		}
		else {
			flag = 1;
		}
		if (flag == 0) {
			if (CheckACK(rbuffer, RUNSTOP_CMD) == 0) {
				AfxMessageBox(L"The MAC protocol is running.");
				pMinfo->runstop = MODEM_RUN;
				hItem = m_uan.GetRootItem();
				hSubItem = m_uan.GetChildItem(hItem);
				for (i = 0; i < g_xbeeOffset; i++) {
					hSubItem = m_uan.GetNextSiblingItem(hSubItem);
				}
				str = m_uan.GetItemText(hSubItem);
				str = str.Left(str.GetLength() - 4);
				str = str + L"Running";
				m_uan.SetItemText(hSubItem, str);
				//m_uan.SetItemState(hSubItem, TVIS_BOLD, TVIS_BOLD);
			}
			else {
				AfxMessageBox(L"The MAC protocol may not run due to some errors.");
			}
		}
	}
	else {
		MessageBox(L"The Modem has been in RUN state.");
	}
}


void CUnderwaterAcousticCommunicationDlg::OnMenuStop()
{
	// TODO: Add your command handler code here
	struct XBeeInfo* pXbee;
	struct ModemInfo* pMinfo;
	char wbuffer[32], rbuffer[32], addr[8];
	HTREEITEM hItem, hSubItem;
	int i, crc, pktRate;
	CString str;

	if (g_stThread != ST_THREAD_STOP) {
		AfxMessageBox(_T("One thread is running, please wait for completion or stop it manually."));
		return;
	}

	if ((g_xbeeOffset < 0) || (g_xbeeOffset > MAX_NUM_OF_NODES)) {
		AfxMessageBox(_T("Please select one node for stop command"));
		return;
	}

	pXbee = &g_xbeeInfo[g_xbeeOffset];
	pMinfo = &g_modemInfo[g_xbeeOffset];
	if (pMinfo->runstop == MODEM_RUN) {
		memset(wbuffer, 0, sizeof(wbuffer));
		wbuffer[0] = wbuffer[1] = (char)0xFF;
		wbuffer[2] = wbuffer[3] = 0x00;
		wbuffer[4] = 0x00;
		wbuffer[5] = COMMON_CMD;
		wbuffer[6] = 0x00;
		wbuffer[7] = 16;
		wbuffer[8] = (char)(RUNSTOP_CMD >> 8);
		wbuffer[9] = (char)RUNSTOP_CMD;
		wbuffer[10] = wbuffer[11] = 0x00;
		// Run or Stop
		wbuffer[12] = MODEMSTOP_CMD;
		// MAC Protocol
		wbuffer[13] = m_dlgPara.m_macSelect + 1;
		// XID
		wbuffer[14] = (char)m_dlgPara.m_xid;
		// Master Address
		wbuffer[15] = (char)m_dlgPara.m_masteraddr;
		// Packet rate
		pktRate = (int)(m_dlgPara.m_packetrate * 10000);
		wbuffer[16] = (char)(pktRate >> 24);
		wbuffer[17] = (char)(pktRate >> 16);
		wbuffer[18] = (char)(pktRate >> 8);
		wbuffer[19] = (char)pktRate;
		// Reserved
		wbuffer[20] = wbuffer[21] = 0x00;
		wbuffer[22] = wbuffer[23] = 0x00;
		// CRC
		crc = m_serialport.CRC32((unsigned char*)(wbuffer + 8), 16);
		wbuffer[24] = (char)(crc >> 24);
		wbuffer[25] = (char)(crc >> 16);
		wbuffer[26] = (char)(crc >> 8);
		wbuffer[27] = (char)crc;
		memcpy(addr, pXbee->shAddr, 4);
		memcpy((addr + 4), pXbee->slAddr, 4);
		// Here we send Tx request frame and get Rx indicator frame in API mode
		TxXbeeMsg(wbuffer, 28, addr, rbuffer);
		if (CheckACK(rbuffer, RUNSTOP_CMD) == 0) {
			AfxMessageBox(L"The MAC protocol is stopped.");
			pMinfo->runstop = MODEM_STOP;
			hItem = m_uan.GetRootItem();
			hSubItem = m_uan.GetChildItem(hItem);
			for (i = 0; i < g_xbeeOffset; i++) {
				hSubItem = m_uan.GetNextSiblingItem(hSubItem);
			}
			str = m_uan.GetItemText(hSubItem);
			str = str.Left(str.GetLength() - 7);
			str = str + L"Stop";
			m_uan.SetItemText(hSubItem, str);
		}
		else {
			AfxMessageBox(L"The MAC protocol may not stop due to some errors.");
		}
	}
	else {
		MessageBox(L"The Modem has been in STOP state.");
	}
}


void CUnderwaterAcousticCommunicationDlg::OnBnClickedMonitorstart()
{
	// TODO: Add your control notification handler code here
	int i, j, check, rowCnt;
	LVITEM lvi;
	CString strItem;
	struct XBeeInfo* pXbee;
	struct ModemInfo* pMinfo;
	HTREEITEM hItem, hSubItem;

	if (g_stThread != ST_THREAD_STOP) {
		AfxMessageBox(_T("One thread is running, please wait for completion or stop it manually."));
		return;
	}

	i = rowCnt = 0;
	pXbee = g_xbeeInfo;
	pMinfo = g_modemInfo;
	hItem = m_uan.GetRootItem();
	hSubItem = m_uan.GetChildItem(hItem);
	m_List.DeleteAllItems();
	do {
		check = 0;
		for (j = 0; j < 4; j++) {
			if ((pXbee->shAddr[j] != 0x00) || (pXbee->slAddr[j] != 0x00)) {
				check = 1;
				break;
			}
		}
		if (check == 1) {
			if (pMinfo->runstop == MODEM_RUN) {
				// Insert the Name
				lvi.mask = LVIF_IMAGE | LVIF_TEXT;
				strItem = m_uan.GetItemText(hSubItem);
				strItem = strItem.Left(strItem.GetLength() - 9);
				lvi.iItem = rowCnt;
				lvi.iSubItem = 0;
				lvi.pszText = (LPTSTR)(LPCTSTR)(strItem);
				m_List.InsertItem(&lvi);
				// Modem Address
				strItem.Format(L"0x%x", pMinfo->addr);
				lvi.iSubItem = 1;
				lvi.pszText = (LPTSTR)(LPCTSTR)(strItem);
				m_List.SetItem(&lvi);
				// State
				strItem.Format(L"OK");
				lvi.iSubItem = 2;
				lvi.pszText = (LPTSTR)(LPCTSTR)(strItem);
				m_List.SetItem(&lvi);
				// monitor data 1
				strItem.Format(L"0");
				lvi.iSubItem = 3;
				lvi.pszText = (LPTSTR)(LPCTSTR)(strItem);
				m_List.SetItem(&lvi);
				// monitor data 2
				strItem.Format(L"0");
				lvi.iSubItem = 4;
				lvi.pszText = (LPTSTR)(LPCTSTR)(strItem);
				m_List.SetItem(&lvi);
				// monitor data 3
				strItem.Format(L"0");
				lvi.iSubItem = 5;
				lvi.pszText = (LPTSTR)(LPCTSTR)(strItem);
				m_List.SetItem(&lvi);
				// monitor data 4
				strItem.Format(L"0");
				lvi.iSubItem = 6;
				lvi.pszText = (LPTSTR)(LPCTSTR)(strItem);
				m_List.SetItem(&lvi);
				// monitor data 5
				strItem.Format(L"0");
				lvi.iSubItem = 7;
				lvi.pszText = (LPTSTR)(LPCTSTR)(strItem);
				m_List.SetItem(&lvi);

				// Remember the list offset
				pMinfo->listOffset = rowCnt;

				++rowCnt;
			}
			hSubItem = m_uan.GetNextSiblingItem(hSubItem);
		}
		++pXbee;
		++pMinfo;
	} while (++ i < MAX_NUM_OF_NODES);

	if (rowCnt > 0) {
		// Start thread for monitoring
		g_stThread = ST_THREAD_MONITOR;
		CreateThread(NULL, 0, MonitorThread, this, 0, NULL);
	}
	else {
		MessageBox(L"No available node for monitoring...");
	}
}



void CUnderwaterAcousticCommunicationDlg::OnBnClickedMonitorstop()
{
	// TODO: Add your control notification handler code here
	if (g_stThread == ST_THREAD_MONITOR) {
		g_endMonitor = MONITOR_STOP;
		// A little delay needed for the termination of monitor thread
		Sleep(200);
		MessageBox(L"Monitor stopped....");
	}
}


void CUnderwaterAcousticCommunicationDlg::OnClickedCheckLight()
{
	// TODO: Add your control notification handler code here
	int err;
	char para, dAddr[8];
	struct XBeeInfo* pXbee;

	if (g_stThread != ST_THREAD_STOP) {
		AfxMessageBox(_T("One thread is running, please wait for completion or stop it manually."));
		if (m_checkLight.GetCheck() == FALSE) {
			m_checkLight.SetCheck(TRUE);
		}
		else {
			m_checkLight.SetCheck(FALSE);
		}
		return;
	}

	if ((g_xbeeOffset < 0) || (g_xbeeOffset > MAX_NUM_OF_NODES)) {
		AfxMessageBox(_T("Please select the node..."));
		if (m_checkLight.GetCheck() == FALSE) {
			m_checkLight.SetCheck(TRUE);
		}
		else {
			m_checkLight.SetCheck(FALSE);
		}
		return;
	}

	pXbee = &g_xbeeInfo[g_xbeeOffset];
	if (m_checkLight.GetCheck()) {
		para = POWER_UP;
	}
	else {
		para = POWER_DOWN;
	}
	memcpy(dAddr, pXbee->shAddr, XBEE_SHADDR_LEN);
	memcpy((dAddr + XBEE_SHADDR_LEN), pXbee->slAddr, XBEE_SLADDR_LEN);
	err = RemoteXbeeMsg(dAddr, PWRLIGHT_CMD, &para, 1);
	if (err != 0) {
		AfxMessageBox(_T("Control power of light error."));
	}
	else {
		g_powerInfo[g_xbeeOffset].light = para;
	}
}


void CUnderwaterAcousticCommunicationDlg::OnClickedCheckSelects()
{
	// TODO: Add your control notification handler code here
	int err;
	char para, dAddr[8];
	struct XBeeInfo* pXbee;

	if (g_stThread != ST_THREAD_STOP) {
		AfxMessageBox(_T("One thread is running, please wait for completion or stop it manually."));
		if (m_checkSelects.GetCheck() == FALSE) {
			m_checkSelects.SetCheck(TRUE);
		}
		else {
			m_checkSelects.SetCheck(FALSE);
		}
		return;
	}

	if ((g_xbeeOffset < 0) || (g_xbeeOffset > MAX_NUM_OF_NODES)) {
		AfxMessageBox(_T("Please select the node..."));
		if (m_checkSelects.GetCheck() == FALSE) {
			m_checkSelects.SetCheck(TRUE);
		}
		else {
			m_checkSelects.SetCheck(FALSE);
		}
		return;
	}

	pXbee = &g_xbeeInfo[g_xbeeOffset];
	if (m_checkSelects.GetCheck()) {
		para = POWER_UP;
	}
	else {
		para = POWER_DOWN;
	}
	memcpy(dAddr, pXbee->shAddr, XBEE_SHADDR_LEN);
	memcpy((dAddr + XBEE_SHADDR_LEN), pXbee->slAddr, XBEE_SLADDR_LEN);
	err = RemoteXbeeMsg(dAddr, PWRSELECTS_CMD, &para, 1);
	if (err != 0) {
		AfxMessageBox(_T("Control power of selects error."));
	}
	else {
		g_powerInfo[g_xbeeOffset].selects = para;
	}
}


void CUnderwaterAcousticCommunicationDlg::OnClickedCheckBeaglebone()
{
	// TODO: Add your control notification handler code here
	int err;
	char para, dAddr[8];
	struct XBeeInfo* pXbee;

	if (g_stThread != ST_THREAD_STOP) {
		AfxMessageBox(_T("One thread is running, please wait for completion or stop it manually."));
		if (m_checkBBB.GetCheck() == FALSE) {
			m_checkBBB.SetCheck(TRUE);
		}
		else {
			m_checkBBB.SetCheck(FALSE);
		}
		return;
	}

	if ((g_xbeeOffset < 0) || (g_xbeeOffset > MAX_NUM_OF_NODES)) {
		AfxMessageBox(_T("Please select the node..."));
		if (m_checkBBB.GetCheck() == FALSE) {
			m_checkBBB.SetCheck(TRUE);
		}
		else {
			m_checkBBB.SetCheck(FALSE);
		}
		return;
	}

	pXbee = &g_xbeeInfo[g_xbeeOffset];
	if (m_checkBBB.GetCheck()) {
		para = POWER_UP;
	}
	else {
		para = POWER_DOWN;
	}
	memcpy(dAddr, pXbee->shAddr, XBEE_SHADDR_LEN);
	memcpy((dAddr + XBEE_SHADDR_LEN), pXbee->slAddr, XBEE_SLADDR_LEN);
	err = RemoteXbeeMsg(dAddr, PWRBBB_CMD, &para, 1);
	if (err != 0) {
		AfxMessageBox(_T("Control power of BeagleBone error."));
	}
	else {
		g_powerInfo[g_xbeeOffset].bbb = para;
	}
}


void CUnderwaterAcousticCommunicationDlg::OnClickedCheckReserved()
{
	// TODO: Add your control notification handler code here
	int err;
	char para, dAddr[8];
	struct XBeeInfo* pXbee;

	if (g_stThread != ST_THREAD_STOP) {
		AfxMessageBox(_T("One thread is running, please wait for completion or stop it manually."));
		if (m_checkReserved.GetCheck() == FALSE) {
			m_checkReserved.SetCheck(TRUE);
		}
		else {
			m_checkReserved.SetCheck(FALSE);
		}
		return;
	}

	if ((g_xbeeOffset < 0) || (g_xbeeOffset > MAX_NUM_OF_NODES)) {
		AfxMessageBox(_T("Please select the node..."));
		if (m_checkReserved.GetCheck() == FALSE) {
			m_checkReserved.SetCheck(TRUE);
		}
		else {
			m_checkReserved.SetCheck(FALSE);
		}
		return;
	}

	pXbee = &g_xbeeInfo[g_xbeeOffset];
	if (m_checkReserved.GetCheck()) {
		para = POWER_UP;
	}
	else {
		para = POWER_DOWN;
	}

	memcpy(dAddr, pXbee->shAddr, XBEE_SHADDR_LEN);
	memcpy((dAddr + XBEE_SHADDR_LEN), pXbee->slAddr, XBEE_SLADDR_LEN);
	err = RemoteXbeeMsg(dAddr, PWRRESERVED_CMD, &para, 1);
	if (err != 0) {
		AfxMessageBox(_T("Control power of reserved error."));
	}
	else {
		g_powerInfo[g_xbeeOffset].reserved = para;
	}
}


void CUnderwaterAcousticCommunicationDlg::OnBnClickedButtonRemotewakeup()
{
	// TODO: Add your control notification handler code here
	if (g_stThread != ST_THREAD_STOP) {
		AfxMessageBox(_T("One thread is running, please wait for completion or stop it manually."));
		return;
	}

	if (m_stApp == ST_APP_ZERO) {
		AfxMessageBox(_T("Please connect COM port at first......"), MB_OK);
		return;
	}

	g_stThread = ST_THREAD_WAKEUP;
	CreateThread(NULL, 0, WakeupThread, this, 0, NULL);
}


void CUnderwaterAcousticCommunicationDlg::OnBnClickedButtonRemotsleep()
{
	// TODO: Add your control notification handler code here
	int err, length, i, val_h, val_l, temp;
	WCHAR wch[64];
	char message[64], dAddr[8];
	char para;

	if (g_stThread != ST_THREAD_STOP) {
		AfxMessageBox(_T("One thread is running, please wait for completion or stop it manually."));
		return;
	}

	if (m_stApp == ST_APP_ZERO) {
		AfxMessageBox(_T("Please connect COM port at first........."), MB_OK);
		return;
	}

	err = val_h = val_l = 0;
	length = m_xbeedh.GetLength();
	wcscpy_s(wch, CT2CW(m_xbeedh));
	UnicodeToAnsi(wch, message);
	length = m_xbeedh.GetLength();
	for (i = 0; i < length; i++) {
		if ((message[i] >= 0x30) && (message[i] <= 0x39)) {
			temp = (int)(message[i] - 48);
		}
		else if ((message[i] >= 0x41) && (message[i] <= 0x46)) {
			temp = (int)(message[i] - 55);
		}
		else if ((message[i] >= 0x61) && (message[i] <= 0x66)) {
			temp = (int)(message[i] - 87);
		}
		else {
			err = 1;
			break;
		}
		val_h += (temp << (4 * (length - i - 1)));
	}
	length = m_xbeedl.GetLength();
	wcscpy_s(wch, CT2CW(m_xbeedl));
	UnicodeToAnsi(wch, message);
	for (i = 0; i < length; i++) {
		if ((message[i] >= 0x30) && (message[i] <= 0x39)) {
			temp = (int)(message[i] - 48);
		}
		else if ((message[i] >= 0x41) && (message[i] <= 0x46)) {
			temp = (int)(message[i] - 55);
		}
		else if ((message[i] >= 0x61) && (message[i] <= 0x66)) {
			temp = (int)(message[i] - 87);
		}
		else {
			err = 1;
			break;
		}
		val_l += (temp << (4 * (length - i - 1)));
	}
	if (err == 0) {
		for (i = 0; i < 4; i++) {
			dAddr[i] = val_h >> ((4 - i - 1) * 8);
			dAddr[4 + i] = val_l >> ((4 - i - 1) * 8);
		}
		para = 0x4;
		err = RemoteXbeeMsg(dAddr, "SM", &para, 1);
	}

	if (err == 0) {
		AfxMessageBox(_T("The remote XBee enters sleep mode successfully"));
	}
	else {
		AfxMessageBox(_T("Error occurs, please try again."));
	}
}
