#ifndef CONSOLE_MANAGER_HPP
#define CONSOLE_MANAGER_HPP

#pragma once

#include <windows.h> // Pentru HWND, WORD, GetStdHandle, SetConsoleTextAttribute, AllocConsole etc.
#include <iostream>  // Pentru std::wcout, std::endl
#include <fstream>   // Nu este direct folosit aici, dar poate fi necesar pentru alte tipuri de logare
#include <string>    // Pentru std::wstring
#include <vector>
#include <mutex>     // Pentru std::mutex (opțional, pentru thread-safety)
#include <chrono>
#include <iomanip>



enum class LogLevel {
    INFO,           // Informații generale (default)
    SUCCESS,        // Operație reușită (opțional, dar util)
    WARNING,        // Avertisment (nu e critic, dar necesită atenție)
    LOG_ERROR,      // Eroare (o problemă care nu oprește programul)
    FATAL_ERROR,    // Eroare critică (programul trebuie să se oprească sau este grav afectat)
    DEBUG           // Mesaje pentru depanare (folosite în timpul dezvoltării)
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
#define LOG_RAW(msg, color)    ConsoleManager::getInstance().writeRaw((msg), (color))




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


    bool m_isSuspended = false; // Flag pentru suspendarea TOTALĂ a logării

    // Lista de ferestre/canvas-uri care vor loguri
    std::vector<ILogOutput*> m_extraOutputs;
    

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

    bool enableFileLogging(const std::wstring& filePath);
    void closeLogFile();
    std::wstring getTimestamp(); // Funcție utilă pentru loguri

    // Suspendă/Reia TOATE logurile (Consolă + Fișier + UI)
    void suspendLogging() { m_isSuspended = true; }
    void resumeLogging() { m_isSuspended = false; }
    bool isSuspended() const { return m_isSuspended; }

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

private:
    // Mutex (dacă e necesar, scoate comentariul și pune-l la începutul clasei)
     //std::mutex mtxLog; 
    
};



#endif // CONSOLE_MANAGER_HPP