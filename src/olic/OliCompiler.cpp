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

        // IMPORTANT: Detectăm începutul și sfârșitul buclei pentru a nu tăia buffer-ul
        if (upperLine.find(L"IF ") == 0 || upperLine.find(L"WHILE ") == 0) {
            nestingLevel++;
        }
        if (upperLine.find(L"ENDIF") != std::wstring::npos || upperLine.find(L"ENDWHILE") != std::wstring::npos) {
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
    this->optimize(chunk);
    return chunk;
}

void OliCompiler::compileStatement(const ShellCommand& sc, OliChunk& chunk) {
   // ShellCommand sc = vOliCommandParser::parse(line);

    // Exemplu: SET x = 10
    std::wstring cmdName = to_upper(sc.name);

    if (cmdName == L"SET") {
        std::wstring varName = sc.args[0];

        // 1. CALCULĂM VALOAREA (RHS - Right Hand Side)
        // Indiferent unde o salvăm, rezultatul trebuie să fie pe stivă primul.
        if (sc.args.size() >= 5 && (sc.args[3] == L"+" || sc.args[3] == L"-")) {
            emitLoadOrConstant(sc.args[2], chunk);
            emitLoadOrConstant(sc.args[4], chunk);

            if (sc.args[3] == L"+") chunk.addByte((uint8_t)OpCode::OP_ADD, 0);
            else                   chunk.addByte((uint8_t)OpCode::OP_SUB, 0);
        }
        else {
            // Extragem valoarea corect, fie că avem "SET $x = 10" sau "SET $x 10"
            std::wstring valStr = (sc.args.size() > 2 && sc.args[1] == L"=") ? sc.args[2] : sc.args[1];
            emitLoadOrConstant(valStr, chunk);
        }

        // 2. DETERMINĂM TIPUL DE SALVARE (DIRECTĂ SAU INDIRECTĂ)
        size_t dollarCount = 0;
        while (dollarCount < varName.size() && varName[dollarCount] == L'$') {
            dollarCount++;
        }

        if (dollarCount > 1) {
            // --- CAZUL INDIRECT (ex: SET $$$c = 100) ---
            // Valoarea calculată la pasul 1 este deja pe stivă.
            // Acum trebuie să punem pe stivă numele variabilei în care vrem să scriem.

            std::wstring baseName = L"$" + varName.substr(dollarCount);

            // Încărcăm variabila de bază (ex: pentru $$$c, încărcăm $c care conține "b")
            chunk.addByte((uint8_t)OpCode::OP_GET_GLOBAL, 0);
            uint16_t nameIdx = chunk.addConstant(vData(baseName));
            chunk.addByte((uint8_t)(nameIdx >> 8), 0);
            chunk.addByte((uint8_t)(nameIdx & 0xFF), 0);

            // Rulăm GET_INDIRECT pentru a naviga prin lanț până la numele final
            // Exemplu $$$c: 
            // 1. GET_GLOBAL($c) -> pune "b" pe stivă
            // 2. Bucla rulează o dată (3-2=1) -> GET_INDIRECT caută "b", pune "a" pe stivă
            for (size_t i = 0; i < dollarCount - 2; ++i) {
                chunk.addByte((uint8_t)OpCode::OP_GET_INDIRECT, 0);
            }

            // Acum stiva are: [Valoare, NumeȚintă ("a")]
            chunk.addByte((uint8_t)OpCode::OP_SET_INDIRECT, 0);
        }
        else {
            // --- CAZUL NORMAL (ex: SET $a = 100) ---
            chunk.addByte((uint8_t)OpCode::OP_SET_GLOBAL, 0);
            uint16_t nameIdx = chunk.addConstant(vData(varName));
            chunk.addByte((uint8_t)(nameIdx >> 8), 0);
            chunk.addByte((uint8_t)(nameIdx & 0xFF), 0);
        }
        }
    // Exemplu: ECHO x
    else if (cmdName == L"ECHO") {
        // Verificăm dacă avem o expresie matematică: ECHO 1 + 1
        if (sc.args.size() >= 3 && (sc.args[1] == L"+" || sc.args[1] == L"-" || sc.args[1] == L"*" || sc.args[1] == L"/")) {
            emitLoadOrConstant(sc.args[0], chunk);
            emitLoadOrConstant(sc.args[2], chunk);

            if (sc.args[1] == L"+")      chunk.addByte((uint8_t)OpCode::OP_ADD, 0);
            else if (sc.args[1] == L"-") chunk.addByte((uint8_t)OpCode::OP_SUB, 0);
            else if (sc.args[1] == L"*") chunk.addByte((uint8_t)OpCode::OP_MUL, 0);
            else if (sc.args[1] == L"/") chunk.addByte((uint8_t)OpCode::OP_DIV, 0);
        }
        else {
            // Cazul simplu: ECHO $a sau ECHO "text"
            emitLoadOrConstant(sc.args[0], chunk);
        }

        // La final, adăugăm instrucțiunea de afișare a ceea ce a rămas pe stivă
        chunk.addByte((uint8_t)OpCode::OP_ECHO, 0);
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
        // Format așteptat: UNTIL $a == 10
        if (untilIdx + 3 < (int)sc.args.size()) {
            emitLoadOrConstant(sc.args[untilIdx + 1], chunk); // LHS ($counter)
            emitLoadOrConstant(sc.args[untilIdx + 3], chunk); // RHS ($limit)

            std::wstring op = sc.args[untilIdx + 2];
            if (op == L">")       chunk.addByte((uint8_t)OpCode::OP_GREATER, 0);
            else if (op == L"<")  chunk.addByte((uint8_t)OpCode::OP_LESS, 0);
            else if (op == L"==") chunk.addByte((uint8_t)OpCode::OP_EQUAL, 0);

            // 5. LOGICA DE JUMP (REPEAT repetă dacă condiția este FALSE)
            // Dacă e TRUE (condiția UNTIL e îndeplinită), sărim peste OP_LOOP
            chunk.addByte((uint8_t)OpCode::OP_JUMP_IF_TRUE, 0);
            size_t patchAddr = chunk.code.size();
            chunk.addByte(0, 0); chunk.addByte(0, 0); // Placeholder offset

            // Dacă JUMP_IF_TRUE nu s-a executat (e False), facem LOOP înapoi
            chunk.addByte((uint8_t)OpCode::OP_LOOP, 0);
            uint16_t loopOffset = (uint16_t)(chunk.code.size() + 2 - loopStart);
            chunk.addByte((uint8_t)(loopOffset >> 8), 0);
            chunk.addByte((uint8_t)(loopOffset & 0xFF), 0);

            // Backpatching: Scriem adresa de ieșire în JUMP_IF_TRUE
            uint16_t exitOffset = (uint16_t)(chunk.code.size() - (patchAddr + 2));
            chunk.code[patchAddr] = (uint8_t)(exitOffset >> 8);
            chunk.code[patchAddr + 1] = (uint8_t)(exitOffset & 0xFF);
        }
        }
}

void OliCompiler::emitConstant(const vData& value, OliChunk& chunk, int line) {
    uint16_t idx = chunk.addConstant(value);
    chunk.addByte((uint8_t)OpCode::OP_CONSTANT, line);
    chunk.addByte((uint8_t)(idx >> 8), line);
    chunk.addByte((uint8_t)(idx & 0xFF), line);
}


void OliCompiler::emitLoadOrConstant(const std::wstring& arg, OliChunk& chunk) {
    // 1. Este șir de caractere între ghilimele?
    if (arg.size() >= 2 && arg.front() == L'\"' && arg.back() == L'\"') {
        std::wstring cleanStr = arg.substr(1, arg.size() - 2);
        emitConstant(vData(cleanStr), chunk, 0);
    }
    // 2. Este un NUMĂR? (Căutăm dacă primul caracter e cifră)
    else if (std::iswdigit(arg[0]) || (arg.size() > 1 && arg[0] == L'-' && std::iswdigit(arg[1]))) {
        double val = std::stod(arg); // Convertim string în double
        emitConstant(vData(val), chunk, 0);
    }
    // 3. Altfel, este o VARIABILĂ
    // Detecție Variable Variables ($$$c)
    else if (arg.size() > 1 && arg[0] == L'$') {
        size_t dollarCount = 0;
        while (dollarCount < arg.size() && arg[dollarCount] == L'$') {
            dollarCount++;
        }

        // Numele de bază (ex: din $$$c extragem "c")
        std::wstring baseName = L"$" + arg.substr(dollarCount);

        // 1. Încărcăm variabila de bază ($c)
        chunk.addByte((uint8_t)OpCode::OP_GET_GLOBAL, 0);
        uint16_t nameIdx = chunk.addConstant(vData(baseName));
        chunk.addByte((uint8_t)(nameIdx >> 8), 0);
        chunk.addByte((uint8_t)(nameIdx & 0xFF), 0);

        // 2. Pentru fiecare '$' extra, adăugăm o dereferențiere indirectă
        // $$$c înseamnă: GET_GLOBAL($c) -> GET_INDIRECT (rezultă $b) -> GET_INDIRECT (rezultă $a)
        for (size_t i = 1; i < dollarCount; ++i) {
            chunk.addByte((uint8_t)OpCode::OP_GET_INDIRECT, 0);
        }
    }
    else {
        chunk.addByte((uint8_t)OpCode::OP_GET_GLOBAL, 0);
        uint16_t nameIdx = chunk.addConstant(vData(arg));
        chunk.addByte((uint8_t)(nameIdx >> 8), 0);
        chunk.addByte((uint8_t)(nameIdx & 0xFF), 0);
    }
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