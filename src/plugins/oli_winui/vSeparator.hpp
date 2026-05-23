#ifndef VSEPARATOR_HPP
#define VSEPARATOR_HPP

#include "vControl.hpp"

enum class SeparatorOrientation {
    Horizontal,
    Vertical
};

class vSeparator : public vControl {
    WNDPROC m_originalWndProc = nullptr;

public:
    vSeparator(HINSTANCE hInstance, const std::string& id, int x, int y, int size,
        SeparatorOrientation orientation, EventDispatcher& disp);

    void create(HWND parent) override;
    LRESULT handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    SeparatorOrientation m_orientation;
    int m_size;
};

#endif
