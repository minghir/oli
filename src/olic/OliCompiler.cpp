#include "../vDataSerialize.hpp"
#include "OliCompiler.hpp"
#include "../OliCommandParser.hpp"
#include "../StringUtils.hpp"
#include <iostream>

OliChunk OliCompiler::compile(const std::wstring& source) {
    OliChunk chunk;
    std::wstringstream ss(source);
    std::wstring line;
    std::wstring commandBuffer = L"";
    int nestingLevel = 0;

    while (std::getline(ss, line)) {
        line = trim(line);
        if (line.empty()) continue;

        std::wstring upperLine = to_upper(line);

        if (upperLine.find(L"IF ") == 0 || upperLine.find(L"WHILE ") == 0 ||
            upperLine.find(L"REPEAT") == 0 || upperLine.find(L"FOR ") == 0) {
            nestingLevel++;
        }
        if (upperLine.find(L"ENDIF") != std::wstring::npos ||
            upperLine.find(L"ENDWHILE") != std::wstring::npos ||
            upperLine.find(L"ENDREPEAT") != std::wstring::npos ||
            upperLine.find(L"ENDFOR") != std::wstring::npos) {
            nestingLevel--;
        }

        commandBuffer += L" " + line;

        // Trimitem la compilare DOAR când toate blocurile sunt închise (nestingLevel == 0)
        if (nestingLevel == 0) {
            std::vector<std::wstring> statements = splitWBySemicolon(commandBuffer);
            for (const auto& stmt : statements) {
                ShellCommand sc = vOliCommandParser::parse(stmt);
                compileStatement(sc, chunk);
            }
            commandBuffer = L"";
        }
    }

    chunk.addByte((uint8_t)OpCode::OP_RETURN, 0);
    //this->optimize(chunk);
    return chunk;
}

void OliCompiler::compileStatement(const ShellCommand& sc, OliChunk& chunk) {
   // ShellCommand sc = vOliCommandParser::parse(line);

    // Exemplu: SET x = 10
    std::wstring cmdName = to_upper(sc.name);

    if (cmdName == L"SET") {
        if (sc.args.size() < 2) return;

        // 1. Detectăm corect destinația (LHS), chiar dacă '*' este token separat
        size_t targetTokenCount = 1;
        std::wstring varName = sc.args[0];

        if (varName == L"*" && sc.args.size() > 1) {
            varName += sc.args[1]; // Reconstituim *$ptr
            targetTokenCount = 2;
        }

        // 2. Identificăm unde începe expresia (RHS), sărind peste '='
        size_t startIdx = targetTokenCount;
        if (startIdx < sc.args.size() && sc.args[startIdx] == L"=") {
            startIdx++;
        }

        // 3. Colectăm restul argumentelor într-un vector curat pentru parser
        // Este CRITIC ca rhsTokens să NU conțină variabila destinație sau '='
        std::vector<std::wstring> rhsTokens;
        for (size_t i = startIdx; i < sc.args.size(); ++i) {
            rhsTokens.push_back(sc.args[i]);
        }

        // 4. Parsăm Right-Hand Side (RHS) și generăm bytecode
        OliExpressionParser exprParser(rhsTokens);
        ASTPtr exprAST = exprParser.parse();

        if (exprAST) {
            generateFromAST(exprAST, chunk); // Rezultatul calculului ajunge pe stivă

            // 5. Salvăm valoarea de pe stivă în destinație (LHS)
            // emitStore va genera OP_GET_GLOBAL + OP_SET_INDIRECT pentru pointeri
            emitStore(varName, chunk);
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

    // 1. ȘIR DE CARACTERE (LITERAL)
    if (arg.size() >= 2 && arg.front() == L'\"' && arg.back() == L'\"') {
        std::wstring cleanStr = arg.substr(1, arg.size() - 2);
        emitConstant(vData(cleanStr), chunk, 0);
        return;
    }

    // 2. NUMĂR (LITERAL)
    if (std::iswdigit(arg[0]) || (arg.size() > 1 && arg[0] == L'-' && std::iswdigit(arg[1]))) {
        double val = std::stod(arg);
        emitConstant(vData(val), chunk, 0);
        return;
    }

    // 3. DEREFERENȚIERE POINTER (*$ptr)
    if (arg[0] == L'*') {
        emitLoadOrConstant(arg.substr(1), chunk); // Încărcăm recursiv (ex: $ptr)
        chunk.addByte((uint8_t)OpCode::OP_GET_INDIRECT, 0); // Citim valoarea de la adresă
        return;
    }

    // 4. VARIABILE (Simple sau Indirecte: $a, $$b, $$$c)
    if (arg[0] == L'$') {
        size_t dollarCount = 0;
        while (dollarCount < arg.size() && arg[dollarCount] == L'$') {
            dollarCount++;
        }

        std::wstring baseName = L"$" + arg.substr(dollarCount);

        // --- ATENȚIE: Trebuie să fie OP_GET_GLOBAL (0x02) ---
        chunk.addByte((uint8_t)OpCode::OP_GET_GLOBAL, 0);
        uint16_t nameIdx = chunk.addConstant(vData(baseName));
        chunk.addByte((uint8_t)(nameIdx >> 8), 0);
        chunk.addByte((uint8_t)(nameIdx & 0xFF), 0);

        for (size_t i = 1; i < dollarCount; ++i) {
            chunk.addByte((uint8_t)OpCode::OP_GET_INDIRECT, 0);
        }
        return;
    }

    // 5. CAZ DEFAULT (Variabilă fără prefix)
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



    // --- 1. ATRIBUIRE (=, +=, -=, etc.) ---
    // Tratăm atribuirea special: evaluăm RHS (dreapta), apoi salvăm în LHS (stânga)
    if (node->type == ASTNodeType::Operator && (node->value == L"=" || node->value == L"+=")) {
        if (node->value == L"=") {
            // 1. Evaluăm Right-Hand Side (RHS). Valoarea rezultată rămâne pe stivă.
            generateFromAST(node->children[1], chunk);

            // 2. Pasăm Left-Hand Side (LHS) direct către emitStore.
            // emitStore va decide dacă generează OP_SET_GLOBAL, OP_SET_INDIRECT (pt *) sau logică de $$.
            emitStore(node->children[0]->value, chunk);

            return; // Finalizăm procesarea nodului de atribuire
        }
        // TODO: Implementare +=, -= aici
    }

    // --- 2. OPERATORI UNARI (-, !, ~, *, ++, --) ---
    if (node->type == ASTNodeType::Operator && node->children.size() == 1) {
        // --- EXCEPȚIE CRITICĂ PENTRU & ---
        if (node->value == L"ADDRESS_OF" || node->value == L"&") {
            std::wstring varName = node->children[0]->value;
            uint16_t nameIdx = chunk.addConstant(vData(varName));
            chunk.addByte((uint8_t)OpCode::OP_GET_ADDR, 0);
            chunk.addByte((nameIdx >> 8) & 0xFF, 0);
            chunk.addByte(nameIdx & 0xFF, 0);
            return; // Ieșim imediat! NU apelăm generateFromAST pe copil.
        }

        generateFromAST(node->children[0], chunk); // Punem operandul pe stivă

        if (node->value == L"UNARY_MINUS")      chunk.addByte((uint8_t)OpCode::OP_NEGATE, 0);
        else if (node->value == L"NOT")         chunk.addByte((uint8_t)OpCode::OP_LOGICAL_NOT, 0);
        else if (node->value == L"BITWISE_NOT") chunk.addByte((uint8_t)OpCode::OP_BNOT, 0);
        else if (node->value == L"DEREFERENCE") chunk.addByte((uint8_t)OpCode::OP_GET_INDIRECT, 0);
        else if (node->value == L"POSTFIX_INC") {
            // Incrementarea este specială: adunăm 1 și salvăm înapoi
            chunk.addByte((uint8_t)OpCode::OP_DUP, 0); // Păstrăm valoarea originală pentru rezultat
            emitConstant(vData(1.0), chunk, 0);
            chunk.addByte((uint8_t)OpCode::OP_ADD, 0);
            emitStore(node->children[0]->value, chunk); // Salvăm noul i
            chunk.addByte((uint8_t)OpCode::OP_POP, 0); // Eliminăm gunoiul, lăsăm originalul pe stivă
        }
        else if (node->value == L"ADDRESS_OF" || node->value == L"&") {
            // Luăm numele variabilei de la copilul nodului &
            std::wstring varName = node->children[0]->value;
            uint16_t nameIdx = chunk.addConstant(vData(varName));
            chunk.addByte((uint8_t)OpCode::OP_GET_ADDR, 0);
            chunk.addByte((nameIdx >> 8) & 0xFF, 0);
            chunk.addByte(nameIdx & 0xFF, 0);
            return;
        }
        return;
    }

    // --- 3. NODURI TERMINALE ---
    if (node->type == ASTNodeType::Literal) {
        emitLoadOrConstant(node->value, chunk);
        return;
    }
    if (node->type == ASTNodeType::Variable) {
        emitLoadOrConstant(node->value, chunk); // Va genera OP_GET_GLOBAL sau GET_INDIRECT
        return;
    }

    // --- 4. OPERATORI BINARI (Standard: Stânga, Dreapta, Op) ---
    if (node->children.size() == 2) {
        // Excepție: Logica && și || necesită salturi (scurtcircuitare)
        if (node->value == L"&&" || node->value == L"||") {
            generateShortCircuit(node, chunk);
            return;
        }

        // Parcurgere normală: evaluăm ambii operanzi
        generateFromAST(node->children[0], chunk);
        generateFromAST(node->children[1], chunk);

        std::wstring op = node->value;
        if (op == L"+")      chunk.addByte((uint8_t)OpCode::OP_ADD, 0);
        else if (op == L"-") chunk.addByte((uint8_t)OpCode::OP_SUB, 0);
        else if (op == L"*") chunk.addByte((uint8_t)OpCode::OP_MUL, 0);
        else if (op == L"/") chunk.addByte((uint8_t)OpCode::OP_DIV, 0);
        else if (op == L"%") chunk.addByte((uint8_t)OpCode::OP_MOD, 0);
        else if (op == L"**") chunk.addByte((uint8_t)OpCode::OP_POW, 0);

        // Comparații
        else if (op == L"==") chunk.addByte((uint8_t)OpCode::OP_EQUAL, 0);
        else if (op == L"!=") chunk.addByte((uint8_t)OpCode::OP_NOT_EQUAL, 0);
        else if (op == L">")  chunk.addByte((uint8_t)OpCode::OP_GREATER, 0);
        else if (op == L">=") chunk.addByte((uint8_t)OpCode::OP_GREATER_EQUAL, 0);
        else if (op == L"<")  chunk.addByte((uint8_t)OpCode::OP_LESS, 0);
        else if (op == L"<=") chunk.addByte((uint8_t)OpCode::OP_LESS_EQUAL, 0);

        // Bitwise
        else if (op == L"&")  chunk.addByte((uint8_t)OpCode::OP_BAND, 0);
        else if (op == L"|")  chunk.addByte((uint8_t)OpCode::OP_BOR, 0);
        else if (op == L"BXOR") chunk.addByte((uint8_t)OpCode::OP_BXOR, 0);

        // Speciali
        else if (op == L"??")    chunk.addByte((uint8_t)OpCode::OP_NULL_COALESCE, 0);
        else if (op == L"CONCAT")chunk.addByte((uint8_t)OpCode::OP_CONCAT, 0);
        else if (op == L"DOT" || op == L"INDEX") chunk.addByte((uint8_t)OpCode::OP_GET_INDIRECT, 0);

        
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