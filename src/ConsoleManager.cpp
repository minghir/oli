#include "ConsoleManager.hpp"
#include "StringUtils.hpp"

#include <codecvt>
#include <locale>
#include <filesystem>
#include <sstream>


ConsoleManager* ConsoleManager::s_instance = nullptr;

// --- Implementarea metodei initialize ---
void ConsoleManager::initialize() {
#ifdef _WIN32
	if (GetConsoleWindow() == NULL) { 
		return ;
	}

    AllocConsole();
    SetConsoleOutputCP(CP_UTF8);

    FILE* stream;
    freopen_s(&stream, "CONOUT$", "w", stdout);
    freopen_s(&stream, "CONOUT$", "w", stderr);
    freopen_s(&stream, "CONIN$", "r", stdin);

    std::ios::sync_with_stdio(true);
    _setmode(_fileno(stdout), _O_U8TEXT);

#else
    // Linux: nu ai nevoie de nimic special.mc
    // Terminalul suportă UTF-8 nativ.
    std::ios::sync_with_stdio(true);
#endif

}

void ConsoleManager::setColor(WORD color) {
#ifdef _WIN32
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
#else
    // Linux folosește ANSI escape codes
    switch (color) {
    case 1:  std::cout << "\033[34m"; break; // albastru
    case 2:  std::cout << "\033[32m"; break; // verde
    case 4:  std::cout << "\033[31m"; break; // roșu
    default: std::cout << "\033[0m";  break; // reset
    }
#endif
}

void ConsoleManager::resetColor() {
#ifdef _WIN32
    setColor(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
#else
    std::cout << "\033[0m";
#endif
}


// Funcție ajutătoare pentru a obține prefixul și culoarea
void ConsoleManager::log(const std::wstring& message, LogLevel level) {
    std::lock_guard<std::recursive_mutex> lock(mtxLog); // Decomentează pentru thread-safety

    if (level < minLevel)
        return;

    if (std::wcout.fail()) {
        std::wcout.clear(); // Resetează starea stream-ului dacă a "crăpat" anterior
    }

    std::wstring prefix = L"[LOG]";
    WORD color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE; // Alb (default)

    switch (level) {
    case LogLevel::INFO:
        //return;
        prefix = L"[INFO]";
        break;

    case LogLevel::SUCCESS:
        prefix = L"[SUCCESS]";
        color = FOREGROUND_GREEN;
        break;

    case LogLevel::WARNING:
        prefix = L"[WARNING]";
        color = FOREGROUND_RED | FOREGROUND_GREEN; // Galben
        break;

    case LogLevel::LOG_ERROR:
        prefix = L"[ERROR]";
        color = FOREGROUND_RED | FOREGROUND_INTENSITY;
        break;

    case LogLevel::FATAL_ERROR:
        prefix = L"[FATAL_ERROR]";
        color = BACKGROUND_RED | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY; // Roșu intens pe fond roșu
        break;

    case LogLevel::DEBUG:
       // return;
        prefix = L"[DEBUG]";
        color = FOREGROUND_BLUE | FOREGROUND_INTENSITY; // Albastru deschis
        break;
    }

    std::wstring timestamp = getTimestamp();

    // Aplică culoarea
    setColor(color);

    // Afișează mesajul
    std::wcout << prefix << L" " << message << std::endl;

    // Resetează culoarea la cea implicită
    resetColor();

    // 2. Pregătim și scriem mesajul pentru Fișier (UTF-8)
    if (logToFileEnabled && !fileLoggingMuted && logFile.is_open()) {
        std::wstring fullWideMessage = L"[" + timestamp + L"] " + prefix + L" " + message;

        // Folosim funcția ta salvatoare!
        std::string utf8Message = utf8_encode(fullWideMessage);

        logFile << utf8Message << std::endl;
        logFile.flush();
      
    }

    for (size_t i = 0; i < m_extraOutputs.size(); ++i) {
        if (m_extraOutputs[i]) {
            m_extraOutputs[i]->writeLog(message, level);
        }
    }
}




// --- Implementarea metodei logTest ---
void ConsoleManager::logTest() {
    std::wcout << L"[TEST] Verificare diacritice în consolă: ș ț ă â î" << std::endl;
    std::wcout << L"[TEST] Această linie ar trebui să apară albă." << std::endl;
    setColor(FOREGROUND_GREEN);
    std::wcout << L"[TEST] Această linie ar trebui să apară verde." << std::endl;
    setColor(FOREGROUND_RED);
    std::wcout << L"[TEST] Această linie ar trebui să apară roșie." << std::endl;
    setColor(FOREGROUND_BLUE);
    std::wcout << L"[TEST] Această linie ar trebui să apară albastră." << std::endl;
    resetColor();
    std::wcout << L"[TEST] Culoarea a fost resetată la alb." << std::endl;
}

// --- Implementarea metodei shutdown (Opțional) ---

void ConsoleManager::shutdown() {
    // Pentru a elibera consola, trebuie să redirecționezi stream-urile înapoi
    // sau să le închizi înainte de a apela FreeConsole().
    // Aceasta este o operație mai complexă și adesea nu este necesară
    // deoarece consola este închisă automat la terminarea procesului.
    // Dacă ai nevoie, caută exemple detaliate de FreeConsole() și redirecționare.
    // FreeConsole();
    log(L"Consola a fost închisă (dacă FreeConsole() a fost apelat).");
}

void ConsoleManager::writeRaw(const std::wstring& message, WORD color) {
    std::lock_guard<std::recursive_mutex> lock(mtxLog);

    // 1. Consolă
    if (color != 0) setColor(color);
    std::wcout << message << std::endl;
    if (color != 0) resetColor();

    // 2. Fișier (ADĂUGĂ ACEST BLOC)
    if (logToFileEnabled && !fileLoggingMuted && logFile.is_open()) {
        // La writeRaw probabil NU vrei timestamp sau prefixul [LOG], 
        // vrei doar instrucțiunea SQL pură.
        std::string utf8Message = utf8_encode(message);
        logFile << utf8Message << std::endl;
        logFile.flush();
    }

    for (size_t i = 0; i < m_extraOutputs.size(); ++i) {
        if (m_extraOutputs[i]) {
            m_extraOutputs[i]->writeLog(message,LogLevel::INFO);
        }
    }
}

void ConsoleManager::writePlain(const std::wstring& message, WORD color) {
    std::lock_guard<std::recursive_mutex> lock(mtxLog);

    // 1. Consolă - scoatem std::endl
    if (color != 0) setColor(color);
    std::wcout << message;
    std::wcout.flush(); // IMPORTANT: flush() asigură afișarea imediată fără newline
    if (color != 0) resetColor();

    // 2. Fișier - scoatem std::endl sau \n
    if (logToFileEnabled && !fileLoggingMuted && logFile.is_open()) {
        std::string utf8Message = utf8_encode(message);
        logFile << utf8Message;
        logFile.flush();
    }

    // 3. Extra Outputs
    for (size_t i = 0; i < m_extraOutputs.size(); ++i) {
        if (m_extraOutputs[i]) {
            m_extraOutputs[i]->writeLog(message, LogLevel::INFO);
        }
    }
}


/*
void ConsoleManager::clear() {
#ifdef _WIN32
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    COORD topLeft = { 0, 0 };

    if (!GetConsoleScreenBufferInfo(hConsole, &csbi)) return;

    DWORD dwConSize = csbi.dwSize.X * csbi.dwSize.Y;
    DWORD dwCharsWritten;

    FillConsoleOutputCharacter(hConsole, ' ', dwConSize, topLeft, &dwCharsWritten);
    FillConsoleOutputAttribute(hConsole, csbi.wAttributes, dwConSize, topLeft, &dwCharsWritten);
    SetConsoleCursorPosition(hConsole, topLeft);

#else
    // Linux: clear screen
    std::cout << "\033[2J\033[H";
#endif
}
*/
void ConsoleManager::clear() {
    std::lock_guard<std::recursive_mutex> lock(mtxLog);
    // Codul ANSI pentru "Clear Screen" (\033[2J) și "Move to 0,0" (\033[H)
    std::wcout << L"\033[2J\033[H";
    std::wcout.flush();
}


bool ConsoleManager::enableFileLogging(const std::wstring& filePath, bool append) {
    std::lock_guard<std::recursive_mutex> lock(mtxLog);


    if (logFile.is_open())closeLogFile();
    // Deschidem ca ofstream normal (fără imbue)
    // Convertim calea la string dacă e nevoie, sau folosim varianta wide pentru Windows
    std::string utf8Path = wstring_to_utf8(filePath);

    std::ios_base::openmode mode = std::ios::out | std::ios::binary;
    if (append) mode |= std::ios::app;
    else mode |= std::ios::trunc;

    //logFile.open(utf8Path, std::ios::out | std::ios::app);
    //logFile.open(filePath, mode);
    logFile.open(utf8Path, mode);
    if (logFile.is_open() && logFile.tellp() == 0) {
        // Scrie BOM-ul pentru UTF-8: EF BB BF
        logFile << "\xEF\xBB\xBF";
    }

    if (logFile.is_open()) {
        logToFileEnabled = true;
        log(L"Logarea în fișier a fost activată (UTF-8)", LogLevel::SUCCESS);
        return true;
    }
    return false;
}
                         
void ConsoleManager::closeLogFile() {
    // Punem lock și aici pentru siguranță dacă e apelată independent
    std::lock_guard<std::recursive_mutex> lock(mtxLog);
    if (logFile.is_open()) {
        logFile.close();
        logToFileEnabled = false;
    }
}

std::wstring ConsoleManager::getTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm bt;

#ifdef _WIN32
    localtime_s(&bt, &in_time_t);
#else
    localtime_r(&in_time_t, &bt);
#endif

    std::wstringstream ss;
    ss << std::put_time(&bt, L"%Y-%m-%d %H:%M:%S");
    return ss.str();
}

void ConsoleManager::addOutput(ILogOutput* output) {
    std::lock_guard<std::recursive_mutex> lock(mtxLog);
    if (output) {
        m_extraOutputs.push_back(output);
    }
}