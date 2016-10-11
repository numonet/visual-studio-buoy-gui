
// Underwater Acoustic CommunicationDlg.h : header file
//

#pragma once
#include "afxwin.h"
#include "afxcmn.h"
#include "serialport.h"
#include "DynamicLED.h"
#include "DialogPara.h"








// CUnderwaterAcousticCommunicationDlg dialog
class CUnderwaterAcousticCommunicationDlg : public CDialogEx
{
// Construction
public:
	CUnderwaterAcousticCommunicationDlg(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
	enum { IDD = IDD_UNDERWATERACOUSTICCOMMUNICATION_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support

	static DWORD WINAPI DownloadThread(LPVOID lpPara);
	static DWORD WINAPI ScanThread(LPVOID lpPara);
	static DWORD WINAPI WakeupThread(LPVOID lpPara);
	static DWORD WINAPI MonitorThread(LPVOID lpPara);

	void AnsiToUnicode(WCHAR* str, const char* szStr);
	void UnicodeToAnsi(wchar_t* wszString, char* szAnsi);
	int XbeeSendMsg(char* cmd, CString data, int data_length);
	int XbeeAddrConverter(char* SH, char* SL, char* addrConverted);
	int XbeeConfigure(char* remote_sh, char* remote_sl);
	int RemoteXbeeMsg(char* destinationAddr, char* atCmd, char* para, int paraLen);
	int LocalXbeeMsg(char* atCmd, char* para, int paraLen);
	int TxXbeeMsg(char* data, int len, char* dAddr, char* recvBuf);
	int RssiXbeeMsg(char* remote_sh, char* remote_sl, char* rssi);
	int CheckACK(char* message, int subCmd);
	int Download(char* remote_addr);
	int ScanNetwork(void);
	int WakeupXbee(void);
	int GetModemAddr(char* remote_sh, char* remote_sl, char* modemAddr);
	int TimeSync(char* remote_sh, char* remote_sl);
	

// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	CSerialPort m_serialport;
	CDialogPara m_dlgPara;

	int m_xbeeNodeNum;
	int m_serialportFlag;
	int m_runstopFlag;
	int m_downloadType;
	int m_downloadFilelen;
	int m_scantime;
	unsigned char* m_downloadpBuffer;

	CString m_xbeedh;
	CString m_xbeedl;
	CString m_xbeeid;
	CString m_xbeecm;
	CString m_xbeesh;
	CString m_xbeesl;
	CButton m_checkLight;
	CButton m_checkSelects;
	CButton m_checkBBB;
	CButton m_checkReserved;
	CComboBox m_comport;
	CComboBox m_baudrate;
	CComboBox m_xbeeMode;
	CComboBox m_rfPower;
	CComboBox m_dlType;
	CComboBox m_ulType;
	CProgressCtrl m_progress;
	CListCtrl m_List;
	CButton m_connect;
	CEdit m_filepath;
	CDynamicLED m_txLed;
	CDynamicLED m_rxLed;
	CTreeCtrl m_uan;

	afx_msg void OnBnClickedButtonConnect();
	afx_msg void OnCbnSelchangeComboXbeemode();
	afx_msg void OnBnClickedButtonReadinfo2();
	afx_msg void OnBnClickedButtonWriteinfo();
	afx_msg void OnBnClickedButtonTimesync();
	afx_msg void OnBnClickedButtonDownload();
	afx_msg void OnBnClickedButtonScan();
	afx_msg void OnBeginlabeleditTreeUan(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnEndlabeleditTreeUan(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnBnClickedButtonUpdateconfigure();
	afx_msg void OnTvnSelchangedTreeUan(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnRclickTreeUan(NMHDR *pNMHDR, LRESULT *pResult);

	virtual void OnOK();
	virtual void OnCancel();
	afx_msg void OnTimesychronizeRun();
	afx_msg void OnMenuRun();
	afx_msg void OnMenuStop();
	afx_msg void OnBnClickedMonitorstart();
	afx_msg void OnBnClickedButtonStopscan();
	afx_msg void OnNMCustomdrawTreeUan(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnBnClickedMonitorstop();
	afx_msg void OnClickedCheckLight();
	afx_msg void OnClickedCheckSelects();
	afx_msg void OnClickedCheckBeaglebone();
	afx_msg void OnClickedCheckReserved();
	afx_msg void OnBnClickedButtonRemotewakeup();
	afx_msg void OnBnClickedButtonRemotsleep();
};
