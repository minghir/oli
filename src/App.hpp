#ifndef APP_HPP
#define APP_HPP

#include "ConsoleManager.hpp" // Pentru funcționalitățile de logare
#include <string>    // Pentru std::string


enum class RunMode { CONSOLE };

class App {
    
public:
    // Constructorul inițializează aplicația cu handle-ul instanței.
    // Setează pointerul static s_instance pentru delegarea globală a mesajelor.
    explicit App(RunMode mode = RunMode::CONSOLE);

    // Destructorul implicit este suficient, deoarece unique_ptr gestionează memoria.
    // virtual este o bună practică pentru clasele de bază.
    virtual ~App() = default;

    // Bucla principală a aplicației.
    // Returnează codul de ieșire al aplicației.
    int run();
    bool init();
    virtual bool initConsole() { return true; }
    static App* getAppInstance() { return s_instance; }
    // Oprește aplicația, efectuând curățenia necesară.
    void shutdown();

    //Porneste consola
    void startConsole();

protected:
    void setRunMode(RunMode mode) { m_runMode = mode; }

private:
    static App* s_instance;

    

    RunMode m_runMode = RunMode::CONSOLE;
};

#endif // APP_HPP