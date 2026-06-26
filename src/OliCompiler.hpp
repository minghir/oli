#ifndef OLICOMPILER_HPP
#define OLICOMPILER_HPP

#include <string>
#include "OliBytecode.hpp"
#include "OliCommandParser.hpp"
#include "OliExpressionParser.hpp"
#include <unordered_map>
#include <functional>

struct Local {
    std::wstring name;
    int depth; // util pentru blocuri imbricate {}
};

// Definim tipul de handler pentru comenzi
struct ShellCommand; // Forward declaration
using OliCommandHandler = std::function<void(const ShellCommand&)>;

// Definim semnătura funcției din DLL (LoadOliCommandPlugin)
class IOliEngine; // Forward declaration

class OliCompiler {


    std::vector<std::vector<size_t>> breakStack;
    std::vector<std::vector<size_t>> continueStack;
    std::unordered_set<std::wstring> m_includedFiles;
	
	std::vector<Local> locals;    // Tabela de variabile locale pentru funcția curentă
    bool isInFunction = false;    // Flag pentru a știi dacă folosim stiva sau globalul

public:
    OliCompiler() = default;

    // Metoda principală: Text -> Bytecode
    OliChunk compile(const std::wstring& source, const std::unordered_map<std::wstring, ByteCodeProcedure>& parentProcs = {}, bool isSubBlock = false);

private:
    void compileStatement(const ShellCommand& sc, OliChunk& chunk,
        const std::unordered_map<std::wstring, ByteCodeProcedure>& externalProcs);

    void emitConstant(const vData& value, OliChunk& chunk, int line);
    void emitLoadOrConstant(const std::wstring& token, OliChunk& chunk);
    void emitStore(const std::wstring& varName, OliChunk& chunk);
    void emitTargetAddress(const std::wstring& varName, OliChunk& chunk);
    void generateShortCircuit(ASTPtr node, OliChunk& chunk, const std::unordered_map<std::wstring, ByteCodeProcedure>& externalProcs);
    std::wstring reconstructRawName(ASTPtr node);

    std::wstring rebuildSubCommand(const std::vector<std::wstring>& args, size_t start, size_t end);

    void generateFromAST(ASTPtr node, OliChunk& chunk,
        const std::unordered_map<std::wstring, ByteCodeProcedure>& externalProcs);

    void optimize(OliChunk& chunk);

    void loadPluginMetadata(std::wstring pluginName);

    void compileSubBlock(const std::vector<std::wstring>& args,
        int start,
        int end,
        OliChunk& chunk,
        const std::unordered_map<std::wstring, ByteCodeProcedure>& externalProcs);

    std::wstring cleanVariableName(const std::wstring& name);
};

#endif