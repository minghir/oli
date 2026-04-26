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

   
    /*
    ASTPtr parseUnary() {
        if (match({ L"-", L"!", L"NOT" })) {
            std::wstring op = m_tokens[m_pos - 1];
            std::wstring internalOp = (op == L"-") ? L"UNARY_MINUS" : L"NOT";
            ASTPtr node = std::make_shared<ASTNode>(ASTNodeType::Operator, internalOp);
            node->addChild(parseUnary());
            return node;
        }
        return parsePostfix(); // Unary depinde de Postfix
    }
    */
    ASTPtr parseUnary() {
        // Adăugăm L"*" în lista de operatori unari
        if (match({ L"-", L"!", L"NOT", L"*" })) {
            std::wstring op = m_tokens[m_pos - 1];

            // Citim recursiv ce urmează după operator (permite **$ptr)
            ASTPtr child = parseUnary();

            if (op == L"*") {
                // Dacă ceea ce urmează este o variabilă, "lipim" asteriscul de ea
                // pentru ca resolveVariable să o poată procesa dintr-o bucată.
                if (child && child->type == ASTNodeType::Variable) {
                    child->value = L"*" + child->value;
                    return child;
                }

                // Dacă urmează o expresie complexă, ex: *(getPtr()), creăm un operator special
                ASTPtr node = std::make_shared<ASTNode>(ASTNodeType::Operator, L"DEREFERENCE");
                node->addChild(child);
                return node;
            }

            std::wstring internalOp = (op == L"-") ? L"UNARY_MINUS" : L"NOT";
            ASTPtr node = std::make_shared<ASTNode>(ASTNodeType::Operator, internalOp);
            node->addChild(child);
            return node;
        }
        return parsePostfix();
    }

    ASTPtr parsePower() {
        ASTPtr left = parseUnary(); // Power depinde de Unary (care duce la Postfix -> Primary)
        while (match({ L"^", L"**" })) {
            std::wstring op = m_tokens[m_pos - 1];
            ASTPtr node = std::make_shared<ASTNode>(ASTNodeType::Operator, op);
            node->addChild(left);
            // Puterea este de obicei asociativă la dreapta: 2^3^2 este 2^(3^2)
            node->addChild(parsePower());
            left = node;
        }
        return left;
    }
    
  
/*
ASTPtr parsePrimary() {
    std::wstring current = peek();
    if (current.empty()) return nullptr;

    // --- Identificăm dacă e variabilă (ORICÂTE SEMNE $ ARE) ---
    if (current[0] == L'$' || current[0] == L'@') {
        m_pos++;

        // Trimitem string-ul EXACT așa cum este (ex: "$$b" sau "$a")
        // NU mai facem .substr(1) aici, pentru că resolveVariable se ocupă de curățare
        return std::make_shared<ASTNode>(ASTNodeType::Variable, current);
    }

    // --- Restul (Literale, Paranteze, etc.) ---
    if (match({ L"[" })) return parseArray();
    if (match({ L"{" })) return parseMap();
    if (match({ L"(" })) {
        ASTPtr node = parseCoalescing();
        consume(L")", "Lipseste )");
        return node;
    }

    m_pos++;
    return std::make_shared<ASTNode>(ASTNodeType::Literal, current);
}
*/

    ASTPtr parsePrimary() {
        std::wstring current = peek();
        if (current.empty()) return nullptr;

        // 1. Variabile
        if (current[0] == L'$' || current[0] == L'@') {
            m_pos++;
            return std::make_shared<ASTNode>(ASTNodeType::Variable, current);
        }

        // 2. Structuri (match consumă automat token-ul, deci e ok)
        if (match({ L"[" })) return parseArray();
        if (match({ L"{" })) return parseMap();
        if (match({ L"(" })) {
            ASTPtr node = parseCoalescing();
            consume(L")", "Lipseste )");
            return node;
        }

        // 3. Bariera (NU consumăm, doar verificăm)
        if (current == L"}" || current == L"]" || current == L")" ||
            current == L"," || current == L":") {
            return nullptr;
        }

        // 4. Literale (Dacă am ajuns aici, e sigur un număr sau string)
        // IMPORTANT: Folosim un token "proaspăt" de la m_pos
        std::wstring literalValue = m_tokens[m_pos];
        m_pos++;
        return std::make_shared<ASTNode>(ASTNodeType::Literal, literalValue);
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

    /*
    void consume(std::wstring token, std::string error) {
        if (!check(token)) {
            std::wstring found = (m_pos < m_tokens.size()) ? m_tokens[m_pos] : L"EOF";
            std::string fullError = error + " (Gasit: '" + std::string(found.begin(), found.end()) + "' in loc de '" + std::string(token.begin(), token.end()) + "')";
            throw std::runtime_error(fullError);
        }
        m_pos++;
    }
    */

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


    /*
    ASTPtr parseMap() {
        // Presupunem că '{' a fost deja consumat de match() în parsePrimary
        ASTPtr node = std::make_shared<ASTNode>(ASTNodeType::Literal, L"MAP_OBJECT");

        // Gestionăm cazul map-ului gol: {}
        if (check(L"}")) {
            m_pos++; // Consumăm '}'
            return node;
        }

        while (m_pos < m_tokens.size()) {
            // 1. Citim cheia. Folosim parsePrimary pentru că o cheie de map 
            // este de obicei un literal (string/număr) sau o variabilă.
            ASTPtr key = parsePrimary();
            if (!key) break;

            // 2. Consumăm neapărat ":"
            if (!match({ L":" })) {
                throw std::runtime_error("Eroare Map: Se astepta ':' dupa cheie la pozitia " + std::to_string(m_pos));
            }

            // 3. Citim valoarea. Folosim parseCoalescing pentru a permite 
            // orice expresie complexă ca valoare (inclusiv alte map-uri sau array-uri).
            ASTPtr value = parseCoalescing();
            if (!value) throw std::runtime_error("Eroare Map: Lipseste valoarea dupa ':'");

            node->addChild(key);
            node->addChild(value);

            // 4. Gestionare virgulă sau închidere
            if (match({ L"," })) {
                // Permitem "trailing comma" (virgulă după ultimul element, ex: {a:1,})
                if (check(L"}")) break;
                continue;
            }
            else {
                // Dacă nu e virgulă, TREBUIE să fie sfarsitul map-ului
                break;
            }
        }

        consume(L"}", "Asteptam '}' la finalul obiectului Map");
        return node;
    }
    */
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
    // Începem cu unitatea de bază ($c1, "nume", 100, etc.)
    ASTPtr node = parsePrimary();
    if (!node) return nullptr;
    // Bucla permite înlănțuiri: $c1.locatie.x sau $arr[0].nume
    while (true) {

        // --- 1. Operatorul DOT (Acces membru: $obj.prop) ---
        if (match({ L"." })) {
            // Ne asigurăm că după punct urmează un nume de câmp valid
            if (m_pos >= m_tokens.size()) {
                throw std::runtime_error("Eroare Sintaxa: Se astepta un nume de camp dupa '.'");
            }

            std::wstring fieldName = m_tokens[m_pos++];
            // Validare opțională: poți verifica dacă fieldName nu începe cu '$' 
            // deoarece membrii structurilor în Oli sunt identificatori simpli.

            ASTPtr dotNode = std::make_shared<ASTNode>(ASTNodeType::Operator, L"DOT");
            dotNode->addChild(node); // Stânga: obiectul (ex: nodul care reprezintă $c1.locatie)

            // Dreapta: numele câmpului ca Literal
            dotNode->addChild(std::make_shared<ASTNode>(ASTNodeType::Literal, fieldName));

            node = dotNode; // Noul nod devine baza pentru următoarea iterație
        }

        // --- 2. INDEXARE (Acces array/map: $arr[index]) ---
        else if (match({ L"[" })) {
            ASTPtr indexNode = std::make_shared<ASTNode>(ASTNodeType::Operator, L"INDEX");
            indexNode->addChild(node); // Containerul ($arr)

            // Permitem orice expresie pentru index (ex: $i + 1)
            indexNode->addChild(parseCoalescing());

            consume(L"]", "Lipseste ']' la inchiderea indexului.");
            node = indexNode;
        }

        // --- 3. APEL DINAMIC ($var() sau $obj.metoda()) ---
        else if (match({ L"(" })) { // <-- Aici a fost eroarea (eliminat () din interior)
            ASTPtr callNode = std::make_shared<ASTNode>(ASTNodeType::FunctionCall, L"DYNAMIC_CALL");
            callNode->addChild(node);

            if (!check(L")")) {
                do {
                    callNode->addChild(parseCoalescing());
                } while (match({ L"," }));
            }

            consume(L")", "Asteptam ')' pentru inchiderea apelului.");
            node = callNode;
        }

        // --- 4. POSTFIX INCREMENT/DECREMENT ($i++) ---
        else if (match({ L"++", L"--" })) {
            std::wstring op = m_tokens[m_pos - 1];
            std::wstring internalOp = (op == L"++") ? L"POSTFIX_INC" : L"POSTFIX_DEC";

            ASTPtr postfixNode = std::make_shared<ASTNode>(ASTNodeType::Operator, internalOp);
            postfixNode->addChild(node);
            node = postfixNode;

            // Postfix-ul de obicei nu mai permite continuarea accesului în Oli
            break;
        }

        else {
            break; // Nu mai sunt operatori postfix
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

};

#endif