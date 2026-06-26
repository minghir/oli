#ifndef OLIEXPRESSIONPARSER_HPP
#define OLIEXPRESSIONPARSER_HPP

#include "ASTNode.hpp"
#include "StringUtils.hpp"
#include <vector>
#include <string>


//Ordinea corectă a apelurilor, de la cea mai mică prioritate la cea mai mare, ar trebui să fie :
//parseAssignment $\rightarrow$ parseCoalescing $\rightarrow$ parseLogical $\rightarrow$ parseBitwiseOR $\rightarrow$ parseBitwiseXOR $\rightarrow$ parseBitwiseAND $\rightarrow$ parseComparison $\rightarrow$ parseShift $\rightarrow$ parseAddition $\rightarrow$ parseMultiplication $\rightarrow$ parsePower $\rightarrow$ parseUnary $\rightarrow$ parsePostfix $\rightarrow$ parsePrimary.


class OliExpressionParser {
    std::vector<std::wstring> m_tokens;
    size_t m_pos = 0;

public:
    OliExpressionParser(const std::vector<std::wstring>& tokens) : m_tokens(tokens) {}
   
    ASTPtr parse() {
        if (m_tokens.empty()) return nullptr;
        // Punctul de intrare trebuie să fie Assignment pentru a recunoaște "="
        return parseAssignment();
    }

    ASTPtr parseAssignment() {
        // Încercăm să citim partea stângă (care poate fi o expresie de coalescing/logică)
        ASTPtr left = parseCoalescing();

        // Dacă urmează un operator de atribuire
        if (match({ L"=", L"+=", L"-=", L"*=", L"/=" })) {
            std::wstring op = m_tokens[m_pos - 1];
            ASTPtr node = std::make_shared<ASTNode>(ASTNodeType::Operator, op);
            node->addChild(left);
            node->addChild(parseAssignment()); // Recursivitate la dreapta
            return node;
        }
        return left;
    }

    // --- Nivelul 0: logic ---
    ASTPtr parseLogical() {
        ASTPtr left = parseBitwiseOR(); // Acum apelează Bitwise OR
        while (match({ L"&&", L"||" })) {
            std::wstring op = m_tokens[m_pos - 1];
            ASTPtr node = std::make_shared<ASTNode>(ASTNodeType::Operator, op);
            node->addChild(left);
            node->addChild(parseBitwiseOR());
            left = node;
        }
        return left;
    }

    // --- Nivelul 0.5: Comparații (==, !=, <, >) ---
    ASTPtr parseComparison() {
        ASTPtr left = parseShift(); // Acum apelează Shift
        while (match({ L"==", L"!=", L"<", L">", L"<=", L">=" })) {
            std::wstring op = m_tokens[m_pos - 1];
            ASTPtr node = std::make_shared<ASTNode>(ASTNodeType::Operator, op);
            node->addChild(left);
            node->addChild(parseShift());
            left = node;
        }
        return left;
    }

    // --- Nivelul 1: Adunare și Scădere ---
    ASTPtr parseAddition() {
        ASTPtr left = parseMultiplication();
        while (match({ L"+", L"-", L".." })) { // Suport pentru concatenare
            std::wstring op = m_tokens[m_pos - 1];
            std::wstring internalOp = (op == L"..") ? L"CONCAT" : op;
            ASTPtr node = std::make_shared<ASTNode>(ASTNodeType::Operator, internalOp);
            node->addChild(left);
            node->addChild(parseMultiplication());
            left = node;
        }
        return left;
    }

  

    ASTPtr parseMultiplication() {
        ASTPtr left = parsePower(); // Multiplication depinde de Power
        while (match({ L"*", L"/", L"%" })) {
            std::wstring op = m_tokens[m_pos - 1];
            ASTPtr node = std::make_shared<ASTNode>(ASTNodeType::Operator, op);
            node->addChild(left);
            node->addChild(parsePower());
            left = node;
        }
        return left;
    }

   
   
    ASTPtr parseUnary() {
        // Adăugăm L"~" pentru Bitwise NOT
        if (match({ L"-", L"!", L"NOT", L"~", L"*", L"**", L"&" })) {
            std::wstring op = m_tokens[m_pos - 1];

            if (op == L"**") {
                // Desfacem ** în două operații de DEREFERENCE separate
                ASTPtr inner = std::make_shared<ASTNode>(ASTNodeType::Operator, L"DEREFERENCE");
                inner->addChild(parseUnary());

                ASTPtr outer = std::make_shared<ASTNode>(ASTNodeType::Operator, L"DEREFERENCE");
                outer->addChild(inner);
                return outer;
            }

            // --- GESTIONARE ADRESĂ (&) ---
            if (op == L"&") {
                ASTPtr node = std::make_shared<ASTNode>(ASTNodeType::Operator, L"ADDRESS_OF");
                node->addChild(parseUnary());
                return node;
            }

            ASTPtr child = parseUnary();

			//pointer dereference: *$ptr sau *(getPtr())
            if (op == L"*") { 
                ASTPtr node = std::make_shared<ASTNode>(ASTNodeType::Operator, L"DEREFERENCE");
                node->addChild(child);
                return node;
            }

            std::wstring internalOp = op;
            if (op == L"-") internalOp = L"UNARY_MINUS";
            else if (op == L"~") internalOp = L"BITWISE_NOT";
            else if (op == L"!" || op == L"NOT") internalOp = L"NOT";

            ASTPtr node = std::make_shared<ASTNode>(ASTNodeType::Operator, internalOp);
            node->addChild(child);
            return node;
        }
        return parsePostfix();
    }

    // --- Nivel: Putere ---
    ASTPtr parsePower() {
        ASTPtr left = parseUnary();
        while (match({ L"**" })) {
            std::wstring op = m_tokens[m_pos - 1];
            ASTPtr node = std::make_shared<ASTNode>(ASTNodeType::Operator, op);
            node->addChild(left);
            node->addChild(parsePower());
            left = node;
        }
        return left;
    }
    
  

    

    ASTPtr parsePrimary() {
        std::wstring current = peek();
        if (current.empty()) return nullptr;

        // 1. Variabile ($a, @arr)
        if (current[0] == L'$' || current[0] == L'@') {
            m_pos++;
            return std::make_shared<ASTNode>(ASTNodeType::Variable, current);
        }

        // 2. Structuri & Paranteze de grupare
        if (match({ L"[" })) return parseArray();
        if (match({ L"{" })) return parseMap();
        if (match({ L"(" })) {
            ASTPtr node = parseAssignment(); // Punct de intrare pentru expresii în paranteze
            consume(L")", "Lipseste )");
            return node;
        }

        // 3. Bariera pentru structuri
        if (current == L"}" || current == L"]" || current == L")" ||
            current == L"," || current == L":") {
            return nullptr;
        }

        // 4. IDENTIFICATORI (test, CAN_CLS) și LITERALE (10, "text")
        // Aceasta este schimbarea cheie!
        std::wstring value = m_tokens[m_pos++];
        return std::make_shared<ASTNode>(ASTNodeType::Literal, value);
    }


    // Utilitare pentru deplasarea în lista de tokeni
    bool match(std::initializer_list<std::wstring> ops) {
        if (m_pos >= m_tokens.size()) return false;
        std::wstring current = to_upper(m_tokens[m_pos]); // Converteste tokenul actual la UPPER
        for (auto op : ops) {
            if (current == to_upper(op)) { // Compară UPPER cu UPPER
                m_pos++;
                return true;
            }
        }
        return false;
    }
    bool check(std::wstring token) {
        if (m_pos >= m_tokens.size()) return false;
        return m_tokens[m_pos] == token;
    }

    std::wstring peek() { return m_pos < m_tokens.size() ? m_tokens[m_pos] : L""; }

    

    void consume(std::wstring token, std::string error) {
        if (!check(token)) {
            std::wstring found = (m_pos < m_tokens.size()) ? m_tokens[m_pos] : L"EOF";

            // CONVERSIE SIGURĂ (Manuală pentru a evita crash-ul iteratorilor)
            std::string foundStr;
            for (wchar_t wc : found) foundStr += (wc < 128) ? (char)wc : '?';

            std::string expectedStr;
            for (wchar_t wc : token) expectedStr += (wc < 128) ? (char)wc : '?';

            std::string fullError = error + " (Gasit: '" + foundStr + "' in loc de '" + expectedStr + "')";
            throw std::runtime_error(fullError);
        }
        m_pos++;
    }


    
    ASTPtr parseMap() {
        // În acest punct, '{' a fost deja consumat de parsePrimary()
        ASTPtr node = std::make_shared<ASTNode>(ASTNodeType::Literal, L"MAP_OBJECT");

        // 1. Gestionăm cazul Map-ului gol: {}
        if (check(L"}")) {
            m_pos++; // Consumăm '}'
            return node;
        }

        // 2. Bucla de citire a perechilor cheie:valoare
        while (m_pos < m_tokens.size()) {

            // --- CITIRE CHEIE ---
            // Folosim parsePrimary() deoarece cheia este de obicei un Literal ("nume") 
            // sau o variabilă ($cheie).
            ASTPtr key = parsePrimary();
            if (!key) {
                throw std::runtime_error("Eroare Map: Se astepta o cheie (literal sau variabila).");
            }

            // --- CITIRE SEPARATOR ':' ---
            if (!match({ L":" })) {
                throw std::runtime_error("Eroare Map: Lipseste ':' dupa cheia '" +
                    std::string(key->value.begin(), key->value.end()) + "'.");
            }

            // --- CITIRE VALOARE ---
            // Folosim parseCoalescing() pentru a permite orice expresie, 
            // inclusiv calcule, alte Map-uri sau Array-uri.
            ASTPtr value = parseCoalescing();
            if (!value) {
                throw std::runtime_error("Eroare Map: Lipseste valoarea pentru cheia '" +
                    std::string(key->value.begin(), key->value.end()) + "'.");
            }

            // Adăugăm perechea în arborele AST
            node->addChild(key);
            node->addChild(value);

            // --- GESTIONARE CONTINUARE (Virgulă sau Inchidere) ---
            if (match({ L"," })) {
                // Suport pentru "trailing comma": dacă după virgulă urmează '}', ne oprim.
                if (check(L"}")) {
                    break;
                }
                // Altfel, continuăm bucla pentru următoarea pereche.
                continue;
            }
            else {
                // Dacă nu avem virgulă, singurul token valid rămas este '}'.
                // Bucla se va opri aici, iar consume() de mai jos va verifica închiderea.
                break;
            }
        }

        // 3. Finalizarea obiectului
        consume(L"}", "Eroare Map: Obiect neinchis. Se astepta '}' la final.");

        return node;
    }

    ASTPtr parseArray() {
        ASTPtr node = std::make_shared<ASTNode>(ASTNodeType::Literal, L"ARRAY_OBJECT");
        while (!check(L"]")) {
            node->addChild(parseLogical()); // Adăugăm elementul calculat
            if (!match({ L"," })) break;     // Dacă nu e virgulă, am terminat
        }
        consume(L"]", "Asteptam ] la finalul array-ului");
        return node;
    }
   
    

    ASTPtr parsePostfix() {
        // Începem cu unitatea de bază (literal, variabilă, paranteze)
        ASTPtr node = parsePrimary();
        if (!node) return nullptr;

        // Bucla permite înlănțuiri: $obj.prop, $arr[0], $func()++
        while (true) {
            // --- 1. ACCES MEMBRU ($obj.prop) ---
            if (match({ L"." })) {
                if (m_pos >= m_tokens.size()) {
                    throw std::runtime_error("Eroare Sintaxa: Se astepta un nume de camp dupa '.'");
                }

                std::wstring fieldName = m_tokens[m_pos++];
                ASTPtr dotNode = std::make_shared<ASTNode>(ASTNodeType::Operator, L"DOT");
                dotNode->addChild(node);
                dotNode->addChild(std::make_shared<ASTNode>(ASTNodeType::Literal, fieldName));
                node = dotNode;
            }

            // --- 2. INDEXARE ($arr[index]) ---
            else if (match({ L"[" })) {
                ASTPtr indexNode = std::make_shared<ASTNode>(ASTNodeType::Operator, L"INDEX");
                indexNode->addChild(node);
                indexNode->addChild(parseAssignment()); // Permite orice expresie în index
                consume(L"]", "Lipseste ]");
                node = indexNode;
            }

            // --- 3. APEL DINAMIC ($var() sau $obj.metoda()) ---
            else if (match({ L"(" })) {
                ASTPtr callNode = std::make_shared<ASTNode>(ASTNodeType::FunctionCall, L"DYNAMIC_CALL");
                callNode->addChild(node);
                if (!check(L")")) {
                    do {
                        ASTPtr arg = parseAssignment();
                        if (arg) callNode->addChild(arg);
                        else break; // Siguranță împotriva buclelor infinite
                    } while (match({ L"," }));
                }
                consume(L")", "Lipseste )");
                node = callNode;
            }

            // --- 4. POSTFIX INCREMENT/DECREMENT ($i++) ---
            else if (match({ L"++", L"--" })) {
                std::wstring op = m_tokens[m_pos - 1];
                std::wstring internalOp = (op == L"++") ? L"POSTFIX_INC" : L"POSTFIX_DEC";

                ASTPtr postfixNode = std::make_shared<ASTNode>(ASTNodeType::Operator, internalOp);
                postfixNode->addChild(node);
                node = postfixNode;

                // După un operator postfix (ca ++), în majoritatea limbajelor (inclusiv Oli)
                // nu mai poți continua cu alți operatori postfix pe aceeași unitate.
                break;
            }

            else {
                // Nu mai există operatori de tip postfix, ieșim din buclă
                break;
            }
        }

        return node;
    }


    ASTPtr parseCoalescing() {
        ASTPtr left = parseLogical();
        while (match({ L"??" })) {
            ASTPtr node = std::make_shared<ASTNode>(ASTNodeType::Operator, L"??");
            node->addChild(left);
            node->addChild(parseLogical());
            left = node;
        }
        return left;
    }


    // --- Nivel: Bitwise OR (|) ---
    ASTPtr parseBitwiseOR() {
        ASTPtr left = parseBitwiseXOR();
        while (match({ L"|" })) {
            ASTPtr node = std::make_shared<ASTNode>(ASTNodeType::Operator, L"|");
            node->addChild(left);
            node->addChild(parseBitwiseXOR());
            left = node;
        }
        return left;
    }

    // --- Nivel: Bitwise XOR (BXOR) ---
    // Notă: Folosim "BXOR" sau un alt token dacă "^" este deja rezervat pentru Putere
    ASTPtr parseBitwiseXOR() {
        ASTPtr left = parseBitwiseAND();
        // Adăugăm suport și pentru simbolul '^'
        while (match({ L"BXOR", L"^" })) {
            std::wstring op = m_tokens[m_pos - 1];
            ASTPtr node = std::make_shared<ASTNode>(ASTNodeType::Operator, op);
            node->addChild(left);
            node->addChild(parseBitwiseAND());
            left = node;
        }
        return left;
    }

    // --- Nivel: Bitwise AND (&) ---
    ASTPtr parseBitwiseAND() {
        ASTPtr left = parseComparison();
        while (match({ L"&" })) {
            ASTPtr node = std::make_shared<ASTNode>(ASTNodeType::Operator, L"&");
            node->addChild(left);
            node->addChild(parseComparison());
            left = node;
        }
        return left;
    }

    // --- Nivel: Shift (<<, >>) ---
    ASTPtr parseShift() {
        ASTPtr left = parseAddition();
        while (match({ L"<<" , L">>" })) {
            std::wstring op = m_tokens[m_pos - 1];
            ASTPtr node = std::make_shared<ASTNode>(ASTNodeType::Operator, op);
            node->addChild(left);
            node->addChild(parseAddition());
            left = node;
        }
        return left;
    }

};

#endif