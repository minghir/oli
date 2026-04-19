#include "Shell.hpp"
#include "OliEngine.hpp"
#include "App.hpp"


#include<iostream>
#include <sstream>
#include <filesystem>
#include <string>
#include <chrono>
#include <iomanip>

class oli : public App {
public:
    oli( RunMode rm) :App() { setRunMode(rm); };
    ~oli() {};

    bool initConsole() override {
        vOliEngine shClient;
        Shell shell(shClient);
        shell.run();
        return true;
    }

};

int main(int argc, char* argv[]) {
    oli app(RunMode::CONSOLE);
    app.startConsole();
    return app.run();
}