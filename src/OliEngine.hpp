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


#include "vData.hpp"
#include "IShellEngine.hpp"
#include "IOliEngine.hpp"
#include "StringUtils.hpp"
#include "ConsoleManager.hpp"
#include "OliCommandParser.hpp"
#include "OliExpressionParser.hpp"
#include "olic/OliBytecode.hpp"

struct vTypeBlueprint {
    std::wstring name;
    std::vector<std::wstring> fields;             // x, y, hp, mana
    std::unordered_map<std::wstring, std::wstring> methods; // attack -> "PLAYER_ATTACK_FUNC"
    bool isClass = false;                         // Doar pentru validare semantică
};

struct Procedure {
    std::wstring name;
    std::vector<std::wstring> params;
    std::vector<std::wstring> body; // Liniile de cod salvate
    bool isVariadic = false;
};

struct VarPath {
    std::wstring rootName;
    std::vector<std::wstring> indexes;
};



enum class OliStatus {
    RUNNING,
    BREAK_REQUESTED,
    CONTINUE_REQUESTED,
    RETURN_REQUESTED,
    ERR
};

struct StackFrame {
    std::wstring functionName;
    std::unordered_map<std::wstring, vData> localVariables;
    int lineCalled; // Opțional, dacă ai un Line Counter
};

class vOliEngine : public IShellEngine, public IOliEngine {

private:
    bool m_running = true;
    std::wstring m_accumulator;
    std::vector<std::wstring> m_history;

    //std::map<std::wstring, OliCommandHandler> m_commandHandlers;
    std::unordered_map<std::wstring, std::function<void(const std::wstring&)>> m_commandHandlers;
    //std::unordered_map<std::wstring, std::function<void(const ShellCommand&)>> m_commandHandlers;

    std::unordered_map<std::wstring, OliFunctionHandler> m_functionsHandlers;

    //std::map<std::wstring, vData> m_globalVariables;
    std::unordered_map<std::wstring, vData> m_globalVariables;

    std::unordered_map<std::wstring, Procedure> m_procedures;
    bool m_isRecording = false;
    std::wstring m_activeProcName;
    OliStatus m_executionStatus = OliStatus::RUNNING;

    std::unordered_map<std::wstring, Procedure> m_userFunctions; // Refolosim structura Procedure
    bool m_isRecordingFunc = false;
    std::wstring m_activeFuncName;
    int m_bracketDepth = 0;

    bool m_shouldReturn = false;

    int m_blockDepth = 0; // Contor pentru IF/FOR/WHILE/CYCLE deschise
    bool m_isMultiLine = false; // Dacă suntem în mod de acumulare automată
    

    // Stiva de contexte (fiecare context este un map de variabile)
    std::vector<StackFrame> m_callStack;

    std::unordered_map<std::wstring, vTypeBlueprint> m_blueprints;

    bool m_nextSetIsGlobal = false;

    // 0 înseamnă "fără limită" (infinit), orice valoare > 0 este pragul de siguranță
    long long m_maxIterations = 10000;
	bool m_echoEnabled = true;
    std::wstring m_pluginsPath = L"./plugins/"; // Valoare default

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
    void executeInternal(const std::wstring& fullInput);
private:
    vData evaluateExpression(const std::wstring& expr);
    vData resolveVariable(const std::wstring& rawVar);
    vData parseArrayContent(const std::wstring& content);
    
    void assignToVariable(const std::wstring& varExpr, const vData& newValue);
    void assignToMapVar(vData* container, const std::wstring& indexExpr, const vData& newValue);
    void assignToArrayVar(vData* container, const std::wstring& indexExpr, const vData& newValue);
    vData* navigateOrCreatePath(vData* root, const std::wstring& varExpr);

    vData* getOrCreateContainer(vData* root, const std::wstring& indexExpr, bool isNextBracketArray);
    std::vector<std::wstring> splitPath(const std::wstring& expr);


    std::vector<std::wstring> splitByCommaIgnoringBrackets(const std::wstring& content);
    size_t findClosingBracket(const std::wstring& str, size_t start);

    std::wstring getVariantTypeName(const vData& data);
    void printVData(const vData& data, bool quoteStrings);
    //void setVariable(const std::wstring& name, const vData& value);
    void setVariable(const std::wstring& name, const vData& value, bool isGlobal=false);
    void updateContainerValue(ASTPtr containerNode, vData key, vData newValue);

    vData executeAST(ASTPtr node);
    vData executeBinaryOperator(const std::wstring& op, const vData& left, const vData& right);
    std::wstring reconstructPath(ASTPtr node);
    
    double vDataToDouble(const vData& data) const;
    long long vDataToLong(const vData& data);
    bool vDataToBool(const vData& data);
    std::wstring vDataToWString(const vData& data);

    //vData parseRawLiteral(const std::wstring& val);
    vData parseRawLiteral(std::wstring_view val);
    vData accessContainer(const vData& container, const vData& index);

    vData* getContainerPointer(vData& container, const std::wstring& keyOrIdx);
    //vData* resolveToParent(const std::wstring& rootName, const std::vector<std::wstring>& indexes);
    vData* resolveToParent(const std::wstring& rootName, const std::vector<std::wstring>& indexes, bool forceGlobal = false);
    VarPath parsePath(const std::wstring& raw);

    std::vector<std::wstring> preParse(const std::wstring& line);
    size_t findKeywordPos(const std::wstring& line, const std::wstring& keyword);
    
    size_t findTopLevelKeyword(const std::wstring& line, const std::wstring& keyword, const std::wstring& startCommand);
    std::wstring substituteVariables(const std::wstring& input);
    std::wstring cleanVariableName(const std::wstring& name);

    void callProcedure(const Procedure& proc, const std::vector<std::wstring>& passedArgs);
    vData callUserFunction(const std::wstring& funcName, const std::vector<vData>& args, vData context = { std::monostate{} });
    void printTraceback();
    void dumpStackTrace();

    bool canBeNumeric(const vData& data) const;
    bool compareVData(const vData& lhs, const vData& rhs);
    vData parseJSONValue(const std::wstring& json, size_t& pos);

    void initializeCommandsHandlers();
    
    void handleIfCommand(const std::wstring& fullLine);
    size_t findTopLevelIfKeyword(const std::wstring& line, const std::wstring& keyword);

    void handleWhileCommand(const std::wstring& fullLine);
    size_t findTopLevelWhileKeyword(const std::wstring& line, const std::wstring& keyword);
    void debugWhile(const std::wstring& condition, const std::vector<std::wstring>& instrs);
    
    void handleRepeatCommand(const std::wstring& fullLine);
    

    void handleCycleCommand(const std::wstring& fullLine);
    size_t findTopLevelCycleKeyword(const std::wstring& line, const std::wstring& keyword);

    bool executeCycleStep(const std::wstring& iterName, const vData& value, const std::vector<std::wstring>& instrs);

    void handleForCommand(const std::wstring& fullLine);

    void handleSwitchCommand(const std::wstring& fullLine);
    size_t findTopLevelSwitchKeyword(const std::wstring& line, const std::wstring& keyword);

    void updateDataMember(vData& container, const vData& key, const vData& newValue);
    void updateRootSource(ASTPtr node, const vData& updatedValue);
    vData deepCopy(const vData& source);

    // Metodele de suport pentru handlere
    void handleQuitCommand(const ShellCommand& sc);
    void handleSetCommand(const ShellCommand& sc);
    void handleEchoCommand(const ShellCommand& sc);// , bool debugMode = false);
    void handleDumpMemCommand(const ShellCommand& sc);
    void handleUnsetCommand(const ShellCommand& sc);
    void handleRunCommand(const ShellCommand& sc);
    void handleSysCommand(const ShellCommand& sc);
    void handleProcCommand(const ShellCommand& sc);
    void handleFuncCommand(const ShellCommand& sc);
    void handlePluginCommand(const ShellCommand& sc);
    void handleListCommand(const ShellCommand& sc);
    void handleListFuncsCommand(const ShellCommand& sc);
    void handleListProcsCommand(const ShellCommand& sc);

    void dumpFunctionDetails(const std::wstring& name);
    void dumpProcedureDetails(const std::wstring& name);
    
    void handleTraceCommand(const ShellCommand& sc);

    void handleBreakCommand(const ShellCommand& sc);
    void handleContinueCommand(const ShellCommand& sc);
    void handleReturnCommand(const ShellCommand& sc);

    void handleClearCommand(const ShellCommand& sc);
    void handleDefCommand(const ShellCommand& sc);

    void handleHelpCommand(const ShellCommand& sc);
    void handleConfigCommand(const ShellCommand& sc);
    

    void initializeFunctionsHandlers();
    vData handleInputFunc(const std::vector<vData>& args);
    vData handleRandomFunc(const std::vector<vData>& args);
    vData handleWaitFunc(const std::vector<vData>& args);
    vData handleSysFunc(const std::vector<vData>& args);
    vData handleContainsFunc(const std::vector<vData>& args);
    vData handleEvalFunc(const std::vector<vData>& args);
    vData handleIntFunc(const std::vector<vData>& args);
    vData handleFloatFunc(const std::vector<vData>& args);
    vData handleStrFunc(const std::vector<vData>& args);
    vData handleArrayFunc(const std::vector<vData>& args);
    vData handleMapFunc(const std::vector<vData>& args);
    vData handleSplitFunc(const std::vector<vData>& args);
    vData handleJoinFunc(const std::vector<vData>& args);
    vData handleTrimFunc(const std::vector<vData>& args);

    vData handleReadFileFunc(const std::vector<vData>& args);
    vData handleWriteFileFunc(const std::vector<vData>& args);
    vData handleAppendFileFunc(const std::vector<vData>& args);
    vData handleExistsFileFunc(const std::vector<vData>& args);
    vData handleDeleteFileFunc(const std::vector<vData>& args);
    //vData handleKeysFunc(const std::vector<vData>& args);
    
    public:

///command plugin interface
        void setVar(const std::wstring& name, const vData& value) override {
            // Aici apelezi logica ta existentă de setare (ex: assignToVariable sau varianta ta de setVariable)
            this->assignToVariable(name, value);
        }

        vData getVar(const std::wstring& name) override {
            // Aici apelezi logica ta de rezolvare (ex: resolveVariable)
            return this->resolveVariable(name);
        }

        void execCommand(const std::wstring& command) override {
            // Aici apelezi executeInternal sau execute
            this->executeInternal(command);
        }

        void logSuccess(const std::wstring& msg) override {
            LOG_SUCCESS(msg); // Presupunând că LOG_SUCCESS e un macro sau funcție globală
        }

        void logError(const std::wstring& msg) override {
            LOG_ERROR(msg);
        }
    
        ///bitecode
        void executeBytecode(const OliChunk& chunk);
        void loadAndRunBytecode(const std::string& path);
};
#endif