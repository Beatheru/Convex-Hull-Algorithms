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
        DragScreen
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

    double                  xOffset = 0;
    double                  yOffset = 0;

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
    HRESULT InsertEllipse(float x, float y);

    BOOL    HitTest(float x, float y);
    void    SetMode(Mode m);
    void    MoveSelection(float x, float y);
    HRESULT CreateGraphicsResources();
    void    DiscardGraphicsResources();
    void    OnPaint();
    void    OnPaintSelect();
    void    PaintMinkowskiDifference();
    void    PaintMinkowskiSum();
    void    UpdateMinkowskiSum();
    void    PaintQuickhull();
    void    UpdateQuickhull();
    void    PaintPointConvexHull();
    void    UpdatePointConvexHull();
    void    PaintGJK();
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

void MainWindow::OnPaint()
{
    HRESULT hr = CreateGraphicsResources();
    if (SUCCEEDED(hr))
    {
        PAINTSTRUCT ps;
        BeginPaint(m_hwnd, &ps);
     
        pRenderTarget->BeginDraw();

        pRenderTarget->Clear( D2D1::ColorF(D2D1::ColorF::Black) );

        for (auto i = ellipses.begin(); i != ellipses.end(); ++i)
        {
            (*i)->Draw(pRenderTarget, pBrush);
        }

        if (Selection())
        {
            pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::Red));
            pRenderTarget->DrawEllipse(Selection()->ellipse, pBrush, 2.0f);
        }

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
        //PaintMinkowskiDifference();
        break;

    case MINKOWSKI_SUM:
        UpdateMinkowskiSum();
        break;

    case QUICKHULL:
        UpdateQuickhull();
        break;

    case POINT_CONVEX_HULL:
        UpdatePointConvexHull();
        break;

    case GJK:
        //PaintGJK();
        break;
    default:
        OnPaint();
    }
}

void MainWindow::PaintMinkowskiDifference()
{
    HRESULT hr = CreateGraphicsResources();
    if (SUCCEEDED(hr))
    {
        PAINTSTRUCT ps;
        BeginPaint(m_hwnd, &ps);

        pRenderTarget->BeginDraw();

        pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));

        hr = pRenderTarget->EndDraw();
        if (FAILED(hr) || hr == D2DERR_RECREATE_TARGET)
        {
            DiscardGraphicsResources();
        }
        EndPaint(m_hwnd, &ps);
    }
}

void MainWindow::PaintMinkowskiSum()
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

        pRenderTarget->DrawLine(D2D1::Point2F(0, rc.bottom / 2.f), D2D1::Point2F(rc.right, rc.bottom / 2.f), pBrush, 1);
        pRenderTarget->DrawLine(D2D1::Point2F(rc.right / 2.f, 0), D2D1::Point2F(rc.right / 2.f, rc.bottom), pBrush, 1);

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
        std::vector<struct point>* hullPoints1 = hull1->getHull();

        pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));

        for (int i = 0; i < hullPoints1->size() - 1; i++) {
            pRenderTarget->DrawLine(D2D1::Point2F((*hullPoints1)[i].x, (*hullPoints1)[i].y),
                D2D1::Point2F((*hullPoints1)[i + 1].x, (*hullPoints1)[i + 1].y),
                pBrush, 1);
        }

        pRenderTarget->DrawLine(D2D1::Point2F((*hullPoints1)[hullPoints1->size() - 1].x, (*hullPoints1)[hullPoints1->size() - 1].y),
            D2D1::Point2F((*hullPoints1)[0].x, (*hullPoints1)[0].y),
            pBrush, 1);

        for (auto i = ellipses.begin(); i != ellipses.end(); ++i)
        {
            (*i)->Draw(pRenderTarget, pBrush);
        }

        delete points;

        /////////////////////////////////////////////////////////////////////////////////////////

        rightLimit = rc.right / 6.f - 50;
        bottomLimit = rc.bottom / 6.f - 50;

        points = new std::vector<struct point>();

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
        std::vector<struct point>* hullPoints2 = hull2->getHull();

        pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));

        for (int i = 0; i < hullPoints2->size() - 1; i++) {
            pRenderTarget->DrawLine(D2D1::Point2F((*hullPoints2)[i].x, (*hullPoints2)[i].y),
                D2D1::Point2F((*hullPoints2)[i + 1].x, (*hullPoints2)[i + 1].y),
                pBrush, 1);
        }

        pRenderTarget->DrawLine(D2D1::Point2F((*hullPoints2)[hullPoints2->size() - 1].x, (*hullPoints2)[hullPoints2->size() - 1].y),
            D2D1::Point2F((*hullPoints2)[0].x, (*hullPoints2)[0].y),
            pBrush, 1);

        for (auto i = ellipses.begin(); i != ellipses.end(); ++i)
        {
            (*i)->Draw(pRenderTarget, pBrush);
        }

        delete points;

        //////////////////////////////////////////////////////////////

        ConvexHull* newHull = hull1->minkowskiSum(hull1, hull2, conv);
        std::vector<struct point>* newHullPoints = newHull->getHull();

        pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));

        for (int i = 0; i < newHullPoints->size() - 1; i++) {
            pRenderTarget->DrawLine(D2D1::Point2F((*newHullPoints)[i].x, (*newHullPoints)[i].y),
                D2D1::Point2F((*newHullPoints)[i + 1].x, (*newHullPoints)[i + 1].y),
                pBrush, 1);
        }

        pRenderTarget->DrawLine(D2D1::Point2F((*newHullPoints)[newHullPoints->size() - 1].x, (*newHullPoints)[newHullPoints->size() - 1].y),
            D2D1::Point2F((*newHullPoints)[0].x, (*newHullPoints)[0].y),
            pBrush, 1);

        for (auto i = ellipses.begin(); i != ellipses.end(); ++i)
        {
            (*i)->Draw(pRenderTarget, pBrush);
        }

        hr = pRenderTarget->EndDraw();
        if (FAILED(hr) || hr == D2DERR_RECREATE_TARGET)
        {
            DiscardGraphicsResources();
        }
        EndPaint(m_hwnd, &ps);
    }
}

void MainWindow::UpdateMinkowskiSum() {
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
        pRenderTarget->DrawLine(D2D1::Point2F(0, rc.bottom / 2.f), D2D1::Point2F(rc.right, rc.bottom / 2.f), pBrush, 1);
        pRenderTarget->DrawLine(D2D1::Point2F(rc.right / 2.f, 0), D2D1::Point2F(rc.right / 2.f, rc.bottom), pBrush, 1);


        std::vector<struct point>* points1 = new std::vector<struct point>();
        std::vector<struct point>* points2 = new std::vector<struct point>();

        for (auto i = ellipses.begin(); i != ellipses.end(); i++) {
            (*i)->ellipse.point.x += xOffset;
            (*i)->ellipse.point.y += yOffset;
        }

        for (int i = 0; i < ellipses.size() / 2; i++) {
            auto iterator = ellipses.begin();
            std::advance(iterator, i);
            
            struct point p = { (*iterator)->ellipse.point.x, (*iterator)->ellipse.point.y };
            points1->push_back(p);
        }

        for (int i = ellipses.size() / 2; i < ellipses.size(); i++) {
            auto iterator = ellipses.begin();
            std::advance(iterator, i);

            struct point p = { (*iterator)->ellipse.point.x, (*iterator)->ellipse.point.y };
            points2->push_back(p);
        }

        ConvexHull* hull1 = new ConvexHull(*points1);
        std::vector<struct point>* hullPoints = hull1->getHull();

        pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));

        for (int i = 0; i < hullPoints->size() - 1; i++) {
            pRenderTarget->DrawLine(D2D1::Point2F((*hullPoints)[i].x, (*hullPoints)[i].y),
                D2D1::Point2F((*hullPoints)[i + 1].x, (*hullPoints)[i + 1].y),
                pBrush, 1);
        }

        pRenderTarget->DrawLine(D2D1::Point2F((*hullPoints)[hullPoints->size() - 1].x, (*hullPoints)[hullPoints->size() - 1].y),
            D2D1::Point2F((*hullPoints)[0].x, (*hullPoints)[0].y),
            pBrush, 1);

        ///////////////////////////////////////////////


        ConvexHull *hull2 = new ConvexHull(*points2);
        hullPoints = hull2->getHull();

        pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));

        for (int i = 0; i < hullPoints->size() - 1; i++) {
            pRenderTarget->DrawLine(D2D1::Point2F((*hullPoints)[i].x, (*hullPoints)[i].y),
                D2D1::Point2F((*hullPoints)[i + 1].x, (*hullPoints)[i + 1].y),
                pBrush, 1);
        }

        pRenderTarget->DrawLine(D2D1::Point2F((*hullPoints)[hullPoints->size() - 1].x, (*hullPoints)[hullPoints->size() - 1].y),
            D2D1::Point2F((*hullPoints)[0].x, (*hullPoints)[0].y),
            pBrush, 1);

        ConvexHull *newHull = hull1->minkowskiSum(hull1, hull2, conv);
        std::vector<struct point> *newHullPoints = newHull->getHull();

        pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));

        for (int i = 0; i < newHullPoints->size() - 1; i++) {
            pRenderTarget->DrawLine(D2D1::Point2F((*newHullPoints)[i].x, (*newHullPoints)[i].y),
                D2D1::Point2F((*newHullPoints)[i + 1].x, (*newHullPoints)[i + 1].y),
                pBrush, 1);
        }

        pRenderTarget->DrawLine(D2D1::Point2F((*newHullPoints)[newHullPoints->size() - 1].x, (*newHullPoints)[newHullPoints->size() - 1].y),
            D2D1::Point2F((*newHullPoints)[0].x, (*newHullPoints)[0].y),
            pBrush, 1);

        for (auto i = ellipses.begin(); i != ellipses.end(); ++i)
        {
            (*i)->Draw(pRenderTarget, pBrush);
        }

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
        std::vector<struct point> *hullPoints = hull->getHull();

        pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));

        for (int i = 0; i < hullPoints->size() - 1; i++) {
            pRenderTarget->DrawLine(D2D1::Point2F((*hullPoints)[i].x, (*hullPoints)[i].y),
                                    D2D1::Point2F((*hullPoints)[i + 1].x, (*hullPoints)[i + 1].y),
                                    pBrush, 1);
        }

        pRenderTarget->DrawLine(D2D1::Point2F((*hullPoints)[hullPoints->size() - 1].x, (*hullPoints)[hullPoints->size() - 1].y),
            D2D1::Point2F((*hullPoints)[0].x, (*hullPoints)[0].y),
            pBrush, 1);

        delete points;
        delete hull;
        delete hullPoints;

        for (auto i = ellipses.begin(); i != ellipses.end(); ++i)
        {
            (*i)->Draw(pRenderTarget, pBrush);
        }

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
        std::vector<struct point>* hullPoints = hull->getHull();

        pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));

        for (int i = 0; i < hullPoints->size() - 1; i++) {
            pRenderTarget->DrawLine(D2D1::Point2F((*hullPoints)[i].x, (*hullPoints)[i].y),
                D2D1::Point2F((*hullPoints)[i + 1].x, (*hullPoints)[i + 1].y),
                pBrush, 1);
        }

        pRenderTarget->DrawLine(D2D1::Point2F((*hullPoints)[hullPoints->size() - 1].x, (*hullPoints)[hullPoints->size() - 1].y),
            D2D1::Point2F((*hullPoints)[0].x, (*hullPoints)[0].y),
            pBrush, 1);

        delete points;
        delete hull;
        delete hullPoints;

        for (auto i = ellipses.begin(); i != ellipses.end(); ++i)
        {
            (*i)->Draw(pRenderTarget, pBrush);
        }

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
        std::vector<struct point>* hullPoints = hull->getHull();

        pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));

        for (int i = 0; i < hullPoints->size() - 1; i++) {
            pRenderTarget->DrawLine(D2D1::Point2F((*hullPoints)[i].x, (*hullPoints)[i].y),
                D2D1::Point2F((*hullPoints)[i + 1].x, (*hullPoints)[i + 1].y),
                pBrush, 1);
        }

        pRenderTarget->DrawLine(D2D1::Point2F((*hullPoints)[hullPoints->size() - 1].x, (*hullPoints)[hullPoints->size() - 1].y),
            D2D1::Point2F((*hullPoints)[0].x, (*hullPoints)[0].y),
            pBrush, 1);

        delete points;
        delete hull;
        delete hullPoints;

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
        std::vector<struct point>* hullPoints = hull->getHull();

        pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));

        for (int i = 0; i < hullPoints->size() - 1; i++) {
            pRenderTarget->DrawLine(D2D1::Point2F((*hullPoints)[i].x, (*hullPoints)[i].y),
                D2D1::Point2F((*hullPoints)[i + 1].x, (*hullPoints)[i + 1].y),
                pBrush, 1);
        }

        pRenderTarget->DrawLine(D2D1::Point2F((*hullPoints)[hullPoints->size() - 1].x, (*hullPoints)[hullPoints->size() - 1].y),
            D2D1::Point2F((*hullPoints)[0].x, (*hullPoints)[0].y),
            pBrush, 1);

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
        delete hullPoints;

        hr = pRenderTarget->EndDraw();
        if (FAILED(hr) || hr == D2DERR_RECREATE_TARGET)
        {
            DiscardGraphicsResources();
        }
        EndPaint(m_hwnd, &ps);
    }
}

void MainWindow::PaintGJK()
{
    HRESULT hr = CreateGraphicsResources();
    if (SUCCEEDED(hr))
    {
        PAINTSTRUCT ps;
        BeginPaint(m_hwnd, &ps);

        pRenderTarget->BeginDraw();

        pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::Yellow));

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
    else {
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
        if (inHull) {
            pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::Red));
        }
        else {
            pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::Green));
        }
        pRenderTarget->FillEllipse(ellipses.back()->ellipse, pBrush);
        pRenderTarget->EndDraw();
    }
    else if (mode == DragScreen) {
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
    else if (flags & MK_LBUTTON && mode == DragScreen) {
        xOffset = (dipX);
        yOffset = (dipY);
    }
}

HRESULT MainWindow::InsertEllipse(float x, float y)
{
    try
    {
        selection = ellipses.insert(
            ellipses.end(), 
            shared_ptr<MyEllipse>(new MyEllipse()));

        Selection()->ellipse.point = ptMouse = D2D1::Point2F(x, y);
        Selection()->ellipse.radiusX = Selection()->ellipse.radiusY = 2.0f; 
        Selection()->color = D2D1::ColorF( colors[nextColor] );

        nextColor = (nextColor + 1) % ARRAYSIZE(colors);
    }
    catch (std::bad_alloc)
    {
        return E_OUTOFMEMORY;
    }
    return S_OK;
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
            PaintMinkowskiDifference();
            return 0;

        case MINKOWSKI_SUM:
            paintMode = MINKOWSKI_SUM;
            PaintMinkowskiSum();
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
            PaintGJK();
            break;
        }
        break;

    }
    return DefWindowProc(m_hwnd, uMsg, wParam, lParam);
}
