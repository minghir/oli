#include "vLabel.hpp"
#include "../../ConsoleManager.hpp" // Pentru logare
#include "FontManager.hpp" // Pentru logare

// Constructor
vLabel::vLabel(HINSTANCE hInstance, const std::string& id, const std::wstring& text, int x, int y, int width, int height, EventDispatcher& dispatcher)
    : vControl(hInstance, id, x, y, width, height, dispatcher),
    //m_hInstance(hInstance),
    m_text(text)
    {
   // ConsoleManager::getInstance().log(L"[vLabel::Constructor] Apelat pentru ID: " + std::wstring(id.begin(), id.end()) + L", Text: '" + text + L"'");
}

// Metoda create - creează controlul STATIC
void vLabel::create(HWND parent)
{
   // ConsoleManager::getInstance().log(
    //    L"[vLabel::create] Creare Label cu ID: "
     //   + std::wstring(m_id.begin(), m_id.end())
      //  + L", Text: '" + m_text
       // + L"' în părinte HWND: "
       // + std::to_wstring(reinterpret_cast<uintptr_t>(parent))
    //);

    if (!parent) {
        ConsoleManager::getInstance().log(
            L"[ERROR] vLabel::create: Părintele HWND este nullptr."
        );
        return;
    }

    HINSTANCE hInstance = (HINSTANCE)GetWindowLongPtr(parent, GWLP_HINSTANCE);
    if (!hInstance) {
        LOG_ERROR(L"[ERROR] vLabel::create: Nu s-a putut obține HINSTANCE.");
        return;
    }

    UINT parentDpi = GetDpiForWindow(parent);
    scale(parentDpi);

    DWORD dwStyle = WS_CHILD | WS_VISIBLE | SS_NOTIFY;
    
    // Aliniere Orizontală
    if (hasFlag(m_textAlign, TextAlign::CENTER)) dwStyle |= SS_CENTER;
    else if (hasFlag(m_textAlign, TextAlign::RIGHT)) dwStyle |= SS_RIGHT;
    else dwStyle |= SS_LEFT;

    // Aliniere Verticală (Specific WinAPI pentru Static)
    if (hasFlag(m_textAlign, TextAlign::MIDDLE)) {
        dwStyle |= SS_CENTERIMAGE; // Atenție: merge doar pentru un singur rând!
    }
    
    m_handle = CreateWindowExW(
        0,
        L"STATIC",
        m_text.c_str(),
        dwStyle,
        getX(), getY(), getWidth(), getHeight(),
        parent,
        (HMENU)(uintptr_t)getWin32Id(),
        hInstance,
        this
    );

    if (!m_handle) {

        ConsoleManager::getInstance().log(
            L"[ERROR] vLabel::create: Eroare la crearea HWND. Cod: "
            + std::to_wstring(GetLastError())
        );
        return;
    }
    else {
        scaleFont(getCurrentDpi());
        //SetWindowLongPtr(m_handle, GWLP_USERDATA, (LONG_PTR)this);
    }
    /*

    // Font DPI-aware
    UINT currentDpi = GetDpiForWindow(m_handle);
    HFONT hFont = FontManager::getInstance().getScaledFont(
        m_fontName, m_baseFontSize, currentDpi
    );

    if (hFont)
        SendMessage(m_handle, WM_SETFONT, (WPARAM)hFont, TRUE);
        */
    // FIX CRITIC: dimensiunea inițială
    GetClientRect(m_handle, &m_originalClientRect);

   // ConsoleManager::getInstance().log(
     //   L"[vLabel::create] Label-ul (ID: "
       // + std::wstring(m_id.begin(), m_id.end())
        //+ L") a fost creat cu succes. HWND: "
        //+ std::to_wstring(reinterpret_cast<uintptr_t>(m_handle))
    //);
}


// Gestionarea mesajelor - pentru label, de obicei nu sunt multe mesaje specifice de interceptat.
LRESULT vLabel::handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // Label-urile de obicei nu au mesaje specifice care necesită o gestionare complexă aici.
    // WM_SETFONT va fi gestionat automat de controlul STATIC.
    // Poți adăuga logici pentru alte mesaje dacă vrei (ex: mouse hover, click, dacă vrei ca label-ul să fie interactiv)
    /*
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rect;
        GetClientRect(hwnd, &rect);

        // --- DESENEAZĂ FUNDALUL ---
        // Dacă nu umplem fundalul, va rămâne griul standard sau ce era înainte
        HBRUSH hBrush = CreateSolidBrush(m_backgroundColor);
        FillRect(hdc, &rect, hBrush);
        DeleteObject(hBrush);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, m_textColor);

        // Folosește fontul scalat corect
        HGDIOBJ oldFont = SelectObject(hdc, m_hFont);

        UINT format = DT_SINGLELINE;

        if (hasFlag(m_textAlign, TextAlign::CENTER)) format |= DT_CENTER;
        else if (hasFlag(m_textAlign, TextAlign::RIGHT)) format |= DT_RIGHT;
        else format |= DT_LEFT;

        if (hasFlag(m_textAlign, TextAlign::MIDDLE)) format |= DT_VCENTER;
        else if (hasFlag(m_textAlign, TextAlign::BOTTOM)) format |= DT_BOTTOM;
        else format |= DT_TOP;

        // IMPORTANT: Pentru DT_BOTTOM sau DT_VCENTER, WinAPI cere DT_SINGLELINE
        DrawText(hdc, m_text.c_str(), -1, &rect, format);

        SelectObject(hdc, oldFont);
        EndPaint(hwnd, &ps);
        return 0;
        
    }
        */
    return vControl::handleMessage(hwnd, msg, wParam, lParam);
}

// Metodă pentru a schimba textul label-ului
void vLabel::setText(const std::wstring& newText) {
    m_text = newText;
    if (m_handle) {
        // WM_SETTEXT actualizează textul afișat de controlul static
        
        SetWindowTextW(m_handle, m_text.c_str());
        InvalidateRect(m_handle, NULL, TRUE);
    //    ConsoleManager::getInstance().log(L"[vLabel::setText] Text label ID: " + std::wstring(m_id.begin(), m_id.end()) + L" schimbat în: '" + newText + L"'");
    }
    else {
        ConsoleManager::getInstance().log(L"[ERROR] vLabel::setText: HWND invalid pentru label ID: " + std::wstring(m_id.begin(), m_id.end()));
    }
}

// Metodă pentru a obține textul label-ului
std::wstring vLabel::getText() const {
    if (m_handle) {
        int length = GetWindowTextLength(m_handle);
        if (length > 0) {
            std::vector<wchar_t> buffer(length + 1);
            GetWindowTextW(m_handle, buffer.data(), length + 1);
            return std::wstring(buffer.data());
        }
    }
    return m_text; // Returnează textul stocat în membru dacă HWND-ul nu e gata
}