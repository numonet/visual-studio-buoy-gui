// DialogPara.cpp : implementation file
//

#include "stdafx.h"
#include "Underwater Acoustic Communication.h"
#include "DialogPara.h"
#include "afxdialogex.h"


// CDialogPara dialog

IMPLEMENT_DYNAMIC(CDialogPara, CDialog)

CDialogPara::CDialogPara(CWnd* pParent /*=NULL*/)
	: CDialog(CDialogPara::IDD, pParent)
	, m_xid(0)
	, m_masteraddr(0)
	, m_packetrate(0)
{

}

CDialogPara::~CDialogPara()
{
}

void CDialogPara::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_XID, m_xid);
	DDX_Text(pDX, IDC_EDIT_MASTERADDR, m_masteraddr);
	DDX_Text(pDX, IDC_EDIT_PACKETRATE, m_packetrate);
	DDX_Control(pDX, IDC_COMBO_MACPROTOCOL, m_macProtocol);
}


BEGIN_MESSAGE_MAP(CDialogPara, CDialog)
	ON_CBN_SELCHANGE(IDC_COMBO_MACPROTOCOL, &CDialogPara::OnSelchangeComboMacprotocol)
END_MESSAGE_MAP()


// CDialogPara message handlers


BOOL CDialogPara::OnInitDialog()
{
	CDialog::OnInitDialog();

	// TODO:  Add extra initialization here
	m_macProtocol.AddString(_T("Aloha"));
	m_macProtocol.AddString(_T("Slotted Aloha"));
	m_macProtocol.AddString(_T("TARS"));
	m_macProtocol.AddString(_T("LiSS"));
	m_macProtocol.SetCurSel(3);
	m_macSelect = 3;
	m_xid = 0;
	m_masteraddr = 0;
	m_packetrate = 0.0;

	this->SetWindowTextW(L"Modem Parameter Setting...");

	UpdateData(FALSE);

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}


void CDialogPara::OnSelchangeComboMacprotocol()
{
	// TODO: Add your control notification handler code here
	m_macSelect = m_macProtocol.GetCurSel();
}
