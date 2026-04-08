#ifndef ASTNODE_HPP
#define ASTNODE_HPP

#include <string>
#include <vector>
#include <memory>
#include <iostream>
//Arbore de Sintaxă Abstractă (AST)
// 
// Tipuri de noduri pe care Oli le va procesa
enum class ASTNodeType {
    Literal,        // "text", 100, 3.14, true
    Variable,       // $nume, $player["hp"]
    FunctionCall,   // TYPE(), LEN(), FACT()
    Operator,       // +, -, *, /, ^, ==, &&
    Group           // Expresii între paranteze ( )
};

struct ASTNode {
    ASTNodeType type;
    std::wstring value; // Numele funcției, al variabilei, al operatorului sau valoarea brută

    // Copiii nodului: 
    // - Pentru FunctionCall: sunt argumentele functiei
    // - Pentru Operator: [0] e operandul stâng, [1] e cel drept
    // - Pentru Literal: vectorul e gol
    std::vector<std::shared_ptr<ASTNode>> children;

    // Constructori utilitari
    ASTNode(ASTNodeType t, std::wstring v) : type(t), value(v) {}

    // Metodă helper pentru a adăuga un copil
    void addChild(std::shared_ptr<ASTNode> child) {
        children.push_back(child);
    }

    // Debug: Afișează arborele structurat (indentat) în consolă
    void dump(int indent = 0) const {
        for (int i = 0; i < indent; ++i) std::wcout << L"  ";
        std::wcout << L"|_ " << getTypeName() << L": " << value << std::endl;
        for (const auto& child : children) {
            child->dump(indent + 1);
        }
    }

private:
    std::wstring getTypeName() const {
        switch (type) {
        case ASTNodeType::Literal:      return L"LITERAL";
        case ASTNodeType::Variable:     return L"VARIABLE";
        case ASTNodeType::FunctionCall: return L"FUNC_CALL";
        case ASTNodeType::Operator:     return L"OPERATOR";
        case ASTNodeType::Group:        return L"GROUP";
        default:                        return L"UNKNOWN";
        }
    }
};

using ASTPtr = std::shared_ptr<ASTNode>;

#endif