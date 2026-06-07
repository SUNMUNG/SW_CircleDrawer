#pragma once

// afxdialogex.h must come before any code that uses CDialogEx.
// (Framework.h may not include it transitively.)
#include "afxdialogex.h"
#include "resource.h"

// Avoid _USE_MATH_DEFINES / PCH ordering issues: define PI directly.
static constexpr double CD_PI = 3.14159265358979323846;

// Custom window messages (posted from worker thread to main thread)
#define WM_RANDOM_UPDATE  (WM_USER + 1)
#define WM_RANDOM_DONE    (WM_USER + 2)

class CCircleDrawerDlg : public CDialogEx
{
    DECLARE_DYNAMIC(CCircleDrawerDlg)

public:
    CCircleDrawerDlg(CWnd* pParent = nullptr);
    virtual ~CCircleDrawerDlg();
    enum { IDD = IDD_MAINDIALOG };

protected:
    virtual BOOL OnInitDialog();
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual void OnOK() {}   // Prevent Enter key from closing dialog
    virtual void OnCancel();

    DECLARE_MESSAGE_MAP()

private:
    // --- State ---
    CPoint  m_pts[3];        // Click points in client (dialog) coordinates
    int     m_nClickCount;   // 0..3
    bool    m_bCircleReady;  // True after 3rd click and valid circumscribed circle found

    // --- Circumscribed circle ---
    double  m_cx, m_cy, m_cr;

    // --- User settings ---
    int     m_nDotRadius;    // Radius of each click-point filled circle (pixels)
    int     m_nLineWidth;    // Stroke width of the circumscribed circle (pixels)

    // --- Drag state ---
    int     m_nDragIdx;      // Index (0/1/2) of point being dragged; -1 = none

    // --- Drawing area (client coordinates of the dialog) ---
    CRect   m_drawRect;

    // --- Controls ---
    CEdit   m_editDotRadius;
    CEdit   m_editLineWidth;
    CStatic m_stcP1, m_stcP2, m_stcP3;
    CButton m_btnReset, m_btnRandom;

    // --- Random-move thread ---
    HANDLE           m_hThread;
    volatile bool    m_bThreadStop;
    CRITICAL_SECTION m_cs;

    // --- Drawing ---
    // Filled circle via horizontal scanlines — no Ellipse / GDI+ API.
    void DrawFilledCircle(CDC* pDC, int cx, int cy, int r);

    // Open (unfilled) circle via parametric LineTo segments — no Ellipse / GDI+ API.
    void DrawOpenCircle(CDC* pDC, double cx, double cy, double r, int lineWidth);

    // Full double-buffered scene render into the drawing area.
    void DrawScene(CDC* pDC);

    // Compute circumscribed circle from m_pts[0..2].
    bool ComputeCircumscribed();

    // Return index (0/1/2) of click point within hit-test radius, or -1.
    int  HitTestPoint(CPoint pt) const;

    // Refresh coordinate labels in the control panel.
    void UpdateCoordLabels();

    // Read dot-radius and line-width from edit controls.
    void UpdateSettings();

    // Trigger a flicker-free repaint of the drawing area only.
    void Redraw();

    // Reset all state to initial.
    void ResetAll();

    // Body of the random-move background thread (runs on worker thread).
    void DoRandomMove();

    // Thread entry point (static, calls DoRandomMove).
    static DWORD WINAPI RandomThreadProc(LPVOID pParam);

    // --- Message handlers ---
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnBnClickedReset();
    afx_msg void OnBnClickedRandom();
    afx_msg void OnDestroy();
    afx_msg LRESULT OnRandomUpdate(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnRandomDone(WPARAM wParam, LPARAM lParam);
};
