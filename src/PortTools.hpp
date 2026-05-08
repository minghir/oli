#ifndef PORTTOOLS_HPP
#define PORTTOOLS_HPP

#include <string>
#include <vector>

namespace PortTools {

    // Definim un tip generic pentru handle-ul librăriei
#ifdef _WIN32
    using LibHandle = void*; // HMODULE este în esență void*
#define NOMINMAX
#else
    using LibHandle = void*;
#endif

    // Încarcă o bibliotecă dinamică (.dll sau .so)
    LibHandle loadDynamicLibrary(const std::wstring& path);

    // Caută o funcție în bibliotecă
    void* getFunctionSymbol(LibHandle handle, const std::string& symbolName);

    // Eliberează biblioteca din memorie
    void freeDynamicLibrary(LibHandle handle);

    // Returnează ultima eroare de sistem sub formă de text
    std::wstring getLastErrorString();


    // Conversii fundamentale
    std::string wstring_to_utf8(const std::wstring& wstr);
    std::wstring utf8_to_wstring(const std::string& str);

    // Conversii specifice pentru CodePage (ex: 1250)
    std::wstring convertToWide(const std::string& input, unsigned int codePage = 65001); // 65001 = CP_UTF8

    // Utilitare pentru fișiere (pentru a abstractiza diferența de std::ifstream)
    void openIfstream(std::ifstream& file, const std::wstring& path);

    FILE* openPipe(const std::wstring& command, const wchar_t* mode);
    int closePipe(FILE* pipe);
    bool readLineFromPipe(FILE* pipe, std::wstring& outLine);

    bool getConsoleInput(const std::wstring& prompt, std::wstring& outLine);

    std::wstring normalize_newlines_for_write(const std::wstring& input);

    // Returnează timpul curent formatat conform unui șablon (ex: "%Y-%m-%d")
    std::wstring getFormattedTime(const std::wstring& format);

    // Returnează extensia corectă pentru plugin (.dll sau .so)
    std::wstring getPluginExtension();
}

#endif