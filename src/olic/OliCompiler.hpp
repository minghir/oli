#ifndef OLICOMPILER_HPP
#define OLICOMPILER_HPP

#include <string>
#include "OliBytecode.hpp"
#include "../OliCommandParser.hpp"
#include "../OliExpressionParser.hpp"

class OliCompiler {
public:
    OliCompiler() = default;

    // Metoda principală: Text -> Bytecode
    OliChunk compile(const std::wstring& source);

private:
    void compileStatement(const ShellCommand& sc, OliChunk& chunk);
    void emitConstant(const vData& value, OliChunk& chunk, int line);
    void emitLoadOrConstant(const std::wstring& token, OliChunk& chunk);
    void emitStore(const std::wstring& varName, OliChunk& chunk);
    void generateShortCircuit(ASTPtr node, OliChunk& chunk);


    std::wstring rebuildSubCommand(const std::vector<std::wstring>& args, size_t start, size_t end);

    void generateFromAST(ASTPtr node, OliChunk& chunk);

    void optimize(OliChunk& chunk);

};

#endif