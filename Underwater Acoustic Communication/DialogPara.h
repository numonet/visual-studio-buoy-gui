#pragma once
#include "afxwin.h"


// CDialogPara dialog

class CDialogPara : public CDialog
{
	DECLARE_DYNAMIC(CDialogPara)

public:
	CDialogPara(CWnd* pParent = NULL);   // standard constructor
	virtual ~CDialogPara();

// Dialog Data
	enum { IDD = IDD_DIALOGPARA };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	int m_xid;
	int m_masteraddr;
	int m_macSelect;
	float m_packetrate;
	CComboBox m_macProtocol;
	virtual BOOL OnInitDialog();
	afx_msg void OnSelchangeComboMacprotocol();
};
