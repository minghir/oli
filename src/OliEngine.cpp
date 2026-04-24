#include "OliEngine.hpp"
#include "OliExpressionParser.hpp"
#include "PortTools.hpp"

#include <fstream>
#include <filesystem>
#include <random>
#include <vector>
#include <string>
#include <cstring>
#include <thread>
#include <chrono>
#include <string_view>
#include <algorithm> // Necesar pentru std::replace

void vOliEngine::execute(const std::wstring& line) {
    // --- 1. CURĂȚARE ȘI COMENTARII ---
    std::wstring cleanLine = trim(line);

    // Eliminăm comentariile de tip #
    size_t commentPos = cleanLine.find(L'#');
    if (commentPos != std::wstring::npos) {
        cleanLine = trim(cleanLine.substr(0, commentPos));
    }

    if (cleanLine.empty() && m_accumulator.empty()) return;

    // --- 2. DETECTARE PARANTEZE (MAPS/ARRAYS) ---
    // Această numărătoare ne spune dacă suntem în interiorul unei definiții de obiect
    for (wchar_t c : cleanLine) {
        if (c == L'{' || c == L'[') m_bracketDepth++;
        if (c == L'}' || c == L']') m_bracketDepth--;
    }

    std::wstring upperLine = cleanLine;
    std::transform(upperLine.begin(), upperLine.end(), upperLine.begin(), ::towupper);

    // --- 3. GESTIONARE ÎNREGISTRARE FUNC/PROC ---
    // Dacă suntem în modul record, salvăm liniile direct în corpul funcției
    if (m_isRecording || m_isRecordingFunc) {
        if (upperLine == L"ENDPROC" || upperLine == L"ENDFUNC") {
            m_isRecording = false;
            m_isRecordingFunc = false;
            m_blockDepth = 0;
            m_bracketDepth = 0; // Reset de siguranță
            vOliKeyWords::registerDynamicCommand(m_activeProcName);
            LOG_SUCCESS(L"Procedure/Function saved.");
            return;
        }

        if (m_isRecording) m_procedures[m_activeProcName].body.push_back(cleanLine);
        else m_userFunctions[m_activeFuncName].body.push_back(cleanLine);
        return;
    }

    // --- 4. TRACKING ADÂNCIME BLOCURI (IF, WHILE, etc.) ---
    bool isHelpCall = (upperLine.find(L"HELP") == 0);

    auto checkAndLog = [&](const std::wstring& key, bool increment) {
        if (isHelpCall) return false; // Dacă e help, nu numărăm blocuri!

        size_t p = upperLine.find(key);
        if (p != std::wstring::npos) {
            bool startOk = (p == 0 || iswspace(upperLine[p - 1]));
            bool endOk = (p + key.length() >= upperLine.length() || iswspace(upperLine[p + key.length()]));
            if (startOk && endOk) {
                if (increment) m_blockDepth++;
                else if (m_blockDepth > 0) m_blockDepth--;
                return true;
            }
        }
        return false;
        };

    // Incrementăm adâncimea pentru cuvinte cheie
    checkAndLog(L"IF", true);
    checkAndLog(L"WHILE", true);
    checkAndLog(L"FOR", true);
    checkAndLog(L"REPEAT", true);
    checkAndLog(L"CYCLE", true);
    checkAndLog(L"PROC", true);
    checkAndLog(L"FUNC", true);
    checkAndLog(L"SWITCH", true);

    // Decrementăm pentru finaluri
    checkAndLog(L"ENDIF", false);
    checkAndLog(L"ENDWHILE", false);
    checkAndLog(L"ENDFOR", false);
    checkAndLog(L"ENDREPEAT", false);
    checkAndLog(L"ENDCYCLE", false);
    checkAndLog(L"ENDPROC", false);
    checkAndLog(L"ENDFUNC", false);
    checkAndLog(L"ENDSWITCH", false);

    // --- 5. ACUMULARE ---
    bool hasBackslash = (!cleanLine.empty() && cleanLine.back() == L'\\');
    if (hasBackslash) cleanLine.pop_back();

    if (!m_accumulator.empty()) m_accumulator += L"\n";
    m_accumulator += cleanLine;

    // Declanșare imediată pentru începutul definiției de PROC/FUNC
    if (upperLine.find(L"PROC ") == 0 || upperLine.find(L"FUNC ") == 0) {
        std::wstring startCmd = m_accumulator;
        m_accumulator.clear();
        this->executeInternal(startCmd);
        return;
    }

    // --- 6. DECIZIA DE AȘTEPTARE ---
    // Dacă avem blocuri deschise, paranteze deschise sau backslash, nu executăm încă.
    if (m_blockDepth > 0 || m_bracketDepth > 0 || hasBackslash) {
        return;
    }

    // --- 7. EXECUȚIE BLOC COMPLET ---
    std::wstring finalBlock = m_accumulator;
    m_accumulator.clear();
    m_bracketDepth = 0; // Resetăm pentru următoarea comandă

    if (trim(finalBlock).empty()) return;

    // --- FIX CRITIC: APLATIZAREA PENTRU MULTI-LINE MAPS/ARRAYS ---
    // Dacă avem un bloc care nu este un corp de FUNC/PROC sau un IF complex,
    // înlocuim \n cu spațiu. Astfel, preParse nu va sparge Map-ul în bucăți
    // care ar genera erori de tip "Lipseste }" în parserul de expresii.
   // if (m_blockDepth == 0) {
   //     std::replace(finalBlock.begin(), finalBlock.end(), L'\n', L' ');
   // }

    // addToHistory(finalBlock); // Opțional, poți reactiva dacă vrei history
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
/*
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
*/
std::vector<std::wstring> vOliEngine::preParse(const std::wstring& input) {
    std::vector<std::wstring> result;
    std::wstring current;
    int blockDepth = 0;   // Pentru WHILE, IF, etc.
    int bracketDepth = 0; // Pentru { } și [ ]
    bool inQuotes = false;

    for (size_t i = 0; i < input.length(); ++i) {
        wchar_t c = input[i];

        // 1. Gestionare ghilimele (ignorăm tot ce e în string-uri)
        if (c == L'"' && (i == 0 || input[i - 1] != L'\\')) {
            inQuotes = !inQuotes;
        }

        if (!inQuotes) {
            // 2. Tracking paranteze (pentru Map-uri și Array-uri multi-line)
            if (c == L'{' || c == L'[') bracketDepth++;
            if (c == L'}' || c == L']') bracketDepth--;

            // 3. Tracking blocuri de control (START)
            // Verificăm dacă suntem la începutul unui cuvânt
            bool isStartOfWord = (i == 0 || iswspace(input[i - 1]) || input[i - 1] == L';');
            if (isStartOfWord) {
                std::wstring_view remView(&input[i], input.length() - i);
                std::wstring startPrefix;
                for (size_t j = 0; j < 10 && j < remView.size(); ++j)
                    startPrefix += std::towupper(remView[j]);

                auto startsWithKey = [&](const std::wstring& k) {
                    if (startPrefix.size() < k.size()) return false;
                    if (startPrefix.substr(0, k.size()) != k) return false;
                    return (startPrefix.size() == k.size() || iswspace(startPrefix[k.size()]) || startPrefix[k.size()] == L';');
                    };

                if (startsWithKey(L"WHILE") || startsWithKey(L"REPEAT") || startsWithKey(L"IF") ||
                    startsWithKey(L"FOR") || startsWithKey(L"CYCLE") || startsWithKey(L"SWITCH") ||
                    startsWithKey(L"PROC") || startsWithKey(L"FUNC")) {
                    blockDepth++;
                }
            }

            // 4. DECIZIA DE TĂIERE (Separator)
            // Tăiem instrucțiunea DOAR dacă nu suntem în interiorul niciunui bloc sau paranteze
            if (blockDepth == 0 && bracketDepth == 0 && (c == L';' || c == L'\n')) {
                std::wstring cmd = trim(current);
                if (!cmd.empty()) result.push_back(cmd);
                current.clear();
                continue;
            }
        }

        current += c;

        // 5. Tracking blocuri de control (END)
        if (!inQuotes) {
            auto endsWithKey = [&](const std::wstring& k) {
                if (current.length() < k.length()) return false;
                std::wstring tail = current.substr(current.length() - k.length());
                for (auto& ch : tail) ch = std::towupper(ch);
                if (tail != k) return false;

                size_t startIdx = current.length() - k.length();
                if (startIdx > 0 && !iswspace(current[startIdx - 1]) && current[startIdx - 1] != L';') return false;
                return true;
                };

            if (endsWithKey(L"ENDWHILE") || endsWithKey(L"ENDREPEAT") || endsWithKey(L"ENDIF") ||
                endsWithKey(L"ENDFOR") || endsWithKey(L"ENDCYCLE") || endsWithKey(L"ENDSWITCH") ||
                endsWithKey(L"ENDPROC") || endsWithKey(L"ENDFUNC")) {
                blockDepth--;
                if (blockDepth < 0) blockDepth = 0;
            }
        }
    }

    // Adăugăm și ultima bucată dacă a mai rămas ceva
    std::wstring lastCmd = trim(current);
    if (!lastCmd.empty()) result.push_back(lastCmd);

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
        m_commandHandlers[L"HELP"] = wrap([this](const auto& sc) {  handleHelpCommand(sc);  });
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
    
    /*
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
    */
    void vOliEngine::handleSetCommand(const ShellCommand& sc) {
        if (sc.args.empty()) return;

        // 1. Reconstituim linia (fără a forța "global ")
        std::wstring fullLine;
        for (const auto& a : sc.args) fullLine += a + L" ";
        fullLine = trim(fullLine);

        // 2. Tokenizăm și Parsăm direct
        auto tokens = vOliCommandParser::tokenize(fullLine);
        if (tokens.empty()) return;

        OliExpressionParser exprParser(tokens);
        ASTPtr root = exprParser.parseAssignment();

        if (!root) {
            LOG_ERROR(L"[RUNTIME ERROR] Invalid assignment expression.");
            return;
        }

        // 3. EXECUȚIA
        // În interiorul executeAST, când ajungi la nodul de Assignment, 
        // acesta va chema assignToVariable(leftNode->name, evaluatedRight).
        // Acolo, prefixul '@' din "@$tinta_nume" va fi procesat corect.
        try {
            executeAST(root);
        }
        catch (...) {
            LOG_ERROR(L"Eroare la execuția AST-ului de asignare.");
        }
    }


    std::wstring vOliEngine::getVariantTypeName(const vData& data) {
        if (std::holds_alternative<long long>(data.value))      return L"INT";
        if (std::holds_alternative<double>(data.value))         return L"FLOAT";
        if (std::holds_alternative<std::wstring>(data.value))   return L"STRING";
        if (std::holds_alternative<bool>(data.value))           return L"BOOL";
        if (std::holds_alternative<vDataArray>(data.value))     return L"ARRAY";

        // --- ADAUGĂM SUPORTUL PENTRU POINTERI ---
        if (std::holds_alternative<vData*>(data.value))         return L"POINTER";

        if (std::holds_alternative<vDataMap>(data.value)) {
            auto& mPtr = std::get<vDataMap>(data.value);

            // Dacă e un Map, verificăm dacă are „buletin” (câmpul __type__)
            if (mPtr && mPtr->count(L"__type__")) {
                return vDataToWString((*mPtr)[L"__type__"]);
            }
            return L"MAP";
        }

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
    

    vData vOliEngine::resolveVariable(const std::wstring& rawVar) {
        std::wstring varName = trim(rawVar);
        if (varName.empty()) return { std::monostate{} };

        // --- STRATUL 1: DEREFERENȚIERE (*) ---
        if (varName[0] == L'*') {
            // 1. Rezolvăm recursiv ce se află după '*'
            vData pointerVar = resolveVariable(varName.substr(1));

            // 2. Încercăm să obținem adresa folosind std::get_if.
            // Acesta returnează un pointer către valoarea din variantă (vData**) 
            // sau nullptr dacă tipul din variantă nu este cel cerut.
            if (vData** addrPtr = std::get_if<vData*>(&pointerVar.value)) {
                vData* actualAddr = *addrPtr; // Extragem adresa stocată (vData*)

                if (actualAddr) {
                    return *actualAddr; // Succes! Returnăm valoarea de la acea adresă
                }
                else {
                    LOG_ERROR(L"Runtime Error: Dereferencing a NULL pointer!");
                }
            }
            else {
                // Dacă am ajuns aici, înseamnă că variabila nu conține un pointer (vData*)
                LOG_ERROR(L"Runtime Error: '" + varName + L"' is not a pointer (Type mismatch).");
            }

            return { std::monostate{} };
        }

        // --- STRATUL 2: ACCES GLOBAL (@) ---
        if (varName[0] == L'@') {
            std::wstring content = varName.substr(1);

            // Cazul reflexiv: @$tinta (numele variabilei e într-o variabilă locală)
            if (!content.empty() && content[0] == L'$') {
                vData evaluatedName = resolveVariable(content);
                if (const std::wstring* nameStr = std::get_if<std::wstring>(&evaluatedName.value)) {
                    auto it = m_globalVariables.find(*nameStr);
                    if (it != m_globalVariables.end()) return it->second;
                }
                return { std::monostate{} };
            }

            // Cazul standard: @nume_global
            size_t firstSep = content.find_first_of(L".[");
            std::wstring root = (firstSep != std::wstring::npos) ? content.substr(0, firstSep) : content;

            auto it = m_globalVariables.find(trim(root));
            return (it != m_globalVariables.end()) ? it->second : vData{ std::monostate{} };
        }

        // --- STRATUL 3: INDIRECȚIE DINAMICĂ ($$, $$$) ---
        if (varName[0] == L'$') {
            size_t dollarCount = 0;
            while (dollarCount < varName.size() && varName[dollarCount] == L'$')
                dollarCount++;

            // Pas 1: Rezolvăm numele de bază (ex: pentru $$$a, rezolvăm mai întâi "a")
            vData result = resolveVariable(varName.substr(dollarCount));

            // Pas 2: Aplicăm restul de semne $ ca niște căutări succesive
            for (size_t i = 1; i < dollarCount; ++i) {
                if (const std::wstring* s = std::get_if<std::wstring>(&result.value)) {
                    result = resolveVariable(*s);
                }
                else {
                    break;
                }
            }
            return result;
        }

        // --- STRATUL 4: SCOPING NORMAL (Local -> Global) ---
        std::wstring root = varName;
        size_t sep = root.find_first_of(L".[");
        if (sep != std::wstring::npos) root = root.substr(0, sep);
        root = trim(root);

        // 1. Căutăm în Frame-ul local (Stivă)
        if (!m_callStack.empty()) {
            auto& locals = m_callStack.back().localVariables;
            auto it = locals.find(root);
            if (it != locals.end()) return it->second;
        }

        // 2. Căutăm în Globale
        auto itGlobal = m_globalVariables.find(root);
        if (itGlobal != m_globalVariables.end()) return itGlobal->second;

        return { std::monostate{} };
    }
    

    void vOliEngine::printVData(const vData& data, bool debugMode) {
        if (data.isNull()) {
            std::wcout << L"~";
            return;
        }

        std::visit([this, debugMode](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, long long>) {
                std::wcout << arg;
            }
            else if constexpr (std::is_same_v<T, double>) {
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
                // ARG este std::shared_ptr<std::vector<vData>>
                if (!arg) {
                    std::wcout << L"[]";
                    return;
                }
                std::wcout << L"[";
                for (size_t i = 0; i < arg->size(); ++i) { // Folosim ->size()
                    printVData((*arg)[i], debugMode);      // Dereferențiem (*arg)[i]
                    if (i < arg->size() - 1) std::wcout << L", ";
                }
                std::wcout << L"]";
            }
            else if constexpr (std::is_same_v<T, vDataMap>) {
                // ARG este std::shared_ptr<std::map<...>>
                if (!arg) {
                    std::wcout << L"{}";
                    return;
                }
                std::wcout << L"{";
                // Iterăm prin obiectul real folosind *arg
                for (auto it = arg->begin(); it != arg->end(); ++it) {
                    std::wcout << L"\"" << it->first << L"\": ";
                    printVData(it->second, debugMode);
                    if (std::next(it) != arg->end()) std::wcout << L", ";
                }
                std::wcout << L"}";
            }
            else if constexpr (std::is_same_v<T, std::monostate>) {
                std::wcout << L"null";
            }
            }, data.value);
    }
    /*
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
    */

    void vOliEngine::handleDumpMemCommand(const ShellCommand& sc) {
        if (m_globalVariables.empty()) {
            LOG_INFO(L"Memory is empty. No variables set.");
            return;
        }

        LOG_RAW(L"\n" + std::wstring(75, L'='));
        LOG_RAW(L"   OLI ENGINE - VIRTUAL RAM ARCHITECTURE (SINGLE-LINE DUMP)");
        LOG_RAW(std::wstring(75, L'='));

        // Cap de tabel
        std::wstringstream header;
        header << std::left << std::setw(15) << L"VARIABLE"
            << std::setw(12) << L"TYPE"
            << std::setw(20) << L"MEMORY_REF"
            << L"VALUE";
        LOG_RAW(header.str());
        LOG_RAW(std::wstring(75, L'-'));

        for (const auto& [name, data] : m_globalVariables) {
            std::wstringstream line;

            // 1. Numele și Tipul
            line << std::left << std::setw(15) << name
                << std::setw(12) << getVariantTypeName(data);

            // 2. Adresa (MEMORY_REF)
            if (auto* p = std::get_if<vData*>(&data.value)) {
                wchar_t buf[32];
                swprintf(buf, 32, L"0x%p", (void*)*p);
                line << std::setw(20) << buf;
            }
            else if (auto* m = std::get_if<vDataMap>(&data.value)) {
                wchar_t buf[32];
                swprintf(buf, 32, L"M:0x%p", (void*)m->get());
                line << std::setw(20) << buf;
            }
            else if (auto* a = std::get_if<vDataArray>(&data.value)) {
                wchar_t buf[32];
                swprintf(buf, 32, L"A:0x%p", (void*)a->get());
                line << std::setw(20) << buf;
            }
            else {
                line << std::setw(20) << L"[INTERNAL]";
            }

            // 3. Valoarea (Lipiți valoarea direct pe același rând)
            line << vDataToWString(data);

            // Trimitem un singur log pe rând
            LOG_RAW(line.str());
        }

        LOG_RAW(std::wstring(75, L'='));
        LOG_INFO(L"Total Variables in Heap: " + std::to_wstring(m_globalVariables.size()));
        LOG_RAW(std::wstring(75, L'=') + L"\n");
    }

    vData vOliEngine::parseArrayContent(const std::wstring& content) {
        bool containsColon = false;
        bool inQuotes = false;
        for (wchar_t c : content) {
            if (c == L'"') inQuotes = !inQuotes;
            if (c == L':' && !inQuotes) { containsColon = true; break; }
        }

        if (containsColon) {
            // 1. Inițializăm pointerul către un Map nou în HEAP
            vDataMap mapResult = std::make_shared<std::map<std::wstring, vData>>();

            auto pairs = splitByCommaIgnoringBrackets(content);
            for (const auto& p : pairs) {
                size_t colonPos = p.find(L':');
                if (colonPos != std::wstring::npos) {
                    std::wstring k = normalizeSpaces(p.substr(0, colonPos));
                    if (k.size() >= 2 && k.front() == L'"' && k.back() == L'"')
                        k = k.substr(1, k.size() - 2);

                    std::wstring v = p.substr(colonPos + 1);

                    // 2. Dereferențiem pointerul (*) pentru a folosi operatorul []
                    (*mapResult)[k] = evaluateExpression(v);
                }
            }
            return { mapResult }; // Returnăm vDataValue care conține shared_ptr-ul
        }
        else {
            // 3. Inițializăm pointerul către un Vector nou în HEAP
            vDataArray arrResult = std::make_shared<std::vector<vData>>();

            auto elements = splitByCommaIgnoringBrackets(content);
            for (const auto& e : elements) {
                // 4. Folosim -> pentru a apela metoda push_back a vectorului de la adresă
                arrResult->push_back(evaluateExpression(e));
            }
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
                swprintf(buf, 64, L"%g", arg);
                return std::wstring(buf);
            }
            else if constexpr (std::is_same_v<T, std::wstring>) {
                return arg;
            }
            else if constexpr (std::is_same_v<T, bool>) {
                return arg ? L"true" : L"false";
            }
            else if constexpr (std::is_same_v<T, vData*>) {
                // --- GESTIONARE POINTER REAL ---
                if (arg == nullptr) return L"[PTR: NULL]";
                // Încercăm să arătăm adresa hexazecimală
                wchar_t buf[32];
                swprintf(buf, 32, L"[PTR: 0x%p]", (void*)arg);
                return std::wstring(buf);
            }
            else if constexpr (std::is_same_v<T, vDataArray>) {
                if (!arg) return L"[]";
                std::wstring res = L"[";
                for (size_t i = 0; i < arg->size(); ++i) {
                    res += this->vDataToWString((*arg)[i]);
                    if (i < arg->size() - 1) res += L", ";
                }
                res += L"]";
                return res;
            }
            else if constexpr (std::is_same_v<T, vDataMap>) {
                if (!arg) return L"{}";
                std::wstring res = L"{";
                size_t i = 0;
                for (auto const& [key, val] : *arg) {
                    res += key + L": " + this->vDataToWString(val);
                    if (++i < arg->size()) res += L", ";
                }
                res += L"}";
                return res;
            }
            else {
                return L"(UNKNOWN TYPE)";
            }
            }, data.value);
    }
    
    
    void vOliEngine::assignToVariable(const std::wstring& varExpr, const vData& newValue) {
        std::wstring trimmedExpr = trim(varExpr);

        // 1. Gestionare Prefix Global (@)
        // Dacă expresia începe cu @, forțăm contextul global și eliminăm simbolul
        bool forceGlobal = (!trimmedExpr.empty() && trimmedExpr[0] == L'@');
        if (forceGlobal) trimmedExpr.erase(0, 1);

        // 2. Separare Rădăcină de Cale (ex: $erou.poz.x -> rootPart: $erou, path: .poz.x)
        size_t firstSep = trimmedExpr.find_first_of(L"[.");
        std::wstring rootPart = (firstSep == std::wstring::npos) ? trimmedExpr : trimmedExpr.substr(0, firstSep);
        std::wstring pathRemainder = (firstSep == std::wstring::npos) ? L"" : trimmedExpr.substr(firstSep);

        // 3. LOGICA DE REFLEXIE (EVALUARE DINAMICĂ)
        // Dacă rootPart conține $, rezolvăm ce e înăuntru (ex: $tinta_nume -> "erou")
        if (rootPart.find(L'$') != std::wstring::npos) {
            vData resolved = resolveVariable(rootPart);
            if (auto* pStr = std::get_if<std::wstring>(&resolved.value)) {
                rootPart = *pStr;
                LOG_DEBUG(L"[REFLEXIE] Nume real determinat din variabila: " + rootPart);
            }
        }

        // --- PASUL CRITIC: CURĂȚARE SIGIL ($) ---
        // În m_globalVariables, cheile sunt salvate ca "erou", nu ca "$erou".
        // Dacă rootPart a rămas cu $ (fie din reflexie, fie din trim), îl eliminăm.
        if (!rootPart.empty() && rootPart[0] == L'$') {
            rootPart.erase(0, 1);
        }

        // 4. OBȚINERE POINTER CĂTRE VARIABILĂ (L-Value)
        vData* rootPtr = nullptr;
        if (forceGlobal || m_callStack.empty()) {
            rootPtr = &m_globalVariables[rootPart];
        }
        else {
            auto& locals = m_callStack.back().localVariables;
            // Shadowing logic: dacă există local, îl folosim, altfel mergem pe global
            rootPtr = (locals.count(rootPart)) ? &locals[rootPart] : &m_globalVariables[rootPart];
        }

        // 5. NAVIGARE PRIN PROPRIETĂȚI (ex: .poz.x)
        // navigateOrCreatePath va returna pointerul către vData care conține "poz" (părintele lui "x")
        vData* target = navigateOrCreatePath(rootPtr, pathRemainder);

        if (target) {
            // Extragem field-ul final (ex: "x" din "$erou.poz.x")
            size_t lastSep = trimmedExpr.find_last_of(L".[");
            std::wstring field = (lastSep == std::wstring::npos) ? rootPart : trimmedExpr.substr(lastSep + 1);
            if (!field.empty() && field.back() == L']') field.pop_back();

            // 6. ACTUALIZARE VALOARE ÎN HEAP
            if (auto* pMapPtr = std::get_if<vDataMap>(&target->value)) {
                // Dacă pointerul partajat este null (neinițializat), creăm obiectul acum
                if (!(*pMapPtr)) *pMapPtr = std::make_shared<std::map<std::wstring, vData>>();

                // (**pMapPtr) dereferențiază pointerul brut, apoi shared_ptr-ul pentru a scrie în Map
                (**pMapPtr)[field] = newValue;
                LOG_DEBUG(L"[SUCCESS] Actualizat campul '" + field + L"' pentru variabila: " + rootPart);
            }
            else {
                // Auto-vivificare: dacă nu era Map, îl transformăm într-unul
                vData newMapObj = vData::CreateMap();
                auto mPtr = std::get<vDataMap>(newMapObj.value);
                (*mPtr)[field] = newValue;

                target->value = newMapObj.value;
                LOG_DEBUG(L"[SUCCESS] Creat ierarhie noua pentru: " + rootPart + L"." + field);
            }
        }
    }

    void vOliEngine::assignToArrayVar(vData* container, const std::wstring& indexExpr, const vData& newValue) {
        // 1. Extragem shared_ptr-ul (telecomanda) din variant
        auto& arrPtr = std::get<vDataArray>(container->value);

        // 2. Verificăm dacă pointerul este valid (dacă nu, îl inițializăm)
        if (!arrPtr) {
            arrPtr = std::make_shared<std::vector<vData>>();
        }

        if (indexExpr.empty()) {
            // Cazul set a[] = val -> Folosim -> pentru a ajunge la vectorul de pe heap
            arrPtr->push_back(newValue);
        }
        else {
            // Cazul set a[$i] = val
            vData idxVal = evaluateExpression(indexExpr);
            long long rawIdx = vDataToLong(idxVal);

            if (rawIdx < 0) return;

            size_t idx = static_cast<size_t>(rawIdx);

            // 3. Folosim -> pentru metodele vectorului (size, resize)
            if (idx >= arrPtr->size()) {
                arrPtr->resize(idx + 1, vData{ std::monostate{} });
            }

            // 4. Folosim (*arrPtr)[idx] sau arrPtr->at(idx) pentru accesul la element
            (*arrPtr)[idx] = newValue;
        }
    }

    void vOliEngine::assignToMapVar(vData* container, const std::wstring& indexExpr, const vData& newValue) {
        // 1. Verificăm dacă nu e Map SAU dacă pointerul este null
        if (!std::holds_alternative<vDataMap>(container->value) || !std::get<vDataMap>(container->value)) {
            // Inițializăm pointerul cu un Map nou pe HEAP
            container->value = std::make_shared<std::map<std::wstring, vData>>();
        }

        // 2. Obținem referința la shared_ptr (telecomanda)
        auto& mPtr = std::get<vDataMap>(container->value);

        // 3. Evaluăm cheia (rămâne neschimbat)
        vData keyVal = evaluateExpression(indexExpr);
        std::wstring key = vDataToWString(keyVal);

        // 4. OPERAȚIA CRITICĂ: Dereferențiem pointerul pentru a folosi []
        // Folosim (*mPtr) pentru a accesa Map-ul real de la adresa indicată
        (*mPtr)[key] = newValue;
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

        std::vector<std::wstring> tokens = splitPath(varExpr);
        if (tokens.empty()) return root;

        vData* current = root;

        for (size_t i = 0; i < tokens.size() - 1; ++i) {
            std::wstring key = tokens[i];
            if (key.empty()) continue;

            if (key.size() >= 2 && key.front() == L'\"' && key.back() == L'\"') {
                key = key.substr(1, key.size() - 2);
            }

            // 1. AUTO-INITIALIZARE (Folosim noile metode statice)
            if (current->isNull()) {
                if (!key.empty() && std::iswdigit(key[0]))
                    current->value = vData::CreateArray().value;
                else
                    current->value = vData::CreateMap().value;
            }

            // 2. NAVIGARE ÎN MAP
            if (current->isMap()) {
                auto& mapPtr = std::get<vDataMap>(current->value);
                // Dacă pointerul e null dintr-un motiv oarecare, îl creăm
                if (!mapPtr) mapPtr = std::make_shared<std::map<std::wstring, vData>>();

                // Returnăm adresa vData-ului din interiorul map-ului real (*mapPtr)
                current = &((*mapPtr)[key]);
            }
            // 3. NAVIGARE ÎN ARRAY
            else if (current->isArray()) {
                try {
                    size_t idx = static_cast<size_t>(std::stoll(key));
                    auto& arrPtr = std::get<vDataArray>(current->value);
                    if (!arrPtr) arrPtr = std::make_shared<std::vector<vData>>();

                    // Folosim -> pentru a accesa metodele vectorului de pe heap
                    if (idx >= arrPtr->size()) {
                        arrPtr->resize(idx + 1, vData{ std::monostate{} });
                    }
                    // Returnăm adresa vData-ului de la indexul respectiv
                    current = &((*arrPtr)[idx]);
                }
                catch (...) { return nullptr; }
            }
            else {
                return nullptr;
            }
        }

        return current;
    }

    vData* vOliEngine::getOrCreateContainer(vData* root, const std::wstring& indexExpr, bool isNextBracketArray) {
        std::wstring cleanIdx = normalizeSpaces(indexExpr);

        // 1. Index GOL [] -> Adăugare la final
        if (cleanIdx.empty()) {
            // Inițializăm Array-ul dacă nu există (folosind metoda statică de creare)
            if (!root->isArray()) root->value = vData::CreateArray().value;

            auto& arrPtr = std::get<vDataArray>(root->value);
            if (!arrPtr) arrPtr = std::make_shared<std::vector<vData>>(); // Double safety

            vData newValue;
            if (isNextBracketArray) newValue.value = vData::CreateArray().value;
            else newValue.value = vData::CreateMap().value;

            arrPtr->push_back(std::move(newValue));
            return &(arrPtr->back()); // Returnăm adresa ultimului element din vectorul de pe heap
        }

        vData indexValue = evaluateExpression(cleanIdx);

        // 2. Index Numeric -> ARRAY
        if (indexValue.isInt() || indexValue.isFloat()) {
            if (!root->isArray()) root->value = vData::CreateArray().value;

            auto& arrPtr = std::get<vDataArray>(root->value);
            if (!arrPtr) arrPtr = std::make_shared<std::vector<vData>>();

            size_t idx = (size_t)vDataToLong(indexValue);
            if (idx >= arrPtr->size()) {
                arrPtr->resize(idx + 1, vData{ std::monostate{} });
            }

            vData& target = (*arrPtr)[idx]; // Accesăm elementul din vectorul real
            if (target.isNull()) {
                if (isNextBracketArray) target.value = vData::CreateArray().value;
                else target.value = vData::CreateMap().value;
            }
            return &target;
        }
        // 3. Index String -> MAP
        else {
            if (!root->isMap()) root->value = vData::CreateMap().value;

            auto& mPtr = std::get<vDataMap>(root->value);
            if (!mPtr) mPtr = std::make_shared<std::map<std::wstring, vData>>();

            std::wstring key = vDataToWString(indexValue);

            // Dereferențiem mPtr pentru a folosi operatorul [] pe map-ul real
            vData& target = (*mPtr)[key];
            if (target.isNull()) {
                if (isNextBracketArray) target.value = vData::CreateArray().value;
                else target.value = vData::CreateMap().value;
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

        m_functionsHandlers[L"REF"] = [this](const std::vector<vData>& args) -> vData {
            if (args.empty()) return { std::monostate{} };

            // Avem nevoie de numele variabilei (ex: REF("a"))
            std::wstring varName = vDataToWString(args[0]);
            if (varName[0] == L'$') varName.erase(0, 1);

            // Căutăm variabila în memorie (Global sau Local)
            vData* targetPtr = nullptr;
            if (!m_callStack.empty()) {
                auto& locals = m_callStack.back().localVariables;
                if (locals.count(varName)) targetPtr = &locals[varName];
            }

            if (!targetPtr && m_globalVariables.count(varName)) {
                targetPtr = &m_globalVariables[varName];
            }

            if (targetPtr) {
                vData refResult;
                refResult.value = targetPtr; // Stocăm adresa brută
                return refResult;
            }

            return { std::monostate{} };
            };


        m_functionsHandlers[L"CLONE"] = [this](const std::vector<vData>& args) -> vData {
            if (args.empty()) {
                LOG_ERROR(L"[RUNTIME ERROR] CLONE() necesita un argument.");
                return { std::monostate{} };
            }

            // Deoarece handler-ul primeste deja argumentele evaluate,
            // pur si simplu trimitem primul argument catre deepCopy.
            return this->deepCopy(args[0]);
            };

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

            // Cazul ARRAY
            if (d.isArray()) {
                auto arrPtr = std::get<vDataArray>(d.value);
                // Folosim ->size() pe pointerul partajat
                return { static_cast<long long>(arrPtr ? arrPtr->size() : 0) };
            }

            // Cazul MAP
            if (d.isMap()) {
                auto mapPtr = std::get<vDataMap>(d.value);
                return { static_cast<long long>(mapPtr ? mapPtr->size() : 0) };
            }

            // Cazul STRING (rămâne neschimbat, wstring nu e pointer)
            if (d.isString()) {
                return { static_cast<long long>(std::get<std::wstring>(d.value).size()) };
            }

            return { 0LL };
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

        m_functionsHandlers[L"FLOAT"] = [this](const std::vector<vData>& args) -> vData {
            return this->handleFloatFunc(args);
            };
        
        m_functionsHandlers[L"STR"] = [this](const std::vector<vData>& args) -> vData {
            return this->handleStrFunc(args);
        };
        m_functionsHandlers[L"STRING"] = m_functionsHandlers[L"STR"];

        m_functionsHandlers[L"ARRAY"] = [this](const auto& args) { return handleArrayFunc(args); };

        m_functionsHandlers[L"MAP"] = [this](const auto& args) { return handleMapFunc(args); };

        m_functionsHandlers[L"TRIM"] = [this](const auto& args) { return handleTrimFunc(args); };

        m_functionsHandlers[L"SPLIT"] = [this](const auto& args) {return this->handleSplitFunc(args); };
        m_functionsHandlers[L"JOIN"] = [this](const auto& args) {return this->handleJoinFunc(args); };

    }

    vData vOliEngine::executeAST(ASTPtr node) {
        if (!node) return { std::monostate{} };

        try {
            switch (node->type) {
            case ASTNodeType::Literal: {
                // --- CREARE ARRAY DIN COD: [1, 2, 3] ---
                if (node->value == L"ARRAY_OBJECT") {
                    vDataArray elements = std::make_shared<std::vector<vData>>();
                    for (auto& child : node->children) {
                        if (child) elements->push_back(executeAST(child));
                    }
                    return { elements };
                }

                // --- CREARE MAP DIN COD: { "a": 1 } ---
                if (node->value == L"MAP_OBJECT") {
                    vDataMap myMap = std::make_shared<std::map<std::wstring, vData>>();
                    for (size_t i = 0; i + 1 < node->children.size(); i += 2) {
                        vData keyData = executeAST(node->children[i]);
                        vData valData = executeAST(node->children[i + 1]);
                        (*myMap)[vDataToWString(keyData)] = valData;
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
                // resolveVariable gestionează acum și citirea prin pointer (*$ptr)
                return resolveVariable(node->value);
            }

            case ASTNodeType::FunctionCall: {
                std::wstring funcName;
                std::vector<vData> evaluatedArgs;
                vData contextObj = { std::monostate{} };

                // 1. IDENTIFICARE CONTEXT ȘI NUME
                if (!node->children.empty() && (node->children[0]->value == L"." || node->children[0]->value == L"DOT")) {
                    const ASTPtr& dotNode = node->children[0];
                    if (dotNode && !dotNode->children.empty()) {
                        contextObj = executeAST(dotNode->children[0]);
                        funcName = vDataToWString(executeAST(dotNode));
                        for (size_t i = 1; i < node->children.size(); ++i)
                            evaluatedArgs.push_back(executeAST(node->children[i]));
                    }
                }
                else if (node->value == L"DYNAMIC_CALL" && !node->children.empty()) {
                    funcName = vDataToWString(executeAST(node->children[0]));
                    for (size_t i = 1; i < node->children.size(); ++i)
                        evaluatedArgs.push_back(executeAST(node->children[i]));
                }
                else if (!node->value.empty() && node->value[0] == L'$') {
                    funcName = vDataToWString(resolveVariable(node->value));
                    for (auto& child : node->children) evaluatedArgs.push_back(executeAST(child));
                }
                else {
                    funcName = node->value;
                    for (auto& child : node->children) evaluatedArgs.push_back(executeAST(child));
                }

                if (funcName.empty()) return { std::monostate{} };

                // 2. CONSTRUCTORI (Blueprints)
                auto itBlueprint = m_blueprints.find(funcName);
                if (itBlueprint != m_blueprints.end()) {
                    vDataMap instance = std::make_shared<std::map<std::wstring, vData>>();
                    (*instance)[L"__type__"] = vData(itBlueprint->second.name);
                    const auto& fields = itBlueprint->second.fields;
                    for (size_t i = 0; i < fields.size(); ++i) {
                        (*instance)[fields[i]] = (i < evaluatedArgs.size()) ? evaluatedArgs[i] : vData{ std::monostate{} };
                    }
                    return { instance };
                }

                // 3. APELARE (Intern/User)
                std::wstring upperName = funcName;
                std::transform(upperName.begin(), upperName.end(), upperName.begin(), ::towupper);

                auto itInternal = m_functionsHandlers.find(upperName);
                if (itInternal != m_functionsHandlers.end()) return itInternal->second(evaluatedArgs);

                auto itUser = m_userFunctions.find(upperName);
                if (itUser != m_userFunctions.end()) return callUserFunction(upperName, evaluatedArgs, contextObj);

                LOG_ERROR(L"[RUNTIME ERROR] Unknown function: " + funcName);
                return { std::monostate{} };
            }

            case ASTNodeType::Operator: {
                // --- ATRIBUIRE (=) ---
                
                // --- ATRIBUIRE (=) ---
                if (node->value == L"=") {
                    if (node->children.size() < 2) return { std::monostate{} };

                    ASTPtr left = node->children[0];
                    vData rightVal = executeAST(node->children[1]);

                    // 1. CAZUL SIMPLE: Variabilă sau Dereferențiere directă (set *$ptr = 10)
                    if (left->type == ASTNodeType::Variable) {
                        std::wstring targetName = left->value;

                        if (!targetName.empty() && targetName[0] == L'*') {
                            vData ptrInfo = resolveVariable(targetName.substr(1));
                            if (vData** addrPtr = std::get_if<vData*>(&ptrInfo.value)) {
                                if (*addrPtr) {
                                    **addrPtr = rightVal;
                                    return rightVal;
                                }
                            }
                            LOG_ERROR(L"Runtime Error: Pointer invalid la atribuire: " + targetName);
                            return { std::monostate{} };
                        }

                        setVariable(targetName, rightVal);
                        return rightVal;
                    }

                    // 2. CAZUL COMPLEX: Atribuire în câmp/index (set (*$ptr).val = 110)
                    // În loc de reconstructPath, evaluăm "stânga" punctului pentru a obține obiectul real
                    if (left->value == L"DOT" || left->value == L".") {
                        vData container = executeAST(left->children[0]); // <--- Aici rezolvăm (*$ptr)
                        std::wstring field = left->children[1]->value;

                        if (container.isMap()) {
                            auto mapPtr = std::get<vDataMap>(container.value);
                            if (mapPtr) {
                                (*mapPtr)[field] = rightVal; // Scriem direct în memoria obiectului
                                return rightVal;
                            }
                        }
                    }

                    // 3. CAZUL INDEXARE: (set (*$ptr)[0] = "nou")
                    if (left->value == L"INDEX" || left->value == L"[") {
                        vData container = executeAST(left->children[0]);
                        vData index = executeAST(left->children[1]);

                        if (container.isArray()) {
                            auto arrPtr = std::get<vDataArray>(container.value);
                            size_t idx = static_cast<size_t>(vDataToDouble(index));
                            if (arrPtr && idx < arrPtr->size()) {
                                (*arrPtr)[idx] = rightVal;
                                return rightVal;
                            }
                        }
                        else if (container.isMap()) {
                            auto mapPtr = std::get<vDataMap>(container.value);
                            if (mapPtr) {
                                (*mapPtr)[vDataToWString(index)] = rightVal;
                                return rightVal;
                            }
                        }
                    }

                    // Fallback pentru căi care nu implică pointeri (ex: @global.prop)
                    std::wstring fullPath = reconstructPath(left);
                    if (!fullPath.empty()) {
                        assignToVariable(fullPath, rightVal);
                        return rightVal;
                    }

                    throw std::runtime_error("L-value required for assignment.");
                }

                // --- DEREFERENȚIERE EXPLICITĂ (*) ---
                if (node->value == L"DEREFERENCE") {
                    vData ptr = executeAST(node->children[0]);

                    // Folosim get_if pentru a fi consistenți și siguri
                    if (vData** addrPtr = std::get_if<vData*>(&ptr.value)) {
                        vData* addr = *addrPtr;
                        if (addr) {
                            return *addr;
                        }
                        else {
                            LOG_ERROR(L"Runtime Error: Dereferencing a NULL pointer in expression.");
                        }
                    }
                    else {
                        // Dacă utilizatorul scrie ceva de genul *("text"), motorul nu crapă, ci raportează:
                        LOG_ERROR(L"Runtime Error: Cannot dereference a non-pointer value.");
                    }

                    return { std::monostate{} };
                }

                // --- ACCES (DOT / INDEX) ---
                if (node->value == L"DOT" || node->value == L".") {
                    if (node->children.size() < 2) return { std::monostate{} };
                    vData container = executeAST(node->children[0]);
                    std::wstring field = node->children[1]->value;
                    if (container.isMap()) {
                        auto mapPtr = std::get<vDataMap>(container.value);
                        if (mapPtr && mapPtr->count(field)) return (*mapPtr).at(field);
                    }
                    return { std::monostate{} };
                }

                if (node->value == L"INDEX" || node->value == L"[") {
                    if (node->children.size() < 2) return { std::monostate{} };
                    vData container = executeAST(node->children[0]);
                    vData index = executeAST(node->children[1]);
                    return accessContainer(container, index);
                }

                // --- BINAR & UNAR ---
                if (node->children.size() >= 2) {
                    vData lhs = executeAST(node->children[0]);
                    vData rhs = executeAST(node->children[1]);
                    return executeBinaryOperator(node->value, lhs, rhs);
                }

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

        vData* pContainer = nullptr;

        // 1. Căutăm containerul (Rămâne neschimbat)
        if (!m_callStack.empty()) {
            auto& locals = m_callStack.back().localVariables;
            if (locals.count(varName)) pContainer = &locals[varName];
        }
        if (!pContainer && m_globalVariables.count(varName)) {
            pContainer = &m_globalVariables[varName];
        }

        if (!pContainer) {
            LOG_ERROR(L"[RUNTIME ERROR] Variable '" + varName + L"' not found.");
            return;
        }

        // 2. Modificăm conținutul folosind pointerii partajați

        // --- CAZUL MAP ---
        if (pContainer->isMap()) {
            // Extragem shared_ptr-ul (telecomanda)
            auto& mapPtr = std::get<vDataMap>(pContainer->value);

            // Verificăm dacă map-ul există pe heap
            if (!mapPtr) mapPtr = std::make_shared<std::map<std::wstring, vData>>();

            std::wstring k = vDataToWString(key);
            // Dereferențiem (*mapPtr) pentru a folosi operatorul []
            (*mapPtr)[k] = newValue;
        }

        // --- CAZUL ARRAY ---
        else if (pContainer->isArray()) {
            auto& arrPtr = std::get<vDataArray>(pContainer->value);

            if (!arrPtr) arrPtr = std::make_shared<std::vector<vData>>();

            size_t idx = static_cast<size_t>(vDataToLong(key));

            // Folosim -> pentru a accesa metodele vectorului de pe heap
            if (idx < arrPtr->size()) {
                (*arrPtr)[idx] = newValue;
            }
            else {
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
    // --- 1. OPERATORI DE COALESCENCE ---
    if (op == L"??") {
        return left.isNull() ? right : left;
    }

    // --- 2. LOGICĂ DE EGALITATE (Punctul critic pentru Pointeri) ---
    if (op == L"==") {
        // A. Verificăm dacă sunt ambii NULL (monostate)
        if (left.isNull() && right.isNull()) return { true };

        // B. LOGICĂ SPECIALĂ PENTRU POINTERI (vData*)
        bool leftIsPtr = std::holds_alternative<vData*>(left.value);
        bool rightIsPtr = std::holds_alternative<vData*>(right.value);

        if (leftIsPtr || rightIsPtr) {
            // Dacă ambii sunt pointeri, comparăm adresele de memorie brute
            if (leftIsPtr && rightIsPtr) {
                return { std::get<vData*>(left.value) == std::get<vData*>(right.value) };
            }

            // Un pointer real NU este egal cu NULL (monostate), decât dacă adresa e nullptr
            if (leftIsPtr && right.isNull()) return { std::get<vData*>(left.value) == nullptr };
            if (rightIsPtr && left.isNull()) return { std::get<vData*>(right.value) == nullptr };

            // Un pointer nu este egal cu un string sau un număr
            return { false };
        }

        // C. Logică standard pentru NULL vs restul
        if (left.isNull() || right.isNull()) return { false };

        // D. Comparație numerică (cu toleranță pentru float)
        if (canBeNumeric(left) && canBeNumeric(right)) {
            return { std::abs(vDataToDouble(left) - vDataToDouble(right)) < 1e-9 };
        }

        // E. Fallback: Comparație ca String
        return { vDataToWString(left) == vDataToWString(right) };
    }

    if (op == L"!=") {
        vData res = executeBinaryOperator(L"==", left, right);
        return vData(!vDataToBool(res));
    }

    // --- 3. ADUNAREA / CONCATENAREA ---
    if (op == L"+") {
        if (left.isString() || right.isString()) {
            return { vDataToWString(left) + vDataToWString(right) };
        }

        if (left.isInt() && right.isInt()) {
            return { std::get<long long>(left.value) + std::get<long long>(right.value) };
        }

        if (canBeNumeric(left) || left.isNull() || canBeNumeric(right) || right.isNull()) {
            double valL = left.isNull() ? 0.0 : vDataToDouble(left);
            double valR = right.isNull() ? 0.0 : vDataToDouble(right);
            return { valL + valR };
        }
        return { vDataToWString(left) + vDataToWString(right) };
    }

    // --- 4. BARIERĂ PENTRU OPERAȚII STRICTE ---
    // Pointers, Maps și Arrays nu pot participa la matematică directă (^, *, /, -)
    if (left.isNull() || right.isNull() ||
        std::holds_alternative<vData*>(left.value) ||
        std::holds_alternative<vData*>(right.value)) {
        return vData();
    }

    // --- 5. OPERAȚII NUMERICE ---
    if (canBeNumeric(left) && canBeNumeric(right)) {

        if (op == L"^" || op == L"**") {
            return { std::pow(vDataToDouble(left), vDataToDouble(right)) };
        }

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

        double dL = vDataToDouble(left);
        double dR = vDataToDouble(right);
        if (op == L"-") return { dL - dR };
        if (op == L"*") return { dL * dR };
        if (op == L"/") return std::abs(dR) > 1e-12 ? vData(dL / dR) : vData();

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

    return vData();
}

double vOliEngine::vDataToDouble(const vData& data) const {
    return std::visit([this](auto&& arg) -> double {
        using T = std::decay_t<decltype(arg)>;

        if constexpr (std::is_same_v<T, double>) {
            return arg;
        }
        else if constexpr (std::is_same_v<T, long long>) {
            return static_cast<double>(arg);
        }
        else if constexpr (std::is_same_v<T, bool>) {
            return arg ? 1.0 : 0.0;
        }
        else if constexpr (std::is_same_v<T, std::wstring>) {
            try {
                return std::stod(arg);
            }
            catch (...) {
                return 0.0;
            }
        }
        else if constexpr (std::is_same_v<T, vData*>) {
            // --- LOGICA PENTRU POINTERI REALI ---
            // Dacă vrei ca o variabilă pointer să fie tratată ca număr, 
            // trebuie să mergem la adresa indicată și să vedem ce e acolo.
            if (arg != nullptr) {
                return this->vDataToDouble(*arg); // Recursivitate: extragem numărul de la adresă
            }
            return 0.0;
        }
        else {
            // monostate (NULL), vDataArray, vDataMap
            return 0.0;
        }
        }, data.value);
}

    



    vData vOliEngine::parseRawLiteral(std::wstring_view val) {
        if (val.empty()) return { std::monostate{} };

        // 1. Verificare rapidă pentru null/bool fără a crea un string nou (lowVal)
        if (val.size() == 4) {
            // Verificăm "null" sau "true" case-insensitive manual sau cu o funcție helper
            if (iequals(val, L"null") || iequals(val, L"none")) return { std::monostate{} };
            if (iequals(val, L"true")) return { true };
        }
        if (val.size() == 5 && iequals(val, L"false")) {
            return { false };
        }

        // 2. Pentru conversia numerică (wcstod/wcstoll au nevoie de null-terminated string)
        // Aici avem două variante:

        // Varianta A: Creăm un string local DOAR dacă e nevoie de conversie
        std::wstring tempStr(val);
        wchar_t* endPtr = nullptr;
        const wchar_t* startPtr = tempStr.c_str();

        if (val.find(L'.') != std::wstring_view::npos) {
            double d = std::wcstod(startPtr, &endPtr);
            if (endPtr != startPtr) return { d };
        }
        else {
            long long ll = std::wcstoll(startPtr, &endPtr, 10);
            if (endPtr != startPtr) return { ll };
        }

        // 3. Dacă nu e număr, returnăm ca string
        return { std::wstring(val) };
    }


    vData vOliEngine::accessContainer(const vData& container, const vData& index) {
        // CAZUL 1: Containerul este un MAP
        if (container.isMap()) {
            std::wstring key = vDataToWString(index);
            // mapPtr este acum std::shared_ptr<std::map<...>>
            const auto& mapPtr = std::get<vDataMap>(container.value);

            // Verificăm dacă pointerul nu este null și căutăm folosind ->
            if (mapPtr) {
                auto it = mapPtr->find(key);
                if (it != mapPtr->end()) {
                    return it->second; // Returnăm o copie a vData (care poate fi un alt shared_ptr)
                }
            }
            return { std::monostate{} };
        }

        // CAZUL 2: Containerul este un ARRAY
        if (container.isArray()) {
            long long idx = 0;
            if (index.isInt()) idx = std::get<long long>(index.value);
            else if (index.isFloat()) idx = static_cast<long long>(std::get<double>(index.value));
            else {
                return { std::monostate{} };
            }

            const auto& arrPtr = std::get<vDataArray>(container.value);

            // Verificăm validitatea pointerului și indexul folosind ->size()
            if (arrPtr && idx >= 0 && idx < static_cast<long long>(arrPtr->size())) {
                // Folosim (*arrPtr) pentru a accesa elementul prin operatorul []
                return (*arrPtr)[static_cast<size_t>(idx)];
            }
            return { std::monostate{} };
        }

        // CAZUL 2.5: Containerul este un STRING (Rămâne neschimbat)
        if (container.isString()) {
            long long idx = vDataToLong(index);
            const std::wstring& str = std::get<std::wstring>(container.value);

            if (idx >= 0 && idx < static_cast<long long>(str.size())) {
                return { std::wstring(1, str[static_cast<size_t>(idx)]) };
            }
            return { std::monostate{} };
        }

        return { std::monostate{} };
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
    
    

    void vOliEngine::handleUnsetCommand(const ShellCommand& sc) {
        if (sc.args.empty()) return;

        std::wstring fullPath = implode(sc.args, L"");
        if (fullPath.empty()) return;

        bool forceGlobal = (fullPath[0] == L'@');

        if (fullPath[0] == L'$' || fullPath[0] == L'@') {
            fullPath.erase(0, 1);
        }

        // --- RESETARE TOTALĂ (Rămâne validă) ---
        if (fullPath == L"all") {
            m_globalVariables.clear();
            LOG_SUCCESS(L"Memory cleared. All global variables removed.");
            return;
        }

        auto path = parsePath(fullPath);

        // --- CAZUL A: Ștergere variabilă simplă (Rămâne validă) ---
        if (path.indexes.empty()) {
            bool deleted = false;
            if (forceGlobal) {
                deleted = (m_globalVariables.erase(path.rootName) > 0);
            }
            else {
                if (!m_callStack.empty()) {
                    deleted = (m_callStack.back().localVariables.erase(path.rootName) > 0);
                }
                else {
                    deleted = (m_globalVariables.erase(path.rootName) > 0);
                }
            }

            if (deleted) LOG_SUCCESS(L"Variable removed.");
            else LOG_ERROR(L"Variable not found.");
            return;
        }

        // --- CAZUL B: ȘTERGERE DIN CONTAINER (Shared Pointer Logic) ---
        vData* parent = resolveToParent(path.rootName, path.indexes, forceGlobal);

        if (!parent) {
            LOG_ERROR(L"Could not resolve path.");
            return;
        }

        std::wstring lastKey = path.indexes.back();
        if (lastKey.size() >= 2 && lastKey.front() == L'\"' && lastKey.back() == L'\"') {
            lastKey = lastKey.substr(1, lastKey.size() - 2);
        }

        // --- LOGICA PENTRU MAP ---
        if (parent->isMap()) {
            auto& mapPtr = std::get<vDataMap>(parent->value);
            // Verificăm dacă pointerul există înainte de erase
            if (mapPtr && mapPtr->erase(lastKey)) {
                LOG_SUCCESS(L"Key removed from Map.");
            }
            else {
                LOG_ERROR(L"Key not found.");
            }
        }
        // --- LOGICA PENTRU ARRAY ---
        else if (parent->isArray()) {
            auto& vecPtr = std::get<vDataArray>(parent->value);
            if (!vecPtr) return;

            try {
                size_t idx = static_cast<size_t>(std::stoll(lastKey));
                // Folosim ->size() și ->erase() pe shared_ptr
                if (idx < vecPtr->size()) {
                    vecPtr->erase(vecPtr->begin() + idx);
                    LOG_SUCCESS(L"Index removed from Array.");
                }
                else {
                    LOG_ERROR(L"Index out of bounds.");
                }
            }
            catch (...) {
                LOG_ERROR(L"Invalid array index.");
            }
        }
        else {
            LOG_ERROR(L"Target is not a container.");
        }
    }
    
    vData* vOliEngine::resolveToParent(const std::wstring& rootName, const std::vector<std::wstring>& indexes, bool forceGlobal) {
        std::wstring cleanRoot = rootName;
        if (!cleanRoot.empty() && (cleanRoot[0] == L'$' || cleanRoot[0] == L'@')) {
            cleanRoot = cleanRoot.substr(1);
        }
        cleanRoot = trim(cleanRoot);

        vData* current = nullptr;

        // --- 1. SCOPING (Găsim de unde plecăm) ---
        if (forceGlobal) {
            auto itGlobal = m_globalVariables.find(cleanRoot);
            if (itGlobal != m_globalVariables.end()) current = &(itGlobal->second);
        }
        else {
            if (!m_callStack.empty()) {
                auto& locals = m_callStack.back().localVariables;
                auto it = locals.find(cleanRoot);
                if (it != locals.end()) current = &(it->second);
            }
            if (!current) {
                auto itGlobal = m_globalVariables.find(cleanRoot);
                if (itGlobal != m_globalVariables.end()) current = &(itGlobal->second);
            }
        }

        if (!current) return nullptr;

        // --- 2. NAVIGARE (Ne oprim cu un pas înainte de final) ---
        for (size_t i = 0; i < indexes.size() - 1; ++i) {
            std::wstring idx = indexes[i];
            if (idx.size() >= 2 && idx.front() == L'\"' && idx.back() == L'\"') {
                idx = idx.substr(1, idx.size() - 2);
            }

            if (current->isMap()) {
                auto& mapPtr = std::get<vDataMap>(current->value);
                // Verificăm dacă pointerul e valid și cheia există
                if (mapPtr && mapPtr->count(idx)) {
                    current = &((*mapPtr)[idx]); // Luăm adresa elementului din heap
                }
                else return nullptr;
            }
            else if (current->isArray()) {
                auto& vecPtr = std::get<vDataArray>(current->value);
                try {
                    size_t nIdx = static_cast<size_t>(std::stoll(idx));
                    if (vecPtr && nIdx < vecPtr->size()) {
                        current = &((*vecPtr)[nIdx]);
                    }
                    else return nullptr;
                }
                catch (...) { return nullptr; }
            }
            else {
                return nullptr;
            }
        }

        return current;
    }

    vData* vOliEngine::getContainerPointer(vData& container, const std::wstring& keyOrIdx) {
        // 1. Dacă e MAP
        if (container.isMap()) {
            auto& mapPtr = std::get<vDataMap>(container.value);

            // Verificăm dacă pointerul e valid și cheia există
            if (mapPtr && mapPtr->count(keyOrIdx)) {
                // (*mapPtr)[keyOrIdx] accesează vData-ul real
                // &(...) ia adresa acelui vData pentru a o returna
                return &((*mapPtr)[keyOrIdx]);
            }
            return nullptr;
        }

        // 2. Dacă e ARRAY
        if (container.isArray()) {
            auto& arrPtr = std::get<vDataArray>(container.value);
            try {
                size_t idx = static_cast<size_t>(std::stoul(keyOrIdx));

                // Verificăm pointerul și limitele vectorului folosind ->
                if (arrPtr && idx < arrPtr->size()) {
                    return &((*arrPtr)[idx]);
                }
            }
            catch (...) {
                return nullptr;
            }
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
        // 1. Booleeni expliciți
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

        // 5. String-uri
        if (data.isString()) {
            const std::wstring& s = std::get<std::wstring>(data.value);
            if (s.empty()) return false;

            std::wstring lowerS = s;
            std::transform(lowerS.begin(), lowerS.end(), lowerS.begin(), ::towlower);
            return (lowerS == L"true" || lowerS == L"1");
        }

        // --- 6. CONTAINERE (Aici intervenim) ---

        if (data.isArray()) {
            auto arrPtr = std::get<vDataArray>(data.value);
            // Un array e "true" doar dacă pointerul există ȘI vectorul nu e gol
            return arrPtr && !arrPtr->empty();
        }

        if (data.isMap()) {
            auto mapPtr = std::get<vDataMap>(data.value);
            // Un map e "true" doar dacă pointerul există ȘI map-ul nu e gol
            return mapPtr && !mapPtr->empty();
        }

        return false;
    }



    
/*
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
    */
    void vOliEngine::handleWhileCommand(const std::wstring& fullLine) {
        // 1. Pregătim o versiune UPPER pentru căutare (case-insensitivity)
        std::wstring upperLine = fullLine;
        std::transform(upperLine.begin(), upperLine.end(), upperLine.begin(), ::towupper);

        // 2. Găsim pozițiile folosind upperLine pentru a ignora casing-ul (do vs DO)
        // IMPORTANT: Folosim upperLine ca sursă de căutare, indicii vor fi identici pentru fullLine
        size_t whilePos = upperLine.find(L"WHILE");
        size_t posDo = findTopLevelKeyword(upperLine, L"DO", L"WHILE");
        size_t posEnd = findTopLevelKeyword(upperLine, L"ENDWHILE", L"WHILE");

        // Fallback robust în caz că findTopLevelKeyword nu a fost precis
        if (posDo == std::wstring::npos) posDo = upperLine.find(L" DO ");
        if (posEnd == std::wstring::npos) posEnd = upperLine.rfind(L"ENDWHILE");

        if (whilePos == std::wstring::npos || posDo == std::wstring::npos || posEnd == std::wstring::npos) {
            LOG_ERROR(L"Malformed WHILE: Missing DO or ENDWHILE keywords.");
            return;
        }

        // 3. Calculăm indicii de start pentru conținut
        // Găsim poziția exactă a lui "DO" (fără spațiul din față dacă am folosit fallback-ul cu spațiu)
        size_t actualDoPos = upperLine.find(L"DO", posDo);

        // Extragem Condiția: între WHILE (+5) și DO
        size_t condStart = whilePos + 5;
        std::wstring conditionPart = trim(fullLine.substr(condStart, actualDoPos - condStart));

        // Extragem Corpul: imediat după DO (+2) până la ENDWHILE
        size_t bodyStart = actualDoPos + 2;
        std::wstring bodyCommand = fullLine.substr(bodyStart, posEnd - bodyStart);

        // 4. Pregătim instrucțiunile (preParse le împarte corect în vector)
        std::vector<std::wstring> instructions = preParse(bodyCommand);
        if (instructions.empty()) return;

        // 5. Bucla de execuție a motorului Oli
        int safetyBreak = 0;
        while (true) {
            // Safety check pentru a nu bloca procesorul în bucle infinite
            if (++safetyBreak > 10000) {
                LOG_ERROR(L"Safety limit reached (10000 iterations). Possible infinite loop.");
                break;
            }

            // Evaluăm condiția la fiecare iterație
            vData condRes = evaluateExpression(conditionPart);
            if (!vDataToBool(condRes)) break;

            for (const auto& instr : instructions) {
                // Executăm intern fiecare linie din corpul buclei
                this->executeInternal(instr);

                // GESTIONARE STATUS (Crucial pentru BREAK / CONTINUE / RETURN)
                if (m_executionStatus != OliStatus::RUNNING) {
                    if (m_executionStatus == OliStatus::CONTINUE_REQUESTED) {
                        m_executionStatus = OliStatus::RUNNING;
                        goto next_iteration; // Sărim peste restul instrucțiunilor din această tură
                    }
                    if (m_executionStatus == OliStatus::BREAK_REQUESTED) {
                        m_executionStatus = OliStatus::RUNNING;
                        return; // Ieșim complet din funcția handleWhile (terminăm bucla)
                    }
                    if (m_executionStatus == OliStatus::RETURN_REQUESTED) {
                        return; // Propagăm return-ul în sus pe stivă
                    }
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

    /*
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
    */

    void vOliEngine::handleRunCommand(const ShellCommand& sc) {
        if (sc.args.empty()) {
            LOG_ERROR(L"Usage: run \"path/to/script.oli\"");
            return;
        }

        std::wstring pathStr = sc.args[0];
        // Curățare ghilimele...

        // --- LOGICĂ DE CURĂȚARE CRITICĂ ---
        if (pathStr.size() >= 2 && pathStr.front() == L'"' && pathStr.back() == L'"') {
            pathStr = pathStr.substr(1, pathStr.size() - 2);
        }
        // Uneori pot rămâne spații accidentale
        pathStr = trim(pathStr);

        std::ifstream file;
        PortTools::openIfstream(file, pathStr); // Abstractizare portabilă

        if (!file.is_open()) {
            LOG_ERROR(L"Could not open script: " + pathStr);
            return;
        }

        std::string lineA;
        bool firstLine = true;

        while (std::getline(file, lineA)) {
            if (!lineA.empty() && lineA.back() == '\r') lineA.pop_back();
            if (lineA.empty()) continue;

            // Folosim utilitarul portabil
            std::wstring lineW = PortTools::utf8_to_wstring(lineA);

            if (firstLine) {
                if (!lineW.empty() && (unsigned short)lineW[0] == 0xFEFF) {
                    lineW.erase(0, 1);
                }
                firstLine = false;
            }

            std::wstring finalLine = trim(lineW);
            if (finalLine.empty() || finalLine[0] == L'#') continue;

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

    /*
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
    */

    vData vOliEngine::handleSysFunc(const std::vector<vData>& args) {
        if (args.empty()) return { L"" };
        std::wstring command = vDataToWString(args[0]);

        // Curățare ghilimele...
        if (command.size() >= 2 && command.front() == L'"' && command.back() == L'"') {
            command = command.substr(1, command.size() - 2);
        }

        std::wstring output;
        std::wstring line;
        FILE* pipe = PortTools::openPipe(command, L"r");

        if (!pipe) return { L"ERROR" };

        // Citire abstractizată - zero platform-specific code aici
        while (PortTools::readLineFromPipe(pipe, line)) {
            output += line;
        }

        PortTools::closePipe(pipe);
        return vData(output);
    }

    /*
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
    */

    void vOliEngine::handleSysCommand(const ShellCommand& sc) {
        // 1. Validare argumente
        if (sc.args.empty()) {
            LOG_ERROR(L"Usage: sys <system_command>");
            return;
        }

        // 2. Reconstruim comanda completă din argumente
        std::wstring fullCommand;
        for (size_t i = 0; i < sc.args.size(); ++i) {
            fullCommand += sc.args[i] + (i < sc.args.size() - 1 ? L" " : L"");
        }

        // 3. Substituim variabilele (folosind noua logică internă de resolve)
        fullCommand = substituteVariables(fullCommand);

        // 4. Curățăm ghilimelele exterioare dacă utilizatorul a trimis comanda ca string literal
        if (fullCommand.size() >= 2 && fullCommand.front() == L'"' && fullCommand.back() == L'"') {
            fullCommand = fullCommand.substr(1, fullCommand.size() - 2);
        }

        LOG_INFO(L"Executing: " + fullCommand);

        // 5. Pregătim stream-urile de output pentru a evita intercalarea mesajelor
        std::wcout.flush();
        fflush(stdout);

        // 6. Deschidem pipe-ul folosind utilitarul portabil
        FILE* pipe = PortTools::openPipe(fullCommand, L"r");
        if (!pipe) {
            LOG_ERROR(L"Could not execute system command.");
            return;
        }

        // 7. Citim și afișăm output-ul în timp real
        std::wstring line;
        while (PortTools::readLineFromPipe(pipe, line)) {
            std::wcout << line;
            std::wcout.flush(); // Asigură afișarea imediată (real-time feel)
        }

        // 8. Închidem pipe-ul și verificăm codul de retur
        int returnCode = PortTools::closePipe(pipe);

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
            //this->execute(instr);
            this->executeInternal(instr);

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

        // 1. Găsire Keyword-uri (DO și ENDCYCLE)
        size_t cyclePos = upperLine.find(L"CYCLE");
        size_t posDo = findTopLevelCycleKeyword(fullLine, L"DO");
        size_t posEnd = findTopLevelCycleKeyword(fullLine, L"ENDCYCLE");

        if (posDo == std::wstring::npos || posEnd == std::wstring::npos) {
            LOG_ERROR(L"Malformed CYCLE: Missing DO or ENDCYCLE at current level");
            return;
        }

        // 2. Extragere Header (Ex: $lista AS $item)
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

        // 3. Evaluare Sursă (Ce iterăm?)
        vData sourceData = evaluateExpression(sourceExpr);
        if (sourceData.isNull()) {
            LOG_ERROR(L"Cycle error: Source '" + sourceExpr + L"' is NULL.");
            return;
        }

        // 4. Pregătire Instrucțiuni din corpul buclei
        size_t bodyStart = posDo + 2;
        std::wstring bodyCommand = trim(fullLine.substr(bodyStart, posEnd - bodyStart));
        std::vector<std::wstring> instructions = preParse(bodyCommand);

        // 5. Salvare valoare veche pentru iterator (Shadowing Protection)
        vData oldVal = resolveVariable(iteratorName);
        bool existed = !oldVal.isNull();

        // 6. EXECUȚIA EFECTIVĂ
        if (sourceData.isArray()) {
            auto arrPtr = std::get<vDataArray>(sourceData.value);
            if (arrPtr) {
                // Iterăm prin vectorul real din Heap folosind *arrPtr
                for (const auto& item : *arrPtr) {
                    if (executeCycleStep(iteratorName, item, instructions)) break; // BREAK detectat
                    if (m_executionStatus == OliStatus::RETURN_REQUESTED) break; // RETURN detectat
                }
            }
        }
        else if (sourceData.isMap()) {
            auto mapPtr = std::get<vDataMap>(sourceData.value);
            if (mapPtr) {
                // Iterăm prin Map-ul real din Heap
                for (const auto& pair : *mapPtr) {
                    // Într-un Map, CYCLE returnează de obicei cheia (pair.first)
                    if (executeCycleStep(iteratorName, vData{ pair.first }, instructions)) break;
                    if (m_executionStatus == OliStatus::RETURN_REQUESTED) break;
                }
            }
        }
        else if (sourceData.isString()) {
            // String-ul nu este pointer, iterația rămâne clasică
            const std::wstring& str = std::get<std::wstring>(sourceData.value);
            for (wchar_t c : str) {
                if (executeCycleStep(iteratorName, vData{ std::wstring(1, c) }, instructions)) break;
                if (m_executionStatus == OliStatus::RETURN_REQUESTED) break;
            }
        }
        else {
            LOG_ERROR(L"Cycle error: Source is not iterable (Array, Map or String).");
        }

        // 7. RESTAURARE (Curățăm "murdăria" lăsată de iterator în memorie)
        if (existed) {
            setVariable(iteratorName, oldVal);
        }
        else {
            // Dacă variabila nu exista, o ștergem complet din scope-ul curent
            std::wstring cleanName = cleanVariableName(iteratorName);
            if (!m_callStack.empty()) {
                m_callStack.back().localVariables.erase(cleanName);
            }
            else {
                m_globalVariables.erase(cleanName);
            }
        }
    }



    size_t vOliEngine::findTopLevelCycleKeyword(const std::wstring& line, const std::wstring& keyword) {
        int depth = 0;
        bool inQuotes = false;

        std::wstring upperLine = line;
        std::transform(upperLine.begin(), upperLine.end(), upperLine.begin(), ::towupper);
        std::wstring upperKey = keyword;
        std::transform(upperKey.begin(), upperKey.end(), upperKey.begin(), ::towupper);

        // 1. Găsim unde începe CYCLE-ul principal în acest segment de cod
        size_t mainCyclePos = upperLine.find(L"CYCLE");
        if (mainCyclePos == std::wstring::npos) return std::wstring::npos;

        // 2. Scanăm începând de după cuvântul "CYCLE"
        for (size_t i = mainCyclePos + 5; i < upperLine.size(); ++i) {
            // Ignorăm conținutul dintre ghilimele
            if (upperLine[i] == L'"' && (i == 0 || upperLine[i - 1] != L'\\')) {
                inQuotes = !inQuotes;
                continue;
            }
            if (inQuotes) continue;

            // Verificăm dacă suntem la începutul unui cuvânt (delimitatori standard)
            bool isStart = (i == 0 || iswspace(upperLine[i - 1]) || wcschr(L";()[]{},", upperLine[i - 1]));
            if (!isStart) continue;

            std::wstring_view rem(&upperLine[i], upperLine.size() - i);

            // Dacă suntem la nivelul 0 și găsim keyword-ul căutat (DO sau ENDCYCLE)
            if (depth == 0 && rem.starts_with(upperKey)) {
                size_t nextIdx = i + upperKey.length();
                // Verificăm validitatea cuvântului întreg
                if (nextIdx >= upperLine.size() || iswspace(upperLine[nextIdx]) || wcschr(L";()[]{},", upperLine[nextIdx])) {
                    return i;
                }
            }

            // Gestionăm adâncimea pentru CYCLE-uri imbricate
            if (rem.starts_with(L"CYCLE")) {
                size_t next = i + 5;
                if (next >= upperLine.size() || iswspace(upperLine[next])) {
                    depth++;
                    i += 4; // Sărim peste restul cuvântului CYCLE
                }
            }
            else if (rem.starts_with(L"ENDCYCLE")) {
                depth--;
                i += 7; // Sărim peste restul cuvântului ENDCYCLE
            }
        }
        return std::wstring::npos;
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
    /*
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
    */

void vOliEngine::callProcedure(const Procedure& proc, const std::vector<std::wstring>& passedArgs) {
    // 1. EVALUĂM ARGUMENTELE ÎN CONTEXTUL APELANTULUI
    // Facem asta PRIMA DATĂ, cât timp m_callStack.back() este încă vechiul context.
    std::map<std::wstring, vData> evaluatedParams;

    for (size_t i = 0; i < proc.params.size(); ++i) {
        std::wstring pName = cleanVariableName(proc.params[i]);
        /*
        if (i < passedArgs.size()) {
            // Evaluarea se face în contextul de dinainte de apel
            evaluatedParams[pName] = evaluateExpression(passedArgs[i]);
        }*/
        if (i < passedArgs.size()) {
            std::wstring arg = passedArgs[i];
            // Dacă utilizatorul a scris 'n' în loc de '$n', îl ajutăm noi:
            if (!arg.empty() && arg[0] != L'$' && arg[0] != L'\"' && !iswdigit(arg[0])) {
                arg = L"$" + arg;
            }
            evaluatedParams[pName] = evaluateExpression(arg);
        }
        else {
            evaluatedParams[pName] = vData{ std::monostate{} };
        }
    }

    // 2. CONSTRUIM FRAME-UL NOU
    StackFrame frame;
    frame.functionName = proc.name.empty() ? L"anonymous_proc" : proc.name;
    frame.localVariables = std::move(evaluatedParams); // Mutăm argumentele evaluate aici

    // 3. PUSH FRAME
    m_callStack.push_back(std::move(frame));

    // Salvare stare return
    bool previousShouldReturn = m_shouldReturn;
    m_shouldReturn = false;

    // 4. EXECUȚIE CORP PROCEDURĂ
    for (const auto& line : proc.body) {
        if (m_shouldReturn ) break;
        execute(line);
    }

    // 5. POP FRAME
    if (!m_callStack.empty()) {
        m_callStack.pop_back();
    }

    m_shouldReturn = previousShouldReturn;
}

    /*
  void vOliEngine::handlePluginCommand(const ShellCommand& sc) {
      // 1. Verificăm dacă avem calea către DLL
      if (sc.args.empty()) {
          LOG_ERROR(L"Usage: plugin \"path/to/plugin.dll\"");
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
  */

    void vOliEngine::handlePluginCommand(const ShellCommand& sc) {
        // 1. Verificăm argumentele
        if (sc.args.empty()) {
            LOG_ERROR(L"Usage: plugin \"path/to/plugin\"");
            return;
        }

        std::wstring dllPath = sc.args[0];

        if (dllPath.size() >= 2 && dllPath.front() == L'"' && dllPath.back() == L'"') {
            dllPath = dllPath.substr(1, dllPath.size() - 2);
        }

        // 2. Încărcăm biblioteca folosind utilitarul portabil
        PortTools::LibHandle hLib = PortTools::loadDynamicLibrary(dllPath);

        if (!hLib) {
            LOG_ERROR(L"Could not load plugin: " + dllPath + L" (Error: " + PortTools::getLastErrorString() + L")");
            return;
        }

        // 3. Definim tipul funcției pe care o căutăm
        typedef void (*RegisterFunc)(std::map<std::wstring, OliFunctionHandler>&);

        // 4. Căutăm simbolul exportat
        RegisterFunc regFunc = (RegisterFunc)PortTools::getFunctionSymbol(hLib, "LoadOliPlugin");

        if (regFunc) {
            // 5. Executăm înregistrarea
            regFunc(this->m_functionsHandlers);
            LOG_SUCCESS(L"Plugin loaded: " + dllPath);
            LOG_SUCCESS(L"          Native functions injected into Oli memory.");
        }
        else {
            LOG_ERROR(L"Invalid Plugin: Export 'LoadOliPlugin' not found in " + dllPath);
            PortTools::freeDynamicLibrary(hLib);
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

  vData vOliEngine::handleFloatFunc(const std::vector<vData>& args) {
      if (args.empty()) return vData{ 0.0 };

      const vData& input = args[0];

      // 1. Deja Float -> returnăm identic
      if (input.isFloat()) return input;

      // 2. Int -> Double
      if (input.isInt()) {
          return vData{ static_cast<double>(std::get<long long>(input.value)) };
      }

      // 3. String -> Double
      if (input.isString()) {
          try {
              // std::stod se ocupă de conversia wstring -> double
              return vData{ std::stod(std::get<std::wstring>(input.value)) };
          }
          catch (...) {
              return vData{ 0.0 }; // Fallback la eroare
          }
      }

      // 4. Bool -> Double (1.0 sau 0.0)
      if (input.isBool()) {
          return vData{ std::get<bool>(input.value) ? 1.0 : 0.0 };
      }

      return vData{ 0.0 };
  }

  vData vOliEngine::handleStrFunc(const std::vector<vData>& args) {
      if (args.empty()) return vData{ L"" }; // Default: șir vid

      const vData& input = args[0];

      // 1. Dacă este deja String, returnăm o copie
      if (input.isString()) return input;

      // 2. Pentru restul tipurilor (Int, Float, Bool, Array, Map)
      // Folosim funcția ta vDataToWString care se ocupă deja de formatare
      return vData{ vDataToWString(input) };
  }
  

  vData vOliEngine::handleArrayFunc(const std::vector<vData>& args) {
      // Creăm un shared_ptr care copiază conținutul lui 'args' direct în heap
      return vData{ std::make_shared<std::vector<vData>>(args) };
  }

  vData vOliEngine::handleMapFunc(const std::vector<vData>& args) {
      // 1. Alocăm Map-ul în Heap și obținem pointerul partajat
      // Folosim std::make_shared pentru eficiență
      vDataMap newMap = std::make_shared<std::map<std::wstring, vData>>();

      // 2. Dacă nu avem argumente, returnăm shared_ptr-ul către map-ul gol
      if (args.empty()) {
          return vData{ newMap };
      }

      // 3. Parcurgem argumentele doi câte doi (cheie, valoare)
      for (size_t i = 0; i + 1 < args.size(); i += 2) {
          std::wstring key;

          // Folosim utilitarul tău vDataToWString pentru a garanta că avem un string
          key = vDataToWString(args[i]);

          // 4. OPERAȚIA CRITICĂ: Dereferențiem pointerul (*) pentru a folosi []
          // Punem paranteze în jurul dereferențierii pentru prioritate: (*newMap)
          (*newMap)[key] = args[i + 1];
      }

      if (args.size() % 2 != 0) {
          // LOG_WARNING(L"MAP() ignored odd last argument.");
      }

      // 5. Returnăm vData care ambalează pointerul nostru
      return vData{ newMap };
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


  
  vData vOliEngine::callUserFunction(const std::wstring& funcName, const std::vector<vData>& args, vData context) {
      // 1. Găsirea definiției funcției
      auto it = m_userFunctions.find(funcName);
      if (it == m_userFunctions.end()) {
          LOG_ERROR(L"Runtime Error: Function '" + funcName + L"' not found.");
          return { std::monostate{} };
      }

      const Procedure& func = it->second;

      // --- 2. PREGĂTIRE FRAME NOU ---
      StackFrame frame;
      frame.functionName = funcName;

      // --- 3. INJECTARE CONTEXT ($this) ---
      // Folosim cheia "this" (fără $). 
      // În OliEngine, resolveVariable("this") va căuta această cheie în localVariables.
      frame.localVariables[L"this"] = context;

      // --- 4. SETARE PARAMETRI LOCALI ---
      for (size_t i = 0; i < func.params.size(); ++i) {
          std::wstring pName = cleanVariableName(func.params[i]);
          frame.localVariables[pName] = (i < args.size()) ? args[i] : vData{ std::monostate{} };
      }

      // Inițializăm rezultatul cu NULL
      frame.localVariables[L"return"] = { std::monostate{} };

      // --- 5. PUSH PE STIVĂ ---
      // De aici încolo, executeAST va vedea acest frame ca fiind "cel curent"
      m_callStack.push_back(std::move(frame));

      // Salvare stare flag return pentru a permite recursivitatea corectă
      bool previousShouldReturn = m_shouldReturn;
      m_shouldReturn = false;

      // --- 6. EXECUȚIE CORP FUNCȚIE ---
      // Reconstruim corpul într-un script executabil
      std::wstring fullBody;
      for (const auto& line : func.body) {
          if (trim(line).empty()) continue;
          fullBody += line;
          // Adăugăm separator dacă lipsește pentru a nu "lipi" instrucțiunile
          if (fullBody.back() != L';') fullBody += L";";
          fullBody += L"\n";
      }

      // Execuția propriu-zisă
      this->executeInternal(fullBody);

      // --- 7. COLECTAREA REZULTATULUI ---
      // IMPORTANT: Luăm rezultatul din frame-ul nostru înainte de a-l șterge
      vData result = { std::monostate{} };
      if (!m_callStack.empty()) {
          result = m_callStack.back().localVariables[L"return"];
      }

      // --- 8. RESTAURARE STIVĂ ȘI FLAG-URI ---
      if (!m_callStack.empty()) {
          m_callStack.pop_back();
      }

      // Restaurăm flag-ul de return al apelantului (esențial pentru funcții imbricate)
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
      // 1. CAZUL MAP
      if (container.isMap()) {
          auto& mPtr = std::get<vDataMap>(container.value);

          // Safety: Dacă pointerul e null, îl inițializăm
          if (!mPtr) mPtr = std::make_shared<std::map<std::wstring, vData>>();

          // Dereferențiem (*mPtr) pentru a folosi operatorul [] pe map-ul real
          (*mPtr)[vDataToWString(key)] = newValue;
      }
      // 2. CAZUL ARRAY
      else if (container.isArray()) {
          auto& aPtr = std::get<vDataArray>(container.value);

          // Safety: Dacă pointerul e null, nu avem ce updata (sau îl poți inițializa)
          if (!aPtr) return;

          // Folosim vDataToLong sau static_cast pe vDataToDouble pentru index
          size_t idx = static_cast<size_t>(vDataToDouble(key));

          // Folosim aPtr->size() pentru a accesa metoda vectorului de pe heap
          if (idx < aPtr->size()) {
              // Dereferențiem (*aPtr) pentru a scrie la indexul respectiv
              (*aPtr)[idx] = newValue;
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

  

  vData vOliEngine::handleSplitFunc(const std::vector<vData>& args) {
      // 1. Returnăm un Array gol (inițializat corect în heap)
      if (args.empty()) {
          return vData{ std::make_shared<std::vector<vData>>() };
      }

      std::wstring text = vDataToWString(args[0]);
      std::wstring delims = (args.size() > 1) ? vDataToWString(args[1]) : L" ";

      // Curățăm secvențele de escape (rămâne neschimbat)
      if (delims == L"\\n") delims = L"\n";
      else if (delims == L"\\r\\n") delims = L"\r\n";
      else if (delims == L"\\t") delims = L"\t";

      // 2. Alocăm vectorul în HEAP
      auto result = std::make_shared<std::vector<vData>>();

      size_t lastPos = 0;
      size_t pos = text.find(delims);
      bool isWhitespaceSplit = (delims == L" ");

      while (pos != std::wstring::npos) {
          std::wstring token = text.substr(lastPos, pos - lastPos);

          if (!isWhitespaceSplit || !token.empty()) {
              // 3. Folosim -> pentru a accesa push_back pe vectorul din heap
              result->push_back(vData{ token });
          }

          lastPos = pos + delims.length();
          pos = text.find(delims, lastPos);
      }

      std::wstring lastToken = text.substr(lastPos);
      if (!isWhitespaceSplit || !lastToken.empty()) {
          result->push_back(vData{ lastToken });
      }

      // 4. Returnăm shared_ptr-ul
      return vData{ result };
  }

  vData vOliEngine::handleJoinFunc(const std::vector<vData>& args) {
      // 1. Validare: Avem nevoie de cel puțin un Array
      if (args.empty() || !args[0].isArray()) {
          return vData{ L"" };
      }

      // 2. Extragem shared_ptr-ul către vector
      const auto& listPtr = std::get<vDataArray>(args[0].value);

      // Verificăm dacă pointerul este valid (nu e null)
      if (!listPtr) return vData{ L"" };

      // 3. Separatorul (rămâne neschimbat)
      std::wstring separator = (args.size() > 1) ? vDataToWString(args[1]) : L" ";

      std::wstring result;

      // 4. Folosim ->size() pentru a accesa vectorul de pe heap
      for (size_t i = 0; i < listPtr->size(); ++i) {
          // 5. Dereferențiem (*listPtr)[i] pentru a ajunge la elementul vData
          result += vDataToWString((*listPtr)[i]);

          if (i < listPtr->size() - 1) {
              result += separator;
          }
      }

      return vData{ result };
  }

  vData vOliEngine::handleTrimFunc(const std::vector<vData>& args) {
      if (args.empty()) return vData{ L"" };

      // Convertim argumentul în string și îi aplicăm funcția de curățare
      std::wstring text = vDataToWString(args[0]);
      return vData{ trim(text) };
  }

  vData vOliEngine::deepCopy(const vData& source) {
      // 1. GESTIONARE MAP
      if (auto* oldMapPtr = std::get_if<vDataMap>(&source.value)) {
          auto oldMap = *oldMapPtr;
          // Cream un shared_ptr NOU către un std::map NOU
          auto newMap = std::make_shared<std::map<std::wstring, vData>>();

          for (auto const& [key, val] : *oldMap) {
              // Copiem recursiv fiecare valoare din map
              (*newMap)[key] = deepCopy(val);
          }
          return vData(newMap);
      }

      // 2. GESTIONARE ARRAY
      if (auto* oldArrPtr = std::get_if<vDataArray>(&source.value)) {
          auto oldArr = *oldArrPtr;
          // Cream un shared_ptr NOU către un std::vector NOU
          auto newArr = std::make_shared<std::vector<vData>>();

          for (const auto& item : *oldArr) {
              newArr->push_back(deepCopy(item));
          }
          return vData(newArr);
      }

      // 3. TIPURI SIMPLE (INT, FLOAT, STRING, POINTER)
      // Acestea se copiază prin valoare în variant, deci nu au nevoie de logică specială
      return source;
  }


  void vOliEngine::handleHelpCommand(const ShellCommand& sc) {
      std::wstring target = sc.args.empty() ? L"manual" : sc.args[0];
      std::transform(target.begin(), target.end(), target.begin(), ::towlower);

      // 1. Construim calea
      std::wstring pathStr;
      if (target == L"manual") {
          pathStr = L"docs/manual.md";
      }
      else {
          pathStr = L"docs/commands/" + target + L".md";
      }

      // 2. Deschidem fișierul folosind std::filesystem::path pentru conversia automată
      // Aceasta rezolvă eroarea de pe Linux (convertind intern wstring în path-ul nativ)
      std::wifstream file{ (std::filesystem::path(pathStr)) };

      // 3. Fallback pentru funcții dacă nu e în comenzi
      if (!file.is_open() && target != L"manual") {
          pathStr = L"docs/functions/" + target + L".md";
          file.open(std::filesystem::path({pathStr}));
      }

      if (file.is_open()) {
          // IMPORTANT pentru Linux: Setează localizarea pentru a citi corect UTF-8
          try {
              file.imbue(std::locale("en_US.UTF-8"));
          }
          catch (...) {
              file.imbue(std::locale::classic());
          }

          LOG_RAW(L"\n--- Oli Help System ---");

          std::wstring line;
          while (std::getline(file, line)) {
              // Logica de culori pentru consolă
              if (!line.empty() && line[0] == L'#') {
                  ConsoleManager::getInstance().writeRaw(line + L"\n", FOREGROUND_GREEN | FOREGROUND_INTENSITY);
              }
              else {
                  LOG_RAW(line);
              }
          }
          LOG_RAW(L"-------------------------\n");
      }
      else {
          LOG_ERROR(L"Documentation not found for: " + target);
      }
  }