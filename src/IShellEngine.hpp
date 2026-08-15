#ifndef ISHELLENGINE_HPP
#define ISHELLENGINE_HPP

#include <string>

class IShellEngine {
public:
    virtual ~IShellEngine() = default;

    // Metoda principală de procesare
    virtual void execute(const std::wstring& line) = 0;

    // Informații cerute de UI (Shell)
    virtual std::wstring getPrompt() const = 0;
    virtual bool shouldExit() const = 0;
};

#endif