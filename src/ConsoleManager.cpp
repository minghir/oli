#include "ConsoleManager.hpp"
#include "stringUtils.hpp"
#include <fcntl.h> // Pentru _O_U8TEXT

#include <io.h>    // Pentru _fileno
#include <codecvt>
#include <locale>
#include <filesystem>
#include <sstream>



// --- Implementarea metodei initialize ---
void ConsoleManager::initialize() {
    // Alocă o nouă consolă pentru procesul curent.
    // Necesare pentru ca o aplicație GUI să aibă o consolă vizibilă.
    AllocConsole();

    // Setează codificarea de ieșire a consolei la UTF-8.
    // Crucial pentru afișarea corectă a caracterelor Unicode și diacriticelor.
    SetConsoleOutputCP(CP_UTF8);

    // Redirecționează stream-urile standard C (`stdout`, `stderr`, `stdin`) către noua consolă.
    // Fără aceste apeluri, std::cout/cerr/cin nu ar funcționa în consola nouă.
    FILE* stream;
    freopen_s(&stream, "CONOUT$", "w", stdout); // Ieșire standard
    freopen_s(&stream, "CONOUT$", "w", stderr); // Ieșire erori
    freopen_s(&stream, "CONIN$", "r", stdin);   // Intrare standard

    // Sincronizează stream-urile C++ cu stream-urile C.
    // Aceasta asigură că funcțiile `freopen_s` afectează și `std::wcout`/`std::cout`.
    std::ios::sync_with_stdio(true);

    // Setează modul pentru `stdout` la UTF-8 text.
    // Important pentru ca `std::wcout` să afișeze corect caracterele late (wide characters).
    _setmode(_fileno(stdout), _O_U8TEXT);

    // Curăță orice erori de stare anterioare ale stream-urilor C++.
    std::cout.clear();
    std::cerr.clear();

    // Inhiba pierderea focusului ferestreu orincipale
    
    //HANDLE hInput = GetStdHandle(STD_INPUT_HANDLE);
    //DWORD dwMode;
    //if (GetConsoleMode(hInput, &dwMode)) {
        //dwMode &= ~(ENABLE_QUICK_EDIT_MODE);
        //SetConsoleMode(hInput, dwMode);
    //}
    
    // Rulează un test rapid pentru a verifica funcționalitatea logării și a diacriticelor.
    //logTest();
    //log(L"Consola a fost inițializată cu succes și este gata de utilizare.");
}


/*
void ConsoleManager::initialize() {
    // 1. Încercăm atașarea sau alocarea
    if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
        AllocConsole();
    }

    // 2. Redirecționăm stream-urile standard
    FILE* fDummy;
    freopen_s(&fDummy, "CONOUT$", "w", stdout);
    freopen_s(&fDummy, "CONOUT$", "w", stderr);
    freopen_s(&fDummy, "CONIN$", "r", stdin);

    // 3. Sincronizăm stream-urile C++
    std::ios::sync_with_stdio(true);

    // 4. SOLUȚIA PENTRU LITERE SPAȚIATE:
    // Folosim U16TEXT pentru wcout. NU mai apela SetConsoleOutputCP aici.
    _setmode(_fileno(stdout), _O_U16TEXT);
    _setmode(_fileno(stderr), _O_U16TEXT);

    std::wcout.clear();

    // Testăm imediat
    logTest();
    log(L"Consola a fost inițializată în mod Unicode Nativ.");
}
*/
// --- Implementarea metodei setColor ---
void ConsoleManager::setColor(WORD color) {
    // Obține handle-ul de ieșire al consolei și setează atributele de culoare.
    // Dacă ai activat mutex-ul pentru thread-safety, blochează-l aici.
    // std::lock_guard<std::mutex> lock(mtxLog);
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

// --- Implementarea metodei resetColor ---
void ConsoleManager::resetColor() {
    // Resetează culoarea la alb standard (combinația de roșu, verde, albastru).
    setColor(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
}

/*
// --- Implementarea metodei log ---
void ConsoleManager::getInstance().log(const std::wstring& message) {
    // Pentru thread-safety, decomentează linia de mai jos:
    // std::lock_guard<std::mutex> lock(mtxLog);
    //if (message.find(L"ERROR") == std::wstring::npos) return;

    std::wcout << L"[LOG] " << message << std::endl;

}
*/

// Funcție ajutătoare pentru a obține prefixul și culoarea
void ConsoleManager::log(const std::wstring& message, LogLevel level) {
    std::lock_guard<std::recursive_mutex> lock(mtxLog); // Decomentează pentru thread-safety

    if (std::wcout.fail()) {
        std::wcout.clear(); // Resetează starea stream-ului dacă a "crăpat" anterior
    }

    std::wstring prefix = L"[LOG]";
    WORD color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE; // Alb (default)
    bool isError = false; // Flag pentru a ști dacă trebuie să trimitem la stderr sau să folosim log-ul de erori

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
        isError = true;
        break;

    case LogLevel::FATAL_ERROR:
        prefix = L"[FATAL_ERROR]";
        color = BACKGROUND_RED | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY; // Roșu intens pe fond roșu
        isError = true;
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

/*
void ConsoleManager::writeRaw(const std::wstring& message, WORD color) {
    std::lock_guard<std::recursive_mutex> lock(mtxLog);
    if (color != 0) setColor(color);
    std::wcout << message << std::endl;
    if (color != 0) resetColor();
}
*/
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

void ConsoleManager::clear() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    COORD topLeft = { 0, 0 };

    // 1. Obținem dimensiunile curente ale buffer-ului consolei
    if (!GetConsoleScreenBufferInfo(hConsole, &csbi)) {
        return;
    }

    DWORD dwConSize = csbi.dwSize.X * csbi.dwSize.Y;
    DWORD dwCharsWritten;

    // 2. Umplem tot ecranul cu spații (' ')
    FillConsoleOutputCharacter(hConsole, (TCHAR)' ', dwConSize, topLeft, &dwCharsWritten);

    // 3. Resetăm atributele de culoare pentru tot ecranul
    FillConsoleOutputAttribute(hConsole, csbi.wAttributes, dwConSize, topLeft, &dwCharsWritten);

    // 4. Mutăm cursorul înapoi în colțul din stânga-sus
    SetConsoleCursorPosition(hConsole, topLeft);


}


bool ConsoleManager::enableFileLogging(const std::wstring& filePath) {
    std::lock_guard<std::recursive_mutex> lock(mtxLog);


    if (logFile.is_open())closeLogFile();
    // Deschidem ca ofstream normal (fără imbue)
    // Convertim calea la string dacă e nevoie, sau folosim varianta wide pentru Windows
    std::string utf8Path = wstring_to_utf8(filePath);
    logFile.open(utf8Path, std::ios::out | std::ios::app);

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
    localtime_s(&bt, &in_time_t);

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