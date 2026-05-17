#ifndef CONSOLE_MANAGER_HPP
#define CONSOLE_MANAGER_HPP

#pragma once

#ifdef _WIN32
#include <windows.h> // Pentru HWND, WORD, GetStdHandle, SetConsoleTextAttribute, AllocConsole etc.
#include <fcntl.h> // Pentru _O_U8TEXT
#include <io.h>    // Pentru _fileno
#else 
using WORD = unsigned short;
// Echivalente ANSI pentru Linux
#define FOREGROUND_BLUE   0x01
#define FOREGROUND_GREEN  0x02
#define FOREGROUND_RED    0x04
#define FOREGROUND_INTENSITY 0x08
#define BACKGROUND_RED    0x10
#endif


#include <iostream>  // Pentru std::wcout, std::endl
#include <fstream>   // Nu este direct folosit aici, dar poate fi necesar pentru alte tipuri de logare
#include <string>    // Pentru std::wstring
#include <vector>
#include <mutex>     // Pentru std::mutex (opțional, pentru thread-safety)
#include <algorithm>
#include <chrono>
#include <iomanip>



enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    SUCCESS = 2,
    WARNING = 3,
    LOG_ERROR = 4,
    FATAL_ERROR = 5
};

class ILogOutput {
public:
    virtual ~ILogOutput() = default;
    virtual void writeLog(const std::wstring& message, LogLevel level) = 0;
};

#define LOG_INFO(msg)         ConsoleManager::getInstance().log((msg),LogLevel::INFO)
#define LOG_SUCCESS(msg)      ConsoleManager::getInstance().log((msg),LogLevel::SUCCESS)
#define LOG_WARNING(msg)      ConsoleManager::getInstance().log((msg),LogLevel::WARNING)
#define LOG_ERROR(msg)        ConsoleManager::getInstance().log((msg),LogLevel::LOG_ERROR)
#define LOG_FATAL(msg)        ConsoleManager::getInstance().log((msg),LogLevel::FATAL_ERROR)
#define LOG_DEBUG(msg)        ConsoleManager::getInstance().log((msg),LogLevel::DEBUG)
#define LOG(msg)    ConsoleManager::getInstance().log((msg), LogLevel::INFO) 
#define LOG_RAW(msg)    ConsoleManager::getInstance().writeRaw((msg)) 




class ConsoleManager {
private:
    std::recursive_mutex mtxLog;
    // Păstrează constructorul privat, dar șterge:
    // ConsoleManager() = delete;
    ConsoleManager() = default; // Acum este necesară o implementare
    ConsoleManager(const ConsoleManager&) = delete;
    ConsoleManager& operator=(const ConsoleManager&) = delete;

    ~ConsoleManager() { closeLogFile(); } // Închidem fișierul la distrugere

    std::ofstream logFile; // Stream-ul pentru fișier
    bool logToFileEnabled = false;
    bool fileLoggingMuted = false; // "Switch-ul" tău pentru silențiozitate

    // Lista de ferestre/canvas-uri care vor loguri
    std::vector<ILogOutput*> m_extraOutputs;
    
    LogLevel minLevel = LogLevel::INFO;

public:

    

    // METODA SINGLETON: Acesta este noul tău punct de acces
    static ConsoleManager& getInstance() {
        // Creează instanța la prima utilizare (thread-safe din C++11 încoace)
        static ConsoleManager instance;
        return instance;
    }

    // Toate metodele publice devin non-statice!
    void initialize();
    void setColor(WORD color);
    void resetColor();
    void log(const std::wstring& message, LogLevel level = LogLevel::INFO);
    void logTest();
    void shutdown();
    void clear();
    void writeRaw(const std::wstring& message, WORD color = 0);
    void writePlain(const std::wstring& message, WORD color = 0);

    bool enableFileLogging(const std::wstring& filePath, bool append = true);
    void closeLogFile();
    std::wstring getTimestamp(); // Funcție utilă pentru loguri
    void suspendFileLogging() { fileLoggingMuted = true; }
    void resumeFileLogging() { fileLoggingMuted = false; }

    void addOutput(ILogOutput* output);
    void removeExtraOutput(ILogOutput* output) {
        std::lock_guard<std::recursive_mutex> lock(mtxLog);
        // Folosim idiomul erase-remove pentru a scoate pointerul din vector
        m_extraOutputs.erase(
            std::remove(m_extraOutputs.begin(), m_extraOutputs.end(), output),
            m_extraOutputs.end()
        );
    }
    void clearExtraOutputs() {
        std::lock_guard<std::recursive_mutex> lock(mtxLog);
        m_extraOutputs.clear();
    }

    void setMinLogLevel(LogLevel lvl) {
        minLevel = lvl;
    }

    LogLevel getLogLevel() const {
        return minLevel;
    }

private:
    // Mutex (dacă e necesar, scoate comentariul și pune-l la începutul clasei)
     //std::mutex mtxLog; 
    
};



#endif // CONSOLE_MANAGER_HPP