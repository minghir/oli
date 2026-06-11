
#include <filesystem>
#include <fstream>
#include <vector>
#include <string>
#include <locale>
#include <codecvt>
#include <fstream>
#include <iostream>
#include <random>


#if defined(_WIN32) || defined(_WIN64)  
#include <windows.h>
#endif

//#include "globals.hpp"
#include "fileUtils.hpp"
#include "../../StringUtils.hpp"
#include "../../ConsoleManager.hpp" // Adjust the path

//#include <sys/types.h>
//#include <sched.h>
//#include <exception>



    namespace fs = std::filesystem;

    //std::string csv_directory_path = ".";
    //std::string pdf_directory_path = ".";
    //std::string reports_directory_path = ".";

    std::map<std::wstring, std::wstring> cfg_file_vars;

    void create_dir_if_missing(const std::string& path) {
        //std::cout << "Verific directorul: " << path << std::endl;
        ConsoleManager::getInstance().log(L"[LOG] create_dir_if_missing: Verific directorul: " + str_to_wstr(path));

        if (path.empty()) {
            ConsoleManager::getInstance().log(L"[LOG] create_dir_if_missing: Path-ul este gol. Nu se poate crea directorul.");
            //std::cerr << "Path-ul este gol. Nu se poate crea directorul.\n";
            return;
        }

        try {
            if (!fs::exists(path)) {
                if (fs::create_directory(path)) {
                    //std::cout << "Directorul \"" << path << "\" a fost creat cu succes.\n";
                    ConsoleManager::getInstance().log(L"[LOG] create_dir_if_missing: Directorul \"" + str_to_wstr(path) + L"\" a fost creat cu succes.");
                }
                else {
                    std::cerr << "Eroare la crearea directorului \"" << path << "\".\n";
                }
            }
            else {
                //std::cout << "Directorul \"" << path << "\" exista deja.\n";
                ConsoleManager::getInstance().log(L"[LOG] create_dir_if_missing: Directorul \"" + str_to_wstr(path) + L"\" exista deja.");
            }
        }
        catch (const fs::filesystem_error& e) {
            std::cerr << "Exceptie: " << e.what() << "\n";
        }
    }




/*
    std::wstring wstr_read_RTF_file(const std::string& filePath) {
        std::string path = getGlobalReportPath() + filePath;
        //std::string path =  filePath;

        //MessageBox(NULL, str_to_wstr(path).c_str(), L"Întrebare", MB_YESNO | MB_ICONQUESTION);


        std::wifstream file(path, std::ios::in);
        if (!file.is_open()) {
            std::wcerr << L"Eroare: Nu s-a putut deschide fisierul " << str_to_wstr(filePath) << std::endl;
            return L""; // Returnează un string gol dacă fișierul nu se deschide
        }

        std::wstring content, line;
        while (std::getline(file, line)) {
            content += line + L"\n"; // Adaugă fiecare linie în conținut
        }

        file.close();
        return content;
    }
*/
    /*
    void convertRTFtoPDF(const std::string& rtfFile, const std::string& pdfDir) {
        std::string command = "LibreOfficePortable\\App\\libreoffice\\program\\soffice.exe --headless --convert-to pdf \""
                              + rtfFile + "\" --outdir \"" + pdfDir + "\"";
        std::system(command.c_str());
    }
    */

    std::string sanitizePath(const std::string& path) {
        if (!path.empty() && path.back() == '\\') {
            return path.substr(0, path.size() - 1);
        }
        return path;
    }

/*
    void convertRTFtoPDF(const std::string& rtfFile, std::string& pdfDir) {

        pdfDir = sanitizePath(pdfDir);

        if (!directoryExists(str_to_wstr(pdfDir))) {
            std::cerr << "Directorul: " << pdfDir << " pentru conversie nu exista!!!" << std::endl;
            return;
        }

        if (!fileExists(str_to_wstr(rtfFile))) {
            std::cerr << "Fisierul rtf: " << rtfFile << " nu exista!!!" << std::endl;
            return;
        }

        std::string command = getGlobalReportPath() + "LibreOfficePortable\\App\\libreoffice\\program\\soffice.exe --headless --convert-to pdf \""
            + rtfFile + "\" --outdir \"" + pdfDir + "\"";
        std::cout << "Execut:" << command << std::endl;
        int exitCode = std::system(command.c_str());

        if (exitCode == 0) {
            std::cout << "Conversia RTF -> PDF s-a terminat cu succes!\n";
        }
        else {
            std::cerr << "Eroare la conversie! Cod de ieșire: " << exitCode << "\n";
        }
    }


    void wconvertRTFtoPDF(const std::string& rtfFile, const std::string& pdfDir) {
        std::string apdfDir = sanitizePath(pdfDir);

        if (!directoryExists(str_to_wstr(apdfDir))) {
            std::cerr << "Directorul: " << apdfDir << " pentru conversie nu exista!!!" << std::endl;
            return;
        }

        if (!fileExists(str_to_wstr(rtfFile))) {
            std::cerr << "Fisierul rtf: " << rtfFile << " nu exista!!!" << std::endl;
            return;
        }

        std::string command = getGlobalReportPath() + "LibreOfficePortable\\App\\libreoffice\\program\\soffice.exe --headless --convert-to pdf \""
            + rtfFile + "\" --outdir \"" + apdfDir + "\"";
        //std::cout << "EXECUT:" << command << std::endl;
        ConsoleManager::getInstance().log(L"[LOG] wconvertRTFtoPDF: EXECUT:" + str_to_wstr(command));
        std::wstring wcommand = str_to_wstr(command);

        STARTUPINFOW si = { sizeof(STARTUPINFOW) };
        si.cb = sizeof(si); // Trebuie setat explicit

        PROCESS_INFORMATION pi = {};


        si.cb = sizeof(si);

        if (CreateProcessW(nullptr, &wcommand[0], nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
            WaitForSingleObject(pi.hProcess, INFINITE);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            //std::cout << "SUCCES:" << command << std::endl;
            ConsoleManager::getInstance().log(L"[LOG] wconvertRTFtoPDF: SUCCES:" + str_to_wstr(command));
        }
        else {
            //std::cout << "EROARE:" << command << std::endl;
            ConsoleManager::getInstance().log(L"[ERROR] wconvertRTFtoPDF: EROARE:" + str_to_wstr(command));
            //MessageBoxW(nullptr, L"Eroare la rularea LibreOffice!", L"Eroare", MB_OK | MB_ICONERROR);
        }

    }


    void initLibreOffice() {
        std::string command = getGlobalReportPath() + "LibreOfficePortable\\App\\libreoffice\\program\\soffice --headless --accept=\"socket,host=localhost,port=8100;urp;\" &";

        std::wstring wcommand = str_to_wstr(command);

        STARTUPINFOW si = { sizeof(STARTUPINFOW) };
        si.cb = sizeof(si); // Trebuie setat explicit

        PROCESS_INFORMATION pi = {};


        si.cb = sizeof(si);

        if (CreateProcessW(nullptr, &wcommand[0], nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
            WaitForSingleObject(pi.hProcess, INFINITE);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
        else {
            MessageBoxW(nullptr, L"Eroare la rularea LibreOffice!", L"Eroare", MB_OK | MB_ICONERROR);
        }
    }
*/


    bool copyFile(const std::string& source, const std::string& destination) {
        try {
            fs::copy_file(source, destination, fs::copy_options::overwrite_existing);
            return true;  // Copiere reușită
        }
        catch (const std::exception& e) {
            std::cerr << "Eroare la copiere: " << e.what() << std::endl;
            return false;  // Copiere eșuată
        }
    }

    bool removeFile(const std::string& filePath) {
        std::filesystem::path path(filePath);
        return std::filesystem::remove(path);
    }

    std::string getCurrentDirectory() {
        return std::filesystem::current_path().string();
    }


    #include <string>
#include <locale>
#include <codecvt>

std::wstring utf8ToWstring(const std::string& utf8)
{
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
    return conv.from_bytes(utf8);
}

std::string wstringToUtf8(const std::wstring& wstr)
{
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
    return conv.to_bytes(wstr);
}


    // Funcție pentru citirea unui fișier CSV în std::vector<std::wstring>
    std::vector<std::wstring> readCSVFile(const std::string& filename) {
        std::vector<std::wstring> lines;
        std::ifstream file(filename);

        if (!file) {
            std::cerr << "Eroare la deschiderea fișierului: " << filename << std::endl;
            return lines;
        }

        std::string line;
        while (std::getline(file, line)) {
            // line = rm_char(line, '/"');
            lines.push_back(processStringForRTF(utf8ToWstring(line)));  // Convertim fiecare linie la std::wstring
          //  std::wcout << utf8ToWstring(line) << std::endl;
        }

        std::wcout << L"Am citit: " << lines.size() << std::endl;
        file.close();
        std::wcout << L"Am inchis fisierul cu: " << lines.size() << std::endl;

        return lines;
    }

    std::vector<std::wstring> readCSVFile2(const std::string& filename) {
        std::vector<std::wstring> lines;
        std::ifstream file(filename);

        if (!file) {
            std::cerr << "Eroare la deschiderea fisierului: " << filename << std::endl;
            return lines;
        }

        std::string line;
        while (std::getline(file, line)) {
            // NU eliminăm ghilimelele — sunt esențiale pentru parsarea corectă a valorilor cu virgulă
            std::wstring wline = utf8ToWstring(line);

            // Dacă ai nevoie de procesare suplimentară (ex: pentru afișare RTF), o poți aplica aici
            wline = processStringForRTF(wline);
            //std::wcout << "INCARC LINIA::::" << wline << std::endl;
            lines.push_back(wline);
        }

        std::wcout << L"Am citit: " << lines.size() << L" linii din fisier." << std::endl;
        file.close();
        std::wcout << L"AM INCHIS FISIERUL" << std::endl;
        return lines;
    }




    std::vector<std::string> getCsvFiles(const std::string& folderPath) {
        std::vector<std::string> csvFiles;
        try {
            for (const auto& entry : fs::directory_iterator(folderPath)) {
                if (entry.is_regular_file()) {
                    if (entry.path().extension() == ".csv") {
                        csvFiles.push_back(entry.path().filename().string());
                    }
                }
            }
        }
        catch (const fs::filesystem_error& e) {
            std::cerr << "Eroare la accesarea directorului: " << e.what() << '\n';
        }


        return csvFiles;
    }

    bool load_options_file(std::string cfg_filepath) {
        std::ifstream fin(cfg_filepath);
        if (!fin) {
            std::cerr << "FILE ERROR: " << cfg_filepath << std::endl;
            return false;
        }
        std::string tmp;
        std::vector <std::string> vtmp;
        while (getline(fin, tmp)) {

            if (!tmp.empty()) {
                if (tmp[0] == '#') {
                    std::cerr << tmp << std::endl;
                    continue;
                }
            }
            else {
                continue;
            }

            //vtmp = explode(tmp, '=');
            vtmp = explode(rm_char(tmp, '\"'), '=');

            //if (vtmp[0] == "csv_directory_path") csv_directory_path = vtmp[1];
            //if (vtmp[0] == "pdf_directory_path") pdf_directory_path = vtmp[1];
            //if (vtmp[0] == "reports_directory_path") reports_directory_path = vtmp[1];
            if (vtmp[0].size() > 0 && vtmp[1].size() > 0)
                cfg_file_vars[str_to_wstr(vtmp[0])] = str_to_wstr(vtmp[1]);


        }
        fin.close();
        return true;
    }


    bool populate_config_map_from_file(const std::string& cfg_filepath, std::map<std::wstring, std::wstring>& out_map) {
        out_map.clear();
        std::ifstream fin(cfg_filepath);
        if (!fin) {
            std::cerr << "FILE ERROR: " << cfg_filepath << std::endl;
            return false;
        }

        std::string tmp;
        while (getline(fin, tmp)) {
            if (tmp.empty()) continue;
            if (tmp[0] == '#') {
                std::cerr << tmp << std::endl;
                continue;
            }

            std::vector<std::string> vtmp = explode(rm_char(tmp, '\"'), '=');
            if (vtmp.size() >= 2 && !vtmp[0].empty() && !vtmp[1].empty()) {
                out_map[str_to_wstr(vtmp[0])] = str_to_wstr(vtmp[1]);
            }
        }

        fin.close();
        return true;
    }


    std::string getFilenameWithoutExtension(const std::string& filepath) {
        std::filesystem::path p(filepath);
        return p.stem().string(); // .stem() returnează numele fără extensie
    }

    std::wstring getFilenameWithoutExtension(const std::wstring& filepath) {
        std::filesystem::path p(filepath);
        return p.stem().wstring(); // .stem() = nume fără extensie
    }



    bool fileExists(const std::wstring& path) {
        return std::filesystem::exists(path);
    }

    bool directoryExists(const std::wstring& path) {
        return std::filesystem::exists(path) && std::filesystem::is_directory(path);
    }


    std::vector<std::wstring> getFilesWithExtension(const std::wstring& directory, const std::wstring& extension) {
        std::vector<std::wstring> matchingFiles;
        std::wcout << "CAUT " << extension << " IN:" << directory << std::endl;
        try {
            for (const auto& entry : fs::directory_iterator(directory)) {
                if (entry.is_regular_file() && entry.path().extension() == extension) {
                    matchingFiles.push_back(entry.path().wstring());
                }
            }
        }
        catch (const fs::filesystem_error& e) {
            std::wcerr << L"Eroare la accesarea directorului: " << e.what() << std::endl;
        }

        return matchingFiles;
    }

    std::wstring get_file_name(const std::wstring& path) {
        size_t pos = path.find_last_of(L"\\/");
        if (pos != std::wstring::npos) {
            return path.substr(pos + 1);
        }
        return path; // Dacă nu există separator, presupunem că e deja numele fișierului
    }

    bool delete_files_by_extension(const std::wstring& dir_path, const std::wstring& extensie) {
        namespace fs = std::filesystem;
        bool sters = false;

        try {
            for (const auto& entry : fs::directory_iterator(dir_path)) {
                if (entry.is_regular_file()) {
                    if (entry.path().extension() == extensie) {
                        fs::remove(entry.path());
                        std::wcout << L"Sters: " << entry.path().filename() << std::endl;
                        sters = true;
                    }
                }
            }
        }
        catch (const std::exception& e) {
            std::wcerr << L"Eroare: " << e.what() << std::endl;
            return false;
        }

        return sters;
    }

    bool renameFile(const std::string& oldName, const std::string& newName) {
        if (std::rename(oldName.c_str(), newName.c_str()) != 0) {
            std::cerr << "Eroare la redenumire: " << oldName << " → " << newName << std::endl;
            return false;
        }
        return true;
    }

#include <filesystem>
#include <iostream>

bool wrenameFile(const std::wstring& oldName, const std::wstring& newName) {
    try {
        std::filesystem::rename(oldName, newName);
        return true;
    }
    catch (const std::exception& e) {
        std::wcerr << L"Eroare la redenumirea fișierului: "
                   << oldName << L" → " << newName
                   << L" | Detalii: " << str_to_wstr(e.what()) << std::endl;
        return false;
    }
}



    bool wcopyFile(const std::wstring& source, const std::wstring& destination) {
        try {
            fs::copy_file(source, destination, fs::copy_options::overwrite_existing);
            std::wcerr << L"Am copiat: " << source << " in " << destination << std::endl;
            return true;  // Copiere reușită
        }
        catch (const fs::filesystem_error& e) {
            std::wcerr << L"Eroare la copiere: " << e.what() << std::endl;
            return false;  // Copiere eșuată
        }
    }

    int numaraFisiereCuExtensie(const std::string& caleDirector, const std::string& extensie) {
        int count = 0;

        try {
            for (const auto& entry : fs::directory_iterator(caleDirector)) {
                if (entry.is_regular_file()) {
                    if (entry.path().extension() == extensie) {
                        ++count;
                    }
                }
            }
        }
        catch (const fs::filesystem_error& e) {
            std::cerr << "Eroare la accesarea directorului: " << e.what() << std::endl;
        }

        return count;
    }

    std::wstring getTempPath() {
        try {
            std::filesystem::path temp = std::filesystem::temp_directory_path();
            return temp.wstring();
        }
        catch (...) {
            return L"";
        }
    }



std::wstring getUniqueTempFilePath(const std::wstring& tempDir, const std::wstring& prefix)
{
    // 1. Directorul temporar (dacă e gol, folosim temp_directory_path)
    std::filesystem::path dir = tempDir.empty()
        ? std::filesystem::temp_directory_path()
        : std::filesystem::path(tempDir);

    // 2. Generator random pentru nume unic
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dist;

    uint64_t unique = dist(gen);

    // 3. Construim numele fișierului
    std::wstring filename = prefix + std::to_wstring(unique) + L".tmp";

    std::filesystem::path fullPath = dir / filename;

    return fullPath.wstring();
}


    std::wstring ensureExtension(const std::wstring& name, const std::wstring& extension) {

        if (name.size() >= extension.size() &&
            name.compare(name.size() - extension.size(), extension.size(), extension) == 0) {
            return name; // deja se termină în .dbf
        }

        return name + extension; // adaugă extensia
    }

