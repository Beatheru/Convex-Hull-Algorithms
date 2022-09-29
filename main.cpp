#include <windows.h>
#include <Windowsx.h>
#include <d2d1.h>

#include "ConvexHull.h"
#include "DataTypes.h"
#include "Converter.h"

#include <list>
#include <memory>
using namespace std;

#pragma comment(lib, "d2d1")

#include "basewin.h"

#define MINKOWSKI_DIFFERENCE 0
#define MINKOWSKI_SUM 1
#define QUICKHULL 2
#define POINT_CONVEX_HULL 3
#define GJK 4

template <class T> void SafeRelease(T **ppT)
{
    if (*ppT)
    {
        (*ppT)->Release();
        *ppT = NULL;
    }
}

class DPIScale
{
    static float scaleX;
    static float scaleY;

public:
    static void Initialize(ID2D1Factory *pFactory)
    {
        FLOAT dpiX, dpiY;
        pFactory->GetDesktopDpi(&dpiX, &dpiY);
        scaleX = dpiX/96.0f;
        scaleY = dpiY/96.0f;
    }

    template <typename T>
    static float PixelsToDipsX(T x)
    {
        return static_cast<float>(x) / scaleX;
    }

    template <typename T>
    static float PixelsToDipsY(T y)
    {
        return static_cast<float>(y) / scaleY;
    }
};

float DPIScale::scaleX = 1.0f;
float DPIScale::scaleY = 1.0f;

struct MyEllipse
{
    D2D1_ELLIPSE    ellipse;
    D2D1_COLOR_F    color;

    void Draw(ID2D1RenderTarget *pRT, ID2D1SolidColorBrush *pBrush)
    {
        pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
        pRT->DrawEllipse(ellipse, pBrush, 5.0f);
        pBrush->SetColor(color);
        pRT->FillEllipse(ellipse, pBrush);
    }

    BOOL HitTest(float x, float y)
    {
        const float a = ellipse.radiusX;
        const float b = ellipse.radiusY;
        const float x1 = x - ellipse.point.x;
        const float y1 = y - ellipse.point.y;
        const float d = ((x1 * x1) / (a * a)) + ((y1 * y1) / (b * b));
        return d <= 1.0f;
    }
};

D2D1::ColorF::Enum colors[] = { D2D1::ColorF::Yellow, D2D1::ColorF::Salmon, D2D1::ColorF::LimeGreen };


class MainWindow : public BaseWindow<MainWindow>
{
    enum Mode
    {
        SelectMode,
        DragMode,
        DragScreen,
        DragHull
    };

    HCURSOR                 hCursor;

    ID2D1Factory            *pFactory;
    ID2D1HwndRenderTarget   *pRenderTarget;
    ID2D1SolidColorBrush    *pBrush;
    D2D1_POINT_2F           ptMouse;

    Mode                    mode;
    size_t                  nextColor;
    int                     paintMode = -1;
    bool                    inHull = false;
    Converter               *conv;
    std::vector<ConvexHull*> *hulls = new std::vector<ConvexHull*>;
    int                     hullSelected;

    double                  xOffset = 0;
    double                  yOffset = 0;

    double                   horizontalOriginalY;
    double                   verticalOriginalX;
    double                   horizontalY;
    double                   verticalX;

    std::vector<struct point>* temp = new std::vector<struct point>;

    list<shared_ptr<MyEllipse>>             ellipses;
    list<shared_ptr<MyEllipse>>::iterator   selection;
     
    shared_ptr<MyEllipse> Selection() 
    { 
        if (selection == ellipses.end()) 
        { 
            return nullptr;
        }
        else
        {
            return (*selection);
        }
    }

    void    ClearSelection() { selection = ellipses.end(); }
    BOOL    HitTest(float x, float y);
    void    SetMode(Mode m);
    void    MoveSelection(float x, float y);
    HRESULT CreateGraphicsResources();
    void    DiscardGraphicsResources();
    void    DrawConvexHull(std::vector<struct point> *hullPoints, D2D1::ColorF color);
    void    OnPaintDefault();
    void    OnPaintSelect();
    void    PaintMinkowskiGJK();
    void    UpdateMinkowskiGJK();
    void    PaintQuickhull();
    void    UpdateQuickhull();
    void    PaintPointConvexHull();
    void    UpdatePointConvexHull();
    void    Resize();
    void    OnLButtonDown(int pixelX, int pixelY, DWORD flags);
    void    OnLButtonUp();
    void    OnMouseMove(int pixelX, int pixelY, DWORD flags);

public:

    MainWindow() : pFactory(NULL), pRenderTarget(NULL), pBrush(NULL), 
        ptMouse(D2D1::Point2F()), nextColor(0), selection(ellipses.end())
    {
    }

    PCWSTR  ClassName() const { return L"Window Class"; }
    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

HRESULT MainWindow::CreateGraphicsResources()
{
    HRESULT hr = S_OK;
    if (pRenderTarget == NULL)
    {
        RECT rc;
        GetClientRect(m_hwnd, &rc);

        D2D1_SIZE_U size = D2D1::SizeU(rc.right, rc.bottom);

        hr = pFactory->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(),
            D2D1::HwndRenderTargetProperties(m_hwnd, size),
            &pRenderTarget);

        if (SUCCEEDED(hr))
        {
            const D2D1_COLOR_F color = D2D1::ColorF(1.0f, 1.0f, 0);
            hr = pRenderTarget->CreateSolidColorBrush(color, &pBrush);
        }

        conv = new Converter(rc.right, rc.bottom);
    }
    return hr;
}

void MainWindow::DiscardGraphicsResources()
{
    SafeRelease(&pRenderTarget);
    SafeRelease(&pBrush);
}

void MainWindow::DrawConvexHull(std::vector<struct point> *hullPoints, D2D1::ColorF color) {
    pBrush->SetColor(color);

    for (int i = 0; i < hullPoints->size() - 1; i++) {
        pRenderTarget->DrawLine(D2D1::Point2F((*hullPoints)[i].x, (*hullPoints)[i].y),
            D2D1::Point2F((*hullPoints)[i + 1].x, (*hullPoints)[i + 1].y),
            pBrush, 1);
    }

    pRenderTarget->DrawLine(D2D1::Point2F((*hullPoints)[hullPoints->size() - 1].x, (*hullPoints)[hullPoints->size() - 1].y),
        D2D1::Point2F((*hullPoints)[0].x, (*hullPoints)[0].y),
        pBrush, 1);
}

void MainWindow::OnPaintDefault()
{
    HRESULT hr = CreateGraphicsResources();
    if (SUCCEEDED(hr))
    {
        PAINTSTRUCT ps;
        BeginPaint(m_hwnd, &ps);
     
        pRenderTarget->BeginDraw();
        pRenderTarget->Clear( D2D1::ColorF(D2D1::ColorF::Black) );
        hr = pRenderTarget->EndDraw();

        if (FAILED(hr) || hr == D2DERR_RECREATE_TARGET)
        {
            DiscardGraphicsResources();
        }
        EndPaint(m_hwnd, &ps);
    }
}

void MainWindow::OnPaintSelect() 
{
    switch (paintMode) {
    case MINKOWSKI_DIFFERENCE:
        UpdateMinkowskiGJK();
        break;

    case MINKOWSKI_SUM:
        UpdateMinkowskiGJK();
        break;

    case QUICKHULL:
        UpdateQuickhull();
        break;

    case POINT_CONVEX_HULL:
        UpdatePointConvexHull();
        break;

    case GJK:
        UpdateMinkowskiGJK();
        break;
    default:
        OnPaintDefault();
    }
}

void MainWindow::PaintMinkowskiGJK()
{
    HRESULT hr = CreateGraphicsResources();
    if (SUCCEEDED(hr))
    {
        PAINTSTRUCT ps;
        ellipses.clear();
        hulls->clear();
        BeginPaint(m_hwnd, &ps);

        pRenderTarget->BeginDraw();
        pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::Black));

        RECT rc;
        GetClientRect(m_hwnd, &rc);

        horizontalY = rc.bottom / 2.f;
        verticalX = rc.right / 2.f;

        pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::Yellow));
        pRenderTarget->DrawLine(D2D1::Point2F(0, horizontalY), D2D1::Point2F(rc.right, horizontalY), pBrush, 1);
        pRenderTarget->DrawLine(D2D1::Point2F(verticalX, 0), D2D1::Point2F(verticalX, rc.bottom), pBrush, 1);

        int rightLimit = rc.right / 6.f - 50;
        int bottomLimit = rc.bottom / 6.f - 50;

        std::vector<struct point>* points = new std::vector<struct point>();

        for (int i = 0; i < 6; i++) {
            int x = rand() % rightLimit + (rc.right / 2.f + 20);
            int y = rand() % bottomLimit + (2 * rc.bottom / 6.f + 20);

            struct point p = { x, y };
            points->push_back(p);

            D2D1_ELLIPSE point = D2D1::Ellipse(D2D1::Point2F(x, y), 10, 10);
            shared_ptr<MyEllipse> newEllipse = shared_ptr<MyEllipse>(new MyEllipse());
            newEllipse->ellipse = point;
            newEllipse->color = D2D1::ColorF(D2D1::ColorF::Green);
            ellipses.insert(ellipses.end(), newEllipse);
        }

        ConvexHull* hull1 = new ConvexHull(*points);
        hulls->push_back(hull1);
        DrawConvexHull(hull1->getHull(), D2D1::ColorF(D2D1::ColorF::White));

        /////////////////////////////////////////////////////////////////////////////////////////

        points->clear();

        for (int i = 0; i < 6; i++) {
            int x = rand() % rightLimit + (4 * (rc.right / 6.f));
            int y = rand() % bottomLimit + (rc.bottom / 6.f);

            struct point p = { x, y };
            points->push_back(p);

            D2D1_ELLIPSE point = D2D1::Ellipse(D2D1::Point2F(x, y), 10, 10);
            shared_ptr<MyEllipse> newEllipse = shared_ptr<MyEllipse>(new MyEllipse());
            newEllipse->ellipse = point;
            newEllipse->color = D2D1::ColorF(D2D1::ColorF::Green);
            ellipses.insert(ellipses.end(), newEllipse);
        }

        ConvexHull* hull2 = new ConvexHull(*points);
        hulls->push_back(hull2);
        DrawConvexHull(hull2->getHull(), D2D1::ColorF(D2D1::ColorF::White));

        delete points;

        //////////////////////////////////////////////////////////////

        ConvexHull* newHull = paintMode == MINKOWSKI_SUM ? hull1->minkowskiSum(hull1, hull2, conv) : hull1->minkowskiDifference(hull1, hull2, conv);
        DrawConvexHull(newHull->getHull(), D2D1::ColorF(D2D1::ColorF::White));

        for (auto i = ellipses.begin(); i != ellipses.end(); ++i)
            (*i)->Draw(pRenderTarget, pBrush);

        hr = pRenderTarget->EndDraw();
        if (FAILED(hr) || hr == D2DERR_RECREATE_TARGET)
        {
            DiscardGraphicsResources();
        }
        EndPaint(m_hwnd, &ps);
    }
}

void MainWindow::UpdateMinkowskiGJK() {
    HRESULT hr = CreateGraphicsResources();
    if (SUCCEEDED(hr))
    {
        PAINTSTRUCT ps;
        BeginPaint(m_hwnd, &ps);

        pRenderTarget->BeginDraw();
        pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::Black));

        RECT rc;
        GetClientRect(m_hwnd, &rc);
        struct point origin = { rc.right / 2.f, rc.bottom / 2.f };

        
        pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::Yellow));
        pRenderTarget->DrawLine(D2D1::Point2F(0, horizontalY), D2D1::Point2F(rc.right, horizontalY), pBrush, 1);
        pRenderTarget->DrawLine(D2D1::Point2F(verticalX, 0), D2D1::Point2F(verticalX, rc.bottom), pBrush, 1);

        std::vector<struct point> *points = new std::vector<struct point>();

        for (int i = 0; i < ellipses.size() / 2; i++) {
            auto iterator = ellipses.begin();
            std::advance(iterator, i);
            
            struct point p = { (*iterator)->ellipse.point.x, (*iterator)->ellipse.point.y };
            points->push_back(p);
        }

        ConvexHull* hull1 = new ConvexHull(*points);
        (*hulls)[0] = hull1;
        DrawConvexHull(hull1->getHull(), D2D1::ColorF(D2D1::ColorF::White));

        ///////////////////////////////////////////////

        points->clear();

        for (int i = ellipses.size() / 2; i < ellipses.size(); i++) {
            auto iterator = ellipses.begin();
            std::advance(iterator, i);

            struct point p = { (*iterator)->ellipse.point.x, (*iterator)->ellipse.point.y };
            points->push_back(p);
        }

        ConvexHull *hull2 = new ConvexHull(*points);
        (*hulls)[1] = hull2;
        DrawConvexHull(hull2->getHull(), D2D1::ColorF(D2D1::ColorF::White));

        ConvexHull* newHull = paintMode == MINKOWSKI_SUM ? hull1->minkowskiSum(hull1, hull2, conv) : hull1->minkowskiDifference(hull1, hull2, conv);

        if (paintMode == GJK)
            if (newHull->containsPoint(origin))
                DrawConvexHull(newHull->getHull(), D2D1::ColorF(D2D1::ColorF::Red));
            else
                DrawConvexHull(newHull->getHull(), D2D1::ColorF(D2D1::ColorF::White));
        else
            DrawConvexHull(newHull->getHull(), D2D1::ColorF(D2D1::ColorF::White));

        for (auto i = ellipses.begin(); i != ellipses.end(); ++i)
            (*i)->Draw(pRenderTarget, pBrush);

        delete points;

        hr = pRenderTarget->EndDraw();
        if (FAILED(hr) || hr == D2DERR_RECREATE_TARGET)
        {
            DiscardGraphicsResources();
        }
        EndPaint(m_hwnd, &ps);
    }
}

void MainWindow::PaintQuickhull()
{
    HRESULT hr = CreateGraphicsResources();
    if (SUCCEEDED(hr))
    {
        PAINTSTRUCT ps;
        ellipses.clear();
        BeginPaint(m_hwnd, &ps);

        pRenderTarget->BeginDraw();
        pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::Black));

        RECT rc;
        GetClientRect(m_hwnd, &rc);
        int rightLimit = rc.right - 200 - 500;
        int bottomLimit = rc.bottom - 200 - 150;

        std::vector<struct point> *points = new std::vector<struct point>();

        for (int i = 0; i < 15; i++) {
            int x = rand() % rightLimit + 500;
            int y = rand() % bottomLimit + 150;

            struct point p = {x, y};
            points->push_back(p);

            D2D1_ELLIPSE point = D2D1::Ellipse(D2D1::Point2F(x, y), 10, 10);
            shared_ptr<MyEllipse> newEllipse = shared_ptr<MyEllipse>(new MyEllipse());
            newEllipse->ellipse = point;
            newEllipse->color = D2D1::ColorF(D2D1::ColorF::Green);
            ellipses.insert(ellipses.end(), newEllipse);
        }

        ConvexHull *hull = new ConvexHull(*points);
        DrawConvexHull(hull->getHull(), D2D1::ColorF(D2D1::ColorF::White));

        delete points;
        delete hull;

        for (auto i = ellipses.begin(); i != ellipses.end(); ++i)
            (*i)->Draw(pRenderTarget, pBrush);

        hr = pRenderTarget->EndDraw();
        if (FAILED(hr) || hr == D2DERR_RECREATE_TARGET)
        {
            DiscardGraphicsResources();
        }
        EndPaint(m_hwnd, &ps);
    }
}

void MainWindow::UpdateQuickhull() {
    HRESULT hr = CreateGraphicsResources();
    if (SUCCEEDED(hr))
    {
        PAINTSTRUCT ps;
        BeginPaint(m_hwnd, &ps);

        pRenderTarget->BeginDraw();
        pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::Black));

        std::vector<struct point>* points = new std::vector<struct point>();

        for (auto i = ellipses.begin(); i != ellipses.end(); ++i)
        {
            struct point p = { (*i)->ellipse.point.x, (*i)->ellipse.point.y };
            points->push_back(p);
        }

        ConvexHull* hull = new ConvexHull(*points);
        DrawConvexHull(hull->getHull(), D2D1::ColorF(D2D1::ColorF::White));

        delete points;
        delete hull;

        for (auto i = ellipses.begin(); i != ellipses.end(); ++i)
            (*i)->Draw(pRenderTarget, pBrush);

        hr = pRenderTarget->EndDraw();
        if (FAILED(hr) || hr == D2DERR_RECREATE_TARGET)
        {
            DiscardGraphicsResources();
        }
        EndPaint(m_hwnd, &ps);
    }
}

void MainWindow::PaintPointConvexHull()
{
    HRESULT hr = CreateGraphicsResources();
    if (SUCCEEDED(hr))
    {
        PAINTSTRUCT ps;
        ellipses.clear();
        BeginPaint(m_hwnd, &ps);

        pRenderTarget->BeginDraw();
        pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::Black));

        RECT rc;
        GetClientRect(m_hwnd, &rc);
        int rightLimit = rc.right - 200 - 500;
        int bottomLimit = rc.bottom - 200 - 150;

        std::vector<struct point>* points = new std::vector<struct point>();

        for (int i = 0; i < 15; i++) {
            int x = rand() % rightLimit + 500;
            int y = rand() % bottomLimit + 150;

            struct point p = { x, y };
            points->push_back(p);

            D2D1_ELLIPSE point = D2D1::Ellipse(D2D1::Point2F(x, y), 1, 1);
            shared_ptr<MyEllipse> newEllipse = shared_ptr<MyEllipse>(new MyEllipse());
            newEllipse->ellipse = point;
            newEllipse->color = D2D1::ColorF(D2D1::ColorF::Green);
            ellipses.insert(ellipses.end(), newEllipse);
        }

        ConvexHull* hull = new ConvexHull(*points);
        DrawConvexHull(hull->getHull(), D2D1::ColorF(D2D1::ColorF::White));

        delete points;
        delete hull;

        int x = rand() % rightLimit + 500;
        int y = rand() % bottomLimit + 150;

        struct point p = { x, y };

        D2D1_ELLIPSE point = D2D1::Ellipse(D2D1::Point2F(x, y), 10, 10);
        shared_ptr<MyEllipse> newEllipse = shared_ptr<MyEllipse>(new MyEllipse());
        newEllipse->ellipse = point;
        newEllipse->color = D2D1::ColorF(D2D1::ColorF::Green);
        ellipses.insert(ellipses.end(), newEllipse);
        (*newEllipse).Draw(pRenderTarget, pBrush);

        hr = pRenderTarget->EndDraw();
        if (FAILED(hr) || hr == D2DERR_RECREATE_TARGET)
        {
            DiscardGraphicsResources();
        }
        EndPaint(m_hwnd, &ps);
    }
}

void MainWindow::UpdatePointConvexHull() {
    HRESULT hr = CreateGraphicsResources();
    if (SUCCEEDED(hr))
    {
        PAINTSTRUCT ps;
        BeginPaint(m_hwnd, &ps);

        pRenderTarget->BeginDraw();
        pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::Black));

        std::vector<struct point>* points = new std::vector<struct point>();

        for (auto i = ellipses.begin(); i != ellipses.end(); ++i)
        {
            struct point p = { (*i)->ellipse.point.x, (*i)->ellipse.point.y };
            points->push_back(p);
        }

        points->pop_back();

        ConvexHull* hull = new ConvexHull(*points);
        DrawConvexHull(hull->getHull(), D2D1::ColorF(D2D1::ColorF::White));

        if (hull->containsPoint({ ellipses.back()->ellipse.point.x, ellipses.back()->ellipse.point.y })) {
            inHull = true;
            if (mode == DragMode) {
                pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::Yellow));
                pRenderTarget->DrawEllipse(ellipses.back()->ellipse, pBrush, 5.0f);
                pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::Red));
                pRenderTarget->FillEllipse(ellipses.back()->ellipse, pBrush);
            }
            else {
                pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
                pRenderTarget->DrawEllipse(ellipses.back()->ellipse, pBrush, 5.0f);
                pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::Red));
                pRenderTarget->FillEllipse(ellipses.back()->ellipse, pBrush);
            }
        }
        else {
            inHull = false;
            if (mode == DragMode) {
                pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::Yellow));
                pRenderTarget->DrawEllipse(ellipses.back()->ellipse, pBrush, 5.0f);
                pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::Green));
                pRenderTarget->FillEllipse(ellipses.back()->ellipse, pBrush);
            }
            else {
                ellipses.back()->Draw(pRenderTarget, pBrush);
            }
        }

        delete points;
        delete hull;

        hr = pRenderTarget->EndDraw();
        if (FAILED(hr) || hr == D2DERR_RECREATE_TARGET)
        {
            DiscardGraphicsResources();
        }
        EndPaint(m_hwnd, &ps);
    }
}

void MainWindow::Resize()
{
    if (pRenderTarget != NULL)
    {
        RECT rc;
        GetClientRect(m_hwnd, &rc);

        D2D1_SIZE_U size = D2D1::SizeU(rc.right, rc.bottom);

        pRenderTarget->Resize(size);

        InvalidateRect(m_hwnd, NULL, FALSE);
    }
}

void MainWindow::OnLButtonDown(int pixelX, int pixelY, DWORD flags)
{
    const float dipX = DPIScale::PixelsToDipsX(pixelX);
    const float dipY = DPIScale::PixelsToDipsY(pixelY);
    
    ClearSelection();

    if (HitTest(dipX, dipY))
    {
        SetCapture(m_hwnd);

        ptMouse = Selection()->ellipse.point;
        ptMouse.x -= dipX;
        ptMouse.y -= dipY;

        SetMode(DragMode);
    }
    else if ((*hulls)[0] != NULL && (*hulls)[0]->containsPoint({ dipX, dipY })) {
        SetCapture(m_hwnd);
        hullSelected = 0;
        ptMouse.x = dipX;
        ptMouse.y = dipY;
        temp->clear();
        for (int i = 0; i < ellipses.size() / 2; i++) {
            auto iterator = ellipses.begin();
            std::advance(iterator, i);

            struct point p = { (*iterator)->ellipse.point.x, (*iterator)->ellipse.point.y };
            temp->push_back(p);
        }
        SetMode(DragHull);
    }
    else if ((*hulls)[1] != NULL && (*hulls)[1]->containsPoint({ dipX, dipY })) {
        SetCapture(m_hwnd);
        hullSelected = 1;
        ptMouse.x = dipX;
        ptMouse.y = dipY;
        temp->clear();
        for (int i = ellipses.size() / 2; i < ellipses.size(); i++) {
            auto iterator = ellipses.begin();
            std::advance(iterator, i);

            struct point p = { (*iterator)->ellipse.point.x, (*iterator)->ellipse.point.y };
            temp->push_back(p);
        }
        SetMode(DragHull);
    }
    else {
        SetCapture(m_hwnd);
        ptMouse.x = dipX;
        ptMouse.y = dipY;
        horizontalOriginalY = horizontalY;
        verticalOriginalX = verticalX;
        temp->clear();
        for (auto i = ellipses.begin(); i != ellipses.end(); i++) {
            temp->push_back({ (*i)->ellipse.point.x, (*i)->ellipse.point.y });
        }
        SetMode(DragScreen);
    }

    InvalidateRect(m_hwnd, NULL, FALSE);
}

void MainWindow::OnLButtonUp()
{
    if (mode == DragMode)
    {
        SetMode(SelectMode);
        pRenderTarget->BeginDraw();
        pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
        pRenderTarget->DrawEllipse(ellipses.back()->ellipse, pBrush, 5.0f);
        if (inHull && paintMode == POINT_CONVEX_HULL) {
            pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::Red));
        }
        else {
            pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::Green));
        }
        pRenderTarget->FillEllipse(ellipses.back()->ellipse, pBrush);
        pRenderTarget->EndDraw();
    }
    else if (mode == DragScreen) {
        //xOffset = 0;
        //yOffset = 0;
        SetMode(SelectMode);
    }
    else if (mode == DragHull) {
        SetMode(SelectMode);
    }

    ReleaseCapture(); 
}

void MainWindow::OnMouseMove(int pixelX, int pixelY, DWORD flags)
{
    const float dipX = DPIScale::PixelsToDipsX(pixelX);
    const float dipY = DPIScale::PixelsToDipsY(pixelY);

    if ((flags & MK_LBUTTON) && Selection())
    { 
        if (mode == DragMode)
        {
            // Move the ellipse.
            Selection()->ellipse.point.x = dipX + ptMouse.x;
            Selection()->ellipse.point.y = dipY + ptMouse.y;
        }
        InvalidateRect(m_hwnd, NULL, FALSE);
    }
    else if (flags & MK_LBUTTON && mode == DragHull) {
        xOffset = (dipX - ptMouse.x);
        yOffset = (dipY - ptMouse.y);
        int j = 0;

        if (hullSelected == 0) {
            for (int i = 0; i < ellipses.size() / 2; i++) {
                auto iterator = ellipses.begin();
                std::advance(iterator, i);

                (*iterator)->ellipse.point.x = (*temp)[j].x + xOffset;
                (*iterator)->ellipse.point.y = (*temp)[j].y + yOffset;
                j++;
            }
        }
        else {
            for (int i = ellipses.size() / 2; i < ellipses.size(); i++) {
                auto iterator = ellipses.begin();
                std::advance(iterator, i);

                (*iterator)->ellipse.point.x = (*temp)[j].x + xOffset;
                (*iterator)->ellipse.point.y = (*temp)[j].y + yOffset;
                j++;
            }
        }

        SendMessage(m_hwnd, WM_PAINT, NULL, NULL);
    }
    else if (flags & MK_LBUTTON && mode == DragScreen) {
        xOffset = (dipX - ptMouse.x);
        yOffset = (dipY - ptMouse.y);

        horizontalY = horizontalOriginalY + yOffset;
        verticalX = verticalOriginalX + xOffset;

        int j = 0;
        for (auto i = ellipses.begin(); i != ellipses.end(); i++) {
            (*i)->ellipse.point.x = (*temp)[j].x + xOffset;
            (*i)->ellipse.point.y = (*temp)[j].y + yOffset;
            j++;
        }

        SendMessage(m_hwnd, WM_PAINT, NULL, NULL);
    }
}

BOOL MainWindow::HitTest(float x, float y)
{
    for (auto i = ellipses.rbegin(); i != ellipses.rend(); ++i)
    {
        if ((*i)->HitTest(x, y))
        {
            selection = (++i).base();
            return TRUE;
        }
    }
    return FALSE;
}

void MainWindow::MoveSelection(float x, float y)
{
    if ((mode == SelectMode) && Selection())
    {
        Selection()->ellipse.point.x += x;
        Selection()->ellipse.point.y += y;
        InvalidateRect(m_hwnd, NULL, FALSE);
    }
}

void MainWindow::SetMode(Mode m)
{
    mode = m;

    LPWSTR cursor;
    switch (mode)
    {
    case SelectMode:
        cursor = IDC_ARROW;
        break;

    case DragMode:
        cursor = IDC_HAND;
        break;

    case DragScreen:
        cursor = IDC_HAND;
        break;

    case DragHull:
        cursor = IDC_HAND;
        break;
    }

    hCursor = LoadCursor(NULL, cursor);
    SetCursor(hCursor);
}

void createButtons(HWND m_hwnd) {
    HWND hwndButton_MinkowskiDifference = CreateWindow(
        L"BUTTON",
        L"Minkowski Difference",
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  // Styles 
        10,         // x position 
        10,         // y position 
        150,        // Button width
        50,        // Button height
        m_hwnd,     // Parent window
        (HMENU)MINKOWSKI_DIFFERENCE,       // Menu.
        (HINSTANCE)GetWindowLongPtr(m_hwnd, GWLP_HINSTANCE),
        NULL);      // Pointer not needed.

    HWND hwndButton_MinkowskiSum = CreateWindow(
        L"BUTTON",
        L"Minkowski Sum",
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  // Styles 
        10,         // x position 
        65,         // y position 
        150,        // Button width
        50,        // Button height
        m_hwnd,     // Parent window
        (HMENU)MINKOWSKI_SUM,       // Menu.
        (HINSTANCE)GetWindowLongPtr(m_hwnd, GWLP_HINSTANCE),
        NULL);      // Pointer not needed.

    HWND hwndButton_Quickhull = CreateWindow(
        L"BUTTON",
        L"Quickhull",
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  // Styles 
        10,         // x position 
        120,         // y position 
        150,        // Button width
        50,        // Button height
        m_hwnd,     // Parent window
        (HMENU)QUICKHULL,       // Menu.
        (HINSTANCE)GetWindowLongPtr(m_hwnd, GWLP_HINSTANCE),
        NULL);      // Pointer not needed.

    HWND hwndButton_PointConvexHull = CreateWindow(
        L"BUTTON",
        L"Point Convex Hull",
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  // Styles 
        10,         // x position 
        175,         // y position 
        150,        // Button width
        50,        // Button height
        m_hwnd,     // Parent window
        (HMENU)POINT_CONVEX_HULL,       // Menu.
        (HINSTANCE)GetWindowLongPtr(m_hwnd, GWLP_HINSTANCE),
        NULL);      // Pointer not needed.

    HWND hwndButton_GJK = CreateWindow(
        L"BUTTON",
        L"GJK",
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  // Styles 
        10,         // x position 
        230,         // y position 
        150,        // Button width
        50,        // Button height
        m_hwnd,     // Parent window
        (HMENU)GJK,       // Menu.
        (HINSTANCE)GetWindowLongPtr(m_hwnd, GWLP_HINSTANCE),
        NULL);      // Pointer not needed.
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
    MainWindow win;

    if (!win.Create(L"Convex Hull Algorithms", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN))
    {
        return 0;
    }

    createButtons(win.Window());

    ShowWindow(win.Window(), nCmdShow);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}

LRESULT MainWindow::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
        if (FAILED(D2D1CreateFactory(
                D2D1_FACTORY_TYPE_SINGLE_THREADED, &pFactory)))
        {
            return -1;  // Fail CreateWindowEx.
        }
        DPIScale::Initialize(pFactory);
        SetMode(SelectMode);
        return 0;

    case WM_DESTROY:
        DiscardGraphicsResources();
        SafeRelease(&pFactory);
        PostQuitMessage(0);
        return 0;

    case WM_PAINT:
        OnPaintSelect();
        return 0;

    case WM_SIZE:
        Resize();
        return 0;

    case WM_LBUTTONDOWN: 
        OnLButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), (DWORD)wParam);
        return 0;

    case WM_LBUTTONUP: 
        OnLButtonUp();
        return 0;

    case WM_MOUSEMOVE: 
        OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), (DWORD)wParam);
        return 0;

    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT)
        {
            SetCursor(hCursor);
            return TRUE;
        }
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam)) 
        {
        case MINKOWSKI_DIFFERENCE:
            paintMode = MINKOWSKI_DIFFERENCE;
            PaintMinkowskiGJK();
            return 0;

        case MINKOWSKI_SUM:
            paintMode = MINKOWSKI_SUM;
            PaintMinkowskiGJK();
            break;

        case QUICKHULL:
            paintMode = QUICKHULL;
            PaintQuickhull();
            break;

        case POINT_CONVEX_HULL:
            paintMode = POINT_CONVEX_HULL;
            PaintPointConvexHull();
            break;

        case GJK:
            paintMode = GJK;
            PaintMinkowskiGJK();
            break;
        }
        break;

    }
    return DefWindowProc(m_hwnd, uMsg, wParam, lParam);
}
