#include "../vDataSerialize.hpp"
#include "OliCompiler.hpp"
#include "../OliCommandParser.hpp"
#include "../StringUtils.hpp"
#include "../PortTools.hpp"
#include <iostream>

// Helper pentru tokenizare rapidă (adaugă-l deasupra metodei compile)
static std::vector<std::wstring> splitW(const std::wstring& s, const std::wstring& delimiters) {
    std::vector<std::wstring> tokens;
    size_t lastPos = s.find_first_not_of(delimiters, 0);
    size_t pos = s.find_first_of(delimiters, lastPos);
    while (std::wstring::npos != pos || std::wstring::npos != lastPos) {
        tokens.push_back(s.substr(lastPos, pos - lastPos));
        lastPos = s.find_first_not_of(delimiters, pos);
        pos = s.find_first_of(delimiters, lastPos);
    }
    return tokens;
}

std::vector<std::wstring> splitStatementsSmart(const std::wstring& input) {
    std::vector<std::wstring> result;
    std::wstring current;
    int depth = 0;
    int bDepth = 0;
    bool inQuotes = false;

    // Tokenizăm grosier pentru a găsi structurile de control
    auto tokens = vOliCommandParser::tokenize(input);

    for (const auto& token : tokens) {
        std::wstring ut = to_upper(token);

        if (token == L"\"") inQuotes = !inQuotes; // Notă: Depinde de cum scoate parserul tău ghilimelele

        if (!inQuotes) {
            // Adăugăm REPEAT aici
            if (ut == L"IF" || ut == L"WHILE" || ut == L"FOR" || ut == L"FUNC" || ut == L"REPEAT") depth++;

            // Adăugăm ENDREPEAT aici
            if (ut == L"ENDIF" || ut == L"ENDWHILE" || ut == L"ENDFOR" || ut == L"ENDFUNC" || ut == L"ENDREPEAT") depth--;

            if (token == L"{" || token == L"[") bDepth++;
            if (token == L"}" || token == L"]") bDepth--;

            if (token == L";" && depth == 0 && bDepth == 0) {
                if (!trim(current).empty()) result.push_back(trim(current));
                current.clear();
                continue;
            }
        }
        current += token + L" ";
    }

    if (!trim(current).empty()) result.push_back(trim(current));
    return result;
}

OliChunk OliCompiler::compile(const std::wstring& source,
    const std::unordered_map<std::wstring, ByteCodeProcedure>& parentProcs,
    bool isSubBlock)
{
    if (!isSubBlock) {
        breakStack.clear();
        continueStack.clear();
    }

    if (isSubBlock) {
        LOG_DEBUG(L"[DEBUG] Start compile. isSubBlock: YES");
    }
    else {
        LOG_DEBUG(L"[DEBUG] Start compile. isSubBlock: NO");
    }

    OliChunk chunk;

    // IMPORTANT: NU copiem parentProcs în chunk.procedures pentru a evita referințele circulare.
    // Tabela 'chunk.procedures' va conține DOAR funcțiile definite în ACEST bloc.

    std::wstringstream ss(source);
    std::wstring line;
    std::wstring commandBuffer = L"";
    int nestingLevel = 0;
    int bracketDepth = 0;

    while (std::getline(ss, line)) {
        std::wstring cleanLine = trim(line);
        if (cleanLine.empty()) continue;

        // --- 1. MASCARE & COMENTARII ---
        bool inQuotes = false;
        std::wstring maskedLine = cleanLine;
        for (size_t i = 0; i < maskedLine.length(); ++i) {
            if (maskedLine[i] == L'"' && (i == 0 || maskedLine[i - 1] != L'\\')) inQuotes = !inQuotes;
            if (inQuotes) maskedLine[i] = L' ';
        }

        size_t commentPos = maskedLine.find(L'#');
        if (commentPos != std::wstring::npos) {
            cleanLine = trim(cleanLine.substr(0, commentPos));
            maskedLine = trim(maskedLine.substr(0, commentPos));
        }
        if (cleanLine.empty()) continue;

        // --- 2. ACTUALIZARE ADÂNCIME (Nesting) ---
        auto tokens = splitW(to_upper(maskedLine), L" \t\n\r();,");
        
        for (const auto& t : tokens) {
            // Adăugăm REPEAT
            if (t == L"IF" || t == L"WHILE" || t == L"FOR" || t == L"PROC" || t == L"FUNC" || t == L"REPEAT") nestingLevel++;

            // Adăugăm ENDREPEAT
            if (t == L"ENDIF" || t == L"ENDWHILE" || t == L"ENDFOR" || t == L"ENDPROC" || t == L"ENDFUNC" || t == L"ENDREPEAT") nestingLevel--;

            if (t == L"{" || t == L"[") bracketDepth++;
            if (t == L"}" || t == L"]") bracketDepth--;
        }

        // --- 3. ACUMULARE BUFFER ---
        if (commandBuffer.empty()) commandBuffer = cleanLine;
        else commandBuffer += L" ; " + cleanLine;

        // --- 4. DECIZIA DE COMPILARE ---
        if (nestingLevel == 0 && bracketDepth == 0) {
            LOG_DEBUG(L"[DEBUG] Nesting reached 0. Compiling buffer...");
            std::wstring trimmedBuf = trim(commandBuffer);

            // Eliminăm punct-virgulele de început
            while (!trimmedBuf.empty() && trimmedBuf.front() == L';') {
                trimmedBuf = trim(trimmedBuf.substr(1));
            }

            if (!trimmedBuf.empty()) {
                auto stmts = splitStatementsSmart(trimmedBuf);

                for (const auto& s : stmts) {
                    LOG_DEBUG(L"[DEBUG] Compiling statement: " + (s.length() > 30 ? s.substr(0, 30) + L"..." : s));
                    std::wstring ts = trim(s);
                    if (ts.empty()) continue;

                    // FIX CRITIC: Pasăm parentProcs către compileStatement pentru lookup-ul funcțiilor recursive
                    compileStatement(vOliCommandParser::parse(ts), chunk, parentProcs);
                }
            }
            commandBuffer = L"";
        }
    }

    // --- 5. FINALIZARE ---
    if (!isSubBlock) {
        LOG_DEBUG(L"[DEBUG] Finalizing Main Chunk.");
        if (chunk.code.empty() || chunk.code.back() != (uint8_t)OpCode::OP_RETURN) {
            chunk.addByte((uint8_t)OpCode::OP_RETURN, 0);
        }
    }

    LOG_DEBUG(L"[DEBUG] End compile successfully.");
    return chunk;
}



void OliCompiler::compileStatement(const ShellCommand& sc, OliChunk& chunk, const std::unordered_map<std::wstring, ByteCodeProcedure>& externalProcs) {
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
            generateFromAST(exprAST, chunk, externalProcs);
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
                generateFromAST(exprAST, chunk, externalProcs);
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


    else if (cmdName == L"FUNC") {
        LOG_DEBUG(L"[DEBUG] Entering FUNC block parsing...");
        // 1. Parsăm Header-ul: SUMA($a, $b)
        std::wstring header = L"";
        for (const auto& a : sc.args) header += a;

        size_t openP = header.find(L'(');
        size_t closeP = header.find(L')');
        if (openP == std::wstring::npos || closeP == std::wstring::npos) {
            LOG_DEBUG(L"[DEBUG] ERROR: Invalid FUNC header.");
            return;
        }

        std::wstring funcName = to_upper(trim(header.substr(0, openP)));
        LOG_DEBUG(L"[DEBUG] Function Name: " + funcName);

        std::wstring paramsPart = header.substr(openP + 1, closeP - openP - 1);

        // 2. Delimităm corpul funcției
        int bodyStart = 0;
        for (int i = 0; i < (int)sc.args.size(); ++i) {
            if (sc.args[i].find(L')') != std::wstring::npos) {
                bodyStart = i + 1;
                break;
            }
        }

        int bodyEnd = -1;
        int depth = 1;
        for (int i = bodyStart; i < (int)sc.args.size(); ++i) {
            std::wstring argU = to_upper(sc.args[i]);
            if (argU == L"FUNC") depth++;
            if (argU == L"ENDFUNC") {
                depth--;
                if (depth == 0) { bodyEnd = i; break; }
            }
        }
        if (bodyEnd == -1) bodyEnd = (int)sc.args.size();

        // 3. Pregătim obiectul procedurii COMPLET (Nume + Parametri)
        ByteCodeProcedure proc;
        proc.name = funcName;
        proc.compiledBody = std::make_shared<OliChunk>();

        // IMPORTANT: Extragem parametrii ACUM, înainte de înregistrarea în map
        auto pTokens = splitW(paramsPart, L", ");
        for (auto& p : pTokens) {
            std::wstring cleaned = trim(p);
            if (cleaned == L"...") proc.isVariadic = true;
            else if (!cleaned.empty()) proc.params.push_back(this->cleanVariableName(cleaned));
        }

        // 4. ÎNREGISTRARE (Shell) - Permitem recursivitatea
        // Acum 'chunk.procedures' conține o funcție FACTORIAL care are parametri corecți.
        chunk.procedures[funcName] = proc;
        LOG_DEBUG(L"[DEBUG] Registered shell for " + funcName + L". Starting sub-block compile..." ) ;
        // 5. COMPILARE CORP (UN SINGUR APEL - cu 5 argumente)
        // ATENȚIE: Nu mai adăuga un al doilea apel după acesta!
        compileSubBlock(sc.args, bodyStart, bodyEnd, *(proc.compiledBody), chunk.procedures);

        // 6. Return implicit și salvare binară
        if (proc.compiledBody->code.empty() || proc.compiledBody->code.back() != (uint8_t)OpCode::OP_RETURN) {
            proc.compiledBody->addByte((uint8_t)OpCode::OP_RETURN, 0);
        }

        // Salvăm varianta cu bytecode-ul proaspăt compilat înapoi în map
        chunk.procedures[funcName] = proc;
    }

    // În OliCompiler::compileStatement, adaugă acest else if:

    else if (cmdName == L"RETURN") {
        if (sc.args.empty()) {
            // Return simplu, fără valoare (punem un NULL/Monostate pe stivă)
            emitConstant(vData(std::monostate{}), chunk, 0);
            chunk.addByte((uint8_t)OpCode::OP_RETURN, 0);
        }
        else {
            // Parsăm expresia de după RETURN (ex: $a + $b)
            OliExpressionParser exprParser(sc.args);
            ASTPtr exprAST = exprParser.parse();

            if (exprAST) {
                // Generăm codul care calculează rezultatul și îl lasă pe stivă
                generateFromAST(exprAST, chunk, externalProcs);
                // Emitem instrucțiunea de ieșire din funcție
                chunk.addByte((uint8_t)OpCode::OP_RETURN, 0);
            }
        }
        }
/*
    else if (cmdName == L"IF") {
        int thenIdx = -1, elseIdx = -1, endifIdx = -1;
        int depth = 0;

        // 1. Identificăm marcatorii locali corect (pentru a suporta IF-uri cuibărite)
        for (int i = 0; i < (int)sc.args.size(); ++i) {
            std::wstring argUpper = to_upper(sc.args[i]);

            if (argUpper == L"IF") depth++;
            else if (argUpper == L"ENDIF") {
                if (depth > 0) depth--;
                else if (endifIdx == -1) endifIdx = i;
            }
            else if (depth == 0) {
                if (argUpper == L"THEN") thenIdx = i;
                else if (argUpper == L"ELSE") elseIdx = i;
            }
        }

        if (thenIdx == -1) return; // Sintaxă invalidă

        // --- 2. COMPILĂM CONDIȚIA (Folosind AST complet) ---
        // Extragem toate jetoanele dintre 'IF' și 'THEN'
        std::vector<std::wstring> condTokens(sc.args.begin(), sc.args.begin() + thenIdx);
        OliExpressionParser condParser(condTokens);
        ASTPtr condAST = condParser.parse();

        // Generăm bytecode pentru condiție (va lăsa un bool pe stivă)
        generateFromAST(condAST, chunk, externalProcs);

        // --- 3. JUMP_IF_FALSE (Către ELSE sau END) ---
        chunk.addByte((uint8_t)OpCode::OP_JUMP_IF_FALSE, 0);
        size_t jumpToElseAddr = chunk.code.size();
        chunk.addByte(0, 0); chunk.addByte(0, 0); // Placeholder pentru offset

        // --- 4. COMPILĂM BLOCUL THEN ---
        // Determinăm unde se termină blocul THEN
        int thenEnd = (elseIdx != -1) ? elseIdx : (endifIdx != -1 ? endifIdx : (int)sc.args.size());

        // Extragem și compilăm sub-comenzile din interior
        // Notă: Folosim o funcție helper 'compileSubBlock' pentru a suporta multiple comenzi
        compileSubBlock(sc.args, thenIdx + 1, thenEnd, chunk, {});

        // --- 5. JUMP PESTE ELSE (Ieșire din IF) ---
        size_t jumpToEndAddr = 0;
        bool hasElse = (elseIdx != -1);
        if (hasElse) {
            chunk.addByte((uint8_t)OpCode::OP_JUMP, 0);
            jumpToEndAddr = chunk.code.size();
            chunk.addByte(0, 0); chunk.addByte(0, 0);
        }

        // --- 6. BACKPATCHING: Condiție FALSE ---
        uint16_t distToElse = (uint16_t)(chunk.code.size() - (jumpToElseAddr + 2));
        chunk.code[jumpToElseAddr] = (uint8_t)(distToElse >> 8);
        chunk.code[jumpToElseAddr + 1] = (uint8_t)(distToElse & 0xFF);

        // --- 7. COMPILĂM BLOCUL ELSE (Dacă există) ---
        if (hasElse) {
            int elseEnd = (endifIdx != -1) ? endifIdx : (int)sc.args.size();
            compileSubBlock(sc.args, elseIdx + 1, elseEnd, chunk, {});

            // BACKPATCHING: Saltul de peste ELSE
            uint16_t distToEnd = (uint16_t)(chunk.code.size() - (jumpToEndAddr + 2));
            chunk.code[jumpToEndAddr] = (uint8_t)(distToEnd >> 8);
            chunk.code[jumpToEndAddr + 1] = (uint8_t)(distToEnd & 0xFF);
        }
    }
    */
    else if (cmdName == L"IF") {
        int thenIdx = -1, elseIdx = -1, endifIdx = -1;
        int depth = 0; // Folosim depth=0 pentru consistență

        // 1. Identificăm markerii (Universal Nesting)
        for (int i = 0; i < (int)sc.args.size(); ++i) {
            std::wstring argU = to_upper(sc.args[i]);

            if (depth == 0) {
                if (argU == L"THEN" && thenIdx == -1) thenIdx = i;
                else if (argU == L"ELSE" && elseIdx == -1) elseIdx = i;
            }

            // Adăugăm TOATE structurile de control în calculul adâncimii
            if (argU == L"IF" || argU == L"WHILE" || argU == L"FOR" || argU == L"REPEAT" || argU == L"FUNC" || argU == L"PROC") {
                depth++;
            }
            else if (argU == L"ENDIF" || argU == L"ENDWHILE" || argU == L"ENDFOR" || argU == L"ENDREPEAT" || argU == L"ENDFUNC" || argU == L"ENDPROC") {
                if (depth > 0) {
                    depth--;
                }
                else if (argU == L"ENDIF" && endifIdx == -1) {
                    endifIdx = i;
                    break;
                }
            }
        }

        if (thenIdx == -1 || endifIdx == -1) return;

        // --- 2. COMPILĂM CONDIȚIA ---
        std::vector<std::wstring> condTokens(sc.args.begin(), sc.args.begin() + thenIdx);
        ASTPtr condAST = OliExpressionParser(condTokens).parse();
        if (condAST) generateFromAST(condAST, chunk, externalProcs);

        // --- 3. JUMP_IF_FALSE ---
        chunk.addByte((uint8_t)OpCode::OP_JUMP_IF_FALSE, 0);
        size_t jumpToElseAddr = chunk.code.size();
        chunk.addByte(0, 0); chunk.addByte(0, 0);

        // --- 4. COMPILĂM BLOCUL THEN ---
        int thenEnd = (elseIdx != -1) ? elseIdx : endifIdx;
        compileSubBlock(sc.args, thenIdx + 1, thenEnd, chunk, externalProcs);

        // --- 5. JUMP PESTE ELSE (Dacă există) ---
        size_t jumpToEndAddr = 0;
        bool hasElse = (elseIdx != -1);
        if (hasElse) {
            chunk.addByte((uint8_t)OpCode::OP_JUMP, 0);
            jumpToEndAddr = chunk.code.size();
            chunk.addByte(0, 0); chunk.addByte(0, 0);
        }

        // --- 6. BACKPATCHING: Condiție FALSE (Salt la ELSE sau ENDIF) ---
        uint16_t distToElse = (uint16_t)(chunk.code.size() - (jumpToElseAddr + 2));
        chunk.code[jumpToElseAddr] = (uint8_t)(distToElse >> 8);
        chunk.code[jumpToElseAddr + 1] = (uint8_t)(distToElse & 0xFF);

        // --- 7. COMPILĂM BLOCUL ELSE ---
        if (hasElse) {
            compileSubBlock(sc.args, elseIdx + 1, endifIdx, chunk, externalProcs);

            // BACKPATCHING: Saltul de peste ELSE către finalul IF-ului
            uint16_t distToEnd = (uint16_t)(chunk.code.size() - (jumpToEndAddr + 2));
            chunk.code[jumpToEndAddr] = (uint8_t)(distToEnd >> 8);
            chunk.code[jumpToEndAddr + 1] = (uint8_t)(distToEnd & 0xFF);
        }
        }

    else if (cmdName == L"BREAK") {
        if (breakStack.empty()) return;

        chunk.addByte((uint8_t)OpCode::OP_JUMP, 0);
        // Salvăm adresa la care încep cei 2 bytes de offset
        breakStack.back().push_back(chunk.code.size());

        chunk.addByte(0, 0); // Placeholder high
        chunk.addByte(0, 0); // Placeholder low
    }

    else if (cmdName == L"CONTINUE") {
        if (continueStack.empty()) {
            LOG_ERROR(L"CONTINUE gasit in afara unei bucle! (Stiva este goala)");
            return;
        }

        size_t patchAddr = chunk.code.size() + 1; // Adresa unde va fi offset-ul
        chunk.addByte((uint8_t)OpCode::OP_JUMP, 0); // Placeholder
        chunk.addByte(0, 0);
        chunk.addByte(0, 0);

        continueStack.back().push_back(patchAddr);
        LOG_DEBUG(L"CONTINUE inregistrat pentru patch la adresa: " + std::to_wstring(patchAddr));
}


    else if (cmdName == L"WHILE") {
        int doIdx = -1, endWhileIdx = -1, depth = 0;

        // 1. Identificăm markerii (Acum și cu REPEAT pentru consistență)
        for (int i = 0; i < (int)sc.args.size(); ++i) {
            std::wstring argU = to_upper(sc.args[i]);
            if (depth == 0 && argU == L"DO") doIdx = i;

            // Adăugăm REPEAT și FUNC/PROC pentru un nesting universal
            if (argU == L"WHILE" || argU == L"FOR" || argU == L"IF" || argU == L"REPEAT" || argU == L"FUNC") {
                depth++;
            }
            else if (argU == L"ENDWHILE" || argU == L"ENDFOR" || argU == L"ENDIF" || argU == L"ENDREPEAT" || argU == L"ENDFUNC") {
                if (depth > 0) {
                    depth--;
                }
                else if (argU == L"ENDWHILE" && endWhileIdx == -1) {
                    endWhileIdx = i;
                    break;
                }
            }
        }

        if (doIdx == -1 || endWhileIdx == -1) {
            LOG_ERROR(L"Structura WHILE invalida (lipseste DO sau ENDWHILE)");
            return;
        }

        // --- SETUP STIVE ---
        breakStack.push_back({});
        continueStack.push_back({});
        size_t currentStackLevel = continueStack.size();

        size_t loopStart = chunk.code.size();

        // 2. Compilăm Condiția
        std::vector<std::wstring> condTokens(sc.args.begin(), sc.args.begin() + doIdx);
        ASTPtr condAST = OliExpressionParser(condTokens).parse();
        if (condAST) generateFromAST(condAST, chunk, externalProcs);

        chunk.addByte((uint8_t)OpCode::OP_JUMP_IF_FALSE, 0);
        size_t exitJumpAddr = chunk.code.size();
        chunk.addByte(0, 0); chunk.addByte(0, 0);

        // 3. Compilăm Corpul (Recursiv)
        compileSubBlock(sc.args, doIdx + 1, endWhileIdx, chunk, externalProcs);

        // --- 4. PATCH CONTINUE ---
        if (continueStack.size() >= currentStackLevel) {
            std::vector<size_t> currentContinues = continueStack.back();
            continueStack.pop_back();

            for (size_t cAddr : currentContinues) {
                if (cAddr > 0 && cAddr + 1 < chunk.code.size()) {
                    chunk.code[cAddr - 1] = (uint8_t)OpCode::OP_LOOP;
                    uint16_t offset = (uint16_t)(cAddr + 2 - loopStart);
                    chunk.code[cAddr] = (uint8_t)(offset >> 8);
                    chunk.code[cAddr + 1] = (uint8_t)(offset & 0xFF);
                }
            }
        }

        // 5. Saltul final înapoi la condiție
        chunk.addByte((uint8_t)OpCode::OP_LOOP, 0);
        uint16_t loopOffset = (uint16_t)(chunk.code.size() + 2 - loopStart);
        chunk.addByte((uint8_t)(loopOffset >> 8), 0);
        chunk.addByte((uint8_t)(loopOffset & 0xFF), 0);

        // --- 6. PATCH BREAK & EXIT ---
        size_t postLoopAddr = chunk.code.size();
        if (exitJumpAddr + 1 < chunk.code.size()) {
            uint16_t exitDist = (uint16_t)(postLoopAddr - (exitJumpAddr + 2));
            chunk.code[exitJumpAddr] = (uint8_t)(exitDist >> 8);
            chunk.code[exitJumpAddr + 1] = (uint8_t)(exitDist & 0xFF);
        }

        if (breakStack.size() >= currentStackLevel) {
            std::vector<size_t> currentBreaks = breakStack.back();
            breakStack.pop_back();
            for (size_t bAddr : currentBreaks) {
                if (bAddr + 1 < chunk.code.size()) {
                    uint16_t bDist = (uint16_t)(postLoopAddr - (bAddr + 2));
                    chunk.code[bAddr] = (uint8_t)(bDist >> 8);
                    chunk.code[bAddr + 1] = (uint8_t)(bDist & 0xFF);
                }
            }
        }
        }
        // --- REPEAT ---
    else if (cmdName == L"REPEAT") {
        int untilIdx = -1, endRepeatIdx = -1, depth = 0;

        LOG_DEBUG(L"--- [REPEAT SCAN] Incepere cautare markeri (Args: " + std::to_wstring(sc.args.size()) + L") ---");

        // 1. Identificăm markerii balansat
        for (int i = 0; i < (int)sc.args.size(); ++i) {
            std::wstring argU = to_upper(sc.args[i]);

            // Logăm fiecare token pentru a vedea exact unde se „pierde” compilatorul
            // LOG_DEBUG(L"Token [" + std::to_wstring(i) + L"]: " + argU + L" (depth: " + std::to_wstring(depth) + L")");

            if (depth == 0 && argU == L"UNTIL") {
                untilIdx = i;
                LOG_DEBUG(L"   -> Gasit UNTIL la indexul " + std::to_wstring(i));
            }

            // Gestionăm adâncimea pentru a nu prinde UNTIL-uri din bucle imbricate
            if (argU == L"REPEAT" || argU == L"WHILE" || argU == L"FOR" || argU == L"IF" || argU == L"FUNC" || argU == L"PROC") {
                depth++;
            }
            else if (argU == L"ENDREPEAT" || argU == L"ENDWHILE" || argU == L"ENDFOR" || argU == L"ENDIF" || argU == L"ENDFUNC" || argU == L"ENDPROC") {
                if (depth > 0) {
                    depth--;
                }
                else if (argU == L"ENDREPEAT" && endRepeatIdx == -1) {
                    endRepeatIdx = i;
                    LOG_DEBUG(L"   -> Gasit ENDREPEAT la indexul " + std::to_wstring(i));
                    break;
                }
            }
        }

        // 2. Verificare de siguranță
        if (untilIdx == -1 || endRepeatIdx == -1) {
            LOG_ERROR(L"Structura REPEAT invalida! untilIdx: " + std::to_wstring(untilIdx) + L", endRepeatIdx: " + std::to_wstring(endRepeatIdx));
            return;
        }

        // --- SETUP STIVE ---
        breakStack.push_back({});
        continueStack.push_back({});
        size_t currentStackLevel = continueStack.size();

        size_t loopStart = chunk.code.size();
        LOG_DEBUG(L"REPEAT Start - loopStart: " + std::to_wstring(loopStart));

        // 3. Compilăm CORPUL (de la început până la UNTIL)
        LOG_DEBUG(L"REPEAT: Compilare sub-bloc CORP...");
        compileSubBlock(sc.args, 0, untilIdx, chunk, externalProcs);

        // --- PUNCTUL DE REINTRARE PENTRU CONTINUE ---
        size_t conditionStart = chunk.code.size();
        LOG_DEBUG(L"REPEAT: conditionStart (target for CONTINUE): " + std::to_wstring(conditionStart));

        // 4. Compilăm CONDIȚIA (între UNTIL și ENDREPEAT)
        LOG_DEBUG(L"REPEAT: Compilare CONDITIE...");
        std::vector<std::wstring> condTokens(sc.args.begin() + untilIdx + 1, sc.args.begin() + endRepeatIdx);
        OliExpressionParser condParser(condTokens);
        ASTPtr condAST = condParser.parse();
        if (condAST) generateFromAST(condAST, chunk, externalProcs);

        // 5. EMITERE SALTURI
        // Dacă condiția e TRUE, sărim la final (ieșim)
        chunk.addByte((uint8_t)OpCode::OP_JUMP_IF_TRUE, 0);
        size_t exitJumpAddr = chunk.code.size();
        chunk.addByte(0, 0); chunk.addByte(0, 0);

        // Dacă condiția e FALSE, sărim înapoi la loopStart
        chunk.addByte((uint8_t)OpCode::OP_LOOP, 0);
        uint16_t loopOffset = (uint16_t)(chunk.code.size() + 2 - loopStart);
        chunk.addByte((uint8_t)(loopOffset >> 8), 0);
        chunk.addByte((uint8_t)(loopOffset & 0xFF), 0);

        // --- 6. BACKPATCHING FINAL ---
        size_t postLoopAddr = chunk.code.size();
        LOG_DEBUG(L"REPEAT End - postLoopAddr: " + std::to_wstring(postLoopAddr));

        // A. Patch Ieșire (UNTIL TRUE)
        if (exitJumpAddr + 1 < chunk.code.size()) {
            uint16_t exitDist = (uint16_t)(postLoopAddr - (exitJumpAddr + 2));
            chunk.code[exitJumpAddr] = (uint8_t)(exitDist >> 8);
            chunk.code[exitJumpAddr + 1] = (uint8_t)(exitDist & 0xFF);
        }

        // B. Patch CONTINUE
        if (continueStack.size() >= currentStackLevel) {
            auto currentContinues = continueStack.back();
            continueStack.pop_back();
            LOG_DEBUG(L"REPEAT: Patch " + std::to_wstring(currentContinues.size()) + L" CONTINUE.");
            for (size_t cAddr : currentContinues) {
                if (cAddr > 0 && cAddr + 1 < chunk.code.size()) {
                    uint16_t cDist = (uint16_t)(conditionStart - (cAddr + 2));
                    chunk.code[cAddr] = (uint8_t)(cDist >> 8);
                    chunk.code[cAddr + 1] = (uint8_t)(cDist & 0xFF);
                }
            }
        }

        // C. Patch BREAK
        if (breakStack.size() >= currentStackLevel) {
            auto currentBreaks = breakStack.back();
            breakStack.pop_back();
            LOG_DEBUG(L"REPEAT: Patch " + std::to_wstring(currentBreaks.size()) + L" BREAK.");
            for (size_t bAddr : currentBreaks) {
                if (bAddr + 1 < chunk.code.size()) {
                    uint16_t bDist = (uint16_t)(postLoopAddr - (bAddr + 2));
                    chunk.code[bAddr] = (uint8_t)(bDist >> 8);
                    chunk.code[bAddr + 1] = (uint8_t)(bDist & 0xFF);
                }
            }
        }
        }

       

        else if (cmdName == L"FOR") {
            int toIdx = -1, byIdx = -1, doIdx = -1, endForIdx = -1;
            int depth = 0;

            // 1. Identificăm markerii TO, BY, DO și ENDFOR (Nesting Universal)
            for (int i = 0; i < (int)sc.args.size(); ++i) {
                std::wstring argU = to_upper(sc.args[i]);
                if (depth == 0) {
                    if (argU == L"TO") toIdx = i;
                    else if (argU == L"BY") byIdx = i;
                    else if (argU == L"DO") doIdx = i;
                }

                // Adăugăm toate structurile de control pentru un nesting corect
                if (argU == L"FOR" || argU == L"IF" || argU == L"WHILE" || argU == L"REPEAT" || argU == L"FUNC" || argU == L"PROC") {
                    depth++;
                }
                else if (argU == L"ENDFOR" || argU == L"ENDIF" || argU == L"ENDWHILE" || argU == L"ENDREPEAT" || argU == L"ENDFUNC" || argU == L"ENDPROC") {
                    if (depth > 0) {
                        depth--;
                    }
                    else if (argU == L"ENDFOR" && endForIdx == -1) {
                        endForIdx = i;
                        break;
                    }
                }
            }

            if (toIdx == -1 || doIdx == -1 || endForIdx == -1) {
                LOG_ERROR(L"Structura FOR invalida (lipseste TO, DO sau ENDFOR)");
                return;
            }

            // --- SETUP STIVE ---
            breakStack.push_back({});
            continueStack.push_back({});
            size_t currentStackLevel = continueStack.size();

            std::wstring varName = sc.args[0];
            std::wstring startVal = sc.args[2];

            // 2. Inițializare ($i = startVal)
            emitLoadOrConstant(startVal, chunk);
            emitStore(varName, chunk);

            size_t loopStart = chunk.code.size();

            // 3. Condiție de ieșire ($i > limitVal)
            emitLoadOrConstant(varName, chunk);
            int limitEnd = (byIdx != -1) ? byIdx : doIdx;
            std::vector<std::wstring> limTokens(sc.args.begin() + toIdx + 1, sc.args.begin() + limitEnd);
            ASTPtr limAST = OliExpressionParser(limTokens).parse();
            if (limAST) generateFromAST(limAST, chunk, externalProcs);

            chunk.addByte((uint8_t)OpCode::OP_GREATER, 0);
            chunk.addByte((uint8_t)OpCode::OP_JUMP_IF_TRUE, 0);
            size_t exitJumpAddr = chunk.code.size();
            chunk.addByte(0, 0); chunk.addByte(0, 0);

            // 4. Corpul buclei (Recursiv)
            compileSubBlock(sc.args, doIdx + 1, endForIdx, chunk, externalProcs);

            // --- PUNCTUL DE REINTRARE PENTRU CONTINUE ---
            // Orice CONTINUE din corp va sări AICI pentru a executa incrementarea
            size_t continueTarget = chunk.code.size();

            // 5. Incrementare ($i = $i + stepVal)
            emitLoadOrConstant(varName, chunk);
            if (byIdx != -1) {
                std::vector<std::wstring> stepTokens(sc.args.begin() + byIdx + 1, sc.args.begin() + doIdx);
                ASTPtr stepAST = OliExpressionParser(stepTokens).parse();
                if (stepAST) generateFromAST(stepAST, chunk, externalProcs);
            }
            else {
                emitConstant(vData(1.0), chunk, 0);
            }
            chunk.addByte((uint8_t)OpCode::OP_ADD, 0);
            emitStore(varName, chunk);

            // 6. Salt înapoi la condiție (Evaluare)
            chunk.addByte((uint8_t)OpCode::OP_LOOP, 0);
            uint16_t offset = (uint16_t)(chunk.code.size() + 2 - loopStart);
            chunk.addByte((uint8_t)(offset >> 8), 0);
            chunk.addByte((uint8_t)(offset & 0xFF), 0);

            // --- BACKPATCHING FINAL ---
            size_t postLoopAddr = chunk.code.size();

            // Patch EXIT Jump (JUMP_IF_TRUE)
            if (exitJumpAddr + 1 < chunk.code.size()) {
                uint16_t exitDist = (uint16_t)(postLoopAddr - (exitJumpAddr + 2));
                chunk.code[exitJumpAddr] = (uint8_t)(exitDist >> 8);
                chunk.code[exitJumpAddr + 1] = (uint8_t)(exitDist & 0xFF);
            }

            // Patch BREAK jumps (Săritură la postLoopAddr)
            if (breakStack.size() >= currentStackLevel) {
                std::vector<size_t> currentBreaks = breakStack.back();
                breakStack.pop_back();
                for (size_t bAddr : currentBreaks) {
                    if (bAddr + 1 < chunk.code.size()) {
                        uint16_t bDist = (uint16_t)(postLoopAddr - (bAddr + 2));
                        chunk.code[bAddr] = (uint8_t)(bDist >> 8);
                        chunk.code[bAddr + 1] = (uint8_t)(bDist & 0xFF);
                    }
                }
            }

            // Patch CONTINUE jumps (Săritură la continueTarget)
            if (continueStack.size() >= currentStackLevel) {
                std::vector<size_t> currentContinues = continueStack.back();
                continueStack.pop_back();
                for (size_t cAddr : currentContinues) {
                    if (cAddr > 0 && cAddr + 1 < chunk.code.size()) {
                        // În FOR, CONTINUE sare înainte la incrementare, deci rămâne OP_JUMP
                        uint16_t cDist = (uint16_t)(continueTarget - (cAddr + 2));
                        chunk.code[cAddr] = (uint8_t)(cDist >> 8);
                        chunk.code[cAddr + 1] = (uint8_t)(cDist & 0xFF);
                    }
                }
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

/*
void OliCompiler::generateFromAST(ASTPtr node, OliChunk& chunk, const std::unordered_map<std::wstring, ByteCodeProcedure>& externalProcs) {
    if (!node) return;

    // --- PROTECȚIE COPII ---
    auto getChild = [&](int idx) -> ASTPtr {
        if (idx < (int)node->children.size()) return node->children[idx];
        return nullptr;
        };

    

    // --- 1. ATRIBUIRE (=, +=, -=, *=, /=) ---
    if (node->type == ASTNodeType::Operator && (node->value == L"=" ||
        node->value == L"+=" || node->value == L"-=" ||
        node->value == L"*=" || node->value == L"/=")) {

        if (node->children.size() < 2) return; // SIGURANȚĂ

        std::wstring rawLHS = reconstructRawName(node->children[0]);

        if (node->value == L"=") {
            generateFromAST(node->children[1], chunk, externalProcs);
            emitStore(rawLHS, chunk);
            return;
        }

        generateFromAST(node->children[0], chunk, externalProcs);
        generateFromAST(node->children[1], chunk, externalProcs);

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

        generateFromAST(node->children[0], chunk, externalProcs);

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
                generateFromAST(child, chunk, externalProcs);
            }
            // 2. Emitem OP_ARRAY urmat de numărul de elemente
            chunk.addByte((uint8_t)OpCode::OP_ARRAY, 0);
            chunk.addByte((uint8_t)node->children.size(), 0);
            return;
        }
        else if (node->value == L"MAP_OBJECT") {
            // 1. Evaluăm fiecare pereche (Cheie apoi Valoare)
            for (auto& child : node->children) {
                generateFromAST(child, chunk, externalProcs);
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

    // --- 3.5 APELURI DE FUNCȚII (INTELIGENT: NATIVE vs USER) ---
    // --- În OliCompiler::generateFromAST ---
    
    if (node->type == ASTNodeType::FunctionCall) {
        bool isDynamic = (node->value == L"DYNAMIC_CALL");

        // 1. VALIDARE DE BAZĂ
        // Un apel dinamic are nevoie de cel puțin un copil (funcția sau variabila apelată)
        if (isDynamic && node->children.empty()) return;

        size_t startIdx = isDynamic ? 1 : 0;
        size_t totalChildren = node->children.size();

        // 2. ÎNCĂRCARE ARGUMENTE
        // Generăm codul pentru fiecare argument. Acestea vor ajunge pe stiva VM.
        for (size_t i = startIdx; i < totalChildren; ++i) {
            generateFromAST(node->children[i], chunk, externalProcs);
        }

        // 3. EXTRAGERE ȘI CURĂȚARE NUME
        // Ne asigurăm că funcția are un nume valid și fără spații accidentale
        std::wstring rawName = isDynamic ? node->children[0]->value : node->value;
        std::wstring funcName = to_upper(trim(rawName));

        if (funcName.empty()) {
            // Dacă numele e gol, ceva a eșuat în parser (ex: cazul de la IP 0090)
            // Punem un nume de fallback pentru a evita crash-ul VM-ului la căutarea în map
            funcName = L"__INVALID_CALL__";
        }

        // 4. CALCULARE ARGCOUNT (Sursă de Segfault rezolvată)
        // Ne asigurăm că argCount reflectă EXACT numărul de noduri procesate la pasul 2
        size_t actualArgs = (totalChildren > startIdx) ? (totalChildren - startIdx) : 0;

        // uint8_t suportă max 255. Dacă parserul a luat-o razna, tăiem la 255
        // pentru a nu corupe restul bytecode-ului (overflow la 0)
        uint8_t argCount = (actualArgs > 255) ? 255 : (uint8_t)actualArgs;

        uint16_t nameIdx = chunk.addConstant(vData(funcName));

        // 5. DECIZIA DE APEL (INTERNAL vs NATIVE)
        // Verificăm în chunk-ul curent SAU în contextul primit de la părinte (externalProcs)
        if (chunk.procedures.count(funcName) || externalProcs.count(funcName)) {
            // Funcție definită în script (Oli)
            chunk.addByte((uint8_t)OpCode::OP_CALL, 0);
        }
        else {
            // Funcție din C++ sau Plugin (Native)
            chunk.addByte((uint8_t)OpCode::OP_CALL_NATIVE, 0);
        }

        // 6. EMITERE INSTRUCȚIUNE
        // Structura: OpCode (1b) + NameIdx (2b) + ArgCount (1b)
        chunk.addByte((uint8_t)((nameIdx >> 8) & 0xFF), 0);
        chunk.addByte((uint8_t)(nameIdx & 0xFF), 0);
        chunk.addByte(argCount, 0);

        return;
    }

    // --- 4. OPERATORI BINARI ---
    if (node->children.size() == 2) {
        if (node->value == L"&&" || node->value == L"||") {
            generateShortCircuit(node, chunk);
            return;
        }

        generateFromAST(node->children[0], chunk, externalProcs);
        generateFromAST(node->children[1], chunk, externalProcs);

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
*/
void OliCompiler::generateFromAST(ASTPtr node, OliChunk& chunk, const std::unordered_map<std::wstring, ByteCodeProcedure>& externalProcs) {
    if (!node) return;

    // --- 1. ATRIBUIRE (=, +=, -=, *=, /=) ---
    if (node->type == ASTNodeType::Operator && (node->value == L"=" ||
        node->value == L"+=" || node->value == L"-=" ||
        node->value == L"*=" || node->value == L"/=")) {

        if (node->children.size() < 2) return;

        ASTPtr lhs = node->children[0];
        ASTPtr rhs = node->children[1];

        // FIX CRITIC: Verificăm dacă scriem într-un Array sau Map ($vx[i] = ...)
        if (node->value == L"=" && lhs->type == ASTNodeType::Operator && (lhs->value == L"INDEX" || lhs->value == L"DOT")) {
            // 1. Punem Containerul pe stivă (ex: $vx)
            generateFromAST(lhs->children[0], chunk, externalProcs);
            // 2. Punem Indexul/Cheia pe stivă (ex: $i)
            generateFromAST(lhs->children[1], chunk, externalProcs);
            // 3. Punem Valoarea pe stivă
            generateFromAST(rhs, chunk, externalProcs);

            // 4. Emitem OP_SET_INDIRECT care așteaptă [Container, Index, Value]
            chunk.addByte((uint8_t)OpCode::OP_SET_INDIRECT, 0);
            return;
        }

        // Atribuire normală sau compusă
        std::wstring rawLHS = reconstructRawName(lhs);

        if (node->value == L"=") {
            generateFromAST(rhs, chunk, externalProcs);
            emitStore(rawLHS, chunk);
            return;
        }

        // Operatori compuși (+=, -=, etc.)
        generateFromAST(lhs, chunk, externalProcs);
        generateFromAST(rhs, chunk, externalProcs);

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

        generateFromAST(node->children[0], chunk, externalProcs);

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

    // --- 3. LITERALI ȘI VARIABILE ---
    if (node->type == ASTNodeType::Literal) {
        if (node->value == L"ARRAY_OBJECT") {
            for (auto& child : node->children) generateFromAST(child, chunk, externalProcs);
            chunk.addByte((uint8_t)OpCode::OP_ARRAY, 0);
            chunk.addByte((uint8_t)node->children.size(), 0);
            return;
        }
        else if (node->value == L"MAP_OBJECT") {
            for (auto& child : node->children) generateFromAST(child, chunk, externalProcs);
            chunk.addByte((uint8_t)OpCode::OP_MAP, 0);
            chunk.addByte((uint8_t)(node->children.size() / 2), 0);
            return;
        }
        emitLoadOrConstant(node->value, chunk);
        return;
    }

    if (node->type == ASTNodeType::Variable) {
        emitLoadOrConstant(node->value, chunk);
        return;
    }

    // --- 4. APELURI DE FUNCȚII (Inclusiv TYPE()) ---
    if (node->type == ASTNodeType::FunctionCall) {
        bool isDynamic = (node->value == L"DYNAMIC_CALL");
        if (isDynamic && node->children.empty()) return;

        std::wstring rawName = isDynamic ? node->children[0]->value : node->value;
        std::wstring funcName = to_upper(trim(rawName));

        // INTRINSIC: TYPE(expr)
        if (funcName == L"TYPE") {
            generateFromAST(node->children[isDynamic ? 1 : 0], chunk, externalProcs);
            chunk.addByte((uint8_t)OpCode::OP_TYPE, 0);
            return;
        }

        // Apel normal
        size_t startIdx = isDynamic ? 1 : 0;
        for (size_t i = startIdx; i < node->children.size(); ++i) {
            generateFromAST(node->children[i], chunk, externalProcs);
        }

        if (funcName.empty()) funcName = L"__INVALID_CALL__";
#undef min
        uint8_t argCount = (uint8_t)std::min((size_t)255, node->children.size() - startIdx);
        uint16_t nameIdx = chunk.addConstant(vData(funcName));

        if (chunk.procedures.count(funcName) || externalProcs.count(funcName)) {
            chunk.addByte((uint8_t)OpCode::OP_CALL, 0);
        }
        else {
            chunk.addByte((uint8_t)OpCode::OP_CALL_NATIVE, 0);
        }

        chunk.addByte((uint8_t)((nameIdx >> 8) & 0xFF), 0);
        chunk.addByte((uint8_t)(nameIdx & 0xFF), 0);
        chunk.addByte(argCount, 0);
        return;
    }

    // --- 5. OPERATORI BINARI ---
    if (node->children.size() == 2) {
        if (node->value == L"&&" || node->value == L"||") {
            generateShortCircuit(node, chunk);
            return;
        }

        generateFromAST(node->children[0], chunk, externalProcs);
        generateFromAST(node->children[1], chunk, externalProcs);

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
        else if (op == L"DOT" || op == L"INDEX") {
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
    //if (!varName.empty() && varName[0] == L'*') {
    if (varName[0] == L'*') {
        if (varName.size() < 2) return;


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
    const std::unordered_map<std::wstring, ByteCodeProcedure>& externalProcs  = {}; // Nu avem nevoie de proceduri externe pentru această funcție
    
    bool isAnd = (node->value == L"&&");

    // 1. Evaluăm partea stângă (rezultatul rămâne pe stivă)
    generateFromAST(node->children[0], chunk, externalProcs);

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
    generateFromAST(node->children[1], chunk, externalProcs );

    // 5. Backpatching: setăm adresa de salt la instrucțiunea de după evaluarea dreptei
    uint16_t offset = (uint16_t)(chunk.code.size() - (jumpAddr + 2));
    chunk.code[jumpAddr] = (uint8_t)(offset >> 8);
    chunk.code[jumpAddr + 1] = (uint8_t)(offset & 0xFF);
}

std::wstring OliCompiler::reconstructRawName(ASTPtr node) {
    if (!node) return L"";

    // Dacă e variabilă (ex: "$a", "$$b", "$$$ptrToPtr"), returnăm valoarea direct
    if (node->type == ASTNodeType::Variable) return node->value;

    if (node->children.empty()) return node->value;

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

void OliCompiler::compileSubBlock(const std::vector<std::wstring>& args,
    int start,
    int end,
    OliChunk& chunk,
    const std::unordered_map<std::wstring, ByteCodeProcedure>& externalProcs) {

    if (start >= end) return;

    // 1. REȚINEM CONTEXTUL ADRESELOR ȘI STIVELOR
    // 'baseAddress' reprezintă offset-ul unde va fi inserat noul bytecode în chunk-ul părinte
    size_t baseAddress = chunk.code.size();

    // Reținem dimensiunea actuală a stivelor pentru a ajusta doar noile intrări adăugate în acest sub-bloc
    size_t startBreakIdx = (!breakStack.empty()) ? breakStack.back().size() : 0;
    size_t startContinueIdx = (!continueStack.empty()) ? continueStack.back().size() : 0;

    // 2. RECONSTRUIM SURSA SUB-BLOCULUI
    std::wstring subSource = rebuildSubCommand(args, start, end) + L"\n";

    // 3. COMPILARE RECURSIVĂ
    // IMPORTANT: Transmitem 'true' pentru isSubBlock pentru a preveni golirea stivelor în funcția compile()
    OliChunk subChunk = this->compile(subSource, externalProcs, true);

    // 4. MIGRARE PROCEDURI GENERATE ÎN SUB-BLOC
    for (auto const& [name, proc] : subChunk.procedures) {
        chunk.procedures[name] = proc;
    }

    // 5. REMAPARE CONSTANTE (Migrăm constantele din subChunk în chunk-ul principal)
    std::map<uint16_t, uint16_t> indexMap;
    for (uint16_t i = 0; i < (uint16_t)subChunk.constants.size(); ++i) {
        indexMap[i] = chunk.addConstant(subChunk.constants[i]);
    }

    // 6. COPIERE ȘI REMAPARE BYTECODE
    size_t i = 0;
    while (i < subChunk.code.size()) {
        uint8_t op = subChunk.code[i];
        chunk.addByte(op, 0);
        i++;

        // Operanzi care fac referire la tabela de constante (2 bytes)
        if (op == (uint8_t)OpCode::OP_CONSTANT || op == (uint8_t)OpCode::OP_GET_GLOBAL ||
            op == (uint8_t)OpCode::OP_SET_GLOBAL || op == (uint8_t)OpCode::OP_UNSET ||
            op == (uint8_t)OpCode::OP_GET_ADDR || op == (uint8_t)OpCode::OP_PLUGIN ||
            op == (uint8_t)OpCode::OP_CALL_NATIVE || op == (uint8_t)OpCode::OP_CALL)
        {
            if (i + 1 >= subChunk.code.size()) break;

            uint16_t oldIdx = (uint16_t)((subChunk.code[i] << 8) | subChunk.code[i + 1]);
            uint16_t newIdx = indexMap[oldIdx];

            chunk.addByte((uint8_t)(newIdx >> 8), 0);
            chunk.addByte((uint8_t)(newIdx & 0xFF), 0);
            i += 2;

            // Gestionare argument suplimentar pentru apeluri
            if (op == (uint8_t)OpCode::OP_CALL_NATIVE || op == (uint8_t)OpCode::OP_CALL) {
                if (i < subChunk.code.size()) chunk.addByte(subChunk.code[i++], 0);
            }
        }
        // Operanzi cu 1 byte (Array/Map size)
        else if (op == (uint8_t)OpCode::OP_ARRAY || op == (uint8_t)OpCode::OP_MAP) {
            if (i < subChunk.code.size()) chunk.addByte(subChunk.code[i++], 0);
        }
        // Salturi (JUMP/LOOP) - 2 bytes. Offset-urile rămân relative la codul propriu.
        else if (op == (uint8_t)OpCode::OP_JUMP || op == (uint8_t)OpCode::OP_JUMP_IF_FALSE ||
            op == (uint8_t)OpCode::OP_LOOP || op == (uint8_t)OpCode::OP_JUMP_IF_TRUE)
        {
            if (i + 1 < subChunk.code.size()) {
                chunk.addByte(subChunk.code[i++], 0);
                chunk.addByte(subChunk.code[i++], 0);
            }
        }
    }

    // 7. AJUSTARE ADRESE PENTRU BACKPATCHING (BREAK & CONTINUE)
    // Deoarece am concatenat bytecode-ul sub-blocului la chunk-ul principal,
    // adresele placeholder emise în timpul compilării recursive trebuie translatate.

    if (!breakStack.empty()) {
        for (size_t k = startBreakIdx; k < breakStack.back().size(); ++k) {
            breakStack.back()[k] += baseAddress;
        }
    }

    if (!continueStack.empty()) {
        for (size_t k = startContinueIdx; k < continueStack.back().size(); ++k) {
            continueStack.back()[k] += baseAddress;
        }
    }
}


std::wstring OliCompiler::cleanVariableName(const std::wstring& name) {
    if (name.empty()) return L"";

    std::wstring clean = name;
    // Eliminăm prefixele de tip $, @ sau alte caractere de control de la început
    if (clean[0] == L'$' || clean[0] == L'@') {
        return clean.substr(1);
    }
    return clean;
}

