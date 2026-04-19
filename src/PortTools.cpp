#include "PortTools.hpp"
#include <fstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <iconv.h>
#include <errno.h>
#endif

namespace PortTools {

    std::wstring utf8_to_wstring(const std::string& str) {
        if (str.empty()) return L"";
#ifdef _WIN32
        int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
        std::wstring result(size, 0);
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &result[0], size);
        return result;
#else
        iconv_t cd = iconv_open("WCHAR_T", "UTF-8");
        if (cd == (iconv_t)-1) return L"";
        size_t inBytes = str.size();
        size_t outBytes = (str.size() + 1) * sizeof(wchar_t);
        std::wstring result(str.size() + 1, 0);
        char* inBuf = const_cast<char*>(str.data());
        char* outBuf = reinterpret_cast<char*>(&result[0]);
        if (iconv(cd, &inBuf, &inBytes, &outBuf, &outBytes) == (size_t)-1) {
            iconv_close(cd);
            return L"";
        }
        iconv_close(cd);
        result.resize((reinterpret_cast<wchar_t*>(outBuf) - result.data()));
        return result;
#endif
    }

    std::string wstring_to_utf8(const std::wstring& wstr) {
        if (wstr.empty()) return "";
#ifdef _WIN32
        int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), NULL, 0, NULL, NULL);
        std::string result(size, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &result[0], size, NULL, NULL);
        return result;
#else
        iconv_t cd = iconv_open("UTF-8", "WCHAR_T");
        if (cd == (iconv_t)-1) return "";
        size_t inBytes = wstr.size() * sizeof(wchar_t);
        size_t outBytes = (wstr.size() * 4) + 1;
        std::string result(outBytes, 0);
        char* inBuf = (char*)wstr.data();
        char* outBuf = &result[0];
        if (iconv(cd, &inBuf, &inBytes, &outBuf, &outBytes) == (size_t)-1) {
            iconv_close(cd);
            return "";
        }
        iconv_close(cd);
        result.resize(result.size() - outBytes);
        return result;
#endif
    }

    void openIfstream(std::ifstream& file, const std::wstring& path) {
#ifdef _WIN32
        file.open(path); // Windows suportă wchar_t direct
#else
        file.open(wstring_to_utf8(path)); // Linux are nevoie de char* (UTF-8)
#endif
    }

} // namespace PortTools