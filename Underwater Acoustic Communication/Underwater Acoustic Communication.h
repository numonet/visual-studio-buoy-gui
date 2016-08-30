
// Underwater Acoustic Communication.h : main header file for the PROJECT_NAME application
//

#pragma once

#ifndef __AFXWIN_H__
	#error "include 'stdafx.h' before including this file for PCH"
#endif

#include "resource.h"		// main symbols


// CUnderwaterAcousticCommunicationApp:
// See Underwater Acoustic Communication.cpp for the implementation of this class
//

class CUnderwaterAcousticCommunicationApp : public CWinApp
{
public:
	CUnderwaterAcousticCommunicationApp();

// Overrides
public:
	virtual BOOL InitInstance();

// Implementation

	DECLARE_MESSAGE_MAP()
};

extern CUnderwaterAcousticCommunicationApp theApp;