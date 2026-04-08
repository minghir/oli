#ifndef OLIEXPRESSIONPARSER_HPP
#define OLIEXPRESSIONPARSER_HPP

#include "ASTNode.hpp"

#include <vector>
#include <string>


class OliExpressionParser {
    std::vector<std::wstring> m_tokens;
    size_t m_pos = 0;

public:
    OliExpressionParser(const std::vector<std::wstring>& tokens) : m_tokens(tokens) {}
    /*
    ASTPtr parse() {
        //ASTPtr left = parseLogical();
        ASTPtr left = parseCoalescing();

        // Concatenăm DOAR dacă următorul token nu este un operator sau terminator
        while (m_pos < m_tokens.size() &&
            !check(L")") && !check(L"]") && !check(L"}") && !check(L",") && !check(L":")) {

            // Dacă e un operator logic sau matematic, înseamnă că parseLogical trebuia să-l prindă
            // Dacă am ajuns aici, e probabil un string liber sau o variabilă lipită
            //ASTPtr right = parseLogical();
            ASTPtr right = parseCoalescing(); 
            ASTPtr concatNode = std::make_shared<ASTNode>(ASTNodeType::Operator, L"+");
            concatNode->addChild(left);
            concatNode->addChild(right);
            left = concatNode;
        }
        return left;
    }
    */
    ASTPtr parse() {
        if (m_tokens.empty()) return nullptr;

        ASTPtr left = parseCoalescing();

        while (m_pos < m_tokens.size()) {
            // Dacă dăm de un token care clar închide o structură superioară, ne oprim
            if (check(L")") || check(L"]") || check(L"}") || check(L",") || check(L":")) {
                break;
            }

            ASTPtr right = parseCoalescing();
            if (!right) break; // Siguranță

            ASTPtr concatNode = std::make_shared<ASTNode>(ASTNodeType::Operator, L"+");
            concatNode->addChild(left);
            concatNode->addChild(right);
            left = concatNode;
        }
        return left;
    }
private:
    // --- Nivelul 0: logic ---
    ASTPtr parseLogical() {
        ASTPtr left = parseComparison();
        while (match({ L"&&", L"||" })) {
            std::wstring op = m_tokens[m_pos - 1];
            ASTPtr node = std::make_shared<ASTNode>(ASTNodeType::Operator, op);
            node->addChild(left);
            node->addChild(parseComparison());
            left = node;
        }
        return left;
    }

    // --- Nivelul 0.5: Comparații (==, !=, <, >) ---
    ASTPtr parseComparison() {
        ASTPtr left = parseAddition(); // Comparația apelează Adunarea
        while (match({ L"==", L"!=", L"<", L">", L"<=", L">=" })) {
            std::wstring op = m_tokens[m_pos - 1];
            ASTPtr node = std::make_shared<ASTNode>(ASTNodeType::Operator, op);
            node->addChild(left);
            node->addChild(parseAddition());
            left = node;
        }
        return left;
    }

    // --- Nivelul 1: Adunare și Scădere ---
    ASTPtr parseAddition() {
        ASTPtr left = parseMultiplication();
        while (match({ L"+", L"-" })) {
            std::wstring op = m_tokens[m_pos - 1];
            ASTPtr node = std::make_shared<ASTNode>(ASTNodeType::Operator, op);
            node->addChild(left);
            node->addChild(parseMultiplication());
            left = node;
        }
        return left;
    }

    // --- Nivelul 2: Înmulțire și Împărțire ---
    /*
    ASTPtr parseMultiplication() {
        ASTPtr left = parsePostfix();
        while (match({ L"*", L"/", L"%", L"^", L"**"})) {
            std::wstring op = m_tokens[m_pos - 1];
            ASTPtr node = std::make_shared<ASTNode>(ASTNodeType::Operator, op);
            node->addChild(left);
            node->addChild(parsePostfix());
            left = node;
        }
        return left;
    }
    */

    ASTPtr parseMultiplication() {
        ASTPtr left = parsePower();
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
        if (match({ L"-", L"!", L"NOT" })) {
            std::wstring op = m_tokens[m_pos - 1];
            // Mapăm minusul unar la un nume specific pentru a-l diferenția de scădere
            std::wstring internalOp = (op == L"-") ? L"UNARY_MINUS" : L"NOT";

            ASTPtr node = std::make_shared<ASTNode>(ASTNodeType::Operator, internalOp);
            node->addChild(parseUnary()); // Recursivitate pentru chestii gen --5
            return node;
        }
        return parsePostfix();
    }

    ASTPtr parsePower() {
        ASTPtr left = parseUnary(); // <--- Schimbat aici din parsePostfix
        while (match({ L"^", L"**" })) {
            std::wstring op = m_tokens[m_pos - 1];
            ASTPtr node = std::make_shared<ASTNode>(ASTNodeType::Operator, op);
            node->addChild(left);
            node->addChild(parseUnary()); // <--- Și aici
            left = node;
        }
        return left;
    }
    /*
    // --- Nivelul 3: Atomi (Cifre, Variabile, Funcții, Paranteze) ---
    ASTPtr parsePrimary() {
        if (match({ L"$" })) {
            if (check(L"$")) {
                ASTPtr node = std::make_shared<ASTNode>(ASTNodeType::Operator, L"DEREFERENCE");
                node->addChild(parsePrimary());
                return node;
            }
            // Variabilă normală: token-ul curent e numele (fără $)
            std::wstring name = m_tokens[m_pos++];
            return std::make_shared<ASTNode>(ASTNodeType::Variable, name);
        }

        if (match({ L"[" })) {
            return parseArray();
        }

        if (match({ L"{" })) {
            return parseMap();
        }

        if (match({ L"(" })) {
            //ASTPtr node = parseAddition(); // Evaluăm ce e în paranteză
            //ASTPtr node = parseLogical();
            ASTPtr node = parseCoalescing();
            consume(L")", "Așteptam închiderea parantezei");
            return node;
        }

        std::wstring current = peek();

        // Este funcție? (Nume urmat de '(')
        if (m_pos + 1 < m_tokens.size() && m_tokens[m_pos + 1] == L"(") {
            std::wstring funcName = m_tokens[m_pos++];
            m_pos++; // sărim peste '('
            ASTPtr node = std::make_shared<ASTNode>(ASTNodeType::FunctionCall, funcName);

            while (!check(L")")) {
                //node->addChild(parseAddition());
                //node->addChild(parseLogical());
                node->addChild(parseCoalescing());
                if (match({ L"," })) {} // sărim peste virgule între argumente
            }
            consume(L")", "Așteptam ')' după funcție");
            return node;
        }

        // Este variabilă?
        if (current[0] == L'$') {
            m_pos++;
            return std::make_shared<ASTNode>(ASTNodeType::Variable, current.substr(1));
        }

        // Altfel, este un literal (număr sau string)
        m_pos++;
        return std::make_shared<ASTNode>(ASTNodeType::Literal, current);
    }
    */

    ASTPtr parsePrimary() {
        std::wstring current = peek();
        if (current.empty()) return nullptr;

        // 1. Gestionare Variabile (Suportă atât "$a" cât și "$" urmat de "a")
        if (current[0] == L'$') {
            if (current == L"$") {
                m_pos++; // Consumăm primul "$"

                // Verificăm dacă urmează încă un "$" pentru DEREFERENCE ($$var)
                if (check(L"$")) {
                    ASTPtr node = std::make_shared<ASTNode>(ASTNodeType::Operator, L"DEREFERENCE");
                    node->addChild(parsePrimary()); // Recursivitate pentru multiple dereferențieri
                    return node;
                }

                // Variabilă normală (cazul în care token-ii sunt separați: ["$", "nume"])
                if (m_pos < m_tokens.size()) {
                    std::wstring name = m_tokens[m_pos++];
                    return std::make_shared<ASTNode>(ASTNodeType::Variable, name);
                }
                return nullptr; // Eroare: $ la final de linie
            }
            else {
                // Cazul în care variabila este un singur token lipit: ["$nume"]
                m_pos++;
                return std::make_shared<ASTNode>(ASTNodeType::Variable, current.substr(1));
            }
        }

        // 2. Structuri de date: Array [...] și Map {...}
        if (match({ L"[" })) return parseArray();
        if (match({ L"{" })) return parseMap();

        // 3. Paranteze (Grupare)
        if (match({ L"(" })) {
            ASTPtr node = parseCoalescing();
            consume(L")", "Asteptam inchiderea parantezei");
            return node;
        }

        // 4. Apeluri de funcții: nume(...)
        if (m_pos + 1 < m_tokens.size() && m_tokens[m_pos + 1] == L"(") {
            std::wstring funcName = m_tokens[m_pos++];
            m_pos++; // sărim peste '('
            ASTPtr node = std::make_shared<ASTNode>(ASTNodeType::FunctionCall, funcName);

            while (m_pos < m_tokens.size() && !check(L")")) {
                node->addChild(parseCoalescing());
                if (match({ L"," })) { /* continuăm la următorul argument */ }
            }
            consume(L")", "Asteptam ')' dupa functie");
            return node;
        }

        // 5. Literale (Numere sau String-uri "...")
        m_pos++;
        return std::make_shared<ASTNode>(ASTNodeType::Literal, current);
    }

    // Utilitare pentru deplasarea în lista de tokeni
    bool match(std::initializer_list<std::wstring> ops) {
        if (m_pos >= m_tokens.size()) return false;
        for (auto op : ops) {
            if (m_tokens[m_pos] == op) {
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
        if (!check(token)) throw std::runtime_error(error);
        m_pos++;
    }

    ASTPtr parseMap() {
        ASTPtr node = std::make_shared<ASTNode>(ASTNodeType::Literal, L"MAP_OBJECT");
        // Presupunem că am consumat deja '{'

        while (!check(L"}")) {
            // Citim cheia (ex: "hp")
            ASTPtr key = parsePrimary();

            // Consumăm ':' dintre cheie și valoare
            if (match({ L":" })) {
                // Citim valoarea (ex: 80)
                ASTPtr value = parseLogical();

                // Adăugăm ambele ca sub-noduri (perechi)
                node->addChild(key);
                node->addChild(value);
            }

            if (match({ L"," })) {
                continue; // Mergem la următoarea pereche
            }
            else {
                break; // Nu mai sunt virgule, ieșim
            }
        }
        consume(L"}", "Asteptam } la finalul obiectului");
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
    /*
    ASTPtr parsePostfix() {
        ASTPtr node = parsePrimary(); // Citesc variabila (ex: $player)

        while (match({ L"[" })) {
            ASTPtr indexNode = std::make_shared<ASTNode>(ASTNodeType::Operator, L"INDEX");
            indexNode->addChild(node);           // Sursa
            indexNode->addChild(parseLogical()); // Indexul (evaluat!)
            consume(L"]", "Lipseste ]");
            node = indexNode;
        }
        return node;
    }
    */

    ASTPtr parsePostfix() {
        ASTPtr node = parsePrimary(); // Citesște $config

        while (match({ L"[" })) {
            ASTPtr indexNode = std::make_shared<ASTNode>(ASTNodeType::Operator, L"INDEX");
            indexNode->addChild(node); // Adaugă $config ca sursă

            // REPARAȚIA: Folosește nivelul cel mai înalt de parsare pentru index!
            // În loc de parseLogical(), folosim parseCoalescing() (sau parse())
            indexNode->addChild(parseCoalescing());

            consume(L"]", "Lipseste ]");
            node = indexNode;
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

};

#endif