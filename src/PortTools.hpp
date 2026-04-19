#ifndef PORTTOOLS_HPP
#define PORTTOOLS_HPP

#include <string>
#include <vector>

namespace PortTools {
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
}

#endif