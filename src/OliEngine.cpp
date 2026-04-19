#include "math/vmath.hpp"
#include "OliEngine.hpp"
#include "OliExpressionParser.hpp"


#include <fstream>
#include <filesystem>
#include <random>
#include <vector>
#include <string>

void vOliEngine::execute(const std::wstring& line) {
    // 1. Curățare
    std::wstring cleanLine = trim(line);
    if (cleanLine.empty() && m_accumulator.empty()) return;

    // Eliminare comentarii
    size_t commentPos = cleanLine.find(L'#');
    if (commentPos != std::wstring::npos) {
        cleanLine = trim(cleanLine.substr(0, commentPos));
    }

    std::wstring upperLine = cleanLine;
    std::transform(upperLine.begin(), upperLine.end(), upperLine.begin(), ::towupper);

    // --- LOG DIAGNOSTIC ---
    //LOG_DEBUG(L"[EXEC] Input: '" + cleanLine + L"' | Depth before: " + std::to_wstring(m_blockDepth));

    // 2. Gestionare înregistrare FUNC/PROC
    if (m_isRecording || m_isRecordingFunc) {
        if (upperLine == L"ENDPROC" || upperLine == L"ENDFUNC") {
            m_isRecording = false; m_isRecordingFunc = false; m_blockDepth = 0;
            vOliKeyWords::registerDynamicCommand(m_activeProcName);
            LOG_SUCCESS(L"Procedure/Function saved.");
            return;
        }
        if (m_isRecording) m_procedures[m_activeProcName].body.push_back(cleanLine);
        else m_userFunctions[m_activeFuncName].body.push_back(cleanLine);
        return;
    }

    // 3. Tracking adâncime (Versiune ultra-sensibilă la loguri)
    auto checkAndLog = [&](const std::wstring& key, bool increment) {
        size_t p = upperLine.find(key);
        if (p != std::wstring::npos) {
            // Verificare cuvânt întreg
            bool startOk = (p == 0 || iswspace(upperLine[p - 1]));
            bool endOk = (p + key.length() >= upperLine.length() || iswspace(upperLine[p + key.length()]));

            if (startOk && endOk) {
                if (increment) m_blockDepth++;
                else if (m_blockDepth > 0) m_blockDepth--;
                //LOG_DEBUG(L"[BLOCK] Found '" + key + L"'. New Depth: " + std::to_wstring(m_blockDepth));
                return true;
            }
        }
        return false;
    };

    checkAndLog(L"IF", true);
    checkAndLog(L"WHILE", true);
    checkAndLog(L"FOR", true);
    checkAndLog(L"REPEAT", true);
    checkAndLog(L"CYCLE", true);
    checkAndLog(L"PROC", true);
    checkAndLog(L"FUNC", true);
    checkAndLog(L"SWITCH", true); // <--- ADAUGĂ ASTA

    checkAndLog(L"ENDIF", false);
    checkAndLog(L"ENDWHILE", false);
    checkAndLog(L"ENDFOR", false);
    checkAndLog(L"ENDREPEAT", false);
    checkAndLog(L"ENDCYCLE", false);
    checkAndLog(L"ENDPROC", false);
    checkAndLog(L"ENDFUNC", false);
    checkAndLog(L"ENDSWITCH", false); // <--- ADAUGĂ ASTA

    // 4. Acumulare
    bool hasBackslash = (!cleanLine.empty() && cleanLine.back() == L'\\');
    if (hasBackslash) cleanLine.pop_back();

    if (!m_accumulator.empty()) m_accumulator += L"\n";
    m_accumulator += cleanLine;

    // 5. Declanșare PROC/FUNC
    if (upperLine.find(L"PROC ") == 0 || upperLine.find(L"FUNC ") == 0) {
        //LOG_DEBUG(L"[EXEC] Starting PROC/FUNC definition...");
        std::wstring startCmd = m_accumulator;
        m_accumulator.clear();
        this->executeInternal(startCmd);
        return;
    }

    // 6. Decizia de așteptare
    if (m_blockDepth > 0 || hasBackslash) {
        //LOG_DEBUG(L"[EXEC] Accumulating... (Depth: " + std::to_wstring(m_blockDepth) + L")");
        return;
    }

    // 7. EXECUȚIE BLOC COMPLET
    std::wstring finalBlock = m_accumulator;
    m_accumulator.clear();

    //LOG_DEBUG(L"[EXEC] Block Complete. Sending to executeInternal. Length: " + std::to_wstring(finalBlock.length()));

    if (trim(finalBlock).empty()) return;

    addToHistory(finalBlock);
    this->executeInternal(finalBlock);
}


void vOliEngine::executeInternal(const std::wstring& fullInput) {
    std::wstring trimmedInput = trim(fullInput);
    if (trimmedInput.empty()) return;

    // --- PASUL 1: DESCOMPUNEREA ÎN INSTRUCȚIUNI ---
    // preParse trebuie să spargă "if ... endif; return ..." în două bucăți
    std::vector<std::wstring> instructions = preParse(trimmedInput);

    // Dacă avem mai multe instrucțiuni (separate prin ; sau \n), le procesăm pe rând
    if (instructions.size() > 1) {
        //LOG_DEBUG(L"[EXEC] Multi-line block detected (" + std::to_wstring(instructions.size()) + L" lines)");
        for (const auto& instr : instructions) {
            if (instr.empty()) continue;

            // Verificăm dacă un 'return' anterior a cerut oprirea execuției
            if (m_shouldReturn) {
                //LOG_DEBUG(L"[EXEC] Skipping instruction due to early return: " + instr);
                break;
            }

            this->executeInternal(instr); // Recursivitate pentru fiecare linie

            if (m_executionStatus != OliStatus::RUNNING) break;
        }
        return;
    }

    // --- PASUL 2: PROCESAREA UNEI SINGURE INSTRUCȚIUNI ---
    std::wstring input = instructions[0];
   // LOG_DEBUG(L"[EXEC] Current Instruction: " + input);

    // Identificăm dacă este un bloc de control (pentru a-l trimite la handler ca tot unitar)
    std::wstring upperCheck = input;
    std::transform(upperCheck.begin(), upperCheck.end(), upperCheck.begin(), ::towupper);

    bool isControlBlock = false;
    static const std::vector<std::wstring> controlKeywords = {
        L"WHILE", L"REPEAT", L"IF", L"FOR", L"PROC", L"FUNC", L"CYCLE", L"SWITCH"
    };

    for (const auto& key : controlKeywords) {
        if (upperCheck.find(key) == 0) {
            if (upperCheck.length() == key.length() || iswspace(upperCheck[key.length()])) {
                isControlBlock = true;
                break;
            }
        }
    }

    // Extragem primul cuvânt pentru identificarea comenzii
    size_t firstSpace = input.find_first_of(L" \t\n\r(");
    std::wstring firstWord = (firstSpace != std::wstring::npos) ? input.substr(0, firstSpace) : input;
    std::wstring upperFirst = firstWord;
    std::transform(upperFirst.begin(), upperFirst.end(), upperFirst.begin(), ::towupper);

    // --- PRIORITATE 1: COMENZI DE SISTEM ȘI BLOCURI DE CONTROL ---
    if (isControlBlock || vOliKeyWords::isShellCommand(upperFirst)) {
        //LOG_DEBUG(L"[EXEC] Routing to command handler: " + upperFirst);
        this->executeCommand(input);
        return;
    }

    // --- PRIORITATE 2: PROCEDURI UTILIZATOR ---
    if (m_procedures.count(firstWord)) {
        //LOG_DEBUG(L"[EXEC] Calling user procedure: " + firstWord);
        std::wstring argsPart = (firstSpace != std::wstring::npos) ? input.substr(firstSpace + 1) : L"";
        std::vector<std::wstring> rawTokens = wexplodeQuoteSafe(argsPart, L' ');
        std::vector<std::wstring> cleanArgs;
        for (const auto& arg : rawTokens) {
            std::wstring t = trim(arg);
            if (!t.empty()) cleanArgs.push_back(t);
        }
        callProcedure(m_procedures[firstWord], cleanArgs);
        return;
    }

    // --- PRIORITATE 3: ATRIBUIRE (x = 5) ---
    /*
    size_t eqPos = input.find(L'=');
    if (eqPos != std::wstring::npos && eqPos > 0) {
        // Excludem operatorii de comparație (==, !=, <=, >=)
        bool isComp = (eqPos + 1 < input.size() && input[eqPos + 1] == L'=') ||
            (input[eqPos - 1] == L'!' || input[eqPos - 1] == L'>' || input[eqPos - 1] == L'<');

        if (!isComp) {
            std::wstring leftSide = trim(input.substr(0, eqPos));
            if (!leftSide.empty() && (leftSide[0] == L'$' || iswalpha(leftSide[0]))) {
                if (leftSide[0] == L'$') leftSide.erase(0, 1);
                //LOG_DEBUG(L"[EXEC] Assignment detected for: " + leftSide);
                executeCommand(L"SET " + leftSide + L" = " + input.substr(eqPos + 1));
                return;
            }
        }
    }
    */
    // --- PRIORITATE 3: ATRIBUIRE (x = 5, x += 5, etc.) ---
    size_t eqPos = input.find(L'=');
    if (eqPos != std::wstring::npos && eqPos > 0) {
        // Excludem operatorii de comparație (==, !=, <=, >=)
        bool isComp = (eqPos + 1 < input.size() && input[eqPos + 1] == L'=') ||
            (input[eqPos - 1] == L'!' || input[eqPos - 1] == L'>' || input[eqPos - 1] == L'<');

        if (!isComp) {
            // Detectăm dacă avem un operator compus (+=, -=, *=, /=)
            size_t varEndPos = eqPos;
            wchar_t prevChar = input[eqPos - 1];
            bool isCompound = (prevChar == L'+' || prevChar == L'-' || prevChar == L'*' || prevChar == L'/');

            if (isCompound) {
                varEndPos = eqPos - 1; // Ajustăm tăierea pentru a scoate operatorul din numele variabilei
            }

            std::wstring leftSide = trim(input.substr(0, varEndPos));

            // Verificăm dacă partea stângă este o variabilă validă
            if (!leftSide.empty() && (leftSide[0] == L'$' || iswalpha(leftSide[0]))) {
                // Reconstruim comanda SET pentru a include operatorul corect
                // Trimitem totul către handleSetCommand care acum știe să proceseze +=
                executeCommand(L"SET " + input);
                return;
            }
        }
    }

    // --- PRIORITATE 4: EVALUARE EXPRESIE (FALLBACK) ---
    try {
        //LOG_DEBUG(L"[EXEC] Fallback to expression evaluation: " + input);
        vData result = evaluateExpression(input);
        if (!result.isNull()) {
            LOG_RAW(vDataToWString(result));
        }
    }
    catch (const std::exception& e) {
        std::string err(e.what());
        LOG_ERROR(L"Unknown command or expression error: " + std::wstring(err.begin(), err.end()));
    }
    catch (...) {
        LOG_ERROR(L"Unknown error executing: " + firstWord);
    }
}

  
void vOliEngine::executeCommand(const std::wstring& fullCommand) {
    if (fullCommand.empty()) return;

    // Extragem numele comenzii (ținem cont de spații, tab-uri și linii noi)
    std::wstring cmdName;
    size_t firstSpace = fullCommand.find_first_of(L" \t\n\r");
    cmdName = (firstSpace != std::wstring::npos) ? fullCommand.substr(0, firstSpace) : fullCommand;

    std::wstring cmdUpper = cmdName;
    std::transform(cmdUpper.begin(), cmdUpper.end(), cmdUpper.begin(), ::towupper);

    // 1. Căutăm în handler-ele de sistem (REPEAT, WHILE, IF, SET, etc.)
    auto it = m_commandHandlers.find(cmdUpper);
    if (it != m_commandHandlers.end()) {
        it->second(fullCommand);
    }
    // 2. Căutăm în procedurile utilizatorului
    else if (m_procedures.count(cmdName)) {
        ShellCommand sc = vOliCommandParser::parse(fullCommand);
        callProcedure(m_procedures[cmdName], sc.args);
    }
    else {
        LOG_ERROR(L"Unknown command or procedure: " + cmdName);
    }
}

std::vector<std::wstring> vOliEngine::preParse(const std::wstring& input) {
    std::vector<std::wstring> result;
    std::wstring current;
    int depth = 0;
    bool inQuotes = false;

    for (size_t i = 0; i < input.length(); ++i) {
        wchar_t c = input[i];

        if (c == L'"' && (i == 0 || input[i - 1] != L'\\')) {
            inQuotes = !inQuotes;
        }

        if (!inQuotes) {
            bool isStart = (i == 0 || iswspace(input[i - 1]) || input[i - 1] == L';');
            if (isStart) {
                // Extragem restul liniei și îl facem UpperCase pentru verificare
                std::wstring_view remView(&input[i], input.length() - i);
                std::wstring remUpper;
                for (size_t j = 0; j < 10 && j < remView.size(); ++j) // Luăm doar începutul
                    remUpper += std::towupper(remView[j]);

                auto check = [&](const std::wstring& k) {
                    if (remUpper.size() < k.size()) return false;
                    if (remUpper.substr(0, k.size()) != k) return false;
                    return (remUpper.size() == k.size() || iswspace(remUpper[k.size()]) || remUpper[k.size()] == L';');
                };

                if (check(L"WHILE") || check(L"REPEAT") || check(L"IF") || check(L"FOR") || check(L"CYCLE") || check(L"SWITCH")) {
                    depth++;
                }
            }

            // Tăiem instrucțiunea DOAR la depth 0
            if (depth == 0 && (c == L';' || c == L'\n')) {
                if (!trim(current).empty()) result.push_back(trim(current));
                current.clear();
                continue;
            }
        }

        current += c;

        // Verificăm scăderea depth-ului
        if (!inQuotes) {
            auto checkEnd = [&](const std::wstring& k) {
                if (current.length() < k.length()) return false;
                std::wstring tail = current.substr(current.length() - k.length());
                for (auto& ch : tail) ch = std::towupper(ch);
                if (tail != k) return false;
                // Verificăm să nu fie parte dintr-un alt cuvânt (în stânga)
                size_t startIdx = current.length() - k.length();
                if (startIdx > 0 && !iswspace(current[startIdx - 1]) && current[startIdx - 1] != L';') return false;
                return true;
            };

            if (checkEnd(L"ENDWHILE") || checkEnd(L"ENDREPEAT") || checkEnd(L"ENDIF") || checkEnd(L"ENDFOR") || checkEnd(L"ENDCYCLE") || checkEnd(L"ENDSWITCH")) {
                depth--;
                if (depth < 0) depth = 0;
            }
        }
    }

    if (!trim(current).empty()) result.push_back(trim(current));
    return result;
}

void vOliEngine::addToHistory(const std::wstring& command) {
    if (command.empty()) return;
    m_history.push_back(command);

    std::wofstream historyFile("history.txt", std::ios::app);
    if (historyFile.is_open()) {
        historyFile << command << std::endl << L"---" << std::endl; // Separator pentru blocuri
    }
}
    

    void vOliEngine::initializeCommandsHandlers() {
        // --- HELPER ADAPTOR ---
        // Această lambda mică ia string-ul brut, îl parsează și îl trimite la handler-ul tău vechi.
        // Ne scutește de rescrierea tuturor funcțiilor handleXCommand.
        auto wrap = [this](auto handlerFunc) {
            return [this, handlerFunc](const std::wstring& rawLine) {
                ShellCommand sc = vOliCommandParser::parse(rawLine);
                handlerFunc(sc);
            };
        };

        // --- COMANDĂ COMPLEXĂ (/IF) ---
        // Presupunem că handleIfCommand(const std::wstring&) este gata.
        // Dacă încă primește ShellCommand, folosește wrap([this](const auto& sc) { handleIfCommand(sc); });
        m_commandHandlers[L"IF"] = [this](const std::wstring& rawLine) {
            handleIfCommand(rawLine);
        };

        m_commandHandlers[L"CYCLE"] = [this](const std::wstring& rawLine) {
            handleCycleCommand(rawLine);
        };

        m_commandHandlers[L"WHILE"] = [this](const std::wstring& rawLine) {
            handleWhileCommand(rawLine);
        };
      
        m_commandHandlers[L"FOR"] = [this](const std::wstring& rawLine) {
            handleForCommand(rawLine);
        };
        
        m_commandHandlers[L"REPEAT"] = [this](const std::wstring& rawLine) {
            handleRepeatCommand(rawLine);
        };

        m_commandHandlers[L"SWITCH"] = [this](const std::wstring& rawLine) {
            handleSwitchCommand(rawLine);
        };


        // --- COMENZI DE SISTEM ---
        m_commandHandlers[L"QUIT"] = wrap([this](const auto& sc) { handleQuitCommand(sc); });
        m_commandHandlers[L"EXIT"] = m_commandHandlers[L"QUIT"];
        m_commandHandlers[L"Q"] = m_commandHandlers[L"QUIT"];

        // --- GESTIUNE VARIABILE ---
        m_commandHandlers[L"SET"] = wrap([this](const auto& sc) { handleSetCommand(sc); });
        m_commandHandlers[L"S"] = m_commandHandlers[L"SET"];

        m_commandHandlers[L"UNSET"] = wrap([this](const auto& sc) { handleUnsetCommand(sc); });
        m_commandHandlers[L"U"] = m_commandHandlers[L"UNSET"];

        // --- AFIȘARE ---
        m_commandHandlers[L"ECHO"] = wrap([this](const auto& sc) { handleEchoCommand(sc); });
        m_commandHandlers[L"E"] = m_commandHandlers[L"ECHO"];
        m_commandHandlers[L"ECHO_DBG"] = wrap([this](const auto& sc) { handleEchoCommand(sc); });
        m_commandHandlers[L"ED"] = m_commandHandlers[L"ECHO_DBG"];
        
        m_commandHandlers[L"CLEAR"] = wrap([this](const auto& sc) { handleClearCommand(sc); });
        m_commandHandlers[L"CLS"] = m_commandHandlers[L"CLEAR"];

        // --- MEMORIE / DEBUG ---
        m_commandHandlers[L"DUMP_MEM"] = wrap([this](const auto& sc) { handleDumpMemCommand(sc); });
        m_commandHandlers[L"DM"] = m_commandHandlers[L"DUMP_MEM"];
        m_commandHandlers[L"VARS"] = m_commandHandlers[L"DUMP_MEM"];

        m_commandHandlers[L"TRACE"] = wrap([this](const auto& sc) { handleTraceCommand(sc); });

        // INFO și HELP (din OliKeyWords)
        m_commandHandlers[L"INFO"] = wrap([this](const auto& sc) { /* handleInfoCommand(sc); */ });
        m_commandHandlers[L"D"] = m_commandHandlers[L"INFO"];
        m_commandHandlers[L"HELP"] = wrap([this](const auto& sc) { /* handleHelpCommand(sc); */ });
        m_commandHandlers[L"H"] = m_commandHandlers[L"HELP"];

        m_commandHandlers[L"RUN"] = wrap([this](const auto& sc) { handleRunCommand(sc); });
        m_commandHandlers[L"R"] = m_commandHandlers[L"RUN"];

        m_commandHandlers[L"SYS"] = wrap([this](const auto& sc) { handleSysCommand(sc); });
        
        m_commandHandlers[L"PROC"] = wrap([this](const auto& sc) { handleProcCommand(sc); });

        m_commandHandlers[L"FUNC"] = wrap([this](const auto& sc) { handleFuncCommand(sc); });

        m_commandHandlers[L"PLUGIN"] = wrap([this](const auto& sc) { handlePluginCommand(sc); });

        m_commandHandlers[L"LIST_PROCS"] = wrap([this](const auto& sc) { handleListProcsCommand(sc); });
        m_commandHandlers[L"LP"] = m_commandHandlers[L"LIST_PROCS"];
        m_commandHandlers[L"PROC_DUMP"] = m_commandHandlers[L"LIST_PROCS"];

        m_commandHandlers[L"LIST_FUNCS"] = wrap([this](const auto& sc) { handleListFuncsCommand(sc); });
        
            

        m_commandHandlers[L"BREAK"] = wrap([this](const auto& sc) { handleBreakCommand(sc); });
        m_commandHandlers[L"CONTINUE"] = wrap([this](const auto& sc) { handleContinueCommand(sc); });

        m_commandHandlers[L"RETURN"] = wrap([this](const auto& sc) { handleReturnCommand(sc); });
        m_commandHandlers[L"RET"] = m_commandHandlers[L"RETURN"];

        m_commandHandlers[L"DEFINE"] = wrap([this](const auto& sc) { handleDefCommand(sc); });
        m_commandHandlers[L"DEF"] = m_commandHandlers[L"DEFINE"];

        
    }

    void vOliEngine::handleQuitCommand(const ShellCommand& sc) {
        LOG_INFO(L"Oli is shutting down...");
        stop();
    }
    
    

    void vOliEngine::handleEchoCommand(const ShellCommand& sc) {
        if (sc.args.empty()) return;

        // 1. Recompunem linia originală
        std::wstring fullLine;
        for (size_t i = 0; i < sc.args.size(); ++i) {
            fullLine += sc.args[i] + (i < sc.args.size() - 1 ? L" " : L"");
        }

        // 2. Evaluăm direct tot ce primim ca pe o expresie
        // Parserul va recunoaște singur ce e string literal, ce e variabilă și ce e map access
        vData result = evaluateExpression(fullLine);

        // 3. Afișăm rezultatul final
        if (result.isString()) {
            std::wcout << std::get<std::wstring>(result.value) << std::endl;
        }
        else {
            std::wcout << vDataToWString(result) << std::endl;
        }
    }
    

    void vOliEngine::handleSetCommand(const ShellCommand& sc) {
        if (sc.args.empty()) return;

        // 1. Reconstituim linia
        std::wstring fullLine;
        for (const auto& a : sc.args) fullLine += a + L" ";
        fullLine = trim(fullLine);

        // 2. Gestionăm flag-ul global
        bool forceGlobal = false;
        if (fullLine.size() >= 7 && fullLine.substr(0, 7) == L"global ") {
            forceGlobal = true;
            fullLine = trim(fullLine.substr(7));
        }

        // IMPORTANT: Setăm flag-ul în engine înainte de orice procesare
        if (forceGlobal) {
            m_nextSetIsGlobal = true;
        }

        // 3. Tokenizăm și Parsăm
        auto tokens = vOliCommandParser::tokenize(fullLine);
        if (tokens.empty()) {
            m_nextSetIsGlobal = false;
            return;
        }

        OliExpressionParser exprParser(tokens);
        ASTPtr root = exprParser.parseAssignment();

        if (!root) {
            LOG_ERROR(L"[RUNTIME ERROR] Invalid assignment expression.");
            m_nextSetIsGlobal = false;
            return;
        }

        // 4. EXECUȚIA UNITARĂ
        // Nu mai interceptăm manual "if (leftNode->type == Variable)".
        // executeAST(root) va ajunge în final să cheme assignToVariable() 
        // care va vedea m_nextSetIsGlobal = true.
        try {
            executeAST(root);
        }
        catch (...) {
            LOG_ERROR(L"Eroare la execuția AST-ului de asignare.");
        }

        // 5. Resetăm flag-ul după ce TOATĂ operațiunea s-a terminat
        m_nextSetIsGlobal = false;
    }


    std::wstring vOliEngine::getVariantTypeName(const vData& data) {
        if (std::holds_alternative<long long>(data.value))      return L"INT";
        if (std::holds_alternative<double>(data.value))         return L"FLOAT";
        if (std::holds_alternative<std::wstring>(data.value))   return L"STRING";
        if (std::holds_alternative<bool>(data.value))           return L"BOOL";
        if (std::holds_alternative<vDataArray>(data.value))     return L"ARRAY";
        if (std::holds_alternative<vDataMap>(data.value))       return L"MAP";
        if (std::holds_alternative<std::monostate>(data.value)) return L"NULL";
        return L"UNKNOWN";
    }

    vData vOliEngine::evaluateExpression(const std::wstring& expr) {
        // Apel corect prin numele clasei:
        std::vector<std::wstring> tokens = vOliCommandParser::tokenize(expr);

        if (tokens.empty()) return { std::monostate{} };

        OliExpressionParser parser(tokens);
        ASTPtr plan = parser.parse();
        /*
        if (plan) {
            std::wcout << L"--- Debug AST ---" << std::endl;
            plan->dump(); // Asta îți va desena arborele în consolă
            std::wcout << L"-----------------" << std::endl;
        }
        */
        return executeAST(plan);
    }
    
/*
vData vOliEngine::resolveVariable(const std::wstring& rawVar) {
    if (rawVar.empty()) return { std::monostate{} };

    // 1. Variabile variabile ($$a) - ramane neschimbat
    // 1. Variabile variabile: număr nelimitat de $
    // Variabile variabile: suportă orice număr de $
    if (!rawVar.empty() && rawVar[0] == L'$') {
        size_t dollarCount = 0;
        while (dollarCount < rawVar.size() && rawVar[dollarCount] == L'$')
            dollarCount++;

        // Rezolvăm ce a rămas după semnele $
        vData result = resolveVariable(rawVar.substr(dollarCount));

        // Dereferențiem pentru fiecare $ extra (peste primul)
        // EX: Pentru $$b, dollarCount este 2. Facem o dereferențiere extra.
        for (size_t i = 1; i < dollarCount; ++i) {
            if (const std::wstring* s = std::get_if<std::wstring>(&result.value)) {
                // Căutăm variabila cu numele stocat în string (indiferent dacă are $ sau nu)
                result = resolveVariable(*s);
            }
            else {
                break;
            }
        }
        return result;
    }

    

    // 2. Curățarea numelui
    std::wstring varName = rawVar;
    if (varName[0] == L'$') varName = varName.substr(1);

    // Eliminăm accesările de tip proprietate sau index pentru a găsi variabila rădăcină
    size_t firstSeparator = varName.find_first_of(L".[");
    if (firstSeparator != std::wstring::npos) {
        varName = varName.substr(0, firstSeparator);
    }
    varName = trim(varName);

    // 3. LOGICA DE SCOPING (Local-First, Global-Second)

    // Pasul A: Căutăm în Frame-ul de deasupra al stivei (Contextul Local al funcției)
    if (!m_callStack.empty()) {
        auto& locals = m_callStack.back().localVariables;
        auto it = locals.find(varName);
        if (it != locals.end()) {
            return it->second;
        }
    }

    // Pasul B: Căutăm în buzunarul Global (care acum este m_globalVariables)
    // Aici va fi găsit $memo chiar dacă suntem în interiorul FIBO_MEMO
    auto itGlobal = m_globalVariables.find(varName);
    if (itGlobal != m_globalVariables.end()) {
        return itGlobal->second;
    }

    // 4. Dacă nu a fost găsită nicăieri, returnăm NULL
    return { std::monostate{} };
}
*/
    /*
    vData vOliEngine::resolveVariable(const std::wstring& rawVar) {
        if (rawVar.empty()) return { std::monostate{} };

        // 1. GESTIONARE GLOBALĂ EXPLICITĂ (@nume)
        if (rawVar[0] == L'@') {
            std::wstring globalName = rawVar.substr(1);

            // Eliminăm eventuale proprietăți/indexări pentru a extrage rădăcina
            size_t firstSep = globalName.find_first_of(L".[");
            if (firstSep != std::wstring::npos) globalName = globalName.substr(0, firstSep);

            globalName = trim(globalName);

            auto it = m_globalVariables.find(globalName);
            if (it != m_globalVariables.end()) return it->second;

            return { std::monostate{} };
        }

        // 2. VARIABILE VARIABILE ($$a, $$$b)
        if (rawVar[0] == L'$') {
            size_t dollarCount = 0;
            while (dollarCount < rawVar.size() && rawVar[dollarCount] == L'$')
                dollarCount++;

            // Rezolvăm restul (poate fi un nume simplu sau altceva)
            vData result = resolveVariable(rawVar.substr(dollarCount));

            // Dereferențiem pentru fiecare $ extra
            for (size_t i = 1; i < dollarCount; ++i) {
                if (const std::wstring* s = std::get_if<std::wstring>(&result.value)) {
                    // IMPORTANT: Aici apelăm tot resolveVariable pentru a permite
                    // ca valoarea din interior să fie ea însăși o variabilă globală (@x)
                    result = resolveVariable(*s);
                }
                else {
                    break;
                }
            }
            return result;
        }

        // 3. CURĂȚARE ȘI SCOPING NORMAL (Local -> Global)
        std::wstring varName = rawVar;

        // Extragere rădăcină (root name)
        size_t firstSeparator = varName.find_first_of(L".[");
        if (firstSeparator != std::wstring::npos) {
            varName = varName.substr(0, firstSeparator);
        }
        varName = trim(varName);

        // Pasul A: Local
        if (!m_callStack.empty()) {
            auto& locals = m_callStack.back().localVariables;
            auto it = locals.find(varName);
            if (it != locals.end()) return it->second;
        }

        // Pasul B: Global
        auto itGlobal = m_globalVariables.find(varName);
        if (itGlobal != m_globalVariables.end()) return itGlobal->second;

        return { std::monostate{} };
    }
    */

vData vOliEngine::resolveVariable(const std::wstring& rawVar) {
    if (rawVar.empty()) return { std::monostate{} };

    std::wstring currentPath = rawVar;
    bool forceGlobal = false;

    // --- PASUL 1: Detecție Scope (@) ---
    // Nu returnăm imediat, doar setăm un flag și curățăm șirul
    if (currentPath[0] == L'@') {
        forceGlobal = true;
        currentPath.erase(0, 1);
    }

    // --- PASUL 2: Rezolvare Indirectare recursivă ($) ---
    // Aici lăsăm logica ta de dollarCount, dar aplicată pe currentPath
    if (!currentPath.empty() && currentPath[0] == L'$') {
        size_t dollarCount = 0;
        while (dollarCount < currentPath.size() && currentPath[dollarCount] == L'$')
            dollarCount++;

        // Rezolvăm ce e după semnele $
        vData result = resolveVariable(currentPath.substr(dollarCount));

        // Dereferențiem pentru fiecare $
        for (size_t i = 0; i < dollarCount; ++i) {
            if (const std::wstring* s = std::get_if<std::wstring>(&result.value)) {
                // Dacă avem @ în față, rezultatul final al dereferențierii 
                // trebuie căutat tot în context global
                std::wstring nextLookup = (forceGlobal ? L"@" : L"") + *s;
                result = resolveVariable(nextLookup);
            }
            else {
                break;
            }
        }
        return result;
    }

    // --- PASUL 3: Identificare Rădăcină și Scoping ---
    std::wstring varName = currentPath;
    size_t firstSep = varName.find_first_of(L".[");
    if (firstSep != std::wstring::npos) varName = varName.substr(0, firstSep);
    varName = trim(varName);

    // A: Dacă am avut @, căutăm DOAR în Global
    if (forceGlobal) {
        auto it = m_globalVariables.find(varName);
        return (it != m_globalVariables.end()) ? it->second : vData{ std::monostate{} };
    }

    // B: Scoping normal (Local -> Global)
    if (!m_callStack.empty()) {
        auto& locals = m_callStack.back().localVariables;
        auto it = locals.find(varName);
        if (it != locals.end()) return it->second;
    }

    auto itGlobal = m_globalVariables.find(varName);
    if (itGlobal != m_globalVariables.end()) return itGlobal->second;

    return { std::monostate{} };
}

    void vOliEngine::printVData(const vData& data, bool debugMode) {
        if (data.isNull()) {
            std::wcout << L"~"; // Simbol pentru null/moonstate
            return;
        }

        std::visit([this, debugMode](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, long long>) {
                std::wcout << arg;
            }
            else if constexpr (std::is_same_v<T, double>) {
                // Afișăm cu 2 zecimale pentru claritate
                wchar_t buf[64];
                swprintf(buf, 64, L"%.2f", arg);
                std::wcout << buf;
            }
            else if constexpr (std::is_same_v<T, std::wstring>) {
                if (debugMode) std::wcout << L"\"" << arg << L"\"";
                else std::wcout << arg;
            }
            else if constexpr (std::is_same_v<T, bool>) {
                std::wcout << (arg ? L"true" : L"false");
            }
            else if constexpr (std::is_same_v<T, vDataArray>) {
                std::wcout << L"[";
                for (size_t i = 0; i < arg.size(); ++i) {
                    printVData(arg[i], debugMode); // Recursivitate sigură
                    if (i < arg.size() - 1) std::wcout << L", ";
                }
                std::wcout << L"]";
            }
            else if constexpr (std::is_same_v<T, vDataMap>) {
                std::wcout << L"{";
                for (auto it = arg.begin(); it != arg.end(); ++it) {
                    std::wcout << L"\"" << it->first << L"\": ";
                    printVData(it->second, debugMode);
                    if (std::next(it) != arg.end()) std::wcout << L", ";
                }
                std::wcout << L"}";
            }
            else if constexpr (std::is_same_v<T, std::monostate>) {
                std::wcout << L"null";
            }
            }, data.value);
    }

    void vOliEngine::handleDumpMemCommand(const ShellCommand& sc) {
        if (m_globalVariables.empty()) {
            LOG_INFO(L"Memory is empty. No variables set.");
            return;
        }

        std::wcout << L"\n--- [Oli Memory Dump] ---" << std::endl;
        // Cap de tabel aliniat (Nume: 15 caractere, Tip: 10 caractere)
        std::wcout << std::left << std::setw(15) << L"NAME"
            << std::setw(12) << L"TYPE"
            << L"VALUE" << std::endl;
        std::wcout << std::wstring(40, L'-') << std::endl;

        for (const auto& [name, data] : m_globalVariables) {
            std::wcout << std::left << std::setw(15) << name
                << std::setw(12) << getVariantTypeName(data);

            // Folosim printVData-ul pe care l-am făcut anterior
            printVData(data, true);

            std::wcout << std::endl;
        }
        std::wcout << std::wstring(40, L'-') << std::endl;
        std::wcout << L"Total variables: " << m_globalVariables.size() << L"\n" << std::endl;
    }

    vData vOliEngine::parseArrayContent(const std::wstring& content) {
        // 1. Verificăm dacă conține ':' (dar nu în ghilimele) pentru a detecta un Map
        bool containsColon = false;
        bool inQuotes = false;
        for (wchar_t c : content) {
            if (c == L'"') inQuotes = !inQuotes;
            if (c == L':' && !inQuotes) { containsColon = true; break; }
        }

        if (containsColon) {
            vDataMap mapResult;
            auto pairs = splitByCommaIgnoringBrackets(content);
            for (const auto& p : pairs) {
                size_t colonPos = p.find(L':');
                if (colonPos != std::wstring::npos) {
                    std::wstring k = normalizeSpaces(p.substr(0, colonPos));
                    if (k.front() == L'"') k = k.substr(1, k.size() - 2); // scoatem ghilimelele

                    std::wstring v = p.substr(colonPos + 1);
                    mapResult[k] = evaluateExpression(v);
                }
            }
            return { mapResult };
        }
        else {
            // Logica veche de Array
            vDataArray arrResult;
            auto elements = splitByCommaIgnoringBrackets(content);
            for (const auto& e : elements) arrResult.push_back(evaluateExpression(e));
            return { arrResult };
        }
    }

    std::wstring vOliEngine::vDataToWString(const vData& data) {
        return std::visit([this](auto&& arg) -> std::wstring {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, std::monostate>) {
                return L"NULL";
            }
            else if constexpr (std::is_same_v<T, long long>) {
                return std::to_wstring(arg);
            }
            else if constexpr (std::is_same_v<T, double>) {
                wchar_t buf[64];
                // Folosim %g pentru a lăsa sistemul să aleagă cel mai scurt format (ex: 12 în loc de 12.0000)
                swprintf(buf, 64, L"%g", arg);
                std::wstring s(buf);

                // Dacă vrei să forțezi un ".0" chiar și la numere întregi (ca să se vadă că e double), 
                // poți adăuga o verificare, dar %g este de obicei ce vor utilizatorii de shell.
                return s;
            }
            else if constexpr (std::is_same_v<T, std::wstring>) {
                return arg;
            }
            else if constexpr (std::is_same_v<T, bool>) {
                return arg ? L"true" : L"false";
            }
            else if constexpr (std::is_same_v<T, vDataArray>) {
                std::wstring res = L"[";
                for (size_t i = 0; i < arg.size(); ++i) {
                    // Apelăm recursiv vDataToWString pentru fiecare element
                    res += this->vDataToWString(arg[i]);
                    if (i < arg.size() - 1) res += L", ";
                }
                res += L"]";
                return res;
            }
            else if constexpr (std::is_same_v<T, vDataMap>) {
                std::wstring res = L"{";
                size_t i = 0;
                for (auto const& [key, val] : arg) {
                    res += key + L": " + this->vDataToWString(val);
                    if (++i < arg.size()) res += L", ";
                }
                res += L"}";
                return res;
            }
            else {
                return L"(UNKNOWN)";
            }
            }, data.value);
    }
    
    /*
    void vOliEngine::assignToVariable(const std::wstring& varExpr, const vData& newValue) {
        size_t bracketStart = varExpr.find(L'[');

        // 1. Variabilă simplă ($v = 10)
        if (bracketStart == std::wstring::npos) {
            setVariable(varExpr, newValue);
            return;
        }

        std::wstring varName = normalizeSpaces(varExpr.substr(0, bracketStart));
        if (!varName.empty() && varName[0] == L'$') varName.erase(0, 1);

        // --- PASUL 1: Găsirea sau Crearea Rădăcinii ---
        vData* rootPtr = nullptr;

        if (!m_callStack.empty()) {
            auto& locals = m_callStack.back().localVariables;
            if (locals.count(varName)) rootPtr = &locals[varName];
        }

        if (!rootPtr && m_globalVariables.count(varName)) {
            rootPtr = &m_globalVariables[varName];
        }

        if (!rootPtr) {
            if (m_nextSetIsGlobal || m_callStack.empty()) {
                rootPtr = &m_globalVariables[varName];
            }
            else {
                rootPtr = &m_callStack.back().localVariables[varName];
            }

            // Inițializare tip la prima creare
            if (varExpr.substr(bracketStart, 2) == L"[]") {
                rootPtr->value = vDataArray{};
            }
            else {
                size_t firstEnd = findClosingBracket(varExpr, bracketStart);
                if (firstEnd != std::wstring::npos) {
                    std::wstring content = varExpr.substr(bracketStart + 1, firstEnd - bracketStart - 1);
                    vData idx = evaluateExpression(content);
                    // Dacă indexul e numeric, presupunem Array, altfel Map
                    if (idx.isInt() || idx.isFloat()) rootPtr->value = vDataArray{};
                    else rootPtr->value = vDataMap{};
                }
            }
        }

        // --- PASUL 2: Navigare către containerul țintă ($a[1][2] -> ținta e $a[1]) ---
        vData* targetContainer = navigateOrCreatePath(rootPtr, varExpr);
        if (!targetContainer) return;

        // Extragerea ultimului index
        size_t lastOpen = varExpr.find_last_of(L'[');
        size_t lastClose = findClosingBracket(varExpr, lastOpen);
        if (lastOpen == std::wstring::npos || lastClose == std::wstring::npos) return;

        std::wstring lastIdxExpr = normalizeSpaces(varExpr.substr(lastOpen + 1, lastClose - lastOpen - 1));
        vData idxVal = lastIdxExpr.empty() ? vData{ std::monostate{} } : evaluateExpression(lastIdxExpr);

        // --- PASUL 3: Asignarea Finală cu Protecția Tipului ---

        // Cazul A: Adăugare la final ($arr[] = val)
        if (lastIdxExpr.empty()) {
            if (!targetContainer->isArray()) targetContainer->value = vDataArray{};
            std::get<vDataArray>(targetContainer->value).push_back(newValue);
        }
        // Cazul B: Containerul este deja un ARRAY
        else if (targetContainer->isArray()) {
            assignToArrayVar(targetContainer, lastIdxExpr, newValue);
        }
        // Cazul C: Containerul este deja un MAP (obiect {})
        else if (targetContainer->isMap()) {
            assignToMapVar(targetContainer, lastIdxExpr, newValue);
        }
        // Cazul D: Containerul este nou/null (decidem tipul acum)
        else {
            if (idxVal.isInt() || idxVal.isFloat()) {
                targetContainer->value = vDataArray{};
                assignToArrayVar(targetContainer, lastIdxExpr, newValue);
            }
            else {
                targetContainer->value = vDataMap{};
                assignToMapVar(targetContainer, lastIdxExpr, newValue);
            }
        }

        // --- DEBUG FINAL ---
        
        std::wcout << L"[DEBUG] Var: " << varName
                   << L" | Path: " << varExpr
                   << L" | Type: " << (targetContainer->isArray() ? L"ARRAY" : L"MAP")
                   << L" | Index: " << vDataToWString(idxVal) << std::endl;
        
    }
    */

    void vOliEngine::assignToVariable(const std::wstring& varExpr, const vData& newValue) {
        std::wstring trimmedExpr = trim(varExpr);

        // Detectăm primul separator (fie [ fie .)
        size_t firstSep = trimmedExpr.find_first_of(L"[.");

        // --- CAZUL 0: Variabilă simplă ---
        if (firstSep == std::wstring::npos) {
            setVariable(trimmedExpr, newValue);
            return;
        }

        // --- 2. DETECȚIE PREFIX ȘI NUME RĂDĂCINĂ ---
        std::wstring fullVarName = normalizeSpaces(trimmedExpr.substr(0, firstSep));
        bool forceGlobal = (!fullVarName.empty() && fullVarName[0] == L'@');

        std::wstring varName = fullVarName;
        if (!varName.empty() && (varName[0] == L'$' || varName[0] == L'@')) {
            varName.erase(0, 1);
        }
        varName = trim(varName);

        // --- 3. GĂSIREA RĂDĂCINII (Shadowing Policy) ---
        vData* rootPtr = nullptr;
        if (forceGlobal) {
            rootPtr = &m_globalVariables[varName];
        }
        else if (!m_callStack.empty()) {
            // Dacă suntem în funcție, tinta e locală (copie a boss-ului global)
            rootPtr = &m_callStack.back().localVariables[varName];
        }
        else {
            rootPtr = &m_globalVariables[varName];
        }

        // Dacă rădăcina e NULL, inițializăm în funcție de primul separator
        if (rootPtr->isNull()) {
            if (trimmedExpr[firstSep] == L'.') rootPtr->value = vDataMap{};
            else rootPtr->value = vDataArray{};
        }

        // --- 4. NAVIGARE CĂTRE PENULTIMUL NOD ---
        // navigateOrCreatePath trebuie să știe să împartă și după puncte!
        vData* targetContainer = navigateOrCreatePath(rootPtr, trimmedExpr);
        if (!targetContainer) return;

        // --- 5. ASIGNAREA FINALĂ ---
        // Extragem ultima parte (după ultimul [ sau .)
        size_t lastSep = trimmedExpr.find_last_of(L"[.");
        if (lastSep == std::wstring::npos) return;

        if (trimmedExpr[lastSep] == L'.') {
            // Asignare prin punct: $obj.prop
            std::wstring field = trim(trimmedExpr.substr(lastSep + 1));
            if (!targetContainer->isMap()) targetContainer->value = vDataMap{};
            std::get<vDataMap>(targetContainer->value)[field] = newValue;
        }
        else {
            // Asignare prin bracket: $arr[idx]
            size_t lastClose = findClosingBracket(trimmedExpr, lastSep);
            if (lastClose == std::wstring::npos) return;
            std::wstring idxExpr = trim(trimmedExpr.substr(lastSep + 1, lastClose - lastSep - 1));

            // Refolosim logica ta existentă pentru Array/Map
            if (idxExpr.empty()) { /* push_back */ }
            else if (targetContainer->isArray()) { assignToArrayVar(targetContainer, idxExpr, newValue); }
            else { assignToMapVar(targetContainer, idxExpr, newValue); }
        }
    }

    void vOliEngine::assignToArrayVar(vData* container, const std::wstring& indexExpr, const vData& newValue) {
        vDataArray& arr = std::get<vDataArray>(container->value);

        if (indexExpr.empty()) {
            // Cazul set a[] = val -> Adaugă elementul la sfârșit
            arr.push_back(newValue);
        }
        else {
            // Cazul set a[$i] = val -> Update la index specific
            vData idxVal = evaluateExpression(indexExpr);
            long long rawIdx = vDataToLong(idxVal); // Acum funcția e găsită!

            if (rawIdx < 0) return; // Index invalid

            size_t idx = static_cast<size_t>(rawIdx);
            if (idx >= arr.size()) {
                arr.resize(idx + 1, { std::monostate{} });
            }
            arr[idx] = newValue;
        }
    }

    void vOliEngine::assignToMapVar(vData* container, const std::wstring& indexExpr, const vData& newValue) {
        if (!std::holds_alternative<vDataMap>(container->value)) container->value = vDataMap();
        vDataMap& m = std::get<vDataMap>(container->value);

        vData keyVal = evaluateExpression(indexExpr); // Suportă chei dinamice
        std::wstring key = vDataToWString(keyVal);

        m[key] = newValue;
    }

    std::vector<std::wstring> vOliEngine::splitPath(const std::wstring& expr) {
        std::vector<std::wstring> tokens;
        std::wstring current;
        for (wchar_t c : expr) {
            if (c == L'.' || c == L'[' || c == L']') {
                if (!current.empty()) tokens.push_back(current);
                current.clear();
            }
            else if (c != L'$' && c != L'@' && c != L' ') {
                current += c;
            }
        }
        if (!current.empty()) tokens.push_back(current);
        return tokens;
    }


    vData* vOliEngine::navigateOrCreatePath(vData* root, const std::wstring& varExpr) {
        if (!root) return nullptr;

        // 1. Spargem calea în componente: "tinta.stats.hp" -> ["tinta", "stats", "hp"]
        std::vector<std::wstring> tokens = splitPath(varExpr);

        // Dacă avem doar rădăcina (ex: "$tinta"), returnăm direct root
        if (tokens.size() <= 1) return root;

        vData* current = root;

        // 2. Navigăm până la PENULTIMUL token
        // Exemplu pentru tinta.stats.hp: i merge de la 1 la 1 (doar pentru "stats")
        for (size_t i = 1; i < tokens.size() - 1; ++i) {
            std::wstring key = tokens[i];

            // Curățăm ghilimelele dacă indexul a fost de tip string: ["stats"] -> stats
            if (key.size() >= 2 && key.front() == L'\"' && key.back() == L'\"') {
                key = key.substr(1, key.size() - 2);
            }

            // Determinăm tipul următorului nivel pentru auto-inițializare
            std::wstring nextKey = tokens[i + 1];
            bool nextIsNumeric = !nextKey.empty() && std::iswdigit(nextKey[0]);

            if (current->isMap()) {
                // Dacă e deja Map (structură), mergem la cheia următoare
                current = &std::get<vDataMap>(current->value)[key];
            }
            else if (current->isArray()) {
                // Dacă e Array, convertim cheia în index
                try {
                    size_t idx = std::stoll(key);
                    auto& vec = std::get<vDataArray>(current->value);
                    if (idx >= vec.size()) vec.resize(idx + 1);
                    current = &vec[idx];
                }
                catch (...) { return nullptr; }
            }
            else if (current->isNull()) {
                // AUTO-INITIALIZARE: Dacă e null, decidem ce devine în funcție de indexul curent
                // Dacă indexul e numeric, facem Array, altfel Map
                bool currentIsNumeric = !key.empty() && std::iswdigit(key[0]);

                if (currentIsNumeric) current->value = vDataArray{};
                else current->value = vDataMap{};

                // Re-executăm pasul după inițializare
                return navigateOrCreatePath(root, varExpr);
            }
            else {
                // Tip incompatibil (ex: încerci să pui punct după un INT)
                return nullptr;
            }
        }

        return current;
    }
    
    vData* vOliEngine::getOrCreateContainer(vData* root, const std::wstring& indexExpr, bool isNextBracketArray) {
        std::wstring cleanIdx = normalizeSpaces(indexExpr);

        // 1. Index GOL [] -> Adăugare la final
        if (cleanIdx.empty()) {
            if (!root->isArray()) root->value = vDataArray{};
            vDataArray& arr = std::get<vDataArray>(root->value);

            vData newValue;
            if (isNextBracketArray) newValue.value = vDataArray{};
            else newValue.value = vDataMap{};

            arr.push_back(std::move(newValue));
            return &arr.back();
        }

        vData indexValue = evaluateExpression(cleanIdx);

        // 2. Index Numeric -> ARRAY
        if (indexValue.isInt() || indexValue.isFloat()) {
            if (!root->isArray()) root->value = vDataArray{};
            vDataArray& arr = std::get<vDataArray>(root->value);

            size_t idx = (size_t)vDataToLong(indexValue);
            if (idx >= arr.size()) {
                arr.resize(idx + 1, { std::monostate{} });
            }

            vData& target = arr[idx];
            if (target.isNull()) {
                if (isNextBracketArray) target.value = vDataArray{};
                else target.value = vDataMap{};
            }
            return &target;
        }
        // 3. Index String -> MAP
        else {
            if (!root->isMap()) root->value = vDataMap{};
            vDataMap& m = std::get<vDataMap>(root->value);
            std::wstring key = vDataToWString(indexValue);

            // Folosim operatorul [] care creează elementul dacă nu există
            vData& target = m[key];
            if (target.isNull()) {
                if (isNextBracketArray) target.value = vDataArray{};
                else target.value = vDataMap{};
            }
            return &target;
        }
    }

    std::vector<std::wstring> vOliEngine::splitByCommaIgnoringBrackets(const std::wstring& content) {
        std::vector<std::wstring> result;
        std::wstring current;
        int bracketLevel = 0;
        int braceLevel = 0;
        bool inQuotes = false;

        for (size_t i = 0; i < content.length(); ++i) {
            wchar_t c = content[i];

            if (c == L'"' && (i == 0 || content[i - 1] != L'\\')) inQuotes = !inQuotes;

            if (!inQuotes) {
                if (c == L'[') bracketLevel++;
                else if (c == L']') bracketLevel--;
                else if (c == L'{') braceLevel++;
                else if (c == L'}') braceLevel--;
            }

            // Dacă găsim virgulă la nivelul 0 (nu în interiorul altor structuri), tăiem
            if (c == L',' && bracketLevel == 0 && braceLevel == 0 && !inQuotes) {
                result.push_back(normalizeSpaces(current));
                current.clear();
            }
            else {
                current += c;
            }
        }

        if (!current.empty()) {
            result.push_back(normalizeSpaces(current));
        }

        return result;
    }

    size_t vOliEngine::findClosingBracket(const std::wstring& str, size_t start) {
        int level = 0;
        bool inQuotes = false;
        for (size_t i = start; i < str.size(); ++i) {
            if (str[i] == L'"') inQuotes = !inQuotes;
            if (inQuotes) continue;

            if (str[i] == L'[') level++;
            else if (str[i] == L']') {
                level--;
                if (level == 0) return i;
            }
        }
        return std::wstring::npos;
    }


    void vOliEngine::initializeFunctionsHandlers() {
        // Înregistrăm funcția TYPE
        m_functionsHandlers[L"TYPE"] = [this](const std::vector<vData>& args) -> vData {
            if (args.empty()) return { L"NULL" };

            // args[0] este deja vData, nu mai facem evaluateExpression!
            return { getVariantTypeName(args[0]) };
        };

        // Funcția LEN
        m_functionsHandlers[L"LEN"] = [this](const std::vector<vData>& args) -> vData {
            if (args.empty()) return { 0LL };
            const vData& d = args[0];

            if (d.isArray()) return { static_cast<long long>(std::get<vDataArray>(d.value).size()) };
            if (d.isMap()) return { static_cast<long long>(std::get<vDataMap>(d.value).size()) };
            if (d.isString()) return { static_cast<long long>(std::get<std::wstring>(d.value).size()) };

            return { 0LL };
        };

        // Bonus: Funcția FACTORIAL (folosind vmath-ul tău)
        m_functionsHandlers[L"FACT"] = [this](const std::vector<vData>& args) -> vData {
            if (args.empty()) return { 0.0 };
            double num = vDataToDouble(args[0]);
            return { factorial(num) };
        };

        m_functionsHandlers[L"INPUT"] = [this](const std::vector<vData>& args) -> vData {
            return this->handleInputFunc(args);
        };

        m_functionsHandlers[L"RANDOM"] = [this](const std::vector<vData>& args) -> vData {
            return this->handleRandomFunc(args);
        };

        m_functionsHandlers[L"WAIT"] = [this](const std::vector<vData>& args) -> vData {
            return this->handleWaitFunc(args);
        };

        m_functionsHandlers[L"SYS"] = [this](const std::vector<vData>& args) -> vData {
            return this->handleSysFunc(args);
        };

        m_functionsHandlers[L"CONTAINS"] = [this](const std::vector<vData>& args) -> vData {
            return this->handleContainsFunc(args);
        };

        m_functionsHandlers[L"EVAL"] = [this](const std::vector<vData>& args) -> vData {
            return this->handleEvalFunc(args);
        };

        m_functionsHandlers[L"INT"] = [this](const std::vector<vData>& args) -> vData {
            return this->handleIntFunc(args);
        };
        

    }

    vData vOliEngine::executeAST(ASTPtr node) {
        if (!node) return { std::monostate{} };

        try {
            switch (node->type) {
            case ASTNodeType::Literal: {
                if (node->value == L"ARRAY_OBJECT") {
                    vDataArray elements;
                    for (auto& child : node->children) {
                        if (child) elements.push_back(executeAST(child));
                    }
                    return { elements };
                }
                if (node->value == L"MAP_OBJECT") {
                    vDataMap myMap;
                    for (size_t i = 0; i + 1 < node->children.size(); i += 2) {
                        vData keyData = executeAST(node->children[i]);
                        vData valData = executeAST(node->children[i + 1]);
                        myMap[vDataToWString(keyData)] = valData;
                    }
                    return { myMap };
                }

                std::wstring val = node->value;
                if (val == L"monostate" || val == L"NULL" || val == L"null") return { std::monostate{} };

                if (val.size() >= 2 && val.front() == L'"' && val.back() == L'"') {
                    return { val.substr(1, val.size() - 2) };
                }
                return parseRawLiteral(val);
            }

            case ASTNodeType::Variable: {
                return resolveVariable(node->value);
            }

            case ASTNodeType::FunctionCall: {
                std::wstring funcName;
                std::vector<vData> evaluatedArgs;

                // 1. Identificăm numele funcției (apel normal, dinamic sau prin variabilă)
                if (node->value == L"DYNAMIC_CALL" && !node->children.empty()) {
                    funcName = vDataToWString(executeAST(node->children[0]));
                    for (size_t i = 1; i < node->children.size(); ++i)
                        evaluatedArgs.push_back(executeAST(node->children[i]));
                }
                else if (!node->value.empty() && node->value[0] == L'$') {
                    funcName = vDataToWString(resolveVariable(node->value.substr(1)));
                    for (auto& child : node->children) evaluatedArgs.push_back(executeAST(child));
                }
                else {
                    funcName = node->value;
                    for (auto& child : node->children) evaluatedArgs.push_back(executeAST(child));
                }

                if (funcName.empty()) return { std::monostate{} };

                // 2. Verificăm dacă este un Constructor (Blueprint)
                auto itBlueprint = m_blueprints.find(funcName);
                if (itBlueprint != m_blueprints.end()) {
                    vDataMap instance;
                    instance[L"__type__"] = vData(itBlueprint->second.name);
                    const auto& fields = itBlueprint->second.fields;
                    for (size_t i = 0; i < fields.size(); ++i) {
                        instance[fields[i]] = (i < evaluatedArgs.size()) ? evaluatedArgs[i] : vData{ std::monostate{} };
                    }
                    return vData(instance);
                }

                // 3. Apelăm funcții interne sau definite de utilizator
                std::wstring upperName = funcName;
                std::transform(upperName.begin(), upperName.end(), upperName.begin(), ::towupper);

                auto itInternal = m_functionsHandlers.find(upperName);
                if (itInternal != m_functionsHandlers.end()) return itInternal->second(evaluatedArgs);

                auto itUser = m_userFunctions.find(upperName);
                if (itUser != m_userFunctions.end()) return callUserFunction(upperName, evaluatedArgs);

                LOG_ERROR(L"[RUNTIME ERROR] Unknown function: " + funcName);
                return { std::monostate{} };
            }

            case ASTNodeType::Operator: {
                // --- ATRIBUIRE (=) ---
                if (node->value == L"=") {
                    if (node->children.size() < 2) return { std::monostate{} };

                    ASTPtr left = node->children[0];
                    vData rightVal = executeAST(node->children[1]);

                    // 1. Atribuire simplă: $v = val
                    if (left->type == ASTNodeType::Variable) {
                        setVariable(left->value, rightVal);
                        return rightVal;
                    }

                    // 2. Atribuire complexă (Deep Access): $obj.prop = val sau $arr[idx] = val
                    if (left->value == L"DOT" || left->value == L"." || left->value == L"INDEX" || left->value == L"[") {
                        // Reconstruim drumul complet (ex: "$memo[$n]") pentru a-l trimite la assignToVariable
                        // assignToVariable va naviga prin pointeri direct la adresa din memoria globală
                        std::wstring fullPath = reconstructPath(left);

                        if (!fullPath.empty()) {
                            assignToVariable(fullPath, rightVal);
                            return rightVal;
                        }
                    }
                    throw std::runtime_error("L-value required for assignment.");
                }

                // --- ACCES (DOT / INDEX) - Citire (Rămâne neschimbat, aici e ok să returnăm copii) ---
                if (node->value == L"DOT" || node->value == L".") {
                    if (node->children.size() < 2) return { std::monostate{} };
                    vData container = executeAST(node->children[0]);
                    std::wstring field = node->children[1]->value;

                    if (container.isMap()) {
                        auto& m = std::get<vDataMap>(container.value);
                        if (m.count(field)) return m.at(field);
                    }
                    return { std::monostate{} };
                }

                if (node->value == L"INDEX" || node->value == L"[") {
                    if (node->children.size() < 2) return { std::monostate{} };
                    vData container = executeAST(node->children[0]);
                    vData index = executeAST(node->children[1]);
                    return accessContainer(container, index);
                }

                // --- OPERATORI BINARI ---
                if (node->children.size() >= 2) {
                    vData lhs = executeAST(node->children[0]);
                    vData rhs = executeAST(node->children[1]);
                    vData result = executeBinaryOperator(node->value, lhs, rhs);

                    if (result.isNull()) {
                        const std::wstring& op = node->value;
                        if (op == L"-" || op == L"*" || op == L"/" || op == L"^" || op == L"**" || op == L"%") {
                            LOG_ERROR(L"Operation '" + op + L"' failed. Math error or NULL operand.");
                            return { std::monostate{} };
                        }
                    }
                    return result;
                }

                // --- OPERATORI UNARI ---
                if (node->children.size() >= 1) {
                    vData operand = executeAST(node->children[0]);
                    if (node->value == L"UNARY_MINUS") return { -vDataToDouble(operand) };
                    if (node->value == L"NOT") return { !vDataToBool(operand) };
                }
                break;
            }
            default: break;
            }
        }
        catch (const std::exception& e) {
            LOG_ERROR(L"[CRITICAL EXECUTION ERROR] " + std::wstring(e.what(), e.what() + strlen(e.what())));
        }

        return { std::monostate{} };
    }

    std::wstring vOliEngine::reconstructPath(ASTPtr node) {
        if (!node) return L"";

        if (node->type == ASTNodeType::Variable) {
            return node->value; // Returnează "$memo"
        }

        if (node->value == L"DOT" || node->value == L".") {
            return reconstructPath(node->children[0]) + L"." + node->children[1]->value;
        }

        if (node->value == L"INDEX" || node->value == L"[") {
            vData idx = executeAST(node->children[1]); // Evaluăm indexul (ex: valoarea lui $n)
            std::wstring idxStr = vDataToWString(idx);

            // Dacă indexul este string, îl punem în ghilimele pentru siguranță, 
            // dacă e număr, îl lăsăm așa.
            if (idx.isString()) idxStr = L"\"" + idxStr + L"\"";

            return reconstructPath(node->children[0]) + L"[" + idxStr + L"]";
        }

        return L"";
    }


    void vOliEngine::updateContainerValue(ASTPtr containerNode, vData key, vData newValue) {
        if (containerNode->type != ASTNodeType::Variable) {
            throw std::runtime_error("L-value required.");
        }

        std::wstring varName = containerNode->value;
        if (!varName.empty() && varName[0] == L'$') varName = varName.substr(1);

        // 1. Găsim referința corectă (Unde se află containerul?)
        vData* pContainer = nullptr;

        // A. Căutăm în Frame-ul LOCAL curent
        if (!m_callStack.empty()) {
            auto& locals = m_callStack.back().localVariables;
            if (locals.count(varName)) {
                pContainer = &locals[varName];
            }
        }

        // B. Dacă nu e local, căutăm în GLOBAL
        if (!pContainer && m_globalVariables.count(varName)) {
            pContainer = &m_globalVariables[varName];
        }

        // C. Verificăm dacă am găsit ceva
        if (!pContainer) {
            LOG_ERROR(L"[RUNTIME ERROR] Variable '" + varName + L"' not found.");
            return;
        }

        // 2. Modificăm conținutul (pContainer este acum o referință directă către memorie)
        if (pContainer->isMap()) {
            auto& map = std::get<vDataMap>(pContainer->value);
            std::wstring k = vDataToWString(key);
            map[k] = newValue;
        }
        else if (pContainer->isArray()) {
            auto& arr = std::get<vDataArray>(pContainer->value);
            size_t idx = static_cast<size_t>(vDataToLong(key)); // Folosim Long pentru index

            if (idx < arr.size()) {
                arr[idx] = newValue;
            }
            else {
                // Opțional: Resize sau Error dacă indexul e out of bounds
                LOG_ERROR(L"[RUNTIME ERROR] Array index out of bounds: " + std::to_wstring(idx));
            }
        }
        else {
            LOG_ERROR(L"[RUNTIME ERROR] Target '" + varName + L"' is not a container (Array/Map).");
        }
    }


    bool vOliEngine::canBeNumeric(const vData& data) const {
        // 1. Verificăm dacă este deja numeric (Int sau Float) sau Boolean
        if (data.isInt() || data.isFloat() || data.isBool()) {
            return true;
        }

        // 2. Dacă este String, verificăm dacă textul poate fi convertit
        if (data.isString()) {
            const std::wstring& s = std::get<std::wstring>(data.value);
            if (s.empty()) return false;

            // Trim pentru spații albe
            size_t first = s.find_first_not_of(L" \t");
            if (first == std::wstring::npos) return false;
            size_t last = s.find_last_not_of(L" \t");
            std::wstring trimmed = s.substr(first, (last - first + 1));

            wchar_t* end = nullptr;
            std::wcstod(trimmed.c_str(), &end);

            // Este numeric dacă am ajuns la sfârșitul string-ului fără caractere invalide
            return end != nullptr && *end == L'\0';
        }

        return false;
    }

    /*
    vData vOliEngine::executeBinaryOperator(const std::wstring& op, const vData& left, const vData& right) {

        // --- STRATUL 0: OPERATORI SPECIALI ---
        if (op == L"??") {
            return left.IsNull() ? right : left;
        }

        // --- STRATUL 1: EGALITATE (Trebuie să permită comparații cu moonstate/null) ---
        if (op == L"==") {
            if (left.IsNull() && right.IsNull()) return { true };
            if (left.IsNull() || right.IsNull()) return { false };

            // Dacă pot fi numere, comparăm numeric (ex: 5.0 == "5")
            if (canBeNumeric(left) && canBeNumeric(right)) {
                return { vDataToDouble(left) == vDataToDouble(right) };
            }
            // Altfel, comparăm ca string-uri
            return { vDataToWString(left) == vDataToWString(right) };
        }

        if (op == L"!=") {
            if (left.IsNull() && right.IsNull()) return { false };
            if (left.IsNull() || right.IsNull()) return { true };

            if (canBeNumeric(left) && canBeNumeric(right)) {
                return { vDataToDouble(left) != vDataToDouble(right) };
            }
            return { vDataToWString(left) != vDataToWString(right) };
        }

        // --- STRATUL 2: BARIERĂ PENTRU OPERANZI NULI ---
        // Orice altă operație matematică/logică dă eroare dacă un operand e moonstate.
        if (left.IsNull() || right.IsNull()) {
            return { L"Error: Operation '" + op + L"' failed. Operand is moonstate." };
        }

        // --- STRATUL 3: PRIORITATE MATEMATICĂ (Aici se rezolvă bucla infinită) ---
        // Dacă ambele părți "arată" a numere, forțăm matematica indiferent dacă sunt stocate ca string.
        if (canBeNumeric(left) && canBeNumeric(right)) {
            double valL = vDataToDouble(left);
            double valR = vDataToDouble(right);

            // Operatori de comparație numerică
            if (op == L"<")  return { valL < valR };
            if (op == L">")  return { valL > valR };
            if (op == L"<=") return { valL <= valR };
            if (op == L">=") return { valL >= valR };

            // Operatori aritmetici
            if (op == L"+")  return { valL + valR };
            if (op == L"-")  return { valL - valR };
            if (op == L"*")  return { valL * valR };
            if (op == L"/") {
                if (valR == 0) return { L"Error: Division by zero" };
                return { valL / valR };
            }
            if (op == L"^" || op == L"**") return { pow(valL, valR) };
            if (op == L"%") {
                if ((long long)valR == 0) return { L"Error: Modulo by zero" };
                return { (long long)valL % (long long)valR };
            }
        }

        // --- STRATUL 4: OPERATORI LOGICI (&&, ||) ---
        if (op == L"&&") return { vDataToBool(left) && vDataToBool(right) };
        if (op == L"||") return { vDataToBool(left) || vDataToBool(right) };

        // --- STRATUL 5: OPERAȚII TEXTUALE (Fallback) ---
        // Dacă am ajuns aici, înseamnă că cel puțin unul nu e număr.
        if (left.isString() || right.isString()) {
            std::wstring sL = vDataToWString(left);
            std::wstring sR = vDataToWString(right);

            if (op == L"+")  return { sL + sR }; // Concatenare text

            // Comparații alfabetice (doar dacă nu sunt numere)
            if (op == L"<")  return { sL < sR };
            if (op == L">")  return { sL > sR };
            if (op == L"<=") return { sL <= sR };
            if (op == L">=") return { sL >= sR };
        }

        return { L"Error: Unsupported operation '" + op + L"' for these types." };
    }
    */

    /*
    vData vOliEngine::executeBinaryOperator(const std::wstring& op, const vData& left, const vData& right) {

        // --- STRATUL 0: OPERATORI SPECIALI ---
        if (op == L"??") {
            return left.IsNull() ? right : left;
        }

        // --- STRATUL 1: EGALITATE ȘI COMPARAȚIE DE BAZĂ ---
        // Permitem NULL == NULL sau NULL != 5 fără să declanșăm bariera de eroare.
        if (op == L"==") {
            if (left.IsNull() && right.IsNull()) return { true };
            if (left.IsNull() || right.IsNull()) return { false };

            if (canBeNumeric(left) && canBeNumeric(right)) {
                return { std::abs(vDataToDouble(left) - vDataToDouble(right)) < 1e-9 };
            }
            return { vDataToWString(left) == vDataToWString(right) };
        }

        if (op == L"!=") {
            if (left.IsNull() && right.IsNull()) return { false };
            if (left.IsNull() || right.IsNull()) return { true };

            if (canBeNumeric(left) && canBeNumeric(right)) {
                return { std::abs(vDataToDouble(left) - vDataToDouble(right)) >= 1e-9 };
            }
            return { vDataToWString(left) != vDataToWString(right) };
        }

        // --- STRATUL 2: CONCATENARE TOLERANTĂ (Fix-ul pentru echo/text) ---
        // Dacă folosim "+" și avem cel puțin un String sau un NULL, forțăm concatenare.
        if (op == L"+") {
            // 1. Verificăm dacă ambii pot fi tratați ca numere (sau sunt NULL/Zero)
            bool leftIsNum = canBeNumeric(left) || left.IsNull();
            bool rightIsNum = canBeNumeric(right) || right.IsNull();

            if (leftIsNum && rightIsNum) {
                // PRIORITATE MATEMATICĂ
                double valL = left.IsNull() ? 0.0 : vDataToDouble(left);
                double valR = right.IsNull() ? 0.0 : vDataToDouble(right);
                return { valL + valR };
            }

            // 2. Doar dacă unul este clar un STRING (și celălalt nu e numeric), concatenăm
            return { vDataToWString(left) + vDataToWString(right) };
        }

        // --- STRATUL 3: BARIERĂ PENTRU OPERANZI NULI ---
        // Orice altă operație (matematică, logică, comparație de mărime) eșuează dacă un operand e NULL.
        if (left.IsNull() || right.IsNull()) {
            return { L"Error: Operation '" + op + L"' failed. Operand is NULL (monostate)." };
        }

        // --- STRATUL 4: PRIORITATE MATEMATICĂ ---
        if (canBeNumeric(left) && canBeNumeric(right)) {
            double valL = vDataToDouble(left);
            double valR = vDataToDouble(right);

            // Comparații numerice
            if (op == L"<")  return { valL < valR };
            if (op == L">")  return { valL > valR };
            if (op == L"<=") return { valL <= valR };
            if (op == L">=") return { valL >= valR };

            // Aritmetică (Notă: "+" este deja gestionat pentru String/Null mai sus, aici rămâne numeric)
            if (op == L"+")  return { valL + valR };
            if (op == L"-")  return { valL - valR };
            if (op == L"*")  return { valL * valR };
            if (op == L"/") {
                if (std::abs(valR) < 1e-12) return { L"Error: Division by zero" };
                return { valL / valR };
            }
            if (op == L"^" || op == L"**") return { std::pow(valL, valR) };
            if (op == L"%") {
                if (static_cast<long long>(valR) == 0) return { L"Error: Modulo by zero" };
                return { static_cast<long long>(valL) % static_cast<long long>(valR) };
            }
        }

        // --- STRATUL 5: OPERATORI LOGICI ---
        if (op == L"&&") return { vDataToBool(left) && vDataToBool(right) };
        if (op == L"||") return { vDataToBool(left) || vDataToBool(right) };

        // --- STRATUL 6: OPERAȚII TEXTUALE (Fallback final) ---
        if (left.isString() || right.isString()) {
            std::wstring sL = vDataToWString(left);
            std::wstring sR = vDataToWString(right);

            if (op == L"<")  return { sL < sR };
            if (op == L">")  return { sL > sR };
            if (op == L"<=") return { sL <= sR };
            if (op == L">=") return { sL >= sR };
        }

        return { L"Error: Unsupported operation '" + op + L"' for these types." };
    }
    */

vData vOliEngine::executeBinaryOperator(const std::wstring& op, const vData& left, const vData& right) {
    // --- 1. OPERATORI DE COALESCENCE (Trebuie să ruleze înainte de verificarea de NULL) ---
    if (op == L"??") {
        return left.isNull() ? right : left;
    }

    // --- 2. LOGICĂ ȘI EGALITATE (Suportă operanzi NULL) ---
    if (op == L"==") {
        if (left.isNull() && right.isNull()) return { true };
        if (left.isNull() || right.isNull()) return { false };
        if (canBeNumeric(left) && canBeNumeric(right)) {
            return { std::abs(vDataToDouble(left) - vDataToDouble(right)) < 1e-9 };
        }
        return { vDataToWString(left) == vDataToWString(right) };
    }

    if (op == L"!=") {
        vData res = executeBinaryOperator(L"==", left, right);
        return vData(!vDataToBool(res));
    }

    // --- 3. ADUNAREA / CONCATENAREA (Tratăm NULL ca 0 sau "") ---
    if (op == L"+") {
        // 1. Dacă oricare este String, forțăm CONCATENARE
        if (left.isString() || right.isString()) {
            return { vDataToWString(left) + vDataToWString(right) };
        }

        // 2. Dacă ambele sunt Int, păstrăm precizia de Long Long
        if (left.isInt() && right.isInt()) {
            return { std::get<long long>(left.value) + std::get<long long>(right.value) };
        }

        // 3. Fallback numeric (Double) pentru Float sau NULL
        if (canBeNumeric(left) || left.isNull() || canBeNumeric(right) || right.isNull()) {
            double valL = left.isNull() ? 0.0 : vDataToDouble(left);
            double valR = right.isNull() ? 0.0 : vDataToDouble(right);
            return { valL + valR };
        }

        // 4. Ultima instanță (ex: obiecte, array-uri transformate în string)
        return { vDataToWString(left) + vDataToWString(right) };
    }

    // --- 4. BARIERĂ PENTRU OPERAȚII STRICTE ---
    // Dacă am ajuns aici și unul e NULL, restul operațiilor (^, *, /, -) nu pot continua.
    if (left.isNull() || right.isNull()) {
        return vData(); // Returnăm NULL pur (monostate)
    }

    // --- 5. OPERAȚII NUMERICE ---
    if (canBeNumeric(left) && canBeNumeric(right)) {

        // Cazul specific pentru PUTERE (Întotdeauna Double pentru a suporta radicali)
        if (op == L"^" || op == L"**") {
            return { std::pow(vDataToDouble(left), vDataToDouble(right)) };
        }

        // Ramura de Integers (Păstrare precizie)
        if (left.isInt() && right.isInt()) {
            long long iL = std::get<long long>(left.value);
            long long iR = std::get<long long>(right.value);

            if (op == L"-") return { iL - iR };
            if (op == L"*") return { iL * iR };
            if (op == L"%") return iR != 0 ? vData(iL % iR) : vData();
            if (op == L"/") {
                if (iR == 0) return vData();
                return (iL % iR == 0) ? vData(iL / iR) : vData((double)iL / (double)iR);
            }
        }

        // Ramura de Floating Point (Fallback)
        double dL = vDataToDouble(left);
        double dR = vDataToDouble(right);
        if (op == L"-") return { dL - dR };
        if (op == L"*") return { dL * dR };
        if (op == L"/") return std::abs(dR) > 1e-12 ? vData(dL / dR) : vData();

        // Comparații numerice
        if (op == L"<")  return { dL < dR };
        if (op == L">")  return { dL > dR };
        if (op == L"<=") return { dL <= dR };
        if (op == L">=") return { dL >= dR };
    }

    // --- 6. OPERATORI LOGICI ---
    if (op == L"&&") return { vDataToBool(left) && vDataToBool(right) };
    if (op == L"||") return { vDataToBool(left) || vDataToBool(right) };

    // --- 7. STRING COMPARISON ---
    if (left.isString() || right.isString()) {
        std::wstring sL = vDataToWString(left);
        std::wstring sR = vDataToWString(right);
        if (op == L"<")  return { sL < sR };
        if (op == L">")  return { sL > sR };
    }

    return vData(); // Fallback: operație nesuportată returnează NULL
}

    double vOliEngine::vDataToDouble(const vData& data) const {
        if (data.isFloat()) return std::get<double>(data.value);
        if (data.isInt())   return static_cast<double>(std::get<long long>(data.value));
        if (data.isBool())  return std::get<bool>(data.value) ? 1.0 : 0.0;

        if (data.isString()) {
            try {
                return std::stod(std::get<std::wstring>(data.value));
            }
            catch (...) {
                return 0.0;
            }
        }
        return 0.0;
    }

    /*
    vData vOliEngine::parseRawLiteral(const std::wstring& val) {
        // 1. Convertim într-o variabilă locală low pentru verificare
        std::wstring lowVal = val;
        for (auto& c : lowVal) c = std::towlower(c);

        // 2. Verificăm starea specială (Null/Monostate)
        if (lowVal == L"monostate" || lowVal == L"null" || lowVal == L"none") {
            return { std::monostate{} };
        }

        // 3. Verificăm dacă e boolean
        if (lowVal == L"true")  return { true };
        if (lowVal == L"false") return { false };

        // 4. Verificăm dacă e un număr (Integer sau Float)
        try {
            if (val.find(L'.') != std::wstring::npos) {
                return { std::stod(val) };
            }
            else {
                // Verificăm dacă șirul conține doar cifre (și eventual semnul -) 
                // pentru a evita ca std::stoll să arunce excepții pe string-uri arbitrare
                return { std::stoll(val) };
            }
        }
        catch (...) {
            // Dacă nu e număr și nici bool, rămâne string brut
            return { val };
        }
    }
    */
    vData vOliEngine::parseRawLiteral(const std::wstring& val) {
        if (val.empty()) return { std::monostate{} };

        std::wstring lowVal = val;
        for (auto& c : lowVal) c = std::towlower(c);

        if (lowVal == L"null" || lowVal == L"none") return { std::monostate{} };
        if (lowVal == L"true")  return { true };
        if (lowVal == L"false") return { false };

        // Verificăm dacă e numeric
        wchar_t* endPtr = nullptr;
        const wchar_t* startPtr = val.c_str();

        if (val.find(L'.') != std::wstring::npos) {
            double d = std::wcstod(startPtr, &endPtr);
            if (endPtr != startPtr) return { d }; // Succes float
        }
        else {
            long long ll = std::wcstoll(startPtr, &endPtr, 10);
            if (endPtr != startPtr) return { ll }; // Succes int
        }

        // Dacă endPtr nu a avansat sau e string pur
        return { val };
    }

    vData vOliEngine::accessContainer(const vData& container, const vData& index) {
        // CAZUL 1: Containerul este un MAP
        if (container.isMap()) {
            std::wstring key = vDataToWString(index); // Convertim indexul la string (cheia map-ului)
            const auto& map = std::get<vDataMap>(container.value);

            auto it = map.find(key);
            if (it != map.end()) {
                return it->second;
            }
            //return { L"(Key '" + key + L"' not found)" };
            return { std::monostate{} }; // În loc de string cu mesaj
        }

        // CAZUL 2: Containerul este un ARRAY
        if (container.isArray()) {
            // Convertim indexul la un număr întreg
            long long idx = 0;
            if (index.isInt()) idx = std::get<long long>(index.value);
            else if (index.isFloat()) idx = static_cast<long long>(std::get<double>(index.value));
            else {
                //return { L"(Error: Array index must be a number)" };
                return { std::monostate{} }; // Index invalid -> monostate
            }

            const auto& arr = std::get<vDataArray>(container.value);
            if (idx >= 0 && idx < static_cast<long long>(arr.size())) {
                return arr[static_cast<size_t>(idx)];
            }
            //return { L"(Error: Index " + std::to_wstring(idx) + L" out of bounds)" };
            return { std::monostate{} }; // Index invalid -> monostate
        }

        // CAZUL 2.5: Containerul este un STRING (pentru a lua un caracter)
        if (container.isString()) {
            long long idx = vDataToLong(index); // Folosim utilitarul creat anterior
            const std::wstring& str = std::get<std::wstring>(container.value);

            if (idx >= 0 && idx < static_cast<long long>(str.size())) {
                return { std::wstring(1, str[static_cast<size_t>(idx)]) };
            }
            //return { L"(Error: String index out of bounds)" };
            return { std::monostate{} }; // Index invalid -> monostate
        }

        // CAZUL 3: Nu este un container
        //return { L"(Error: Type " + getVariantTypeName(container) + L" is not indexable)" };
        return { std::monostate{} }; // Index invalid -> monostate
    }

    long long vOliEngine::vDataToLong(const vData& data) {
        if (std::holds_alternative<long long>(data.value)) {
            return std::get<long long>(data.value);
        }
        if (std::holds_alternative<double>(data.value)) {
            return static_cast<long long>(std::get<double>(data.value));
        }
        if (std::holds_alternative<std::wstring>(data.value)) {
            try {
                return std::stoll(std::get<std::wstring>(data.value));
            }
            catch (...) {
                return 0;
            }
        }
        if (std::holds_alternative<bool>(data.value)) {
            return std::get<bool>(data.value) ? 1 : 0;
        }
        return 0;
    }
    
    /*
    void vOliEngine::handleUnsetCommand(const ShellCommand& sc) {
        if (sc.args.empty()) return;

        std::wstring fullPath = implode(sc.args, L"");
        if (!fullPath.empty() && fullPath[0] == L'$') fullPath.erase(0, 1);

        // 1. Caz special: Ștergere totală (Resetăm doar Globalul)
        if (fullPath == L"all") {
            m_globalVariables.clear();
            LOG_SUCCESS(L"Memory cleared. All global variables removed.");
            return;
        }

        auto path = parsePath(fullPath);

        // 2. Caz simplu: unset $x
        if (path.indexes.empty()) {
            bool found = false;

            // Încercăm să ștergem din Local (dacă suntem într-o funcție)
            if (!m_callStack.empty()) {
                if (m_callStack.back().localVariables.erase(path.rootName)) {
                    found = true;
                    LOG_SUCCESS((L"Local variable $" + path.rootName + L" removed.").c_str());
                }
            }

            // Dacă nu a fost locală, încercăm în Global
            if (!found) {
                if (m_globalVariables.erase(path.rootName)) {
                    found = true;
                    LOG_SUCCESS((L"Global variable $" + path.rootName + L" removed.").c_str());
                }
            }

            if (!found) {
                LOG_ERROR((L"Variable $" + path.rootName + L" not found.").c_str());
            }
            return;
        }

        // 3. Caz complex: unset $a[0][key]
        // Trebuie să ne asigurăm că resolveToParent știe să caute rootName în ierarhie
        vData* parent = resolveToParent(path.rootName, path.indexes);

        if (!parent) {
            LOG_ERROR((L"Could not resolve path: " + fullPath).c_str());
            return;
        }

        std::wstring lastKey = path.indexes.back();
        if (lastKey.size() >= 2 && lastKey.front() == L'\"' && lastKey.back() == L'\"') {
            lastKey = lastKey.substr(1, lastKey.size() - 2);
        }

        if (parent->isMap()) {
            auto& map = std::get<vDataMap>(parent->value);
            if (map.erase(lastKey)) {
                LOG_SUCCESS((L"Key '" + lastKey + L"' removed from Map.").c_str());
            }
            else {
                LOG_ERROR((L"Key '" + lastKey + L"' not found in Map.").c_str());
            }
        }
        else if (parent->isArray()) {
            auto& vec = std::get<vDataArray>(parent->value);
            try {
                size_t idx = std::stoll(lastKey); // stoll e mai sigur pentru indici mari
                if (idx < vec.size()) {
                    vec.erase(vec.begin() + idx);
                    LOG_SUCCESS((L"Index " + std::to_wstring(idx) + L" removed from Array.").c_str());
                }
                else {
                    LOG_ERROR(L"Index out of bounds.");
                }
            }
            catch (...) {
                LOG_ERROR(L"Invalid array index.");
            }
        }
    }
    */

    void vOliEngine::handleUnsetCommand(const ShellCommand& sc) {
        if (sc.args.empty()) return;

        std::wstring fullPath = implode(sc.args, L"");
        if (fullPath.empty()) return;

        // --- 1. DETECȚIE PREFIX ȘI MOD (Global vs Local) ---
        bool forceGlobal = (fullPath[0] == L'@');

        // Curățăm prefixul pentru procesare
        if (fullPath[0] == L'$' || fullPath[0] == L'@') {
            fullPath.erase(0, 1);
        }

        // --- 2. CAZ SPECIAL: RESETARE TOTALĂ ---
        if (fullPath == L"all") {
            m_globalVariables.clear();
            LOG_SUCCESS(L"Memory cleared. All global variables removed.");
            return;
        }

        auto path = parsePath(fullPath);

        // --- 3. CAZUL A: ȘTERGERE VARIABILĂ SIMPLĂ (ex: unset $x) ---
        if (path.indexes.empty()) {
            bool deleted = false;

            if (forceGlobal) {
                // @x forțează ștergerea din tabela globală
                if (m_globalVariables.erase(path.rootName)) {
                    LOG_SUCCESS((std::wstring(L"Global variable @") + path.rootName + L" removed.").c_str());
                    deleted = true;
                }
            }
            else {
                // Logica de Shadowing: Dacă suntem în funcție, acționăm DOAR pe local
                if (!m_callStack.empty()) {
                    if (m_callStack.back().localVariables.erase(path.rootName)) {
                        LOG_SUCCESS((std::wstring(L"Local variable $") + path.rootName + L" removed.").c_str());
                        deleted = true;
                    }
                }
                else {
                    // În CLI, unset $x șterge din global
                    if (m_globalVariables.erase(path.rootName)) {
                        LOG_SUCCESS((std::wstring(L"Global variable $") + path.rootName + L" removed.").c_str());
                        deleted = true;
                    }
                }
            }

            if (!deleted) {
                std::wstring prefix = forceGlobal ? L"@" : L"$";
                LOG_ERROR((std::wstring(L"Variable ") + prefix + path.rootName + L" not found in current scope.").c_str());
            }
            return;
        }

        // --- 4. CAZUL B: ȘTERGERE DIN CONTAINER (ex: unset $a[0]) ---
        // Notă: Asigură-te că resolveToParent primește forceGlobal
        vData* parent = resolveToParent(path.rootName, path.indexes, forceGlobal);

        if (!parent) {
            std::wstring prefix = forceGlobal ? L"@" : L"$";
            LOG_ERROR((std::wstring(L"Could not resolve path: ") + prefix + fullPath).c_str());
            return;
        }

        std::wstring lastKey = path.indexes.back();
        // Curățăm ghilimelele pentru cheile de tip Map ("cheie" -> cheie)
        if (lastKey.size() >= 2 && lastKey.front() == L'\"' && lastKey.back() == L'\"') {
            lastKey = lastKey.substr(1, lastKey.size() - 2);
        }

        if (parent->isMap()) {
            auto& map = std::get<vDataMap>(parent->value);
            if (map.erase(lastKey)) {
                LOG_SUCCESS((std::wstring(L"Key '") + lastKey + L"' removed from Map.").c_str());
            }
            else {
                LOG_ERROR((std::wstring(L"Key '") + lastKey + L"' not found in Map.").c_str());
            }
        }
        else if (parent->isArray()) {
            auto& vec = std::get<vDataArray>(parent->value);
            try {
                size_t idx = std::stoll(lastKey);
                if (idx < vec.size()) {
                    vec.erase(vec.begin() + idx);
                    LOG_SUCCESS((std::wstring(L"Index ") + std::to_wstring(idx) + L" removed from Array.").c_str());
                }
                else {
                    LOG_ERROR(L"Index out of bounds.");
                }
            }
            catch (...) {
                LOG_ERROR(L"Invalid array index for unset.");
            }
        }
        else {
            LOG_ERROR(L"Target is not a container (Array/Map).");
        }
    }
    /*
    vData* vOliEngine::resolveToParent(const std::wstring& rootName, const std::vector<std::wstring>& indexes) {
        std::wstring cleanRoot = rootName;
        if (!cleanRoot.empty() && cleanRoot[0] == L'$') cleanRoot = cleanRoot.substr(1);

        vData* current = nullptr;

        // 1. GĂSIREA RĂDĂCINII (Scoping Logic)

        // A. Căutăm în contextul LOCAL (dacă suntem într-o funcție)
        if (!m_callStack.empty()) {
            auto& locals = m_callStack.back().localVariables;
            auto it = locals.find(cleanRoot);
            if (it != locals.end()) {
                current = &(it->second);
            }
        }

        // B. Dacă nu a fost găsită local, căutăm în GLOBAL
        if (!current) {
            auto itGlobal = m_globalVariables.find(cleanRoot);
            if (itGlobal != m_globalVariables.end()) {
                current = &(itGlobal->second);
            }
        }

        // Dacă variabila nu există deloc, returnăm nullptr
        if (!current) return nullptr;

        // 2. NAVIGAREA PRIN INDEXURI (Deep Access)
        // Ne oprim înainte de ultimul index pentru a returna "părintele"
        for (size_t i = 0; i < indexes.size() - 1; ++i) {
            const std::wstring& idx = indexes[i];

            if (current->isMap()) {
                auto& map = std::get<vDataMap>(current->value);
                // Evaluăm indexul dacă este o expresie sau îl folosim ca atare
                if (map.count(idx)) {
                    current = &map[idx];
                }
                else return nullptr;
            }
            else if (current->isArray()) {
                auto& vec = std::get<vDataArray>(current->value);
                try {
                    // Notă: std::stoll este mai robust pentru indici
                    size_t nIdx = static_cast<size_t>(std::stoll(idx));
                    if (nIdx < vec.size()) {
                        current = &vec[nIdx];
                    }
                    else return nullptr;
                }
                catch (...) { return nullptr; }
            }
            else {
                return nullptr; // Nu este container (Array/Map)
            }
        }

        return current;
    }
    */
    vData* vOliEngine::resolveToParent(const std::wstring& rootName, const std::vector<std::wstring>& indexes, bool forceGlobal) {
        std::wstring cleanRoot = rootName;

        // Curățăm prefixele dacă au rămas (deși de obicei sunt curățate înainte de apel)
        if (!cleanRoot.empty() && (cleanRoot[0] == L'$' || cleanRoot[0] == L'@')) {
            cleanRoot = cleanRoot.substr(1);
        }
        cleanRoot = trim(cleanRoot);

        vData* current = nullptr;

        // --- 1. GĂSIREA RĂDĂCINII (Scoping Logic cu Shadowing) ---

        if (forceGlobal) {
            // Dacă am folosit @, ignorăm localul complet
            auto itGlobal = m_globalVariables.find(cleanRoot);
            if (itGlobal != m_globalVariables.end()) {
                current = &(itGlobal->second);
            }
        }
        else {
            // Căutăm întâi în contextul LOCAL (dacă suntem într-o funcție)
            if (!m_callStack.empty()) {
                auto& locals = m_callStack.back().localVariables;
                auto it = locals.find(cleanRoot);
                if (it != locals.end()) {
                    current = &(it->second);
                }
            }

            // Dacă nu a fost găsită local, căutăm în GLOBAL
            if (!current) {
                auto itGlobal = m_globalVariables.find(cleanRoot);
                if (itGlobal != m_globalVariables.end()) {
                    current = &(itGlobal->second);
                }
            }
        }

        // Dacă rădăcina nu există în scope-ul permis, ne oprim
        if (!current) return nullptr;

        // --- 2. NAVIGAREA PRIN INDEXURI (Deep Access) ---
        // Ne oprim înaintea ULTIMULUI index pentru a returna pointer către "părinte"
        for (size_t i = 0; i < indexes.size() - 1; ++i) {
            std::wstring idx = indexes[i];

            // Curățăm ghilimelele pentru chei de tip string ("key" -> key)
            if (idx.size() >= 2 && idx.front() == L'\"' && idx.back() == L'\"') {
                idx = idx.substr(1, idx.size() - 2);
            }

            if (current->isMap()) {
                auto& map = std::get<vDataMap>(current->value);
                if (map.count(idx)) {
                    current = &map[idx];
                }
                else return nullptr;
            }
            else if (current->isArray()) {
                auto& vec = std::get<vDataArray>(current->value);
                try {
                    // Dacă indexul este o expresie, ar trebui evaluat, 
                    // dar aici presupunem că parsePath a extras deja literali.
                    size_t nIdx = static_cast<size_t>(std::stoll(idx));
                    if (nIdx < vec.size()) {
                        current = &vec[nIdx];
                    }
                    else return nullptr;
                }
                catch (...) { return nullptr; }
            }
            else {
                return nullptr; // Nod intermediar care nu este container
            }
        }

        return current;
    }

    vData* vOliEngine::getContainerPointer(vData& container, const std::wstring& keyOrIdx) {
        // 1. Dacă e MAP
        if (container.isMap()) {
            auto& map = std::get<vDataMap>(container.value);
            if (map.count(keyOrIdx)) return &map[keyOrIdx]; // Returnăm adresa elementului
            return nullptr;
        }

        // 2. Dacă e ARRAY
        if (container.isArray()) {
            auto& arr = std::get<vDataArray>(container.value);
            try {
                size_t idx = std::stoul(keyOrIdx);
                if (idx < arr.size()) return &arr[idx];
            }
            catch (...) { return nullptr; }
        }

        return nullptr;
    }

    VarPath vOliEngine::parsePath(const std::wstring& raw) {
      
        VarPath vp;
        size_t firstBracket = raw.find(L'[');

        if (firstBracket == std::wstring::npos) {
            vp.rootName = raw;
            return vp;
        }

        vp.rootName = raw.substr(0, firstBracket);
        std::wstring remaining = raw.substr(firstBracket);

        // Extragem tot ce e între [ ]
        size_t i = 0;
        while (i < remaining.length()) {
            if (remaining[i] == L'[') {
                size_t start = i + 1;
                size_t end = remaining.find(L']', start);
                if (end != std::wstring::npos) {
                    std::wstring idx = remaining.substr(start, end - start);
                    // Curățăm ghilimelele dacă e cheie de Map (ex: "db" -> db)
                    if (!idx.empty() && (idx[0] == L'"' || idx[0] == L'\'')) {
                        idx = idx.substr(1, idx.length() - 2);
                    }
                    vp.indexes.push_back(idx);
                    i = end + 1;
                }
                else break;
            }
            else i++;
        }
        return vp;
    }

    size_t vOliEngine::findKeywordPos(const std::wstring& line, const std::wstring& keyword) {
        bool inQuotes = false;
        for (size_t i = 0; i < line.size(); ++i) {
            if (line[i] == L'"') inQuotes = !inQuotes;
            if (!inQuotes) {
                // Verificăm dacă la poziția i începe keyword-ul (ex: /THEN)
                if (startsWith(line.substr(i), keyword, true)) {
                    return i;
                }
            }
        }
        return std::wstring::npos;
    }

    void vOliEngine::handleIfCommand(const std::wstring& fullLine) {
        // 1. Găsim pozițiile delimitatorilor la nivelul de top (fără /)
        size_t posThen = findTopLevelIfKeyword(fullLine, L"THEN");
        size_t posElse = findTopLevelIfKeyword(fullLine, L"ELSE");
        size_t posEndif = findTopLevelIfKeyword(fullLine, L"ENDIF");

        if (posThen == std::wstring::npos || posEndif == std::wstring::npos) {
            LOG_ERROR(L"Eroare IF: Lipsesc cuvintele cheie THEN sau ENDIF.");
            return;
        }

        // 2. Extragem și evaluăm Condiția (între IF și THEN)
        // "IF " are lungime 3
        std::wstring conditionPart = fullLine.substr(2, posThen - 2);
        conditionPart = normalizeSpaces(conditionPart);

        vData result = evaluateExpression(conditionPart);

        // Propagare erori din expresie
        if (result.isString() && std::get<std::wstring>(result.value).find(L"Error:") == 0) {
            LOG_ERROR(std::get<std::wstring>(result.value));
            return;
        }

        bool isTrue = vDataToBool(result);

        // 3. Extragem blocul de cod care trebuie executat
        std::wstring commandToRun;
        if (isTrue) {
            // De la THEN + 4 până la ELSE sau ENDIF
            size_t start = posThen + 4;
            size_t end = (posElse != std::wstring::npos) ? posElse : posEndif;
            commandToRun = fullLine.substr(start, end - start);
        }
        else if (posElse != std::wstring::npos) {
            // De la ELSE + 4 până la ENDIF
            size_t start = posElse + 4;
            commandToRun = fullLine.substr(start, posEndif - start);
        }

        // 4. Execuție recursivă folosind preParse
        commandToRun = normalizeSpaces(commandToRun);
        if (!commandToRun.empty()) {
            std::vector<std::wstring> subInstructions = preParse(commandToRun);
            for (const auto& subInstr : subInstructions) {
                if (!subInstr.empty()) {
                    this->execute(subInstr);

                    // Oprire în caz de BREAK/CONTINUE/RETURN
                    if (m_executionStatus != OliStatus::RUNNING) return;
                }
            }
        }
    }

   

    size_t vOliEngine::findTopLevelIfKeyword(const std::wstring& line, const std::wstring& keyword) {
        int depth = 0;
        bool inQuotes = false;

        std::wstring upperLine = line;
        std::transform(upperLine.begin(), upperLine.end(), upperLine.begin(), ::towupper);
        std::wstring upperKey = keyword;
        std::transform(upperKey.begin(), upperKey.end(), upperKey.begin(), ::towupper);

        // 1. Găsim unde începe IF-ul curent
        size_t mainIfPos = upperLine.find(L"IF");
        if (mainIfPos == std::wstring::npos) return std::wstring::npos;

        // 2. Începem scanarea IMEDIAT DUPĂ "IF"
        for (size_t i = mainIfPos + 2; i < upperLine.size(); ++i) {
            if (upperLine[i] == L'"') { inQuotes = !inQuotes; continue; }
            if (inQuotes) continue;

            // Verificăm dacă suntem la început de cuvânt
            bool isStart = (i == 0 || iswspace(upperLine[i - 1]) || wcschr(L"=+-*<>|;()[]{},:%/", upperLine[i - 1]));
            if (!isStart) continue;

            std::wstring_view rem(&upperLine[i], upperLine.size() - i);

            // Dacă găsim keyword-ul (THEN/ELSE) la depth 0, e cel bun!
            if (depth == 0 && rem.starts_with(upperKey)) {
                size_t nextIdx = i + upperKey.length();
                if (nextIdx >= upperLine.size() || iswspace(upperLine[nextIdx]) || wcschr(L"=+-*<>|;()[]{},:%/", upperLine[nextIdx])) {
                    return i;
                }
            }

            // Gestionăm adâncimea pentru IF-uri imbricate
            if (rem.starts_with(L"IF")) {
                size_t next = i + 2;
                if (next >= upperLine.size() || iswspace(upperLine[next]) || upperLine[next] == L'(') {
                    depth++; // Intrăm într-un IF secundar
                    i += 1;
                }
            }
            else if (rem.starts_with(L"ENDIF")) {
                depth--; // Ieșim dintr-un IF secundar
                i += 4;
            }
        }
        return std::wstring::npos;
    }

    bool vOliEngine::vDataToBool(const vData& data) {
        // 1. Booleeni expliciți - TREBUIE să fie primii!
        if (data.isBool()) {
            return std::get<bool>(data.value);
        }

        // 2. Dacă este nulă (std::monostate) -> FALSE
        if (data.isNull()) return false;

        // 3. Numere întregi
        if (data.isInt()) {
            return std::get<long long>(data.value) != 0;
        }

        // 4. Numere cu virgulă
        if (data.isFloat()) {
            return std::abs(std::get<double>(data.value)) > 1e-9;
        }

        // 5. String-uri - ATENȚIE AICI
        if (data.isString()) {
            const std::wstring& s = std::get<std::wstring>(data.value);
            if (s.empty()) return false;

            // Conversie normalizată: doar "true" sau "1" sunt true, restul false
            // SAU păstrezi logica ta, dar asigură-te că nu mănâncă rezultatele booleene
            std::wstring lowerS = s;
            std::transform(lowerS.begin(), lowerS.end(), lowerS.begin(), ::towlower);
            return (lowerS == L"true" || lowerS == L"1");
        }

        // 6. Containere
        if (data.isArray()) return !std::get<vDataArray>(data.value).empty();
        if (data.isMap()) return !std::get<vDataMap>(data.value).empty();

        return false;
    }



    

    void vOliEngine::handleWhileCommand(const std::wstring& fullLine) {
        std::wstring upperLine = fullLine;
        std::transform(upperLine.begin(), upperLine.end(), upperLine.begin(), ::towupper);

        size_t whilePos = upperLine.find(L"WHILE");
        // Încercăm varianta smart
        size_t posDo = findTopLevelKeyword(fullLine, L"DO", L"WHILE");
        size_t posEnd = findTopLevelKeyword(fullLine, L"ENDWHILE", L"WHILE");

        // FALLBACK: Dacă findTopLevelKeyword a dat chix din cauza offset-urilor
        if (posDo == std::wstring::npos) posDo = upperLine.find(L" DO ");
        if (posEnd == std::wstring::npos) posEnd = upperLine.rfind(L"ENDWHILE");

        if (whilePos == std::wstring::npos || posDo == std::wstring::npos || posEnd == std::wstring::npos) {
            LOG_ERROR(L"Malformed WHILE: Structura incompleta (WHILE/DO/ENDWHILE).");
            // Loghează ce am primit pentru debug
            //LOG_DEBUG(L"Received: " + fullLine);
            return;
        }

        // Extragem condiția și corpul (restul logicii tale e corectă)
        size_t condStart = whilePos + 5;
        std::wstring conditionPart = trim(fullLine.substr(condStart, posDo - condStart));
        size_t bodyStart = posDo + 2; // +2 pentru "DO"
        std::wstring bodyCommand = fullLine.substr(bodyStart, posEnd - bodyStart);

        // IMPORTANT: Re-pasăm corpul prin preParse pentru a-l sparge în linii corecte
        std::vector<std::wstring> instructions = preParse(bodyCommand);

        int safetyBreak = 0;
        while (true) {
            if (++safetyBreak > 5000) break;

            vData condRes = evaluateExpression(conditionPart);
            if (!vDataToBool(condRes)) break;

            for (const auto& instr : instructions) {
                this->executeInternal(instr);
                if (m_executionStatus != OliStatus::RUNNING) {
                    // Gestionare BREAK/CONTINUE/RETURN...
                    if (m_executionStatus == OliStatus::CONTINUE_REQUESTED) {
                        m_executionStatus = OliStatus::RUNNING;
                        goto next_iteration;
                    }
                    if (m_executionStatus == OliStatus::BREAK_REQUESTED) {
                        m_executionStatus = OliStatus::RUNNING;
                        return;
                    }
                    if (m_executionStatus == OliStatus::RETURN_REQUESTED) return;
                }
            }
        next_iteration:;
        }
    }

    

    size_t vOliEngine::findTopLevelKeyword(const std::wstring& line, const std::wstring& keyword, const std::wstring& startCommand) {
        int depth = 0;
        bool inQuotes = false;

        std::wstring upperLine = line;
        std::transform(upperLine.begin(), upperLine.end(), upperLine.begin(), ::towupper);
        std::wstring upperKey = keyword;
        std::transform(upperKey.begin(), upperKey.end(), upperKey.begin(), ::towupper);
        std::wstring upperStart = startCommand;
        std::transform(upperStart.begin(), upperStart.end(), upperStart.begin(), ::towupper);

        size_t mainPos = upperLine.find(upperStart);
        if (mainPos == std::wstring::npos) return std::wstring::npos;

        // IMPORTANT: Începem căutarea imediat după cuvântul de start
        size_t searchStart = mainPos + upperStart.length();

        for (size_t i = searchStart; i < upperLine.size(); ++i) {
            // 1. Skip ghilimele
            if (upperLine[i] == L'"' && (i == 0 || upperLine[i - 1] != L'\\')) {
                inQuotes = !inQuotes;
                continue;
            }
            if (inQuotes) continue;

            // 2. Verificăm dacă suntem la începutul unui cuvânt
            bool isStartOfWord = (i == 0 || iswspace(upperLine[i - 1]) || wcschr(L"=+-*<>|;()[]{},:%/", upperLine[i - 1]));
            if (!isStartOfWord) continue;

            std::wstring_view rem(&upperLine[i], upperLine.size() - i);

            // --- A. PRIORITATE: Verificăm dacă am găsit chiar KEYWORD-ul căutat ---
            if (depth == 0 && rem.starts_with(upperKey)) {
                size_t nextIdx = i + upperKey.length();
                // Verificăm să fie cuvânt întreg (să nu fie "UNTILLY")
                if (nextIdx >= upperLine.size() || iswspace(upperLine[nextIdx]) || wcschr(L"=+-*<>|;()[]{},:%/", upperLine[nextIdx])) {
                    return i;
                }
            }

            // --- B. GESTIONARE ADÂNCIME (Pentru structuri imbricate) ---
            // Incrementare: Dacă găsim un alt bloc de același tip sau diferit
            static const std::vector<std::pair<std::wstring, int>> startTokens = {
                {L"REPEAT", 6}, {L"WHILE", 5}, {L"FOR", 3}, {L"IF", 2}
            };

            for (const auto& token : startTokens) {
                if (rem.starts_with(token.first)) {
                    size_t nextIdx = i + token.second;
                    if (nextIdx >= upperLine.size() || iswspace(upperLine[nextIdx]) || upperLine[nextIdx] == L'(') {
                        depth++;
                        i = nextIdx - 1;
                        goto next_iter;
                    }
                }
            }

            // Decrementare: Orice formă de închidere
            if (rem.starts_with(L"END") || rem.starts_with(L"UNTIL")) {
                // Dacă am găsit UNTIL și îl căutam ca keyword principal, a fost deja prins la punctul A.
                // Dacă NU îl căutam pe el (e un UNTIL al unui REPEAT interior), atunci scadem depth.
                if (depth > 0) depth--;

                // Sărim peste cuvânt
                while (i < upperLine.size() && iswalnum(upperLine[i])) i++;
                i--;
            }

        next_iter:;
        }

        return std::wstring::npos;
    }


    void vOliEngine::handleRunCommand(const ShellCommand& sc) {
        if (sc.args.empty()) {
            LOG_ERROR(L"Usage: run \"path/to/script.oli\"");
            return;
        }

        std::wstring pathStr = sc.args[0];
        // Eliminăm ghilimelele dacă au supraviețuit parserului
        if (pathStr.size() >= 2 && pathStr.front() == L'"' && pathStr.back() == L'"') {
            pathStr = pathStr.substr(1, pathStr.size() - 2);
        }

        // Convertim wstring (calea) în string pentru std::ifstream pe Windows
        // Sau folosim _wfopen dacă vrei suport pentru căi cu caractere Unicode speciale
        std::ifstream file(pathStr);
        if (!file.is_open()) {
            LOG_ERROR(L"Could not open script: " + pathStr);
            return;
        }

        std::string lineA;
        bool firstLine = true;

        while (std::getline(file, lineA)) {
            // --- FIX pentru \r (Windows style line endings) ---
            if (!lineA.empty() && lineA.back() == '\r') {
                lineA.pop_back();
            }

            if (lineA.empty()) continue;

            // 1. Conversie UTF-8 -> UTF-16
            int size_needed = MultiByteToWideChar(CP_UTF8, 0, lineA.c_str(), (int)lineA.size(), NULL, 0);
            std::wstring lineW(size_needed, 0);
            MultiByteToWideChar(CP_UTF8, 0, lineA.c_str(), (int)lineA.size(), &lineW[0], size_needed);

            // 2. Gestionare BOM (Byte Order Mark)
            if (firstLine) {
                if (!lineW.empty() && lineW[0] == 0xFEFF) {
                    lineW.erase(0, 1);
                }
                firstLine = false;
            }

            // 3. Normalizare și Execuție
            std::wstring finalLine = trim(lineW); // Folosește trim pentru a fi sigur
            if (finalLine.empty() || finalLine[0] == L'#') continue;

            // LOG_INFO(L"Executing: " + finalLine); // Debug util pentru scripturi
            this->execute(finalLine);
        }
        file.close();
    }


    vData vOliEngine::handleInputFunc(const std::vector<vData>& args) {
        // 1. Afișăm prompt-ul (dacă există)
        if (!args.empty()) {
            std::wcout << vDataToWString(args[0]);
        }

        // 2. Citim linia de la utilizator
        std::wstring userInput;
        // Folosim getline pentru a permite spații în input
        if (!std::getline(std::wcin, userInput)) {
            return { L"" }; // Returnăm string gol în caz de EOF sau eroare
        }

        // 3. (Opțional) Putem încerca să detectăm dacă input-ul este număr
        // Pentru simplitate, momentan îl returnăm ca String. 
        // Utilizatorul poate face operații matematice oricum datorită vDataToDouble-ului tău.
        return { userInput };
    }

    vData vOliEngine::handleRandomFunc(const std::vector<vData>& args) {
        long long min = 0, max = 100;
        if (args.size() >= 2) {
            min = vDataToLong(args[0]);
            max = vDataToLong(args[1]);
        }

        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<long long> dis(min, max);

        return { dis(gen) };
    }

    vData vOliEngine::handleWaitFunc(const std::vector<vData>& args) {
        if (!args.empty()) {
            long long ms = vDataToLong(args[0]);
            std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        }
        return { std::monostate{} };
    }

    vData vOliEngine::handleSysFunc(const std::vector<vData>& args) {
        if (args.empty()) return { L"" };
        
        // 1. Pregătim comanda (args[0] este string-ul cu comanda)
        std::wstring command = vDataToWString(args[0]);

        // Curățăm ghilimelele dacă e un string literal
        if (command.size() >= 2 && command.front() == L'"' && command.back() == L'"') {
            command = command.substr(1, command.size() - 2);
        }

        // 2. Executăm și capturăm
        std::wstring output;
        FILE* pipe = _wpopen(command.c_str(), L"r");
        if (!pipe) return { L"ERROR" };

        wchar_t buffer[128];
        while (fgetws(buffer, 128, pipe)) {
            output += buffer;
        }

        _pclose(pipe);

        // 3. Returnăm rezultatul ca STRING în Oli
        //return vData(L"\"" + output + L"\"");
        return vData( output );
    }


    void vOliEngine::handleSysCommand(const ShellCommand& sc) {
        if (sc.args.empty()) {
            LOG_ERROR(L"Usage: /sys <system_command>");
            return;
        }

        std::wstring fullCommand;
        for (const auto& arg : sc.args) fullCommand += arg + L" ";
        if (!fullCommand.empty()) fullCommand.pop_back();

        // --- MODIFICARE AICI ---
        // Nu mai trimitem m_variables. 
        // Funcția substituteVariables trebuie să apeleze intern resolveVariable(name)
        fullCommand = substituteVariables(fullCommand);

        if (fullCommand.size() >= 2 && fullCommand.front() == L'"' && fullCommand.back() == L'"') {
            fullCommand = fullCommand.substr(1, fullCommand.size() - 2);
        }

        LOG_INFO(L"Executing: " + fullCommand);

        std::wcout.flush();
        fflush(stdout);

        FILE* pipe = _wpopen(fullCommand.c_str(), L"r");
        if (!pipe) {
            LOG_ERROR(L"Could not execute system command.");
            return;
        }

        wchar_t buffer[128];
        while (fgetws(buffer, 128, pipe)) {
            std::wcout << buffer;
            std::wcout.flush();
        }

        int returnCode = _pclose(pipe);

        if (returnCode != 0) {
            LOG_ERROR(L"Command failed with code: " + std::to_wstring(returnCode));
        }
        else {
            LOG_SUCCESS(L"Command finished.");
        }
    }

    std::wstring vOliEngine::substituteVariables(const std::wstring& input) {
        std::wstring result = input;
        size_t pos = 0;

        while ((pos = result.find(L'$', pos)) != std::wstring::npos) {
            // 1. Verificăm dacă avem sintaxa cu acolade: ${var}
            bool hasBraces = (pos + 1 < result.length() && result[pos + 1] == L'{');
            size_t startName = pos + (hasBraces ? 2 : 1);
            size_t endName = std::wstring::npos;

            if (hasBraces) {
                size_t closeBrace = result.find(L'}', startName);
                if (closeBrace != std::wstring::npos) {
                    endName = closeBrace;
                }
            }
            else {
                endName = startName;
                // Un nume de variabilă valid: alfanumeric sau underscore
                while (endName < result.length() && (iswalnum(result[endName]) || result[endName] == L'_')) {
                    endName++;
                }
            }

            // 2. Extragem numele și căutăm valoarea
            if (endName != std::wstring::npos && endName > startName) {
                std::wstring varName = result.substr(startName, endName - startName);

                // APELĂM LOGICA NOUĂ: resolveVariable caută în Local apoi în Global
                vData data = resolveVariable(varName);

                if (!data.isNull()) {
                    std::wstring varValue = vDataToWString(data);

                    // Curățăm ghilimelele pentru a putea folosi variabila în căi de fișiere sau comenzi shell
                    if (varValue.size() >= 2 && varValue.front() == L'"' && varValue.back() == L'"') {
                        varValue = varValue.substr(1, varValue.size() - 2);
                    }

                    size_t totalLenToReplace = hasBraces ? (endName - pos + 1) : (endName - pos);
                    result.replace(pos, totalLenToReplace, varValue);

                    // Avansăm cursorul după valoarea inserată
                    pos += varValue.length();
                    continue;
                }
            }

            // Dacă nu am găsit variabila, mergem mai departe (lăsăm $ în text sau trecem peste el)
            pos++;
        }
        return result;
    }

    vData vOliEngine::handleContainsFunc(const std::vector<vData>& args) {
        // Avem nevoie de exact 2 argumente: substring și sursă
        if (args.size() < 2) {
            LOG_ERROR(L"CONTAINS requires 2 arguments: (substring, source_text)");
            return vData(false);
        }

        // 1. Extragem datele folosind vDataToWString pentru a suporta orice tip (chiar și INT convertit)
        std::wstring toFind = vDataToWString(args[0]);
        std::wstring source = vDataToWString(args[1]);

        // 2. Funcție lambda rapidă pentru a elimina ghilimelele de la exterior ("text" -> text)
        auto stripQuotes = [](std::wstring& s) {
            if (s.size() >= 2 && s.front() == L'"' && s.back() == L'"') {
                s = s.substr(1, s.size() - 2);
            }
        };

        stripQuotes(toFind);
        stripQuotes(source);

        // 3. Executăm căutarea
        // std::wstring::npos înseamnă că substring-ul NU a fost găsit
        bool found = (source.find(toFind) != std::wstring::npos);

        //LOG_DEBUG(L"CONTAINS: Checking if '" + toFind + L"' is in source... " + (found ? L"Found" : L"Not Found"));

        return vData(found);
    }

    /*
    void vOliEngine::handleProcCommand(const ShellCommand& sc) {
        if (sc.args.empty()) {
            LOG_ERROR(L"Usage: /proc name [param1, param2...]");
            return;
        }

        // 1. Normalizăm numele: dacă userul scrie "test", noi salvăm "/test"
        std::wstring procName = sc.args[0];
        
        if (procName[0] != L'/') {
            procName = L"/" + procName;
        }
        
        if (vOliKeyWords::isInternalFixedCommand(procName)) {
            LOG_ERROR(L"Cannot shadow INTERNAL system command: " + procName);
            return;
        }

        // 2. Dacă procedura există deja, opțional putem da un warning
        if (m_procedures.count(procName)) {
            LOG_INFO(L"Overwriting existing procedure: " + procName);
        }
       
        m_activeProcName = procName;

        Procedure newProc;
        newProc.name = m_activeProcName;
        newProc.body.clear(); // Ne asigurăm că e gol corpul la început

        // 2. Extragem parametrii
        // Mergem prin toate argumentele de după nume
        for (size_t i = 1; i < sc.args.size(); ++i) {
            std::wstring arg = sc.args[i];

            // Eliminăm caracterele care țin de sintaxă: [ ] ,
            arg.erase(std::remove_if(arg.begin(), arg.end(), [](wchar_t c) {
                return c == L'[' || c == L']' || c == L',';
                }), arg.end());

            if (!arg.empty()) {
                newProc.params.push_back(arg);
            }
        }

        // 3. Salvăm procedura în map și activăm modul recording
        m_procedures[m_activeProcName] = newProc;
        m_isRecording = true;

        LOG_INFO(L"Started recording procedure: " + m_activeProcName);
    }
    */
    void vOliEngine::handleProcCommand(const ShellCommand& sc) {
        if (sc.args.empty()) {
            LOG_ERROR(L"Usage: proc name [param1, param2...]");
            return;
        }

        // 1. Extragere și curățare nume procedură
        // Eliminăm eventuale virgule lipite de nume, ex: "proc test,"
        std::wstring procName = sc.args[0];
        procName.erase(std::remove_if(procName.begin(), procName.end(), [](wchar_t c) {
            return c == L',' || c == L'(' || c == L')';
            }), procName.end());

        // Verificăm shadowing pentru comenzi interne
        std::wstring upperName = procName;
        std::transform(upperName.begin(), upperName.end(), upperName.begin(), ::towupper);

        if (vOliKeyWords::isInternalFixedCommand(upperName)) {
            LOG_ERROR(L"Cannot shadow INTERNAL system command: " + procName);
            return;
        }

        if (m_procedures.count(procName)) {
            LOG_INFO(L"Overwriting existing procedure: " + procName);
        }

        // Setează contextul activ
        m_activeProcName = procName;

        Procedure newProc;
        newProc.name = m_activeProcName;
        // .clear() nu e strict necesar la un obiect nou, dar e bine pentru siguranță
        newProc.params.clear();
        newProc.body.clear();

        // 2. Extragem parametrii cu filtrare strictă
        for (size_t i = 1; i < sc.args.size(); ++i) {
            std::wstring arg = sc.args[i];

            // Eliminăm caracterele de control/separatori din numele parametrului
            arg.erase(std::remove_if(arg.begin(), arg.end(), [](wchar_t c) {
                return c == L'[' || c == L']' || c == L',' || c == L'(' || c == L')';
                }), arg.end());

            // Adăugăm în listă DOAR dacă a mai rămas ceva din string
            if (!arg.empty()) {
                newProc.params.push_back(arg);
            }
        }

        // 3. Activăm starea de înregistrare
        m_procedures[m_activeProcName] = newProc;
        m_isRecording = true;
        m_isRecordingFunc = false; // Ne asigurăm că nu se bat cap în cap

        LOG_INFO(L"Started recording procedure: " + m_activeProcName +
            L" with " + std::to_wstring(newProc.params.size()) + L" parameters.");
    }

    // Returnează true dacă trebuie să dăm BREAK la bucla principală C++
    bool vOliEngine::executeCycleStep(const std::wstring& iterName, const vData& value, const std::vector<std::wstring>& instrs) {
        // 1. Salvăm iteratorul folosind logica de scoping (va merge în LocalVariables dacă suntem în funcție)
        setVariable(iterName, value);

        for (const auto& instr : instrs) {
            if (instr.empty()) continue;

            // Executăm instrucțiunea
            this->execute(instr);

            // 2. Verificăm semnalele de control (BREAK / CONTINUE / RETURN)

            // Dacă s-a cerut RETURN dintr-o funcție, trebuie să oprim și bucla!
            if (m_executionStatus == OliStatus::RETURN_REQUESTED) {
                return true; // Oprim iterația
            }

            // Verificăm BREAK
            if (m_executionStatus == OliStatus::BREAK_REQUESTED) {
                m_executionStatus = OliStatus::RUNNING; // Resetăm starea pentru motor
                return true; // Oprim execuția buclei (true = stop cycle)
            }

            // Verificăm CONTINUE
            if (m_executionStatus == OliStatus::CONTINUE_REQUESTED) {
                m_executionStatus = OliStatus::RUNNING; // Resetăm starea
                return false; // Sărim peste restul instrucțiunilor, dar continuăm următorul pas (false = don't stop)
            }
        }
        return false;
    }


    std::wstring vOliEngine::cleanVariableName(const std::wstring& name) {
        if (name.empty()) return L"";
        std::wstring cleaned = name;

        // Eliminăm prefixul '$' dacă există
        if (cleaned[0] == L'$') {
            cleaned.erase(0, 1);
        }

        // Eliminăm eventualele spații accidentale
        return trim(cleaned);
    }

    void vOliEngine::handleCycleCommand(const std::wstring& fullLine) {
        std::wstring upperLine = fullLine;
        std::transform(upperLine.begin(), upperLine.end(), upperLine.begin(), ::towupper);

        // 1. Găsire Keyword-uri
        size_t cyclePos = upperLine.find(L"CYCLE");
        size_t posDo = findTopLevelKeyword(fullLine, L"DO", L"CYCLE");
        size_t posEnd = findTopLevelKeyword(fullLine, L"ENDCYCLE", L"CYCLE");

        if (posDo == std::wstring::npos || posEnd == std::wstring::npos) {
            LOG_ERROR(L"Malformed CYCLE: Missing DO or ENDCYCLE");
            return;
        }

        // 2. Extragere Header (Sursă AS Iterator)
        size_t headerStart = cyclePos + 5;
        std::wstring header = trim(fullLine.substr(headerStart, posDo - headerStart));

        std::wstring upperHeader = header;
        std::transform(upperHeader.begin(), upperHeader.end(), upperHeader.begin(), ::towupper);
        size_t asPos = upperHeader.find(L"AS");

        if (asPos == std::wstring::npos) {
            LOG_ERROR(L"CYCLE requires 'as'. Ex: CYCLE $list AS $item");
            return;
        }

        std::wstring sourceExpr = trim(header.substr(0, asPos));
        std::wstring iteratorName = trim(header.substr(asPos + 2));

        // 3. Evaluare Sursă
        // evaluateExpression va folosi noul resolveVariable (Local -> Global)
        vData sourceData = evaluateExpression(sourceExpr);

        // 4. Pregătire Instrucțiuni
        size_t bodyStart = posDo + 2;
        std::wstring bodyCommand = trim(fullLine.substr(bodyStart, posEnd - bodyStart));
        std::vector<std::wstring> instructions = preParse(bodyCommand);

        // 5. Shadowing Protection (Modernizat)
        // Căutăm variabila veche folosind ierarhia corectă
        vData oldVal = resolveVariable(iteratorName);
        bool existed = !oldVal.isNull();

        // 6. Execuția efectivă
        if (sourceData.isArray()) {
            const auto& items = std::get<vDataArray>(sourceData.value);
            for (const auto& item : items) {
                // executeCycleStep folosește setVariable (care pune iteratorul în Frame-ul local)
                if (executeCycleStep(iteratorName, item, instructions)) break;
                if (m_executionStatus == OliStatus::RETURN_REQUESTED) break;
            }
        }
        else if (sourceData.isMap()) {
            const auto& mapItems = std::get<vDataMap>(sourceData.value);
            for (const auto& pair : mapItems) {
                if (executeCycleStep(iteratorName, vData{ pair.first }, instructions)) break;
                if (m_executionStatus == OliStatus::RETURN_REQUESTED) break;
            }
        }
        else {
            LOG_ERROR(L"Cycle error: Source is not an Array or Map.");
        }

        // 7. Restaurare (Clean-up)
        // Dacă am terminat ciclul, curățăm iteratorul sau punem valoarea veche înapoi
        if (existed) {
            setVariable(iteratorName, oldVal);
        }
        else {
            // Dacă nu exista înainte, o scoatem din contextul curent
            if (!m_callStack.empty()) {
                m_callStack.back().localVariables.erase(cleanVariableName(iteratorName));
            }
            else {
                m_globalVariables.erase(cleanVariableName(iteratorName));
            }
        }
    }

    /*
    void vOliEngine::callProcedure(const Procedure& proc, const std::vector<std::wstring>& passedArgs) {
        // 1. Snapshot doar pentru ce vrem să protejăm (Parametrii)
        std::map<std::wstring, vData> protectedVariablesBackup;

        for (const auto& paramName : proc.params) {
            if (m_variables.count(paramName)) {
                protectedVariablesBackup[paramName] = m_variables[paramName];
            }
        }

        // 2. Setăm valorile noi pentru parametri
        for (size_t i = 0; i < proc.params.size(); ++i) {
            if (i < passedArgs.size()) {
                m_variables[proc.params[i]] = evaluateExpression(passedArgs[i]);
            }
        }

        // 3. Executăm corpul
        for (const auto& line : proc.body) {
            execute(line);
        }

        // 4. RESTAURĂM doar variabilele protejate
        // Astfel, dacă procedura a modificat "$res", modificarea RĂMÂNE.
        // Dar dacă a modificat "$n", acesta revine la valoarea de dinainte de apel.
        for (const auto& [name, value] : protectedVariablesBackup) {
            m_variables[name] = value;
        }

        LOG_DEBUG(L"Procedure " + proc.name + L" finished.");
    }
    */
    /*
    void vOliEngine::callProcedure(const Procedure& proc, const std::vector<std::wstring>& passedArgs) {
        std::map<std::wstring, vData> protectedVariablesBackup;

        // 1. Shadowing protection
        for (const auto& rawParamName : proc.params) {
            std::wstring paramName = rawParamName;
            if (!paramName.empty() && paramName[0] == L'$') paramName.erase(0, 1);
            if (m_variables.count(paramName)) {
                protectedVariablesBackup[paramName] = m_variables[paramName];
            }
        }

        // 2. Mapare parametri (passedArgs sunt deja curățate de execute)
        for (size_t i = 0; i < proc.params.size(); ++i) {
            std::wstring paramName = proc.params[i];
            if (!paramName.empty() && paramName[0] == L'$') paramName.erase(0, 1);

            if (i < passedArgs.size()) {
                // Evaluăm argumentul (ex: "10" devine INT 10)
                vData evalResult = evaluateExpression(passedArgs[i]);
                m_variables[paramName] = evalResult;
                //LOG_DEBUG(L"    Parametru local setat: " + paramName + L" = " + vDataToWString(evalResult));
            }
            else {
                m_variables[paramName] = vData(); // Parametru lipsă setat pe Null
                //LOG_DEBUG(L"    Parametru local " + paramName + L" lipseste, setat NULL");
            }
        }

        // 3. Executăm liniile procedurii
        for (const auto& line : proc.body) {
            execute(line);
        }

        // 4. Restaurare variabile originale
        for (const auto& [name, value] : protectedVariablesBackup) {
            m_variables[name] = value;
        }
    }
    */
    void vOliEngine::callProcedure(const Procedure& proc, const std::vector<std::wstring>& passedArgs) {
        // --- 1. PUSH FRAME ---
        // Creăm un context local nou pentru această procedură
        StackFrame frame;
        frame.functionName = proc.name.empty() ? L"anonymous_proc" : proc.name;

        // NOTĂ: Nu mai facem std::move(m_variables). 
        // m_globalVariables rămâne neatins și accesibil prin resolveVariable.

        // --- 2. SETARE PARAMETRI LOCALI ---
        // Parametrii procedurii devin variabile locale în noul frame
        for (size_t i = 0; i < proc.params.size(); ++i) {
            std::wstring pName = cleanVariableName(proc.params[i]);

            if (i < passedArgs.size()) {
                // Evaluăm argumentul în contextul APELANTULUI (înainte de a face push la noul frame)
                vData val = evaluateExpression(passedArgs[i]);
                frame.localVariables[pName] = val;
            }
            else {
                frame.localVariables[pName] = vData{ std::monostate{} };
            }
        }

        // Adăugăm frame-ul în stivă - din acest moment resolveVariable va vedea aceste variabile ca "Locale"
        m_callStack.push_back(std::move(frame));

        // Salvare stare flag return
        bool previousShouldReturn = m_shouldReturn;
        m_shouldReturn = false;

        // --- 3. EXECUȚIE ---
        for (const auto& line : proc.body) {
            if (m_shouldReturn) break;

            // execute() va folosi intern resolveVariable/setVariable care 
            // acum "văd" noul frame din vârful stivei.
            execute(line);
        }

        // --- 4. POP FRAME ---
        // Curățăm memoria locală a procedurii
        if (!m_callStack.empty()) {
            m_callStack.pop_back();
        }

        // Restaurăm flag-ul de return al apelantului
        m_shouldReturn = previousShouldReturn;
    }


  void vOliEngine::handlePluginCommand(const ShellCommand& sc) {
      // 1. Verificăm dacă avem calea către DLL
      if (sc.args.empty()) {
          LOG_ERROR(L"Usage: /plugin \"path/to/plugin.dll\"");
          return;
      }

      // Luăm primul argument (calea). Parserul tău ar trebui să o curețe de ghilimele.
      std::wstring dllPath = sc.args[0];
      //LOG_DEBUG(dllPath);
      // 2. Încărcăm DLL-ul (Windows API)
      HMODULE hLib = LoadLibrary(dllPath.c_str());
      if (!hLib) {
          DWORD lastError = GetLastError();
          LOG_ERROR(L"Could not load DLL: " + dllPath + L" (Code: " + std::to_wstring(lastError) + L")");
          return;
      }

      // 3. Definim tipul funcției pe care o căutăm în DLL
      // Corespunde cu: void LoadOliPlugin(std::map<std::wstring, OliFunctionHandler>&)
      typedef void (*RegisterFunc)(std::map<std::wstring, OliFunctionHandler>&);

      // 4. Căutăm simbolul exportat
      RegisterFunc regFunc = (RegisterFunc)GetProcAddress(hLib, "LoadOliPlugin");

      if (regFunc) {
          // 5. Executăm funcția de înregistrare, trimițând map-ul de funcții al motorului
          regFunc(this->m_functionsHandlers);
          LOG_SUCCESS(L"Plugin loaded: " + dllPath);
          LOG_SUCCESS(L"          Native functions injected into Oli memory.");
      }
      else {
          LOG_ERROR(L"Invalid Plugin: Export 'LoadOliPlugin' not found in " + dllPath);
          FreeLibrary(hLib); // Eliberăm memoria dacă plugin-ul nu este valid
      }
  }

  vData vOliEngine::handleEvalFunc(const std::vector<vData>& args) {
      if (args.empty()) return vData{ 0.0 };

      // 1. Extragem expresia sub formă de text
      std::wstring expr;
      if (std::holds_alternative<std::wstring>(args[0].value)) {
          expr = std::get<std::wstring>(args[0].value);
      }
      else {
          // Dacă e număr, îl returnăm ca atare
          return args[0];
      }

      // 2. „Recursivitate” de evaluare: 
      // Chemăm evaluatorul principal pe conținutul string-ului
      return evaluateExpression(expr);
  }


  void vOliEngine::handleListProcsCommand(const ShellCommand& sc) {
      
      if (m_procedures.empty()) {
          ConsoleManager::getInstance().writeRaw(L"No user procedures defined.");
          return;
      }

      ConsoleManager::getInstance().writeRaw(L"--- [User Defined Procedures] ---");
      ConsoleManager::getInstance().writeRaw(L"NAME            PARAMETERS");
      ConsoleManager::getInstance().writeRaw(L"---------------------------------");

      for (auto const& [name, proc] : m_procedures) {
          std::wstring paramsStr = L"[";
          for (size_t i = 0; i < proc.params.size(); ++i) {
              paramsStr += proc.params[i];
              if (i < proc.params.size() - 1) paramsStr += L", ";
          }
          paramsStr += L"]";

          // Formatare simplă pentru aliniere
          std::wstring padding(std::max<int>(1, 15 - (int)name.length()), L' ');
          ConsoleManager::getInstance().writeRaw(name + padding + paramsStr);
      }
      ConsoleManager::getInstance().writeRaw(L"---------------------------------");
  }


  void vOliEngine::handleListFuncsCommand(const ShellCommand& sc) {
      if (m_userFunctions.empty() && m_functionsHandlers.empty()) {
          ConsoleManager::getInstance().writeRaw(L"No functions defined.");
          return;
      }

      ConsoleManager::getInstance().writeRaw(L"--- [Native Functions (C++)] ---");
      for (auto const& [name, _] : m_functionsHandlers) {
          ConsoleManager::getInstance().writeRaw(L"NATIVE: " + name);
      }

      ConsoleManager::getInstance().writeRaw(L"\n--- [User Defined Functions] ---");
      ConsoleManager::getInstance().writeRaw(L"NAME            PARAMETERS");
      ConsoleManager::getInstance().writeRaw(L"---------------------------------");

      for (auto const& [name, func] : m_userFunctions) {
          std::wstring paramsStr = L"[";
          for (size_t i = 0; i < func.params.size(); ++i) {
              paramsStr += func.params[i] + (i < func.params.size() - 1 ? L", " : L"");
          }
          paramsStr += L"]";

          std::wstring padding(std::max<int>(1, 15 - (int)name.length()), L' ');
          ConsoleManager::getInstance().writeRaw(name + padding + paramsStr);
      }
  }

  size_t vOliEngine::findTopLevelWhileKeyword(const std::wstring& line, const std::wstring& keyword) {
      int depth = 0;
      bool inQuotes = false;
      std::wstring upperLine = line;
      std::transform(upperLine.begin(), upperLine.end(), upperLine.begin(), ::towupper);
      std::wstring upperKey = keyword; // De obicei "/ENDWHILE" sau "/DO"
      std::transform(upperKey.begin(), upperKey.end(), upperKey.begin(), ::towupper);

      size_t firstWhile = upperLine.find(L"/WHILE");
      if (firstWhile == std::wstring::npos) return std::wstring::npos;

      for (size_t i = firstWhile + 6; i < upperLine.size(); ++i) {
          if (upperLine[i] == L'"') { inQuotes = !inQuotes; continue; }
          if (inQuotes) continue;

          std::wstring_view rem(&upperLine[i], upperLine.size() - i);

          // Verificăm dacă am găsit keyword-ul căutat (ex: /ENDWHILE) la nivelul 0 al buclei WHILE
          if (depth == 0 && rem.starts_with(upperKey)) {
              return i;
          }

          // Gestionăm DOAR ierarhia de WHILE-uri imbricate
          if (rem.starts_with(L"/WHILE")) {
              depth++;
              i += 5;
              continue;
          }
          if (rem.starts_with(L"/ENDWHILE")) {
              depth--;
              i += 8;
              continue;
          }
      }
      return std::wstring::npos;
  }

  
  void vOliEngine::handleBreakCommand(const ShellCommand& sc) {
      m_executionStatus = OliStatus::BREAK_REQUESTED;
  }

  void vOliEngine::handleContinueCommand(const ShellCommand& sc) {
      m_executionStatus = OliStatus::CONTINUE_REQUESTED;
  }



  vData vOliEngine::handleIntFunc(const std::vector<vData>& args) {
      if (args.empty()) return vData{ 0LL }; // Default la 0 de tip long long

      const vData& input = args[0];

      // Dacă este deja Int, returnăm copia
      if (input.isInt()) return input;

      // Dacă este Float, facem cast la long long (std::get<double>)
      if (input.isFloat()) {
          return vData{ static_cast<long long>(std::get<double>(input.value)) };
      }

      // Dacă este String, încercăm conversia numerică
      if (input.isString()) {
          try {
              return vData{ std::stoll(std::get<std::wstring>(input.value)) };
          }
          catch (...) {
              return vData{ 0LL }; // Conversie eșuată
          }
      }

      return vData{ 0LL };
  }


  void vOliEngine::handleFuncCommand(const ShellCommand& sc) {
      if (sc.args.empty()) {
          LOG_ERROR(L"Usage: func name [param1, param2...]");
          return;
      }

      // 1. Preluăm numele și îl normalizăm imediat (Uppercase)
      std::wstring funcName = sc.args[0];
      std::transform(funcName.begin(), funcName.end(), funcName.begin(), ::towupper);

      // 2. Salvăm numele normalizat ca fiind cel activ
      m_activeFuncName = funcName;

      Procedure newFunc;
      newFunc.name = m_activeFuncName;

      // 3. Extragem parametrii (asigură-te că și parametrii sunt tratați consistent)
      for (size_t i = 1; i < sc.args.size(); ++i) {
          std::wstring arg = sc.args[i];
          arg.erase(std::remove_if(arg.begin(), arg.end(), [](wchar_t c) {
              return c == L'[' || c == L']' || c == L',';
              }), arg.end());

          if (!arg.empty()) {
              // Recomandare: și parametrii ar trebui să fie normalizați dacă 
              // motorul tău îi caută ulterior în m_variables (care probabil e case-sensitive)
              newFunc.params.push_back(arg);
          }
      }

      // 4. Mapăm funcția folosind cheia Uppercase
      m_userFunctions[m_activeFuncName] = newFunc;
      m_isRecordingFunc = true;

      LOG_INFO(L"Started recording function: " + m_activeFuncName);
  }

 /*
  vData vOliEngine::callUserFunction(const std::wstring& funcName, const std::vector<vData>& args) {
      auto it = m_userFunctions.find(funcName);
      if (it == m_userFunctions.end()) return vData{ std::monostate{} };

      const Procedure& func = it->second;

      // 1. Snapshot la variabilele globale
      std::map<std::wstring, vData> globalVariables = std::move(m_variables);
      m_variables.clear();

      // 2. Setăm argumentele locale
      for (size_t i = 0; i < func.params.size(); ++i) {
          m_variables[func.params[i]] = (i < args.size()) ? args[i] : vData{ std::monostate{} };
      }
      m_variables[L"return"] = vData{ std::monostate{} };

      // 3. RECONSTRUCȚIA CORPULUI (Soluția pentru erorile tale)
      std::wstring fullBody;
      for (const auto& line : func.body) {
          std::wstring trimmed = trim(line);
          if (trimmed.empty()) continue;

          fullBody += trimmed;
          // Adăugăm separator dacă nu există deja
          if (trimmed.back() != L';') fullBody += L";";
          fullBody += L" ";
      }

      // 4. Executăm TOT corpul ca un singur bloc de cod
      bool prevShouldReturn = m_shouldReturn;
      m_shouldReturn = false;

      // Aici e magia: executeInternal va vedea tot textul și va 
      // extrage corect WHILE...ENDWHILE sau IF...ENDIF
      this->executeInternal(fullBody);

      // 5. Cleanup și Return
      vData result = m_variables[L"return"];
      m_variables = std::move(globalVariables);
      m_shouldReturn = prevShouldReturn;

      return result;
  }
  */

  vData vOliEngine::callUserFunction(const std::wstring& funcName, const std::vector<vData>& args) {
      auto it = m_userFunctions.find(funcName);
      if (it == m_userFunctions.end()) {
          LOG_ERROR(L"Runtime Error: Function '" + funcName + L"' not found.");
          return vData{ std::monostate{} };
      }

      const Procedure& func = it->second;

      // --- 1. PREGĂTIRE FRAME NOU ---
      StackFrame frame;
      frame.functionName = funcName;

      // ATENȚIE: Nu mai facem std::move(m_variables). 
      // m_globalVariables stă pe loc, nu se mișcă nicăieri.

      // --- 2. SETARE PARAMETRI LOCALI ---
      // Parametrii funcției (ex: $n) se duc direct în rucsacul local al noului frame
      for (size_t i = 0; i < func.params.size(); ++i) {
          std::wstring pName = cleanVariableName(func.params[i]);
          frame.localVariables[pName] = (i < args.size()) ? args[i] : vData{ std::monostate{} };
      }

      // Variabila specială locală pentru rezultatul funcției
      frame.localVariables[L"return"] = vData{ std::monostate{} };

      // --- 3. PUSH PE STIVĂ ---
      m_callStack.push_back(std::move(frame));

      // Salvare stare flag return
      bool previousShouldReturn = m_shouldReturn;
      m_shouldReturn = false;

      // --- 4. EXECUȚIE ---
      std::wstring fullBody;
      for (const auto& line : func.body) {
          if (line.empty()) continue;
          fullBody += line;
          if (line.back() != L';') fullBody += L";";
          fullBody += L" ";
      }

      // Executăm corpul. Acum, orice "set" sau "resolve" va vedea frame-ul de deasupra.
      this->executeInternal(fullBody);

      // --- 5. COLECTAREA REZULTATULUI ---
      // Rezultatul este în vârful stivei noastre, în variabila locală "return"
      vData result = m_callStack.back().localVariables[L"return"];

      // --- 6. POP FRAME (Restaurare) ---
      if (!m_callStack.empty()) {
          m_callStack.pop_back(); // Ștergem rucsacul local, eliberăm memoria
      }

      m_shouldReturn = previousShouldReturn;

      return result;
  }


  void vOliEngine::handleReturnCommand(const ShellCommand& sc) {
      // 1. Verificăm dacă suntem într-o funcție
      // Dacă stiva e goală, un "return" în CLI nu are unde să scrie rezultatul
      if (m_callStack.empty()) {
          LOG_ERROR(L"Return command used outside of a function context.");
          return;
      }

      vData finalResult;

      // 2. Evaluăm valoarea de returnat
      if (sc.args.empty()) {
          // Dacă nu avem argumente, returnăm null (sau 0, depinde de preferința ta)
          finalResult = vData{ std::monostate{} };
      }
      else {
          // Reconstruim expresia (ex: return $a + $b)
          std::wstring expression;
          for (size_t i = 0; i < sc.args.size(); ++i) {
              expression += sc.args[i] + (i < sc.args.size() - 1 ? L" " : L"");
          }

          try {
              finalResult = evaluateExpression(expression);
          }
          catch (const std::exception& e) {
              std::string err(e.what());
              LOG_ERROR(L"Return evaluation error: " + std::wstring(err.begin(), err.end()));
              return;
          }
      }

      // 3. SALVAREA ÎN FRAME-UL CURENT
      // Scriem rezultatul în variabila specială "return" din contextul local
      m_callStack.back().localVariables[L"return"] = finalResult;

      // 4. SEMNALIZAREA IEȘIRII (Early Return)
      // Acest flag va face ca loop-urile din callUserFunction sau callProcedure să se oprească
      m_shouldReturn = true;
  }

  void vOliEngine::handleForCommand(const std::wstring& fullLine) {
      std::wstring upperLine = fullLine;
      std::transform(upperLine.begin(), upperLine.end(), upperLine.begin(), ::towupper);

      // 1. Identificare FOR
      size_t forPos = upperLine.find(L"FOR");
      if (forPos == std::wstring::npos) return;

      // 2. Delimitatori (findTopLevelKeyword previne coliziunile în FOR-uri imbricate)
      size_t posTo = findTopLevelKeyword(fullLine, L"TO", L"FOR");
      size_t posDo = findTopLevelKeyword(fullLine, L"DO", L"FOR");
      size_t posEnd = findTopLevelKeyword(fullLine, L"ENDFOR", L"FOR");

      if (posTo == std::wstring::npos || posDo == std::wstring::npos || posEnd == std::wstring::npos) {
          LOG_ERROR(L"Malformed FOR: Lipsesc TO, DO sau ENDFOR");
          return;
      }

      size_t posBy = findTopLevelKeyword(fullLine, L"BY", L"FOR");

      // 3. Extragere segmente
      size_t initStart = forPos + 3;
      std::wstring initPart = trim(fullLine.substr(initStart, posTo - initStart));

      size_t limitStart = posTo + 2;
      size_t limitEnd = (posBy != std::wstring::npos) ? posBy : posDo;
      std::wstring limitExpr = trim(fullLine.substr(limitStart, limitEnd - limitStart));

      std::wstring stepExpr = L"1";
      if (posBy != std::wstring::npos) {
          size_t stepStart = posBy + 2;
          stepExpr = trim(fullLine.substr(stepStart, posDo - stepStart));
      }

      size_t bodyStart = posDo + 2;
      std::wstring bodyCommand = trim(fullLine.substr(bodyStart, posEnd - bodyStart));
      std::vector<std::wstring> instructions = preParse(bodyCommand);

      // 4. Extragere nume variabilă și valoare inițială
      size_t eqPos = initPart.find(L'=');
      if (eqPos == std::wstring::npos) {
          LOG_ERROR(L"FOR init format invalid. Folosește: FOR $i = 1 TO...");
          return;
      }

      std::wstring varName = trim(initPart.substr(0, eqPos));
      if (!varName.empty() && varName[0] == L'$') varName.erase(0, 1);

      std::wstring initValueExpr = trim(initPart.substr(eqPos + 1));

      // --- 5. EXECUȚIA ---

      // A. Inițializare (evaluăm valoarea de start și o setăm direct)
      vData startVal = evaluateExpression(initValueExpr);
      setVariable(varName, startVal);

      int safetyBreak = 0;
      const int MAX_ITER = 10000;

      while (true) {
          // B. Evaluăm condițiile de control
          vData currentVal = resolveVariable(varName);
          vData limitVal = evaluateExpression(limitExpr);
          vData stepData = evaluateExpression(stepExpr);

          double current = vDataToDouble(currentVal);
          double limit = vDataToDouble(limitVal);
          double step = vDataToDouble(stepData);

          // C. Verificăm ieșirea
          if (step >= 0 && current > limit) break;
          if (step < 0 && current < limit) break;

          // D. Executăm corpul buclei
          bool breakFromLoop = false;
          for (const auto& instr : instructions) {
              if (instr.empty()) continue;

              this->execute(instr);

              if (m_executionStatus == OliStatus::BREAK_REQUESTED) {
                  m_executionStatus = OliStatus::RUNNING;
                  breakFromLoop = true;
                  break;
              }
              if (m_executionStatus == OliStatus::CONTINUE_REQUESTED) {
                  m_executionStatus = OliStatus::RUNNING;
                  goto perform_step; // Sarim direct la incrementare
              }
              if (m_executionStatus == OliStatus::RETURN_REQUESTED) return;
          }

          if (breakFromLoop) break;

      perform_step:
          // E. Incrementare manuală (fără a trece prin parserul de SET)
          current += step;
          setVariable(varName, vData(current));

          if (++safetyBreak > MAX_ITER) {
              LOG_ERROR(L"FOR Infinite loop safety trigger!");
              break;
          }
      }
  }


  void vOliEngine::handleRepeatCommand(const std::wstring& fullLine) {
      std::wstring upperLine = fullLine;
      std::transform(upperLine.begin(), upperLine.end(), upperLine.begin(), ::towupper);

      size_t repeatPos = upperLine.find(L"REPEAT");
      size_t posUntil = findTopLevelKeyword(fullLine, L"UNTIL", L"REPEAT");
      size_t posEnd = findTopLevelKeyword(fullLine, L"ENDREPEAT", L"REPEAT");

      //LOG_DEBUG(L"DEBUG FIND: FullLine Length = " + std::to_wstring(fullLine.length()));
      //LOG_DEBUG(L"DEBUG FIND: UntilPos = " + (posUntil == std::wstring::npos ? L"NPOS" : std::to_wstring(posUntil)));

      if (posUntil == std::wstring::npos || posEnd == std::wstring::npos) {
          LOG_ERROR(L"Malformed REPEAT: Missing UNTIL or ENDREPEAT");
          return;
      }

      // 1. Extragem corpul și îl curățăm de whitespace-ul de la margini înainte de preParse
      size_t bodyStart = repeatPos + 6;
      std::wstring bodyCommand = fullLine.substr(bodyStart, posUntil - bodyStart);

      // 2. Parsăm instrucțiunile
      std::vector<std::wstring> instructions = preParse(bodyCommand);

      // 3. Extragem condiția
      size_t condStart = posUntil + 5;
      std::wstring conditionPart = trim(fullLine.substr(condStart, posEnd - condStart));

      int safetyBreak = 0;
      while (safetyBreak < 1000) {
          safetyBreak++;

          // Resetăm statusul pentru a permite execuția liniei următoare
          m_executionStatus = OliStatus::RUNNING;

          bool hasExecutedAtLeastOne = false;

          for (auto& instr : instructions) {
              std::wstring cleanInstr = trim(instr);
              if (cleanInstr.empty()) continue;

              hasExecutedAtLeastOne = true;
              this->executeInternal(cleanInstr);

              // Verificăm dacă instrucțiunea a cerut oprirea
              if (m_executionStatus == OliStatus::RETURN_REQUESTED) return;
              if (m_executionStatus == OliStatus::BREAK_REQUESTED) {
                  m_executionStatus = OliStatus::RUNNING;
                  return;
              }
              if (m_executionStatus == OliStatus::CONTINUE_REQUESTED) {
                  m_executionStatus = OliStatus::RUNNING;
                  goto do_condition;
              }
          }

          // Dacă nu a existat nicio instrucțiune validă în corp, ieșim să nu facem loop infinit
          if (!hasExecutedAtLeastOne) break;

      do_condition:
          // IMPORTANT: În REPEAT...UNTIL, bucla se oprește când condiția devine TRUE
          vData result = evaluateExpression(conditionPart);
          if (vDataToBool(result)) {
              break;
          }
      }
  }



  std::wstring toUpper(std::wstring str) {
      std::transform(str.begin(), str.end(), str.begin(), ::towupper);
      return str;
  }


  void vOliEngine::handleSwitchCommand(const std::wstring& fullLine) {
      size_t posEnd = findTopLevelSwitchKeyword(fullLine, L"ENDSWITCH");
      if (posEnd == std::wstring::npos) {
          LOG_ERROR(L"Eroare SWITCH: Lipseste ENDSWITCH.");
          return;
      }

      // 1. Extragem valoarea de control (intre SWITCH si primul CASE/DEFAULT)
      // Căutăm unde începe primul CASE sau DEFAULT
      size_t firstCase = findTopLevelSwitchKeyword(fullLine, L"CASE");
      size_t firstDefault = findTopLevelSwitchKeyword(fullLine, L"DEFAULT");
      size_t posStartBody = (firstCase < firstDefault) ? firstCase : firstDefault;

      if (posStartBody == std::wstring::npos) posStartBody = posEnd;

      std::wstring controlExpr = trim(fullLine.substr(6, posStartBody - 6)); // "SWITCH " are 7 caractere
      vData controlValue = evaluateExpression(controlExpr);
      std::wstring controlStr = vDataToWString(controlValue);

      // 2. Extragem corpul switch-ului
      std::wstring body = fullLine.substr(posStartBody, posEnd - posStartBody);
      std::vector<std::wstring> lines = preParse(body);

      bool matchFound = false;
      bool executeNext = false; // Pentru fall-through

      for (const auto& line : lines) {
          std::wstring trimmed = trim(line);
          std::wstring upper = toUpper(trimmed);

          // Verificăm dacă linia este un CASE
          if (upper.find(L"CASE ") == 0) {
              if (matchFound && !executeNext) break; // Am terminat un case anterior cu match

              std::wstring caseExpr = trim(trimmed.substr(5));
              vData caseVal = evaluateExpression(caseExpr);
              //if (vDataToWString(caseVal) == controlStr) {
              if (compareVData(caseVal, controlValue)){
                  matchFound = true;
                  executeNext = true;
              }
              continue;
          }

          // Verificăm dacă linia este DEFAULT
          if (upper == L"DEFAULT") {
              if (matchFound) {
                  // Dacă am găsit deja un match și am executat ceva, 
                  // DEFAULT nu ar trebui să se mai atingă decât dacă am vrea fall-through (rar)
                  executeNext = false;
                  break;
              }
              executeNext = true;
              matchFound = true;
              continue;
          }

          // Verificăm BREAK
          if (upper == L"BREAK") {
              if (executeNext) {
                  executeNext = false;
                  break; // Ieșim complet din bucla liniilor switch-ului
              }
              continue;
          }

          // Executăm instrucțiunea dacă suntem într-un case activ
          if (executeNext) {
              this->executeInternal(line);
              // Propagăm RETURN sau alte stări de control
              if (m_shouldReturn || m_executionStatus != OliStatus::RUNNING) return;
          }
      }
  }

  size_t vOliEngine::findTopLevelSwitchKeyword(const std::wstring& line, const std::wstring& keyword) {
      int depth = 0;
      bool inQuotes = false;
      size_t kwLen = keyword.length();

      for (size_t i = 0; i < line.length(); ++i) {
          if (line[i] == L'"') inQuotes = !inQuotes;
          if (inQuotes) continue;

          // Verificăm început de blocuri care pot conține CASE (SWITCH, IF, WHILE etc)
          // Aici depinde de ce keywords ai deja, dar minim SWITCH/ENDSWITCH
          if (line.substr(i, 6) == L"SWITCH") { depth++; i += 5; continue; }
          if (line.substr(i, 9) == L"ENDSWITCH") { depth--; i += 8; continue; }

          if (depth == 0) {
              if (i + kwLen <= line.length()) {
                  std::wstring sub = line.substr(i, kwLen);
                  if (toUpper(sub) == keyword) {
                      // Verificăm delimitatorii (să nu fie sub-șir în alt cuvânt)
                      bool startOk = (i == 0 || iswspace(line[i - 1]));
                      bool endOk = (i + kwLen == line.length() || iswspace(line[i + kwLen]));
                      if (startOk && endOk) return i;
                  }
              }
          }
      }
      return std::wstring::npos;
  }

  bool vOliEngine::compareVData(const vData& lhs, const vData& rhs) {
      // 1. Cazul ambelor Null
      if (lhs.isNull() && rhs.isNull()) return true;
      if (lhs.isNull() || rhs.isNull()) return false;

      // 2. Comparație numerică (Promovare automată la double pentru flexibilitate)
      if (canBeNumeric(lhs) && canBeNumeric(rhs)) {
          return std::abs(vDataToDouble(lhs) - vDataToDouble(rhs)) < 1e-9;
      }

      // 3. Dacă au tipuri identice, comparăm direct valorile din variant
      if (lhs.value.index() == rhs.value.index()) {
          return std::visit([&](auto&& leftArg) -> bool {
              using T = std::decay_t<decltype(leftArg)>;

              // Verificăm dacă tipul suportă operatorul ==
              if constexpr (std::is_same_v<T, std::wstring> ||
                  std::is_same_v<T, long long> ||
                  std::is_same_v<T, double> ||
                  std::is_same_v<T, bool>) {
                  return leftArg == std::get<T>(rhs.value);
              }

              // Pentru Array și Map, comparația se face recursiv (vector/map au operator== definit)
              // dar vData trebuie să aibă și el operator== definit pentru asta.
              if constexpr (std::is_same_v<T, vDataArray> || std::is_same_v<T, vDataMap>) {
                  // Dacă nu vrei să supraîncarci operator== în vData, poți returna false aici
                  // sau poți implementa o logică de iterație manuală.
                  return false;
              }

              return false;
              }, lhs.value);
      }

      // 4. Fallback: Comparație ca text (cel mai sigur pentru scripting)
      // De exemplu: un bool true va fi egal cu string-ul "true" sau "1" depinde de vDataToWString
      return vDataToWString(lhs) == vDataToWString(rhs);
  }

  void vOliEngine::handleClearCommand(const ShellCommand& sc) {
      // Apelăm pur și simplu metoda din singleton-ul ConsoleManager
      ConsoleManager::getInstance().clear();
  }
  /*
void vOliEngine::setVariable(const std::wstring& name, const vData& value, bool isGlobal) {
    // 1. Curățăm numele variabilei
    std::wstring cleanName = name;
    if (!cleanName.empty() && cleanName[0] == L'$') {
        cleanName = cleanName.substr(1);
    }
    cleanName = trim(cleanName);

    // 2. Determinăm dacă trebuie să fie Globală
    // isGlobal vine din parametri, m_nextSetIsGlobal vine de la comanda 'set global ...'
    bool finalGlobal = isGlobal || m_nextSetIsGlobal;

    if (finalGlobal) {
        m_globalVariables[cleanName] = value;
        m_nextSetIsGlobal = false; // Consumăm flag-ul după utilizare
        return;
    }

    // 3. Logica de Scope (Local-First)
    if (!m_callStack.empty()) {
        auto& currentFrame = m_callStack.back();

        // A. Dacă variabila există deja în contextul LOCAL al funcției, o actualizăm acolo
        if (currentFrame.localVariables.count(cleanName)) {
            currentFrame.localVariables[cleanName] = value;
            return;
        }

        // B. Dacă variabila există deja în GLOBAL, o actualizăm pe cea globală
        // Aceasta este piesa critică pentru FIBO_MEMO: permite modificarea lui $memo global
        if (m_globalVariables.count(cleanName)) {
            m_globalVariables[cleanName] = value;
            return;
        }

        // C. Dacă nu există nicăieri, devine LOCALĂ (nu poluează globalul)
        currentFrame.localVariables[cleanName] = value;
    }
    else {
        // 4. Suntem în CLI / Script principal (Global Scope)
        m_globalVariables[cleanName] = value;
    }
}
*/
/*
  void vOliEngine::setVariable(const std::wstring& name, const vData& value, bool isGlobal) {
      if (name.empty()) return;

      // 1. Verificăm prefixul @ (Forțare Globală Directă)
      if (name[0] == L'@') {
          std::wstring cleanName = trim(name.substr(1));
          // Tăiem eventualele căi de tip proprietate dacă e nevoie (ex: @obj.x)
          size_t firstSep = cleanName.find_first_of(L".[");
          if (firstSep != std::wstring::npos) cleanName = cleanName.substr(0, firstSep);

          m_globalVariables[cleanName] = value;
          return;
      }

      // 2. Curățăm numele variabilei standard ($)
      std::wstring cleanName = name;
      if (cleanName[0] == L'$') {
          cleanName = cleanName.substr(1);
      }
      cleanName = trim(cleanName);

      // 3. Determinăm dacă trebuie să fie Globală (flag-uri)
      bool finalGlobal = isGlobal || m_nextSetIsGlobal;

      if (finalGlobal) {
          m_globalVariables[cleanName] = value;
          // m_nextSetIsGlobal = false; // Mutăm resetarea în handleSetCommand pentru siguranță
          return;
      }

      // 4. Logica de Scope (Local-First)
      if (!m_callStack.empty()) {
          auto& currentFrame = m_callStack.back();

          // A. Update LOCAL existent
          if (currentFrame.localVariables.count(cleanName)) {
              currentFrame.localVariables[cleanName] = value;
              return;
          }

          // B. Update GLOBAL existent (Permite modificarea $memo)
          if (m_globalVariables.count(cleanName)) {
              m_globalVariables[cleanName] = value;
              return;
          }

          // C. CREARE LOCALĂ (Default pentru variabile noi în funcții)
          currentFrame.localVariables[cleanName] = value;
      }
      else {
          // 5. Global Scope (CLI)
          m_globalVariables[cleanName] = value;
      }
  }
  */

void vOliEngine::setVariable(const std::wstring& name, const vData& value, bool isGlobal) {
    if (name.empty()) return;

    // 1. PREFIXUL @ -> Scrie DIRECT în Global, indiferent de restul regulilor
    if (name[0] == L'@') {
        std::wstring cleanName = trim(name.substr(1));
        size_t firstSep = cleanName.find_first_of(L".[");
        if (firstSep != std::wstring::npos) cleanName = cleanName.substr(0, firstSep);
        m_globalVariables[cleanName] = value;
        return;
    }

    // 2. CURĂȚARE NUME
    std::wstring cleanName = name;
    if (cleanName[0] == L'$') cleanName = cleanName.substr(1);
    cleanName = trim(cleanName);

    // 3. FLAG-URI GLOBALE (ex: set global ...)
    if (isGlobal || m_nextSetIsGlobal) {
        m_globalVariables[cleanName] = value;
        return;
    }

    // 4. LOGICA DE SCOPING (Shadowing activat)
    if (!m_callStack.empty()) {
        // Suntem în funcție: Orice 'set $a' devine LOCAL.
        // Nu ne interesează dacă există o globală numită 'a'.
        // Aceasta va fi "ascunsă" de variabila locală nouă.
        m_callStack.back().localVariables[cleanName] = value;
    }
    else {
        // Suntem în contextul principal: Totul este Global.
        m_globalVariables[cleanName] = value;
    }
}


  void vOliEngine::printTraceback() {
      LOG_ERROR(L"--- Call Stack Traceback ---");

      // 1. Afișăm unde suntem acum (vârful stivei)
      LOG_ERROR(L"  -> In function: [Current Scope]");

      // 2. Parcurgem stiva de la cel mai recent apel spre Global
      for (auto it = m_callStack.rbegin(); it != m_callStack.rend(); ++it) {
          std::wstring name = it->functionName.empty() ? L"Global Scope" : it->functionName;
          LOG_ERROR(L"  -> Called from: " + name);
      }
      LOG_ERROR(L"----------------------------");
  }

  void vOliEngine::dumpStackTrace() {
      std::wcout << L"\n--- [TRACEBACK] ---\n";
      for (int i = m_callStack.size() - 1; i >= 0; --i) {
          auto& frame = m_callStack[i];
          std::wcout << L"#" << i << L" " << frame.functionName;

          // Dacă vrei să vezi valoarea lui 'n' în fiecare frame:
          if (frame.localVariables.count(L"n")) {
              std::wcout << L" (n = " << vDataToWString(frame.localVariables.at(L"n")) << L")";
          }
          std::wcout << L"\n";
      }
  }

  void vOliEngine::handleTraceCommand(const ShellCommand& sc) {
      LOG_INFO(L""); // Linie goală pentru lizibilitate
      LOG_INFO(L"--- [OLI CALL STACK TRACEBACK] ---");

      // 1. Afișăm contextul curent (Frame 0)
      // Acesta este frame-ul activ care nu a fost încă "împins" în m_callStack
      std::wstring currentContext = L"Global Scope";

      // Dacă suntem într-o funcție, putem deduce asta dacă m_callStack nu e goală
      // sau dacă am salvat numele funcției curente undeva.
      LOG_INFO(L"Frame [0]: In execution (Active Scope)");

      // 2. Parcurgem stiva de la cel mai recent apel la cel mai vechi
      int depth = 1;
      for (auto it = m_callStack.rbegin(); it != m_callStack.rend(); ++it) {
          std::wstring name = it->functionName.empty() ? L"Anonymous/Global" : it->functionName;

          // Bonus: Afișăm și câte variabile locale sunt în acel frame
          std::wstring info = L"Frame [" + std::to_wstring(depth++) + L"]: Called from " + name;
          info += L" (" + std::to_wstring(it->localVariables.size()) + L" variables)";

          LOG_INFO(info);
      }

      LOG_INFO(L"----------------------------------");
      LOG_INFO(L"");
  }

  void vOliEngine::handleDefCommand(const ShellCommand& sc) {
      if (sc.args.size() < 3) {
          LOG_ERROR(L"[SYNTAX ERROR] Usage: def struct/class Name { field1, field2 }");
          return;
      }

      // 1. Reconstruim linia pentru a procesa blocul de acolade
      std::wstring fullLine;
      for (const auto& a : sc.args) fullLine += a + L" ";
      fullLine = wstr_trim(fullLine);

      // 2. Extragem tipul și numele (folosind tokens pentru siguranță)
      // Ne așteptăm la: [0]=struct/class, [1]=Name
      std::vector<std::wstring> tokens = vOliCommandParser::tokenize(fullLine);
      if (tokens.size() < 2) return;

      std::wstring subType = tokens[0];
      std::wstring typeName = tokens[1];

      // Normalizăm subType (struct/class) pentru verificare
      std::wstring subTypeLower = subType;
      std::transform(subTypeLower.begin(), subTypeLower.end(), subTypeLower.begin(), ::towlower);

      // 3. Localizăm și extragem conținutul dintre acolade
      size_t startBrace = fullLine.find(L'{');
      size_t endBrace = fullLine.find(L'}');

      if (startBrace == std::wstring::npos || endBrace == std::wstring::npos) {
          LOG_ERROR(L"[SYNTAX ERROR] Missing fields block { ... } in definition.");
          return;
      }

      std::wstring fieldsContent = fullLine.substr(startBrace + 1, endBrace - startBrace - 1);

      // 4. Folosim wexplodeQuoteSafe pentru a separa câmpurile prin virgulă
      std::vector<std::wstring> cleanFields = wexplodeQuoteSafe(fieldsContent, L',');

      // 5. Creăm Blueprint-ul
      vTypeBlueprint bp;
      bp.name = typeName;
      bp.fields = cleanFields;
      bp.isClass = (subTypeLower == L"class");

      // Salvăm în registrul motorului
      m_blueprints[typeName] = bp;

      LOG_SUCCESS(L"Blueprint '" + typeName + L"' (as " + subTypeLower + L") recorded with " +
          std::to_wstring(cleanFields.size()) + L" fields.");
  }

  void vOliEngine::updateDataMember(vData& container, const vData& key, const vData& newValue) {
      if (container.isMap()) {
          auto& m = std::get<vDataMap>(container.value);
          m[vDataToWString(key)] = newValue;
      }
      else if (container.isArray()) {
          auto& a = std::get<vDataArray>(container.value);
          size_t idx = static_cast<size_t>(vDataToDouble(key));
          if (idx < a.size()) {
              a[idx] = newValue;
          }
      }
  }

  void vOliEngine::updateRootSource(ASTPtr node, const vData& updatedValue) {
      if (!node) return;

     // LOG_DEBUG(L"updateRootSource: Processing node type " + std::to_wstring((int)node->type) + L" with value: " + node->value);
      // CAZUL 1: Am ajuns la variabila rădăcină ($c1)
      if (node->type == ASTNodeType::Variable) {
          if (node->value.find(L'.') != std::wstring::npos) {
              // Dacă am ajuns aici, înseamnă că Parserul a colapsat drumul într-un singur nod.
              // Trebuie să-l spargem manual pentru a găsi rădăcina reală.
              std::wstring rootName = node->value.substr(0, node->value.find(L'.'));
              //LOG_DEBUG(L"updateRootSource: Correcting path. Real root is [" + rootName + L"]");

              // Re-obținem obiectul întreg, modificăm bucata și salvăm
              vData rootObj = resolveVariable(rootName);
              // Aici ar trebui o logică de navigare manuală dacă parserul e plat, 
              // dar cea mai bună cale e să te asiguri că nodul DOT este cel procesat.
              setVariable(rootName, updatedValue);
              return;
          }

          setVariable(node->value, updatedValue, m_nextSetIsGlobal);
          return;
      }

      // CAZUL 2: Suntem pe un nod de acces (DOT / .)
      if (node->value == L"DOT" || node->value == L".") {
          // 1. Re-evaluăm părintele (ex: obiectul $c1 pentru a-i modifica 'locatie')
          vData parentContainer = executeAST(node->children[0]);

          // DEBUG CRITIC: Vedem dacă părintele a fost găsit
          if (parentContainer.isNull()) {
              //LOG_ERROR(L"updateRootSource: parentContainer is NULL for key: " + node->children[1]->value);
          }

          // 2. Extragem numele câmpului și îl convertim în vData pentru updateDataMember
          std::wstring fieldName = node->children[1]->value;
          vData key(fieldName); // Conversia cerută de compilator

          // 3. Modificăm copia părintelui cu noua valoare a membrului
          updateDataMember(parentContainer, key, updatedValue);

          //LOG_DEBUG(L"updateRootSource: Bubbling up to parent of [" + fieldName + L"]");
          // 4. RECURSIVITATE: Urcăm spre rădăcină
          updateRootSource(node->children[0], parentContainer);
      }
      // CAZUL 3: Suntem pe un nod de INDEX ([])
      else if (node->value == L"INDEX" || node->value == L"[") {
          vData parentContainer = executeAST(node->children[0]);
          vData index = executeAST(node->children[1]);

          updateDataMember(parentContainer, index, updatedValue);
          updateRootSource(node->children[0], parentContainer);
      }
  }