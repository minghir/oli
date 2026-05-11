#include "../vDataSerialize.hpp"
#include "OliCompiler.hpp"
#include "../OliCommandParser.hpp"
#include "../StringUtils.hpp"
#include "../PortTools.hpp"
#include <iostream>
#undef min
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
            if (ut == L"IF" || ut == L"WHILE" || ut == L"FOR" || ut == L"FUNC" || ut == L"REPEAT" || ut == L"CYCLE" || ut == L"SWITCH" || ut == L"PROC") depth++;

            // Adăugăm ENDREPEAT aici
            if (ut == L"ENDIF" || ut == L"ENDWHILE" || ut == L"ENDFOR" || ut == L"ENDFUNC" || ut == L"ENDREPEAT" || ut == L"ENDCYCLE" || ut == L"ENDSWITCH" || ut == L"ENDPROC") depth--;

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

    vOliKeyWords::populateNativeFunctions();

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

        // --- 0. PREPROCESARE: INCLUDE ---
        std::wstring upperLine = to_upper(cleanLine);
        if (upperLine.find(L"INCLUDE") == 0) {
            size_t startQuote = cleanLine.find(L"\"");
            size_t endQuote = cleanLine.find_last_of(L"\"");

            if (startQuote != std::wstring::npos && endQuote != std::wstring::npos && startQuote < endQuote) {
                std::wstring path = cleanLine.substr(startQuote + 1, endQuote - startQuote - 1);
                LOG_DEBUG(L"[PREPROCESSOR] Includem sursa: " + path);

                std::ifstream file;
                PortTools::openIfstream(file, path);
                if (file.is_open()) {
                    std::stringstream buffer;
                    buffer << file.rdbuf();
                    std::wstring includedSource = PortTools::utf8_to_wstring(buffer.str());

                    // Compilăm recursiv
                    OliChunk includedChunk = this->compile(includedSource, parentProcs, true);

                    // --- PASUL CRITIC 1: Remapare Constante ---
                    std::map<uint16_t, uint16_t> indexMap;
                    for (uint16_t i = 0; i < (uint16_t)includedChunk.constants.size(); ++i) {
                        indexMap[i] = chunk.addConstant(includedChunk.constants[i]);
                    }

                    // --- PASUL CRITIC 2: Migrare Bytecode cu corecție de indici ---
                    size_t i = 0;
                    // Nu copiem OP_RETURN-ul de la finalul fișierului inclus
                    size_t limit = includedChunk.code.size();
                    if (!includedChunk.code.empty() && includedChunk.code.back() == (uint8_t)OpCode::OP_RETURN) {
                        limit--;
                    }

                    while (i < limit) {
                        uint8_t op = includedChunk.code[i];
                        chunk.addByte(op, 0);
                        i++;

                        // Instrucțiuni care folosesc 2 bytes pentru indici de constante
                        if (op == (uint8_t)OpCode::OP_CONSTANT || op == (uint8_t)OpCode::OP_GET_GLOBAL ||
                            op == (uint8_t)OpCode::OP_SET_GLOBAL || op == (uint8_t)OpCode::OP_UNSET ||
                            op == (uint8_t)OpCode::OP_GET_ADDR || op == (uint8_t)OpCode::OP_PLUGIN ||
                            op == (uint8_t)OpCode::OP_CALL_NATIVE || op == (uint8_t)OpCode::OP_CALL)
                        {
                            uint16_t oldIdx = (uint16_t)((includedChunk.code[i] << 8) | includedChunk.code[i + 1]);
                            uint16_t newIdx = indexMap[oldIdx];
                            chunk.code[chunk.code.size() - 0] = (uint8_t)(newIdx >> 8); // Rescriem High Byte (deja adăugat un placeholder sau addByte)
                            // Corecție: addByte a pus deja OP-ul, acum punem cei 2 bytes corecți
                            chunk.code.pop_back(); // Scoatem byte-ul adăugat greșit mai sus dacă e cazul, sau gestionăm flow-ul:

                            // Versiune curată de emisie:
                            chunk.addByte((uint8_t)(newIdx >> 8), 0);
                            chunk.addByte((uint8_t)(newIdx & 0xFF), 0);
                            i += 2;

                            if (op == (uint8_t)OpCode::OP_CALL_NATIVE || op == (uint8_t)OpCode::OP_CALL) {
                                chunk.addByte(includedChunk.code[i++], 0); // ArgCount
                            }
                        }
                        // Salturi (JUMP/LOOP) - se copiază ca atare (sunt relative)
                        else if (op == (uint8_t)OpCode::OP_JUMP || op == (uint8_t)OpCode::OP_JUMP_IF_FALSE ||
                            op == (uint8_t)OpCode::OP_JUMP_IF_TRUE || op == (uint8_t)OpCode::OP_LOOP) {
                            chunk.addByte(includedChunk.code[i++], 0);
                            chunk.addByte(includedChunk.code[i++], 0);
                        }
                        // Instrucțiuni cu 1 byte argument (Array/Map/CallMethod)
                        else if (op == (uint8_t)OpCode::OP_ARRAY || op == (uint8_t)OpCode::OP_MAP ||
                            op == (uint8_t)OpCode::OP_CALL_METHOD || op == (uint8_t)OpCode::OP_CALL_DYNAMIC) {
                            chunk.addByte(includedChunk.code[i++], 0);
                        }
                    }

                    // Migrăm procedurile în tabela principală
                    for (auto const& [name, proc] : includedChunk.procedures) {
                        chunk.procedures[name] = proc;
                    }
                    continue;
                }
            }
        }

        // --- 1. MASCARE & COMENTARII ---
        /*
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
        //auto tokens = splitW(to_upper(maskedLine), L" \t\n\r();,");
        auto nestingTokens = vOliCommandParser::tokenize(maskedLine);
        for (const auto& t : nestingTokens) {
            // Adăugăm REPEAT
            if (t == L"IF" || t == L"WHILE" || t == L"FOR" || t == L"PROC" || t == L"FUNC" || t == L"REPEAT" || t == L"CYCLE") nestingLevel++;

            // Adăugăm ENDREPEAT
            if (t == L"ENDIF" || t == L"ENDWHILE" || t == L"ENDFOR" || t == L"ENDPROC" || t == L"ENDFUNC" || t == L"ENDREPEAT" || t == L"ENDCYCLE") nestingLevel--;

            if (t == L"{" || t == L"[") bracketDepth++;
            if (t == L"}" || t == L"]") bracketDepth--;
        }
        */

        // --- 1. MASCARE & COMENTARII (Versiunea Corectă) ---
        bool lineInQuotes = false;
        std::wstring maskedLine = cleanLine;
        for (size_t i = 0; i < maskedLine.length(); ++i) {
            if (maskedLine[i] == L'"' && (i == 0 || maskedLine[i - 1] != L'\\')) {
                lineInQuotes = !lineInQuotes;
                continue; // Nu mascăm caracterul ghilimele în sine!
            }
            if (lineInQuotes) maskedLine[i] = L' ';
        }

        size_t commentPos = maskedLine.find(L'#');
        if (commentPos != std::wstring::npos) {
            cleanLine = trim(cleanLine.substr(0, commentPos));
            maskedLine = trim(maskedLine.substr(0, commentPos));
        }
        if (cleanLine.empty()) continue;

        // --- 2. ACTUALIZARE ADÂNCIME (Nesting) ---
        auto nestingTokens = vOliCommandParser::tokenize(maskedLine);
        for (const auto& rawT : nestingTokens) {
            std::wstring t = to_upper(rawT); // IMPORTANT: Lucrăm cu majuscule aici

            if (t == L"IF" || t == L"WHILE" || t == L"FOR" || t == L"PROC" || t == L"FUNC" || t == L"REPEAT" || t == L"CYCLE" || t == L"SWITCH" || t == L"PROC") nestingLevel++;
            if (t == L"ENDIF" || t == L"ENDWHILE" || t == L"ENDFOR" || t == L"ENDPROC" || t == L"ENDFUNC" || t == L"ENDREPEAT" || t == L"ENDCYCLE" || t == L"ENDSWITCH" || t == L"ENDPROC") nestingLevel--;

            if (rawT == L"{" || rawT == L"[") bracketDepth++;
            if (rawT == L"}" || rawT == L"]") bracketDepth--;
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
    LOG_DEBUG(L"[DEBUG] compileStatement primeste: " + cmdName + L" | Valid: " + (sc.isValid ? L"DA" : L"NU"));
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
    /*
    else if (cmdName == L"DEF") {
        if (sc.args.size() < 3) return;

        // 1. struct / class
        std::wstring subType = to_lower(sc.args[0]);
        bool isClass = (subType == L"class");

        // 2. Numele Tipului
        std::wstring typeName = sc.args[1];
        uint16_t nameIdx = chunk.addConstant(vData(typeName));

        // 3. Extragerea Câmpurilor (între { și })
        std::vector<uint16_t> fieldIndices;
        bool inBraces = false;

        for (size_t i = 2; i < sc.args.size(); ++i) {
            if (sc.args[i] == L"{") { inBraces = true; continue; }
            if (sc.args[i] == L"}") { inBraces = false; break; }

            if (inBraces && sc.args[i] != L",") {
                fieldIndices.push_back(chunk.addConstant(vData(sc.args[i])));
            }
        }

        // 4. Emiterea Bytecode-ului
        chunk.addByte((uint8_t)OpCode::OP_DEF_TYPE, 0);
        chunk.addByte((uint8_t)(nameIdx >> 8), 0);
        chunk.addByte((uint8_t)(nameIdx & 0xFF), 0);
        chunk.addByte(isClass ? 1 : 0, 0);
        chunk.addByte((uint8_t)fieldIndices.size(), 0);

        for (uint16_t fIdx : fieldIndices) {
            chunk.addByte((uint8_t)(fIdx >> 8), 0);
            chunk.addByte((uint8_t)(fIdx & 0xFF), 0);
        }

        LOG_DEBUG(L"[COMPILER] DEF Type: " + typeName + L" with " + std::to_wstring(fieldIndices.size()) + L" fields.");
    }
    */
    else if (cmdName == L"DEF") {
        if (sc.args.size() < 3) return;

        std::wstring subType = to_lower(sc.args[0]);
        bool isClass = (subType == L"class");
        std::wstring typeName = sc.args[1];
        std::wstring typeNameUpper = to_upper(typeName); // Normalizăm numele clasei

        uint16_t nameIdx = chunk.addConstant(vData(typeNameUpper));

        std::vector<uint16_t> fieldIndices;
        std::vector<uint16_t> methodIndices;
        bool inBraces = false;

        for (size_t i = 2; i < sc.args.size(); ++i) {
            std::wstring token = sc.args[i];
            if (token == L"{") { inBraces = true; continue; }
            if (token == L"}") { inBraces = false; break; }

            // Curățăm token-ul de virgule reziduale (ex: "hp," -> "hp")
            while (!token.empty() && (token.back() == L',' || token.back() == L' ')) token.pop_back();
            if (!inBraces || token.empty()) continue;

            // --- LOGICA DE DETECȚIE METODĂ ---
            size_t openParen = token.find(L'(');
            if (openParen != std::wstring::npos) {
                // Cazul: ataca() sau ataca($val)
                std::wstring methodName = to_upper(trim(token.substr(0, openParen)));
                if (!methodName.empty()) {
                    methodIndices.push_back(chunk.addConstant(vData(methodName)));
                }
            }
            else if (i + 1 < sc.args.size() && sc.args[i + 1][0] == L'(') {
                // Cazul: ataca ($val)
                std::wstring methodName = to_upper(trim(token));
                methodIndices.push_back(chunk.addConstant(vData(methodName)));
                // Sărim peste parametri până la închiderea metodei
                while (i < sc.args.size() && sc.args[i].find(L')') == std::wstring::npos) {
                    i++;
                }
            }
            else {
                // Este un câmp simplu (dată)
                fieldIndices.push_back(chunk.addConstant(vData(to_lower(token))));
            }
        }

        // --- EMITERE BYTECODE (Contractul cu VM & Disassembler) ---
        chunk.addByte((uint8_t)OpCode::OP_DEF_TYPE, 0);
        chunk.addByte((uint8_t)(nameIdx >> 8), 0);
        chunk.addByte((uint8_t)(nameIdx & 0xFF), 0);
        chunk.addByte(isClass ? 1 : 0, 0);

        // --- 1. METADATELE (Ordinea contează pentru dezasamblator!) ---
        chunk.addByte((uint8_t)fieldIndices.size(), 0);
        chunk.addByte((uint8_t)methodIndices.size(), 0); // Scriem methodCount imediat după fieldCount

        // --- 2. DATELE (Array-urile de indici) ---
        // Scriem toți indicii câmpurilor
        for (uint16_t fIdx : fieldIndices) {
            chunk.addByte((uint8_t)(fIdx >> 8), 0);
            chunk.addByte((uint8_t)(fIdx & 0xFF), 0);
        }

        // Scriem toți indicii metodelor
        for (uint16_t mIdx : methodIndices) {
            chunk.addByte((uint8_t)(mIdx >> 8), 0);
            chunk.addByte((uint8_t)(mIdx & 0xFF), 0);
        }

        LOG_DEBUG(L"[COMPILER] DEF " + typeNameUpper +
            L" - Câmpuri: " + std::to_wstring(fieldIndices.size()) +
            L", Metode: " + std::to_wstring(methodIndices.size()));
    }

    else if (cmdName == L"PROC") {
        if (sc.args.empty()) return;

        // 1. Identificăm Numele (Primul argument)
        std::wstring procName = to_upper(sc.args[0]);

        // Eliminăm eventuale caractere reziduale (curățare similară cu interpretorul)
        procName.erase(std::remove_if(procName.begin(), procName.end(), [](wchar_t c) {
            return c == L'(' || c == L')' || c == L':';
            }), procName.end());

        // 2. Delimităm corpul căutând ENDPROC (Nesting Universal)
        int bodyEnd = -1;
        int depth = 1;
        for (int i = 1; i < (int)sc.args.size(); ++i) {
            std::wstring argU = to_upper(sc.args[i]);
            if (argU == L"PROC") depth++;
            if (argU == L"ENDPROC") {
                depth--;
                if (depth == 0) { bodyEnd = i; break; }
            }
        }
        if (bodyEnd == -1) bodyEnd = (int)sc.args.size();

        // 3. Pregătim obiectul procedurii
        ByteCodeProcedure proc;
        proc.name = procName;
        proc.compiledBody = std::make_shared<OliChunk>();

        // 4. Extragem Parametrii
        // Mergem de la indexul 1 până când dăm de prima instrucțiune sau un marker de start
        int bodyStart = 1;
        for (int i = 1; i < bodyEnd; ++i) {
            std::wstring arg = sc.args[i];
            // Dacă token-ul începe cu $ sau @, e un parametru
            if (arg[0] == L'$' || arg[0] == L'@' || arg.find(L',') != std::wstring::npos) {
                std::wstring cleaned = this->cleanVariableName(arg);
                if (!cleaned.empty()) {
                    proc.params.push_back(cleaned);
                }
                bodyStart = i + 1;
            }
            else {
                // Am dat de cod (instrucțiune), aici se opresc parametrii
                bodyStart = i;
                break;
            }
        }

        LOG_DEBUG(L"[PROC] Înregistrare: " + procName + L" cu " + std::to_wstring(proc.params.size()) + L" parametri.");

        // 5. Compilare Corp (Recursiv)
        // Pasăm chunk.procedures pentru a permite recursivitatea (procedura se vede pe ea însăși)
        chunk.procedures[procName] = proc;

        compileSubBlock(sc.args, bodyStart, bodyEnd, *(proc.compiledBody), chunk.procedures);

        // 6. Return implicit pentru siguranță
        if (proc.compiledBody->code.empty() || proc.compiledBody->code.back() != (uint8_t)OpCode::OP_RETURN) {
            proc.compiledBody->addByte((uint8_t)OpCode::OP_RETURN, 0);
        }

        // Actualizăm varianta finală în map
        chunk.procedures[procName] = proc;
    }

    else if (cmdName == L"SWITCH") {
        // 1. Găsim markerii (CASE, DEFAULT, ENDSWITCH) cu Nesting Corect
        std::vector<int> caseIndices;
        int defaultIdx = -1, endSwitchIdx = -1, depth = 0;

        for (int i = 0; i < (int)sc.args.size(); ++i) {
            std::wstring argU = to_upper(sc.args[i]);
            if (depth == 0) {
                if (argU == L"CASE") caseIndices.push_back(i);
                else if (argU == L"DEFAULT") defaultIdx = i;
            }

            if (argU == L"IF" || argU == L"WHILE" || argU == L"FOR" || argU == L"REPEAT" || argU == L"FUNC" || argU == L"PROC" || argU == L"CYCLE" || argU == L"SWITCH" || argU == L"PROC") {
                depth++;
            }
            else if (argU == L"ENDIF" || argU == L"ENDWHILE" || argU == L"ENDFOR" || argU == L"ENDREPEAT" || argU == L"ENDFUNC" || argU == L"ENDPROC" || argU == L"ENDCYCLE" || argU == L"ENDSWITCH" || argU == L"ENDPROC") {
                if (depth > 0) depth--;
                else if (argU == L"ENDSWITCH" && endSwitchIdx == -1) {
                    endSwitchIdx = i;
                    break;
                }
            }
        }

        if (endSwitchIdx == -1) {
            LOG_ERROR(L"SWITCH fără ENDSWITCH!");
            return;
        }

        // --- SETUP STIVĂ PENTRU BREAK ---
        breakStack.push_back({}); // Orice BREAK din interior va sări la finalul switch-ului
        size_t currentStackLevel = breakStack.size();

        // 2. Evaluăm expresia de control (ex: SWITCH $x)
        int firstMarker = !caseIndices.empty() ? caseIndices[0] : (defaultIdx != -1 ? defaultIdx : endSwitchIdx);
        std::vector<std::wstring> ctrlTokens(sc.args.begin(), sc.args.begin() + firstMarker);
        ASTPtr ctrlAST = OliExpressionParser(ctrlTokens).parse();
        if (ctrlAST) generateFromAST(ctrlAST, chunk, externalProcs);

        // Acum avem Valoarea de Control pe stivă.
        std::vector<size_t> nextCaseJumps; // Patch-uri pentru JUMP_IF_FALSE (când case-ul nu se potrivește)

        // 3. Procesăm fiecare CASE
        for (size_t i = 0; i < caseIndices.size(); ++i) {
            // Dacă am avut un case anterior, îi patch-uim saltul de "fail" aici
            if (!nextCaseJumps.empty()) {
                size_t patchAddr = nextCaseJumps.back();
                nextCaseJumps.pop_back();
                uint16_t dist = (uint16_t)(chunk.code.size() - (patchAddr + 2));
                chunk.code[patchAddr] = (uint8_t)(dist >> 8);
                chunk.code[patchAddr + 1] = (uint8_t)(dist & 0xFF);
            }

            // Comparația: DUP (control) == (valoare_case)
            chunk.addByte((uint8_t)OpCode::OP_DUP, 0);

            // Găsim sfârșitul expresiei CASE (până la următorul marker sau corp)
            int caseExprEnd = caseIndices[i] + 2; // Presupunem CASE <val> <restul...>
            std::vector<std::wstring> valTokens = { sc.args[caseIndices[i] + 1] };
            ASTPtr valAST = OliExpressionParser(valTokens).parse();
            if (valAST) generateFromAST(valAST, chunk, externalProcs);

            chunk.addByte((uint8_t)OpCode::OP_EQUAL, 0);
            chunk.addByte((uint8_t)OpCode::OP_JUMP_IF_FALSE, 0);
            size_t failJumpAddr = chunk.code.size();
            chunk.addByte(0, 0); chunk.addByte(0, 0);
            nextCaseJumps.push_back(failJumpAddr);

            // Compilăm corpul case-ului (până la următorul CASE/DEFAULT/ENDSWITCH)
            int bodyStart = caseIndices[i] + 2;
            int bodyEnd = (i + 1 < caseIndices.size()) ? caseIndices[i + 1] : (defaultIdx != -1 ? defaultIdx : endSwitchIdx);
            compileSubBlock(sc.args, bodyStart, bodyEnd, chunk, externalProcs);
        }

        // 4. DEFAULT (Dacă există)
        if (defaultIdx != -1) {
            // Patch ultimul JUMP_IF_FALSE să vină aici
            if (!nextCaseJumps.empty()) {
                size_t patchAddr = nextCaseJumps.back();
                nextCaseJumps.pop_back();
                uint16_t dist = (uint16_t)(chunk.code.size() - (patchAddr + 2));
                chunk.code[patchAddr] = (uint8_t)(dist >> 8);
                chunk.code[patchAddr + 1] = (uint8_t)(dist & 0xFF);
            }
            compileSubBlock(sc.args, defaultIdx + 1, endSwitchIdx, chunk, externalProcs);
        }
        else {
            // Dacă nu avem DEFAULT, patch-uim ultimul salt să meargă la final
            if (!nextCaseJumps.empty()) {
                size_t patchAddr = nextCaseJumps.back();
                nextCaseJumps.pop_back();
                uint16_t dist = (uint16_t)(chunk.code.size() + 1 - (patchAddr + 2)); // +1 pentru POP-ul de la final
                chunk.code[patchAddr] = (uint8_t)(dist >> 8);
                chunk.code[patchAddr + 1] = (uint8_t)(dist & 0xFF);
            }
        }

        // 5. Curățenie Stivă (Scoatem Valoarea de Control)
        size_t postSwitchAddr = chunk.code.size();
        chunk.addByte((uint8_t)OpCode::OP_POP, 0);

        // 6. Backpatch BREAK-uri
        if (breakStack.size() >= currentStackLevel) {
            auto currentBreaks = breakStack.back();
            breakStack.pop_back();
            for (size_t bAddr : currentBreaks) {
                uint16_t bDist = (uint16_t)(postSwitchAddr - (bAddr + 2));
                chunk.code[bAddr] = (uint8_t)(bDist >> 8);
                chunk.code[bAddr + 1] = (uint8_t)(bDist & 0xFF);
            }
        }
    }

    else if (cmdName == L"CYCLE") {
        LOG_DEBUG(L"[CYCLE] Detectat! Incepere scanare markeri...");
        int asIdx = -1, doIdx = -1, endCycleIdx = -1, depth = 0;

        for (int i = 0; i < (int)sc.args.size(); ++i) {
            std::wstring argU = to_upper(sc.args[i]);
            if (depth == 0) {
                if (argU == L"AS") asIdx = i;
                else if (argU == L"DO") doIdx = i;
            }
            if (argU == L"CYCLE") depth++;
            else if (argU == L"ENDCYCLE") {
                if (depth > 0) depth--;
                else if (endCycleIdx == -1) endCycleIdx = i;
            }
        }

        LOG_DEBUG(L"[CYCLE] Markeri gasiti -> AS: " + std::to_wstring(asIdx) + L", DO: " + std::to_wstring(doIdx) + L", END: " + std::to_wstring(endCycleIdx));

        if (asIdx == -1 || doIdx == -1 || endCycleIdx == -1) {
            LOG_ERROR(L"[CYCLE] EROARE: Lipsesc markeri critici (AS/DO/ENDCYCLE)");
            return;
        }

        std::wstring iteratorName = sc.args[asIdx + 1];
        LOG_DEBUG(L"[CYCLE] Iterator: " + iteratorName);

        breakStack.push_back({});
        continueStack.push_back({});
        size_t currentStackLevel = continueStack.size();

        // Sursă
        std::vector<std::wstring> srcTokens(sc.args.begin(), sc.args.begin() + asIdx);
        ASTPtr srcAST = OliExpressionParser(srcTokens).parse();
        if (srcAST) {
            LOG_DEBUG(L"[CYCLE] Generare cod pentru Sursa...");
            generateFromAST(srcAST, chunk, externalProcs);
        }

        chunk.addByte((uint8_t)OpCode::OP_ITER_START, 0);
        size_t loopStart = chunk.code.size();
        LOG_DEBUG(L"[CYCLE] LoopStart la adresa: " + std::to_wstring(loopStart));

        chunk.addByte((uint8_t)OpCode::OP_ITER_NEXT, 0);
        chunk.addByte((uint8_t)OpCode::OP_JUMP_IF_TRUE, 0);
        size_t exitJumpAddr = chunk.code.size();
        chunk.addByte(0, 0); chunk.addByte(0, 0);

        LOG_DEBUG(L"[CYCLE] Generare Store pentru iterator...");
        emitStore(iteratorName, chunk);

        //chunk.addByte((uint8_t)OpCode::OP_POP, 0);

        LOG_DEBUG(L"[CYCLE] Compilare corp bucla (SubBlock)...");
        compileSubBlock(sc.args, doIdx + 1, endCycleIdx, chunk, externalProcs);

        // Patch Continue
        if (continueStack.size() >= currentStackLevel) {
            auto currentContinues = continueStack.back();
            continueStack.pop_back();
            LOG_DEBUG(L"[CYCLE] Patch " + std::to_wstring(currentContinues.size()) + L" CONTINUE.");
            for (size_t cAddr : currentContinues) {
                chunk.code[cAddr - 1] = (uint8_t)OpCode::OP_LOOP;
                uint16_t offset = (uint16_t)(cAddr + 2 - loopStart);
                chunk.code[cAddr] = (uint8_t)(offset >> 8);
                chunk.code[cAddr + 1] = (uint8_t)(offset & 0xFF);
            }
        }

        chunk.addByte((uint8_t)OpCode::OP_LOOP, 0);
        uint16_t loopOffset = (uint16_t)(chunk.code.size() + 2 - loopStart);
        chunk.addByte((uint8_t)(loopOffset >> 8), 0);
        chunk.addByte((uint8_t)(loopOffset & 0xFF), 0);

        size_t postLoopAddr = chunk.code.size();

        // Patch Exit
        uint16_t exitDist = (uint16_t)(postLoopAddr - (exitJumpAddr + 2));
        chunk.code[exitJumpAddr] = (uint8_t)(exitDist >> 8);
        chunk.code[exitJumpAddr + 1] = (uint8_t)(exitDist & 0xFF);

        // Patch Break
        if (breakStack.size() >= currentStackLevel) {
            auto currentBreaks = breakStack.back();
            breakStack.pop_back();
            LOG_DEBUG(L"[CYCLE] Patch " + std::to_wstring(currentBreaks.size()) + L" BREAK.");
            for (size_t bAddr : currentBreaks) {
                uint16_t bDist = (uint16_t)(postLoopAddr - (bAddr + 2));
                chunk.code[bAddr] = (uint8_t)(bDist >> 8);
                chunk.code[bAddr + 1] = (uint8_t)(bDist & 0xFF);
            }
        }

        chunk.addByte((uint8_t)OpCode::OP_ITER_FREE, 0);
        LOG_DEBUG(L"[CYCLE] Compilare finalizata cu succes.");
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

    /*
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
    */
    else if (cmdName == L"FUNC") {
        LOG_DEBUG(L"[DEBUG] Entering FUNC block parsing...");

        // 1. Reconstruim header-ul pentru a analiza semnătura
        // Folosim un spațiu la concatenare pentru a nu lipi token-urile accidental, 
        // apoi curățăm la extragere.
        std::wstring header = L"";
        for (const auto& a : sc.args) header += a + L" ";

        size_t openP = header.find(L'(');
        size_t closeP = header.find(L')');

        if (openP == std::wstring::npos || closeP == std::wstring::npos) {
            LOG_ERROR(L"Runtime Error: Antet FUNC invalid (lipsesc parantezele).");
            return;
        }

        // --- LOGICA DE NAMESPACE (Punct::print) ---
        // Extragem tot ce e înainte de '(', eliminăm spațiile și forțăm UPPERCASE
        std::wstring rawName = trim(header.substr(0, openP));
        std::wstring cleanName = L"";
        for (wchar_t c : rawName) {
            if (!iswspace(c)) cleanName += c;
        }
        //std::wstring funcName = to_upper(rawName);
        std::wstring funcName = to_upper(cleanName);

        LOG_DEBUG(L"[DEBUG] Final Function/Method Name: " + funcName);

        // 2. Extragem Parametrii
        std::wstring paramsPart = header.substr(openP + 1, closeP - openP - 1);

        // 3. Delimităm corpul funcției (ca în codul tău)
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

        // 4. Pregătim ByteCodeProcedure
        ByteCodeProcedure proc;
        proc.name = funcName;
        proc.compiledBody = std::make_shared<OliChunk>();

        // Extragem parametrii nominali
        auto pTokens = splitW(paramsPart, L","); // Split doar după virgulă
        for (auto& p : pTokens) {
            std::wstring cleaned = trim(p);
            if (cleaned == L"...") proc.isVariadic = true;
            else if (!cleaned.empty()) {
                proc.params.push_back(this->cleanVariableName(cleaned));
            }
        }

        // 5. ÎNREGISTRARE ÎN CHUNK
        // funcName va fi de forma "PUNCT::PRINT"
        chunk.procedures[funcName] = proc;

        // 6. COMPILARE CORP
        compileSubBlock(sc.args, bodyStart, bodyEnd, *(proc.compiledBody), chunk.procedures);

        // Return implicit
        if (proc.compiledBody->code.empty() || proc.compiledBody->code.back() != (uint8_t)OpCode::OP_RETURN) {
            proc.compiledBody->addByte((uint8_t)OpCode::OP_RETURN, 0);
        }

        // Salvăm înapoi în map-ul global de proceduri al chunk-ului
        chunk.procedures[funcName] = proc;

        LOG_DEBUG(L"[COMPILER] Registered " + std::wstring( (funcName.find(L"::") != std::wstring::npos ? L"Method: " : L"Function: ") ) + funcName);
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
            if (argU == L"IF" || argU == L"WHILE" || argU == L"FOR" || argU == L"REPEAT" || argU == L"FUNC" || argU == L"PROC" || argU == L"CYCLE" || argU == L"SWITCH" || argU == L"PROC") {
                depth++;
            }
            else if (argU == L"ENDIF" || argU == L"ENDWHILE" || argU == L"ENDFOR" || argU == L"ENDREPEAT" || argU == L"ENDFUNC" || argU == L"ENDPROC" || argU == L"ENDCYCLE" || argU == L"ENDSWITCH" || argU == L"ENDPROC") {
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
            if (argU == L"WHILE" || argU == L"FOR" || argU == L"IF" || argU == L"REPEAT" || argU == L"FUNC" || argU == L"CYCLE" || argU == L"SWITCH" || argU == L"PROC") {
                depth++;
            }
            else if (argU == L"ENDWHILE" || argU == L"ENDFOR" || argU == L"ENDIF" || argU == L"ENDREPEAT" || argU == L"ENDFUNC" || argU == L"ENDCYCLE" || argU == L"ENDSWITCH" || argU == L"ENDPROC") {
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
            if (argU == L"REPEAT" || argU == L"WHILE" || argU == L"FOR" || argU == L"IF" || argU == L"FUNC" || argU == L"PROC" || argU == L"CYCLE" || argU == L"SWITCH" || argU == L"PROC") {
                depth++;
            }
            else if (argU == L"ENDREPEAT" || argU == L"ENDWHILE" || argU == L"ENDFOR" || argU == L"ENDIF" || argU == L"ENDFUNC" || argU == L"ENDPROC" || argU == L"ENDCYCLE" || argU == L"ENDSWITCH" || argU == L"ENDPROC") {
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
                if (argU == L"FOR" || argU == L"IF" || argU == L"WHILE" || argU == L"REPEAT" || argU == L"FUNC" || argU == L"PROC" || argU == L"CYCLE" || argU == L"SWITCH" || argU == L"PROC") {
                    depth++;
                }
                else if (argU == L"ENDFOR" || argU == L"ENDIF" || argU == L"ENDWHILE" || argU == L"ENDREPEAT" || argU == L"ENDFUNC" || argU == L"ENDPROC" || argU == L"ENDCYCLE" || argU == L"ENDSWITCH" || argU == L"ENDPROC") {
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
            // 2. FALLBACK PENTRU EXPRESII LIBERE ($a = 10 sau test())
    // Dacă am ajuns aici, sc.isValid e false, dar avem tokenii în sc.name + sc.args
            /*
    else {
        // FALLBACK pentru expresii libere ($a = 10, test())
        std::vector<std::wstring> tokens;
        tokens.push_back(sc.name);
        for (const auto& arg : sc.args) tokens.push_back(arg);

        OliExpressionParser exprParser(tokens);
        ASTPtr exprAST = exprParser.parse();
        if (exprAST) {
            if (exprAST->type == ASTNodeType::Variable && tokens.size() == 1) return;
            generateFromAST(exprAST, chunk, externalProcs);
            chunk.addByte((uint8_t)OpCode::OP_POP, 0); // Curățăm stiva
        }
    }
    */

            else {
                // 1. Reconstruim numele potențialei proceduri
                std::wstring funcName = to_upper(sc.name);

                // 2. Verificăm dacă numele este o procedură cunoscută (locală sau externă)
                bool isUserProc = (chunk.procedures.count(funcName) > 0 || externalProcs.count(funcName) > 0);

                if (isUserProc) {
                    LOG_DEBUG(L"[VM] Detectat apel direct la procedura: " + funcName);

                    // 3. Punem argumentele pe stivă
                    for (const auto& arg : sc.args) {
                        emitLoadOrConstant(arg, chunk);
                    }

                    // 4. Emitem OP_CALL cu numărul de argumente detectat
                    uint16_t nameIdx = chunk.addConstant(vData(funcName));
                    chunk.addByte((uint8_t)OpCode::OP_CALL, 0);
                    chunk.addByte((uint8_t)(nameIdx >> 8), 0);
                    chunk.addByte((uint8_t)(nameIdx & 0xFF), 0);
                    chunk.addByte((uint8_t)sc.args.size(), 0);
                }
                else {
                    // 5. Fallback pentru expresii libere (ex: $a = 10)
                    std::vector<std::wstring> tokens;
                    tokens.push_back(sc.name);
                    for (const auto& arg : sc.args) tokens.push_back(arg);

                    OliExpressionParser exprParser(tokens);
                    ASTPtr exprAST = exprParser.parse();

                    if (exprAST) {
                        // Verificăm dacă AST-ul este o atribuire
                        bool isAssignment = (exprAST->type == ASTNodeType::Operator &&
                            (exprAST->value == L"=" || exprAST->value == L"set"));

                        generateFromAST(exprAST, chunk, externalProcs);

                        // Dacă NU este atribuire (ex: e doar un apel de funcție sau o adunare), 
                        // atunci curățăm stiva de rezultatul nefolosit
                        if (!isAssignment) {
                            chunk.addByte((uint8_t)OpCode::OP_POP, 0);
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

    // --- 0. PRE-PROCESARE PREFIXE (Mapping @ -> $) ---
    std::wstring normalizedArg = arg;
    if (arg[0] == L'@') {
        normalizedArg = L"$" + arg.substr(1);
        LOG_DEBUG(L"[EMIT_LOAD] Mapping global alias: " + arg + L" -> " + normalizedArg);
    }

    // --- 1. LITERALI (Booleeni, Null) ---
    if (normalizedArg == L"true" || normalizedArg == L"false") {
        emitConstant(vData(normalizedArg == L"true"), chunk, 0);
        return;
    }
    if (normalizedArg == L"NULL" || normalizedArg == L"null" || normalizedArg == L"monostate") {
        emitConstant(vData(std::monostate{}), chunk, 0);
        return;
    }

    // --- 2. LITERAL STRING ---
    if (normalizedArg.size() >= 2 && normalizedArg.front() == L'\"' && normalizedArg.back() == L'\"') {
        std::wstring cleanStr = normalizedArg.substr(1, normalizedArg.size() - 2);
        emitConstant(vData(cleanStr), chunk, 0);
        return;
    }

    // --- 3. LITERAL NUMĂR ---
    if (std::iswdigit(normalizedArg[0]) || (normalizedArg.size() > 1 && normalizedArg[0] == L'-' && std::iswdigit(normalizedArg[1]))) {
        try {
            double val = std::stod(normalizedArg);
            emitConstant(vData(val), chunk, 0);
        } catch (...) {
            LOG_ERROR(L"Eroare conversie număr: " + normalizedArg);
        }
        return;
    }

    // --- 4. DEREFERENȚIERE POINTER (*$ptr) ---
    if (normalizedArg[0] == L'*') {
        emitLoadOrConstant(normalizedArg.substr(1), chunk);
        chunk.addByte((uint8_t)OpCode::OP_GET_INDIRECT, 0);
        return;
    }

    // --- 5. VARIABILE (Cu prefix $ sau indirație $$a) ---
    if (normalizedArg[0] == L'$') {
        size_t dollarCount = 0;
        while (dollarCount < normalizedArg.size() && normalizedArg[dollarCount] == L'$') {
            dollarCount++;
        }

        // Luăm numele de bază (ex: din $$$a luăm $a)
        std::wstring baseName = L"$" + normalizedArg.substr(dollarCount);
        
        // Emitem încărcarea rădăcinii
        uint16_t nameIdx = chunk.addConstant(vData(baseName));
        chunk.addByte((uint8_t)OpCode::OP_GET_GLOBAL, 0);
        chunk.addByte((uint8_t)(nameIdx >> 8), 0);
        chunk.addByte((uint8_t)(nameIdx & 0xFF), 0);

        // Aplicăm indirațiile suplimentare (pentru $$ sau $$$)
        for (size_t i = 1; i < dollarCount; ++i) {
            chunk.addByte((uint8_t)OpCode::OP_GET_INDIRECT, 0);
        }
        return;
    }

    // --- 6. CAZ DEFAULT (Nume fără prefix - tratat ca Global) ---
    uint16_t nameIdx = chunk.addConstant(vData(normalizedArg));
    chunk.addByte((uint8_t)OpCode::OP_GET_GLOBAL, 0);
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

    // 1. ATRIBUIRE
    std::wstring opValue = to_upper(node->value);
    if (node->type == ASTNodeType::Operator && (opValue == L"=" || opValue == L"SET" ||
        opValue == L"+=" || opValue == L"-=" || opValue == L"*=" || opValue == L"/=")) {

        ASTPtr lhs = node->children[0];
        ASTPtr rhs = node->children[1];
        std::wstring lhsOp = to_upper(lhs->value);

        // LOG 1: Să vedem ce crede compilatorul că are în stânga egalului
        LOG_DEBUG(L"[DEBUG_ASSIGN] Op: " + opValue + L" | LHS Val: " + lhs->value + L" | LHS Op: " + lhsOp);

        // A. Dacă e INDEXARE (@a[0] = x)
        if (lhs->type == ASTNodeType::Operator && (lhsOp == L"INDEX" || lhsOp == L"[" || lhsOp == L"DOT")) {
            LOG_DEBUG(L"[DEBUG_ASSIGN] -> Ramura: INDEXARE detectata.");

            generateFromAST(lhs->children[0], chunk, externalProcs);
            if (lhsOp == L"DOT") emitConstant(vData(lhs->children[1]->value), chunk, 0);
            else generateFromAST(lhs->children[1], chunk, externalProcs);

            generateFromAST(rhs, chunk, externalProcs);
            chunk.addByte((uint8_t)OpCode::OP_SET_INDIRECT, 0);
            return;
        }

        // B. Dacă e variabilă simplă ($a = x)
        std::wstring rawLHS = reconstructRawName(lhs);

        // LOG 2: Să vedem ce nume reconstruiește pentru variabila simplă
        LOG_DEBUG(L"[DEBUG_ASSIGN] -> Ramura: NORMALA. rawLHS extras: '" + rawLHS + L"'");

        if (!rawLHS.empty()) {
            if (opValue == L"=") {
                generateFromAST(rhs, chunk, externalProcs);
            }
            else {
                generateFromAST(lhs, chunk, externalProcs);
                generateFromAST(rhs, chunk, externalProcs);
                // ... logică operatori compuși ...
            }

            // LOG 3: Aici e momentul critic unde s-ar putea emite SET_GLOBAL peste Map
            LOG_DEBUG(L"[DEBUG_ASSIGN] -> Apelam emitStore pentru: " + rawLHS);
            emitStore(rawLHS, chunk);
            return;
        }

        LOG_DEBUG(L"[DEBUG_ASSIGN] -> EROARE: Nicio ramura de atribuire nu a fost aleasa!");
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
        ASTPtr funcSource = isDynamic ? node->children[0] : node;

        // A. VERIFICĂM DACĂ ESTE UN APEL DE TIP METODĂ: $p.metoda()
        if (isDynamic && funcSource->value == L"DOT") {
            size_t startIdx = 1; // Argumentele încep de la index 1 în DYNAMIC_CALL
            for (size_t i = startIdx; i < node->children.size(); ++i) {
                generateFromAST(node->children[i], chunk, externalProcs);
            }

            // Punem obiectul ($p) și numele metodei pe stivă
            generateFromAST(funcSource->children[0], chunk, externalProcs);
            chunk.addByte((uint8_t)OpCode::OP_DUP, 0); // Unu pentru contextul 'this', unu pentru lookup
            emitConstant(vData(funcSource->children[1]->value), chunk, 0);
            chunk.addByte((uint8_t)OpCode::OP_GET_INDIRECT, 0);

            uint8_t argCount = (uint8_t)std::min((size_t)255, node->children.size() - startIdx);
            chunk.addByte((uint8_t)OpCode::OP_CALL_METHOD, 0);
            chunk.addByte(argCount, 0);
            return;
        }

        // B. PREGĂTIRE NUME ȘI TYPE()
        std::wstring rawName = isDynamic ? node->children[0]->value : node->value;
        std::wstring funcName = to_upper(trim(rawName));
        LOG_DEBUG(L"[DEBUG_CALL] Functie: " + funcName + L" | Native: " + (vOliKeyWords::isNativeFunction(funcName) ? L"DA" : L"NU"));
        // Suport pentru funcția intrinsecă TYPE()
        if (funcName == L"TYPE") {
            generateFromAST(node->children[isDynamic ? 1 : 0], chunk, externalProcs);
            chunk.addByte((uint8_t)OpCode::OP_TYPE, 0);
            return;
        }

        // C. GENERARE ARGUMENTE PENTRU APEL NORMAL
        size_t startIdx = isDynamic ? 1 : 0;
        for (size_t i = startIdx; i < node->children.size(); ++i) {
            generateFromAST(node->children[i], chunk, externalProcs);
        }

        if (funcName.empty()) funcName = L"__INVALID_CALL__";
        uint8_t argCount = (uint8_t)std::min((size_t)255, node->children.size() - startIdx);
        uint16_t nameIdx = chunk.addConstant(vData(funcName));

        // D. LOGICA DE DECIZIE (SOLUȚIA PENTRU TB_SPLIT)
        // Nu verificăm dacă există în procedures (single-pass issue).
        // Verificăm DOAR dacă este o comandă nativă cunoscută de motor (PRINT, LEN, MAP etc.)
        if (vOliKeyWords::isNativeFunction(funcName)) {
            chunk.addByte((uint8_t)OpCode::OP_CALL_NATIVE, 0);
        }
        else {
            // Dacă nu e în lista de keywords native, presupunem că este o funcție 
            // definită în script (chiar dacă apare mai jos în cod).
            chunk.addByte((uint8_t)OpCode::OP_CALL, 0);
        }

        chunk.addByte((uint8_t)((nameIdx >> 8) & 0xFF), 0);
        chunk.addByte((uint8_t)(nameIdx & 0xFF), 0);
        chunk.addByte(argCount, 0);
        return;
    }


    // --- 5. OPERATORI BINARI ---
    if (node->children.size() == 2) {
        // 5.1. LOGICĂ CU SCURTCIRCUIT (&&, ||)
        // Acestea trebuie tratate primele deoarece nu evaluează ambii copii deodată
        if (node->value == L"&&" || node->value == L"||") {
            generateShortCircuit(node, chunk, externalProcs);
            return; // Ieșire imediată: generateShortCircuit se ocupă de tot flow-ul
        }

        // 5.2. ACCES MEMBRU (DOT) - Tratament special pentru identificator
        if (node->value == L"DOT") {
            // Evaluăm obiectul (ex: $p) -> va lăsa un Map pe stivă
            generateFromAST(node->children[0], chunk, externalProcs);

            // Emitem numele câmpului (ex: "x") direct ca string constant
            // Nu folosim generateFromAST pentru copilul[1] deoarece acolo este un nume, nu o variabilă
            emitConstant(vData(node->children[1]->value), chunk, 0);

            // Extragem valoarea din Map
            chunk.addByte((uint8_t)OpCode::OP_GET_INDIRECT, 0);
            return; // Ieșire imediată
        }

        // 5.3. OPERATORI BINARI STANDARD (+, -, *, /, CONCAT, INDEX, etc.)
        // Evaluăm ambii copii în ordine: Stânga, apoi Dreapta
        generateFromAST(node->children[0], chunk, externalProcs);
        generateFromAST(node->children[1], chunk, externalProcs);

        std::wstring op = node->value;

        // Aritmetică
        if (op == L"+")           chunk.addByte((uint8_t)OpCode::OP_ADD, 0);
        else if (op == L"-")      chunk.addByte((uint8_t)OpCode::OP_SUB, 0);
        else if (op == L"*")      chunk.addByte((uint8_t)OpCode::OP_MUL, 0);
        else if (op == L"/")      chunk.addByte((uint8_t)OpCode::OP_DIV, 0);
        else if (op == L"%")      chunk.addByte((uint8_t)OpCode::OP_MOD, 0);
        else if (op == L"**")     chunk.addByte((uint8_t)OpCode::OP_POW, 0);

        // Comparații
        else if (op == L"==")     chunk.addByte((uint8_t)OpCode::OP_EQUAL, 0);
        else if (op == L"!=")     chunk.addByte((uint8_t)OpCode::OP_NOT_EQUAL, 0);
        else if (op == L">")      chunk.addByte((uint8_t)OpCode::OP_GREATER, 0);
        else if (op == L">=")     chunk.addByte((uint8_t)OpCode::OP_GREATER_EQUAL, 0);
        else if (op == L"<")      chunk.addByte((uint8_t)OpCode::OP_LESS, 0);
        else if (op == L"<=")     chunk.addByte((uint8_t)OpCode::OP_LESS_EQUAL, 0);

        // Bitwise
        else if (op == L"&")      chunk.addByte((uint8_t)OpCode::OP_BAND, 0);
        else if (op == L"|")      chunk.addByte((uint8_t)OpCode::OP_BOR, 0);
        else if (op == L"BXOR" || op == L"^") chunk.addByte((uint8_t)OpCode::OP_BXOR, 0);
        else if (op == L"<<")     chunk.addByte((uint8_t)OpCode::OP_SHL, 0);
        else if (op == L">>")     chunk.addByte((uint8_t)OpCode::OP_SHR, 0);

        // Operatori speciali
        else if (op == L"??")     chunk.addByte((uint8_t)OpCode::OP_NULL_COALESCE, 0);
        else if (op == L"CONCAT") chunk.addByte((uint8_t)OpCode::OP_CONCAT, 0);
        else if (op == L"INDEX")  chunk.addByte((uint8_t)OpCode::OP_GET_INDIRECT, 0);

        return; // Final de procesare pentru operatori binari
    }
}
*/
void OliCompiler::generateFromAST(ASTPtr node, OliChunk& chunk, const std::unordered_map<std::wstring, ByteCodeProcedure>& externalProcs) {
    if (!node) return;

    // 1. ATRIBUIRE
    std::wstring opValue = to_upper(node->value);
    if (node->type == ASTNodeType::Operator && (opValue == L"=" || opValue == L"SET" ||
        opValue == L"+=" || opValue == L"-=" || opValue == L"*=" || opValue == L"/=")) {

        ASTPtr lhs = node->children[0];
        ASTPtr rhs = node->children[1];
        std::wstring lhsOp = to_upper(lhs->value);

        // LOG 1: Să vedem ce crede compilatorul că are în stânga egalului
        LOG_DEBUG(L"[DEBUG_ASSIGN] Op: " + opValue + L" | LHS Val: " + lhs->value + L" | LHS Op: " + lhsOp);

        // A. Dacă e INDEXARE (@a[0] = x)
        if (lhs->type == ASTNodeType::Operator && (lhsOp == L"INDEX" || lhsOp == L"[" || lhsOp == L"DOT")) {
			ASTPtr collectionNode = lhs->children[0];
			std::wstring collectionName = reconstructRawName(collectionNode);
			// TRATARE @: Dacă începe cu @, emitem forțat o încărcare GLOBALĂ
			if (!collectionName.empty() && collectionName[0] == L'@') {
				// Opțional: poți mapa @ în $ intern dacă VM-ul tău caută după $
				// std::wstring finalName = L"$" + collectionName.substr(1);
				emitLoadOrConstant(collectionName, chunk); 
			} else {
				generateFromAST(collectionNode, chunk, externalProcs);
			}
            LOG_DEBUG(L"[DEBUG_ASSIGN] -> Ramura: INDEXARE detectata.");

            
            if (lhsOp == L"DOT") emitConstant(vData(lhs->children[1]->value), chunk, 0);
            else generateFromAST(lhs->children[1], chunk, externalProcs);

            generateFromAST(rhs, chunk, externalProcs);
            chunk.addByte((uint8_t)OpCode::OP_SET_INDIRECT, 0);
            return;
        }

        // B. Dacă e variabilă simplă ($a = x)
        std::wstring rawLHS = reconstructRawName(lhs);

        // LOG 2: Să vedem ce nume reconstruiește pentru variabila simplă
        LOG_DEBUG(L"[DEBUG_ASSIGN] -> Ramura: NORMALA. rawLHS extras: '" + rawLHS + L"'");

        if (!rawLHS.empty()) {
            if (opValue == L"=") {
                generateFromAST(rhs, chunk, externalProcs);
            }
            else {
                generateFromAST(lhs, chunk, externalProcs);
                generateFromAST(rhs, chunk, externalProcs);
                // ... logică operatori compuși ...
            }

            // LOG 3: Aici e momentul critic unde s-ar putea emite SET_GLOBAL peste Map
            LOG_DEBUG(L"[DEBUG_ASSIGN] -> Apelam emitStore pentru: " + rawLHS);
            emitStore(rawLHS, chunk);
            return;
        }

        LOG_DEBUG(L"[DEBUG_ASSIGN] -> EROARE: Nicio ramura de atribuire nu a fost aleasa!");
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
        else if (node->value == L"POSTFIX_INC" || node->value == L"POSTFIX_DEC") {
            ASTPtr target = node->children[0];
            bool isDec = (node->value == L"POSTFIX_DEC");

            if (target->value == L"DEREFERENCE") {
                // 1. Obținem VALOAREA VECHE (pentru echo)
                generateFromAST(target, chunk, externalProcs); // Stiva: [Val_Old]

                // 2. Obținem VALOAREA NOUĂ
                generateFromAST(target, chunk, externalProcs); // Stiva: [Val_Old, Val_Old]
                emitConstant(vData(1.0), chunk, 0);
                chunk.addByte((uint8_t)(isDec ? OpCode::OP_SUB : OpCode::OP_ADD), 0); // Stiva: [Val_Old, Val_New]

                // 3. Obținem ADRESA (re-evaluăm pointerul)
                generateFromAST(target->children[0], chunk, externalProcs); // Stiva: [Val_Old, Val_New, Addr]

                // 4. Scriem folosind un OP_SET_PTR care ia [Addr, Val_New]
                // (Atenție: trebuie să modifici OP_SET_PTR să facă pop la Addr apoi la Val_New)
                chunk.addByte((uint8_t)OpCode::OP_SET_PTR, 0);
            }
            else {
                // 1. Încărcăm valoarea actuală a variabilei pe stivă (ex: $a)
                // Stiva: [Val_Old]
                generateFromAST(target, chunk, externalProcs);

                // 2. Duplicăm valoarea
                // Stiva: [Val_Old, Val_Old]
                chunk.addByte((uint8_t)OpCode::OP_DUP, 0);

                // 3. Calculăm noua valoare folosind a doua copie
                emitConstant(vData(1.0), chunk, 0);
                chunk.addByte((uint8_t)(isDec ? OpCode::OP_SUB : OpCode::OP_ADD), 0);
                // Stiva: [Val_Old, Val_New]

                // 4. Salvăm Val_New în variabilă
                // emitStore va genera OP_SET_GLOBAL sau local logic, consumând Val_New
                emitStore(target->value, chunk);

                // REZULTAT FINAL pe stivă: [Val_Old]
                // Exact ce ne trebuie pentru comportamentul de postfix!
            }
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

    /*
    // --- 4. APELURI DE FUNCȚII (Inclusiv TYPE()) ---
    if (node->type == ASTNodeType::FunctionCall) {
        bool isDynamic = (node->value == L"DYNAMIC_CALL");
        ASTPtr funcSource = isDynamic ? node->children[0] : node;

        // A. VERIFICĂM DACĂ ESTE UN APEL DE TIP METODĂ: $p.metoda()
        if (isDynamic && funcSource->value == L"DOT") {
            // 1. Punem argumentele pe stivă
            size_t startIdx = 1;
            for (size_t i = startIdx; i < node->children.size(); ++i) {
                generateFromAST(node->children[i], chunk, externalProcs);
            }

            // 2. Punem obiectul ($p) pe stivă
            // Acesta va fi extras de VM și folosit ca $this
            generateFromAST(funcSource->children[0], chunk, externalProcs);

            // 3. Punem NUMELE metodei pe stivă (ca string constant, ex: "ataca")
            // IMPORTANT: NU facem OP_GET_INDIRECT aici!
            emitConstant(vData(funcSource->children[1]->value), chunk, 0);

            // 4. Emitem apelul de metodă
            uint8_t argCount = (uint8_t)std::min((size_t)255, node->children.size() - startIdx);
            chunk.addByte((uint8_t)OpCode::OP_CALL_METHOD, 0);
            chunk.addByte(argCount, 0);

            LOG_DEBUG(L"[COMPILER] Method Call: " + funcSource->children[1]->value + L" with " + std::to_wstring(argCount) + L" args.");
            return;
        }

        // B. PREGĂTIRE NUME ȘI TYPE()
        std::wstring rawName = isDynamic ? node->children[0]->value : node->value;
        std::wstring funcName = to_upper(trim(rawName));
        LOG_DEBUG(L"[DEBUG_CALL] Functie: " + funcName + L" | Native: " + (vOliKeyWords::isNativeFunction(funcName) ? L"DA" : L"NU"));
        // Suport pentru funcția intrinsecă TYPE()
        if (funcName == L"TYPE") {
            generateFromAST(node->children[isDynamic ? 1 : 0], chunk, externalProcs);
            chunk.addByte((uint8_t)OpCode::OP_TYPE, 0);
            return;
        }

        // C. GENERARE ARGUMENTE PENTRU APEL NORMAL
        size_t startIdx = isDynamic ? 1 : 0;
        for (size_t i = startIdx; i < node->children.size(); ++i) {
            generateFromAST(node->children[i], chunk, externalProcs);
        }

        if (funcName.empty()) funcName = L"__INVALID_CALL__";
        uint8_t argCount = (uint8_t)std::min((size_t)255, node->children.size() - startIdx);
        uint16_t nameIdx = chunk.addConstant(vData(funcName));

        // D. LOGICA DE DECIZIE (SOLUȚIA PENTRU TB_SPLIT)
        // Nu verificăm dacă există în procedures (single-pass issue).
        // Verificăm DOAR dacă este o comandă nativă cunoscută de motor (PRINT, LEN, MAP etc.)
        if (vOliKeyWords::isNativeFunction(funcName)) {
            chunk.addByte((uint8_t)OpCode::OP_CALL_NATIVE, 0);
        }
        else {
            // Dacă nu e în lista de keywords native, presupunem că este o funcție 
            // definită în script (chiar dacă apare mai jos în cod).
            chunk.addByte((uint8_t)OpCode::OP_CALL, 0);
        }

        chunk.addByte((uint8_t)((nameIdx >> 8) & 0xFF), 0);
        chunk.addByte((uint8_t)(nameIdx & 0xFF), 0);
        chunk.addByte(argCount, 0);
        return;
    }
    */

    // --- 4. APELURI DE FUNCȚII (Inclusiv TYPE() și Metode) ---
    if (node->type == ASTNodeType::FunctionCall) {
        bool isDynamic = (node->value == L"DYNAMIC_CALL");
        ASTPtr funcSource = isDynamic ? node->children[0] : node;

        // A. IDENTIFICARE NUME FUNCȚIE
        std::wstring rawName = isDynamic ? node->children[0]->value : node->value;
        std::wstring funcName = to_upper(trim(rawName));

        // B. COLECTARE ARGUMENTE REALE (FILTRARE)
        // Colectăm argumentele într-un vector temporar pentru a le număra corect
        std::vector<ASTPtr> realArgs;
        size_t startIdx = isDynamic ? 1 : 0;

        for (size_t i = startIdx; i < node->children.size(); ++i) {
            ASTPtr arg = node->children[i];
            // SĂRIM peste paranteze și virgule rătăcite în AST
            if (arg->value == L"(" || arg->value == L")" || arg->value == L",") {
                continue;
            }
            realArgs.push_back(arg);
        }

        // C. CAZ SPECIAL: TYPE()
        if (funcName == L"TYPE") {
            if (!realArgs.empty()) {
                generateFromAST(realArgs[0], chunk, externalProcs);
                chunk.addByte((uint8_t)OpCode::OP_TYPE, 0);
            }
            return;
        }

        // D. CAZ SPECIAL: APEL METODĂ ($obj.metoda())
        if (isDynamic && funcSource->value == L"DOT") {
            for (auto& arg : realArgs) generateFromAST(arg, chunk, externalProcs);
            generateFromAST(funcSource->children[0], chunk, externalProcs); // obiectul ($this)
            emitConstant(vData(funcSource->children[1]->value), chunk, 0);   // numele metodei

            chunk.addByte((uint8_t)OpCode::OP_CALL_METHOD, 0);
            chunk.addByte((uint8_t)realArgs.size(), 0);
            return;
        }

        // E. APEL NORMAL (GLOBAL SAU NATIV)
        uint8_t finalArgCount = 0;
        for (size_t i = 0; i < realArgs.size(); ++i) {
            ASTPtr arg = realArgs[i];

            // --- LOGICA PENTRU REFERINȚĂ (&) ---
            if (arg->value == L"&" || arg->value == L"ADDRESS_OF") {
                if (i + 1 < realArgs.size()) {
                    // Luăm următorul argument (care trebuie să fie variabila)
                    ASTPtr varNode = realArgs[i + 1];
                    uint16_t nameIdx = chunk.addConstant(vData(varNode->value));

                    // Emitem instrucțiunea de ADRESĂ, nu de VALOARE
                    chunk.addByte((uint8_t)OpCode::OP_GET_ADDR, 0);
                    chunk.addByte((nameIdx >> 8) & 0xFF, 0);
                    chunk.addByte(nameIdx & 0xFF, 0);

                    i++; // Sărim peste variabilă, am procesat-o deja aici
                    finalArgCount++;
                    continue;
                }
            }

            // Generare standard pentru argumente normale
            generateFromAST(arg, chunk, externalProcs);
            finalArgCount++;
        }

        // F. EMITERE APEL FINAL
        uint16_t nameIdx = chunk.addConstant(vData(funcName.empty() ? L"__INVALID__" : funcName));
        OpCode callOp = vOliKeyWords::isNativeFunction(funcName) ? OpCode::OP_CALL_NATIVE : OpCode::OP_CALL;

        chunk.addByte((uint8_t)callOp, 0);
        chunk.addByte((uint8_t)((nameIdx >> 8) & 0xFF), 0);
        chunk.addByte((uint8_t)(nameIdx & 0xFF), 0);
        chunk.addByte(finalArgCount, 0); // Transmitem numărul REAL de argumente (1)

        return;
    }

    // --- 5. OPERATORI BINARI ---
    if (node->children.size() == 2) {
        // 5.1. LOGICĂ CU SCURTCIRCUIT (&&, ||)
        // Acestea trebuie tratate primele deoarece nu evaluează ambii copii deodată
        if (node->value == L"&&" || node->value == L"||") {
            generateShortCircuit(node, chunk, externalProcs);
            return; // Ieșire imediată: generateShortCircuit se ocupă de tot flow-ul
        }

        // 5.2. ACCES MEMBRU (DOT) - Tratament special pentru identificator
        if (node->value == L"DOT") {
            // Evaluăm obiectul (ex: $p) -> va lăsa un Map pe stivă
            generateFromAST(node->children[0], chunk, externalProcs);

            // Emitem numele câmpului (ex: "x") direct ca string constant
            // Nu folosim generateFromAST pentru copilul[1] deoarece acolo este un nume, nu o variabilă
            emitConstant(vData(node->children[1]->value), chunk, 0);

            // Extragem valoarea din Map
            chunk.addByte((uint8_t)OpCode::OP_GET_INDIRECT, 0);
            return; // Ieșire imediată
        }

        // 5.3. OPERATORI BINARI STANDARD (+, -, *, /, CONCAT, INDEX, etc.)
        // Evaluăm ambii copii în ordine: Stânga, apoi Dreapta
        generateFromAST(node->children[0], chunk, externalProcs);
        generateFromAST(node->children[1], chunk, externalProcs);

        std::wstring op = node->value;

        // Aritmetică
        if (op == L"+")           chunk.addByte((uint8_t)OpCode::OP_ADD, 0);
        else if (op == L"-")      chunk.addByte((uint8_t)OpCode::OP_SUB, 0);
        else if (op == L"*")      chunk.addByte((uint8_t)OpCode::OP_MUL, 0);
        else if (op == L"/")      chunk.addByte((uint8_t)OpCode::OP_DIV, 0);
        else if (op == L"%")      chunk.addByte((uint8_t)OpCode::OP_MOD, 0);
        else if (op == L"**")     chunk.addByte((uint8_t)OpCode::OP_POW, 0);

        // Comparații
        else if (op == L"==")     chunk.addByte((uint8_t)OpCode::OP_EQUAL, 0);
        else if (op == L"!=")     chunk.addByte((uint8_t)OpCode::OP_NOT_EQUAL, 0);
        else if (op == L">")      chunk.addByte((uint8_t)OpCode::OP_GREATER, 0);
        else if (op == L">=")     chunk.addByte((uint8_t)OpCode::OP_GREATER_EQUAL, 0);
        else if (op == L"<")      chunk.addByte((uint8_t)OpCode::OP_LESS, 0);
        else if (op == L"<=")     chunk.addByte((uint8_t)OpCode::OP_LESS_EQUAL, 0);

        // Bitwise
        else if (op == L"&")      chunk.addByte((uint8_t)OpCode::OP_BAND, 0);
        else if (op == L"|")      chunk.addByte((uint8_t)OpCode::OP_BOR, 0);
        else if (op == L"BXOR" || op == L"^") chunk.addByte((uint8_t)OpCode::OP_BXOR, 0);
        else if (op == L"<<")     chunk.addByte((uint8_t)OpCode::OP_SHL, 0);
        else if (op == L">>")     chunk.addByte((uint8_t)OpCode::OP_SHR, 0);

        // Operatori speciali
        else if (op == L"??")     chunk.addByte((uint8_t)OpCode::OP_NULL_COALESCE, 0);
        else if (op == L"CONCAT") chunk.addByte((uint8_t)OpCode::OP_CONCAT, 0);
        else if (op == L"INDEX")  chunk.addByte((uint8_t)OpCode::OP_GET_INDIRECT, 0);

        return; // Final de procesare pentru operatori binari
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

/*
void OliCompiler::emitStore(const std::wstring& varName, OliChunk& chunk) {
    LOG_DEBUG(L"[DEBUG_EMIT] emitStore chemat pentru: " + varName);
    if (varName.empty()) return;

    // --- 1. DEREFERENȚIERE POINTER (*$ptr = valoare) ---
    // Exemplu: *$b = 10
    if (varName[0] == L'*') {
        if (varName.size() < 2) return;

        std::wstring targetVar = varName.substr(1); // Extragem "$ptr"

        // Stiva la intrare în emitStore conține [VALOARE_NOUA]

        // 1.1. Punem ADRESA pe stivă peste VALOARE
        // Evaluăm variabila $ptr pentru a-i lua valoarea (care este o adresă de memorie)
        uint16_t nameIdx = chunk.addConstant(vData(targetVar));
        chunk.addByte((uint8_t)OpCode::OP_GET_GLOBAL, 0);
        chunk.addByte((uint8_t)(nameIdx >> 8), 0);
        chunk.addByte((uint8_t)(nameIdx & 0xFF), 0);

        // Stiva acum: [VALOARE_NOUA, ADRESA_MEMORIE]

        // 1.2. Folosim instrucțiunea specializată pentru pointeri
        // Aceasta va face: addr = pop(), val = pop(), *addr = val
        chunk.addByte((uint8_t)OpCode::OP_SET_PTR, 0);

        LOG_DEBUG(L"[DEBUG_EMIT] -> Emis OP_SET_PTR pentru dereferentiere: " + targetVar);
        return;
    }

    // --- 2. CALCULARE INDIRAȚIE ($$a, $$$b) ---
    // Această logică rămâne pentru „variable-variable” (nume de variabile dinamice)
    size_t dollarCount = 0;
    while (dollarCount < varName.size() && varName[dollarCount] == L'$') {
        dollarCount++;
    }

    if (dollarCount > 1) {
        LOG_DEBUG(L"[DEBUG_EMIT] -> Emitem SET_INDIRECT pentru indirație multiple.");
        std::wstring baseName = L"$" + varName.substr(dollarCount);

        uint16_t nameIdx = chunk.addConstant(vData(baseName));
        chunk.addByte((uint8_t)OpCode::OP_GET_GLOBAL, 0);
        chunk.addByte((uint8_t)(nameIdx >> 8), 0);
        chunk.addByte((uint8_t)(nameIdx & 0xFF), 0);

        for (size_t i = 0; i < dollarCount - 2; ++i) {
            chunk.addByte((uint8_t)OpCode::OP_GET_INDIRECT, 0);
        }

        // Aici SET_INDIRECT e OK pentru că lucrăm cu tabela de simboluri (care e un Map intern)
        chunk.addByte((uint8_t)OpCode::OP_SET_INDIRECT, 0);
    }
    else {
        // --- 3. ATRIBUIRE DIRECTĂ ($a = valoare) ---
        LOG_DEBUG(L"[DEBUG_EMIT] -> Emitem OP_SET_GLOBAL pentru: " + varName);
        uint16_t nameIdx = chunk.addConstant(vData(varName));
        chunk.addByte((uint8_t)OpCode::OP_SET_GLOBAL, 0);
        chunk.addByte((uint8_t)(nameIdx >> 8), 0);
        chunk.addByte((uint8_t)(nameIdx & 0xFF), 0);
    }
} */

void OliCompiler::emitStore(const std::wstring& varName, OliChunk& chunk) {
    LOG_DEBUG(L"[DEBUG_EMIT] emitStore chemat pentru: " + varName);
    if (varName.empty()) return;
		
		
	
	
	// --- 0. TRATARE SCOPE GLOBAL (@var = valoare) ---
    // Dacă variabila începe cu @, o mapăm intern la variabila globală echivalentă
    std::wstring finalVarName = varName;
    bool forceGlobal = false;

    if (varName[0] == L'@') {
        forceGlobal = true;
        // Transformăm @memo în $memo pentru a partaja aceeași locație de memorie
        finalVarName = L"$" + varName.substr(1);
        LOG_DEBUG(L"[DEBUG_EMIT] -> Detectat prefix @. Mapare: " + varName + L" -> " + finalVarName);
    }
	
    // --- 1. DEREFERENȚIERE POINTER (*$ptr = valoare) ---
    // Exemplu: *$b = 10
    if (varName[0] == L'*') {
        // --- CAZ POINTER: *ptr = valoare ---
        std::wstring targetVar = varName.substr(1);
        uint16_t nameIdx = chunk.addConstant(vData(targetVar));

        // Punem adresa pe stivă (peste valoarea care așteaptă deja acolo)
        chunk.addByte((uint8_t)OpCode::OP_GET_GLOBAL, 0);
        chunk.addByte((uint8_t)(nameIdx >> 8), 0);
        chunk.addByte((uint8_t)(nameIdx & 0xFF), 0);

        // FOLOSIM INSTRUCȚIUNEA CORECTĂ!
        chunk.addByte((uint8_t)OpCode::OP_SET_PTR, 0);
        return;
    }

    // --- 2. CALCULARE INDIRAȚIE ($$a, $$$b) ---
    // Această logică rămâne pentru „variable-variable” (nume de variabile dinamice)
    size_t dollarCount = 0;
    while (dollarCount < varName.size() && varName[dollarCount] == L'$') {
        dollarCount++;
    }

    if (dollarCount > 1) {
        LOG_DEBUG(L"[DEBUG_EMIT] -> Emitem SET_INDIRECT pentru indirație multiple.");
        std::wstring baseName = L"$" + varName.substr(dollarCount);

        uint16_t nameIdx = chunk.addConstant(vData(baseName));
        chunk.addByte((uint8_t)OpCode::OP_GET_GLOBAL, 0);
        chunk.addByte((uint8_t)(nameIdx >> 8), 0);
        chunk.addByte((uint8_t)(nameIdx & 0xFF), 0);

        for (size_t i = 0; i < dollarCount - 2; ++i) {
            chunk.addByte((uint8_t)OpCode::OP_GET_INDIRECT, 0);
        }

        // Aici SET_INDIRECT e OK pentru că lucrăm cu tabela de simboluri (care e un Map intern)
        chunk.addByte((uint8_t)OpCode::OP_SET_INDIRECT, 0);
    }
    else {
        // --- 3. ATRIBUIRE DIRECTĂ ($a = valoare) ---
        LOG_DEBUG(L"[DEBUG_EMIT] -> Emitem OP_SET_GLOBAL pentru: " + varName);
        uint16_t nameIdx = chunk.addConstant(vData(varName));
        chunk.addByte((uint8_t)OpCode::OP_SET_GLOBAL, 0);
        chunk.addByte((uint8_t)(nameIdx >> 8), 0);
        chunk.addByte((uint8_t)(nameIdx & 0xFF), 0);
    }
}

void OliCompiler::generateShortCircuit(ASTPtr node, OliChunk& chunk, const std::unordered_map<std::wstring, ByteCodeProcedure>& externalProcs) {
    bool isAnd = (node->value == L"&&");

    // 1. Evaluăm partea stângă (LHS)
    // Stivă după: [LHS_Result]
    generateFromAST(node->children[0], chunk, externalProcs);

    // 2. DUPLICĂM rezultatul
    // Avem nevoie de o copie pentru verificare și una pentru a rămâne ca rezultat final
    // Stivă după: [LHS_Result, LHS_Result]
    chunk.addByte((uint8_t)OpCode::OP_DUP, 0);

    // 3. Generăm Jump-ul de scurtcircuit
    uint8_t jumpOp = isAnd ? (uint8_t)OpCode::OP_JUMP_IF_FALSE : (uint8_t)OpCode::OP_JUMP_IF_TRUE;
    chunk.addByte(jumpOp, 0);

    size_t jumpAddr = chunk.code.size();
    chunk.addByte(0, 0); // Offset placeholder
    chunk.addByte(0, 0);

    // --- CALEA FĂRĂ SCURTCIRCUIT (Trebuie să evaluăm RHS) ---

    // 4. Dacă am ajuns aici, copia de LHS nu ne mai folosește (e True pt && sau False pt ||)
    // O eliminăm pentru a face loc rezultatului de la RHS
    // Stivă după: []
    chunk.addByte((uint8_t)OpCode::OP_POP, 0);

    // 5. Evaluăm partea dreaptă (RHS)
    // Stivă după: [RHS_Result]
    generateFromAST(node->children[1], chunk, externalProcs);

    // 6. Backpatching
    uint16_t offset = (uint16_t)(chunk.code.size() - (jumpAddr + 2));
    chunk.code[jumpAddr] = (uint8_t)(offset >> 8);
    chunk.code[jumpAddr + 1] = (uint8_t)(offset & 0xFF);

    // --- CALEA CU SCURTCIRCUIT (Jump direct aici) ---
    // Dacă s-a sărit, pe stivă a rămas a doua copie de LHS_Result.
    // Dacă nu s-a sărit, pe stivă este RHS_Result.
    // În ambele cazuri, avem EXACT o valoare pe stivă la final.
}
std::wstring OliCompiler::reconstructRawName(ASTPtr node) {
    if (!node) return L"";

    LOG_DEBUG(L"[DEBUG_RECONSTRUCT] Intrare pentru nod tip: " + std::to_wstring((int)node->type) + L" valoare: " + node->value);

    if (node->type == ASTNodeType::Variable) {
        LOG_DEBUG(L"[DEBUG_RECONSTRUCT] -> Este variabilă, returnăm: " + node->value);
        return node->value;
    }

    if (node->type == ASTNodeType::Operator) {
        std::wstring op = to_upper(node->value);
        if (op == L"INDEX" || op == L"DOT" || op == L"[") {
            LOG_DEBUG(L"[DEBUG_RECONSTRUCT] -> Este OPERATOR DE ACCES. Returnăm GOL (previne emitStore).");
            return L"";
        }
    }

    return L"";
}
/*
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
*/
/*
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
*/

void OliCompiler::loadPluginMetadata(std::wstring pluginName) {
    // 1. Curățare cale (deja implementat de tine)
    if (pluginName.size() >= 2 && pluginName.front() == L'"' && pluginName.back() == L'"') {
        pluginName = pluginName.substr(1, pluginName.size() - 2);
    }
    if (pluginName.empty()) return;

    std::wstring dllPath = pluginName;
    std::wstring ext = PortTools::getPluginExtension();
    if (dllPath.size() < ext.size() || dllPath.substr(dllPath.size() - ext.size()) != ext) {
        dllPath += ext;
    }

    PortTools::LibHandle hLib = PortTools::loadDynamicLibrary(dllPath);
    if (!hLib) return;

    // --- A. ÎNCĂRCARE COMENZI DIN PLUGIN ---
    LoadCommandsFunc regCmds = (LoadCommandsFunc)PortTools::getFunctionSymbol(hLib, "LoadOliCommandPlugin");
    if (regCmds) {
        std::unordered_map<std::wstring, OliCommandHandler> dummyCmds;
        try {
            regCmds(dummyCmds, nullptr); // nullptr pentru că suntem în faza de compilare
            for (auto const& [name, handler] : dummyCmds) {
                if (!name.empty()) vOliKeyWords::registerDynamicCommand(name);
            }
        }
        catch (...) {}
    }

    // --- B. ÎNCĂRCARE FUNCȚII DIN PLUGIN (Asta lipsea!) ---
    // Presupunând că ai un simbol similar pentru funcții, de ex: "LoadOliFunctionPlugin"
    typedef void (*LoadFunctionsFunc)(std::unordered_map<std::wstring, OliFunctionHandler>&);
    LoadFunctionsFunc regFuncs = (LoadFunctionsFunc)PortTools::getFunctionSymbol(hLib, "LoadOliPlugin");

    if (regFuncs) {
        std::unordered_map<std::wstring, OliFunctionHandler> dummyFuncs;
        try {
            regFuncs(dummyFuncs);
            for (auto const& [name, handler] : dummyFuncs) {
                if (!name.empty()) {
                    // Înregistrăm numele ca fiind o funcție nativă (C++)
                    vOliKeyWords::registerNativeFunction(name);
                }
            }
        }
        catch (...) {}
    }
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
        else if (op == (uint8_t)OpCode::OP_ARRAY || op == (uint8_t)OpCode::OP_MAP || op == (uint8_t)OpCode::OP_CALL_METHOD || op == (uint8_t)OpCode::OP_CALL_DYNAMIC) {
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

