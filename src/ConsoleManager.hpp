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

#define LOG_INFO(msg)        ConsoleManager::getInstance().log((msg),LogLevel::INFO)
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
    ConsoleManager() = default; 
    ConsoleManager(const ConsoleManager&) = delete;
    ConsoleManager& operator=(const ConsoleManager&) = delete;

    ~ConsoleManager() { closeLogFile(); } 

    std::ofstream logFile; 
    bool logToFileEnabled = false;
    bool fileLoggingMuted = false; 

    std::vector<ILogOutput*> m_extraOutputs;
    LogLevel minLevel = LogLevel::INFO;

    // 🔥 MODIFICARE: Pointerul static de instanță
    static ConsoleManager* s_instance;

public:
    // 🔥 MODIFICARE: Punctul de acces Singleton bazat pe pointer
    static ConsoleManager& getInstance() {
        if (!s_instance) {
            s_instance = new ConsoleManager();
        }
        return *s_instance;
    }

    // 🔥 MODIFICARE: Funcția prin care DLL-ul va adopta instanța din EXE
    static void setInstance(ConsoleManager* hostInstance) {
        s_instance = hostInstance;
    }

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
    std::wstring getTimestamp(); 
    void suspendFileLogging() { fileLoggingMuted = true; }
    void resumeFileLogging() { fileLoggingMuted = false; }

    void addOutput(ILogOutput* output);
    void removeExtraOutput(ILogOutput* output) {
        std::lock_guard<std::recursive_mutex> lock(mtxLog);
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
};

#endif // CONSOLE_MANAGER_HPP