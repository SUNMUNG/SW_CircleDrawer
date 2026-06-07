// CircleDrawerDlg.cpp
//LineTo 기반으로 원을 직접 그림

#include "pch.h"
#include "framework.h"
#include "CircleDrawer.h"
#include "CircleDrawerDlg.h"
#include "afxdialogex.h"
#include <cmath>
#include <cstdlib>
#include <ctime>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNAMIC(CCircleDrawerDlg, CDialogEx)

// 메시지 → 핸들러 함수 연결 테이블
BEGIN_MESSAGE_MAP(CCircleDrawerDlg, CDialogEx)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_LBUTTONDOWN()
    ON_WM_MOUSEMOVE()
    ON_WM_LBUTTONUP()
    ON_WM_DESTROY()
    ON_BN_CLICKED(IDC_BTN_RESET,  &CCircleDrawerDlg::OnBnClickedReset)
    ON_BN_CLICKED(IDC_BTN_RANDOM, &CCircleDrawerDlg::OnBnClickedRandom)
    ON_MESSAGE(WM_RANDOM_UPDATE,  &CCircleDrawerDlg::OnRandomUpdate)
    ON_MESSAGE(WM_RANDOM_DONE,    &CCircleDrawerDlg::OnRandomDone)
END_MESSAGE_MAP()

// 생성자: 멤버 변수 초기값 설정, 난수 시드, 임계 구역 초기화
CCircleDrawerDlg::CCircleDrawerDlg(CWnd* pParent)
    : CDialogEx(IDD_MAINDIALOG, pParent)
    , m_nClickCount(0)
    , m_bCircleReady(false)
    , m_cx(0.0), m_cy(0.0), m_cr(0.0)
    , m_nDotRadius(10)
    , m_nLineWidth(2)
    , m_nDragIdx(-1)
    , m_hThread(nullptr)
    , m_bThreadStop(false)
{
    m_pts[0] = m_pts[1] = m_pts[2] = CPoint(0, 0);
    m_drawRect.SetRectEmpty();
    std::srand(static_cast<unsigned>(std::time(nullptr)));
    InitializeCriticalSection(&m_cs);  // 스레드 간 m_pts 보호용 임계 구역
}

CCircleDrawerDlg::~CCircleDrawerDlg()
{
    DeleteCriticalSection(&m_cs);
}

// RC 컨트롤 ID ↔ 멤버 변수 바인딩
void CCircleDrawerDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_EDIT_DOT_RADIUS, m_editDotRadius);
    DDX_Control(pDX, IDC_EDIT_LINE_WIDTH,  m_editLineWidth);
    DDX_Control(pDX, IDC_STATIC_P1,        m_stcP1);
    DDX_Control(pDX, IDC_STATIC_P2,        m_stcP2);
    DDX_Control(pDX, IDC_STATIC_P3,        m_stcP3);
    DDX_Control(pDX, IDC_BTN_RESET,        m_btnReset);
    DDX_Control(pDX, IDC_BTN_RANDOM,       m_btnRandom);
}

// 다이얼로그 초기화: 한국어 레이블 설정 + 그림 영역(m_drawRect) 계산
BOOL CCircleDrawerDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    SetWindowText(_T("Circle Drawer - 세 점을 지나는 정원"));

    // RC 파일은 ASCII 텍스트 → 런타임에 한국어로 교체
    GetDlgItem(IDC_LABEL_DOT_RADIUS)->SetWindowText(_T("점 원 반지름:"));
    GetDlgItem(IDC_LABEL_LINE_WIDTH)->SetWindowText(_T("정원 선 두께:"));
    GetDlgItem(IDC_LABEL_COORD_HEADER)->SetWindowText(_T("클릭 지점 좌표:"));
    GetDlgItem(IDC_LABEL_P1)->SetWindowText(_T("P1:"));
    GetDlgItem(IDC_LABEL_P2)->SetWindowText(_T("P2:"));
    GetDlgItem(IDC_LABEL_P3)->SetWindowText(_T("P3:"));
    m_btnReset.SetWindowText(_T("초기화"));
    m_btnRandom.SetWindowText(_T("랜덤 이동"));

    m_editDotRadius.SetWindowText(_T("10"));
    m_editLineWidth.SetWindowText(_T("2"));

    // 첫 번째 에디트 박스 왼쪽 끝을 기준으로 그림 영역 결정
    CRect editRect;
    m_editDotRadius.GetWindowRect(&editRect);
    ScreenToClient(&editRect);

    CRect clientRect;
    GetClientRect(&clientRect);

    m_drawRect = CRect(0, 0, editRect.left - 8, clientRect.bottom);

    return TRUE;
}

// 창 닫기(ESC) / 소멸 시 실행 중인 스레드를 안전하게 종료
void CCircleDrawerDlg::OnCancel()
{
    m_bThreadStop = true;
    if (m_hThread)
    {
        WaitForSingleObject(m_hThread, 2000);
        CloseHandle(m_hThread);
        m_hThread = nullptr;
    }
    CDialogEx::OnCancel();
}

void CCircleDrawerDlg::OnDestroy()
{
    m_bThreadStop = true;
    if (m_hThread)
    {
        WaitForSingleObject(m_hThread, 2000);
        CloseHandle(m_hThread);
        m_hThread = nullptr;
    }
    CDialogEx::OnDestroy();
}


// 원 그리기 LineTo 직접 구현
// 채워진 원: y 한 줄씩 내려가며 가로 선(MoveTo→LineTo)으로 채움
void CCircleDrawerDlg::DrawFilledCircle(CDC* pDC, int cx, int cy, int r)
{
    CPen pen(PS_SOLID, 1, RGB(0, 0, 0));
    CPen* pOld = pDC->SelectObject(&pen);

    for (int y = cy - r; y <= cy + r; y++)
    {
        double dx = std::sqrt(static_cast<double>(r * r - (y - cy) * (y - cy)));
        int x0 = cx - static_cast<int>(dx);
        int x1 = cx + static_cast<int>(dx);
        pDC->MoveTo(x0, y);
        pDC->LineTo(x1 + 1, y);  // LineTo는 끝점 미포함이므로 +1
    }

    pDC->SelectObject(pOld);
}

// 빈 원(외접원): 360도를 720등분해 꼭짓점을 LineTo로 연결
void CCircleDrawerDlg::DrawOpenCircle(CDC* pDC, double cx, double cy, double r, int lineWidth)
{
    CPen pen(PS_SOLID, lineWidth, RGB(0, 0, 0));
    CPen* pOld = pDC->SelectObject(&pen);

    const int N = 720;
    bool first = true;
    for (int i = 0; i <= N; i++)
    {
        double angle = 2.0 * CD_PI * i / N;
        int x = static_cast<int>(std::round(cx + r * std::cos(angle)));
        int y = static_cast<int>(std::round(cy + r * std::sin(angle)));
        if (first) { pDC->MoveTo(x, y); first = false; }
        else        { pDC->LineTo(x, y); }
    }

    pDC->SelectObject(pOld);
}

// 더블 버퍼링: 오프스크린 비트맵에 다 그린 뒤 BitBlt로 한 번에 복사 → 깜빡임 방지
void CCircleDrawerDlg::DrawScene(CDC* pDC)
{
    int w = m_drawRect.Width();
    int h = m_drawRect.Height();
    if (w <= 0 || h <= 0) return;

    // 오프스크린 버퍼 생성
    CDC memDC;
    memDC.CreateCompatibleDC(pDC);
    CBitmap bmp;
    bmp.CreateCompatibleBitmap(pDC, w, h);
    CBitmap* pOldBmp = memDC.SelectObject(&bmp);

    memDC.FillSolidRect(0, 0, w, h, RGB(255, 255, 255));  // 흰 배경

    // 테두리
    CPen borderPen(PS_SOLID, 1, RGB(160, 160, 160));
    CPen* pOldPen = memDC.SelectObject(&borderPen);
    memDC.MoveTo(0, 0);     memDC.LineTo(w - 1, 0);
    memDC.LineTo(w - 1, h - 1);
    memDC.LineTo(0, h - 1); memDC.LineTo(0, 0);
    memDC.SelectObject(pOldPen);

    // m_pts 좌표는 다이얼로그 기준 → memDC 기준으로 변환하기 위해 오프셋 적용
    const int ox = m_drawRect.left;
    const int oy = m_drawRect.top;

    if (m_bCircleReady)
        DrawOpenCircle(&memDC, m_cx - ox, m_cy - oy, m_cr, m_nLineWidth);  // 외접원

    for (int i = 0; i < m_nClickCount; i++)
        DrawFilledCircle(&memDC, m_pts[i].x - ox, m_pts[i].y - oy, m_nDotRadius);  // 클릭 점

    pDC->BitBlt(m_drawRect.left, m_drawRect.top, w, h, &memDC, 0, 0, SRCCOPY);  // 화면에 복사
    memDC.SelectObject(pOldBmp);
}


// 외접원 수학 계산 (수직이등분선 교점)
// P1·P2·P3를 지나는 원의 중심(m_cx, m_cy)과 반지름(m_cr) 계산
// 세 점이 일직선이면 false 반환
bool CCircleDrawerDlg::ComputeCircumscribed()
{
    if (m_nClickCount < 3) return false;

    const double x1 = m_pts[0].x, y1 = m_pts[0].y;
    const double x2 = m_pts[1].x, y2 = m_pts[1].y;
    const double x3 = m_pts[2].x, y3 = m_pts[2].y;

    const double ax = x2 - x1, ay = y2 - y1;
    const double bx = x3 - x1, by = y3 - y1;
    const double D  = 2.0 * (ax * by - ay * bx);

    if (std::abs(D) < 1e-6) return false;  // 세 점 일직선 → 원 불가

    const double ux = (by * (ax*ax + ay*ay) - ay * (bx*bx + by*by)) / D;
    const double uy = (ax * (bx*bx + by*by) - bx * (ax*ax + ay*ay)) / D;

    m_cx = x1 + ux;
    m_cy = y1 + uy;
    m_cr = std::sqrt(ux*ux + uy*uy);

    return true;
}

// 클릭한 위치가 어떤 점 원 안인지 확인 → 해당 인덱스(0/1/2) 또는 -1 반환
int CCircleDrawerDlg::HitTestPoint(CPoint pt) const
{
    const int hitRadius = m_nDotRadius + 6;  // 클릭 여유 반경
    for (int i = 0; i < m_nClickCount; i++)
    {
        const double dx = pt.x - m_pts[i].x;
        const double dy = pt.y - m_pts[i].y;
        if (std::sqrt(dx*dx + dy*dy) <= hitRadius)
            return i;
    }
    return -1;
}

// UI 보조 함수
// P1/P2/P3 좌표 레이블 갱신
void CCircleDrawerDlg::UpdateCoordLabels()
{
    auto setLabel = [](CStatic& stc, bool valid, int x, int y)
    {
        if (valid)
        {
            CString s;
            s.Format(_T("(%d, %d)"), x, y);
            stc.SetWindowText(s);
        }
        else
        {
            stc.SetWindowText(_T(""));
        }
    };

    setLabel(m_stcP1, m_nClickCount >= 1, m_pts[0].x, m_pts[0].y);
    setLabel(m_stcP2, m_nClickCount >= 2, m_pts[1].x, m_pts[1].y);
    setLabel(m_stcP3, m_nClickCount >= 3, m_pts[2].x, m_pts[2].y);
}

// 에디트 박스에서 점 반지름·선 두께 읽기 (그릴 때마다 호출)
void CCircleDrawerDlg::UpdateSettings()
{
    CString s;

    m_editDotRadius.GetWindowText(s);
    int r = _ttoi(s);
    if (r > 0 && r <= 100) m_nDotRadius = r;

    m_editLineWidth.GetWindowText(s);
    int w = _ttoi(s);
    if (w > 0 && w <= 20) m_nLineWidth = w;
}

// 그림 영역만 다시 그리기 (FALSE = 배경 지우기 생략 → 깜빡임 방지)
void CCircleDrawerDlg::Redraw()
{
    InvalidateRect(&m_drawRect, FALSE);
    UpdateWindow();
}

// 모든 상태를 초기값으로 리셋
void CCircleDrawerDlg::ResetAll()
{
    m_nClickCount  = 0;
    m_bCircleReady = false;
    m_nDragIdx     = -1;
    m_cx = m_cy = m_cr = 0.0;
    UpdateCoordLabels();
    Redraw();
}

// -----------------------------------------------------------------------
// WM_PAINT / WM_ERASEBKGND
// -----------------------------------------------------------------------

// 오른쪽 컨트롤 패널은 시스템 색으로, 그림 영역은 흰색으로 지움
// TRUE 반환 → MFC 기본 배경 지우기 생략 (깜빡임 방지)
BOOL CCircleDrawerDlg::OnEraseBkgnd(CDC* pDC)
{
    CRect clientRect;
    GetClientRect(&clientRect);

    if (!m_drawRect.IsRectEmpty())
    {
        CRect rightRect(m_drawRect.right, 0, clientRect.right, clientRect.bottom);
        if (!rightRect.IsRectEmpty())
            pDC->FillSolidRect(&rightRect, GetSysColor(COLOR_BTNFACE));

        pDC->FillSolidRect(&m_drawRect, RGB(255, 255, 255));
    }
    else
    {
        pDC->FillSolidRect(&clientRect, GetSysColor(COLOR_BTNFACE));
    }

    return TRUE;
}

// 매 프레임: 설정값 읽기 → 씬 전체 렌더
void CCircleDrawerDlg::OnPaint()
{
    CPaintDC dc(this);
    UpdateSettings();
    DrawScene(&dc);
}

// -----------------------------------------------------------------------
// 마우스 이벤트
// -----------------------------------------------------------------------

void CCircleDrawerDlg::OnLButtonDown(UINT nFlags, CPoint point)
{
    if (!m_drawRect.PtInRect(point))  // 그림 영역 밖 클릭 무시
    {
        CDialogEx::OnLButtonDown(nFlags, point);
        return;
    }

    if (m_bCircleReady)
    {
        // 정원이 이미 그려진 상태 → 점 클릭 시 드래그 시작
        int idx = HitTestPoint(point);
        if (idx >= 0)
        {
            m_nDragIdx = idx;
            SetCapture();  // 마우스가 창 밖으로 나가도 이벤트 계속 수신
        }
    }
    else if (m_nClickCount < 3)
    {
        // 1~3번째 클릭: 점 추가 (4번째부터는 이 조건 불통과 → 무시)
        UpdateSettings();
        m_pts[m_nClickCount] = point;
        m_nClickCount++;

        if (m_nClickCount == 3)
            m_bCircleReady = ComputeCircumscribed();  // 3점 완성 → 외접원 계산

        UpdateCoordLabels();
        Redraw();
    }
}

void CCircleDrawerDlg::OnMouseMove(UINT nFlags, CPoint point)
{
    if (m_nDragIdx < 0)  // 드래그 중이 아니면 무시
    {
        CDialogEx::OnMouseMove(nFlags, point);
        return;
    }

    // 점이 그림 영역 밖으로 나가지 않도록 클램프
    const int margin = m_nDotRadius;
    const int nx = max(m_drawRect.left + margin, min(m_drawRect.right  - margin, point.x));
    const int ny = max(m_drawRect.top  + margin, min(m_drawRect.bottom - margin, point.y));

    m_pts[m_nDragIdx] = CPoint(nx, ny);
    m_bCircleReady = ComputeCircumscribed();  // 점 위치 바뀔 때마다 즉시 재계산

    UpdateCoordLabels();
    Redraw();
}

void CCircleDrawerDlg::OnLButtonUp(UINT nFlags, CPoint point)
{
    if (m_nDragIdx >= 0)
    {
        m_nDragIdx = -1;
        ReleaseCapture();  // 마우스 캡처 해제
    }
    CDialogEx::OnLButtonUp(nFlags, point);
}

// -----------------------------------------------------------------------
// 버튼 핸들러
// -----------------------------------------------------------------------

// [초기화]: 실행 중인 스레드 종료 후 전체 리셋
void CCircleDrawerDlg::OnBnClickedReset()
{
    m_bThreadStop = true;
    if (m_hThread)
    {
        WaitForSingleObject(m_hThread, 2000);
        CloseHandle(m_hThread);
        m_hThread = nullptr;
    }
    m_bThreadStop = false;
    ResetAll();
}

// [랜덤 이동]: 정원이 그려진 상태에서만 백그라운드 스레드 시작
void CCircleDrawerDlg::OnBnClickedRandom()
{
    if (!m_bCircleReady)
    {
        MessageBox(_T("먼저 세 점을 클릭하여 정원을 그려주세요."),
                   _T("Circle Drawer"), MB_ICONINFORMATION);
        return;
    }
    if (m_hThread)
        return;  // 이미 실행 중이면 중복 시작 방지

    m_bThreadStop = false;
    m_hThread = CreateThread(nullptr, 0, RandomThreadProc, this, 0, nullptr);
}

// -----------------------------------------------------------------------
// 랜덤 이동 백그라운드 스레드
// -----------------------------------------------------------------------

// CreateThread가 요구하는 정적 진입점 → 멤버 함수 DoRandomMove 호출
DWORD WINAPI CCircleDrawerDlg::RandomThreadProc(LPVOID pParam)
{
    CCircleDrawerDlg* pDlg = static_cast<CCircleDrawerDlg*>(pParam);
    pDlg->DoRandomMove();
    return 0;
}

// 0.5초 간격으로 10회 반복: m_pts 랜덤 변경 후 PostMessage로 UI 갱신 요청
// UI 스레드와 m_pts 공유 → CRITICAL_SECTION으로 보호
void CCircleDrawerDlg::DoRandomMove()
{
    for (int i = 0; i < 10; i++)
    {
        Sleep(500);  // 0.5초 대기 → 초당 2회

        if (m_bThreadStop) break;

        EnterCriticalSection(&m_cs);

        const int margin = m_nDotRadius + 10;
        const int w = m_drawRect.Width()  - 2 * margin;
        const int h = m_drawRect.Height() - 2 * margin;

        if (w > 0 && h > 0)
        {
            for (int j = 0; j < 3; j++)
            {
                m_pts[j].x = m_drawRect.left + margin + (std::rand() % w);
                m_pts[j].y = m_drawRect.top  + margin + (std::rand() % h);
            }
        }

        LeaveCriticalSection(&m_cs);

        // UI 스레드에만 SetWindowText / InvalidateRect 가능 → PostMessage로 신호만 전달
        PostMessage(WM_RANDOM_UPDATE);
    }

    PostMessage(WM_RANDOM_DONE);
}

// -----------------------------------------------------------------------
// 커스텀 메시지 핸들러 (PostMessage로 전달받아 UI 스레드에서 실행)
// -----------------------------------------------------------------------

// WM_RANDOM_UPDATE: 새 좌표로 외접원 재계산 + 화면 갱신
LRESULT CCircleDrawerDlg::OnRandomUpdate(WPARAM, LPARAM)
{
    EnterCriticalSection(&m_cs);
    m_bCircleReady = ComputeCircumscribed();
    LeaveCriticalSection(&m_cs);

    UpdateCoordLabels();
    Redraw();
    return 0;
}

// WM_RANDOM_DONE: 스레드 핸들 정리
LRESULT CCircleDrawerDlg::OnRandomDone(WPARAM, LPARAM)
{
    if (m_hThread)
    {
        WaitForSingleObject(m_hThread, 500);
        CloseHandle(m_hThread);
        m_hThread = nullptr;
    }
    return 0;
}
