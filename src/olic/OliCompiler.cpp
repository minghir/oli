#include "../vDataSerialize.hpp"
#include "OliCompiler.hpp"
#include "../OliCommandParser.hpp"
#include "../StringUtils.hpp"
#include "../PortTools.hpp"
#include <iostream>

OliChunk OliCompiler::compile(const std::wstring& source) {
    OliChunk chunk;
    std::wstringstream ss(source);
    std::wstring line;
    std::wstring commandBuffer = L"";
    int nestingLevel = 0;
    int bracketDepth = 0; // <--- ADAUGAT: Monitorizare paranteze

    while (std::getline(ss, line)) {
        std::wstring cleanLine = trim(line);
        if (cleanLine.empty()) continue;

        // --- MASCARE GHILIMELE (Logica împrumutată din interpretor) ---
        bool inQuotes = false;
        std::wstring maskedLine = cleanLine;
        for (size_t i = 0; i < maskedLine.length(); ++i) {
            if (maskedLine[i] == L'"' && (i == 0 || maskedLine[i - 1] != L'\\')) {
                inQuotes = !inQuotes;
                maskedLine[i] = L' ';
                continue;
            }
            if (inQuotes) maskedLine[i] = L' ';
        }

        // --- DETECTARE COMENTARII ---
        size_t commentPos = maskedLine.find(L'#');
        if (commentPos != std::wstring::npos) {
            cleanLine = trim(cleanLine.substr(0, commentPos));
            maskedLine = trim(maskedLine.substr(0, commentPos));
        }
        if (cleanLine.empty()) continue;

        // --- ACTUALIZARE ADÂNCIME BLOCURI ȘI PARANTEZE ---
        std::wstring upperLine = to_upper(maskedLine);

        // Blocuri (IF, WHILE, etc.)
        if (upperLine.find(L"IF ") == 0 || upperLine.find(L"WHILE ") == 0 ||
            upperLine.find(L"REPEAT") == 0 || upperLine.find(L"FOR ") == 0 ||
            upperLine.find(L"PROC ") == 0 || upperLine.find(L"FUNC ") == 0) {
            nestingLevel++;
        }
        if (upperLine.find(L"ENDIF") != std::wstring::npos ||
            upperLine.find(L"ENDWHILE") != std::wstring::npos ||
            upperLine.find(L"ENDREPEAT") != std::wstring::npos ||
            upperLine.find(L"ENDFOR") != std::wstring::npos ||
            upperLine.find(L"ENDPROC") != std::wstring::npos ||
            upperLine.find(L"ENDFUNC") != std::wstring::npos) {
            nestingLevel--;
        }

        // Paranteze (Array/Map)
        for (wchar_t c : maskedLine) {
            if (c == L'{' || c == L'[') bracketDepth++;
            if (c == L'}' || c == L']') bracketDepth--;
        }

        commandBuffer += L" " + cleanLine;

        // --- DECIZIA DE COMPILARE ---
        // Compilăm doar când nestingLevel ȘI bracketDepth sunt 0
        if (nestingLevel == 0 && bracketDepth == 0) {
            std::vector<std::wstring> statements = splitWBySemicolon(commandBuffer);
            for (const auto& stmt : statements) {
                if (trim(stmt).empty()) continue;
                ShellCommand sc = vOliCommandParser::parse(stmt);
                compileStatement(sc, chunk);
            }
            commandBuffer = L"";
        }
    }

    // Verificare de siguranță la final
    if (bracketDepth != 0) {
        throw std::runtime_error("Eroare Compilare: Paranteze neinchise la finalul fisierului.");
    }

    chunk.addByte((uint8_t)OpCode::OP_RETURN, 0);
    return chunk;
}
void OliCompiler::compileStatement(const ShellCommand& sc, OliChunk& chunk) {
   // ShellCommand sc = vOliCommandParser::parse(line);

    // Exemplu: SET x = 10
    std::wstring cmdName = to_upper(sc.name);
    
    if (cmdName == L"SET") {
        if (sc.args.empty()) return;

        // --- REPARARE: Pasăm TOATE argumentele parserului ---
        OliExpressionParser exprParser(sc.args);
        ASTPtr exprAST = exprParser.parse();

        if (exprAST) {
            // generateFromAST va detecta nodul de atribuire (=, +=) 
            // și va gestiona singur calculul și salvarea.
            generateFromAST(exprAST, chunk);
        }
    }
    // Exemplu: ECHO x
    else if (cmdName == L"ECHO") {
        if (sc.args.empty()) {
            emitConstant(vData(L""), chunk, 0);
            chunk.addByte((uint8_t)OpCode::OP_ECHO, 0);
        }
        else {
            // Parsăm toate argumentele ca o singură expresie (suportă concatenări)
            OliExpressionParser exprParser(sc.args);
            ASTPtr exprAST = exprParser.parse();

            if (exprAST) {
                generateFromAST(exprAST, chunk);
                chunk.addByte((uint8_t)OpCode::OP_ECHO, 0);
            }
        }
    }

    // Exemplu: UNSET $user.id sau UNSET $matrix[0][1]
    else if (cmdName == L"UNSET") {
        if (sc.args.empty()) return;

        // 1. Reconstruim calea completă din argumente (ex: ["$", "user", ".", "id"] -> "$user.id")
        std::wstring fullPath = L"";
        for (const auto& arg : sc.args) {
            fullPath += arg;
        }

        if (!fullPath.empty()) {
            // 2. Înregistrăm calea ca o constantă în tabel
            uint16_t pathIdx = chunk.addConstant(vData(fullPath));

            // 3. Emitem OpCode-ul OP_UNSET urmat de indexul pe 2 bytes
            // (Folosim 0 pentru linie momentan, sau sc.line dacă ai această info)
            chunk.addByte((uint8_t)OpCode::OP_UNSET, 0);
            chunk.addByte((uint8_t)((pathIdx >> 8) & 0xFF), 0);
            chunk.addByte((uint8_t)(pathIdx & 0xFF), 0);
        }
    }

    // Exemplu: PLUGIN "math_ext" sau PLUGIN "C:/libs/my_plugin.dll"
    else if (cmdName == L"PLUGIN") {
        if (sc.args.empty()) return;

        std::wstring path = L"";
        for (const auto& arg : sc.args) path += arg;

        if (!path.empty()) {
            // 1. Bytecode pentru VM (ca să încarce plugin-ul la execuție)
            uint16_t pathIdx = chunk.addConstant(vData(path));
            chunk.addByte((uint8_t)OpCode::OP_PLUGIN, 0);
            chunk.addByte((uint8_t)((pathIdx >> 8) & 0xFF), 0);
            chunk.addByte((uint8_t)(pathIdx & 0xFF), 0);

            // 2. Metadata pentru Compilator (ca să știe noile cuvinte cheie)
            this->loadPluginMetadata(path);
        }
    }

    else if (cmdName == L"IF") {
        int thenIdx = -1, elseIdx = -1, endifIdx = -1;
        int depth = 0;

        // 1. Identificăm marcatorii locali (ignorăm ce e în interiorul altor IF-uri)
        for (int i = 0; i < (int)sc.args.size(); ++i) {
            std::wstring argUpper = to_upper(sc.args[i]);

            if (argUpper == L"IF") {
                depth++;
            }
            else if (argUpper == L"ENDIF") {
                if (depth > 0) depth--;
                else if (endifIdx == -1) endifIdx = i;
            }
            else if (depth == 0) {
                if (argUpper == L"THEN") thenIdx = i;
                else if (argUpper == L"ELSE") elseIdx = i;
            }
        }

        // Verificare: dacă nu am găsit un ENDIF pe linie, folosim finalul listei de argumente
        if (endifIdx == -1) endifIdx = (int)sc.args.size();

        // Siguranță minimă: IF [cond] THEN
        if (thenIdx == -1 || sc.args.size() < 4) return;

        // 2. Compilăm condiția
        emitLoadOrConstant(sc.args[0], chunk);
        emitLoadOrConstant(sc.args[2], chunk);

        if (sc.args[1] == L">") chunk.addByte((uint8_t)OpCode::OP_GREATER, 0);
        else if (sc.args[1] == L"<") chunk.addByte((uint8_t)OpCode::OP_LESS, 0);
        else if (sc.args[1] == L"==") chunk.addByte((uint8_t)OpCode::OP_EQUAL, 0);

        // 3. JUMP_IF_FALSE (către ELSE sau FINAL)
        chunk.addByte((uint8_t)OpCode::OP_JUMP_IF_FALSE, 0);
        size_t jumpToElseAddr = chunk.code.size();
        chunk.addByte(0, 0); chunk.addByte(0, 0);

        // 4. Compilăm blocul THEN
        int thenEnd = (elseIdx != -1) ? elseIdx : endifIdx;
        if (thenIdx + 1 < thenEnd) {
            ShellCommand thenCmd;
            thenCmd.name = sc.args[thenIdx + 1];
            for (int i = thenIdx + 2; i < thenEnd; ++i) {
                thenCmd.args.push_back(sc.args[i]);
            }
            compileStatement(thenCmd, chunk);
        }

        // 5. JUMP peste ELSE (doar dacă avem ramură ELSE)
        size_t jumpToEndAddr = 0;
        bool hasElse = (elseIdx != -1);
        if (hasElse) {
            chunk.addByte((uint8_t)OpCode::OP_JUMP, 0);
            jumpToEndAddr = chunk.code.size();
            chunk.addByte(0, 0); chunk.addByte(0, 0);
        }

        // 6. Backpatching: Unde duce condiția FALSE
        uint16_t distToElse = (uint16_t)(chunk.code.size() - (jumpToElseAddr + 2));
        chunk.code[jumpToElseAddr] = (uint8_t)(distToElse >> 8);
        chunk.code[jumpToElseAddr + 1] = (uint8_t)(distToElse & 0xFF);

        // 7. Compilăm blocul ELSE
        if (hasElse && (elseIdx + 1) < endifIdx) {
            ShellCommand elseCmd;
            elseCmd.name = sc.args[elseIdx + 1];
            for (int i = elseIdx + 2; i < endifIdx; ++i) {
                elseCmd.args.push_back(sc.args[i]);
            }
            compileStatement(elseCmd, chunk);

            // Backpatching: Unde duce saltul de peste ELSE
            uint16_t distToEnd = (uint16_t)(chunk.code.size() - (jumpToEndAddr + 2));
            chunk.code[jumpToEndAddr] = (uint8_t)(distToEnd >> 8);
            chunk.code[jumpToEndAddr + 1] = (uint8_t)(distToEnd & 0xFF);
        }
    }

    else if (cmdName == L"WHILE") {
        int doIdx = -1;
        int endWhileIdx = -1;

        // 1. Găsim DO-ul buclei curent
        for (int i = 0; i < (int)sc.args.size(); ++i) {
            if (to_upper(sc.args[i]) == L"DO") {
                doIdx = i;
                break;
            }
        }

        // 2. Găsim ENDWHILE-ul CORECT (balansat)
        int counter = 1;
        for (int i = doIdx + 1; i < (int)sc.args.size(); ++i) {
            std::wstring argUpper = to_upper(sc.args[i]);
            if (argUpper == L"WHILE") counter++;
            if (argUpper == L"ENDWHILE") {
                counter--;
                if (counter == 0) {
                    endWhileIdx = i;
                    break;
                }
            }
        }

        if (doIdx == -1 || endWhileIdx == -1) return;

        // 3. Punctul de întoarcere (Loop Start)
        size_t loopStart = chunk.code.size();

        // 4. Compilăm condiția (ex: $i < $limit)
        emitLoadOrConstant(sc.args[0], chunk);
        emitLoadOrConstant(sc.args[2], chunk);

        if (sc.args[1] == L">")       chunk.addByte((uint8_t)OpCode::OP_GREATER, 0);
        else if (sc.args[1] == L"<")  chunk.addByte((uint8_t)OpCode::OP_LESS, 0);
        else if (sc.args[1] == L"==") chunk.addByte((uint8_t)OpCode::OP_EQUAL, 0);

        // 5. Saltul de ieșire (OP_JUMP_IF_FALSE)
        chunk.addByte((uint8_t)OpCode::OP_JUMP_IF_FALSE, 0);
        size_t exitJumpPatchAddr = chunk.code.size();
        chunk.addByte(0, 0); chunk.addByte(0, 0); // Placeholder offset

        // 6. PROCESAREA CORPULUI (Suport pentru imbricare)
        int i = doIdx + 1;
        while (i < endWhileIdx) {
            ShellCommand bCmd;
            bCmd.name = sc.args[i];
            std::wstring uName = to_upper(bCmd.name);
            i++;

            if (uName == L"WHILE" || uName == L"IF") {
                // Dacă e un bloc, colectăm tot până la închiderea lui corectă
                int subCounter = 1;
                std::wstring endTag = (uName == L"WHILE") ? L"ENDWHILE" : L"ENDIF";

                while (i < endWhileIdx && subCounter > 0) {
                    std::wstring uArg = to_upper(sc.args[i]);
                    if (uArg == uName) subCounter++;
                    if (uArg == endTag) subCounter--;
                    bCmd.args.push_back(sc.args[i]);
                    i++;
                }
            }
            else {
                // Comandă simplă (SET, ECHO), colectăm până la următorul cuvânt cheie
                while (i < endWhileIdx) {
                    std::wstring uArg = to_upper(sc.args[i]);
                    if (uArg == L"SET" || uArg == L"ECHO" || uArg == L"IF" || uArg == L"WHILE") break;
                    bCmd.args.push_back(sc.args[i]);
                    i++;
                }
            }

            if (!bCmd.name.empty()) compileStatement(bCmd, chunk);
        }

        // 7. OP_LOOP (Salt înapoi la condiție)
        chunk.addByte((uint8_t)OpCode::OP_LOOP, 0);
        uint16_t loopOffset = (uint16_t)(chunk.code.size() + 2 - loopStart);
        chunk.addByte((uint8_t)(loopOffset >> 8), 0);
        chunk.addByte((uint8_t)(loopOffset & 0xFF), 0);

        // 8. Backpatching: Scriem adresa de ieșire în JUMP_IF_FALSE
        uint16_t exitOffset = (uint16_t)(chunk.code.size() - (exitJumpPatchAddr + 2));
        chunk.code[exitJumpPatchAddr] = (uint8_t)(exitOffset >> 8);
        chunk.code[exitJumpPatchAddr + 1] = (uint8_t)(exitOffset & 0xFF);
        }


    else if (cmdName == L"REPEAT") {
        int untilIdx = -1;
        int endRepeatIdx = -1;

        // 1. Identificăm UNTIL și ENDREPEAT la nivelul curent
        int counter = 1;
        for (int i = 0; i < (int)sc.args.size(); ++i) {
            std::wstring argUpper = to_upper(sc.args[i]);
            if (argUpper == L"REPEAT") counter++;
            else if (argUpper == L"ENDREPEAT") {
                counter--;
                if (counter == 0) { endRepeatIdx = i; break; }
            }
            else if (counter == 1 && argUpper == L"UNTIL") {
                untilIdx = i;
            }
        }

        if (untilIdx == -1 || endRepeatIdx == -1) return;

        // 2. Loop Start (adresa primei instrucțiuni din REPEAT)
        size_t loopStart = chunk.code.size();

        // 3. COMPILAREA CORPULUI (Până la UNTIL)
        int i = 0;
        while (i < untilIdx) {
            ShellCommand bCmd;
            bCmd.name = sc.args[i];
            std::wstring uName = to_upper(bCmd.name);
            i++;

            if (uName == L"WHILE" || uName == L"IF" || uName == L"REPEAT") {
                int subCounter = 1;
                std::wstring endTag = (uName == L"WHILE") ? L"ENDWHILE" : (uName == L"IF" ? L"ENDIF" : L"ENDREPEAT");
                while (i < untilIdx && subCounter > 0) {
                    std::wstring uArg = to_upper(sc.args[i]);
                    if (uArg == uName) subCounter++;
                    if (uArg == endTag) subCounter--;
                    bCmd.args.push_back(sc.args[i]);
                    i++;
                }
            }
            else {
                // CRITIC: Adăugăm UNTIL în lista de stop pentru a nu fi "înghițit"
                while (i < untilIdx) {
                    std::wstring uArg = to_upper(sc.args[i]);
                    if (uArg == L"SET" || uArg == L"ECHO" || uArg == L"IF" ||
                        uArg == L"WHILE" || uArg == L"REPEAT" || uArg == L"UNTIL") break;
                    bCmd.args.push_back(sc.args[i]);
                    i++;
                }
            }
            if (!bCmd.name.empty()) compileStatement(bCmd, chunk);
        }

        // 4. COMPILAREA CONDIȚIEI (Evaluarea are loc DUPĂ corp)
    // Ne asigurăm că avem cel puțin 3 token-uri după UNTIL ($a == $b)
            if (untilIdx != -1 && untilIdx + 3 < (int)sc.args.size()) {
                emitLoadOrConstant(sc.args[untilIdx + 1], chunk); // LHS
                emitLoadOrConstant(sc.args[untilIdx + 3], chunk); // RHS

                std::wstring op = sc.args[untilIdx + 2];
                if (op == L">")       chunk.addByte((uint8_t)OpCode::OP_GREATER, 0);
                else if (op == L"<")  chunk.addByte((uint8_t)OpCode::OP_LESS, 0);
                else if (op == L"==") chunk.addByte((uint8_t)OpCode::OP_EQUAL, 0);

                // 5. LOGICA DE JUMP
                // Dacă e TRUE (condiția UNTIL e gata), sărim peste OP_LOOP
                chunk.addByte((uint8_t)OpCode::OP_JUMP_IF_TRUE, 0);
                size_t patchAddr = chunk.code.size();
                chunk.addByte(0, 0); chunk.addByte(0, 0);

                // Dacă e FALSE, facem LOOP înapoi la loopStart
                chunk.addByte((uint8_t)OpCode::OP_LOOP, 0);
                uint16_t loopOffset = (uint16_t)(chunk.code.size() + 2 - loopStart);
                chunk.addByte((uint8_t)(loopOffset >> 8), 0);
                chunk.addByte((uint8_t)(loopOffset & 0xFF), 0);

                // Backpatching: Scriem adresa de ieșire
                uint16_t exitOffset = (uint16_t)(chunk.code.size() - (patchAddr + 2));
                chunk.code[patchAddr] = (uint8_t)(exitOffset >> 8);
                chunk.code[patchAddr + 1] = (uint8_t)(exitOffset & 0xFF);
            }
            else {
                std::wcout << L"[DEBUG] REPEAT condition skipped! Size check failed." << std::endl;
            }
        }

    else if (cmdName == L"FOR") {
        int toIdx = -1, byIdx = -1, doIdx = -1, endForIdx = -1;
        int counter = 1;

        // 1. Identificăm pozițiile cuvintelor cheie (balansat)
        for (int i = 0; i < (int)sc.args.size(); ++i) {
            std::wstring argU = to_upper(sc.args[i]);
            if (argU == L"FOR") counter++;
            else if (argU == L"ENDFOR") {
                counter--;
                if (counter == 0) { endForIdx = i; break; }
            }

            if (counter == 1) {
                if (argU == L"TO") toIdx = i;
                else if (argU == L"BY") byIdx = i;
                else if (argU == L"DO") doIdx = i;
            }
        }

        if (toIdx == -1 || doIdx == -1 || endForIdx == -1) return;

        std::wstring varName = sc.args[0];   // $i
        std::wstring startVal = sc.args[2];  // valoare start
        std::wstring limitVal = sc.args[toIdx + 1];
        std::wstring stepVal = (byIdx != -1) ? sc.args[byIdx + 1] : L"1";

        // 2. INIȚIALIZARE: SET $i = startVal
        emitLoadOrConstant(startVal, chunk);
        chunk.addByte((uint8_t)OpCode::OP_SET_GLOBAL, 0);
        uint16_t nameIdx = chunk.addConstant(vData(varName));
        chunk.addByte((uint8_t)(nameIdx >> 8), 0);
        chunk.addByte((uint8_t)(nameIdx & 0xFF), 0);

        // --- CRITIC: LOOP START ---
        // Punctul de întoarcere trebuie să fie EXACT la încărcarea variabilei pentru test
        size_t loopStart = chunk.code.size();

        // 3. CONDIȚIA: EXIT dacă $i > limitVal
        // Re-încărcăm $i pe stivă la fiecare iterație
        emitLoadOrConstant(varName, chunk);
        emitLoadOrConstant(limitVal, chunk);
        chunk.addByte((uint8_t)OpCode::OP_GREATER, 0);

        chunk.addByte((uint8_t)OpCode::OP_JUMP_IF_TRUE, 0);
        size_t exitJumpAddr = chunk.code.size();
        chunk.addByte(0, 0); chunk.addByte(0, 0);

        // 4. COMPILARE CORP (Între DO și ENDFOR)
        int i = doIdx + 1;
        while (i < endForIdx) {
            ShellCommand bCmd;
            bCmd.name = sc.args[i++];
            std::wstring uName = to_upper(bCmd.name);

            if (uName == L"WHILE" || uName == L"IF" || uName == L"FOR" || uName == L"REPEAT") {
                int subCounter = 1;
                std::wstring endTag = (uName == L"WHILE") ? L"ENDWHILE" : (uName == L"IF" ? L"ENDIF" : (uName == L"FOR" ? L"ENDFOR" : L"ENDREPEAT"));
                while (i < endForIdx && subCounter > 0) {
                    std::wstring uArg = to_upper(sc.args[i]);
                    if (uArg == uName) subCounter++;
                    if (uArg == endTag) subCounter--;
                    bCmd.args.push_back(sc.args[i++]);
                }
            }
            else {
                while (i < endForIdx) {
                    std::wstring uArg = to_upper(sc.args[i]);
                    // Adăugăm și UNTIL/TO/BY/DO în stop-list pentru siguranță, deși parserul ar trebui să le ignore
                    if (uArg == L"SET" || uArg == L"ECHO" || uArg == L"IF" || uArg == L"WHILE" || uArg == L"FOR" || uArg == L"REPEAT") break;
                    bCmd.args.push_back(sc.args[i++]);
                }
            }
            if (!bCmd.name.empty()) compileStatement(bCmd, chunk);
        }

        // 5. INCREMENTARE (Pasul): $i = $i + stepVal
        emitLoadOrConstant(varName, chunk); // Luăm valoarea curentă
        emitLoadOrConstant(stepVal, chunk);  // Luăm pasul
        chunk.addByte((uint8_t)OpCode::OP_ADD, 0);

        // Salvăm înapoi în variabilă
        chunk.addByte((uint8_t)OpCode::OP_SET_GLOBAL, 0);
        chunk.addByte((uint8_t)(nameIdx >> 8), 0);
        chunk.addByte((uint8_t)(nameIdx & 0xFF), 0);

        // 6. JUMP ÎNAPOI la loopStart (care este OP_GET_GLOBAL $i de la pasul 3)
        chunk.addByte((uint8_t)OpCode::OP_LOOP, 0);
        uint16_t loopOffset = (uint16_t)(chunk.code.size() + 2 - loopStart);
        chunk.addByte((uint8_t)(loopOffset >> 8), 0);
        chunk.addByte((uint8_t)(loopOffset & 0xFF), 0);

        // 7. BACKPATCHING EXIT
        uint16_t exitOffset = (uint16_t)(chunk.code.size() - (exitJumpAddr + 2));
        chunk.code[exitJumpAddr] = (uint8_t)(exitOffset >> 8);
        chunk.code[exitJumpAddr + 1] = (uint8_t)(exitOffset & 0xFF);
        }
}

void OliCompiler::emitConstant(const vData& value, OliChunk& chunk, int line) {
    uint16_t idx = chunk.addConstant(value);
    chunk.addByte((uint8_t)OpCode::OP_CONSTANT, line);
    chunk.addByte((uint8_t)(idx >> 8), line);
    chunk.addByte((uint8_t)(idx & 0xFF), line);
}


void OliCompiler::emitLoadOrConstant(const std::wstring& arg, OliChunk& chunk) {
    if (arg.empty()) return;

    if (arg == L"true" || arg == L"false") {
        emitConstant(vData(arg == L"true"), chunk, 0);
        return;
    }

    // 0. GESTIONARE NULL / MONOSTATE
    if (arg == L"NULL" || arg == L"null" || arg == L"monostate") {
        emitConstant(vData(std::monostate{}), chunk, 0);
        return;
    }

    // 1. LITERAL STRING
    if (arg.size() >= 2 && arg.front() == L'\"' && arg.back() == L'\"') {
        std::wstring cleanStr = arg.substr(1, arg.size() - 2);
        emitConstant(vData(cleanStr), chunk, 0);
        return;
    }

    // 2. LITERAL NUMĂR
    if (std::iswdigit(arg[0]) || (arg.size() > 1 && arg[0] == L'-' && std::iswdigit(arg[1]))) {
        double val = std::stod(arg);
        emitConstant(vData(val), chunk, 0);
        return;
    }

    // 3. DEREFERENȚIERE POINTER
    if (arg[0] == L'*') {
        emitLoadOrConstant(arg.substr(1), chunk);
        chunk.addByte((uint8_t)OpCode::OP_GET_INDIRECT, 0);
        return;
    }

    // 4. VARIABILE (Cu prefix $ sau @)
    if (arg[0] == L'$' || arg[0] == L'@') {
        size_t dollarCount = 0;
        while (dollarCount < arg.size() && (arg[dollarCount] == L'$' || arg[dollarCount] == L'@')) {
            dollarCount++;
        }

        std::wstring baseName = arg.substr(dollarCount - 1); // Păstrăm un singur prefix ($ sau @)
        chunk.addByte((uint8_t)OpCode::OP_GET_GLOBAL, 0);
        uint16_t nameIdx = chunk.addConstant(vData(baseName));
        chunk.addByte((uint8_t)(nameIdx >> 8), 0);
        chunk.addByte((uint8_t)(nameIdx & 0xFF), 0);

        for (size_t i = 1; i < dollarCount; ++i) {
            chunk.addByte((uint8_t)OpCode::OP_GET_INDIRECT, 0);
        }
        return;
    }

    // 5. CAZ DEFAULT: Eroare sau tratare ca variabilă fără prefix
    // Dacă ajungem aici cu "++", înseamnă că parserul a greșit, 
    // dar cel puțin nu mai emitem cod pentru NULL.
    chunk.addByte((uint8_t)OpCode::OP_GET_GLOBAL, 0);
    uint16_t nameIdx = chunk.addConstant(vData(arg));
    chunk.addByte((uint8_t)(nameIdx >> 8), 0);
    chunk.addByte((uint8_t)(nameIdx & 0xFF), 0);
}


std::wstring OliCompiler::rebuildSubCommand(const std::vector<std::wstring>& args, size_t start, size_t end) {
    std::wstring result = L"";
    for (size_t i = start; i < end; ++i) {
        result += args[i];
        if (i < end - 1) result += L" ";
    }
    return result;
}

void OliCompiler::optimize(OliChunk& chunk) {
    if (chunk.code.size() < 3) return; // Prea mic pentru orice optimizare

    for (size_t i = 0; i < chunk.code.size(); ) {
        uint8_t current = chunk.code[i];

        // --- OPTIMIZAREA 1: JUMP la adresa imediat următoare ---
        if (current == (uint8_t)OpCode::OP_JUMP && (i + 2 < chunk.code.size())) {
            uint16_t offset = (chunk.code[i + 1] << 8) | chunk.code[i + 2];
            if (offset == 0) {
                // Ștergem JUMP-ul (3 bytes: opcode + 2 bytes offset)
                chunk.code.erase(chunk.code.begin() + i, chunk.code.begin() + i + 3);
                // Nu incrementăm i, verificăm noua instrucțiune care a ajuns la acest index
                continue;
            }
        }

        // --- OPTIMIZAREA 2: SET urmat de GET pe aceeași variabilă ---
        if (current == (uint8_t)OpCode::OP_SET_GLOBAL && (i + 5 < chunk.code.size())) {
            // Verificăm dacă următoarea instrucțiune (după cei 3 bytes ai SET) este un GET
            if (chunk.code[i + 3] == (uint8_t)OpCode::OP_GET_GLOBAL) {
                uint16_t setVarIdx = (chunk.code[i + 1] << 8) | chunk.code[i + 2];
                uint16_t getVarIdx = (chunk.code[i + 4] << 8) | chunk.code[i + 5];

                if (setVarIdx == getVarIdx) {
                    // 1. Înlocuim SET cu DUP (păstrăm valoarea pe stivă)
                    chunk.code[i] = (uint8_t)OpCode::OP_DUP;

                    // 2. Mutăm SET-ul imediat după DUP
                    chunk.code[i + 1] = (uint8_t)OpCode::OP_SET_GLOBAL;
                    chunk.code[i + 2] = (uint8_t)(setVarIdx >> 8);
                    chunk.code[i + 3] = (uint8_t)(setVarIdx & 0xFF);

                    // 3. Ștergem restul de 2 bytes rămași de la vechiul GET_GLOBAL
                    chunk.code.erase(chunk.code.begin() + i + 4, chunk.code.begin() + i + 6);

                    // Continuăm verificarea de la noul SET (poate urmează altceva de optimizat)
                    continue;
                }
            }
        }

        i++; // Mergem mai departe doar dacă nu am făcut nicio ștergere
    }
}


void OliCompiler::generateFromAST(ASTPtr node, OliChunk& chunk) {
    if (!node) return;

    // --- 1. ATRIBUIRE (=, +=, -=, *=, /=) ---
    if (node->type == ASTNodeType::Operator && (node->value == L"=" ||
        node->value == L"+=" || node->value == L"-=" ||
        node->value == L"*=" || node->value == L"/=")) {

        std::wstring rawLHS = reconstructRawName(node->children[0]);

        if (node->value == L"=") {
            generateFromAST(node->children[1], chunk);
            emitStore(rawLHS, chunk);
            return;
        }

        generateFromAST(node->children[0], chunk);
        generateFromAST(node->children[1], chunk);

        if (node->value == L"+=")      chunk.addByte((uint8_t)OpCode::OP_ADD, 0);
        else if (node->value == L"-=") chunk.addByte((uint8_t)OpCode::OP_SUB, 0);
        else if (node->value == L"*=") chunk.addByte((uint8_t)OpCode::OP_MUL, 0);
        else if (node->value == L"/=") chunk.addByte((uint8_t)OpCode::OP_DIV, 0);

        emitStore(rawLHS, chunk);
        return;
    }

    // --- 2. OPERATORI UNARI ---
    if (node->type == ASTNodeType::Operator && node->children.size() == 1) {
        if (node->value == L"ADDRESS_OF" || node->value == L"&") {
            std::wstring varName = node->children[0]->value;
            uint16_t nameIdx = chunk.addConstant(vData(varName));
            chunk.addByte((uint8_t)OpCode::OP_GET_ADDR, 0);
            chunk.addByte((nameIdx >> 8) & 0xFF, 0);
            chunk.addByte(nameIdx & 0xFF, 0);
            return;
        }

        generateFromAST(node->children[0], chunk);

        if (node->value == L"UNARY_MINUS")      chunk.addByte((uint8_t)OpCode::OP_NEGATE, 0);
        else if (node->value == L"NOT")         chunk.addByte((uint8_t)OpCode::OP_LOGICAL_NOT, 0);
        else if (node->value == L"BITWISE_NOT") chunk.addByte((uint8_t)OpCode::OP_BNOT, 0);
        else if (node->value == L"DEREFERENCE") chunk.addByte((uint8_t)OpCode::OP_GET_INDIRECT, 0);
        else if (node->value == L"POSTFIX_INC") {
            chunk.addByte((uint8_t)OpCode::OP_DUP, 0);
            emitConstant(vData(1.0), chunk, 0);
            chunk.addByte((uint8_t)OpCode::OP_ADD, 0);
            emitStore(node->children[0]->value, chunk);
        }
        return;
    }

    // --- 3. NODURI TERMINALE & STRUCTURI (Array/Map) ---
    if (node->type == ASTNodeType::Literal) {
        if (node->value == L"ARRAY_OBJECT") {
            // 1. Evaluăm fiecare element și îl punem pe stivă
            for (auto& child : node->children) {
                generateFromAST(child, chunk);
            }
            // 2. Emitem OP_ARRAY urmat de numărul de elemente
            chunk.addByte((uint8_t)OpCode::OP_ARRAY, 0);
            chunk.addByte((uint8_t)node->children.size(), 0);
            return;
        }
        else if (node->value == L"MAP_OBJECT") {
            // 1. Evaluăm fiecare pereche (Cheie apoi Valoare)
            for (auto& child : node->children) {
                generateFromAST(child, chunk);
            }
            // 2. Emitem OP_MAP urmat de numărul de perechi (size/2)
            chunk.addByte((uint8_t)OpCode::OP_MAP, 0);
            chunk.addByte((uint8_t)(node->children.size() / 2), 0);
            return;
        }

        // Literal simplu (Număr, String)
        emitLoadOrConstant(node->value, chunk);
        return;
    }

    if (node->type == ASTNodeType::Variable) {
        emitLoadOrConstant(node->value, chunk);
        return;
    }

    // --- 3.5 APELURI DE FUNCȚII (NATIVE) ---
    if (node->type == ASTNodeType::FunctionCall) {
        // Determinăm dacă este un apel dinamic (ex: $var()) sau unul simplu (ex: RANDOM())
        bool isDynamic = (node->value == L"DYNAMIC_CALL");
        size_t startIdx = isDynamic ? 1 : 0;

        // 1. Punem toate argumentele pe stivă (de la stânga la dreapta)
        for (size_t i = startIdx; i < node->children.size(); ++i) {
            generateFromAST(node->children[i], chunk);
        }

        // 2. Identificăm numele funcției
        // Dacă e DYNAMIC_CALL, numele/variabila e în primul copil. Altfel e în node->value.
        std::wstring funcName = isDynamic ? node->children[0]->value : node->value;
        uint8_t argCount = (uint8_t)(node->children.size() - startIdx);

        // 3. Adăugăm numele în constante și emitem OP_CALL_NATIVE
        uint16_t nameIdx = chunk.addConstant(vData(funcName));

        chunk.addByte((uint8_t)OpCode::OP_CALL_NATIVE, 0);
        chunk.addByte((uint8_t)((nameIdx >> 8) & 0xFF), 0);
        chunk.addByte((uint8_t)(nameIdx & 0xFF), 0);
        chunk.addByte(argCount, 0); // Spunem VM-ului câte elemente să „înghită” de pe stivă
        return;
    }

    // --- 4. OPERATORI BINARI ---
    if (node->children.size() == 2) {
        if (node->value == L"&&" || node->value == L"||") {
            generateShortCircuit(node, chunk);
            return;
        }

        generateFromAST(node->children[0], chunk);
        generateFromAST(node->children[1], chunk);

        std::wstring op = node->value;
        if (op == L"+")      chunk.addByte((uint8_t)OpCode::OP_ADD, 0);
        else if (op == L"-") chunk.addByte((uint8_t)OpCode::OP_SUB, 0);
        else if (op == L"*") chunk.addByte((uint8_t)OpCode::OP_MUL, 0);
        else if (op == L"/") chunk.addByte((uint8_t)OpCode::OP_DIV, 0);
        else if (op == L"%") chunk.addByte((uint8_t)OpCode::OP_MOD, 0);
        else if (op == L"**") chunk.addByte((uint8_t)OpCode::OP_POW, 0);
        else if (op == L"==") chunk.addByte((uint8_t)OpCode::OP_EQUAL, 0);
        else if (op == L"!=") chunk.addByte((uint8_t)OpCode::OP_NOT_EQUAL, 0);
        else if (op == L">")  chunk.addByte((uint8_t)OpCode::OP_GREATER, 0);
        else if (op == L">=") chunk.addByte((uint8_t)OpCode::OP_GREATER_EQUAL, 0);
        else if (op == L"<")  chunk.addByte((uint8_t)OpCode::OP_LESS, 0);
        else if (op == L"<=") chunk.addByte((uint8_t)OpCode::OP_LESS_EQUAL, 0);
        else if (op == L"&")  chunk.addByte((uint8_t)OpCode::OP_BAND, 0);
        else if (op == L"|")  chunk.addByte((uint8_t)OpCode::OP_BOR, 0);
        else if (op == L"BXOR" || op == L"^") chunk.addByte((uint8_t)OpCode::OP_BXOR, 0);
        else if (op == L"<<") chunk.addByte((uint8_t)OpCode::OP_SHL, 0);
        else if (op == L">>") chunk.addByte((uint8_t)OpCode::OP_SHR, 0);
        else if (op == L"??") chunk.addByte((uint8_t)OpCode::OP_NULL_COALESCE, 0);
        else if (op == L"CONCAT") chunk.addByte((uint8_t)OpCode::OP_CONCAT, 0);

        // --- ACCES INDEXAT (Array/Map) ---
        // Aici DOT și INDEX pun pe stivă Containerul și Cheia/Indexul
        else if (op == L"DOT" || op == L"INDEX") {
            // Recomand folosirea unei instrucțiuni dedicate OP_INDEX_GET în loc de OP_GET_INDIRECT
            // pentru a clarifica faptul că scoatem 2 valori de pe stivă
            chunk.addByte((uint8_t)OpCode::OP_GET_INDIRECT, 0);
        }
    }
}

void OliCompiler::emitTargetAddress(const std::wstring& varName, OliChunk& chunk) {
    size_t dollarCount = 0;
    while (dollarCount < varName.size() && varName[dollarCount] == L'$') dollarCount++;

    if (dollarCount > 1) {
        std::wstring baseName = L"$" + varName.substr(dollarCount);
        uint16_t nameIdx = chunk.addConstant(vData(baseName));
        chunk.addByte((uint8_t)OpCode::OP_GET_GLOBAL, 0);
        chunk.addByte((nameIdx >> 8), 0); chunk.addByte((nameIdx & 0xFF), 0);

        // Navigăm până la penultimul nivel (pentru a lăsa numele final pe stivă)
        for (size_t i = 0; i < dollarCount - 2; ++i) {
            chunk.addByte((uint8_t)OpCode::OP_GET_INDIRECT, 0);
        }
    }
    else {
        // Pentru variabile simple, punem doar numele ca constantă
        uint16_t nameIdx = chunk.addConstant(vData(varName));
        emitConstant(vData(varName), chunk, 0);
    }
}


void OliCompiler::emitStore(const std::wstring& varName, OliChunk& chunk) {
    if (varName.empty()) return;

    // --- 1. DEREFERENȚIERE POINTER (*$ptr = valoare) ---
    if (!varName.empty() && varName[0] == L'*') {
        std::wstring targetVar = varName.substr(1); // Extragem "$ptr"

        // Punem ADRESA pe stivă. Trebuie să fie OP_GET_GLOBAL (index 2).
        uint16_t nameIdx = chunk.addConstant(vData(targetVar));
        chunk.addByte((uint8_t)OpCode::OP_GET_GLOBAL, 0); // <--- FORȚĂM GET
        chunk.addByte((uint8_t)(nameIdx >> 8), 0);
        chunk.addByte((uint8_t)(nameIdx & 0xFF), 0);

        // Scriem la adresa de pe stivă
        chunk.addByte((uint8_t)OpCode::OP_SET_INDIRECT, 0);
        return;
    }

    // --- 2. CALCULARE INDIRAȚIE ($$a, $$$b) ---
    size_t dollarCount = 0;
    while (dollarCount < varName.size() && varName[dollarCount] == L'$') {
        dollarCount++;
    }

    if (dollarCount > 1) {
        // Identificăm variabila de bază (ex: din $$$c extragem "$c")
        std::wstring baseName = L"$" + varName.substr(dollarCount);

        // Punem pe stivă numele sau valoarea variabilei de bază
        uint16_t nameIdx = chunk.addConstant(vData(baseName));
        chunk.addByte((uint8_t)OpCode::OP_GET_GLOBAL, 0);
        chunk.addByte((uint8_t)(nameIdx >> 8), 0);
        chunk.addByte((uint8_t)(nameIdx & 0xFF), 0);

        // Navigăm prin straturile de indirație ($$$c necesită un pas de dereferențiere extra)
        for (size_t i = 0; i < dollarCount - 2; ++i) {
            chunk.addByte((uint8_t)OpCode::OP_GET_INDIRECT, 0);
        }

        // Stiva acum: [Valoare_Nouă, Nume_Final]
        chunk.addByte((uint8_t)OpCode::OP_SET_INDIRECT, 0);
    }
    else {
        // --- 3. ATRIBUIRE DIRECTĂ ($a = valoare) ---
        // Aceasta este SINGURA ramură unde se emite OP_SET_GLOBAL.
        uint16_t nameIdx = chunk.addConstant(vData(varName));
        chunk.addByte((uint8_t)OpCode::OP_SET_GLOBAL, 0);
        chunk.addByte((uint8_t)(nameIdx >> 8), 0);
        chunk.addByte((uint8_t)(nameIdx & 0xFF), 0);
    }
}

void OliCompiler::generateShortCircuit(ASTPtr node, OliChunk& chunk) {
    bool isAnd = (node->value == L"&&");

    // 1. Evaluăm partea stângă (rezultatul rămâne pe stivă)
    generateFromAST(node->children[0], chunk);

    // 2. Generăm Jump-ul de scurtcircuit
    // Dacă e &&: sărim la final dacă stânga e FALSE (false && orice == false)
    // Dacă e ||: sărim la final dacă stânga e TRUE (true || orice == true)
    uint8_t jumpOp = isAnd ? (uint8_t)OpCode::OP_JUMP_IF_FALSE : (uint8_t)OpCode::OP_JUMP_IF_TRUE;
    chunk.addByte(jumpOp, 0);

    size_t jumpAddr = chunk.code.size();
    chunk.addByte(0, 0); chunk.addByte(0, 0); // Placeholder offset

    // 3. Dacă am ajuns aici, înseamnă că rezultatul depinde de partea dreaptă.
    // Mai întâi eliminăm rezultatul părții stângi de pe stivă (POP)
    chunk.addByte((uint8_t)OpCode::OP_POP, 0);

    // 4. Evaluăm partea dreaptă
    generateFromAST(node->children[1], chunk);

    // 5. Backpatching: setăm adresa de salt la instrucțiunea de după evaluarea dreptei
    uint16_t offset = (uint16_t)(chunk.code.size() - (jumpAddr + 2));
    chunk.code[jumpAddr] = (uint8_t)(offset >> 8);
    chunk.code[jumpAddr + 1] = (uint8_t)(offset & 0xFF);
}

std::wstring OliCompiler::reconstructRawName(ASTPtr node) {
    if (!node) return L"";

    // Dacă e variabilă (ex: "$a", "$$b", "$$$ptrToPtr"), returnăm valoarea direct
    if (node->type == ASTNodeType::Variable) return node->value;

    // Dacă e dereferențiere (operatoul *), adăugăm "*" și procesăm recursiv copilul
    if (node->type == ASTNodeType::Operator && node->value == L"DEREFERENCE") {
        return L"*" + reconstructRawName(node->children[0]);
    }

    return node->value; // Fallback pentru alte cazuri
}

void OliCompiler::loadPluginMetadata(std::wstring pluginName) {
    // 1. Curățăm ghilimelele de la începutul și sfârșitul căii
    if (pluginName.size() >= 2 && pluginName.front() == L'"' && pluginName.back() == L'"') {
        pluginName = pluginName.substr(1, pluginName.size() - 2);
    }

    if (pluginName.empty()) return;

    // 2. Pregătim calea către DLL
    std::wstring dllPath = pluginName;
    std::wstring ext = PortTools::getPluginExtension();

    // Adăugăm extensia (.dll / .so) doar dacă lipsește
    if (dllPath.size() < ext.size() ||
        dllPath.substr(dllPath.size() - ext.size()) != ext) {
        dllPath += ext;
    }

    // 3. Încărcăm biblioteca în procesul curent al compilatorului
    // Folosim PortTools pentru abstractizarea Windows/Linux
    PortTools::LibHandle hLib = PortTools::loadDynamicLibrary(dllPath);

    if (!hLib) {
        // Dacă ești în mod de debug, poți afișa eroarea, altfel ignorăm silențios.
        // VM-ul va raporta eroarea reală la execuție.
        return;
    }

    // 4. Extragem simbolul pentru înregistrarea comenzilor
    // Castăm simbolul la tipul LoadCommandsFunc definit în header
    LoadCommandsFunc regCmds = (LoadCommandsFunc)PortTools::getFunctionSymbol(hLib, "LoadOliCommandPlugin");

    if (regCmds) {
        // Creăm un map temporar (dummy) pentru a colecta cheile (numele comenzilor)
        std::unordered_map<std::wstring, OliCommandHandler> dummyMap;

        // Apelăm funcția din plugin.
        // CRITIC: Trimitem nullptr pentru instanța de engine deoarece compilatorul 
        // nu rulează codul, doar are nevoie de numele instrucțiunilor.
        try {
            regCmds(dummyMap, nullptr);

            // 5. Înregistrăm fiecare comandă găsită în lista de cuvinte cheie dinamice
            for (auto const& [name, handler] : dummyMap) {
                if (!name.empty()) {
                    vOliKeyWords::registerDynamicCommand(name);
                }
            }
        }
        catch (...) {
            // Prevenim crash-ul compilatorului dacă plugin-ul încearcă 
            // să folosească engine-ul (care e nullptr) fără să verifice.
        }
    }

    // Notă: Nu facem FreeLibrary(hLib) aici. Deși std::wstring copiază datele,
    // păstrarea bibliotecii încărcate până la finalul compilării previne
    // orice problemă de memorie legată de simbolurile statice din plugin.
}