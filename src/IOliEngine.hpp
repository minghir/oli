#ifndef IOLIENGINE_HPP
#define IOLIENGINE_HPP

#include <string>
#include "vData.hpp"
#include "OliCommandParser.hpp"

// Interfața pe care Engine-ul o implementează și Plugin-ul o consumă
class IOliEngine {
public:
    virtual ~IOliEngine() {}

    virtual void setVar(const std::wstring& name, const vData& value) = 0;
    virtual vData getVar(const std::wstring& name) = 0;
    virtual void execCommand(const std::wstring& command) = 0;
    virtual void logSuccess(const std::wstring& msg) = 0;
    virtual void logError(const std::wstring& msg) = 0;

    virtual vData callUserByteCodeFunction(
        const wchar_t* funcName,
        const vData* argsArray,
        size_t argCount,
        vData context
    ) = 0;
};

// Tipul de funcție pe care DLL-ul trebuie să o exporte
typedef void (*LoadCommandsFunc)(
    std::unordered_map<std::wstring, std::function<void(const std::wstring&)>>&, 
    IOliEngine*
);


#endif