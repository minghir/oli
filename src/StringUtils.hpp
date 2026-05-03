#ifndef STRINGUTILS_HPP
#define STRINGUTILS_HPP

#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <exception>
#include <string_view>
#include <cwctype>

typedef unsigned int uint;

template<typename T>
std::string to_string(const T& t) {
    std::ostringstream s;
    s << t;
    return s.str();
}

template<typename T>
T from_string(const std::string& s) {
    std::istringstream is(s);
    T t;
    is >> t;
    return t;
}

template<typename T>
std::wstring to_wstring(const T& t) {
    std::wostringstream woss;
    woss << t;
    return woss.str();
}



/*
template<typename T>
T from_wstring(const std::wstring& ws) {
    std::wistringstream wis(ws);
    T t;
    wis >> t;
    return t;
}
*/
template<typename T>
T from_wstring(const std::wstring& ws) {
    std::wistringstream wis(ws);
    T t;
    wis >> std::ws >> t;

    if (wis.fail()) {
        throw std::runtime_error("Conversia din wstring a esuat.");
    }

    return t;
}


std::wstring str_to_wstr(const std::string& str);
std::string wstr_to_str(const std::wstring& wstr);

std::vector <std::string> explode(std::string str, std::string ch);
std::vector <std::string> explode(std::string str, std::string sch, std::string ech);
std::vector <std::string> explode(std::string str, char ch = ' ');
std::vector<std::wstring> wexplode(const std::wstring& str, wchar_t ch);
std::vector<std::wstring> wexplodeQuoteSafe(const std::wstring& str, wchar_t ch);


std::string implode(std::vector<std::string> str, std::string ch = " ");
std::string implode(std::vector<std::string> str, std::string sch, std::string ech);


std::wstring implode(std::vector<std::wstring> str, wchar_t ch);
std::wstring implode(std::vector<std::wstring> str, std::wstring ch);

std::string addslashes(std::string str, uint pos = 0);
std::string stripslashes(std::string str);
std::string rm_blank(std::string str, uint pos = 0);
std::string rm_char(const std::string& str, char c);
std::wstring rm_char(const std::wstring& str, wchar_t c);
std::string rpl_ch_in_str(const std::string& s, char old, const std::string& repl);
std::wstring rpl_ch_in_wstr(const std::wstring& s, wchar_t old, const std::wstring& repl);
std::wstring rpl_wstr_in_wstr(std::wstring str, const std::wstring& from, const std::wstring& to); 
std::string rpl_str_in_str(std::string str, const std::string& from, const std::string& to);

std::wstring wstr_trim(const std::wstring& str);
std::wstring trim(const std::wstring& str);

std::string trim(const std::string& str);



void print_wstr_vct(const std::vector<std::wstring>& vec);
void print_str_vct(const std::vector<std::string>& vec);
void print_wstr_map(const std::map<std::wstring, std::wstring>& m); 
void print_str_map(const std::map<std::string, std::string>& m); 

std::wstring replaceSpecialCharacters(const std::wstring& input);
void trim_wstr_vec(std::vector<std::wstring>& vec);

std::wstring wstr_rtf_escap(const std::wstring& input);
std::wstring processStringForRTF(const std::wstring& input);

std::wstring getWstrBetween(const std::wstring& input, wchar_t deschidere, wchar_t inchidere);
std::string getStrBetween(const std::string& input, char deschidere, char inchidere);

std::string sleft_of(const std::string& input, char delimiter);
std::wstring wleft_of(const std::wstring& input, wchar_t delimiter);

std::wstring wstrToLower(const std::wstring& str);
std::wstring stripQuotes(const std::wstring& str);



std::string strExtractPrefix(const std::string& input, char c = '(');

std::wstring to_wstring_precise(long double value, int precision = 15 );
std::wstring to_wstring_precise2(long double value, int precision);

std::wstring normalize_number(const std::wstring& ws, int precision = 2);

bool is_plain_decimal(const std::wstring& ws);

std::wstring escape_commas(const std::wstring& input);


std::vector<std::wstring> splitCSVLine(const std::wstring& line, wchar_t delimiter = L',');

bool contains_substring(const std::wstring& text, const std::wstring& pattern);

std::wstring sanitize_filename(const std::wstring& input);

size_t count_char_in_wstring(const std::wstring& str, wchar_t ch);

int numaraFisiereCuExtensie(const std::string& caleDirector, const std::string& extensie);


std::string utf8_encode(const std::wstring& wstr);
std::string wstring_to_utf8(const std::wstring& wstr);
std::wstring utf8_to_wstring(const std::string& str);

std::wstring to_upper(const std::wstring& input);
std::wstring to_lower(const std::wstring& input);

std::wstring trim_zeros(const std::wstring& input, int max_dec = 0);

std::wstring convertCp1250ToWideChar(const std::string& input);
std::wstring convertSingleByteToWideChar(const std::string& input, unsigned int codePage);

std::vector<std::wstring> split_to_words(const std::wstring& text);

std::wstring normalizeSpaces(const std::wstring& input);
std::wstring shellNormalizeSpaces(const std::wstring& input);

bool isNumber(const std::wstring& s);

bool startsWith(const std::wstring& text, const std::wstring& prefix, bool ignoreCase = true);

bool iequals(std::wstring_view a, std::wstring_view b);
std::wstring unescape(const std::wstring& s);

std::vector<std::wstring> splitWBySemicolon(const std::wstring& s);
#endif // STRINGUTILS_HPP

