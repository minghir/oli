#ifndef WINFILEDIALOG_HPP
#define WINFILEDIALOG_HPP
#include <windows.h>
#include <commdlg.h>
#include <string>

class WinFileDialog {
private:
    std::wstring m_title;
    std::wstring m_filter;
    std::wstring m_resultPath;
	std::wstring m_initialPath;

public:
    WinFileDialog(const std::wstring& title = L"Select File")
        : m_title(title), m_filter(L"All Files\0*.*\0") {
    }

    void setFilter(const std::wstring& filter) { m_filter = filter; }
	void setInitialPath(const std::wstring& path) { m_initialPath = path; }

    bool showOpen(HWND parent = nullptr) {
        wchar_t szFile[MAX_PATH] = { 0 };
        wchar_t szCurrentDir[MAX_PATH] = { 0 };
        GetCurrentDirectoryW(MAX_PATH, szCurrentDir);

        OPENFILENAMEW ofn = { 0 };
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = parent;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrInitialDir = szCurrentDir;

        // 🔥 AICI ESTE MAGIA PENTRU FILTRARE:
        // Formatul este: Descriere1\0*.extensie1\0Descriere2\0*.*\0\0
        ofn.lpstrFilter = L"Oli Files (*.oli)\0*.oli\0All Files (*.*)\0*.*\0";
        ofn.nFilterIndex = 1; // Selectează automat primul filtru (.oli) la deschidere

        // Păstrăm și OFN_NOCHANGEDIR ca să nu ne strice căile relative!
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;

        if (GetOpenFileNameW(&ofn)) {
            m_resultPath = szFile;
            return true;
        }
        return false;
    }

	/*
    bool showSave(HWND parent = nullptr) {
        wchar_t szFile[MAX_PATH] = { 0 };
        OPENFILENAMEW ofn = { 0 };
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = parent;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = MAX_PATH;

        // Filtru identic și pentru salvare
        ofn.lpstrFilter = L"Oli Files (*.oli)\0*.oli\0All Files (*.*)\0*.*\0";
        ofn.nFilterIndex = 1;

        // 🔥 Extensia implicită în caz că utilizatorul uită să o scrie în căsuță
        ofn.lpstrDefExt = L"oli";

        ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;

        if (GetSaveFileNameW(&ofn)) {
            m_resultPath = szFile;
            return true;
        }
        return false;
    }
	*/
	bool showSave(HWND parent = nullptr) {
        wchar_t szFile[MAX_PATH] = { 0 };
        
        // Copiem calea inițială în buffer-ul de ieșire
        if (!m_initialPath.empty()) {
            wcsncpy_s(szFile, m_initialPath.c_str(), MAX_PATH);
        }

        OPENFILENAMEW ofn = { 0 };
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = parent;
        ofn.lpstrFile = szFile; // Acesta va conține calea inițială
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrFilter = L"Oli Files (*.oli)\0*.oli\0All Files (*.*)\0*.*\0";
        ofn.nFilterIndex = 1;
        ofn.lpstrDefExt = L"oli";
        ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;

        if (GetSaveFileNameW(&ofn)) {
            m_resultPath = szFile;
            return true;
        }
        return false;
    }

    std::wstring getFilePath() const { return m_resultPath; }
};
#endif // WINFILEDIALOG_HPP