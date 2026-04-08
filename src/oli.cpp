#include "vShell.hpp"
#include "OliEngine.hpp"
#include "vApp.hpp"


#include<iostream>
#include <sstream>
#include <filesystem>
#include <string>
#include <chrono>
#include <iomanip>

class oli : public vApp {
public:
    oli( RunMode rm) :vApp() { setRunMode(rm); };
    ~oli() {};

    bool initConsole() override {
        vOliEngine shClient;
        vShell shell(shClient);
        shell.run();
        return true;
    }

};

int main(int argc, char* argv[]) {
    oli app(RunMode::CONSOLE);
    app.startConsole();
    return app.run();
}