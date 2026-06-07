// CircleDrawer.cpp: Application class implementation (Dialog-based).

#include "pch.h"
#include "framework.h"
#include "afxwinappex.h"
#include "afxdialogex.h"
#include "CircleDrawer.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CCircleDrawerApp, CWinAppEx)
    ON_COMMAND(ID_APP_ABOUT, &CCircleDrawerApp::OnAppAbout)
END_MESSAGE_MAP()

// ---------------------------------------------------------------------------
// CCircleDrawerApp construction
// ---------------------------------------------------------------------------

CCircleDrawerApp::CCircleDrawerApp() noexcept
{
    m_bHiColorIcons = TRUE;
    m_nAppLook = 0;
    SetAppID(_T("CircleDrawer.AppID.NoVersion"));
}

CCircleDrawerApp theApp;

// ---------------------------------------------------------------------------
// CCircleDrawerApp initialization — Dialog-based
// ---------------------------------------------------------------------------

BOOL CCircleDrawerApp::InitInstance()
{
    INITCOMMONCONTROLSEX InitCtrls;
    InitCtrls.dwSize = sizeof(InitCtrls);
    InitCtrls.dwICC  = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&InitCtrls);

    CWinApp::InitInstance();

    AfxEnableControlContainer();

    SetRegistryKey(_T("CircleDrawer"));

    // Show the main dialog (dialog-based app)
    CCircleDrawerDlg dlg;
    m_pMainWnd = &dlg;
    dlg.DoModal();

    // Return FALSE so MFC exits the message loop after DoModal returns.
    return FALSE;
}

int CCircleDrawerApp::ExitInstance()
{
    return CWinApp::ExitInstance();
}

// ---------------------------------------------------------------------------
// PreLoadState / LoadCustomState / SaveCustomState
// Not needed for dialog-based app; override to do nothing.
// ---------------------------------------------------------------------------

void CCircleDrawerApp::PreLoadState()
{
    // No MDI context-menu resources to load.
}

void CCircleDrawerApp::LoadCustomState()
{
}

void CCircleDrawerApp::SaveCustomState()
{
}

// ---------------------------------------------------------------------------
// About dialog
// ---------------------------------------------------------------------------

class CAboutDlg : public CDialogEx
{
public:
    CAboutDlg() noexcept : CDialogEx(IDD_ABOUTBOX) {}
    enum { IDD = IDD_ABOUTBOX };
protected:
    virtual void DoDataExchange(CDataExchange* pDX) { CDialogEx::DoDataExchange(pDX); }
    DECLARE_MESSAGE_MAP()
};

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()

void CCircleDrawerApp::OnAppAbout()
{
    CAboutDlg aboutDlg;
    aboutDlg.DoModal();
}
