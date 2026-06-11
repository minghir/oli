#include "vDataSerialize.hpp"
#include "OliCompiler.hpp"
#include "OliCommandParser.hpp"
#include "OliSyntaxValidator.hpp"
#include "IOliEngine.hpp"
#include "StringUtils.hpp"
#include "PortTools.hpp"
#include <iostream>
#include <filesystem>

#ifndef _WIN32
#include <unistd.h>
#include <limits.h>
#endif

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

    int depth = 0;      // Nivelul de blocuri (IF, WHILE, PROC etc.)
    int bDepth = 0;     // Nivelul de paranteze ( [ { )
    bool inQuotes = false;

    LOG_DEBUG(L"[SPLIT] Incepe procesarea blocului de dimensiune: " + std::to_wstring(input.length()));

    for (size_t i = 0; i < input.length(); ++i) {
        wchar_t c = input[i];

        // =================================================================
        // 1. SCUTUL ANTI-ESCAPE (Rezolvă problema cu căile și ghilimelele)
        // =================================================================
        // Dacă întâlnim un backslash, îl adăugăm în buffer împreună cu următorul 
        // caracter și sărim peste el. Astfel, perechi ca \\ sau \" sunt consumate corect
        // și nu vor păcăli mașina de stări a ghilimelelor de mai jos.
        if (c == L'\\') {
            current += c;
            if (i + 1 < input.length()) {
                current += input[i + 1];
                i++; // Sărim peste caracterul care a fost escapat
            }
            continue;
        }

        // =================================================================
        // 2. GESTIONARE GHILIMELE (State Machine pentru String-uri)
        // =================================================================
        // Acum verificarea este 100% sigură: dacă am ajuns aici și c este '"', 
        // înseamnă sigur că este un delimitator de string, nu o ghilimea escapată!
        if (c == L'"') {
            inQuotes = !inQuotes;
            // LOG_DEBUG(inQuotes ? L"[SPLIT] Intrat in mod STRING" : L"[SPLIT] Iesit din mod STRING");
        }

        // =================================================================
        // 3. LOGICĂ DE SEPARARE (Activă doar în afara ghilimelelor)
        // =================================================================
        if (!inQuotes) {

            // Lambda pentru a verifica dacă la poziția curentă începe un keyword
            auto isKeyword = [&](const std::wstring& targetUpper, size_t pos) {
                if (pos + targetUpper.length() > input.length()) return false;

                // Extragem bucata și o facem Upper pentru comparație case-insensitive
                std::wstring sub = input.substr(pos, targetUpper.length());
                for (auto& sc : sub) sc = std::towupper(sc);

                if (sub != targetUpper) return false;

                // Verificăm marginile (să nu fie parte dintr-un alt cuvânt, ex: "SHIFT" să nu fie "IF")
                bool prevOk = (pos == 0 || iswspace(input[pos - 1]) || wcschr(L";()[]{}\"", input[pos - 1]));
                size_t endPos = pos + targetUpper.length();
                bool nextOk = (endPos >= input.length() || iswspace(input[endPos]) || wcschr(L";()[]{}\"", input[endPos]));

                return prevOk && nextOk;
                };

            // Detectăm structurile care deschid blocuri de cod
            if (isKeyword(L"IF", i) || isKeyword(L"WHILE", i) || isKeyword(L"FOR", i) ||
                isKeyword(L"PROC", i) || isKeyword(L"FUNC", i) || isKeyword(L"REPEAT", i) ||
                isKeyword(L"CYCLE", i) || isKeyword(L"SWITCH", i)) {
                depth++;
                LOG_DEBUG(L"[SPLIT] Gasit Keyword Start. Depth: " + std::to_wstring(depth));
            }
            // Detectăm structurile care închid blocurile de cod
            else if (isKeyword(L"ENDIF", i) || isKeyword(L"ENDWHILE", i) || isKeyword(L"ENDFOR", i) ||
                isKeyword(L"ENDPROC", i) || isKeyword(L"ENDFUNC", i) || isKeyword(L"ENDREPEAT", i) ||
                isKeyword(L"ENDCYCLE", i) || isKeyword(L"ENDSWITCH", i)) {
                if (depth > 0) depth--;
                LOG_DEBUG(L"[SPLIT] Gasit Keyword End. Depth: " + std::to_wstring(depth));
            }

            // Gestionăm adâncimea parantezelor pentru structurile de tip Array/Map/Expresii
            if (c == L'{' || c == L'[') {
                bDepth++;
            }
            if (c == L'}' || c == L']') {
                if (bDepth > 0) bDepth--;
            }

            // =================================================================
            // 4. SEPARARE LA PUNCT ȘI VIRGULĂ (Tăierea instrucțiunilor)
            // =================================================================
            // Tăiem linia doar dacă suntem la nivelul "zero" global (nu în interiorul unui IF/WHILE sau Array)
            if (c == L';' && depth == 0 && bDepth == 0) {
                if (!trim(current).empty()) {
                    LOG_DEBUG(L"[SPLIT] Statement finalizat la ';': " + (current.length() > 20 ? current.substr(0, 20) + L"..." : current));
                    result.push_back(trim(current));
                }
                current.clear();
                continue; // Sărim peste ';' pentru a nu-l introduce în instrucțiunea următoare
            }
        }

        // Adăugăm caracterul curent la instrucțiunea aflată în construcție
        current += c;
    }

    // Adăugăm și ultima instrucțiune din buffer dacă a rămas ceva nefinalizat cu ';'
    if (!trim(current).empty()) {
        LOG_DEBUG(L"[SPLIT] Ultimul statement adaugat: " + (current.length() > 20 ? current.substr(0, 20) + L"..." : current));
        result.push_back(trim(current));
    }

    return result;
}



OliChunk OliCompiler::compile(const std::wstring& source,
    const std::unordered_map<std::wstring, ByteCodeProcedure>& parentProcs,
    bool isSubBlock)
{
/*
    vOliKeyWords::populateNativeFunctions();
    if (!isSubBlock) {
        breakStack.clear();
        continueStack.clear();
        this->locals.clear();
        this->isInFunction = false;
    }
*/
    
    if (!isSubBlock) {
        // =================================================================
        // 🔥 PIPELINE DE VALIDARE SINTACTICĂ CU ALERTE DINAMICE
        // =================================================================
        std::vector<SyntaxError> syntaxErrors;
        OliSyntaxValidator validator;

        if (!validator.validate(source, syntaxErrors)) {
            bool hasCriticalErrors = false;

                // Afișăm organizat mesajele direct prin metodele dedicate ale ConsoleManager
                for (const auto& err : syntaxErrors) {

                        // Asamblăm corpul mesajului (comun pentru toate nivelurile)
                        std::wstring errMsg = L"Linia " + std::to_wstring(err.lineNumber) +
                            L": " + err.message +
                            L" -> Cod afectat: '" + trim(err.rawLine) + L"'";

                        // Rutăm mesajul către macro-ul corect în funcție de severitate
                        switch (err.level) {
                        case DiagnosticLevel::OLI_ERROR:
                            hasCriticalErrors = true; // S-a găsit o eroare blocantă!
                            LOG_ERROR(L"❌ " + errMsg);
                            break;

                        case DiagnosticLevel::OLI_WARNING:
                            LOG_WARNING(L"⚠️ " + errMsg);
                            break;

                        case DiagnosticLevel::OLI_NOTICE:
                            LOG_INFO(L"ℹ️ " + errMsg); // Observațiile de stil le rutăm ca INFO
                            break;
                        }
                    }

                    // Dacă am avut cel puțin o eroare critică, abandonăm compilarea
                    if (hasCriticalErrors) {
                        LOG_FATAL(L"⛔ Compilarea a fost abandonată din cauza erorilor structurale.");
                        OliChunk brokenChunk;
                        brokenChunk.code.clear(); // Returnăm un chunk gol configurat corect pentru barieră
                        return brokenChunk;
                    }
            }
            

            

        // =================================================================
        // RESETARE STIVE GLOBALE (Dacă am trecut de validare sau avem doar Warning-uri)
        // =================================================================
        breakStack.clear();
        continueStack.clear();
        this->locals.clear();
        this->isInFunction = false;
    }
    
    OliChunk chunk;

    // IMPORTANT: NU copiem parentProcs în chunk.procedures pentru a evita referințele circulare.
    // Tabela 'chunk.procedures' va conține DOAR funcțiile definite în ACEST bloc.

    std::wstringstream ss(source);
    std::wstring line;
    std::wstring commandBuffer = L"";
    int nestingLevel = 0;
    int bracketDepth = 0;

    bool isMultilineString = false;

    while (std::getline(ss, line)) {
        //std::wstring cleanLine = trim(line);
        std::wstring cleanLine = isMultilineString ? line : trim(line);
        if (cleanLine.empty() && !isMultilineString) continue;

        // --- 2. LOGICĂ DETECTARE GHILIMELE DESCHISE ---
        // Verificăm dacă linia curentă lasă un string deschis pentru linia următoare
        for (size_t i = 0; i < line.length(); ++i) {
            if (line[i] == L'"' && (i == 0 || line[i - 1] != L'\\')) {
                isMultilineString = !isMultilineString;
            }
        }

        // --- PREPROCESARE: INCLUDE ---
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

                    // Compilăm recursiv fișierul inclus
                    OliChunk includedChunk = this->compile(includedSource, parentProcs, true);

                    // --- PASUL 1: Remapare Constante (Folosim indicii lor REALI din vector) ---
                    std::map<uint16_t, uint16_t> indexMap;
                    for (size_t cIdx = 0; cIdx < includedChunk.constants.size(); ++cIdx) {
                        indexMap[static_cast<uint16_t>(cIdx)] = chunk.addConstant(includedChunk.constants[cIdx]);
                    }

                    // --- PASUL 2: Migrare Bytecode sigură și liniară (Sincronizată cu OpCodes) ---
                    size_t i = 0;
                    size_t limit = includedChunk.code.size();
                    if (!includedChunk.code.empty() && includedChunk.code.back() == (uint8_t)OpCode::OP_RETURN) {
                        limit--; // Ignorăm OP_RETURN-ul global al fișierului inclus
                    }

                    while (i < limit) {
                        uint8_t op = includedChunk.code[i];

                        // 1. Instrucțiuni standard cu indici de constante simpli pe 2 bytes
                        if (op == (uint8_t)OpCode::OP_CONSTANT || op == (uint8_t)OpCode::OP_GET_GLOBAL ||
                            op == (uint8_t)OpCode::OP_SET_GLOBAL || op == (uint8_t)OpCode::OP_UNSET ||
                            op == (uint8_t)OpCode::OP_GET_ADDR || op == (uint8_t)OpCode::OP_PLUGIN ||
                            op == (uint8_t)OpCode::OP_CALL_NATIVE || op == (uint8_t)OpCode::OP_CALL)
                        {
                            // 🛡️ SCUT CRITIC: Verificăm dacă mai avem octeți suficienți înainte de citire
                            if (i + 2 >= includedChunk.code.size()) {
                                LOG_ERROR(L"❌ [COMPILER] Eroare critică: Bytecode corupt la finalul fișierului inclus!");
                                break;
                            }

                            uint16_t oldIdx = (uint16_t)((includedChunk.code[i + 1] << 8) | includedChunk.code[i + 2]);
                            uint16_t newIdx = 0;

                            if (indexMap.count(oldIdx)) {
                                newIdx = indexMap[oldIdx];
                            }
                            else {
                                LOG_ERROR(L"⚠️ [COMPILER] Index de constantă invalid în fișierul inclus: " + std::to_wstring(oldIdx));
                            }

                            chunk.addByte(op, 0);
                            chunk.addByte((uint8_t)(newIdx >> 8), 0);
                            chunk.addByte((uint8_t)(newIdx & 0xFF), 0);

                            i += 3; // Consumăm OpCode și indicele pe 2 bytes

                            // Verificăm dacă instrucțiunea este apel de funcție și copiem argumentul ArgCount (1 byte)
                            if (op == (uint8_t)OpCode::OP_CALL_NATIVE || op == (uint8_t)OpCode::OP_CALL) {
                                if (i < includedChunk.code.size()) {
                                    chunk.addByte(includedChunk.code[i++], 0);
                                }
                            }
                            continue; // 🔥 Sărim la următorul OpCode din buclă
                        }

                        // 🚀 2. MAPAREA INSTRUCTIUNII COMPLEXE DINAMICE: OP_DEF_TYPE
                        else if (op == (uint8_t)OpCode::OP_DEF_TYPE) {
                            // Verificăm header-ul minim fix (Op + NameIdx(2) + ParentIdx(2) + isClass(1) + fieldCount(1) + methodCount(1) = 8 bytes)
                            if (i + 7 >= includedChunk.code.size()) {
                                LOG_ERROR(L"❌ [COMPILER] Eroare critică: Header OP_DEF_TYPE trunchiat în fișierul inclus!");
                                break;
                            }

                            chunk.addByte(op, 0); // Adăugăm codul instrucțiunii

                            // Remapăm NameIdx (2 bytes)
                            uint16_t oldNameIdx = (uint16_t)((includedChunk.code[i + 1] << 8) | includedChunk.code[i + 2]);
                            uint16_t newNameIdx = indexMap.count(oldNameIdx) ? indexMap[oldNameIdx] : 0;
                            chunk.addByte((uint8_t)(newNameIdx >> 8), 0);
                            chunk.addByte((uint8_t)(newNameIdx & 0xFF), 0);

                            // Remapăm ParentIdx (2 bytes)
                            uint16_t oldParentIdx = (uint16_t)((includedChunk.code[i + 3] << 8) | includedChunk.code[i + 4]);
                            uint16_t newParentIdx = indexMap.count(oldParentIdx) ? indexMap[oldParentIdx] : 0;
                            chunk.addByte((uint8_t)(newParentIdx >> 8), 0);
                            chunk.addByte((uint8_t)(newParentIdx & 0xFF), 0);

                            // Preluăm flag-urile și lungimile vectorilor dinamici
                            uint8_t isClass = includedChunk.code[i + 5];
                            uint8_t fieldCount = includedChunk.code[i + 6];
                            uint8_t methodCount = includedChunk.code[i + 7];

                            chunk.addByte(isClass, 0);
                            chunk.addByte(fieldCount, 0);
                            chunk.addByte(methodCount, 0);

                            i += 8; // Am terminat de procesat și emis header-ul fix

                            // Parcurgem și remapăm vectorul de Fields (câmpuri) (2 bytes per field)
                            for (int f = 0; f < (int)fieldCount; ++f) {
                                if (i + 1 >= includedChunk.code.size()) break;
                                uint16_t oldFIdx = (uint16_t)((includedChunk.code[i] << 8) | includedChunk.code[i + 1]);
                                uint16_t newFIdx = indexMap.count(oldFIdx) ? indexMap[oldFIdx] : 0;
                                chunk.addByte((uint8_t)(newFIdx >> 8), 0);
                                chunk.addByte((uint8_t)(newFIdx & 0xFF), 0);
                                i += 2;
                            }

                            // Parcurgem și remapăm vectorul de Methods (metode) (2 bytes per method)
                            for (int m = 0; m < (int)methodCount; ++m) {
                                if (i + 1 >= includedChunk.code.size()) break;
                                uint16_t oldMIdx = (uint16_t)((includedChunk.code[i] << 8) | includedChunk.code[i + 1]);
                                uint16_t newMIdx = indexMap.count(oldMIdx) ? indexMap[oldMIdx] : 0;
                                chunk.addByte((uint8_t)(newMIdx >> 8), 0);
                                chunk.addByte((uint8_t)(newMIdx & 0xFF), 0);
                                i += 2;
                            }

                            continue; // 🔥 Mergem în siguranță la următoarea iterație
                        }

                        // 3. Salturi relative JUMP/LOOP (3 bytes în total)
                        else if (op == (uint8_t)OpCode::OP_JUMP || op == (uint8_t)OpCode::OP_JUMP_IF_FALSE ||
                            op == (uint8_t)OpCode::OP_JUMP_IF_TRUE || op == (uint8_t)OpCode::OP_LOOP) {

                            if (i + 2 >= includedChunk.code.size()) break;

                            chunk.addByte(includedChunk.code[i++], 0); // OpCode
                            chunk.addByte(includedChunk.code[i++], 0); // Offset High
                            chunk.addByte(includedChunk.code[i++], 0); // Offset Low
                            continue;
                        }
                        // 4. Instrucțiuni cu 1 byte argument suplimentar (2 bytes în total)
                        else if (op == (uint8_t)OpCode::OP_ARRAY || op == (uint8_t)OpCode::OP_MAP ||
                            op == (uint8_t)OpCode::OP_CALL_METHOD || op == (uint8_t)OpCode::OP_CALL_DYNAMIC) {

                            if (i + 1 >= includedChunk.code.size()) break;

                            chunk.addByte(includedChunk.code[i++], 0); // OpCode
                            chunk.addByte(includedChunk.code[i++], 0); // Parametru count
                            continue;
                        }
                        // 5. Instrucțiuni simple de 1 singur octet (OP_ADD, OP_POP, OP_DUP etc.)
                        else {
                            chunk.addByte(includedChunk.code[i++], 0);
                        }
                    }

                    // Migrăm definițiile procedurilor în tabela principală
                    for (auto const& [name, proc] : includedChunk.procedures) {
                        chunk.procedures[name] = proc;
                    }
                    continue;
                }
            }
        }

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
        
    // =================================================================
        // 🔥 FIX STRUCTURI MULTI-LINE: Reținem dacă eram deja într-o paranteză/acoladă
        // =================================================================
        bool insideBrackets = (bracketDepth > 0);

        // --- 2. ACTUALIZARE ADÂNCIME (Nesting) ---
        auto nestingTokens = vOliCommandParser::tokenize(maskedLine);
        for (const auto& rawT : nestingTokens) {
            std::wstring t = to_upper(rawT); // IMPORTANT: Lucrăm cu majuscule aici

            if (t == L"IF" || t == L"WHILE" || t == L"FOR" || t == L"PROC" || t == L"FUNC" || t == L"REPEAT" || t == L"CYCLE" || t == L"SWITCH") nestingLevel++;
            if (t == L"ENDIF" || t == L"ENDWHILE" || t == L"ENDFOR" || t == L"ENDPROC" || t == L"ENDFUNC" || t == L"ENDREPEAT" || t == L"ENDCYCLE" || t == L"ENDSWITCH") nestingLevel--;

            if (rawT == L"{" || rawT == L"[") bracketDepth++;
            if (rawT == L"}" || rawT == L"]") bracketDepth--;
        }

        // 🔥 Dacă și după tokenizare trackerul spune că suntem în interior, activăm flag-ul
        if (bracketDepth > 0) insideBrackets = true;

        // --- 3. ACUMULARE BUFFER INTELIGENTĂ ---
        if (commandBuffer.empty()) commandBuffer = cleanLine;
        else {
            if (insideBrackets) {
                // Dacă suntem în interiorul unui [Array] sau {Map} multi-line, separăm doar prin spațiu!
                commandBuffer += L" " + cleanLine;
            }
            else {
                // Pentru blocuri de instrucțiuni (IF, WHILE, etc.), punct-virgula rămâne separatorul corect
                commandBuffer += L" ; " + cleanLine;
            }
        }

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

    if (cmdName == L"CONFIG") {
        return;
    }

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
    
    

    else if (cmdName == L"DEF") {
        if (sc.args.size() < 3) return;

        std::wstring subType = to_lower(sc.args[0]);
        bool isClass = (subType == L"class");
        std::wstring typeName = sc.args[1];
        std::wstring typeNameUpper = to_upper(typeName);

        // --- DETECȚIE MOȘTENIRE (EXTENDS) ---
        std::wstring parentName = L"";
        size_t bodyStartIndex = 2;

        if (sc.args.size() > 4 && to_lower(sc.args[2]) == L"extends") {
            parentName = to_upper(sc.args[3]);
            bodyStartIndex = 4;
        }

        uint16_t nameIdx = chunk.addConstant(vData(typeNameUpper));
        uint16_t parentIdx = chunk.addConstant(vData(parentName));

        std::vector<uint16_t> fieldIndices;
        std::vector<uint16_t> methodIndices;

        // =========================================================================
        // 🔥 FIX CRITIC 1: Normalizăm și spargem token-urile lipite (ex: "{$a}" -> "{", "$a", "}")
        // =========================================================================
        std::vector<std::wstring> cleanTokens;
        for (size_t i = bodyStartIndex; i < sc.args.size(); ++i) {
            std::wstring arg = sc.args[i];
            std::wstring current = L"";

            for (wchar_t ch : arg) {
                if (ch == L'{' || ch == L'}') {
                    if (!current.empty()) { cleanTokens.push_back(current); current = L""; }
                    cleanTokens.push_back(std::wstring(1, ch));
                }
                else if (ch == L',' || ch == L' ') {
                    if (!current.empty()) { cleanTokens.push_back(current); current = L""; }
                }
                else {
                    current += ch;
                }
            }
            if (!current.empty()) cleanTokens.push_back(current);
        }

        // =========================================================================
        // 🚀 PROCESARE TOKEN-URI DETAȘATE
        // =========================================================================
        bool inBraces = false;
        for (size_t i = 0; i < cleanTokens.size(); ++i) {
            std::wstring token = cleanTokens[i];

            if (token == L"{") { inBraces = true; continue; }
            if (token == L"}") { inBraces = false; break; }

            if (!inBraces || token.empty()) continue;

            // --- LOGICA DE DETECȚIE METODĂ ---
            size_t openParen = token.find(L'(');
            if (openParen != std::wstring::npos) {
                std::wstring methodName = to_upper(trim(token.substr(0, openParen)));
                if (!methodName.empty()) {
                    methodIndices.push_back(chunk.addConstant(vData(methodName)));
                }
            }
            else if (i + 1 < cleanTokens.size() && cleanTokens[i + 1][0] == L'(') {
                std::wstring methodName = to_upper(trim(token));
                methodIndices.push_back(chunk.addConstant(vData(methodName)));
                while (i < cleanTokens.size() && cleanTokens[i].find(L')') == std::wstring::npos) {
                    i++;
                }
            }
            else {
                // --- 🔥 FIX CRITIC 2: Curățăm prefixele '$' sau '@' de la proprietăți ---
                std::wstring fieldName = to_lower(token);
                if (!fieldName.empty() && (fieldName[0] == L'$' || fieldName[0] == L'@')) {
                    fieldName.erase(0, 1); // "$a" devine "a"
                }

                fieldIndices.push_back(chunk.addConstant(vData(fieldName)));
            }
        }

        // --- EMITERE BYTECODE ---
        chunk.addByte((uint8_t)OpCode::OP_DEF_TYPE, 0);

        // 1. Indice Nume Clasă Curentă
        chunk.addByte((uint8_t)(nameIdx >> 8), 0);
        chunk.addByte((uint8_t)(nameIdx & 0xFF), 0);

        // 2. Indice Nume Clasă Părinte
        chunk.addByte((uint8_t)(parentIdx >> 8), 0);
        chunk.addByte((uint8_t)(parentIdx & 0xFF), 0);

        // 3. Tip (Class/Struct)
        chunk.addByte(isClass ? 1 : 0, 0);

        // 4. METADATE
        chunk.addByte((uint8_t)fieldIndices.size(), 0);
        chunk.addByte((uint8_t)methodIndices.size(), 0);

        // 5. DATE (Câmpuri)
        for (uint16_t fIdx : fieldIndices) {
            chunk.addByte((uint8_t)(fIdx >> 8), 0);
            chunk.addByte((uint8_t)(fIdx & 0xFF), 0);
        }

        // 6. DATE (Metode)
        for (uint16_t mIdx : methodIndices) {
            chunk.addByte((uint8_t)(mIdx >> 8), 0);
            chunk.addByte((uint8_t)(mIdx & 0xFF), 0);
        }

        LOG_DEBUG(L"[COMPILER] DEF " + typeNameUpper +
            (parentName.empty() ? L"" : L" EXTENDS " + parentName) +
            L" - Câmpuri: " + std::to_wstring(fieldIndices.size()) +
            L", Metode: " + std::to_wstring(methodIndices.size()));
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

   
	
	else if (cmdName == L"FUNC") {
    LOG_DEBUG(L"[COMPILER] --- Început procesare bloc FUNC ---");

    // 1. RECONSTRUCȚIE HEADER
    // Reconstruim semnătura (ex: "Inamic::ataca()") din argumente pentru analiză
    std::wstring header = L"";
    for (const auto& a : sc.args) header += a + L" ";

    size_t openP = header.find(L'(');
    size_t closeP = header.find(L')');

    if (openP == std::wstring::npos || closeP == std::wstring::npos) {
        LOG_ERROR(L"Runtime Error: Antet FUNC invalid (lipsesc parantezele).");
        return;
    }

    // 2. LOGICĂ NAMESPACE ȘI NUME
    // Extragem numele (poate fi "Suma" sau "Inamic::ataca"), curățăm spațiile și forțăm UPPERCASE
    std::wstring rawName = trim(header.substr(0, openP));
    std::wstring cleanName = L"";
    for (wchar_t c : rawName) {
        if (!iswspace(c)) cleanName += c;
    }
    std::wstring funcName = to_upper(cleanName);

    LOG_DEBUG(L"[COMPILER] Nume funcție identificat: " + funcName);

    // 3. DELIMITARE CORP (Nesting)
    // Găsim unde începe codul în lista de argumente (imediat după paranteza de închidere)
    int bodyStart = 0;
    for (int i = 0; i < (int)sc.args.size(); ++i) {
        if (sc.args[i].find(L')') != std::wstring::npos) {
            bodyStart = i + 1;
            break;
        }
    }

    // Căutăm ENDFUNC-ul corespunzător, ținând cont de funcții imbricate
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

    LOG_DEBUG(L"[COMPILER] Corp funcție delimitat la indicii: " + std::to_wstring(bodyStart) + L" -> " + std::to_wstring(bodyEnd));

    // 4. PREGĂTIRE PROCEDURĂ ȘI PARAMETRI
    ByteCodeProcedure proc;
    proc.name = funcName;
    proc.compiledBody = std::make_shared<OliChunk>();

    // Extragem lista de parametri (ex: "$n, $k")
    std::wstring paramsPart = header.substr(openP + 1, closeP - openP - 1);
    auto pTokens = splitW(paramsPart, L",");
    for (auto& p : pTokens) {
        std::wstring cleaned = trim(p);
        if (cleaned == L"...") {
            proc.isVariadic = true;
            LOG_DEBUG(L"[COMPILER] Funcție detectată ca variadică (...)");
        } else if (!cleaned.empty()) {
            // Curățăm numele (ex: "$n") și îl adăugăm în lista de parametri a procedurii
            proc.params.push_back(cleaned); 
        }
    }

    // 5. ACTIVARE CONTEXT LOCALE (Stiva VM)
    // Din acest moment, compilatorul va genera instrucțiuni de stivă (OP_GET_LOCAL)
    this->isInFunction = true;
    this->locals.clear();

    // --- LOGICĂ CRITICĂ: REZERVARE SLOT 0 PENTRU METODE ---
    // Dacă funcția aparține unei clase (are "::" în nume), Slotul 0 pe stivă este REZERVAT pentru $this
    if (funcName.find(L"::") != std::wstring::npos) {
        this->locals.push_back({ L"$this", 0 });
        LOG_DEBUG(L"[COMPILER] Metodă detectată. Slot 0 rezervat automat pentru variabila locală $this");
    }

    // Mapăm restul parametrilor în continuarea stivei
    for (const auto& pName : proc.params) {
        this->locals.push_back({ pName, 0 });
        LOG_DEBUG(L"[COMPILER] Parametru mapat la slot stivă [" + std::to_wstring(this->locals.size() - 1) + L"]: " + pName);
    }

    // 6. COMPILARE RECURSIVĂ CORP
    // Înregistrăm funcția în chunk-ul părinte înainte de compilarea corpului pentru a permite auto-recursivitatea
    chunk.procedures[funcName] = proc;
    
    LOG_DEBUG(L"[COMPILER] Începe compilarea sub-blocului recursiv pentru " + funcName);
    compileSubBlock(sc.args, bodyStart, bodyEnd, *(proc.compiledBody), chunk.procedures);

    // Asigurăm un Return implicit la finalul bytecode-ului
    if (proc.compiledBody->code.empty() || proc.compiledBody->code.back() != (uint8_t)OpCode::OP_RETURN) {
        proc.compiledBody->addByte((uint8_t)OpCode::OP_RETURN, 0);
    }

    // 7. DEZACTIVARE CONTEXT ȘI SALVARE FINALĂ
    // Revenim la contextul Global
    this->isInFunction = false;
    this->locals.clear();

    chunk.procedures[funcName] = proc;
    LOG_DEBUG(L"[COMPILER] --- Finalizare înregistrare FUNC: " + funcName + L" ---");
}
	
	else if (cmdName == L"PROC") {
    if (sc.args.empty()) return;

    LOG_DEBUG(L"[COMPILER] --- Început procesare bloc PROC ---");

    // 1. Identificăm Numele procedurii (primul argument după cuvântul PROC)
    std::wstring procName = to_upper(sc.args[0]);
    // Curățăm caracterele de tip punctuație dacă au fost puse accidental (ex: "PROC Pascal:")
    procName.erase(std::remove_if(procName.begin(), procName.end(), [](wchar_t c) {
        return c == L'(' || c == L')' || c == L':';
    }), procName.end());

    LOG_DEBUG(L"[COMPILER] Nume procedură: " + procName);

    // 2. Delimităm corpul căutând ENDPROC (gestionează corect procedurile imbricate)
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

    // 3. Pregătim obiectul ByteCodeProcedure
    ByteCodeProcedure proc;
    proc.name = procName;
    proc.compiledBody = std::make_shared<OliChunk>();

    // 4. Extragem Parametrii (pentru PROC aceștia vin direct după nume, fără paranteze)
    int bodyStart = 1;
    for (int i = 1; i < bodyEnd; ++i) {
        std::wstring arg = sc.args[i];
        // Identificăm dacă argumentul este un nume de variabilă ($n)
        if (arg[0] == L'$' || arg[0] == L'@' || arg.find(L',') != std::wstring::npos) {
            std::wstring cleaned = trim(arg);
            // Eliminăm virgulele reziduale din lista de argumente
            cleaned.erase(std::remove(cleaned.begin(), cleaned.end(), L','), cleaned.end());
            
            if (!cleaned.empty()) {
                proc.params.push_back(cleaned);
            }
            bodyStart = i + 1; // Avansăm startul corpului după ultimul parametru găsit
        } else {
            // Dacă am dat de o instrucțiune, aici se oprește lista de parametri
            bodyStart = i;
            break;
        }
    }

    // --- ACTIVARE CONTEXT LOCALE ---
    this->isInFunction = true;
    this->locals.clear();

    // Mapăm parametrii în vectorul de locale pentru acces pe stivă (OP_GET_LOCAL)
    for (const auto& pName : proc.params) {
        this->locals.push_back({ pName, 0 });
        LOG_DEBUG(L"[COMPILER] Parametru PROC mapat LOCAL: " + pName);
    }

    // 5. Compilare Corp
    chunk.procedures[procName] = proc;
    
    LOG_DEBUG(L"[COMPILER] Compilare sub-bloc pentru PROC: " + procName);
    compileSubBlock(sc.args, bodyStart, bodyEnd, *(proc.compiledBody), chunk.procedures);

    // Return implicit pentru siguranță
    if (proc.compiledBody->code.empty() || proc.compiledBody->code.back() != (uint8_t)OpCode::OP_RETURN) {
        proc.compiledBody->addByte((uint8_t)OpCode::OP_RETURN, 0);
    }

    // --- DEZACTIVARE CONTEXT ---
    this->isInFunction = false;
    this->locals.clear();

    chunk.procedures[procName] = proc;
    LOG_DEBUG(L"[COMPILER] --- Finalizare înregistrare PROC: " + procName + L" ---");
}
	
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

            size_t currentStackLevel = continueStack.size();

            std::wstring varName = sc.args[0];
            std::wstring startVal = sc.args[2];

            // 2. Inițializare ($i = startVal)
            emitLoadOrConstant(startVal, chunk);
            emitStore(varName, chunk);

            size_t loopStart = chunk.code.size();

            // 3. Condiție de ieșire (evaluată la runtime):
            //    dacă ((i - limit) * step) > 0 -> ieșim.
            // Emităm: load i, load limit, OP_SUB, load step (sau 1), OP_MUL, OP_CONSTANT 0, OP_GREATER, OP_JUMP_IF_TRUE
            emitLoadOrConstant(varName, chunk);
            int limitEnd = (byIdx != -1) ? byIdx : doIdx;
            std::vector<std::wstring> limTokens(sc.args.begin() + toIdx + 1, sc.args.begin() + limitEnd);
            ASTPtr limAST = OliExpressionParser(limTokens).parse();
            if (limAST) generateFromAST(limAST, chunk, externalProcs);

            // a = i - limit
            chunk.addByte((uint8_t)OpCode::OP_SUB, 0);

            // push step (dacă există) sau 1
            if (byIdx != -1) {
                std::vector<std::wstring> stepTokens(sc.args.begin() + byIdx + 1, sc.args.begin() + doIdx);
                
                // Avertisment dacă step e literal 0
                std::wstring stepExpr;
                for (const auto& t : stepTokens) stepExpr += trim(t);
                stepExpr = trim(stepExpr);
                try {
                    double v = std::stod(stepExpr);
                    if (v == 0.0) {
                        LOG_WARNING(L"FOR loop cu `by 0` va crea o buclă infinită.");
                    }
                } catch (...) { /* nu putem evalua literal la compilare */ }
                
                ASTPtr stepAST = OliExpressionParser(stepTokens).parse();
                if (stepAST) generateFromAST(stepAST, chunk, externalProcs);
                else emitConstant(vData(1.0), chunk, 0);
            }
            else {
                emitConstant(vData(1.0), chunk, 0);
            }

            // (i - limit) * step
            chunk.addByte((uint8_t)OpCode::OP_MUL, 0);

            // compare with 0: if > 0 then exit
            emitConstant(vData(0.0), chunk, 0);
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
           
			else {
				// 1. Încercăm mai întâi să vedem dacă este un apel de PROCEDURĂ (stil comandă: nume arg1 arg2)
				// Procedurile au prioritate față de expresiile libere pentru a evita interpretarea eronată.
				std::wstring procLookupName = to_upper(sc.name);
				bool isKnownProc = (chunk.procedures.count(procLookupName) > 0 || externalProcs.count(procLookupName) > 0);

				if (  isKnownProc && (sc.args.empty() || sc.args[0] != L"(") ) {
					LOG_DEBUG(L"[COMPILER] Apel procedură stil comandă detectat: " + procLookupName);

					// A. Încărcăm argumentele pe stivă (fiecare argument este tratat ca o mini-expresie)
					for (const auto& arg : sc.args) {
						OliExpressionParser argParser({arg});
						ASTPtr argAST = argParser.parse();
						if (argAST) {
							generateFromAST(argAST, chunk, externalProcs);
						}
					}

					// B. Punem numele procedurii în tabela de constante și emitem OP_CALL
					uint16_t nameIdx = chunk.addConstant(vData(procLookupName));
					chunk.addByte((uint8_t)OpCode::OP_CALL, 0);
					chunk.addByte((uint8_t)((nameIdx >> 8) & 0xFF), 0);
					chunk.addByte((uint8_t)(nameIdx & 0xFF), 0);
					chunk.addByte((uint8_t)sc.args.size(), 0); // Numărul de argumente pasate

					// C. Curățăm stiva: procedurile tip comandă nu sunt folosite în expresii, 
					// deci orice return (chiar și NIL) trebuie eliminat pentru a păstra stiva curată.
					chunk.addByte((uint8_t)OpCode::OP_POP, 0);
					LOG_DEBUG(L"[COMPILER] Bytecode generat pentru procedură: " + procLookupName + L" (POP inclus)");
				} 
				else {
					// 2. Dacă nu este o procedură cunoscută, tratăm linia ca pe o EXPRESIE LIBERĂ (ex: $a = 10 sau func(args))
					LOG_DEBUG(L"[COMPILER] Pasare către Parserul de Expresii pentru: " + sc.name);

					std::vector<std::wstring> tokens;
					tokens.push_back(sc.name);
					for (const auto& arg : sc.args) tokens.push_back(arg);

					OliExpressionParser exprParser(tokens);
					ASTPtr exprAST = exprParser.parse();

					if (exprAST) {
						// Verificăm dacă expresia lasă o valoare pe stivă care trebuie curățată (managementul stivei)
						bool isAssignment = (exprAST->type == ASTNodeType::Operator &&
							(exprAST->value == L"=" || exprAST->value == L"+=" || exprAST->value == L"-="));

						bool isCall = (exprAST->type == ASTNodeType::FunctionCall || exprAST->value == L"DYNAMIC_CALL");

						// Generăm bytecode-ul din arborele sintactic (AST)
						generateFromAST(exprAST, chunk, externalProcs);

						// Dacă este un apel de funcție de sine stătător, curățăm rezultatul returnat.
						if (isCall && !isAssignment) {
							chunk.addByte((uint8_t)OpCode::OP_POP, 0);
							LOG_DEBUG(L"[COMPILER] Management stivă: Adăugat OP_POP după apelul de funcție: " + sc.name);
						}
					}
					else {
						LOG_ERROR(L"[COMPILER] Eroare critică de sintaxă: Linia nu a putut fi interpretată: " + sc.name);
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

    // --- 0. PRE-PROCESARE PREFIXE (Mapping @ -> $ Global) ---
    // Variabilele care încep cu @ sunt tratate ca GLOBALE explicite, sărind peste căutarea în stivă.
    std::wstring normalizedArg = arg;
    bool isExplicitGlobal = (arg[0] == L'@');
    if (isExplicitGlobal) {
        normalizedArg = L"$" + arg.substr(1);
        LOG_DEBUG(L"[EMIT_LOAD] Mapping global alias detectat: " + arg + L" -> " + normalizedArg);
    }

    // --- 1. LITERALI (Booleeni, Null) ---
    if (normalizedArg == L"true" || normalizedArg == L"false") {
        bool val = (normalizedArg == L"true");
        LOG_DEBUG(L"[EMIT_LOAD] Constantă booleană: " + normalizedArg);
        emitConstant(vData(val), chunk, 0);
        return;
    }
    if (normalizedArg == L"NULL" || normalizedArg == L"null" || normalizedArg == L"monostate") {
        LOG_DEBUG(L"[EMIT_LOAD] Constantă NULL/Monostate");
        emitConstant(vData(std::monostate{}), chunk, 0);
        return;
    }

    // --- 2. LITERAL STRING ---
    if (normalizedArg.size() >= 2 && normalizedArg.front() == L'\"' && normalizedArg.back() == L'\"') {
        std::wstring cleanStr = normalizedArg.substr(1, normalizedArg.size() - 2);
        LOG_DEBUG(L"[EMIT_LOAD] Constantă string: " + cleanStr);
        emitConstant(vData(cleanStr), chunk, 0);
        return;
    }

    // --- 3. LITERAL NUMĂR ---
    /*
    if (std::iswdigit(normalizedArg[0]) || (normalizedArg.size() > 1 && normalizedArg[0] == L'-' && std::iswdigit(normalizedArg[1]))) {
        try {
            double val = std::stod(normalizedArg);
            LOG_DEBUG(L"[EMIT_LOAD] Constantă numerică: " + std::to_wstring(val));
            emitConstant(vData(val), chunk, 0);
        } catch (...) {
            LOG_ERROR(L"[EMIT_LOAD] Eroare critică la conversia numărului: " + normalizedArg);
        }
        return;
    }
    */
    // --- 3. LITERAL NUMĂR ---
    if (std::iswdigit(normalizedArg[0]) || (normalizedArg.size() > 1 && normalizedArg[0] == L'-' && std::iswdigit(normalizedArg[1]))) {
        try {
            // Verificăm dacă string-ul conține un punct zecimal
            if (normalizedArg.find(L'.') != std::wstring::npos) {
                // Are punct -> Este un număr cu virgulă (FLOAT / DOUBLE)
                double val = std::stod(normalizedArg);
                LOG_DEBUG(L"[EMIT_LOAD] Constantă numerică (FLOAT): " + std::to_wstring(val));
                emitConstant(vData(val), chunk, 0);
            }
            else {
                // NU are punct -> Este un număr întreg pur (INT / int64_t)
                long long val = std::stoll(normalizedArg, nullptr, 0);
                LOG_DEBUG(L"[EMIT_LOAD] Constantă numerică (INT): " + std::to_wstring(val));
                emitConstant(vData(val), chunk, 0);
            }
        }
        catch (...) {
            LOG_ERROR(L"[EMIT_LOAD] Eroare critică la conversia numărului: " + normalizedArg);
        }
        return;
    }
    // --- 4. DEREFERENȚIERE POINTER (*$ptr) ---
    if (normalizedArg[0] == L'*') {
        LOG_DEBUG(L"[EMIT_LOAD] Dereferențiere detectată (*).");
        emitLoadOrConstant(normalizedArg.substr(1), chunk);
        chunk.addByte((uint8_t)OpCode::OP_GET_INDIRECT, 0);
        return;
    }

    // --- 5. VARIABILE (Locals vs Globals) ---
    if (normalizedArg[0] == L'$') {
        // Calculăm nivelul de indirație (ex: $a = 1, $$a = 2)
        size_t dollarCount = 0;
        while (dollarCount < normalizedArg.size() && normalizedArg[dollarCount] == L'$') {
            dollarCount++;
        }

        // Numele de bază pentru căutare (ex: din $$$a extragem $a)
        std::wstring baseName = L"$" + normalizedArg.substr(dollarCount);

        // --- VERIFICARE VARIABILE LOCALE ---
        // Căutăm în stiva de locale doar dacă suntem în interiorul unei funcții 
        // și nu am forțat accesul global prin prefixul '@'.
        if (this->isInFunction && !isExplicitGlobal) {
            int stackIndex = -1;
            for (int i = 0; i < (int)this->locals.size(); ++i) {
                if (this->locals[i].name == baseName) {
                    stackIndex = i;
                    break;
                }
            }

            if (stackIndex != -1) {
                LOG_DEBUG(L"[EMIT_LOAD] Identificat LOCAL: " + baseName + L" la slotul de stivă: " + std::to_wstring(stackIndex));
                
                // Emitem instrucțiunea pentru citire de pe stivă (Fast Access)
                chunk.addByte((uint8_t)OpCode::OP_GET_LOCAL, 0);
                chunk.addByte((uint8_t)stackIndex, 0);

                // Aplicăm indirațiile dacă există ($$ sau $$$)
                for (size_t i = 1; i < dollarCount; ++i) {
                    chunk.addByte((uint8_t)OpCode::OP_GET_INDIRECT, 0);
                }
                return;
            }
        }

        // --- FALLBACK LA GLOBALE ---
        // Dacă nu e locală sau suntem în Main, căutăm în tabelul global de simboluri.
        LOG_DEBUG(L"[EMIT_LOAD] Identificat GLOBAL: " + baseName);
        // 🔥 FIX: Curățăm prefixul $ pentru compatibilitate cu OP_GET_INDIRECT
        uint16_t nameIdx = chunk.addConstant(vData(cleanVariableName(baseName)));
        chunk.addByte((uint8_t)OpCode::OP_GET_GLOBAL, 0);
        chunk.addByte((uint8_t)(nameIdx >> 8), 0);
        chunk.addByte((uint8_t)(nameIdx & 0xFF), 0);

        // Aplicăm indirațiile
        for (size_t i = 1; i < dollarCount; ++i) {
            chunk.addByte((uint8_t)OpCode::OP_GET_INDIRECT, 0);
        }
        return;
    }

    // --- 6. CAZ DEFAULT (Tratat ca Global) ---
    LOG_DEBUG(L"[EMIT_LOAD] Fallback global pentru identificator: " + normalizedArg);
    // 🔥 FIX: Curățăm și aici identificatorul default
    uint16_t nameIdx = chunk.addConstant(vData(cleanVariableName(normalizedArg)));
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


void OliCompiler::generateFromAST(ASTPtr node, OliChunk& chunk, const std::unordered_map<std::wstring, ByteCodeProcedure>& externalProcs) {
    if (!node) return;
   
    // --- 1. ATRIBUIRE (Assignment & Compound Assignment) ---
    std::wstring opValue = to_upper(node->value);
    if (node->type == ASTNodeType::Operator && (opValue == L"=" || opValue == L"SET" ||
        opValue == L"+=" || opValue == L"-=" || opValue == L"*=" || opValue == L"/=")) {

        ASTPtr lhs = node->children[0];
        ASTPtr rhs = node->children[1];
        std::wstring lhsOp = to_upper(lhs->value);

        LOG_DEBUG(L"[DEBUG_ASSIGN] Procesare Op: " + opValue + L" pe LHS: " + lhs->value);

        // =========================================================================
        // A. RAMURA INDEXARE: Obiecte, Array-uri sau Dereferențieri ($obj.prop, $arr[0])
        // =========================================================================
        if (lhs->type == ASTNodeType::Operator && (lhsOp == L"INDEX" || lhsOp == L"[" || lhsOp == L"DOT")) {
            LOG_DEBUG(L"[DEBUG_ASSIGN] -> Detectata ramura INDEXARE (Map/Array/Object)");

            ASTPtr collectionNode = lhs->children[0];

            // 1. Pregătim terenul: Punem CONTAINERUL și CHEIA pe stivă
            // Acestea trebuie să fie primele două elemente pentru OP_SET_INDIRECT final.
            std::wstring collectionName = reconstructRawName(collectionNode);
            if (!collectionName.empty() && collectionName[0] == L'@') {
                emitLoadOrConstant(collectionName, chunk);
            }
            else {
                generateFromAST(collectionNode, chunk, externalProcs);
            }

            // Punem cheia (numele proprietății sau indexul)
            if (lhsOp == L"DOT") emitConstant(vData(lhs->children[1]->value), chunk, 0);
            else generateFromAST(lhs->children[1], chunk, externalProcs);

            // Stiva acum: [Container, Key]

            // 2. Gestionăm OPERATORII COMPUȘI (+=, -=, etc.)
            if (opValue != L"=" && opValue != L"SET") {
                LOG_DEBUG(L"[DEBUG_ASSIGN] -> Calcul compus detectat pentru membru. Extragem valoarea veche.");

                // Pentru a face +=, trebuie să știm ce era înainte acolo.
                // Re-evaluăm containerul și cheia pentru a apela un GET temporar.
                // (Alternativ, dacă ai instrucțiune DUP2 în VM, ar fi mai eficient)
                generateFromAST(collectionNode, chunk, externalProcs);
                if (lhsOp == L"DOT") emitConstant(vData(lhs->children[1]->value), chunk, 0);
                else generateFromAST(lhs->children[1], chunk, externalProcs);

                chunk.addByte((uint8_t)OpCode::OP_GET_INDIRECT, 0); // Stiva: [Container, Key, OldValue]

                // Evaluăm partea dreaptă a egalului (RHS)
                generateFromAST(rhs, chunk, externalProcs); // Stiva: [Container, Key, OldValue, RHSValue]

                // Aplicăm operația matematică
                if (opValue == L"+=")      chunk.addByte((uint8_t)OpCode::OP_ADD, 0);
                else if (opValue == L"-=") chunk.addByte((uint8_t)OpCode::OP_SUB, 0);
                else if (opValue == L"*=") chunk.addByte((uint8_t)OpCode::OP_MUL, 0);
                else if (opValue == L"/=") chunk.addByte((uint8_t)OpCode::OP_DIV, 0);

                // Stiva finală înainte de salvare: [Container, Key, CalculatedNewValue]
            }
            else {
                // Atribuire simplă: Doar evaluăm RHS
                generateFromAST(rhs, chunk, externalProcs); // Stiva: [Container, Key, RHSValue]
            }

            // 3. Executăm salvarea în memorie
            chunk.addByte((uint8_t)OpCode::OP_SET_INDIRECT, 0);
            LOG_DEBUG(L"[DEBUG_ASSIGN] -> Emis OP_SET_INDIRECT pentru structura.");
            return;
        }

        // =========================================================================
        // 🔥 B0. NOUA RAMURĂ C: DEREFERENȚIERE POINTER (*$p_a = 50 sau *$p_a += 10)
        // =========================================================================
        if (lhs->type == ASTNodeType::Operator && (lhsOp == L"DEREFERENCE" || lhsOp == L"*")) {
            LOG_DEBUG(L"[DEBUG_ASSIGN] -> Detectata ramura DEREFERENTIERE POINTER (*ptr = ...)");

            if (opValue == L"=" || opValue == L"SET") {
                // Cazul Simplu: *$p_a = 50
                // 1. Calculăm valoarea nouă din dreapta (RHS) -> ajunge prima pe stivă
                generateFromAST(rhs, chunk, externalProcs);

                // 2. Evaluăm expresia pointerului (LHS child, ex: $p_a) -> numele/adresa ajunge pe stivă
                generateFromAST(lhs->children[0], chunk, externalProcs);
            }
            else {
                // Cazul Compus: *$p_a += 10
                // 1. Încărcăm valoarea veche din pointer (evaluăm tot nodul LHS pentru OP_GET_INDIRECT)
                generateFromAST(lhs, chunk, externalProcs);

                // 2. Evaluăm valoarea adunată (RHS)
                generateFromAST(rhs, chunk, externalProcs);

                // 3. Aplicăm operația matematică compusă
                if (opValue == L"+=")      chunk.addByte((uint8_t)OpCode::OP_ADD, 0);
                else if (opValue == L"-=") chunk.addByte((uint8_t)OpCode::OP_SUB, 0);
                else if (opValue == L"*=") chunk.addByte((uint8_t)OpCode::OP_MUL, 0);
                else if (opValue == L"/=") chunk.addByte((uint8_t)OpCode::OP_DIV, 0);

                // Stiva are acum: [CalculatedNewValue]
                // 4. Evaluăm din nou expresia pointerului pentru a pune adresa de destinație pe stivă
                generateFromAST(lhs->children[0], chunk, externalProcs);
            }

            // Stiva respectă ordinea LIFO cerută de VM: [Valoare_Noua, Adresa/Nume] <- top
            chunk.addByte((uint8_t)OpCode::OP_SET_PTR, 0);
            LOG_DEBUG(L"[DEBUG_ASSIGN] -> Emis OP_SET_PTR pentru scriere directa prin pointer.");
            return;
        }


        // =========================================================================
        // B. RAMURA NORMALĂ: Variabile simple ($boss = ...)
        // =========================================================================
        std::wstring rawLHS = reconstructRawName(lhs);
        if (!rawLHS.empty()) {
            LOG_DEBUG(L"[DEBUG_ASSIGN] -> Detectata ramura NORMALA (Variabila: " + rawLHS + L")");

            if (opValue == L"=" || opValue == L"SET") {
                // Cazul simplu: calculăm RHS și stocăm
                generateFromAST(rhs, chunk, externalProcs);
            }
            else {
                LOG_DEBUG(L"[DEBUG_ASSIGN] -> Calcul compus pe variabila simpla.");
                // Cazul compus: Luăm valoarea actuală, calculăm RHS, facem operația
                generateFromAST(lhs, chunk, externalProcs); // Load Old
                generateFromAST(rhs, chunk, externalProcs); // Load RHS

                if (opValue == L"+=")      chunk.addByte((uint8_t)OpCode::OP_ADD, 0);
                else if (opValue == L"-=") chunk.addByte((uint8_t)OpCode::OP_SUB, 0);
                else if (opValue == L"*=") chunk.addByte((uint8_t)OpCode::OP_MUL, 0);
                else if (opValue == L"/=") chunk.addByte((uint8_t)OpCode::OP_DIV, 0);
            }

            // Salvăm rezultatul final în variabilă
            LOG_DEBUG(L"[DEBUG_ASSIGN] -> Apelam emitStore pentru: " + rawLHS);
            emitStore(rawLHS, chunk);
            return;
        }

        LOG_DEBUG(L"[DEBUG_ASSIGN] -> EROARE: LHS-ul nu este o destinatie valida pentru scriere!");
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

    // --- 4. APELURI DE FUNCȚII (Inclusiv TYPE() și Metode) ---
// Verificăm dacă nodul curent este un apel de funcție sau un apel dinamic
    if (node->type == ASTNodeType::FunctionCall || node->value == L"DYNAMIC_CALL") {
        bool isDynamic = (node->value == L"DYNAMIC_CALL");
        ASTPtr funcSource = isDynamic ? node->children[0] : node;

        // A. IDENTIFICARE NUME FUNCȚIE
        // Dacă este dinamic (ex: $obj.metoda()), numele vine din primul copil
        std::wstring rawName = isDynamic ? node->children[0]->value : node->value;
        std::wstring funcName = to_upper(trim(rawName));

        LOG_DEBUG(L"[COMPILER] Pregatire apel pentru: " + funcName + (isDynamic ? L" (DINAMIC)" : L" (STATIC)"));

        // B. COLECTARE ARGUMENTE REALE (FILTRARE ANTI-FANTOME)
        // Eliminăm token-urile de control (paranteze, virgule) care pot rămâne în AST din faza de parsing
        std::vector<ASTPtr> realArgs;
        size_t startIdx = isDynamic ? 1 : 0;

        for (size_t i = startIdx; i < node->children.size(); ++i) {
            ASTPtr arg = node->children[i];
            if (arg->value == L"(" || arg->value == L")" || arg->value == L",") {
                LOG_DEBUG(L"  -> Skip token control: " + arg->value);
                continue;
            }
            realArgs.push_back(arg);
        }

        // C. CAZ SPECIAL: TYPE()
        // Intrinsecul TYPE() returnează tipul unei variabile/obiect la runtime
        if (funcName == L"TYPE") {
            if (!realArgs.empty()) {
                LOG_DEBUG(L"  -> Procesare intrinsec TYPE()");
                generateFromAST(realArgs[0], chunk, externalProcs);
                chunk.addByte((uint8_t)OpCode::OP_TYPE, 0);
            }
            return;
        }

        // D. CAZ SPECIAL: APEL METODĂ ($obj.metoda())
        // Dacă sursa funcției este un operator DOT, înseamnă că avem un apel de metodă pe obiect
        if (isDynamic && funcSource->value == L"DOT") {
            LOG_DEBUG(L"  -> Detectat apel de metoda: " + funcSource->children[1]->value);

            // 1. Punem argumentele pe stivă
            for (auto& arg : realArgs) generateFromAST(arg, chunk, externalProcs);

            // 2. Punem obiectul ($this) pe stivă
            generateFromAST(funcSource->children[0], chunk, externalProcs);

            // 3. Punem numele metodei (constant string)
            emitConstant(vData(funcSource->children[1]->value), chunk, 0);

            chunk.addByte((uint8_t)OpCode::OP_CALL_METHOD, 0);
            chunk.addByte((uint8_t)realArgs.size(), 0);
            return;
        }
        
        // =========================================================================
        // 🔥 RAMURĂ APEL DINAMIC PUR (ex: $var()("MERGE") sau $f($val))
        // =========================================================================
        if (isDynamic) {
            LOG_DEBUG(L"  -> Generare apel dinamic bazat pe stivă.");

            // 1. Punem argumentele funcției pe stivă
            uint8_t finalArgCount = 0;
            for (size_t i = 0; i < realArgs.size(); ++i) {
                ASTPtr arg = realArgs[i];
                if (arg->value == L"&" || arg->value == L"ADDRESS_OF") {
                    if (i + 1 < realArgs.size()) {
                        ASTPtr varNode = realArgs[i + 1];
                        uint16_t nameIdx = chunk.addConstant(vData(varNode->value));
                        chunk.addByte((uint8_t)OpCode::OP_GET_ADDR, 0);
                        chunk.addByte((nameIdx >> 8) & 0xFF, 0);
                        chunk.addByte(nameIdx & 0xFF, 0);
                        i++; finalArgCount++;
                        continue;
                    }
                }
                generateFromAST(arg, chunk, externalProcs);
                finalArgCount++;
            }

            // 2. 🔥 REPARARE CRITICĂ: Evaluăm expresia care determină funcția țintă
            std::wstring sourceVal = funcSource->value;

            // Adăugăm verificări stricte: sursa NU trebuie să fie un alt operator, NU trebuie să fie un alt apel de funcție (FunctionCall)
            // și NU trebuie să fie string-ul de control "DYNAMIC_CALL"
            if (funcSource->type != ASTNodeType::Operator &&
                funcSource->type != ASTNodeType::FunctionCall &&
                sourceVal != L"DYNAMIC_CALL" &&
                !sourceVal.empty() &&
                sourceVal[0] != L'$' &&
                sourceVal[0] != L'@')
            {
                LOG_DEBUG(L"  -> Identificator literal de functie detectat: " + sourceVal);
                emitConstant(vData(sourceVal), chunk, 0); // Pune direct string-ul "FACT" pe stivă
            }
            else {
                // Dacă este o variabilă ($var) sau un sub-apel înlănțuit ($var()), o evaluăm normal (recursiv)
                LOG_DEBUG(L"  -> Expresie complexa/variabila pentru functie. Evaluare AST recursiva.");
                generateFromAST(funcSource, chunk, externalProcs);
            }

            // 3. Emitem OpCode-ul care îi spune VM-ului să citească numele funcției direct de pe stivă
            chunk.addByte((uint8_t)OpCode::OP_CALL_DYNAMIC, 0);
            chunk.addByte(finalArgCount, 0);
            return;
        }


        // E. APEL NORMAL (GLOBAL SAU NATIV)
        uint8_t finalArgCount = 0;
        LOG_DEBUG(L"  -> Procesare argumente (" + std::to_wstring(realArgs.size()) + L" gasite)");

        for (size_t i = 0; i < realArgs.size(); ++i) {
            ASTPtr arg = realArgs[i];

            // --- LOGICA PENTRU REFERINȚĂ (&) ---
            // Dacă argumentul este operatorul de adresă, forțăm obținerea adresei variabilei
            if (arg->value == L"&" || arg->value == L"ADDRESS_OF") {
                if (i + 1 < realArgs.size()) {
                    ASTPtr varNode = realArgs[i + 1];
                    LOG_DEBUG(L"    -> Argument[" + std::to_wstring(finalArgCount) + L"]: REFERINTA la " + varNode->value);

                    uint16_t nameIdx = chunk.addConstant(vData(varNode->value));
                    chunk.addByte((uint8_t)OpCode::OP_GET_ADDR, 0);
                    chunk.addByte((nameIdx >> 8) & 0xFF, 0);
                    chunk.addByte(nameIdx & 0xFF, 0);

                    i++; // Consumăm și nodul variabilei, deoarece i-am luat deja adresa
                    finalArgCount++;
                    continue;
                }
            }

            // Generare standard pentru argumente normale (expresii, literali, variabile)
            LOG_DEBUG(L"    -> Argument[" + std::to_wstring(finalArgCount) + L"]: Evaluare valoare (" + arg->value + L")");
            generateFromAST(arg, chunk, externalProcs);
            finalArgCount++;
        }

        // F. EMITERE INSTRUCȚIUNE CALL FINALĂ
        // Alegem între CALL (procedură utilizator) și CALL_NATIVE (funcție C++ înregistrată)
        uint16_t nameIdx = chunk.addConstant(vData(funcName.empty() ? L"__INVALID__" : funcName));
        OpCode callOp = vOliKeyWords::isNativeFunction(funcName) ? OpCode::OP_CALL_NATIVE : OpCode::OP_CALL;

        chunk.addByte((uint8_t)callOp, 0);
        chunk.addByte((uint8_t)((nameIdx >> 8) & 0xFF), 0);
        chunk.addByte((uint8_t)(nameIdx & 0xFF), 0);

        // Transmitem numărul REAL de argumente (fără paranteze sau tokeni skip-uiți)
        chunk.addByte(finalArgCount, 0);

        LOG_DEBUG(L"[COMPILER] Apel finalizat: " + funcName + L" | Args: " + std::to_wstring(finalArgCount));
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




void OliCompiler::emitStore(const std::wstring& varName, OliChunk& chunk) {
    LOG_DEBUG(L"[DEBUG_EMIT] emitStore chemat pentru: " + varName);
    if (varName.empty()) return;

    // --- 0. TRATARE SCOPE GLOBAL FORȚAT (@var = valoare) ---
    if (varName[0] == L'@') {
        LOG_DEBUG(L"[DEBUG_EMIT] -> Forțăm GLOBAL pentru: " + varName);
        // 🔥 FIX: Curățăm prefixul '@' înainte de salvare, pentru ca OP_GET_INDIRECT să îl poată găsi prin string-uri dinamice
        uint16_t nameIdx = chunk.addConstant(vData(cleanVariableName(varName)));
        chunk.addByte((uint8_t)OpCode::OP_SET_GLOBAL, 0);
        chunk.addByte((uint8_t)(nameIdx >> 8), 0);
        chunk.addByte((uint8_t)(nameIdx & 0xFF), 0);
        return;
    }

    // --- 1. LOGICA DE VARIABILE LOCALE ($var = valoare) ---
    if (isInFunction && varName[0] == L'$') {
        // Căutăm dacă variabila există deja în tabela de locale a funcției curente
        int stackIndex = -1;
        for (int i = 0; i < (int)locals.size(); ++i) {
            if (locals[i].name == varName) {
                stackIndex = i;
                break;
            }
        }

        // Dacă nu există, o adăugăm (auto-alocare pe stivă)
        if (stackIndex == -1) {
            stackIndex = (int)locals.size();
            locals.push_back({ varName, 0 });
            LOG_DEBUG(L"[DEBUG_EMIT] -> Alocare variabilă LOCALĂ nouă: " + varName + L" la index: " + std::to_wstring(stackIndex));
        }

        // Emitem instrucțiunea de scriere pe stivă
        chunk.addByte((uint8_t)OpCode::OP_SET_LOCAL, 0);
        chunk.addByte((uint8_t)stackIndex, 0);
        return;
    }

    // --- 2. DEREFERENȚIERE POINTER (*$ptr = valoare) ---
    if (varName[0] == L'*') {
        std::wstring targetVar = varName.substr(1);
        emitLoadOrConstant(targetVar, chunk);
        chunk.addByte((uint8_t)OpCode::OP_SET_PTR, 0);
        return;
    }

    // --- 3. ATRIBUIRE GLOBALĂ (Implicită, când nu suntem în funcție) ---
    LOG_DEBUG(L"[DEBUG_EMIT] -> Emitem OP_SET_GLOBAL (Default) pentru: " + varName);
    // 🔥 FIX: Curățăm prefixul '$' (ex: "$a" devine "a"). La runtime, rezoluția dinamică ($$b) va funcționa impecabil!
    uint16_t nameIdx = chunk.addConstant(vData(cleanVariableName(varName)));
    chunk.addByte((uint8_t)OpCode::OP_SET_GLOBAL, 0);
    chunk.addByte((uint8_t)(nameIdx >> 8), 0);
    chunk.addByte((uint8_t)(nameIdx & 0xFF), 0);
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


void OliCompiler::loadPluginMetadata(std::wstring pluginName) {
    // 1. Curățare cale și ghilimele
    if (pluginName.size() >= 2 && pluginName.front() == L'"' && pluginName.back() == L'"') {
        pluginName = pluginName.substr(1, pluginName.size() - 2);
    }
    if (pluginName.empty()) return;

    // 2. 🔥 FIX CRITIC: Determinăm folderul executabilului principal (Exact ca în VM!)
    std::filesystem::path exeFullPath;
#ifdef _WIN32
    wchar_t exePathBuffer[MAX_PATH];
    GetModuleFileNameW(NULL, exePathBuffer, MAX_PATH);
    exeFullPath = std::filesystem::path(exePathBuffer);
#else
    char exePathBuffer[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exePathBuffer, sizeof(exePathBuffer) - 1);
    if (len != -1) {
        exePathBuffer[len] = '\0';
        exeFullPath = std::filesystem::path(std::string(exePathBuffer));
    } else {
        exeFullPath = std::filesystem::current_path();
    }
#endif

    std::filesystem::path exeDir = exeFullPath.parent_path();

    // 3. Extragem numele curat al DLL-ului
    std::filesystem::path rawPath(pluginName);
    std::wstring pureName = rawPath.stem().wstring();

    // 4. 🔥 FIX CRITIC: Construim calea absolută orientată către subfolderul 'plugins'
    std::filesystem::path finalDllPath = exeDir / "plugins" / pureName;
    std::wstring dllPath = finalDllPath.wstring();

    std::wstring ext = PortTools::getPluginExtension();
    if (dllPath.size() < ext.size() || dllPath.substr(dllPath.size() - ext.size()) != ext) {
        dllPath += ext;
    }

    // Încercăm încărcarea bibliotecii din folderul corect
    PortTools::LibHandle hLib = PortTools::loadDynamicLibrary(dllPath);
    if (!hLib) {
        std::wcerr << L"[COMPILER ERROR] Nu s-a putut incarca metadata din plugin-ul: " << dllPath
            << L" (Cod: " << PortTools::getLastErrorString() << L")" << std::endl;
        return;
    }

    // --- A. ÎNCĂRCARE COMENZI DIN PLUGIN ---
    typedef void (*LoadCommandsFunc)(std::unordered_map<std::wstring, OliCommandHandler>&, void*);
    LoadCommandsFunc regCmds = (LoadCommandsFunc)PortTools::getFunctionSymbol(hLib, "LoadOliCommandPlugin");

    if (regCmds) {
        std::unordered_map<std::wstring, OliCommandHandler> dummyCmds;
        try {
            regCmds(dummyCmds, nullptr);
            for (auto const& [name, handler] : dummyCmds) {
                if (!name.empty()) vOliKeyWords::registerDynamicCommand(name);
            }
        }
        catch (...) {}
    }

    // --- B. ÎNCĂRCARE FUNCȚII DIN PLUGIN ---
    typedef void (*LoadFunctionsFunc)(std::unordered_map<std::wstring, OliFunctionHandler>&, void*);
    LoadFunctionsFunc regFuncs = (LoadFunctionsFunc)PortTools::getFunctionSymbol(hLib, "LoadOliPlugin");

    if (regFuncs) {
        std::unordered_map<std::wstring, OliFunctionHandler> dummyFuncs;
        try {
            regFuncs(dummyFuncs, nullptr);

            for (auto const& [name, handler] : dummyFuncs) {
                if (!name.empty()) {
                    // Convertim la UPPERCASE pentru consistență totală cu parserul general
                    std::wstring upFuncName = name;
                    for (auto& c : upFuncName) c = std::towupper(c);

                    // Înregistrăm funcția în tabela globală de cuvinte cheie a compilatorului
                    vOliKeyWords::registerNativeFunction(upFuncName);

                    // Dezactivează acest log în producție, e doar pentru confirmare vizuală acum
                    std::wcout << L"[COMPILER METADATA] Inregistrat nativ: " << upFuncName << std::endl;
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
		/*
        else if (op == (uint8_t)OpCode::OP_ARRAY || op == (uint8_t)OpCode::OP_MAP || op == (uint8_t)OpCode::OP_CALL_METHOD || op == (uint8_t)OpCode::OP_CALL_DYNAMIC) {
            if (i < subChunk.code.size()) chunk.addByte(subChunk.code[i++], 0);
        }
		*/
		else if (op == (uint8_t)OpCode::OP_ARRAY || 
				 op == (uint8_t)OpCode::OP_MAP || 
				 op == (uint8_t)OpCode::OP_CALL_METHOD || 
				 op == (uint8_t)OpCode::OP_CALL_DYNAMIC ||
				 op == (uint8_t)OpCode::OP_GET_LOCAL ||   // <--- ADĂUGAT
				 op == (uint8_t)OpCode::OP_SET_LOCAL)    // <--- ADĂUGAT
		{
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

