#ifndef VOLIENGINE_HPP
#define VOLIENGINE_HPP
#pragma once

#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
#include <cwctype>
#include <cctype>
#include <map>
#include <functional>
#include <variant>

#pragma once

#include "IShellEngine.hpp"
#include "stringUtils.hpp"
#include "ConsoleManager.hpp"
#include "OliCommandParser.hpp"
#include "OliExpressionParser.hpp"

struct Procedure {
    std::wstring name;
    std::vector<std::wstring> params;
    std::vector<std::wstring> body; // Liniile de cod salvate
};

struct VarPath {
    std::wstring rootName;
    std::vector<std::wstring> indexes;
};

struct vData; // Forward declaration

using vDataArray = std::vector<vData>;
using vDataMap = std::map<std::wstring, vData>;
using OliCommandHandler = std::function<void(const ShellCommand&)>;
//using OliFunctionHandler = std::function<vData(const std::vector<std::wstring>& args)>;
using OliFunctionHandler = std::function<vData(const std::vector<vData>& args)>;

using vDataValue = std::variant<
    std::monostate,
    std::wstring,
    long long,
    double,
    bool,
    vDataArray,
    vDataMap
>;

struct vData {
    vDataValue value;

    // Utilitar pentru a verifica dacă este array sau string
    bool isArray() const { return std::holds_alternative<vDataArray>(value); }
    bool isMap() const { return std::holds_alternative<vDataMap>(value); }
    bool isString() const { return std::holds_alternative<std::wstring>(value); }
    bool isInt() const { return std::holds_alternative<long long>(value); }
    bool isFloat() const { return std::holds_alternative<double>(value); }
    bool isBool() const { return std::holds_alternative<bool>(value); }
    bool IsNull() const { return std::holds_alternative<std::monostate>(value); }
};

enum class OliStatus {
    RUNNING,
    BREAK_REQUESTED,
    CONTINUE_REQUESTED,
    ERR
};

class vOliEngine : public IShellEngine {

private:
    bool m_running = true;
    std::wstring m_accumulator;
    std::vector<std::wstring> m_history;

    //std::map<std::wstring, OliCommandHandler> m_commandHandlers;
    std::unordered_map<std::wstring, std::function<void(const std::wstring&)>> m_commandHandlers;
    std::map<std::wstring, OliFunctionHandler> m_functionsHandlers;

    std::map<std::wstring, vData> m_variables;


    std::map<std::wstring, Procedure> m_procedures;
    bool m_isRecording = false;
    std::wstring m_activeProcName;
    OliStatus m_executionStatus = OliStatus::RUNNING;

    std::map<std::wstring, Procedure> m_userFunctions; // Refolosim structura Procedure
    bool m_isRecordingFunc = false;
    std::wstring m_activeFuncName;

    bool m_shouldReturn = false;
    
public:

    vOliEngine(){
        initializeCommandsHandlers();
        initializeFunctionsHandlers();
    };
    /*
    std::wstring getPrompt() const override {
        return m_accumulator.empty() || !m_isRecording ? L"\noli# " : L"  -> ";
    }
    */

    std::wstring getPrompt() const override {
        if (m_isRecording) return L"  -> ";
        if(m_isRecordingFunc)return L"  -> ";
        if (!m_accumulator.empty()) return L"   -> "; // Bonus: pentru linii neterminate cu 
        return L"oli# ";
    }
    bool shouldExit() const override { return !m_running; }
    bool stop() { return m_running = false; }
    
    void execute(const std::wstring& line);

    void addToHistory(const std::wstring& command);

protected:
    void executeCommand(const std::wstring& command);
private:
    vData evaluateExpression(const std::wstring& expr);
    vData resolveVariable(const std::wstring& rawVar);
    vData parseArrayContent(const std::wstring& content);
    
    void assignToVariable(const std::wstring& varExpr, const vData& newValue);
    void assignToMapVar(vData* container, const std::wstring& indexExpr, const vData& newValue);
    void assignToArrayVar(vData* container, const std::wstring& indexExpr, const vData& newValue);
    vData* navigateOrCreatePath(vData* root, const std::wstring& varExpr);

    vData* getOrCreateContainer(vData* root, const std::wstring& indexExpr, bool isNextBracketArray);

    std::vector<std::wstring> splitByCommaIgnoringBrackets(const std::wstring& content);
    size_t findClosingBracket(const std::wstring& str, size_t start);

    std::wstring getVariantTypeName(const vData& data);
    void printVData(const vData& data, bool quoteStrings);

    vData executeAST(ASTPtr node);
    vData executeBinaryOperator(const std::wstring& op, const vData& left, const vData& right);
    
    double vDataToDouble(const vData& data);
    long long vDataToLong(const vData& data);
    bool vDataToBool(const vData& data);
    std::wstring vDataToWString(const vData& data);

    vData parseRawLiteral(const std::wstring& val);
    vData accessContainer(const vData& container, const vData& index);

    vData* getContainerPointer(vData& container, const std::wstring& keyOrIdx);
    vData* resolveToParent(const std::wstring& rootName, const std::vector<std::wstring>& indexes);
    VarPath parsePath(const std::wstring& raw);

    std::vector<std::wstring> preParse(const std::wstring& line);
    size_t findKeywordPos(const std::wstring& line, const std::wstring& keyword);
    
    size_t findTopLevelKeyword(const std::wstring& line, const std::wstring& keyword, const std::wstring& startCommand);
    std::wstring substituteVariables(const std::wstring& input, const std::map<std::wstring, vData>& vars);
    
    void callProcedure(const Procedure& proc, const std::vector<std::wstring>& passedArgs);
    vData callUserFunction(const std::wstring& funcName, const std::vector<vData>& args);




    void initializeCommandsHandlers();
    
    void handleIfCommand(const std::wstring& fullLine);
    size_t findTopLevelIfKeyword(const std::wstring& line, const std::wstring& keyword);

    void handleWhileCommand(const std::wstring& fullLine);
    size_t findTopLevelWhileKeyword(const std::wstring& line, const std::wstring& keyword);
    
    

    void handleCycleCommand(const std::wstring& fullLine);
    bool executeCycleStep(const std::wstring& iterName, const vData& value, const std::vector<std::wstring>& instrs);

    void handleForCommand(const std::wstring& fullLine);


    // Metodele de suport pentru handlere
    void handleQuitCommand(const ShellCommand& sc);
    void handleSetCommand(const ShellCommand& sc);
    void handleEchoCommand(const ShellCommand& sc);// , bool debugMode = false);
    void handleDumpMemCommand(const ShellCommand& sc);
    void handleUnsetCommand(const ShellCommand& sc);
    void handleUnsetAllCommand(const ShellCommand& sc);;
    void handleRunCommand(const ShellCommand& sc);
    void handleSysCommand(const ShellCommand& sc);
    void handleProcCommand(const ShellCommand& sc);
    void handleFuncCommand(const ShellCommand& sc);
    void handlePluginCommand(const ShellCommand& sc);
    void handleListProcsCommand(const ShellCommand& sc);

    void handleBreakCommand(const ShellCommand& sc);
    void handleContinueCommand(const ShellCommand& sc);
    void handleReturnCommand(const ShellCommand& sc);

    void initializeFunctionsHandlers();
    vData handleInputFunc(const std::vector<vData>& args);
    vData handleRandomFunc(const std::vector<vData>& args);
    vData handleWaitFunc(const std::vector<vData>& args);
    vData handleSysFunc(const std::vector<vData>& args);
    vData handleContainsFunc(const std::vector<vData>& args);
    vData handleEvalFunc(const std::vector<vData>& args);
    vData handleIntFunc(const std::vector<vData>& args);
    //vData handleKeysFunc(const std::vector<vData>& args);
    

    
    
    
};
#endif