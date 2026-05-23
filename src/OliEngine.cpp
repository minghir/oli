#include "OliEngine.hpp"
#include "OliExpressionParser.hpp"
#include "PortTools.hpp"
#include "vDataSerialize.hpp"

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
    // --- 1. CURĂȚARE INIȚIALĂ ---
    std::wstring cleanLine = trim(line);
    if (cleanLine.empty() && m_accumulator.empty()) return;

    // --- 2. MASCARE GHILIMELE (PAS CRITIC) ---
    // Creăm o variantă a liniei unde tot ce e între ghilimele devine spațiu.
    // Asta previne detectarea eronată a parantezelor sau cuvintelor cheie din string-uri.
    bool lineInQuotes = false;
    std::wstring maskedLine = cleanLine;
    for (size_t i = 0; i < maskedLine.length(); ++i) {
        // Detectăm ghilimelele și ignorăm escape-ul \"
        if (maskedLine[i] == L'"' && (i == 0 || maskedLine[i - 1] != L'\\')) {
            lineInQuotes = !lineInQuotes;
            maskedLine[i] = L' ';
            continue;
        }
        if (lineInQuotes) {
            maskedLine[i] = L' '; // Mascăm conținutul string-ului
        }
    }

    // --- 3. DETECTARE COMENTARII (DOAR ÎN AFARA GHILIMELELOR) ---
    size_t commentPos = maskedLine.find(L'#');
    if (commentPos != std::wstring::npos) {
        cleanLine = trim(cleanLine.substr(0, commentPos));
        maskedLine = trim(maskedLine.substr(0, commentPos));
    }

    if (cleanLine.empty() && m_accumulator.empty()) return;

    // --- 4. ACTUALIZARE ADÂNCIME PARANTEZE (MAPS/ARRAYS) ---
    // Folosim maskedLine pentru a fi siguri că nu numărăm paranteze din string-uri
    for (wchar_t c : maskedLine) {
        if (c == L'{' || c == L'[') m_bracketDepth++;
        if (c == L'}' || c == L']') m_bracketDepth--;
    }

    // Pregătim o variantă Uppercase a liniei mascate pentru detectarea comenzilor
    std::wstring upperMasked = maskedLine;
    std::transform(upperMasked.begin(), upperMasked.end(), upperMasked.begin(), ::towupper);

    // --- 5. GESTIONARE ÎNREGISTRARE FUNC/PROC ---
    if (m_isRecording || m_isRecordingFunc) {
        if (upperMasked == L"ENDPROC" || upperMasked == L"ENDFUNC") {
            m_isRecording = false;
            m_isRecordingFunc = false;
            m_blockDepth = 0;
            m_bracketDepth = 0;
            vOliKeyWords::registerDynamicCommand(m_activeProcName);
            LOG_SUCCESS(L"Procedure/Function saved.");
            return;
        }

        if (m_isRecording) m_procedures[m_activeProcName].body.push_back(cleanLine);
        else m_userFunctions[m_activeFuncName].body.push_back(cleanLine);
        return;
    }

   
    auto checkBlock = [&](const std::wstring& key, bool increment, bool mustBeStart = false) {
        size_t p = upperMasked.find(key);
        if (p != std::wstring::npos) {
            // Dacă am setat mustBeStart, verificăm să fie la poziția 0
            if (mustBeStart && p != 0) return false;

            // Verificăm dacă este cuvânt de sine stătător
            bool startOk = (p == 0 || iswspace(upperMasked[p - 1]) || wcschr(L";()[]{}\"", upperMasked[p - 1]));
            bool endOk = (p + key.length() >= upperMasked.length() || iswspace(upperMasked[p + key.length()]) || wcschr(L";()[]{}\"", upperMasked[p + key.length()]));

            if (startOk && endOk) {
                if (increment) m_blockDepth++;
                else if (m_blockDepth > 0) m_blockDepth--;
                return true;
            }
        }
        return false;
        };

    // Incrementări
    checkBlock(L"IF", true);      checkBlock(L"WHILE", true);
    checkBlock(L"FOR", true);     checkBlock(L"REPEAT", true);
    checkBlock(L"CYCLE", true);   checkBlock(L"PROC", true, true);
    checkBlock(L"FUNC", true, true);    checkBlock(L"SWITCH", true);

    // Decrementări
    checkBlock(L"ENDIF", false);  checkBlock(L"ENDWHILE", false);
    checkBlock(L"ENDFOR", false); checkBlock(L"ENDREPEAT", false);
    checkBlock(L"ENDCYCLE", false); checkBlock(L"ENDPROC", false);
    checkBlock(L"ENDFUNC", false); checkBlock(L"ENDSWITCH", false);

    // --- 7. ACUMULARE ---
    bool hasBackslash = (!cleanLine.empty() && cleanLine.back() == L'\\');
    if (hasBackslash) cleanLine.pop_back();

    if (!m_accumulator.empty()) m_accumulator += L"\n";
    m_accumulator += cleanLine;

    // Declanșare specială pentru început de PROC/FUNC
    // (trebuie să trimitem antetul la executeInternal pentru a activa m_isRecording)
    if (upperMasked.find(L"PROC ") == 0 || upperMasked.find(L"FUNC ") == 0) {
        std::wstring startCmd = m_accumulator;
        m_accumulator.clear();
        this->executeInternal(startCmd);
        return;
    }

    // --- 8. DECIZIA DE EXECUȚIE ---
    // Dacă suntem într-un bloc deschis (IF, WHILE), într-un Map/Array neînchis sau avem backslash, așteptăm.
    if (m_blockDepth > 0 || m_bracketDepth > 0 || hasBackslash) {
        return;
    }

    // Executăm blocul acumulat
    std::wstring finalBlock = m_accumulator;
    m_accumulator.clear();

    // Resetări de siguranță pentru buffer
    m_bracketDepth = 0;

    if (trim(finalBlock).empty()) return;

    this->executeInternal(finalBlock);
}

void vOliEngine::executeInternal(const std::wstring& fullInput) {
    std::wstring trimmedInput = trim(fullInput);
    if (trimmedInput.empty()) return;

    // --- PASUL 1: DESCOMPUNEREA ÎN INSTRUCȚIUNI ---
    // preParse sparge blocul în instrucțiuni individuale (separate de ; sau \n)
    std::vector<std::wstring> instructions = preParse(trimmedInput);

    if (instructions.size() > 1) {
        for (const auto& instr : instructions) {
            if (instr.empty()) continue;

            // Verificăm dacă un 'return' anterior sau o eroare a cerut oprirea
            if (m_shouldReturn || m_executionStatus != OliStatus::RUNNING) {
                break;
            }

            this->executeInternal(instr); // Recursivitate pentru fiecare linie
        }
        return;
    }

    // --- PASUL 2: PROCESAREA UNEI SINGURE INSTRUCȚIUNI ---
    std::wstring input = instructions[0];

    // Identificăm dacă este un bloc de control (IF, WHILE, etc.)
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

    // Extragem primul cuvânt (comanda sau procedura)
    size_t firstSpace = input.find_first_of(L" \t\n\r(");
    std::wstring firstWord = (firstSpace != std::wstring::npos) ? input.substr(0, firstSpace) : input;
    std::wstring upperFirst = firstWord;
    std::transform(upperFirst.begin(), upperFirst.end(), upperFirst.begin(), ::towupper);

    // --- PRIORITATE 1: COMENZI DE SISTEM ȘI BLOCURI DE CONTROL ---
    if (isControlBlock || vOliKeyWords::isShellCommand(upperFirst)) {
        this->executeCommand(input);
        return;
    }

    // --- PRIORITATE 2: PROCEDURI UTILIZATOR ---
    //if (m_procedures.count(firstWord)) {
      if (m_procedures.count(upperFirst)) {
        std::wstring argsPart = (firstSpace != std::wstring::npos) ? input.substr(firstSpace + 1) : L"";
        std::vector<std::wstring> rawTokens = wexplodeQuoteSafe(argsPart, L' ');
        std::vector<std::wstring> cleanArgs;
        for (const auto& arg : rawTokens) {
            std::wstring t = trim(arg);
            if (!t.empty()) cleanArgs.push_back(t);
        }
        //callProcedure(m_procedures[firstWord], cleanArgs);
        callProcedure(m_procedures[upperFirst], cleanArgs);
        return;
    }

    // --- PRIORITATE 3: ATRIBUIRE (x = 5, x += 5, etc.) ---
    // Căutăm semnul egal, dar ne asigurăm că nu este un operator de comparație
    size_t eqPos = input.find(L'=');
    if (eqPos != std::wstring::npos && eqPos > 0) {
        bool isComparison = (eqPos + 1 < input.size() && input[eqPos + 1] == L'=') ||
            (input[eqPos - 1] == L'!' || input[eqPos - 1] == L'>' || input[eqPos - 1] == L'<');

        if (!isComparison) {
            // Verificăm dacă avem operatori compuși (+=, -=, etc.)
            size_t varEndPos = eqPos;
            wchar_t prevChar = input[eqPos - 1];
            if (prevChar == L'+' || prevChar == L'-' || prevChar == L'*' || prevChar == L'/') {
                varEndPos = eqPos - 1;
            }

            std::wstring leftSide = trim(input.substr(0, varEndPos));

            // Dacă partea stângă începe cu $ sau literă, o tratăm ca pe un SET
            if (!leftSide.empty() && (leftSide[0] == L'$' || iswalpha(leftSide[0]))) {
                executeCommand(L"SET " + input);
                return;
            }
        }
    }

    // --- PRIORITATE 4: EVALUARE EXPRESIE (FALLBACK) ---
    try {
        vData result = evaluateExpression(input);
        //bool silentMode = (m_blockDepth > 0 || !m_callStack.empty() || m_isRecording || m_isRecordingFunc);
        bool silentMode = (!m_callStack.empty() || m_blockDepth > 0);
        if (m_echoEnabled && !silentMode && !result.isNull()) {
            // Doar în modul interactiv (prompt direct) afișăm rezultatul
            LOG_RAW(vDataToWString(result));
        }
    }
    catch (const std::exception& e) {
        // --- REPARAȚIE CRITICĂ PENTRU CRASH ---
        // Nu folosim std::wstring(err.begin(), err.end()) deoarece provoacă Access Violation
        std::string err(e.what());
        std::wstring werr;
        werr.reserve(err.size());
        for (char c : err) { werr += static_cast<wchar_t>(static_cast<unsigned char>(c)); }

        LOG_ERROR(L"Runtime Error: " + werr);
    }
    catch (...) {
        LOG_ERROR(L"An unknown critical error occurred during execution.");
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
    else if (m_procedures.count(cmdUpper)) {
        ShellCommand sc = vOliCommandParser::parse(fullCommand);
        callProcedure(m_procedures[cmdUpper], sc.args);
    }
    else {
        LOG_ERROR(L"Unknown command or procedure: " + cmdName);
    }
}


std::vector<std::wstring> vOliEngine::preParse(const std::wstring& input) {
    std::vector<std::wstring> result;
    std::wstring current;
    int blockDepth = 0;
    int bracketDepth = 0;
    bool inQuotes = false;

    // Definim cuvintele cheie pentru o căutare mai curată
    const std::vector<std::wstring> openKeys = { L"WHILE", L"REPEAT", L"IF", L"FOR", L"CYCLE", L"SWITCH", L"PROC", L"FUNC" };
    const std::vector<std::wstring> closeKeys = { L"ENDWHILE", L"ENDREPEAT", L"ENDIF", L"ENDFOR", L"ENDCYCLE", L"ENDSWITCH", L"ENDPROC", L"ENDFUNC" };

    for (size_t i = 0; i < input.length(); ++i) {
        wchar_t c = input[i];

        // 1. Gestionare ghilimele (ignorăm conținutul lor)
        if (c == L'"' && (i == 0 || input[i - 1] != L'\\')) {
            inQuotes = !inQuotes;
        }

        if (!inQuotes) {
            // 2. Tracking paranteze (pentru Map/Array multi-line)
            if (c == L'{' || c == L'[') bracketDepth++;
            if (c == L'}' || c == L']') bracketDepth--;

            // 3. Tracking blocuri (START) - Ignorăm spațiile de indentare
            bool isPotentialStart = (i == 0 || iswspace(input[i - 1]) || input[i - 1] == L';');
            if (isPotentialStart && !iswspace(c)) {
                std::wstring_view remView(&input[i], input.length() - i);

                for (const auto& k : openKeys) {
                    if (remView.size() >= k.size()) {
                        // Verificare case-insensitive manuală pentru performanță
                        bool match = true;
                        for (size_t j = 0; j < k.size(); ++j) {
                            if (std::towupper(remView[j]) != k[j]) { match = false; break; }
                        }

                        if (match) {
                            // Verificăm să fie cuvânt întreg (nu parte din "IFFY")
                            size_t nextIdx = k.size();
                            if (nextIdx >= remView.size() || iswspace(remView[nextIdx]) || wcschr(L";()[]{}=+-*/", remView[nextIdx])) {
                                blockDepth++;
                                break;
                            }
                        }
                    }
                }
            }

            // 4. DECIZIA DE TĂIERE
            // Tăiem DOAR dacă suntem la adâncime 0 (toate blocurile sunt închise)
            if (blockDepth == 0 && bracketDepth == 0 && (c == L';' || c == L'\n')) {
                std::wstring cmd = trim(current);
                if (!cmd.empty()) result.push_back(cmd);
                current.clear();
                continue;
            }
        }

        current += c;

        // 5. Tracking blocuri (END)
        if (!inQuotes) {
            // Verificăm dacă tocmai am terminat de adăugat un keyword de închidere în 'current'
            bool isAtEnd = (i + 1 == input.length() || iswspace(input[i + 1]) || input[i + 1] == L';' || input[i + 1] == L'\n');
            if (isAtEnd) {
                for (const auto& k : closeKeys) {
                    if (current.length() >= k.length()) {
                        size_t startPos = current.length() - k.length();

                        // Validăm că keyword-ul de închidere este un cuvânt separat
                        bool validStart = (startPos == 0 || iswspace(current[startPos - 1]) || current[startPos - 1] == L';');
                        if (!validStart) continue;

                        bool match = true;
                        for (size_t j = 0; j < k.size(); ++j) {
                            if (std::towupper(current[startPos + j]) != k[j]) { match = false; break; }
                        }

                        if (match) {
                            blockDepth--;
                            if (blockDepth < 0) blockDepth = 0;
                            break;
                        }
                    }
                }
            }
        }
    }

    // Adăugăm restul rămas
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

        m_commandHandlers[L"LIST"] = wrap([this](const auto& sc) { handleListCommand(sc); });
        //m_commandHandlers[L"LP"] = m_commandHandlers[L"LIST_PROCS"];
        //m_commandHandlers[L"PROC_DUMP"] = m_commandHandlers[L"LIST_PROCS"];
        //m_commandHandlers[L"LIST_FUNCS"] = wrap([this](const auto& sc) { handleListFuncsCommand(sc); });
        
            

        m_commandHandlers[L"BREAK"] = wrap([this](const auto& sc) { handleBreakCommand(sc); });
        m_commandHandlers[L"CONTINUE"] = wrap([this](const auto& sc) { handleContinueCommand(sc); });

        m_commandHandlers[L"RETURN"] = wrap([this](const auto& sc) { handleReturnCommand(sc); });
        m_commandHandlers[L"RET"] = m_commandHandlers[L"RETURN"];

        m_commandHandlers[L"DEFINE"] = wrap([this](const auto& sc) { handleDefCommand(sc); });
        m_commandHandlers[L"DEF"] = m_commandHandlers[L"DEFINE"];

        m_commandHandlers[L"CONFIG"] = wrap([this](const auto& sc) { handleConfigCommand(sc); });
        m_commandHandlers[L"CONF"] = m_commandHandlers[L"CONFIG"];

        
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
            LOG_ERROR(L"Error during execution of the assignment AST node.");
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
        std::wstring cleanExpr = expr;

        // Dacă expresia începe cu "set ", îl eliminăm pentru a o transforma 
        // dintr-o comandă de shell într-o expresie de atribuire validă
        if (cleanExpr.substr(0, 4) == L"set ") {
            cleanExpr = cleanExpr.substr(4);
        }

        std::vector<std::wstring> tokens = vOliCommandParser::tokenize(cleanExpr);
        if (tokens.empty()) return { std::monostate{} };

        OliExpressionParser parser(tokens);
        ASTPtr plan = parser.parse();

        return executeAST(plan);
    }
    

    vData vOliEngine::resolveVariable(const std::wstring& rawVar) {
        std::wstring varName = trim(rawVar);
        if (varName.empty()) return { std::monostate{} };

       
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
            vDataMap mapResult = std::make_shared<std::unordered_map<std::wstring, vData>>();

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
                if (!(*pMapPtr)) *pMapPtr = std::make_shared<std::unordered_map<std::wstring, vData>>();

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
            container->value = std::make_shared<std::unordered_map<std::wstring, vData>>();
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
                if (!mapPtr) mapPtr = std::make_shared<std::unordered_map<std::wstring, vData>>();

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
            if (!mPtr) mPtr = std::make_shared<std::unordered_map<std::wstring, vData>>();

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
                    vDataMap myMap = std::make_shared<std::unordered_map<std::wstring, vData>>();
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
                    std::wstring raw = val.substr(1, val.size() - 2);
                    return { unescape(raw) };
                    //return { val.substr(1, val.size() - 2) };
                }
                return parseRawLiteral(val);
            }

            case ASTNodeType::Variable: {
                // resolveVariable gestionează acum și citirea prin pointer (*$ptr)
                return resolveVariable(node->value);
            }

            case ASTNodeType::FunctionCall: {
                std::wstring funcName = L"";
                std::vector<vData> evaluatedArgs;
                vData contextObj = { std::monostate{} };

                // --- 1. IDENTIFICARE CONTEXT ȘI NUME ---
                if (!node->children.empty() && (node->children[0]->value == L"." || node->children[0]->value == L"DOT")) {
                    // Caz: $obj.metoda()
                    const ASTPtr& dotNode = node->children[0];

                    // Evaluăm obiectul (partea stângă a punctului)
                    contextObj = executeAST(dotNode->children[0]);
                    std::wstring rawMethodName = dotNode->children[1]->value;

                    // Verificăm dacă obiectul aparține unei CLASE (Dynamic Dispatch)
                    if (contextObj.isMap()) {
                        auto m = contextObj.rawMap();
                        if (m->count(L"__type__")) {
                            std::wstring typeName = to_upper((*m)[L"__type__"].toWString());
                            std::wstring methodUpper = to_upper(rawMethodName);

                            // Căutăm în Blueprints dacă metoda este definită în clasă
                            if (m_blueprints.count(typeName)) {
                                auto& bp = m_blueprints[typeName];
                                if (bp.methods.count(methodUpper)) {
                                    // Am găsit metoda în clasă! Rezultă "CLASA::METODA"
                                    funcName = bp.methods[methodUpper];
                                }
                            }
                        }
                    }

                    // Fallback: Dacă nu e metodă de clasă, căutăm direct în obiect (metodă ad-hoc)
                    if (funcName.empty()) {
                        funcName = vDataToWString(executeAST(dotNode));
                    }

                    // Colectăm argumentele apelului (începând de la indexul 1 al nodului de apel)
                    for (size_t i = 1; i < node->children.size(); ++i)
                        evaluatedArgs.push_back(executeAST(node->children[i]));
                }
                else if (node->value == L"DYNAMIC_CALL" && !node->children.empty()) {
                    // Caz: $var()
                    funcName = vDataToWString(executeAST(node->children[0]));
                    for (size_t i = 1; i < node->children.size(); ++i)
                        evaluatedArgs.push_back(executeAST(node->children[i]));
                }
                else if (!node->value.empty() && node->value[0] == L'$') {
                    // Caz: apelare variabilă directă
                    funcName = vDataToWString(resolveVariable(node->value));
                    for (auto& child : node->children)
                        evaluatedArgs.push_back(executeAST(child));
                }
                else {
                    // Caz: apelare funcție globală după nume
                    funcName = node->value;
                    for (auto& child : node->children)
                        evaluatedArgs.push_back(executeAST(child));
                }

                if (funcName.empty()) return { std::monostate{} };

                // --- 2. CONSTRUCTORI (Instanțiere din Blueprints) ---
                std::wstring upperName = to_upper(funcName);
                auto itBlueprint = m_blueprints.find(upperName);
                if (itBlueprint != m_blueprints.end()) {
                    vDataMap instance = std::make_shared<std::unordered_map<std::wstring, vData>>();

                    // Marcăm obiectul cu tipul său (esențial pentru OP_CALL_METHOD și dispatch)
                    (*instance)[L"__type__"] = vData(itBlueprint->second.name);

                    const auto& fields = itBlueprint->second.fields;
                    for (size_t i = 0; i < fields.size(); ++i) {
                        // Inițializăm câmpurile cu argumentele date sau NULL
                        (*instance)[fields[i]] = (i < evaluatedArgs.size()) ? evaluatedArgs[i] : vData{ std::monostate{} };
                    }
                    return { instance };
                }

                // --- 3. EXECUȚIE (Native sau User Defined) ---
                // Căutăm în handler-ele interne (ECHO, PRINT, etc.)
                auto itInternal = m_functionsHandlers.find(upperName);
                if (itInternal != m_functionsHandlers.end()) {
                    return itInternal->second(evaluatedArgs);
                }

                // Căutăm în funcțiile definite de utilizator în script (FUNC)
                auto itUser = m_userFunctions.find(upperName);
                if (itUser != m_userFunctions.end()) {
                    // Injectăm contextObj (dacă există) pentru a deveni $this în funcție
                    return callUserFunction(upperName, evaluatedArgs, contextObj);
                }

                LOG_ERROR(L"[INTERPRETER ERROR] Unknown function or method: " + funcName);
                return { std::monostate{} };
            }
            case ASTNodeType::Operator: {
                std::wstring op = node->value;
                // --- 1. OPERATORI DE CITIRE (Evaluare R-Value) ---

                // Handler pentru ADDRESS_OF (&) - TREBUIE să fie aici, în afara blocului de assignment
                if (op == L"ADDRESS_OF" ) {
                    ASTPtr child = node->children[0];
                    if (child->type == ASTNodeType::Variable) {
                        std::wstring varName = child->value;
                        if (!varName.empty() && (varName[0] == L'$' || varName[0] == L'@')) varName.erase(0, 1);

                        vData* targetPtr = nullptr;
                        if (!m_callStack.empty()) {
                            auto& locals = m_callStack.back().localVariables;
                            if (locals.count(varName)) targetPtr = &locals[varName];
                        }
                        if (!targetPtr && m_globalVariables.count(varName)) targetPtr = &m_globalVariables[varName];

                        if (targetPtr) {
                            vData res; res.value = targetPtr;
                            return res;
                        }
                    }
                    LOG_ERROR(L"Runtime Error: Operator '&' requires a variable.");
                    return { std::monostate{} };
                }

                // Handler pentru DEREFERENCE (*) în modul citire (ECHO *ptr)
                if (op == L"DEREFERENCE") {
                    vData ptrContainer = executeAST(node->children[0]);
                    if (vData** addrPtr = std::get_if<vData*>(&ptrContainer.value)) {
                        if (*addrPtr) {
                            // IMPORTANT: Returnăm valoarea brută de la adresă, FĂRĂ getTrueData()!
                            // toWString() se va ocupa de afișare mai târziu oricum.
                            return **addrPtr;
                        }
                    }
                    LOG_ERROR(L"Runtime Error: Cannot dereference a non-pointer value.");
                    return { std::monostate{} };
                }

                // 1. Identificăm dacă este o formă de atribuire sau incrementare
                bool isCompound = (op == L"+=" || op == L"-=" || op == L"*=" || op == L"/=");
                bool isSimpleAssign = (op == L"=");
                bool isPostfix = (op == L"POSTFIX_INC" || op == L"POSTFIX_DEC");

                if (isSimpleAssign || isCompound || isPostfix) {
                    if (node->children.empty()) return { std::monostate{} };

                    ASTPtr leftNode = node->children[0];


                    // 1. Obținem valoarea curentă (pentru postfix sau operatori compuși +=)
                    vData currentVal = (isSimpleAssign) ? vData{} : executeAST(leftNode);
                    vData newValue;

                    if (isSimpleAssign) {
                        newValue = executeAST(node->children[1]);
                    }
                    else if (isPostfix) {
                        std::wstring baseOp = (op == L"POSTFIX_INC") ? L"+" : L"-";
                        newValue = executeBinaryOperator(baseOp, currentVal, vData(1LL));
                    }
                    else if (isCompound) {
                        vData rhsEvaluated = executeAST(node->children[1]);
                        std::wstring baseOp = op.substr(0, 1);
                        newValue = executeBinaryOperator(baseOp, currentVal, rhsEvaluated);
                    }
					
                    // --- 2. LOGICA DE SCRIERE (L-Value) ---

                    // A. SCRIERE PRIN POINTER EXPLICIT (*$b = ...)
                    if (leftNode->value == L"DEREFERENCE") {
                        vData ptrContainer = executeAST(leftNode->children[0]);
                        if (vData** addrPtr = std::get_if<vData*>(&ptrContainer.value)) {
                            if (*addrPtr && *addrPtr) {
                                **addrPtr = newValue;
                                // FIX CRITIC: Returnăm valoarea veche dacă e postfix!
                                return isPostfix ? currentVal : newValue;
                            }
                        }
                        LOG_ERROR(L"Runtime Error: Cannot dereference a null or invalid pointer.");
                        return { std::monostate{} };
                    }

                    if (leftNode->type == ASTNodeType::Variable) {
                        std::wstring rawName = leftNode->value;

                        // --- 1. DETECTARE MOD SCRIERE (*) ---
                        // Verificăm dacă scriem la o adresă stocată într-un pointer (ex: *$ptr = 10)
                        bool isPointer = (!rawName.empty() && rawName[0] == L'*');
                        std::wstring targetName = isPointer ? rawName.substr(1) : rawName;

                        // --- 2. GESTIONARE SCOPE GLOBAL (@) ---
                        bool forceGlobal = (!targetName.empty() && targetName[0] == L'@');
                        if (forceGlobal) {
                            // Normalizăm prefixul pentru ca bucla de indirație să poată procesa numele
                            targetName[0] = L'$';
                        }

                        // --- 3. REZOLVARE INDIRAȚIE DINAMICĂ ($$, $$$) ---
                        // Săpăm prin semnele de dolar până găsim variabila „container” finală
                        int safetyGuard = 0;
                        while (targetName.size() > 1 && targetName[0] == L'$' && targetName[1] == L'$') {
                            vData nextNameData = resolveVariable(targetName.substr(1));
                            std::wstring nextName = vDataToWString(nextNameData);

                            if (nextName.empty() || nextName == L"null") {
                                LOG_ERROR(L"Runtime Error: Indirection broken for " + targetName);
                                return { std::monostate{} };
                            }

                            // Re-atașăm prefixul necesar pentru următoarea iterație de resolve
                            targetName = (nextName[0] == L'$' || nextName[0] == L'@') ? nextName : L"$" + nextName;

                            if (++safetyGuard > 20) {
                                LOG_ERROR(L"Runtime Error: Circular reference in L-Value indirection.");
                                return { std::monostate{} };
                            }
                        }

                        // --- 4. RESTAURARE SCOPE (@) ---
                        if (forceGlobal && !targetName.empty()) {
                            if (targetName[0] == L'$') targetName[0] = L'@';
                            else if (targetName[0] != L'@') targetName = L"@" + targetName;
                        }

                        // --- 5. EXECUȚIE SCRIERE FINALĂ ---

                        if (isPointer) {
                            // --- CAZ A: Scrierea la adresa din pointer (*$var) ---
                            vData ptrInfo = resolveVariable(targetName);
                            if (vData** addrPtr = std::get_if<vData*>(&ptrInfo.value)) {
                                if (*addrPtr && *addrPtr) {
                                    **addrPtr = newValue;
                                    // CRITIC pentru Postfix: returnăm valoarea originală (currentVal)
                                    return isPostfix ? currentVal : newValue;
                                }
                            }
                            LOG_ERROR(L"Runtime Error: Invalid pointer write attempt via *" + targetName);
                            return { std::monostate{} };
                        }
                        else {
                            // --- CAZ B: Scrierea într-o variabilă directă ($var) ---
                            setVariable(targetName, newValue);

                            // CRITIC pentru Postfix: returnăm valoarea originală (currentVal)
                            // Astfel echo $a++ va afișa valoarea veche, deși în memorie este cea nouă.
                            return isPostfix ? currentVal : newValue;
                        }
                    }


                    // --- 2. Acces membru (obj.prop) ---
                    if (leftNode->value == L"DOT" || leftNode->value == L".") {
                        vData container = executeAST(leftNode->children[0]);
                        // AUTO-DEREFERENȚIERE: Mergem la obiectul real dacă avem pointer
                        // Sărim prin oricâte niveluri de pointeri (ptr -> ptr -> ptr -> obiect)
                        int jumpGuard = 0;
                        while (vData** addrPtr = std::get_if<vData*>(&container.value)) {
                            if (*addrPtr && *addrPtr) {
                                container = **addrPtr;
                            }
                            else break;

                            if (++jumpGuard > 20) {
                                LOG_ERROR(L"Runtime Error: Circular pointer reference detected.");
                                break;
                            }
                        }

                        std::wstring field = leftNode->children[1]->value;
                        if (container.isMap()) {
                            auto mapPtr = std::get<vDataMap>(container.value);
                            if (mapPtr) {
                                (*mapPtr)[field] = newValue;
                                return isPostfix ? currentVal : newValue;
                            }
                        }
                    }

                    // --- 3. Indexare (arr[index]) ---
                    if (leftNode->value == L"INDEX" || leftNode->value == L"[") {
                        vData container = executeAST(leftNode->children[0]);

                        // Sărim prin oricâte niveluri de pointeri (ptr -> ptr -> ptr -> obiect)
                        int jumpGuard = 0;
                        while (vData** addrPtr = std::get_if<vData*>(&container.value)) {
                            if (*addrPtr && *addrPtr) {
                                container = **addrPtr;
                            }
                            else break;

                            if (++jumpGuard > 20) {
                                LOG_ERROR(L"Runtime Error: Circular pointer reference detected.");
                                break;
                            }
                        }

                        vData index = executeAST(leftNode->children[1]);
                        if (container.isArray()) {
                            auto arrPtr = std::get<vDataArray>(container.value);
                            size_t idx = static_cast<size_t>(vDataToDouble(index));
                            if (arrPtr && idx < arrPtr->size()) {
                                (*arrPtr)[idx] = newValue;
                                return isPostfix ? currentVal : newValue;
                            }
                        }
                        else if (container.isMap()) {
                            auto mapPtr = std::get<vDataMap>(container.value);
                            if (mapPtr) {
                                (*mapPtr)[vDataToWString(index)] = newValue;
                                return isPostfix ? currentVal : newValue;
                            }
                        }
                    }

                    // Fallback pentru căi complexe
                    std::wstring fullPath = reconstructPath(leftNode);
                    if (!fullPath.empty()) {
                        assignToVariable(fullPath, newValue);
                        return isPostfix ? currentVal : newValue;
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

                    // ACEEAȘI AUTO-DEREFERENȚIERE AICI
                    // Sărim prin oricâte niveluri de pointeri (ptr -> ptr -> ptr -> obiect)
                    int jumpGuard = 0;
                    while (vData** addrPtr = std::get_if<vData*>(&container.value)) {
                        if (*addrPtr && *addrPtr) {
                            container = **addrPtr;
                        }
                        else break;

                        if (++jumpGuard > 20) {
                            LOG_ERROR(L"Runtime Error: Circular pointer reference detected.");
                            break;
                        }
                    }

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

                    // Sărim prin oricâte niveluri de pointeri (ptr -> ptr -> ptr -> obiect)
                    int jumpGuard = 0;
                    while (vData** addrPtr = std::get_if<vData*>(&container.value)) {
                        if (*addrPtr && *addrPtr) {
                            container = **addrPtr;
                        }
                        else break;

                        if (++jumpGuard > 20) {
                            LOG_ERROR(L"Runtime Error: Circular pointer reference detected.");
                            break;
                        }
                    }

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
                    if (node->value == L"BITWISE_NOT" || node->value == L"~") {
                        return { ~vDataToLong(operand) }; // Presupunem că ai un helper vDataToLong
                    }
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
            if (!mapPtr) mapPtr = std::make_shared<std::unordered_map<std::wstring, vData>>();

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

    


vData vOliEngine::executeBinaryOperator(const std::wstring& op, const vData& left, const vData& right) {
    // --- 1. OPERATORI DE COALESCENCE ---
    if (op == L"??") {
        return left.isNull() ? right : left;
    }

    // --- 2. LOGICĂ DE EGALITATE ---
    if (op == L"==") {
        if (left.isNull() && right.isNull()) return { true };

        bool leftIsPtr = std::holds_alternative<vData*>(left.value);
        bool rightIsPtr = std::holds_alternative<vData*>(right.value);

        if (leftIsPtr || rightIsPtr) {
            if (leftIsPtr && rightIsPtr) {
                return { std::get<vData*>(left.value) == std::get<vData*>(right.value) };
            }
            if (leftIsPtr && right.isNull()) return { std::get<vData*>(left.value) == nullptr };
            if (rightIsPtr && left.isNull()) return { std::get<vData*>(right.value) == nullptr };
            return { false };
        }

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

    // --- 3. CONCATENARE EXPLICITĂ (..) ---
    // Aceasta forțează transformarea ambelor părți în string
    if (op == L".." || op == L"CONCAT") {
        return { vDataToWString(left) + vDataToWString(right) };
    }

    // --- 4. ADUNAREA / CONCATENAREA IMPLICITĂ (+) ---
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

    // --- 5. BARIERĂ PENTRU OPERAȚII STRICTE ---
    if (left.isNull() || right.isNull() ||
        std::holds_alternative<vData*>(left.value) ||
        std::holds_alternative<vData*>(right.value)) {
        return vData();
    }

    // --- 6. OPERAȚII NUMERICE ȘI BITWISE ---
    if (canBeNumeric(left) && canBeNumeric(right)) {

        // Exponentiere (Prioritate mare)
        //if (op == L"^" || op == L"**") {
        if (op == L"**") {
            return { std::pow(vDataToDouble(left), vDataToDouble(right)) };
        }

        // --- ADĂUGAT: OPERATORI PE BIȚI ---
        // Operăm pe long long pentru precizie binară
        // --- OPERATORI PE BIȚI (Am adăugat L"^" aici) ---
        if (op == L"&" || op == L"|" || op == L"BXOR" || op == L"^" || op == L"<<" || op == L">>") {
            long long iL = (left.isInt()) ? std::get<long long>(left.value) : (long long)vDataToDouble(left);
            long long iR = (right.isInt()) ? std::get<long long>(right.value) : (long long)vDataToDouble(right);

            if (op == L"&")    return { iL & iR };
            if (op == L"|")    return { iL | iR };
            if (op == L"BXOR" || op == L"^") return { iL ^ iR }; // XOR binar
            if (op == L"<<")   return { iL << iR };
            if (op == L">>")   return { iL >> iR };
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

    // --- 7. OPERATORI LOGICI ---
    if (op == L"&&") return { vDataToBool(left) && vDataToBool(right) };
    if (op == L"||") return { vDataToBool(left) || vDataToBool(right) };

    // --- 8. STRING COMPARISON ---
    if (left.isString() || right.isString()) {
        std::wstring sL = vDataToWString(left);
        std::wstring sR = vDataToWString(right);
        if (op == L"<")  return { sL < sR };
        if (op == L">")  return { sL > sR };
    }

    return vData();
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
    

    double vOliEngine::vDataToDouble(const vData& data) const {
        const vData* current = &data;
        int jumpGuard = 0;

        // 1. REZOLVARE POINTERI (Dereferențiere iterativă)
        while (std::holds_alternative<vData*>(current->value)) {
            vData* next = std::get<vData*>(current->value);
            if (!next || next == current || ++jumpGuard > 10) break;
            current = next;
        }

        // 2. CONVERSIE VALOARE REALĂ
        return std::visit([this](auto&& arg) -> double {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, double>) return arg;
            if constexpr (std::is_same_v<T, long long>) return static_cast<double>(arg);
            if constexpr (std::is_same_v<T, bool>) return arg ? 1.0 : 0.0;
            if constexpr (std::is_same_v<T, std::wstring>) {
                if (arg.empty()) return 0.0;
                try { return std::stod(arg); }
                catch (...) { return 0.0; }
            }
            return 0.0; // Map, Array, Null, etc.
            }, current->value);
    }
    
    long long vOliEngine::vDataToLong(const vData& data) {
        // PASUL 1: Întotdeauna extragem datele reale (eliminăm "blindajul" de pointer)
        const vData& actual = data.getTrueData();

        // PASUL 2: Conversia propriu-zisă pe datele dereferențiate
        if (std::holds_alternative<long long>(actual.value)) {
            return std::get<long long>(actual.value);
        }

        if (std::holds_alternative<double>(actual.value)) {
            // Conversie cu trunchiere de la float la int
            return static_cast<long long>(std::get<double>(actual.value));
        }

        if (std::holds_alternative<std::wstring>(actual.value)) {
            try {
                return std::stoll(std::get<std::wstring>(actual.value));
            }
            catch (...) {
                return 0; // String-ul nu este un număr valid
            }
        }

        if (std::holds_alternative<bool>(actual.value)) {
            return std::get<bool>(actual.value) ? 1 : 0;
        }

        // Default pentru Array, Map sau Null
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
        // 1. Găsim delimitatorii la nivelul de top
        size_t posThen = findTopLevelIfKeyword(fullLine, L"THEN");
        size_t posElse = findTopLevelIfKeyword(fullLine, L"ELSE");
        size_t posEndif = findTopLevelIfKeyword(fullLine, L"ENDIF");

        // --- LOGICĂ DE ERORI (GUARD RAILS) ---
        if (posThen == std::wstring::npos) {
            LOG_ERROR(L"Sintaxă IF invalidă: Lipsește 'THEN'. Oli nu știe când să înceapă execuția.");
            return;
        }

        if (posEndif == std::wstring::npos) {
            LOG_ERROR(L"Sintaxă IF invalidă: Blocul IF nu este închis. Lipsește 'ENDIF'.");
            return;
        }

        // Verificăm ordinea logică (THEN trebuie să fie înainte de ENDIF)
        if (posThen > posEndif) {
            LOG_ERROR(L"Structură IF coruptă: 'THEN' apare după 'ENDIF'. Verifică imbricarea blocurilor.");
            return;
        }

        // Verificăm ELSE (dacă există, trebuie să fie între THEN și ENDIF)
        if (posElse != std::wstring::npos && (posElse < posThen || posElse > posEndif)) {
            LOG_ERROR(L"Structură IF coruptă: 'ELSE' este plasat în afara limitelor THEN-ENDIF.");
            return;
        }
        // -------------------------------------

        // 2. Evaluăm Condiția (între 'IF' și 'THEN')
        // Substr de la indexul 2 (după 'IF') până la 'THEN'
        std::wstring conditionPart = fullLine.substr(2, posThen - 2);
        vData result = evaluateExpression(normalizeSpaces(conditionPart));
        bool isTrue = vDataToBool(result);

        // 3. Extragem blocul de cod corect
        std::wstring commandToRun;
        if (isTrue) {
            size_t start = posThen + 4; // după "THEN"
            size_t end = (posElse != std::wstring::npos) ? posElse : posEndif;
            commandToRun = fullLine.substr(start, end - start);
        }
        else if (posElse != std::wstring::npos) {
            size_t start = posElse + 4; // după "ELSE"
            commandToRun = fullLine.substr(start, posEndif - start);
        }

        // 4. Execuție recursivă folosind preParse (pe codul brut, multiline)
        if (!trim(commandToRun).empty()) {
            std::vector<std::wstring> subInstructions = preParse(commandToRun);
            for (const auto& subInstr : subInstructions) {
                this->executeInternal(subInstr);

                // Verificăm dacă instrucțiunea a cerut oprirea execuției (BREAK/RETURN)
                if (m_executionStatus != OliStatus::RUNNING) return;
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



    


    void vOliEngine::debugWhile(const std::wstring& condition, const std::vector<std::wstring>& instrs) {
        LOG_INFO(L"--- [DEBUG WHILE] ---");
        LOG_INFO(L"Conditie: " + condition);
        LOG_INFO(L"Instructiuni detectate: " + std::to_wstring(instrs.size()));
        for (size_t i = 0; i < instrs.size(); ++i) {
            LOG_INFO(L"  [" + std::to_wstring(i) + L"]: " + instrs[i]);
        }
        LOG_INFO(L"---------------------");
    }

    void vOliEngine::handleWhileCommand(const std::wstring& fullLine) {
        // 1. Pregătim o versiune UPPER pentru căutare (case-insensitivity)
        std::wstring upperLine = fullLine;
        std::transform(upperLine.begin(), upperLine.end(), upperLine.begin(), ::towupper);

        // 2. Găsim pozițiile folosind upperLine pentru a ignora casing-ul (do vs DO)
        // IMPORTANT: Folosim upperLine ca sursă de căutare, indicii vor fi identici pentru fullLine
        size_t whilePos = upperLine.find(L"WHILE");
        size_t posDo = findTopLevelKeyword(upperLine, L"DO", L"WHILE");
        size_t posEnd = findTopLevelKeyword(upperLine, L"ENDWHILE", L"WHILE");
        if (posEnd == std::wstring::npos) posEnd = upperLine.rfind(L"ENDWHILE");

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

        //debugWhile(conditionPart, instructions);

        if (instructions.empty()) return;

        // 5. Bucla de execuție a motorului Oli
        int safetyBreak = 0;
        while (true) {
            // Safety check pentru a nu bloca procesorul în bucle infinite
            if (m_maxIterations > 0) {
                if (++safetyBreak > m_maxIterations) {
                    LOG_ERROR(L"Safety limit reached (" + std::to_wstring(m_maxIterations) +
                        L" iterations). Use 'CONFIG MAX_ITERATIONS 0' for infinite loops.");
                    break;
                }
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

    
    
    // Helper pentru a detecta orice fel de whitespace (inclusiv Non-Breaking Space)
    inline bool isOliWhitespace(wchar_t c) {
        return iswspace(c) || c == L'\xA0';
    }

    // Helper pentru a detecta separatori de cuvinte
    inline bool isOliSeparator(wchar_t c) {
        if (isOliWhitespace(c)) return true;
        return wcschr(L"=+-*<>|;()[]{},:%/#!", c) != nullptr;
    }

    size_t vOliEngine::findTopLevelKeyword(const std::wstring& line, const std::wstring& keyword, const std::wstring& startCommand) {
        int depth = 0;
        bool inQuotes = false;

        // Convertim totul la Upper o singură dată pentru eficiență
        std::wstring upperLine = line;
        for (auto& c : upperLine) c = std::towupper(c);
        std::wstring upperKey = keyword;
        for (auto& c : upperKey) c = std::towupper(c);
        std::wstring upperStart = startCommand;
        for (auto& c : upperStart) c = std::towupper(c);

        size_t mainPos = upperLine.find(upperStart);
        if (mainPos == std::wstring::npos) return std::wstring::npos;

        size_t searchStart = mainPos + upperStart.length();

        for (size_t i = searchStart; i < upperLine.size(); ++i) {
            // 1. Skip ghilimele
            if (upperLine[i] == L'"' && (i == 0 || upperLine[i - 1] != L'\\')) {
                inQuotes = !inQuotes;
                continue;
            }
            if (inQuotes) continue;

            // 2. Verificăm început de cuvânt folosind helper-ul robust
            bool isStart = (i == 0 || isOliSeparator(upperLine[i - 1]));
            if (!isStart) continue;

            std::wstring_view rem(&upperLine[i], upperLine.size() - i);

            // --- A. DETECTARE KEYWORD ȚINTĂ ---
            if (depth == 0 && rem.starts_with(upperKey)) {
                size_t nextIdx = i + upperKey.length();
                if (nextIdx >= upperLine.size() || isOliSeparator(upperLine[nextIdx])) {
                    return i;
                }
            }

            // --- B. TRACKING ADÂNCIME ---
            // Incrementăm pentru orice bloc nou
            static const std::vector<std::wstring> startTokens = { L"WHILE", L"IF", L"FOR", L"REPEAT", L"FUNC", L"PROC" };
            for (const auto& token : startTokens) {
                if (rem.starts_with(token)) {
                    size_t nextIdx = i + token.length();
                    if (nextIdx >= upperLine.size() || isOliSeparator(upperLine[nextIdx])) {
                        depth++;
                        i = nextIdx - 1; // Avansăm indexul
                        goto next_loop_iter;
                    }
                }
            }

            // Decrementăm pentru orice închidere
            if (rem.starts_with(L"END") || rem.starts_with(L"UNTIL")) {
                // Nu scădem dacă suntem deja la 0 (înseamnă că END-ul găsit este chiar keyword-ul nostru)
                if (depth > 0) depth--;

                // Sărim peste cuvântul END... (ex: ENDWHILE)
                while (i < upperLine.size() && iswalnum(upperLine[i])) i++;
                i--;
            }

        next_loop_iter:;
        }

        return std::wstring::npos;
    }
    

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

        if (m_blockDepth > 0 || m_bracketDepth > 0) {
            LOG_ERROR(L"Unexpected end of script: unclosed blocks detected! (Depth: " + std::to_wstring(m_blockDepth) + L")");

            // --- RESET CRITIC ---
            m_blockDepth = 0;
            m_bracketDepth = 0;
            m_accumulator.clear();
        }
    }

    

    

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

    

    /*
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

    */
    void vOliEngine::handleProcCommand(const ShellCommand& sc) {
        if (sc.args.empty()) {
            LOG_ERROR(L"Usage: proc name [param1, param2...]");
            return;
        }

        // 1. Extragere și curățare nume procedură
        std::wstring procName = sc.args[0];
        procName.erase(std::remove_if(procName.begin(), procName.end(), [](wchar_t c) {
            return c == L',' || c == L'(' || c == L')';
            }), procName.end());

        // --- REPARAȚIA: Normalizăm numele imediat după curățare ---
        std::transform(procName.begin(), procName.end(), procName.begin(), ::towupper);

        if (vOliKeyWords::isInternalFixedCommand(procName)) {
            LOG_ERROR(L"Cannot shadow INTERNAL system command: " + procName);
            return;
        }

        if (m_procedures.count(procName)) {
            LOG_INFO(L"Overwriting existing procedure: " + procName);
        }

        // Setează contextul activ (acum garantat UPPERCASE)
        m_activeProcName = procName;

        Procedure newProc;
        newProc.name = m_activeProcName;
        newProc.params.clear();
        newProc.body.clear();

        // 2. Extragem parametrii (aici pot rămâne case-sensitive pentru contextul intern)
        for (size_t i = 1; i < sc.args.size(); ++i) {
            std::wstring arg = sc.args[i];
            arg.erase(std::remove_if(arg.begin(), arg.end(), [](wchar_t c) {
                return c == L'[' || c == L']' || c == L',' || c == L'(' || c == L')';
                }), arg.end());

            if (!arg.empty()) {
                newProc.params.push_back(arg);
            }
        }

        // 3. Activăm starea de înregistrare folosind cheia normalizată
        m_procedures[m_activeProcName] = newProc;
        m_isRecording = true;
        m_isRecordingFunc = false;

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


    

void vOliEngine::callProcedure(const Procedure& proc, const std::vector<std::wstring>& passedArgs) {
    // 1. EVALUĂM ARGUMENTELE ÎN CONTEXTUL APELANTULUI
    // Facem asta PRIMA DATĂ, cât timp m_callStack.back() este încă vechiul context.
    std::unordered_map<std::wstring, vData> evaluatedParams;

    for (size_t i = 0; i < proc.params.size(); ++i) {
        std::wstring pName = cleanVariableName(proc.params[i]);
        
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
    if (sc.args.empty()) {
        LOG_ERROR(L"Usage: plugin \"path/to/plugin\"");
        return;
    }

    std::wstring dllPath = sc.args[0];

    // Scoatem ghilimelele
    if (dllPath.size() >= 2 && dllPath.front() == L'"' && dllPath.back() == L'"') {
        dllPath = dllPath.substr(1, dllPath.size() - 2);
    }

    // Adăugăm extensia corectă dacă lipsește
    std::wstring ext = PortTools::getPluginExtension();
    if (dllPath.size() < ext.size() ||
        dllPath.substr(dllPath.size() - ext.size()) != ext)
    {
        dllPath += ext;
    }

    // --- 2. Încărcăm biblioteca ---
    PortTools::LibHandle hLib = PortTools::loadDynamicLibrary(dllPath);

    if (!hLib) {
        LOG_ERROR(L"Could not load plugin: " + dllPath +
            L" (Error: " + PortTools::getLastErrorString() + L")");
        return;
    }

    typedef void (*RegisterFunc)(std::unordered_map<std::wstring, OliFunctionHandler>&);
    RegisterFunc regFunc = (RegisterFunc)PortTools::getFunctionSymbol(hLib, "LoadOliPlugin");

    if (regFunc) {
        regFunc(this->m_functionsHandlers);
        LOG_SUCCESS(L"Plugin loaded: " + dllPath);
        LOG_SUCCESS(L"          Native functions injected into Oli memory.");
    }
    else {
        LOG_ERROR(L"Invalid Plugin: Export 'LoadOliPlugin' not found in " + dllPath);
        PortTools::freeDynamicLibrary(hLib);
    }
}*/
/*
void vOliEngine::handlePluginCommand(const ShellCommand& sc) {
    if (sc.args.empty()) {
        LOG_ERROR(L"Usage: plugin \"path/to/plugin\"");
        return;
    }

    std::wstring pluginName = sc.args[0];

    // 1. Curățăm ghilimelele
    if (pluginName.size() >= 2 && pluginName.front() == L'"' && pluginName.back() == L'"') {
        pluginName = pluginName.substr(1, pluginName.size() - 2);
    }

    // 2. Determinăm calea finală (Logica de Default Path)
    std::wstring dllPath;

    // Dacă numele pluginului NU conține separatoare de directoare (/ sau \), 
    // înseamnă că e doar un nume simplu și îl căutăm în folderul de plugin-uri setat.
    if (pluginName.find(L'/') == std::wstring::npos && pluginName.find(L'\\') == std::wstring::npos) {
        dllPath = m_pluginsPath + pluginName;
    }
    else {
        dllPath = pluginName; // Este o cale specifică (relativă sau absolută)
    }

    // 3. Adăugăm extensia corectă (.dll sau .so)
    std::wstring ext = PortTools::getPluginExtension();
    if (dllPath.size() < ext.size() ||
        dllPath.substr(dllPath.size() - ext.size()) != ext)
    {
        dllPath += ext;
    }

    // --- 4. Încărcăm biblioteca (Restul rămâne la fel) ---
    PortTools::LibHandle hLib = PortTools::loadDynamicLibrary(dllPath);

    if (!hLib) {
        LOG_ERROR(L"Could not load plugin: " + dllPath +
            L" (Error: " + PortTools::getLastErrorString() + L")");
        return;
    }

    typedef void (*RegisterFunc)(std::unordered_map<std::wstring, OliFunctionHandler>&);
    RegisterFunc regFunc = (RegisterFunc)PortTools::getFunctionSymbol(hLib, "LoadOliPlugin");

    if (regFunc) {
        regFunc(this->m_functionsHandlers);
        LOG_SUCCESS(L"Plugin loaded: " + dllPath);
    }
    else {
        LOG_ERROR(L"Invalid Plugin: Export 'LoadOliPlugin' not found in " + dllPath);
        PortTools::freeDynamicLibrary(hLib);
    }
}
*/

/*
void vOliEngine::handlePluginCommand(const ShellCommand& sc) {
    if (sc.args.empty()) {
        LOG_ERROR(L"Usage: plugin \"path/to/plugin\"");
        return;
    }
    // Tot ce făceai înainte este acum încapsulat aici:
    this->internalLoadPlugin(sc.args[0]);
}
*/

void vOliEngine::handlePluginCommand(const ShellCommand& sc) {
    if (sc.args.empty()) {
        LOG_ERROR(L"Usage: plugin \"path/to/plugin\"");
        return;
    }

    std::wstring pluginName = sc.args[0];

    // 1. Curățăm ghilimelele
    if (pluginName.size() >= 2 && pluginName.front() == L'"' && pluginName.back() == L'"') {
        pluginName = pluginName.substr(1, pluginName.size() - 2);
    }

    // 2. Determinăm calea finală
    std::wstring dllPath;
    if (pluginName.find(L'/') == std::wstring::npos && pluginName.find(L'\\') == std::wstring::npos) {
        dllPath = m_pluginsPath + pluginName;
    }
    else {
        dllPath = pluginName;
    }

    // 3. Adăugăm extensia corectă (.dll / .so)
    std::wstring ext = PortTools::getPluginExtension();
    if (dllPath.size() < ext.size() ||
        dllPath.substr(dllPath.size() - ext.size()) != ext)
    {
        dllPath += ext;
    }

    // 4. Încărcăm biblioteca
    PortTools::LibHandle hLib = PortTools::loadDynamicLibrary(dllPath);

    if (!hLib) {
        LOG_ERROR(L"Could not load plugin: " + dllPath +
            L" (Error: " + PortTools::getLastErrorString() + L")");
        return;
    }

    bool loadedAnything = false;

    // --- A. Încărcare FUNCȚII (Sistemul Vechi) ---
    //typedef void (*RegisterFunc)(std::unordered_map<std::wstring, OliFunctionHandler>&);
    //RegisterFunc regFunc = (RegisterFunc)PortTools::getFunctionSymbol(hLib, "LoadOliPlugin");

    typedef void (*LoadFunctionsFunc)(std::unordered_map<std::wstring, OliFunctionHandler>&, IOliEngine*);
    LoadFunctionsFunc regFunc = (LoadFunctionsFunc)PortTools::getFunctionSymbol(hLib, "LoadOliPlugin");

    if (regFunc) {
        std::unordered_map<std::wstring, OliFunctionHandler> pluginFuncs;
        try {
            // 1. Executăm funcția din plugin (acum alinierea de memorie este perfectă)
            regFunc(pluginFuncs, this);

            // ⚠️ CRITIC: Nu uita să muți funcțiile injectate în map-ul principal al motorului!
            for (auto const& [name, handler] : pluginFuncs) {
                std::wstring upName = name;
                for (auto& c : upName) c = std::towupper(c);

                // Înregistrăm în VM cu numele normalizat în litere mari
                this->m_functionsHandlers[upName] = handler;

                // Anunțăm parserul că este o funcție nativă validă
                vOliKeyWords::registerNativeFunction(upName);
            }

            LOG_SUCCESS(L"Functions injected from: " + dllPath);
            loadedAnything = true;
        }
        catch (...) {
            LOG_ERROR(L"Exception in LoadOliPlugin");
        }
    }

    // --- B. Încărcare COMENZI (Sistemul Nou prin Interfață) ---
    // Folosim typedef-ul definit în IOliEngine.hpp
    LoadCommandsFunc regCmds = (LoadCommandsFunc)PortTools::getFunctionSymbol(hLib, "LoadOliCommandPlugin");

    if (regCmds) {
        // Păstrăm o listă cu ce era înainte în map pentru a vedea ce s-a adăugat
    // Sau, mai simplu, iterăm prin map-ul de handlere după înregistrare
        size_t countBefore = this->m_commandHandlers.size();

        regCmds(this->m_commandHandlers, this);

        // Înregistrăm noile chei în vOliKeyWords pentru a fi recunoscute ca instrucțiuni valide
        for (auto const& [name, handler] : this->m_commandHandlers) {
            // Înregistrăm tot ce e în map în lista de comenzi dinamice
            vOliKeyWords::registerDynamicCommand(name);
        }

        LOG_SUCCESS(L"Commands injected and registered in KeyWords: " + dllPath);
        loadedAnything = true;
    }

    // 5. Verificare validitate plugin
    if (loadedAnything) {
        LOG_SUCCESS(L"Plugin '" + pluginName + L"' is fully operational.");
    }
    else {
        LOG_ERROR(L"Invalid Plugin: No 'LoadOliPlugin' or 'LoadOliCommandPlugin' found in " + dllPath);
        PortTools::freeDynamicLibrary(hLib);
    }
}


void vOliEngine::handleListProcsCommand(const ShellCommand& sc) {
    if (m_procedures.empty()) {
        ConsoleManager::getInstance().writeRaw(L"No user procedures defined.");
        return;
    }

    // 1. Colectăm cheile într-un vector pentru sortare
    std::vector<std::wstring> sortedNames;
    for (auto const& [name, _] : m_procedures) {
        sortedNames.push_back(name);
    }

    // 2. Sortăm alfabetic
    std::sort(sortedNames.begin(), sortedNames.end());

    ConsoleManager::getInstance().writeRaw(L"--- [User Defined Procedures] ---");
    ConsoleManager::getInstance().writeRaw(L"NAME            PARAMETERS");
    ConsoleManager::getInstance().writeRaw(L"---------------------------------");

    // 3. Afișăm folosind vectorul sortat
    for (const auto& name : sortedNames) {
        const auto& proc = m_procedures[name];
        std::wstring paramsStr = L"[";
        for (size_t i = 0; i < proc.params.size(); ++i) {
            paramsStr += proc.params[i] + (i < proc.params.size() - 1 ? L", " : L"");
        }
        paramsStr += L"]";

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



void vOliEngine::handleListCommand(const ShellCommand& sc) {
    if (sc.args.empty()) {
        ConsoleManager::getInstance().writeRaw(L"Usage: LIST [FUNCS|PROCS|FUNC <name>|PROC <name>]");
        return;
    }

    // 1. Normalizăm sub-comanda (FUNCS, PROCS, FUNC, etc.)
    std::wstring subCommand = sc.args[0];
    std::transform(subCommand.begin(), subCommand.end(), subCommand.begin(), ::towupper);

    if (subCommand == L"FUNCS") {
        handleListFuncsCommand(sc);
    }
    else if (subCommand == L"PROCS") {
        handleListProcsCommand(sc);
    }
    else if (subCommand == L"FUNC" || subCommand == L"PROC") {
        if (sc.args.size() < 2) {
            ConsoleManager::getInstance().writeRaw(L"Error: Please specify a name.");
            return;
        }

        // 2. IMPORTANT: Normalizăm și numele căutat (ex: "test" -> "TEST")
        std::wstring targetName = sc.args[1];
        std::transform(targetName.begin(), targetName.end(), targetName.begin(), ::towupper);

        if (subCommand == L"FUNC") {
            dumpFunctionDetails(targetName);
        }
        else {
            dumpProcedureDetails(targetName);
        }
    }
    else {
        ConsoleManager::getInstance().writeRaw(L"Unknown sub-command: " + subCommand);
    }
}

std::wstring formatParams(const std::vector<std::wstring>& params) {
    std::wstring res = L"[";
    for (size_t i = 0; i < params.size(); ++i) {
        res += params[i] + (i < params.size() - 1 ? L", " : L"");
    }
    return res + L"]";
}

void vOliEngine::dumpFunctionDetails(const std::wstring& name) {
    // Căutăm în funcțiile native
    if (m_functionsHandlers.find(name) != m_functionsHandlers.end()) {
        ConsoleManager::getInstance().writeRaw(L"Function '" + name + L"' is a NATIVE (C++) handler.");
        return;
    }

    // Căutăm în funcțiile utilizator
    auto it = m_userFunctions.find(name);
    if (it != m_userFunctions.end()) {
        const auto& func = it->second;
        ConsoleManager::getInstance().writeRaw(L"--- [User Function: " + name + L"] ---");

        // Afișăm parametrii
        std::wstring p = L"Parameters: [";
        for (size_t i = 0; i < func.params.size(); ++i)
            p += func.params[i] + (i < func.params.size() - 1 ? L", " : L"");
        ConsoleManager::getInstance().writeRaw(p + L"]");

        // Afișăm corpul
        ConsoleManager::getInstance().writeRaw(L"Body:");
        if (func.body.empty()) {
            ConsoleManager::getInstance().writeRaw(L"  (empty body)");
        }
        else {
            for (const auto& line : func.body) {
                ConsoleManager::getInstance().writeRaw(L"  " + line);
            }
        }
    }
    else {
        ConsoleManager::getInstance().writeRaw(L"Error: Function '" + name + L"' not found.");
    }
}

void vOliEngine::dumpProcedureDetails(const std::wstring& name) {
    if (m_procedures.count(name)) {
        auto& proc = m_procedures[name];
        ConsoleManager::getInstance().writeRaw(L"--- [User Procedure: " + name + L"] ---");
        ConsoleManager::getInstance().writeRaw(L"Parameters: " + formatParams(proc.params));
        ConsoleManager::getInstance().writeRaw(L"Body:");
        for (const auto& line : proc.body) {
            ConsoleManager::getInstance().writeRaw(L"  " + line);
        }
    }
    else {
        ConsoleManager::getInstance().writeRaw(L"Error: Procedure '" + name + L"' not found.");
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



 /*

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

  */
  void vOliEngine::handleFuncCommand(const ShellCommand& sc) {
    if (sc.args.empty()) {
        LOG_ERROR(L"Usage: func name(param1, param2, ...)");
        return;
    }

    // Reconstruim linia pentru procesare
    std::wstring fullLine;
    for (const auto& arg : sc.args) fullLine += arg;

    size_t openParen = fullLine.find(L'(');
    size_t closeParen = fullLine.find(L')');

    // --- VALIDARE CRITICĂ ---
    if (openParen == std::wstring::npos || closeParen == std::wstring::npos || closeParen < openParen) {
        LOG_ERROR(L"[PARSER ERROR] Missing parentheses in function definition!");
        LOG_ERROR(L"Correct syntax: func name(params)");
        return; // Oprim execuția aici pentru a preveni înregistrarea unei funcții corupte
    }

    std::wstring funcName = fullLine.substr(0, openParen);
    std::wstring paramsStr = fullLine.substr(openParen + 1, closeParen - openParen - 1);

    funcName = to_upper(trim(funcName));
    if (funcName.empty()) {
        LOG_ERROR(L"[PARSER ERROR] Function name cannot be empty.");
        return;
    }

    m_activeFuncName = funcName;
    Procedure newFunc;
    newFunc.name = m_activeFuncName;
    newFunc.isVariadic = false;

    // Procesare parametri
    std::vector<std::wstring> tokens = wexplode(paramsStr, L',');
    for (auto& t : tokens) {
        std::wstring p = trim(t);
        if (p == L"...") {
            newFunc.isVariadic = true;
        }
        else if (!p.empty()) {
            newFunc.params.push_back(cleanVariableName(p));
        }
    }

    m_userFunctions[m_activeFuncName] = newFunc;
    m_isRecordingFunc = true;

    LOG_INFO(L"Started recording function: " + m_activeFuncName +
        (newFunc.isVariadic ? L" (Variadic support enabled)" : L""));
}


  /*
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
  }*/


  vData vOliEngine::callUserFunction(const std::wstring& funcName, const std::vector<vData>& args, vData context) {
      // 1. Căutăm funcția în map-ul de funcții utilizator
      auto it = m_userFunctions.find(funcName);
      if (it == m_userFunctions.end()) {
          LOG_ERROR(L"Runtime Error: Function '" + funcName + L"' not found.");
          return { std::monostate{} };
      }

      const Procedure& func = it->second;

      // 2. Pregătirea noului cadru de stivă (Stack Frame)
      StackFrame frame;
      frame.functionName = funcName;

      // 3. Injectarea contextului 'this' (pentru metode de obiect)
      frame.localVariables[L"this"] = context;

      // 4. Maparea parametrilor ficși
      for (size_t i = 0; i < func.params.size(); ++i) {
          std::wstring pName = func.params[i];
          // Atribuim valoarea primită sau NULL dacă argumentul lipsește
          frame.localVariables[pName] = (i < args.size()) ? args[i] : vData{ std::monostate{} };
      }

      // 5. Gestionarea argumentelor variadice (...)
      if (func.isVariadic) {
          // Creăm un vDataArray (shared_ptr către std::vector<vData>)
          vData extraParams = vData::CreateArray();
          std::vector<vData>* vecPtr = extraParams.rawArray();

          if (vecPtr && args.size() > func.params.size()) {
              for (size_t i = func.params.size(); i < args.size(); ++i) {
                  vecPtr->push_back(args[i]);
              }
          }
          // Injectăm lista de argumente extra în variabila locală 'params'
          frame.localVariables[L"params"] = extraParams;
      }

      // Inițializăm variabila de return implicită
      frame.localVariables[L"return"] = { std::monostate{} };

      // 6. Adăugăm cadrul în stivă și pregătim execuția
      m_callStack.push_back(std::move(frame));
      bool previousShouldReturn = m_shouldReturn;
      m_shouldReturn = false;

      // 7. Construirea și execuția corpului funcției
      std::wstring fullBody;
      for (const auto& line : func.body) {
          std::wstring cleanLine = trim(line);
          if (cleanLine.empty()) continue;
          fullBody += cleanLine + (cleanLine.back() == L';' ? L"\n" : L";\n");
      }

      this->executeInternal(fullBody);

      // 8. Preluarea rezultatului din 'return' înainte de a elimina cadrul
      vData result = { std::monostate{} };
      if (!m_callStack.empty()) {
          result = m_callStack.back().localVariables[L"return"];
          m_callStack.pop_back();
      }

      // Restaurăm starea flag-ului de return pentru apelant
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
  /*
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
          LOG_ERROR(L"Malformed FOR statement: TO, DO or ENDFOR keyword missing.");
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
          LOG_ERROR(L"Invalid FOR init format. Expected: FOR $i = 1 TO ...");
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
  */
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
          LOG_ERROR(L"Malformed FOR statement: TO, DO or ENDFOR keyword missing.");
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
          LOG_ERROR(L"Invalid FOR init format. Expected: FOR $i = 1 TO ...");
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

              //this->execute(instr);
              this->executeInternal(instr);

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
          vData latestVal = resolveVariable(varName);
          double latest = vDataToDouble(latestVal);

          // 2. Incrementăm valoarea proaspăt citită
          latest += step;

          // 3. Salvăm rezultatul în motor
          setVariable(varName, vData(latest));

          // --- SAFETY CHECK ---
          if (m_maxIterations > 0) {
              if (++safetyBreak > m_maxIterations) {
                  LOG_ERROR(L"Safety limit reached in FOR loop...");
                  break;
              }
          }
      }
  }

  /*
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
  */

  void vOliEngine::handleRepeatCommand(const std::wstring& fullLine) {
      std::wstring upperLine = fullLine;
      std::transform(upperLine.begin(), upperLine.end(), upperLine.begin(), ::towupper);

      size_t repeatPos = upperLine.find(L"REPEAT");

      // 1. Găsim UNTIL și ENDREPEAT folosind varianta UPPER
      size_t posUntil = findTopLevelKeyword(upperLine, L"UNTIL", L"REPEAT");
      size_t posEnd = upperLine.rfind(L"ENDREPEAT");

      if (posUntil == std::wstring::npos || posEnd == std::wstring::npos) {
          LOG_ERROR(L"Malformed REPEAT: Missing UNTIL or ENDREPEAT keywords.");
          return;
      }

      // --- CHEIA ESTE AICI ---
      // 2. Extragem corpul: tot ce e între REPEAT (+6) și UNTIL
      size_t bodyStart = repeatPos + 6;
      std::wstring bodyCommand = fullLine.substr(bodyStart, posUntil - bodyStart);

      // 3. Extragem condiția: tot ce e între UNTIL (+5) și ENDREPEAT
      size_t condStart = posUntil + 5;
      std::wstring conditionPart = trim(fullLine.substr(condStart, posEnd - condStart));
      // -----------------------

      std::vector<std::wstring> instructions = preParse(bodyCommand);
      if (instructions.empty()) return;

      int safetyBreak = 0;
      while (true) {
          if (m_maxIterations > 0) {
              if (++safetyBreak > m_maxIterations) {
                  LOG_ERROR(L"Safety limit reached in REPEAT loop...");
                  break;
              }
          }

          for (const auto& instr : instructions) {
              this->executeInternal(instr);

              if (m_executionStatus != OliStatus::RUNNING) {
                  if (m_executionStatus == OliStatus::CONTINUE_REQUESTED) {
                      m_executionStatus = OliStatus::RUNNING;
                      goto evaluate_repeat_condition;
                  }
                  if (m_executionStatus == OliStatus::BREAK_REQUESTED) {
                      m_executionStatus = OliStatus::RUNNING;
                      return;
                  }
                  if (m_executionStatus == OliStatus::RETURN_REQUESTED) return;
              }
          }

      evaluate_repeat_condition:
          vData condRes = evaluateExpression(conditionPart);
          // REPEAT rulează cât timp condiția este FALSE. Se oprește la TRUE.
          if (vDataToBool(condRes)) {
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
          LOG_ERROR(L"SWITCH error: ENDSWITCH keyword is missing.");
          return;
      }

      // 1. Extragem valoarea de control (intre SWITCH si primul CASE/DEFAULT)
      // Căutăm unde începe primul CASE sau DEFAULT
      size_t firstCase = findTopLevelSwitchKeyword(fullLine, L"CASE");
      size_t firstDefault = findTopLevelSwitchKeyword(fullLine, L"DEFAULT");
      size_t posStartBody = (firstCase < firstDefault) ? firstCase : firstDefault;

      if (posStartBody == std::wstring::npos) posStartBody = posEnd;

      size_t spaceAfterSwitch = fullLine.find(L' ', 0);
      if (spaceAfterSwitch == std::wstring::npos) return; // Switch fără expresie?

      std::wstring controlExpr = trim(fullLine.substr(spaceAfterSwitch, posStartBody - spaceAfterSwitch));

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
      std::wstring upperKey = toUpper(keyword);
      size_t kwLen = upperKey.length();

      for (size_t i = 0; i < line.length(); ++i) {
          // 1. Gestionare ghilimele
          if (line[i] == L'"' && (i == 0 || line[i - 1] != L'\\')) {
              inQuotes = !inQuotes;
              continue;
          }
          if (inQuotes) continue;

          // 2. Verificăm dacă suntem la un cuvânt cheie (normalizat)
          // Extragem o porțiune sigură pentru a nu ieși din string
          std::wstring currentChunk = toUpper(line.substr(i, (std::min)(kwLen, line.length() - i)));

          if (currentChunk == upperKey) {
              bool startOk = (i == 0 || iswspace(line[i - 1]) || wcschr(L";()[]{}\"", line[i - 1]));
              bool endOk = (i + kwLen >= line.length() || iswspace(line[i + kwLen]) || wcschr(L";()[]{}\"", line[i + kwLen]));

              if (startOk && endOk) {
                  // Dacă căutăm ENDSWITCH, el este valid la depth 1 (ne va scoate la 0)
                  if (upperKey == L"ENDSWITCH" && depth == 1) return i;
                  // CASE și DEFAULT sunt valide doar direct în interiorul switch-ului curent
                  if ((upperKey == L"CASE" || upperKey == L"DEFAULT") && depth == 1) return i;
                  // Pentru alte căutări la nivelul de bază
                  if (depth == 0 && upperKey != L"ENDSWITCH") return i;
              }
          }

          // 3. --- FIX-UL CRITIC: Gestionare adâncime CASE-INSENSITIVE ---
          if (i + 6 <= line.length()) {
              std::wstring checkSwitch = toUpper(line.substr(i, 6));
              if (checkSwitch == L"SWITCH") {
                  depth++;
                  i += 5; // Sărim peste restul cuvântului
                  continue;
              }
          }

          if (i + 9 <= line.length()) {
              std::wstring checkEnd = toUpper(line.substr(i, 9));
              if (checkEnd == L"ENDSWITCH") {
                  depth--;
                  i += 8;
                  continue;
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
        auto& locals = m_callStack.back().localVariables;
        if (locals.count(cleanName)) {
            locals[cleanName] = value;
            return;
        }
    }

    // B. IMPORTANT: Verificăm dacă variabila există deja în Globale
    // Dacă utilizatorul a definit-o deja global, o actualizăm acolo!
    if (m_globalVariables.count(cleanName)) {
        m_globalVariables[cleanName] = value;
        return;
    }

    // C. Dacă nu a fost găsită nicăieri, decidem unde o creăm (nouă)
    if (!m_callStack.empty()) {
        m_callStack.back().localVariables[cleanName] = value;
    }
    else {
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
		  if (it->localVariables.empty()) {
            LOG_INFO(L"    (no local variables)");
        } else {
            for (const auto& [varName, varData] : it->localVariables) {
                // Folosim vDataToWString sau o metodă de debug pentru a vedea valoarea
                std::wstring valStr = vDataToWString(varData); 
                
                // Limităm lungimea string-ului dacă e prea mare (ex: codul sursă al unei funcții)
                if (valStr.length() > 50) valStr = valStr.substr(0, 47) + L"...";

                LOG_INFO(L"    " + varName + L" = " + valStr);
            }
        }

         
      }

      LOG_INFO(L"----------------------------------");
      LOG_INFO(L"");
  }
  /*
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
  */


  void vOliEngine::handleDefCommand(const ShellCommand& sc) {
      if (sc.args.size() < 3) {
          LOG_ERROR(L"[SYNTAX ERROR] Usage: def class Name { field, method() }");
          return;
      }

      std::wstring subType = to_lower(sc.args[0]);
      std::wstring typeName = to_upper(sc.args[1]); // Normalizăm numele tipului

      // Extragem conținutul dintre acolade (curățat de spații)
      std::wstring fullLine;
      for (size_t i = 2; i < sc.args.size(); ++i) fullLine += sc.args[i];

      size_t start = fullLine.find(L'{');
      size_t end = fullLine.find(L'}');
      if (start == std::wstring::npos || end == std::wstring::npos) return;

      std::wstring content = fullLine.substr(start + 1, end - start - 1);
      std::vector<std::wstring> tokens = wexplodeQuoteSafe(content, L',');

      vTypeBlueprint bp;
      bp.name = typeName;
      bp.isClass = (subType == L"class");

      for (auto& t : tokens) {
          std::wstring item = trim(t);
          size_t paren = item.find(L'(');

          if (paren != std::wstring::npos) {
              // Este o METODĂ
              std::wstring methodName = to_upper(trim(item.substr(0, paren)));
              // Mapăm "METODA" -> "CLASA::METODA"
              bp.methods[methodName] = typeName + L"::" + methodName;
          }
          else {
              // Este un CÂMP
              bp.fields.push_back(to_lower(item));
          }
      }

      m_blueprints[typeName] = bp;
      LOG_SUCCESS(L"Interpreter Blueprint '" + typeName + L"' registered with " +
          std::to_wstring(bp.fields.size()) + L" fields and " +
          std::to_wstring(bp.methods.size()) + L" methods.");
  }


  void vOliEngine::updateDataMember(vData& container, const vData& key, const vData& newValue) {
      // 1. CAZUL MAP
      if (container.isMap()) {
          auto& mPtr = std::get<vDataMap>(container.value);

          // Safety: Dacă pointerul e null, îl inițializăm
          if (!mPtr) mPtr = std::make_shared<std::unordered_map<std::wstring, vData>>();

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

  

 

  vData vOliEngine::deepCopy(const vData& source) {
      // 1. GESTIONARE MAP
      if (auto* oldMapPtr = std::get_if<vDataMap>(&source.value)) {
          auto oldMap = *oldMapPtr;
          // Cream un shared_ptr NOU către un std::map NOU
          auto newMap = std::make_shared<std::unordered_map<std::wstring, vData>>();

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

  
  void vOliEngine::handleConfigCommand(const ShellCommand& sc) {
      // 1. DUMP: Listăm setările actuale
      if (sc.args.empty()) {
          LOG_RAW(L"\n--- Oli Engine Configuration ---");
          LOG_RAW(L"MAX_ITERATIONS: " + (m_maxIterations == 0 ? L"INFINITE" : std::to_wstring(m_maxIterations)));

          // Adăugăm ECHO în listă
          LOG_RAW(L"ECHO: " + std::wstring(m_echoEnabled ? L"ON" : L"OFF"));
          LOG_RAW(L"PLUGINS_PATH: " + m_pluginsPath);

          LOG_RAW(L"--------------------------------\n");
          LOG_RAW(L"Tip: Folosește 'CONFIG <PARAM> <VALOARE>' pentru a schimba.");
          return;
      }

      std::wstring target = to_upper(sc.args[0]);

      // 2. HELP: Căutăm documentația (docs/config/echo.md)
      if (sc.args.size() == 1) {
          std::wstring pathStr = L"docs/config/" + to_lower(target) + L".md";
          std::ifstream file{ std::filesystem::path(pathStr), std::ios::binary };

          if (file.is_open()) {
              std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
              std::wstring wContent = PortTools::utf8_to_wstring(content);

              LOG_RAW(L"\n--- Config Info: " + target + L" ---");
              std::wstringstream wss(wContent);
              std::wstring line;
              while (std::getline(wss, line)) {
                  if (!line.empty() && line[0] == L'#')
                      ConsoleManager::getInstance().writeRaw(line + L"\n", FOREGROUND_GREEN | FOREGROUND_INTENSITY);
                  else
                      LOG_RAW(line);
              }
              LOG_RAW(L"-----------------------------\n");
          }
          else {
              LOG_ERROR(L"No configuration documentation for: " + target);
          }
          return;
      }

      // 3. UPDATE: Încercăm să schimbăm valoarea
      std::wstring valueStr = sc.args[1];

      if (target == L"MAX_ITERATIONS") {
          try {
              m_maxIterations = std::stoll(valueStr);
              LOG_SUCCESS(L"MAX_ITERATIONS updated to: " + (m_maxIterations == 0 ? L"INFINITE" : std::to_wstring(m_maxIterations)));
          }
          catch (...) {
              LOG_ERROR(L"Invalid numeric value: " + valueStr);
          }
      }
      else if (target == L"ECHO") {
          std::wstring valLower = to_lower(valueStr);

          if (valLower == L"true" || valLower == L"on" || valLower == L"1") {
              m_echoEnabled = true;
          }
          else if (valLower == L"false" || valLower == L"off" || valLower == L"0") {
              m_echoEnabled = false;
          }
          else {
              LOG_ERROR(L"Invalid boolean value: " + valueStr + L". Use ON/OFF, TRUE/FALSE or 1/0.");
              return;
          }

          LOG_SUCCESS(L"ECHO updated to: " + std::wstring(m_echoEnabled ? L"ON" : L"OFF"));
      }
      else if (target == L"PLUGINS_PATH") {
          // Curățăm ghilimelele dacă utilizatorul a scris CONFIG PLUGINS_PATH "folder/"
          if (valueStr.size() >= 2 && valueStr.front() == L'"' && valueStr.back() == L'"') {
              valueStr = valueStr.substr(1, valueStr.size() - 2);
          }

          m_pluginsPath = valueStr;

          // Validare: Ne asigurăm că path-ul se termină cu separator pentru concatenare sigură
          if (!m_pluginsPath.empty() && m_pluginsPath.back() != L'/' && m_pluginsPath.back() != L'\\') {
              m_pluginsPath += L"/";
          }

          LOG_SUCCESS(L"PLUGINS_PATH updated to: " + m_pluginsPath);
      }
      else {
          LOG_ERROR(L"Unknown configuration parameter: " + target);
      }
      
  }
  
  
 bool vOliEngine::runEmbeddedIfPresent(const std::string& exePath) {
    std::ifstream file(exePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;

    std::streamsize fileSize = file.tellg();
    
    // FIX: static_cast pentru a elimina warning-ul de signed/unsigned comparison
    if (fileSize < static_cast<std::streamsize>(sizeof(uint64_t))) return false;

    // 1. Citim ultimii 8 octeți (footer-ul cu dimensiunea)
    file.seekg(-8, std::ios::end);
    uint64_t bytecodeSize = 0;
    file.read(reinterpret_cast<char*>(&bytecodeSize), sizeof(uint64_t));

    // 2. Verificăm dacă dimensiunea este plauzibilă
    if (bytecodeSize == 0 || bytecodeSize > static_cast<uint64_t>(fileSize) - 1024) {
        return false; 
    }

    // 3. Ne poziționăm la începutul bytecode-ului
    file.seekg(static_cast<std::streamoff>(fileSize) - 8 - static_cast<std::streamoff>(bytecodeSize));

    // 4. Încărcăm și rulăm
    try {
        // Deserializăm chunk-ul din fișier
        OliChunk chunk = vDataSerialize::deserializeChunk(file);
        
        // Creăm o instanță a motorului
        vOliEngine engine;

        // FIX: Transmitem și al doilea argument (framePtr = 0)
        // Deoarece acesta este punctul de intrare (Main), stiva începe de la 0.
        engine.executeBytecode(chunk, 0); 
        
        return true;
    } catch (...) {
        LOG_ERROR(L"Eroare critică la încărcarea bytecode-ului embedded.");
        return false;
    }
}