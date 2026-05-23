#include "vSeparator.hpp"

vSeparator::vSeparator(HINSTANCE hInstance, const std::string& id, int x, int y, int size,
    SeparatorOrientation orientation, EventDispatcher& disp)
    : vControl(hInstance, id, x, y, 0, 0, disp),
    m_orientation(orientation),
    m_size(size)
{
    m_ControlType = ControlType::Separator;

    if (m_orientation == SeparatorOrientation::Horizontal) {
        m_width = size;
        m_height = 20;
        setWidthMode(SizeMode::FILL);
    }
    else {
        m_width = 20;
        m_height = size;
        setWidthMode(SizeMode::FIXED);
    }

    setHeightMode(SizeMode::FIXED);
}

void vSeparator::create(HWND parent) {
    if (!parent) return;

    // Folosim 0 în loc de CreateWindowEx flags complicate pentru început
    m_handle = CreateWindowExW(
        WS_EX_TOPMOST, // Îl punem peste tot
        L"STATIC",
        nullptr,
        WS_CHILD | WS_VISIBLE | SS_NOTIFY,
        m_x, m_y, m_width, m_height,
        parent, (HMENU)(INT_PTR)getWin32Id(), m_hInstance, nullptr
    );

    if (m_handle) {
        SetWindowLongPtr(m_handle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

        // Subclassing
        m_originalWndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(m_handle, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(vControl::StaticWndProc)));

        // Îl aducem în față explicit
        BringWindowToTop(m_handle);
        InvalidateRect(m_handle, NULL, TRUE);
    }
}

LRESULT vSeparator::handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // LOG DE TEST - Dacă nici ăsta nu apare, subclassing-ul e mort
    if (msg == WM_PAINT) {
        LOG_ERROR(L"!!! EVENT DETECTED: WM_PAINT on %S"+ str_to_wstr(m_id));
    }

    switch (msg)
    {
    case WM_PAINT: {
        LOG_ERROR(L"!!! PAINT EVENT !!!"); // Schimbă textul să fii sigur că e cel nou
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rc;
        GetClientRect(hwnd, &rc);

        HBRUSH br = CreateSolidBrush(RGB(255, 0, 0));
        FillRect(hdc, &rc, br);
        DeleteObject(br);

        EndPaint(hwnd, &ps);
        return 0; // Obligatoriu 0 pentru WM_PAINT
    }
                 // Asigură-te că nu returnezi 1 la WM_ERASEBKGND fără să desenezi nimic
    case WM_ERASEBKGND:
        return 1;
    }

    if (m_originalWndProc)
        return CallWindowProc(m_originalWndProc, hwnd, msg, wParam, lParam);

    return vControl::handleMessage(hwnd, msg, wParam, lParam);
}