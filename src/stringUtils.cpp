#include "stringUtils.hpp"

#include <fstream>
#include <iostream>
#include <vector>
#include <iomanip>
#include <locale>
#include <algorithm>
#include <exception>
#include <string>
#include <cwctype>
#include <cctype>
#include <unordered_map>
#include <regex>
#include <windows.h>


using namespace std;

vector <std::string> explode(std::string str, char ch) {
    vector <std::string> res;
    int pos = 0, lpos = 0;
    if (str[0] == ch)
        str = str.substr(1, str.size());

    if (str[str.size() - 1] == ch)
        str = str.substr(0, str.size() - 1);

    if (str.find(ch) == string::npos) {
        res.push_back(str);
        return res;
    }

    std::string test, cut;
    test = str;

    do {
        lpos = lpos == pos ? pos : pos + 1;
        pos = str.find(ch, pos + 1);

        if (pos < 0)
            break;
        cut = str.substr(lpos, pos - lpos);
        res.push_back(cut);

        test = str.substr(pos, str.size());

    } while ((uint)pos != string::npos);
    if (test.size() > 0) {
        res.push_back(test.substr(1, test.size()));
    }

    return res;
}

vector <std::string> explode(std::string str, std::string ch) {
    int pos = 0, lpos = 0;
    vector <std::string> res;

    do {
        lpos = lpos == pos ? pos : pos + ch.size();
        pos = str.find(ch, pos + 1);

        if (pos < 0)
            break;

        res.push_back(str.substr(lpos, pos - lpos));
    } while ((uint)pos != string::npos);
    return res;
}

vector <std::string> explode(std::string str, std::string sch, std::string ech) {

    int pos = 0, lpos = 0;
    vector <std::string> res;

    do {
        pos = lpos == 0 ? str.find(sch, pos) : str.find(sch, pos + 1);
        lpos = str.find(ech, lpos + 1);

        if (pos < 0 || lpos < 0)
            break;

        res.push_back(str.substr(pos + sch.size(), lpos - pos - sch.size()));
    } while ((uint)pos != std::string::npos);
    return res;
}

string implode(vector<std::string> str, std::string ch) {
    string res;
    vector <std::string> ::iterator it = str.begin();
    int i = 0;
    for (it = str.begin(); it != str.end(); ++it, i++)
        if (i == (int)str.size() - 1)
            res += (*it);
        else
            res += (*it) + ch;

    return res;
}

string implode(vector<std::string> str, std::string sch, std::string ech) {
    string res;
    vector <std::string> ::iterator it = str.begin();

    for (it = str.begin(); it != str.end(); ++it)
        res += sch + (*it) + ech;

    return res;
}


wstring implode(vector<std::wstring> str, wchar_t ch) {
    wstring res;
    vector <std::wstring> ::iterator it = str.begin();
    int i = 0;
    for (it = str.begin(); it != str.end(); ++it, i++)
        if (i == (int)str.size() - 1)
            res += (*it);
        else
            res += (*it) + ch;

    return res;
}


wstring implode(vector<std::wstring> str, std::wstring ch) {
    wstring res;
    vector <std::wstring> ::iterator it = str.begin();
    int i = 0;
    for (it = str.begin(); it != str.end(); ++it, i++)
        if (i == (int)str.size() - 1)
            res += (*it);
        else
            res += (*it) + ch;

    return res;
}



string addslashes(std::string str, uint pos) {
    if ((pos = str.find('\'', pos)) != std::string::npos) {
        str.insert(pos, "\\");
        return addslashes(str, pos + 2);
    }
    return str;
}


string stripslashes(std::string str) {
    return str = "aa";
}

string rm_blank(std::string str, uint pos) {

    if ((pos = str.find(' ', pos)) != string::npos) {
        str[pos] = '_';
        return rm_blank(str, pos + 2);
    }

    return str;
}

std::string rm_char(const std::string& str, char c) {
    std::string result = str;
    result.erase(std::remove(result.begin(), result.end(), c), result.end());
    return result;
}


std::string rpl_ch_in_str(const std::string& s, char old, const std::string& repl) {
    std::string r;
    for (char c : s) r += (c == old ? repl : std::string(1, c));
    return r;
}


std::wstring rpl_ch_in_wstr(const std::wstring& s, wchar_t old, const std::wstring& repl) {
    std::wstring r;
    for (wchar_t c : s) r += (c == old ? repl : std::wstring(1, c));
    return r;
}

std::wstring rm_char(const std::wstring& str, wchar_t c) {
    auto res = str;
    res.erase(std::remove(res.begin(), res.end(), c), res.end());
    return res;
}

/*
std::wstring str_to_wstr(const std::string& str) {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return converter.from_bytes(str);
}

std::wstring str_to_wstr(const std::string& str) {
    return std::wstring(str.begin(), str.end());
}
/*
std::string wstr_to_str(const std::wstring& wstr) {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return converter.to_bytes(wstr);
}
*/
std::wstring str_to_wstr(const std::string& str) {
    if (str.empty()) return L"";

    // 1. Aflăm de cât spațiu avem nevoie în noul string (wide)
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);

    // 2. Alocăm buffer-ul necesar
    std::wstring wstrTo(size_needed, 0);

    // 3. Facem conversia propriu-zisă
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);

    return wstrTo;
}


std::string wstr_to_str(const std::wstring& wstr) {
    std::string str;
    for (wchar_t wc : wstr) {
        if (wc <= 0x7F) {
            str.push_back(static_cast<char>(wc));
        } else {
            // Conversie manuală pentru caractere non-ASCII (poate fi îmbunătățită)
            str.push_back('?'); // Înlocuiește caracterele non-ASCII cu '?'
        }
    }
    return str;
}




void print_str_vct(const std::vector<std::string>& vec) {
    std::cout << "Vector {\n";
    for (size_t i = 0; i < vec.size(); ++i) {
        std::cout << "  [" << i << "] => " << vec[i] << "\n";
    }
    std::cout << "}" << std::endl;
}

void print_wstr_vct(const std::vector<std::wstring>& vec) {
    std::wcout << L"Vector {\n";
    for (size_t i = 0; i < vec.size(); ++i) {
        std::wcout << L"  [" << i << L"] => " << vec[i] << L"\n";
    }
    std::wcout << L"}" << std::endl;
}

void print_wstr_map(const std::map<std::wstring, std::wstring>& m) {
    std::wcout << L"Map {\n";
    for (const auto& [key, value] : m) {
        std::wcout << L"  [" << key << L"] => " << value << L"\n";
    }
    std::wcout << L"}" << std::endl;
}

void print_str_map(const std::map<std::string, std::string>& m) {
    std::cout << "Map {\n";
    for (const auto& [key, value] : m) {
        std::cout << "  [" << key << "] => " << value << "\n";
    }
    std::cout << "}" << std::endl;
}

std::wstring rpl_wstr_in_wstr(std::wstring str, const std::wstring& from, const std::wstring& to) {
    if (from.empty()) {
        return str; // Evită bucla infinită dacă șirul de căutat este gol
    }

    size_t pos = 0;
    while ((pos = str.find(from, pos)) != std::wstring::npos) {
        str.replace(pos, from.length(), to);
        pos += to.length(); // Evită recapturarea aceluiași segment
    }

    return str;
}

std::string rpl_str_in_str(std::string str, const std::string& from, const std::string& to) {
    if (from.empty()) {
        return str; // Evită bucla infinită dacă șirul de căutat este gol
    }

    size_t pos = 0;
    while ((pos = str.find(from, pos)) != std::string::npos) {
        str.replace(pos, from.length(), to);
        pos += to.length(); // Evită recapturarea aceluiași segment
    }

    return str;
}

/*
std::wstring wstr_trim(const std::wstring& str) {
    std::wstring t_str = str;
    size_t start = t_str.find_first_not_of(L" \t\n\r\0");
    size_t end = t_str.find_last_not_of(L" \t\n\r\0");
    
    t_str.erase(std::remove(t_str.begin(), t_str.end(), L'\0'), t_str.end());

    return (start == std::wstring::npos || end == std::wstring::npos) ? L"" : t_str.substr(start, end - start + 1);
}
*/

std::wstring wstr_trim(const std::wstring& str) {

    // Setul de spații albe standard (fără a include L'\0', care nu ar trebui să fie acolo oricum)
    const wchar_t* whitespace = L" \t\n\r";

    // 1. Găsește primul caracter non-spațiu
    size_t start = str.find_first_not_of(whitespace);

    if (start == std::wstring::npos) {
        return L""; // Gol sau doar spații albe
    }

    // 2. Găsește ultimul caracter non-spațiu
    size_t end = str.find_last_not_of(whitespace);

    // 3. Returnează subșirul din șirul original (lungimea = end - start + 1)
    return str.substr(start, end - start + 1);
}

std::wstring trim(const std::wstring& str) {
    return wstr_trim(str);
}

std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

/*
std::wstring wstr_trim(const std::wstring& str) {
    size_t start = str.find_first_not_of(L" \t\n\r");
    size_t end = str.find_last_not_of(L" \t\n\r");

    if (start == std::wstring::npos || end == std::wstring::npos)
        return L"";  // Dacă doar spații, returnează un șir gol

    return str.substr(start, end - start + 1);
}



std::vector<std::wstring> wexplode(const std::wstring& str, wchar_t ch) {
    std::vector<std::wstring> res;
    size_t pos = 0, lpos = 0;
    std::wstring tempStr = str;

    // Eliminăm separatorul de la început, dacă există
    if (!tempStr.empty() && tempStr[0] == ch)
        tempStr = tempStr.substr(1);

    // Eliminăm separatorul de la sfârșit, dacă există
    if (!tempStr.empty() && tempStr[tempStr.size() - 1] == ch)
        tempStr = tempStr.substr(0, tempStr.size() - 1);

    // Dacă nu există separator, returnăm întreaga linie
    if (tempStr.find(ch) == std::wstring::npos) {
        res.push_back(tempStr);
        return res;
    }

    std::wstring test, cut;
    test = tempStr;

    do {
        lpos = (lpos == pos) ? pos : pos + 1;
        pos = tempStr.find(ch, pos + 1);

        if (pos == std::wstring::npos)
            break;

        cut = tempStr.substr(lpos, pos - lpos);
        res.push_back(cut);

        test = tempStr.substr(pos);
    } while (pos != std::wstring::npos);

    if (!test.empty()) {
        res.push_back(test.substr(1));
    }

    return res;
}
*/

std::vector<std::wstring> wexplode(const std::wstring& str, wchar_t ch) {
    std::vector<std::wstring> res;
    std::wstring tempStr = wstr_trim(str);
    size_t start = 0;
    size_t end = tempStr.find(ch);

    while (end != std::wstring::npos) {
        std::wstring token = tempStr.substr(start, end - start);
        res.push_back(wstr_trim(token)); // chiar dacă e gol, îl adaugă
        start = end + 1;
        end = tempStr.find(ch, start);
    }

    // Adaugă ultimul token (sau un string gol dacă se termină cu separator)
    std::wstring lastToken = tempStr.substr(start);
    res.push_back(wstr_trim(lastToken));

    return res;
}


std::vector<std::wstring> wexplodeQuoteSafe(const std::wstring& str, wchar_t ch) {
    std::vector<std::wstring> res;
    std::wstring current;
    bool inQuotes = false;

    for (wchar_t c : str) {
        if (c == L'\'') inQuotes = !inQuotes; // Schimbăm starea când găsim ghilimele

        if (c == ch && !inQuotes) {
            res.push_back(wstr_trim(current));
            current.clear();
        }
        else {
            current += c;
        }
    }
    res.push_back(wstr_trim(current));
    return res;
}

/*
std::vector<std::wstring> wexplode(const std::wstring& str, wchar_t ch) {
    std::vector<std::wstring> res;
    size_t pos = 0, lpos = 0;
    std::wstring tempStr = str;

    // Eliminăm separatorul de la început și sfârșit
    tempStr = wstr_trim(tempStr);

    // Dacă nu există separator, returnăm întreaga linie
    if (tempStr.find(ch) == std::wstring::npos) {
        res.push_back(tempStr);
        return res;
    }

    std::wstring test, cut;
    test = tempStr;

    do {
        lpos = (lpos == pos) ? pos : pos + 1;
        pos = tempStr.find(ch, pos + 1);

        if (pos == std::wstring::npos)
            break;

        cut = wstr_trim(tempStr.substr(lpos, pos - lpos));
        res.push_back(cut);

        test = tempStr.substr(pos);
    } while (pos != std::wstring::npos);

    if (!test.empty()) {
        res.push_back(wstr_trim(test.substr(1)));
    }

    return res;
}
*/


std::wstring replaceSpecialCharacters(const std::wstring& input) {
    // Map pentru caractere speciale și echivalentele lor ASCII
    std::unordered_map<wchar_t, wchar_t> replacements = {
       {L'“', L'"'}, {L'”', L'"'}, {L'‘', L'\''}, {L'’', L'\''}, // Ghilimele tipografice
       {L'–', L'-'}, {L'—', L'-'}, {L'…', L'.'}, // Linii și puncte
       {L'À', L'A'}, {L'Á', L'A'}, {L'Â', L'A'}, {L'Ã', L'A'}, {L'Ä', L'A'}, // Majuscule
       {L'á', L'a'}, {L'à', L'a'}, {L'â', L'a'}, {L'ã', L'a'}, {L'ä', L'a'}, // Minuscule
       {L'É', L'E'}, {L'È', L'E'}, {L'Ê', L'E'}, {L'Ë', L'E'},
       {L'é', L'e'}, {L'è', L'e'}, {L'ê', L'e'}, {L'ë', L'e'},
       {L'Í', L'I'}, {L'Ì', L'I'}, {L'Î', L'I'}, {L'Ï', L'I'},
       {L'í', L'i'}, {L'ì', L'i'}, {L'î', L'i'}, {L'ï', L'i'},
       {L'Ó', L'O'}, {L'Ò', L'O'}, {L'Ô', L'O'}, {L'Õ', L'O'}, {L'Ö', L'O'},
       {L'ó', L'o'}, {L'ò', L'o'}, {L'ô', L'o'}, {L'õ', L'o'}, {L'ö', L'o'},
       {L'Ú', L'U'}, {L'Ù', L'U'}, {L'Û', L'U'}, {L'Ü', L'U'},
       {L'ú', L'u'}, {L'ù', L'u'}, {L'û', L'u'}, {L'ü', L'u'},
       {L'Ç', L'C'}, {L'ç', L'c'},
       {L'Ñ', L'N'}, {L'ñ', L'n'}
    };



    std::wstring result = input;
    std::transform(result.begin(), result.end(), result.begin(), [&](wchar_t ch) {
        return replacements.count(ch) ? replacements[ch] : ch;
        });

    return result;
}

void trim_wstr_vec(std::vector<std::wstring>& vec) {
    for (auto& str : vec) {
        if (!str.empty()) { // Evită accesarea unui string gol
            str = wstr_trim(str);
        }
    }
}

std::wstring wstr_rtf_escap(const std::wstring& input) {
    std::wostringstream escaped;

    for (wchar_t c : input) {
        if (c == L'\\' || c == L'{' || c == L'}') {
            escaped << L'\\' << c; // Escapare pentru RTF
        }
        else if (c > 127) { // Caractere speciale sau non-ASCII
            escaped << L"\\u" << static_cast<int>(c) << L"?";
        }
        else {
            escaped << c;
        }
    }

    return escaped.str();
}



std::wstring processStringForRTF(const std::wstring& input) {
    // Map pentru caractere speciale și echivalentele lor ASCII
    std::unordered_map<wchar_t, wchar_t> replacements = {
       {L'“', L'"'}, {L'”', L'"'}, {L'‘', L'\''}, {L'’', L'\''}, // Ghilimele tipografice
       {L'–', L'-'}, {L'—', L'-'}, {L'…', L'.'}, // Linii și puncte
       {L'À', L'A'}, {L'Á', L'A'}, {L'Â', L'A'}, {L'Ã', L'A'}, {L'Ä', L'A'}, // Majuscule
       {L'á', L'a'}, {L'à', L'a'}, {L'â', L'a'}, {L'ã', L'a'}, {L'ä', L'a'}, // Minuscule
       {L'É', L'E'}, {L'È', L'E'}, {L'Ê', L'E'}, {L'Ë', L'E'},
       {L'é', L'e'}, {L'è', L'e'}, {L'ê', L'e'}, {L'ë', L'e'},
       {L'Í', L'I'}, {L'Ì', L'I'}, {L'Î', L'I'}, {L'Ï', L'I'},
       {L'í', L'i'}, {L'ì', L'i'}, {L'î', L'i'}, {L'ï', L'i'},
       {L'Ó', L'O'}, {L'Ò', L'O'}, {L'Ô', L'O'}, {L'Õ', L'O'}, {L'Ö', L'O'},
       {L'ó', L'o'}, {L'ò', L'o'}, {L'ô', L'o'}, {L'õ', L'o'}, {L'ö', L'o'},
       {L'Ú', L'U'}, {L'Ù', L'U'}, {L'Û', L'U'}, {L'Ü', L'U'},
       {L'ú', L'u'}, {L'ù', L'u'}, {L'û', L'u'}, {L'ü', L'u'},
       {L'Ç', L'C'}, {L'ç', L'c'},
       {L'Ñ', L'N'}, {L'ñ', L'n'}
    };

    // Înlocuirea caracterelor speciale
    std::wstring replaced = input;
    std::transform(replaced.begin(), replaced.end(), replaced.begin(), [&](wchar_t ch) {
        return replacements.count(ch) ? replacements[ch] : ch;
        });

    // Aplicarea escape-ului pentru RTF
    std::wostringstream escaped;
    for (wchar_t c : replaced) {
        if (c == L'\\' || c == L'{' || c == L'}') {
            escaped << L'\\' << c; // Escapare pentru RTF
        }
        else if (c > 127) { // Caractere non-ASCII
            escaped << L"\\u" << static_cast<int>(c) << L"?";
        }
        else {
            escaped << c;
        }
    }

    return escaped.str();
}

std::string getStrBetween(const std::string& input, char deschidere, char inchidere) {
    size_t start = input.find(deschidere);
    size_t end = input.find(inchidere, start);

    if (start != std::string::npos && end != std::string::npos && end > start) {
        return input.substr(start + 1, end - start - 1);
    }

    return "";
}

std::wstring getWstrBetween(const std::wstring& input, wchar_t deschidere, wchar_t inchidere) {
    size_t start = input.find(deschidere);
    size_t end = input.find(inchidere, start);

    if (start != std::wstring::npos && end != std::wstring::npos && end > start) {
        return input.substr(start + 1, end - start - 1);
    }

    return L"";
}


std::string sleft_of(const std::string& input, char delimiter) {
    size_t pos = input.find(delimiter);
    if (pos != std::string::npos) {
        return input.substr(0, pos);
    }
    return input; // Dacă nu găsește delimiterul, returnează tot stringul
}

std::wstring wleft_of(const std::wstring& input, wchar_t delimiter) {
    size_t pos = input.find(delimiter);
    if (pos != std::wstring::npos) {
        return input.substr(0, pos);
    }
    return input;
}


std::wstring wstrToLower(const std::wstring& str) {
    std::wstring result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::towlower);
    return result;
}

/*
std::wstring stripQuotes(const std::wstring& input) {
    if (input.length() >= 2 && input.front() == L'"' && input.back() == L'"') {
        return input.substr(1, input.length() - 2);
    }
    return input;
}
*/

std::wstring stripQuotes(const std::wstring& str) {
    if (str.size() >= 2 &&
        ((str.front() == L'\'' && str.back() == L'\'') ||
            (str.front() == L'\"' && str.back() == L'\"'))) {
        return str.substr(1, str.size() - 2);
    }
    return str;
}


std::string strExtractPrefix(const std::string& input, char c ) {
    size_t pos = input.find(c);
    if (pos != std::string::npos) {
        return input.substr(0, pos + 1); // include și paranteza
    }
    return ""; // dacă nu există paranteză, returnează șir gol
}

std::wstring to_wstring_precise(long double value, int precision ) {
    std::wostringstream woss;
    woss << std::fixed << std::setprecision(precision) << value;
    return woss.str();
}

std::wstring to_wstring_precise2(long double value, int precision) {
    long double factor = std::pow(10.0L, precision);
    value = std::round(value * factor) / factor;

    std::wostringstream woss;
    woss << std::fixed << std::setprecision(precision) << value;
    return woss.str();
}




std::wstring normalize_number(const std::wstring& ws, int precision) {
    try {
        long double value = std::stold(ws);  // acceptă și notația științifică
        std::wostringstream woss;
        woss << std::fixed << std::setprecision(precision) << value;
        return woss.str();
    }
    catch (...) {
        throw std::runtime_error("Normalizarea a eșuat: șir invalid.");
    }
}

bool is_plain_decimal(const std::wstring& ws) {
    // Expresie regulată pentru număr zecimal simplu (fără e+06 etc.)
    static const std::wregex pattern(LR"(^[+-]?\d+(\.\d+)?$)");
    return std::regex_match(ws, pattern);
}

std::wstring escape_commas(const std::wstring& input) {
    std::wstring output;
    for (wchar_t ch : input) {
        if (ch == L',') {
            output += L"\\,";  // adaugă backslash înainte de virgulă
        }
        else {
            output += ch;
        }
    }
    return output;
}

std::vector<std::wstring> splitCSVLine(const std::wstring& line, wchar_t delimiter ) {
    // std::wcout << "FAC SPLIT LA LINIA:" << line << std::endl;
    std::vector<std::wstring> result;
    std::wstring cell;
    bool insideQuotes = false;

    for (size_t i = 0; i < line.size(); ++i) {
        wchar_t ch = line[i];

        if (ch == L'"') {
            insideQuotes = !insideQuotes;
            //cell += ch;
        }
        else if (ch == delimiter && !insideQuotes) {
            result.push_back(wstr_trim(cell));
            cell.clear();
        }
        else {
            cell += ch;
        }
    }

    result.push_back(wstr_trim(rm_char(cell, ','))); // adaugă ultima celulă

    //for (auto aa : result) {  std::wcout << "LINE:" << line << "CELULE:" << aa << std::endl;   }

    return result;
}

bool contains_substring(const std::wstring& text, const std::wstring& pattern) {
    return text.find(pattern) != std::wstring::npos;
}

std::wstring sanitize_filename(const std::wstring& input) {
    // Lista caracterelor interzise în numele de fișiere Windows
    const std::wstring invalid_chars = L"\\/:*?\"<>|";

    std::wstring result = input;

    // Elimină fiecare caracter interzis
    result.erase(
        std::remove_if(result.begin(), result.end(),
            [&invalid_chars](wchar_t c) {
                return invalid_chars.find(c) != std::wstring::npos;
            }),
        result.end()
                );

    return result;
}



size_t count_char_in_wstring(const std::wstring& str, wchar_t ch) {
    return std::count(str.begin(), str.end(), ch);
}



std::string utf8_encode(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();

    int size_needed = WideCharToMultiByte(
        CP_UTF8, 0, wstr.c_str(), (int)wstr.size(),
        NULL, 0, NULL, NULL);

    std::string result(size_needed, 0);
    WideCharToMultiByte(
        CP_UTF8, 0, wstr.c_str(), (int)wstr.size(),
        &result[0], size_needed, NULL, NULL);

    return result;
}

std::string wstring_to_utf8(const std::wstring& wstr) {
    return utf8_encode(wstr);
}


std::wstring utf8_to_wstring(const std::string& str) {
    if (str.empty()) return std::wstring();

    // Calculează dimensiunea buffer-ului necesar pentru wstring
    int size_needed = MultiByteToWideChar(
        CP_UTF8, 0, str.c_str(), (int)str.size(),
        NULL, 0
    );

    // Creează un wstring cu dimensiunea necesară
    std::wstring result(size_needed, 0);

    // Efectuează conversia
    MultiByteToWideChar(
        CP_UTF8, 0, str.c_str(), (int)str.size(),
        &result[0], size_needed
    );

    return result;
}

std::wstring to_upper(const std::wstring& input) {
    std::wstring result = input;
    for (wchar_t& ch : result) {
        ch = std::towupper(ch);
    }
    return result;
}

std::wstring to_lower(const std::wstring& input) {
    std::wstring result = input;
    for (wchar_t& ch : result) {
        ch = std::towlower(ch);
    }
    return result;
}
/*
std::wstring trim_zeros(const std::wstring& input) {
    std::wstring s = input;

    // elimină zerourile de la final dacă există un punct
    while (s.find(L'.') != std::wstring::npos && !s.empty() && s.back() == L'0') {
        s.pop_back();
    }
    // dacă rămâne doar punctul la final, îl eliminăm
    if (!s.empty() && s.back() == L'.') {
        s.pop_back();
    }
    return s;
}
*/
std::wstring trim_zeros(const std::wstring& input, int max_dec ) {
    std::wstring s = input;

    size_t pos = s.find(L'.');
    if (pos == std::wstring::npos) {
        // nu există punct zecimal
        if (max_dec > 0) {
            s += L'.';
            s.append(max_dec, L'0');
        }
        return s;
    }

    // elimină zerourile de la sfârșitul părții zecimale
    while (!s.empty() && s.back() == L'0') {
        s.pop_back();
    }
    if (!s.empty() && s.back() == L'.') {
        s.pop_back();
    }

    if (max_dec > 0) {
        size_t decimals = (s.length() - pos - 1);
        if (decimals < (size_t)max_dec) {
            // completăm cu zerouri până la max_dec
            s.append(max_dec - decimals, L'0');
        }
        // dacă sunt mai multe zecimale decât max_dec, NU tăiem cifre semnificative,
        // ci lăsăm așa (pentru că vrei să păstrezi cifrele reale)
    }

    return s;
}

std::wstring convertCp1250ToWideChar(const std::string& input) {
    if (input.empty()) {
        return L"";
    }

    // Dimensiunea necesară în caractere wide (inclusiv terminatorul null)
    int sizeNeeded = MultiByteToWideChar(1250, 0, input.c_str(), (int)input.length(), NULL, 0);

    std::wstring output(sizeNeeded, 0);
    MultiByteToWideChar(1250, 0, input.c_str(), (int)input.length(), &output[0], sizeNeeded);

    // Eliminăm terminatorul null dacă a fost inclus în sizeNeeded
    if (sizeNeeded > 0 && output.back() == L'\0') {
        output.pop_back();
    }

    return output;
}

std::wstring convertSingleByteToWideChar(const std::string& input, unsigned int codePage) {
    if (input.empty()) return L"";

    // Lungimea este 1, deoarece procesăm un singur octet RTF
    const int len = 1;

    // Asigurați-vă că 'codePage' (1250) este folosit!
    int sizeNeeded = MultiByteToWideChar(
        codePage,
        0,
        input.c_str(),
        len, // FIXAT LA 1
        NULL,
        0
    );

    if (sizeNeeded == 0) {
        // Dacă GetLastError() == ERROR_NO_UNICODE_TRANSLATION, înseamnă că octetul nu e valid în CP.
        return L"";
    }

    std::wstring output(sizeNeeded, L'\0');
    MultiByteToWideChar(codePage, 0, input.c_str(), len, &output[0], sizeNeeded); // FIXAT LA 1

    // Eliminăm terminatorul null dacă sizeNeeded este mai mare de 1, deși pentru un singur octet nu ar trebui.
    if (output.length() > 0 && output.back() == L'\0') {
        output.pop_back();
    }

    return output;
}

/*
std::vector<std::wstring> split_to_words(const std::wstring& text) {
    std::vector<std::wstring> tokens;
    std::wstring currentWord;

    for (wchar_t ch : text) {
        if (ch == L'\n' || ch == L'\f' || ch == L'\t') { // ⭐ ADĂUGARE L'\f' AICI
            // 1. Dacă există un cuvânt acumulat, adaugă-l
            if (!currentWord.empty()) {
                tokens.push_back(currentWord);
                currentWord.clear();
            }
            // 2. Adaugă \n sau \f ca token separat
            tokens.push_back(std::wstring(1, ch)); // Creează un string de lungime 1
        }
        else if (iswspace(ch)) {
            // Dacă întâlnești orice alt spațiu alb (spațiu, tab, etc.):
            // Dacă există un cuvânt acumulat, adaugă-l.
            if (!currentWord.empty()) {
                tokens.push_back(currentWord);
                currentWord.clear();
            }
            // Spațiile albe simple nu sunt adăugate ca token-uri.
        }
        else {
            // Dacă este un caracter normal (non-spațiu, non-\n), adaugă-l la cuvântul curent.
            currentWord += ch;
        }
    }

    // 3. Adaugă ultimul cuvânt rămas (dacă nu s-a terminat cu un spațiu/newline)
    if (!currentWord.empty()) {
        tokens.push_back(currentWord);
    }

    return tokens;
}
*/

std::vector<std::wstring> split_to_words(const std::wstring& text) {
    std::vector<std::wstring> tokens;
    std::wstring currentWord;

    for (wchar_t ch : text) {
        // ⭐ 1. TRATAREA CARACTERELOR SPECIALE RTF (izolate ca token-uri)
        if (ch == L'\n' || ch == L'\f' || ch == L'\t') { // Adaugă L'\t' aici

            // a. Dacă există un cuvânt acumulat, adaugă-l
            if (!currentWord.empty()) {
                tokens.push_back(currentWord);
                currentWord.clear();
            }
            // b. Adaugă \n, \f sau \t ca token separat
            // Notă: În acest context, un token L"\t" (caracterul tabulator) 
            // este ceea ce processRtfParagraph așteaptă să proceseze
            // ca acțiune, nu ca text de randat.
            tokens.push_back(std::wstring(1, ch));
        }
        // ⭐ 2. TRATAREA ALTOR SPAȚII ALBE (separatori)
        else if (iswspace(ch)) {
            // Dacă întâlnești orice alt spațiu alb (DOAR spațiu simplu, fără \t, \n, \f):
            if (!currentWord.empty()) {
                tokens.push_back(currentWord);
                currentWord.clear();
            }
            // Spațiile albe simple sunt ignorate ca token-uri.
        }
        // 3. TRATAREA CARACTERELOR NORMALE
        else {
            currentWord += ch;
        }
    }

    // 4. Adaugă ultimul cuvânt rămas
    if (!currentWord.empty()) {
        tokens.push_back(currentWord);
    }

    return tokens;
}

std::wstring normalizeSpaces(const std::wstring& input) {
    if (input.empty()) return L"";

    std::wstring result;
    bool inQuotes = false;
    wchar_t quoteChar = 0;
    bool lastWasSpace = false;

    // 1. Trim de început
    size_t start = input.find_first_not_of(L" \t\r\n");
    if (start == std::wstring::npos) return L"";

    // 2. Trim de sfârșit (folosit pentru limitarea buclei)
    size_t end = input.find_last_not_of(L" \t\r\n");

    for (size_t i = start; i <= end; ++i) {
        wchar_t c = input[i];

        // Detecție ghilimele (suportă și " și ')
        if ((c == L'\"' || c == L'\'') && (i == 0 || input[i - 1] != L'\\')) {
            if (!inQuotes) {
                inQuotes = true;
                quoteChar = c;
            }
            else if (c == quoteChar) {
                inQuotes = false;
            }
            result += c;
            lastWasSpace = false;
        }
        // Dacă suntem în interiorul ghilimelelor, copiem totul așa cum este
        else if (inQuotes) {
            result += c;
            lastWasSpace = false;
        }
        // Dacă suntem în exterior, gestionăm spațiile multiple
        else {
            if (std::iswspace(c)) {
                if (!lastWasSpace) {
                    result += L' '; // Transformăm orice whitespace (\t, \r) în spațiu simplu
                    lastWasSpace = true;
                }
            }
            else {
                result += c;
                lastWasSpace = false;
            }
        }
    }
    return result;
}


std::wstring shellNormalizeSpaces(const std::wstring& input) {
    if (input.empty()) return L"";

    std::wstring result;
    result.reserve(input.size());

    bool inQuotes = false;
    wchar_t quoteChar = 0;
    bool lastWasSpace = false;

    // 1. Trim la început
    size_t start = input.find_first_not_of(L" \t\r\n");
    if (start == std::wstring::npos) return L"";

    // 2. Trim la sfârșit
    size_t end = input.find_last_not_of(L" \t\r\n");

    for (size_t i = start; i <= end; ++i) {
        wchar_t c = input[i];

        // Detectăm ghilimele doar dacă nu sunt escape
        bool isQuote = (c == L'\"' || c == L'\'');
        bool escaped = (i > start && input[i - 1] == L'\\');

        if (isQuote && !escaped) {
            if (!inQuotes) {
                inQuotes = true;
                quoteChar = c;
            }
            else if (c == quoteChar) {
                inQuotes = false;
            }

            result += c;
            lastWasSpace = false;
            continue;
        }

        // Dacă suntem în ghilimele, copiem totul
        if (inQuotes) {
            result += c;
            lastWasSpace = false;
            continue;
        }

        // În afara ghilimelelor, normalizăm whitespace
        if (iswspace(c)) {
            if (!lastWasSpace) {
                result += L' ';
                lastWasSpace = true;
            }
        }
        else {
            result += c;
            lastWasSpace = false;
        }
    }

    return result;
}


bool isNumber(const std::wstring& s) {
    if (s.empty()) return false;

    size_t pos = 0;

    try {
        std::stod(s, &pos);
    }
    catch (...) {
        return false;
    }

    return pos == s.size();
}

bool startsWith(const std::wstring& text, const std::wstring& prefix, bool ignoreCase) {
    if (prefix.size() > text.size()) return false;

    if (ignoreCase) {
        std::wstring textPart = text.substr(0, prefix.size());
        std::wstring upperText, upperPrefix;

        upperText.resize(textPart.size());
        upperPrefix.resize(prefix.size());

        std::transform(textPart.begin(), textPart.end(), upperText.begin(), ::towupper);
        std::transform(prefix.begin(), prefix.end(), upperPrefix.begin(), ::towupper);

        return upperText == upperPrefix;
    }

    return text.compare(0, prefix.size(), prefix) == 0;
}
