#include "vmath.hpp"
#include "OliEngine.hpp"
#include "OliExpressionParser.hpp"


#include <fstream>
#include <filesystem>
#include <random>
#include <vector>
#include <string>
#include <thread>


void vOliEngine::execute(const std::wstring& line) {
    // 0. Curățare comentarii (necesară și în timpul înregistrării)
    std::wstring cleanLine = line;
    size_t commentPos = cleanLine.find(L'#');
    if (commentPos != std::wstring::npos) cleanLine = cleanLine.substr(0, commentPos);
    cleanLine = normalizeSpaces(cleanLine);

    if (cleanLine.empty() && m_accumulator.empty()) return;

    // 1. Gestionare continuare linie (\)
    if (!cleanLine.empty() && cleanLine.back() == L'\\') {
        cleanLine.pop_back();
        m_accumulator += (m_accumulator.empty() ? L"" : L" ") + cleanLine;
        return;
    }

    m_accumulator += (m_accumulator.empty() ? L"" : L" ") + cleanLine;
    std::wstring fullInput = normalizeSpaces(m_accumulator);
    m_accumulator.clear();

    if (fullInput.empty()) return;

    // --- LOGICA DE ÎNREGISTRARE (PROCEDURI) ---

    // Verificăm dacă linia curentă este /endproc (trebuie să iasă din mod recording)
    if (fullInput.find(L"/endproc") == 0) { // Verificăm la începutul liniei
        m_isRecording = false;
        vOliKeyWords::registerDynamicCommand(m_activeProcName);
        LOG_SUCCESS(L"Procedure '" + m_activeProcName + L"' was saved.");
        addToHistory(fullInput);
        return;
    }

    if (fullInput.find(L"/endfunc") == 0) {
        m_isRecordingFunc = false;
        LOG_SUCCESS(L"Function '" + m_activeFuncName + L"' saved.");
        return;
    }
    // Dacă înregistrăm o funcție, salvăm liniile în body-ul ei
    if (m_isRecordingFunc) {
        m_userFunctions[m_activeFuncName].body.push_back(fullInput);
        return;
    }

    if (m_isRecording) {
        // Dacă înregistrăm, salvăm linia așa cum este în corpul procedurii active
        m_procedures[m_activeProcName].body.push_back(fullInput);
        // Putem afișa un indicator vizual în consolă dacă ai acces la std::wcout aici
        // std::wcout << L"  -> " << fullInput << std::endl; 
        return;
    }

    // --- LOGICA NORMALĂ (EXECUȚIE) ---

    addToHistory(fullInput);
    /*
    if (fullInput[0] != L'/') {
        //executeCommand(L"/sys \"" + fullInput + L"\"");
        LOG_RAW(vDataToWString(evaluateExpression(fullInput)));
        return;
    }
    */
    if (fullInput[0] != L'/') {
        //if(runAsShell) return  executeCommand(L"/sys \"" + fullInput + L"\"");
        
        // [MODIFICARE]: Verificăm dacă este o tentativă de atribuire implicită: $a = ... sau a = ...
        size_t eqPos = fullInput.find(L'=');

        // Verificăm să avem un '=' care nu e la început și nu e un operator de comparație '=='
        if (eqPos != std::wstring::npos && eqPos > 0) {
            std::wstring leftSide = normalizeSpaces(fullInput.substr(0, eqPos));
            std::wstring rightSide = normalizeSpaces(fullInput.substr(eqPos + 1));

            // Verificăm dacă partea stângă arată ca o variabilă validă (ex: începe cu $ sau literă)
            if (!leftSide.empty() && (leftSide[0] == L'$' || iswalpha(leftSide[0]))) {
                // Dacă începe cu $, îl scoatem pentru a refolosi comanda /set intern
                if (leftSide[0] == L'$') leftSide.erase(0, 1);

                // Re-rutăm către comanda /set existentă
                executeCommand(L"/set " + leftSide + L" = " + rightSide);
                return;
            }
        }

        // Dacă nu a fost o atribuire, evaluăm ca o expresie normală (echo implicit)
        LOG_RAW(vDataToWString(evaluateExpression(fullInput)));
        return;
    }


    // 3. SEGMENTARE PRIN PREPARSER
    std::vector<std::wstring> commands = preParse(fullInput);

    // 4. Executăm fiecare piesă
    for (const auto& cmd : commands) {
        executeCommand(cmd);
    }
}

  void vOliEngine::addToHistory(const std::wstring& command) {
        m_history.push_back(command);
        // Salvare opțională în fișier
        std::wofstream historyFile("history.txt", std::ios::app);
        if (historyFile.is_open()) {
            historyFile << command << std::endl;
        }
    }

    /*
    void vOliEngine::executeCommand(const std::wstring& command) {
        try {
            ShellCommand sc = vOliCommandParser::parse(command);

            // Căutăm handler-ul în mapă
            auto it = m_commandHandlers.find(sc.name);
            if (it != m_commandHandlers.end()) {
                it->second(sc); // Executăm lambda-ul corespunzător
            }
            else {
                LOG_ERROR(L"Unknown command: " + sc.name);
            }
        }
        catch (const std::bad_variant_access& e) {
            LOG_ERROR(L"Internal Engine Error: Invalid data type access.");
        }
        catch (const std::exception& e) {
            LOG_ERROR(L"System Error: " + std::wstring(e.what(), e.what() + strlen(e.what())));
        }
        catch (...) {
            LOG_ERROR(L"An unknown critical error occurred.");
        }
    }
    */
    /*
    void vOliEngine::executeCommand(const std::wstring& command) {
        if (command.empty()) return;

        // Extragem doar primul cuvânt (numele comenzii)
        std::wstring cmdName;
        size_t firstSpace = command.find(L' ');
        if (firstSpace != std::wstring::npos) {
            cmdName = command.substr(0, firstSpace);
        }
        else {
            cmdName = command;
        }

        auto it = m_commandHandlers.find(cmdName);
        if (it != m_commandHandlers.end()) {
            // Trimitem string-ul BRUT către handler. 
            // Fiecare handler va decide CUM vrea să-și parseze restul liniei.
            it->second(command);
        }
        else {
            LOG_ERROR(L"Unknown command: " + cmdName);
        }
    }
    */
  /*
    void vOliEngine::executeCommand(const std::wstring& fullCommand) {
        if (fullCommand.empty()) return;

        // 1. Extragem DOAR numele comenzii (primul cuvânt până la spațiu)
        std::wstring cmdName;
        size_t firstSpace = fullCommand.find_first_of(L" \t\n\r");
        if (firstSpace != std::wstring::npos) {
            cmdName = fullCommand.substr(0, firstSpace);
        }
        else {
            cmdName = fullCommand;
        }

        // Facem numele Case-Insensitive pentru căutare
        std::wstring cmdUpper = cmdName;
        std::transform(cmdUpper.begin(), cmdUpper.end(), cmdUpper.begin(), ::towupper);

        // 2. Căutăm handler-ul
        auto it = m_commandHandlers.find(cmdUpper);
        if (it != m_commandHandlers.end()) {
            // --- SCHIMBARE AICI ---
            // Nu mai trimitem un ShellCommand gata parsat!
            // Trimitem string-ul BRUT (fullCommand) sau restul lui.
            it->second(fullCommand);
        }
        else {
            LOG_ERROR(L"Unknown command: " + cmdName);
        }
    }
    */
  void vOliEngine::executeCommand(const std::wstring& fullCommand) {
      if (fullCommand.empty()) return;

      // 1. Extragem numele comenzii
      std::wstring cmdName;
      size_t firstSpace = fullCommand.find_first_of(L" \t\n\r");
      if (firstSpace != std::wstring::npos) {
          cmdName = fullCommand.substr(0, firstSpace);
      }
      else {
          cmdName = fullCommand;
      }

      // Facem numele Case-Insensitive pentru căutarea în handlerele de sistem
      std::wstring cmdUpper = cmdName;
      std::transform(cmdUpper.begin(), cmdUpper.end(), cmdUpper.begin(), ::towupper);

      // 2. Căutăm în comenzile sistem (/SET, /ECHO, etc.)
      auto it = m_commandHandlers.find(cmdUpper);
      if (it != m_commandHandlers.end()) {
          it->second(fullCommand);
      }
      // 3. Dacă nu e comandă de sistem, căutăm în procedurile utilizatorului
      else if (m_procedures.count(cmdName)) {
          // Parsăm restul liniei pentru a extrage argumentele transmise
          ShellCommand sc = vOliCommandParser::parse(fullCommand);
          callProcedure(m_procedures[cmdName], sc.args);
      }
      else {
          LOG_ERROR(L"Unknown command or procedure: " + cmdName);
      }
  }

    /*
    void vOliEngine::initializeCommandsHandlers() {
        // Înregistrare comenzi de sistem
        m_commandHandlers[L"/quit"] = [this](const auto& sc) { handleQuitCommand(sc); };
        m_commandHandlers[L"/exit"] = m_commandHandlers[L"/quit"];
        m_commandHandlers[L"/q"] = m_commandHandlers[L"/quit"];

        // Gestiune variabile
        m_commandHandlers[L"/set"] = [this](const auto& sc) { handleSetCommand(sc); };
        m_commandHandlers[L"/s"] = m_commandHandlers[L"/set"];

        // Afișare (cu suport pentru expresii complexe)
        m_commandHandlers[L"/echo"] = [this](const auto& sc) { handleEchoCommand(sc); };
        m_commandHandlers[L"/echo_dbg"] = [this](const auto& sc) { handleEchoCommand(sc); };
        m_commandHandlers[L"/ed"] = m_commandHandlers[L"/echo_dbg"]; // Alias scurt
        m_commandHandlers[L"/e"] = m_commandHandlers[L"/echo"];

        // Afisare variabile setate
        m_commandHandlers[L"/dump_mem"] = [this](const auto& sc) { handleDumpMemCommand(sc); };
        m_commandHandlers[L"/dm"] = m_commandHandlers[L"/dump_mem"];
        m_commandHandlers[L"/vars"] = m_commandHandlers[L"/dump_mem"]; // O altă variantă intuitivă

        m_commandHandlers[L"/unset"] = [this](const auto& sc) { handleUnsetCommand(sc); };
        m_commandHandlers[L"/u"] = m_commandHandlers[L"/unset"];

        m_commandHandlers[L"/if"] = [this](const auto& sc) { handleIfCommand(sc); };
        

    }
    */

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
        m_commandHandlers[L"/IF"] = [this](const std::wstring& rawLine) {
            handleIfCommand(rawLine);
        };

        m_commandHandlers[L"/CYCLE"] = [this](const std::wstring& rawLine) {
            handleCycleCommand(rawLine);
        };

        m_commandHandlers[L"/WHILE"] = [this](const std::wstring& rawLine) {
            handleWhileCommand(rawLine);
        };
      
        m_commandHandlers[L"/FOR"] = [this](const std::wstring& rawLine) {
            handleForCommand(rawLine);
        };
        

        // --- COMENZI DE SISTEM ---
        m_commandHandlers[L"/QUIT"] = wrap([this](const auto& sc) { handleQuitCommand(sc); });
        m_commandHandlers[L"/EXIT"] = m_commandHandlers[L"/QUIT"];
        m_commandHandlers[L"/Q"] = m_commandHandlers[L"/QUIT"];

        // --- GESTIUNE VARIABILE ---
        m_commandHandlers[L"/SET"] = wrap([this](const auto& sc) { handleSetCommand(sc); });
        m_commandHandlers[L"/S"] = m_commandHandlers[L"/SET"];

        m_commandHandlers[L"/UNSET"] = wrap([this](const auto& sc) { handleUnsetCommand(sc); });
        m_commandHandlers[L"/U"] = m_commandHandlers[L"/UNSET"];

        // --- AFIȘARE ---
        m_commandHandlers[L"/ECHO"] = wrap([this](const auto& sc) { handleEchoCommand(sc); });
        m_commandHandlers[L"/E"] = m_commandHandlers[L"/ECHO"];
        m_commandHandlers[L"/ECHO_DBG"] = wrap([this](const auto& sc) { handleEchoCommand(sc); });
        m_commandHandlers[L"/ED"] = m_commandHandlers[L"/ECHO_DBG"];

        // --- MEMORIE / DEBUG ---
        m_commandHandlers[L"/DUMP_MEM"] = wrap([this](const auto& sc) { handleDumpMemCommand(sc); });
        m_commandHandlers[L"/DM"] = m_commandHandlers[L"/DUMP_MEM"];
        m_commandHandlers[L"/VARS"] = m_commandHandlers[L"/DUMP_MEM"];

        // INFO și HELP (din OliKeyWords)
        m_commandHandlers[L"/INFO"] = wrap([this](const auto& sc) { /* handleInfoCommand(sc); */ });
        m_commandHandlers[L"/D"] = m_commandHandlers[L"/INFO"];
        m_commandHandlers[L"/HELP"] = wrap([this](const auto& sc) { /* handleHelpCommand(sc); */ });
        m_commandHandlers[L"/H"] = m_commandHandlers[L"/HELP"];

        m_commandHandlers[L"/RUN"] = wrap([this](const auto& sc) { handleRunCommand(sc); });
        m_commandHandlers[L"/R"] = m_commandHandlers[L"/RUN"];

        m_commandHandlers[L"/SYS"] = wrap([this](const auto& sc) { handleSysCommand(sc); });
        
        m_commandHandlers[L"/PROC"] = wrap([this](const auto& sc) { handleProcCommand(sc); });

        m_commandHandlers[L"/FUNC"] = wrap([this](const auto& sc) { handleFuncCommand(sc); });

        m_commandHandlers[L"/PLUGIN"] = wrap([this](const auto& sc) { handlePluginCommand(sc); });

        m_commandHandlers[L"/LIST_PROCS"] = wrap([this](const auto& sc) { handleListProcsCommand(sc); });
        m_commandHandlers[L"/LP"] = m_commandHandlers[L"/LIST_PROCS"];
        m_commandHandlers[L"/PROC_DUMP"] = m_commandHandlers[L"/LIST_PROCS"];

        m_commandHandlers[L"/BREAK"] = wrap([this](const auto& sc) { handleBreakCommand(sc); });
        m_commandHandlers[L"/CONTINUE"] = wrap([this](const auto& sc) { handleContinueCommand(sc); });

        m_commandHandlers[L"/RETURN"] = wrap([this](const auto& sc) { handleReturnCommand(sc); });
        m_commandHandlers[L"/RET"] = m_commandHandlers[L"/RETURN"];
        
    }

    void vOliEngine::handleQuitCommand(const ShellCommand& sc) {
        LOG_INFO(L"Oli is shutting down...");
        stop();
    }
    
    /*
    void vOliEngine::handleEchoCommand(const ShellCommand& sc) {
        if (sc.args.empty()) return;

        // 1. Recompunem linia
        std::wstring fullLine;
        for (size_t i = 0; i < sc.args.size(); ++i) {
            fullLine += sc.args[i] + (i < sc.args.size() - 1 ? L" " : L"");
        }

        // 2. Verificăm dacă este un string literal pur (ex: "Salut $user")
        if (fullLine.size() >= 2 && fullLine.front() == L'"' && fullLine.back() == L'"') {
            // Dacă e string, DOAR substituim și afișăm (fără evaluateExpression)
            std::wstring output = substituteVariables(fullLine, m_variables);

            // Curățăm ghilimelele pentru afișarea finală
            if (output.size() >= 2 && output.front() == L'"' && output.back() == L'"') {
                output = output.substr(1, output.size() - 2);
            }
            std::wcout << output << std::endl;
        }
        else {
            // Dacă NU are ghilimele (ex: /echo $x + 5), atunci evaluăm
            std::wstring substituted = substituteVariables(fullLine, m_variables);
            vData result = evaluateExpression(substituted);
            std::wcout << vDataToWString(result) << std::endl;
        }
    }
    */

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
    //void vOliEngine::handleSetCommand(const std::wstring& fullLine) {
      //  ShellCommand sc = vOliCommandParser::parse(fullLine);
        if (sc.args.empty()) return;

        // 1. RECOMPUNERE CORECTĂ (Păstrăm spațiile pentru Tokenizer!)
        std::wstring fullExpr;
        for (size_t i = 0; i < sc.args.size(); ++i) {
            fullExpr += sc.args[i];
            if (i < sc.args.size() - 1) fullExpr += L" ";
        }

        size_t eqPos = fullExpr.find(L'=');
        if (eqPos == std::wstring::npos) return;

        std::wstring leftSide = normalizeSpaces(fullExpr.substr(0, eqPos));
        std::wstring rightSide = normalizeSpaces(fullExpr.substr(eqPos + 1));

        // 2. EVALUARE (Acum evaluateExpression va primi un string curat)
        vData finalValue = evaluateExpression(rightSide);

        if (!leftSide.empty() && leftSide[0] == L'$') leftSide = leftSide.substr(1);

        size_t bracketPos = leftSide.find(L'[');
        if (bracketPos != std::wstring::npos) {
            assignToVariable(leftSide, finalValue);
            //LOG_SUCCESS(L"Element in $" + leftSide + L" updated.");
        }
        else {
            m_variables[leftSide] = finalValue;
            //LOG_SUCCESS(L"Variable $" + leftSide + L" set to " + getVariantTypeName(finalValue));
        }
    }

    std::wstring vOliEngine::getVariantTypeName(const vData& data) {
        if (std::holds_alternative<long long>(data.value)) return L"(INT)";
        if (std::holds_alternative<double>(data.value)) return L"(FLOAT)";
        if (std::holds_alternative<std::wstring>(data.value)) return L"(STRING)";
        if (std::holds_alternative<bool>(data.value)) return L"(BOOL)";
        if (std::holds_alternative<vDataArray>(data.value)) return L"(ARRAY)";
        if (std::holds_alternative<vDataMap>(data.value)) return L"(MAP)";
        if (std::holds_alternative<std::monostate>(data.value)) return L"(NULL)";
        return L"(UNKNOWN)";
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
        if (rawVar.empty()) return { std::monostate{} };

        // 1. LOGICA PENTRU $$ (Variabile Variabile)
        // Trebuie să fie PRIMA verificare.
        if (rawVar[0] == L'$') {
            vData intermediate = resolveVariable(rawVar.substr(1));
            std::wstring nextVarName = vDataToWString(intermediate);
            // Re-apelăm resolveVariable cu numele extras (ex: "viata")
            return resolveVariable(nextVarName);
        }

        // 2. Extragere nume variabilă normală
        size_t bracketStart = rawVar.find(L'[');
        std::wstring varName = (bracketStart == std::wstring::npos) ? rawVar : rawVar.substr(0, bracketStart);
        varName = normalizeSpaces(varName);

        // Verificăm dacă variabila rădăcină există
        if (m_variables.find(varName) == m_variables.end()) return { std::monostate{} };

        vData* current = &m_variables[varName];
        size_t currentPos = bracketStart;

        // 3. Navigare prin indici [][][]
        while (currentPos != std::wstring::npos) {
            size_t bracketEnd = findClosingBracket(rawVar, currentPos);
            if (bracketEnd == std::wstring::npos) break;

            std::wstring indexExpr = normalizeSpaces(rawVar.substr(currentPos + 1, bracketEnd - currentPos - 1));
            vData indexValue = evaluateExpression(indexExpr);

            if (auto* pArray = std::get_if<vDataArray>(&current->value)) {
                if (auto* pIdx = std::get_if<long long>(&indexValue.value)) {
                    size_t idx = (size_t)*pIdx;
                    if (idx < pArray->size()) current = &((*pArray)[idx]);
                    else return { std::monostate{} };
                }
                else return { std::monostate{} };
            }
            else if (auto* pMap = std::get_if<vDataMap>(&current->value)) {
                std::wstring key = vDataToWString(indexValue);
                if (pMap->count(key)) current = &((*pMap)[key]);
                else return { std::monostate{} };
            }
            else return { std::monostate{} };

            currentPos = rawVar.find(L'[', bracketEnd + 1);
        }
        return *current;
    }

    void vOliEngine::printVData(const vData& data, bool debugMode) {
        if (data.IsNull()) {
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
        if (m_variables.empty()) {
            LOG_INFO(L"Memory is empty. No variables set.");
            return;
        }

        std::wcout << L"\n--- [Oli Memory Dump] ---" << std::endl;
        // Cap de tabel aliniat (Nume: 15 caractere, Tip: 10 caractere)
        std::wcout << std::left << std::setw(15) << L"NAME"
            << std::setw(12) << L"TYPE"
            << L"VALUE" << std::endl;
        std::wcout << std::wstring(40, L'-') << std::endl;

        for (const auto& [name, data] : m_variables) {
            std::wcout << std::left << std::setw(15) << name
                << std::setw(12) << getVariantTypeName(data);

            // Folosim printVData-ul pe care l-am făcut anterior
            printVData(data, true);

            std::wcout << std::endl;
        }
        std::wcout << std::wstring(40, L'-') << std::endl;
        std::wcout << L"Total variables: " << m_variables.size() << L"\n" << std::endl;
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
        return std::visit([](auto&& arg) -> std::wstring {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, long long>) return std::to_wstring(arg);
            else if constexpr (std::is_same_v<T, double>) {
                // Formatare frumoasă pentru float
                wchar_t buf[64];
                swprintf(buf, 64, L"%.4f", arg);
                return buf;
            }
            else if constexpr (std::is_same_v<T, std::wstring>) return arg;
            else if constexpr (std::is_same_v<T, bool>) return arg ? L"true" : L"false";
            else if constexpr (std::is_same_v<T, vDataArray>) {
                std::wstring res = L"[";
                // Aici e recursivitatea pentru array-uri
                // Ai grijă să nu apelezi std::get aici, folosește tot visit sau recursivitate
                return L"(ARRAY)";
            }
            else return L"null";
            }, data.value);
    }
    /*
    void vOliEngine::assignToVariable(const std::wstring& varExpr, const vData& newValue) {
        size_t bracketStart = varExpr.find(L'[');

        // --- CAZ 1: Variabilă simplă (/set x = 10) ---
        if (bracketStart == std::wstring::npos) {
            m_variables[normalizeSpaces(varExpr)] = newValue;
            return;
        }

        // --- CAZ 2: Structură complexă (Array/Map) ---
        std::wstring varName = normalizeSpaces(varExpr.substr(0, bracketStart));

        // 1. Inițializăm rădăcina dacă nu există (Auto-vivification)
        if (m_variables.find(varName) == m_variables.end() || m_variables[varName].IsNull()) {
            size_t bracketEnd = findClosingBracket(varExpr, bracketStart);
            std::wstring firstContent = varExpr.substr(bracketStart + 1, bracketEnd - bracketStart - 1);

            // Dacă primul index e gol [] sau numeric, pornim cu un ARRAY, altfel cu un MAP
            if (firstContent.empty()) {
                m_variables[varName] = { vDataArray{} };
            }
            else {
                vData firstIdxVal = evaluateExpression(firstContent);
                if (firstIdxVal.isInt() || firstIdxVal.isFloat())
                    m_variables[varName] = { vDataArray{} };
                else
                    m_variables[varName] = { vDataMap{} };
            }
        }

        // 2. Navigăm până la ultimul container (ex: în a[0][1], targetContainer va fi a[0])
        vData* targetContainer = navigateOrCreatePath(&m_variables[varName], varExpr);

        // 3. Extragem ultimul index pentru asignarea finală
        size_t lastOpen = varExpr.find_last_of(L'[');
        size_t lastClose = findClosingBracket(varExpr, lastOpen);
        std::wstring lastIndexExpr = varExpr.substr(lastOpen + 1, lastClose - lastOpen - 1);

        // --- LOGICA DE ASIGNARE ȘI CONVERSIE ---

        if (lastIndexExpr.empty()) {
            // Cazul: /set variabila[] = ... (APPEND)

            // Dacă containerul țintă e Null, îl promovăm la Array
            if (targetContainer->IsNull()) {
                targetContainer->value = vDataArray{};
            }

            if (std::holds_alternative<vDataArray>(targetContainer->value)) {
                assignToArrayVar(targetContainer, L"", newValue);
            }
            else {
                // Eroare: nu poți face append [] pe un Map sau String
                std::wcout << L"[ERROR] Nu se poate folosi [] pe un tip care nu este Array." << std::endl;
            }
        }
        else {
            // Cazul: /set variabila[index] = ... (UPDATE/INSERT)

            // Dacă e deja Array, folosim logica de array (index numeric)
            if (std::holds_alternative<vDataArray>(targetContainer->value)) {
                assignToArrayVar(targetContainer, lastIndexExpr, newValue);
            }
            // Altfel, tratăm ca Map (index string)
            else {
                assignToMapVar(targetContainer, lastIndexExpr, newValue);
            }
        }
    }
    */

    void vOliEngine::assignToVariable(const std::wstring& varExpr, const vData& newValue) {
        size_t bracketStart = varExpr.find(L'[');
        if (bracketStart == std::wstring::npos) {
            m_variables[normalizeSpaces(varExpr)] = newValue;
            return;
        }

        std::wstring varName = normalizeSpaces(varExpr.substr(0, bracketStart));

        // --- PASUL 1: Inițializare Smart Rădăcină ---
        if (m_variables[varName].IsNull()) {
            // Dacă prima paranteză este [], forțăm ARRAY
            if (varExpr.substr(bracketStart, 2) == L"[]") {
                m_variables[varName].value = vDataArray{};
            }
            else {
                // Evaluăm conținutul primei paranteze
                size_t firstEnd = findClosingBracket(varExpr, bracketStart);
                std::wstring content = varExpr.substr(bracketStart + 1, firstEnd - bracketStart - 1);
                vData idx = evaluateExpression(content);
                if (idx.isInt()) m_variables[varName].value = vDataArray{};
                else m_variables[varName].value = vDataMap{};
            }
        }

        // --- PASUL 2: Navigare ---
        // navigateOrCreatePath se oprește la penultimul nivel
        vData* targetContainer = navigateOrCreatePath(&m_variables[varName], varExpr);

        // --- PASUL 3: Asignare Finală ---
        size_t lastOpen = varExpr.find_last_of(L'[');
        size_t lastClose = findClosingBracket(varExpr, lastOpen);
        std::wstring lastIdxExpr = normalizeSpaces(varExpr.substr(lastOpen + 1, lastClose - lastOpen - 1));

        if (lastIdxExpr.empty()) {
            // Cazul APPEND: $var[] = val
            if (!targetContainer->isArray()) targetContainer->value = vDataArray{};
            std::get<vDataArray>(targetContainer->value).push_back(newValue);
        }
        else {
            // Cazul UPDATE: $var[index] = val
            vData idxVal = evaluateExpression(lastIdxExpr);
            if (idxVal.isInt() || idxVal.isFloat()) {
                if (!targetContainer->isArray()) targetContainer->value = vDataArray{};
                assignToArrayVar(targetContainer, lastIdxExpr, newValue);
            }
            else {
                if (!targetContainer->isMap()) targetContainer->value = vDataMap{};
                assignToMapVar(targetContainer, lastIdxExpr, newValue);
            }
        }
    }

    void vOliEngine::assignToArrayVar(vData* container, const std::wstring& indexExpr, const vData& newValue) {
        vDataArray& arr = std::get<vDataArray>(container->value);

        if (indexExpr.empty()) {
            // Cazul /set a[] = val -> Adaugă elementul la sfârșit
            arr.push_back(newValue);
        }
        else {
            // Cazul /set a[$i] = val -> Update la index specific
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

    vData* vOliEngine::navigateOrCreatePath(vData* root, const std::wstring& varExpr) {
        size_t bracketStart = varExpr.find(L'[');
        if (bracketStart == std::wstring::npos) return root;

        vData* current = root;
        size_t currentPos = bracketStart;

        while (currentPos != std::wstring::npos) {
            size_t bracketEnd = findClosingBracket(varExpr, currentPos);
            size_t nextBracket = varExpr.find(L'[', bracketEnd + 1);

            // --- SCHIMBARE AICI ---
            // Dacă NU mai există o altă paranteză după bracketEnd, 
            // înseamnă că suntem la nivelul final. NU mai intra în getOrCreateContainer.
            if (nextBracket == std::wstring::npos) {
                return current;
            }

            std::wstring content = varExpr.substr(currentPos + 1, bracketEnd - currentPos - 1);

            // Detectăm ce urmează să creăm
            std::wstring nextPart = varExpr.substr(nextBracket + 1);
            bool nextIsArray = (nextPart.empty() || nextPart[0] == L']' || isdigit(nextPart[0]) || nextPart[0] == L'$');

            current = getOrCreateContainer(current, content, nextIsArray);
            currentPos = nextBracket;
        }
        return current;
    }
    /*
    vData* vOliEngine::getOrCreateContainer(vData* root, const std::wstring& indexExpr, bool isNextBracketArray) {
        if (indexExpr.empty()) {
            if (!std::holds_alternative<vDataArray>(root->value)) root->value = vDataArray{};
            vDataArray& arr = std::get<vDataArray>(root->value);

            // Dacă avem deja un container de tipul cerut ca ultim element, nu mai creăm unul
            if (!arr.empty()) {
                vData& last = arr.back();
                if (isNextBracketArray && std::holds_alternative<vDataArray>(last.value)) return &last;
                if (!isNextBracketArray && std::holds_alternative<vDataMap>(last.value)) return &last;
            }

            if (isNextBracketArray) arr.push_back({ vDataArray{} });
            else arr.push_back({ vDataMap{} });
            return &arr.back();
        }

        // 2. Gestionare Indexare Normală (rămâne neschimbată, e corectă)
        vData indexValue = evaluateExpression(normalizeSpaces(indexExpr));

        if (auto* pIdx = std::get_if<long long>(&indexValue.value)) {
            if (!std::holds_alternative<vDataArray>(root->value)) root->value = vDataArray();
            vDataArray& arr = std::get<vDataArray>(root->value);
            if (*pIdx >= (long long)arr.size()) arr.resize(*pIdx + 1, { std::monostate{} });

            // Dacă indexăm un element care urmează să fie container dar e gol
            if (std::holds_alternative<std::monostate>(arr[*pIdx].value)) {
                if (isNextBracketArray) arr[*pIdx].value = vDataArray{};
                else arr[*pIdx].value = vDataMap{};
            }
            return &arr[*pIdx];
        }
        else {
            if (!std::holds_alternative<vDataMap>(root->value)) root->value = vDataMap();
            vDataMap& m = std::get<vDataMap>(root->value);
            std::wstring key = vDataToWString(indexValue);

            if (m.find(key) == m.end() || std::holds_alternative<std::monostate>(m[key].value)) {
                if (isNextBracketArray) m[key].value = vDataArray{};
                else m[key].value = vDataMap{};
            }
            return &m[key];
        }
    }
    */
    /*
    vData* vOliEngine::getOrCreateContainer(vData* root, const std::wstring& indexExpr, bool isNextBracketArray) {
        // 1. CAZUL: Index gol [] -> Comportament de APPEND (Vector)
        if (indexExpr.empty()) {
            if (!root->isArray()) root->value = vDataArray{};
            if (!std::holds_alternative<vDataArray>(root->value)) {
                root->value = vDataArray{}; // Forțăm să fie Array
            }
            vDataArray& arr = std::get<vDataArray>(root->value);

            // Dacă avem deja un container valid la final, îl refolosim (pentru stabilitate)
            if (!arr.empty()) {
                vData& last = arr.back();
                if (isNextBracketArray && std::holds_alternative<vDataArray>(last.value)) return &last;
                if (!isNextBracketArray && std::holds_alternative<vDataMap>(last.value)) return &last;
            }

            // Altfel, adăugăm unul nou
            if (isNextBracketArray) arr.push_back({ vDataArray{} });
            else arr.push_back({ vDataMap{} });
            return &arr.back();
        }

        // 2. CAZUL: Index normal [ceva]
        vData indexValue = evaluateExpression(normalizeSpaces(indexExpr));

        // A: Index Numeric -> Lucrăm cu ARRAY
        if (indexValue.isInt() || indexValue.isFloat()) {
            if (!std::holds_alternative<vDataArray>(root->value)) root->value = vDataArray{};
            vDataArray& arr = std::get<vDataArray>(root->value);

            size_t idx = (size_t)vDataToLong(indexValue);
            if (idx >= arr.size()) arr.resize(idx + 1, { std::monostate{} });

            if (std::holds_alternative<std::monostate>(arr[idx].value)) {
                if (isNextBracketArray) arr[idx].value = vDataArray{};
                else arr[idx].value = vDataMap{};
            }
            return &arr[idx];
        }
        // B: Index String -> Lucrăm cu MAP
        else {
            if (!std::holds_alternative<vDataMap>(root->value)) root->value = vDataMap{};
            vDataMap& m = std::get<vDataMap>(root->value);
            std::wstring key = vDataToWString(indexValue);

            if (m.find(key) == m.end() || std::holds_alternative<std::monostate>(m[key].value)) {
                if (isNextBracketArray) m[key].value = vDataArray{};
                else m[key].value = vDataMap{};
            }
            return &m[key];
        }
    }
    */
vData* vOliEngine::getOrCreateContainer(vData* root, const std::wstring& indexExpr, bool isNextBracketArray) {
    std::wstring cleanIdx = normalizeSpaces(indexExpr);

    // 1. CAZUL: Index GOL [] -> Întotdeauna creăm un element NOU la finalul array-ului
    if (cleanIdx.empty()) {
        if (!root->isArray()) root->value = vDataArray{};
        vDataArray& arr = std::get<vDataArray>(root->value);

        // Adăugăm un container nou de tipul cerut de următorul bracket
        if (isNextBracketArray) arr.push_back({ vDataArray{} });
        else arr.push_back({ vDataMap{} });

        return &arr.back();
    }

    // 2. CAZUL: Index EXPLICIT [0] sau ["cheie"]
    vData indexValue = evaluateExpression(cleanIdx);

    // A: Index Numeric -> Lucrăm cu ARRAY
    if (indexValue.isInt() || indexValue.isFloat()) {
        if (!root->isArray()) root->value = vDataArray{};
        vDataArray& arr = std::get<vDataArray>(root->value);

        size_t idx = (size_t)vDataToLong(indexValue);
        if (idx >= arr.size()) {
            arr.resize(idx + 1, { std::monostate{} });
        }

        // Inițializăm elementul de la indexul respectiv dacă e gol
        if (arr[idx].IsNull()) {
            if (isNextBracketArray) arr[idx].value = vDataArray{};
            else arr[idx].value = vDataMap{};
        }
        return &arr[idx];
    }
    // B: Index String -> Lucrăm cu MAP
    else {
        if (!root->isMap()) root->value = vDataMap{};
        vDataMap& m = std::get<vDataMap>(root->value);
        std::wstring key = vDataToWString(indexValue);

        if (m.find(key) == m.end() || m[key].IsNull()) {
            if (isNextBracketArray) m[key].value = vDataArray{};
            else m[key].value = vDataMap{};
        }
        return &m[key];
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
            if (args.empty()) return { L"(NONE)" };

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

        switch (node->type) {
        case ASTNodeType::Literal: {
            if (node->value == L"ARRAY_OBJECT") {
                vDataArray elements;
                for (auto& child : node->children) {
                    elements.push_back(executeAST(child)); // Executăm fiecare element (10, 20, etc.)
                }
                return { elements }; // Returnăm un vData de tip Array
            }

            if (node->value == L"MAP_OBJECT") {
                vDataMap myMap;
                for (size_t i = 0; i < node->children.size(); i += 2) {
                    vData keyData = executeAST(node->children[i]);
                    vData valData = executeAST(node->children[i + 1]);
                    myMap[vDataToWString(keyData)] = valData;
                }
                return { myMap };
            }

            std::wstring val = node->value;

            if (val == L"moonstate") {
                return { std::monostate{} };
            }

            // 1. Dacă este string (începe și se termină cu ghilimele)
            if (val.size() >= 2 && val.front() == L'"' && val.back() == L'"') {
                return { val.substr(1, val.size() - 2) }; // Returnăm conținutul direct
            }

            // 2. Dacă este un număr sau boolean, folosim o metodă care DOAR convertește, nu parsează
            return parseRawLiteral(val);
        }

        case ASTNodeType::Variable:
            return resolveVariable(node->value);

        case ASTNodeType::FunctionCall: {

            std::vector<vData> evaluatedArgs;
            for (auto& child : node->children) {
                evaluatedArgs.push_back(executeAST(child));
            }

            // 1. Căutăm întâi în funcțiile interne (C++)
            auto itInternal = m_functionsHandlers.find(node->value);
            if (itInternal != m_functionsHandlers.end()) {
                return itInternal->second(evaluatedArgs);
            }

            // 2. [MODIFICARE]: Căutăm în funcțiile definite de utilizator (/func)
            // Folosim o copie a numelui pentru a asigura case-insensitivity dacă dorești
            std::wstring funcName = node->value;
            auto itUser = m_userFunctions.find(funcName);
            if (itUser != m_userFunctions.end()) {
                // Apelăm metoda care gestionează execuția corpului funcției și return-ul
                return callUserFunction(funcName, evaluatedArgs);
            }

            // 3. Dacă nu e niciuna, returnăm eroare

            LOG_ERROR(L"[RUNTIME ERROR] Function '" + node->value + L"' is not defined.");
            //return { L"Error: Unknown function " + node->value };
            return { std::monostate{} };
        }
/*
        case ASTNodeType::Operator: {
            if (node->value == L"INDEX") {
                vData container = executeAST(node->children[0]);
                vData key = executeAST(node->children[1]);
                return accessContainer(container, key); // O funcție care caută în Map/Array
            }

            if (node->value == L"DEREFERENCE") {
                    // 1. Evaluăm copilul pentru a obține numele variabilei țintă
                    vData targetNameData = executeAST(node->children[0]);
                    std::wstring targetName = vDataToWString(targetNameData);

                    // 2. Curățăm numele (scoatem $ dacă utilizatorul l-a pus din greșeală în string)
                    if (!targetName.empty() && targetName[0] == L'$') {
                        targetName = targetName.substr(1);
                    }

                    // 3. Rezolvăm variabila cu numele rezultat dinamic
                    return resolveVariable(targetName);
            }

            vData left = executeAST(node->children[0]);
            vData right = executeAST(node->children[1]);

            // Aici intervine biblioteca ta vmath (adaptată pentru vData)
            return executeBinaryOperator(node->value, left, right);
        }
*/
        case ASTNodeType::Operator: {
            // 1. Operatori unari (care au un singur copil)
            if (node->value == L"UNARY_MINUS") {
                vData val = executeAST(node->children[0]);
                // Negăm valoarea (asigură-te că vDataToDouble funcționează)
                return { -vDataToDouble(val) };
            }

            if (node->value == L"NOT") {
                vData val = executeAST(node->children[0]);
                // Logic NOT (ex: !true = false)
                return { !vDataToBool(val) };
            }

            // 2. Operatori speciali (INDEX, DEREFERENCE)
            if (node->value == L"INDEX") {
                vData container = executeAST(node->children[0]);
                vData key = executeAST(node->children[1]);
                return accessContainer(container, key);
            }

            if (node->value == L"DEREFERENCE") {
                vData targetNameData = executeAST(node->children[0]);
                std::wstring targetName = vDataToWString(targetNameData);
                if (!targetName.empty() && targetName[0] == L'$') {
                    targetName = targetName.substr(1);
                }
                return resolveVariable(targetName);
            }

            // 3. Operatori binari (au garantat 2 copii)
            // Punem o protecție aici pentru orice eventualitate
            if (node->children.size() < 2) {
                return { L"Error: Operator " + node->value + L" requires two operands" };
            }

            vData left = executeAST(node->children[0]);
            vData right = executeAST(node->children[1]);

            return executeBinaryOperator(node->value, left, right);
        }
        }
        return { std::monostate{} };
    }

    /*
    vData vOliEngine::executeBinaryOperator(const std::wstring& op, const vData& left, const vData& right) {

        std::wcout << L"Op: " << op << L" Left index: " << left.value.index() << std::endl;
        
        if (std::holds_alternative<std::monostate>(left.value) ||
            std::holds_alternative<std::monostate>(right.value)) {
            return { L"Error: Operation '" + op + L"' failed. One or more operands are undefined." };
        }

        // PASUL 2: Tratăm operatorul de egalitate SEPARAT de restul matematicii
        if (op == L"==") {
            // Dacă unul este string, comparăm totul ca string (fără vDataToDouble!)
            if (left.isString() || right.isString()) {
                return { vDataToWString(left) == vDataToWString(right) };
            }
            // Altfel, dacă ambele sunt numere, comparăm double
            return { vDataToDouble(left) == vDataToDouble(right) };
        }

        // 1. Logica pentru Concatenare (daca unul e string si folosim +)
        if (op == L"+" && (left.isString() || right.isString())) {
            return { vDataToWString(left) + vDataToWString(right) };
        }

        // 2. Logica pentru Matematică (Double/Long Long)
        // Convertim vData în double pentru calcul (folosind o metodă helper)
        double valL = vDataToDouble(left);
        double valR = vDataToDouble(right);

        if (op == L"+") {
            if (left.isString() || right.isString()) {
                return { vDataToWString(left) + vDataToWString(right) };
            }
            return { valL + valR };
        }
        if (op == L"-")  return { valL - valR };
        if (op == L"*")  return { valL * valR };
        if (op == L"/") {
            if (valR == 0) return { L"Error: Division by zero" };
            return { valL / valR };
        }
        if (op == L"^" || op == L"**") return { pow(valL, valR) };

        // 3. Logica pentru Operatori Logici și Comparații
        //if (op == L"==") return { valL == valR };

        

        return { std::monostate{} };
    }
    */

    vData vOliEngine::executeBinaryOperator(const std::wstring& op, const vData& left, const vData& right) {

        // 1. Operatorul Null Coalescing (TREBUIE să fie înainte de bariera IsNull)
        if (op == L"??") {
            // Dacă stânga e moonstate, returnăm dreapta. Altfel, returnăm stânga.
            return left.IsNull() ? right : left;
        }

        // --- STRATUL 1: OPERATORI DE EGALITATE (Suportă moonstate) ---
        // Aceștia trebuie să fie primii pentru a permite: if ($x == moonstate)
        if (op == L"==") {
            if (left.IsNull() && right.IsNull()) return { true };
            if (left.IsNull() || right.IsNull()) return { false };

            // Dacă unul este string, comparăm lexical (vDataToWString se ocupă de conversia celuilalt)
            if (left.isString() || right.isString()) {
                return { vDataToWString(left) == vDataToWString(right) };
            }
            // Comparăm numeric
            return { vDataToDouble(left) == vDataToDouble(right) };
        }

        if (op == L"!=") {
            if (left.IsNull() && right.IsNull()) return { false };
            if (left.IsNull() || right.IsNull()) return { true };

            if (left.isString() || right.isString()) {
                return { vDataToWString(left) != vDataToWString(right) };
            }
            return { vDataToDouble(left) != vDataToDouble(right) };
        }

        // --- STRATUL 2: BARIERĂ PENTRU OPERANZI NULI ---
        // După egalitate, nicio altă operație (matematică/logică) nu are sens cu moonstate.
        if (left.IsNull() || right.IsNull()) {
            return { L"Error: Operation '" + op + L"' failed. One or more operands are moonstate (null)." };
        }

        // --- STRATUL 3: OPERATORI LOGICI (&&, ||) ---
        // Folosim vDataToBool pentru a suporta "truthiness" (0 e false, restul true)
        if (op == L"&&") return { vDataToBool(left) && vDataToBool(right) };
        if (op == L"||") return { vDataToBool(left) || vDataToBool(right) };

        // --- STRATUL 4: CONCATENARE SAU ADUNARE ---
        if (op == L"+") {
            if (left.isString() || right.isString()) {
                return { vDataToWString(left) + vDataToWString(right) };
            }
            return { vDataToDouble(left) + vDataToDouble(right) };
        }

        // --- STRATUL 5: MATEMATICĂ PURĂ ȘI COMPARAȚII DE MĂRIME ---
        if (left.isString() || right.isString()) {
            std::wstring sL = vDataToWString(left);
            std::wstring sR = vDataToWString(right);

            if (op == L"<")  return { sL < sR };
            if (op == L">")  return { sL > sR };
            if (op == L"<=") return { sL <= sR };
            if (op == L">=") return { sL >= sR };
        }

        double valL = vDataToDouble(left);
        double valR = vDataToDouble(right);

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

        // Comparări de mărime
        if (op == L"<")  return { valL < valR };
        if (op == L">")  return { valL > valR };
        if (op == L"<=") return { valL <= valR };
        if (op == L">=") return { valL >= valR };

        // Dacă operatorul nu este recunoscut
        return { L"Error: Unknown operator '" + op + L"'" };
    }


    double vOliEngine::vDataToDouble(const vData& data) {
        if (data.isInt())   return (double)std::get<long long>(data.value);
        if (data.isFloat()) return std::get<double>(data.value);
        if (data.isBool())  return std::get<bool>(data.value) ? 1.0 : 0.0;
        if (data.isString()) {
            try { return std::stod(std::get<std::wstring>(data.value)); }
            catch (...) { return 0.0; }
        }
        return 0.0;
    }

    vData vOliEngine::parseRawLiteral(const std::wstring& val) {
        // Verificăm starea specială (Null/Moonstate)
        if (val == L"moonstate" || val == L"null") {
            return { std::monostate{} };
        }

        // Verificăm dacă e boolean
        if (val == L"true")  return { true };
        if (val == L"false") return { false };

        // Verificăm dacă e un număr (Integer sau Float)
        try {
            if (val.find(L'.') != std::wstring::npos) {
                return { std::stod(val) }; // Este Double
            }
            else {
                return { std::stoll(val) }; // Este Long Long (Integer)
            }
        }
        catch (...) {
            // Dacă nu e număr și nici bool, îl tratăm ca pe un string brut (fără ghilimele)
            return { val };
        }
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
        // 1. Cazul "/unset all"
        if (sc.args.size() == 1 && sc.args[0] == L"all") {
            size_t count = m_variables.size();
            m_variables.clear();
            std::wcout << L"[SUCCESS] Memory cleared. " << count << L" variables removed." << std::endl;
            return;
        }

        // 2. Cazul "/unset var1 var2 ..." (Argumente multiple)
        if (sc.args.empty()) {
            std::wcout << L"[ERROR] Usage: /unset <varName> or /unset all" << std::endl;
            return;
        }

        for (const auto& arg : sc.args) {
            std::wstring cleanName = arg;
            if (!cleanName.empty() && cleanName[0] == L'$') cleanName.erase(0, 1);

            if (m_variables.erase(cleanName)) {
                std::wcout << L"[SUCCESS] Variable $" << cleanName << L" removed." << std::endl;
            }
            else {
                std::wcout << L"[WARNING] Variable $" << cleanName << L" not found." << std::endl;
            }
        }
    }
    */

    /*
    void vOliEngine::handleUnsetCommand(const ShellCommand& sc) {
        if (sc.args.empty()) return;

        for (const auto& rawArg : sc.args) {
            if (rawArg == L"all") {
                m_variables.clear();
                std::wcout << L"[SUCCESS] Memory cleared." << std::endl;
                return;
            }

            // Curățăm numele de eventualul prefix $ pentru rootName
            std::wstring cleanArg = rawArg;
            if (!cleanArg.empty() && cleanArg[0] == L'$') cleanArg.erase(0, 1);

            auto path = parsePath(cleanArg);

            if (path.indexes.empty()) {
                if (m_variables.erase(path.rootName)) {
                    std::wcout << L"[SUCCESS] Variable $" << path.rootName << L" removed." << std::endl;
                }
                else {
                    std::wcout << L"[WARNING] Variable $" << path.rootName << L" not found." << std::endl;
                }
            }
            else {
                vData* parent = resolveToParent(path.rootName, path.indexes);

                if (parent) {
                    std::wstring lastKey = path.indexes.back();

                    if (parent->isMap()) {
                        auto& map = std::get<vDataMap>(parent->value);
                        if (map.erase(lastKey)) {
                            std::wcout << L"[SUCCESS] Key '" << lastKey << L"' removed from Map." << std::endl;
                        }
                        else {
                            std::wcout << L"[WARNING] Key '" << lastKey << L"' not found in Map." << std::endl;
                        }
                    }
                    else if (parent->isArray()) {
                        auto& vec = std::get<vDataArray>(parent->value);
                        try {
                            size_t idx = std::stoul(lastKey);
                            if (idx < vec.size()) {
                                vec.erase(vec.begin() + idx);
                                std::wcout << L"[SUCCESS] Index " << idx << L" removed from Array." << std::endl;
                            }
                            else {
                                std::wcout << L"[ERROR] Index " << idx << L" out of bounds." << std::endl;
                            }
                        }
                        catch (...) {
                            std::wcout << L"[ERROR] Invalid array index: " << lastKey << std::endl;
                        }
                    }
                }
                else {
                    std::wcout << L"[ERROR] Could not resolve path: " << cleanArg << std::endl;
                }
            }
        }
    }
    */
    /*
    void vOliEngine::handleUnsetCommand(const ShellCommand& sc) {
        if (sc.args.empty()) return;

        
        print_wstr_vct(sc.args);
        std::wstring raw_var = implode(sc.args, L"");
        vData var = resolveVariable(raw_var);
        if (var.isArray()) {
            LOG_DEBUG(L"Este vector");
        }
        else if (var.isMap()) {
            LOG_DEBUG(L"Este map");
        }
        else {
            LOG_DEBUG(L"Este normala");

        }

      
        
        //LOG_ERROR(L"AICI:" + tmp_str);
        for (const auto& rawArg : sc.args) {
            // Ignorăm reziduurile de tip "[" "]" "0" dacă tokenizer-ul le-a spart greșit
            if (rawArg == L"[" || rawArg == L"]" || rawArg == L"$") continue;

            if (rawArg == L"all") {
                m_variables.clear();
                std::wcout << L"[SUCCESS] Memory cleared." << std::endl;
                return;
            }

            std::wstring cleanArg = rawArg;
            if (!cleanArg.empty() && cleanArg[0] == L'$') cleanArg.erase(0, 1);

            // Folosim un PathParser care știe să separe "nume[0][1]" în {root: "nume", indexes: ["0", "1"]}
            auto path = parsePath(cleanArg);
            LOG_WARNING(path.rootName);
            if (path.indexes.empty()) {
                if (m_variables.erase(path.rootName)) {
                    std::wcout << L"[SUCCESS] Variable $" << path.rootName << L" removed." << std::endl;
                }
                else {
                    std::wcout << L"[WARNING] Variable $" << path.rootName << L" not found." << std::endl;
                }
            }
            else {
                // Căutăm containerul părinte (ex: pentru a[0][1], căutăm a[0])
                vData* parent = resolveToParent(path.rootName, path.indexes);

                if (parent) {
                    std::wstring lastKey = path.indexes.back();

                    if (parent->isMap()) {
                        auto& map = std::get<vDataMap>(parent->value);
                        if (map.erase(lastKey)) {
                            std::wcout << L"[SUCCESS] Key '" << lastKey << L"' removed from Map." << std::endl;
                        }
                        else {
                            std::wcout << L"[WARNING] Key '" << lastKey << L"' not found." << std::endl;
                        }
                    }
                    else if (parent->isArray()) {
                        auto& vec = std::get<vDataArray>(parent->value);
                        try {
                            size_t idx = std::stoul(lastKey);
                            if (idx < vec.size()) {
                                vec.erase(vec.begin() + idx);
                                std::wcout << L"[SUCCESS] Index " << idx << L" removed from Array." << std::endl;
                            }
                            else {
                                std::wcout << L"[ERROR] Index " << idx << L" out of bounds." << std::endl;
                            }
                        }
                        catch (...) {
                            std::wcout << L"[ERROR] Invalid array index: " << lastKey << std::endl;
                        }
                    }
                }
                else {
                    std::wcout << L"[ERROR] Could not resolve parent for: " << cleanArg << std::endl;
                }
            }
        }
    }
    */
void vOliEngine::handleUnsetCommand(const ShellCommand& sc) {
    if (sc.args.empty()) return;

    // 1. Unim totul ca să reparăm ce a spart tokenizer-ul
    std::wstring fullPath = implode(sc.args, L"");
    if (!fullPath.empty() && fullPath[0] == L'$') fullPath.erase(0, 1);

    // Caz special: Stergere totala
    if (fullPath == L"all") {
        m_variables.clear();
        LOG_SUCCESS(L"Memory cleared. All variables removed.");
        return;
    }

    // 2. Analizăm drumul
    auto path = parsePath(fullPath);

    // 3. Caz simplu: /unset $x
    if (path.indexes.empty()) {
        if (m_variables.erase(path.rootName)) {
            LOG_SUCCESS((L"Variable $" + path.rootName + L" removed.").c_str());
        }
        else {
            LOG_ERROR((L"Variable $" + path.rootName + L" not found.").c_str());
        }
        return;
    }

    // 4. Caz complex: /unset $a[0][key]
    vData* parent = resolveToParent(path.rootName, path.indexes);

    if (!parent) {
        // Mesajul de eroare specific vine deja din resolveToParent (dacă ai pus logurile acolo)
        LOG_ERROR((L"Could not resolve path: " + fullPath).c_str());
        return;
    }

    std::wstring lastKey = path.indexes.back();
    // Curățăm ghilimelele dacă lastKey este un string key într-un map
    if (lastKey.size() >= 2 && lastKey.front() == L'\"' && lastKey.back() == L'\"') {
        lastKey = lastKey.substr(1, lastKey.size() - 2);
    }

    bool success = false;
    if (parent->isMap()) {
        auto& map = std::get<vDataMap>(parent->value);
        if (map.erase(lastKey)) {
            success = true;
            LOG_SUCCESS((L"Key '" + lastKey + L"' removed from Map.").c_str());
        }
        else {
            LOG_ERROR((L"Key '" + lastKey + L"' not found in Map.").c_str());
        }
    }
    else if (parent->isArray()) {
        auto& vec = std::get<vDataArray>(parent->value);
        try {
            size_t idx = std::stoul(lastKey);
            if (idx < vec.size()) {
                vec.erase(vec.begin() + idx);
                success = true;
                LOG_SUCCESS((L"Index " + std::to_wstring(idx) + L" removed from Array.").c_str());
            }
            else {
                LOG_ERROR((L"Index " + std::to_wstring(idx) + L" out of bounds.").c_str());
            }
        }
        catch (...) {
            LOG_ERROR((L"Invalid array index: " + lastKey).c_str());
        }
    }
    else {
        LOG_ERROR(L"Parent is not a container (Map or Array).");
    }
}

    vData* vOliEngine::resolveToParent(const std::wstring& rootName, const std::vector<std::wstring>& indexes) {
        // 1. Găsim rădăcina în m_variables
        auto it = m_variables.find(rootName);
        if (it == m_variables.end()) return nullptr;

        vData* current = &(it->second);

        // 2. Navigăm prin indexuri, dar ne oprim înainte de ultimul
        // Exemplu: pentru a[0][1], navigăm doar până la a[0]
        for (size_t i = 0; i < indexes.size() - 1; ++i) {
            const std::wstring& idx = indexes[i];

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
                    size_t nIdx = std::stoul(idx);
                    if (nIdx < vec.size()) {
                        current = &vec[nIdx];
                    }
                    else return nullptr;
                }
                catch (...) { return nullptr; }
            }
            else {
                return nullptr; // Nu este container, nu putem naviga mai adânc
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
        // fullLine arată cam așa: /if 1==1 /then /echo "A" /else /echo "B" /endif

        // 1. Găsim pozițiile cuvintelor cheie (Case-Insensitive folosind startsWith sau find)
        // Folosim o metodă simplă de căutare a sub-stringurilor
        // Căutăm delimitatorii care aparțin de ACEST /IF, nu de cele din interior
        size_t posThen = findTopLevelIfKeyword(fullLine, L"/THEN");
        size_t posElse = findTopLevelIfKeyword(fullLine, L"/ELSE");
        size_t posEndif = findTopLevelIfKeyword(fullLine, L"/ENDIF");

        if (posThen == std::wstring::npos || posEndif == std::wstring::npos) {
            LOG_ERROR(L"Malformed /IF: Missing /THEN or /ENDIF at top level");
            return;
        }

        // 2. Extragem Condiția (între /if și /then)
        // Lungimea lui "/if" este 3 (incluzând spațiul de după, deci +4)
        std::wstring conditionPart = fullLine.substr(3, posThen - 3);
        conditionPart = normalizeSpaces(conditionPart);
        //LOG_DEBUG(conditionPart);
        // Evaluăm condiția
        vData result = evaluateExpression(conditionPart);

        // 2. VERIFICARE PENTRU ERORI (Propagare)
        if (result.isString()) {
            const std::wstring& val = std::get<std::wstring>(result.value);
            if (val.find(L"Error:") == 0) {
                // Afișăm eroarea și ieșim fără să executăm nimic (nici THEN, nici ELSE)
                LOG_ERROR(val);
                return;
            }
        }

        bool isTrue = vDataToBool(result);

        // 3. Extragem sub-comanda care trebuie executată
        std::wstring commandToRun;
        if (isTrue) {
            // De la /then + 5 până la (/else sau /endif)
            size_t start = posThen + 5;
            size_t end = (posElse != std::wstring::npos) ? posElse : posEndif;
            commandToRun = fullLine.substr(start, end - start);
        }
        else if (posElse != std::wstring::npos) {
            // De la /else + 5 până la /endif
            size_t start = posElse + 5;
            commandToRun = fullLine.substr(start, posEndif - start);
        }

        // 4. Execuție recursivă (MODIFICAT)
        commandToRun = normalizeSpaces(commandToRun);
        if (!commandToRun.empty()) {
            // Trimitem înapoi la execute() pentru a permite PreParser-ului 
            // să spargă comenzile multiple din interiorul ramurii if/else
            //this->execute(commandToRun);
            std::vector<std::wstring> subInstructions = preParse(commandToRun);
            for (const auto& subInstr : subInstructions) {
                if (!subInstr.empty()) {
                    this->execute(subInstr);

                    // DACĂ S-A CERUT BREAK SAU CONTINUE, IEȘIM IMEDIAT DIN IF
                    // Nu resetăm statusul aici! Îl lăsăm pentru WHILE.
                    if (m_executionStatus != OliStatus::RUNNING) {
                        return;
                    }
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

        size_t firstIf = upperLine.find(L"/IF");
        if (firstIf == std::wstring::npos) return std::wstring::npos;

        for (size_t i = firstIf + 3; i < upperLine.size(); ++i) {
            if (upperLine[i] == L'"') { inQuotes = !inQuotes; continue; }
            if (inQuotes) continue;

            std::wstring_view rem(&upperLine[i], upperLine.size() - i);

            // --- LOGICA CORECTATĂ ---

            // 1. Dacă căutăm /ENDIF și l-am găsit la depth 0, îl returnăm imediat
            if (depth == 0 && rem.starts_with(upperKey)) {
                // Verificare delimitare (spațiu, sfârșit de linie, etc.)
                size_t nextIdx = i + upperKey.length();
                if (nextIdx >= upperLine.size() || iswspace(upperLine[nextIdx]) || upperLine[nextIdx] == L'/') {
                    return i;
                }
            }

            // 2. Gestionăm ierarhia pentru SUB-BLOCURI
            if (rem.starts_with(L"/IF ")) {
                depth++;
                i += 2;
                continue;
            }
            if (rem.starts_with(L"/ENDIF")) {
                depth--;
                i += 5;
                continue;
            }
        }
        return std::wstring::npos;
    }

    bool vOliEngine::vDataToBool(const vData& data) {
        // 1. Dacă este nulă (std::monostate) -> FALSE
        if (data.IsNull()) return false;

        // 2. Numere întregi -> TRUE dacă nu sunt 0
        if (data.isInt()) {
            return std::get<long long>(data.value) != 0;
        }

        // 3. Numere cu virgulă -> TRUE dacă sunt diferite de 0 (cu o mică toleranță)
        if (data.isFloat()) {
            return std::abs(std::get<double>(data.value)) > 1e-9;
        }

        // 4. String-uri -> TRUE dacă nu sunt goale
        if (data.isString()) {
            const std::wstring& s = std::get<std::wstring>(data.value);
            // Putem decide dacă string-ul "false" e tratat ca boolean false
            return !s.empty() && s != L"false";
        }

        // 5. Containere (Array/Map) -> TRUE dacă au elemente
        if (data.isArray()) {
            return !std::get<vDataArray>(data.value).empty();
        }
        if (data.isMap()) {
            return !std::get<vDataMap>(data.value).empty();
        }

        // 6. Booleeni expliciți
        if (data.isBool()) {
            return std::get<bool>(data.value);
        }

        return false;
    }

    /*
    std::vector<std::wstring> vOliEngine::preParse(const std::wstring& line) {
        std::vector<std::wstring> commands;
        size_t i = 0;

        while (i < line.size()) {
            // 1. Salt peste spații
            while (i < line.size() && iswspace(line[i])) i++;
            if (i >= line.size()) break;

            // 2. Orice comandă OLI începe cu '/'
            if (line[i] == L'/') {
                size_t start = i;

                // Extragem doar numele comenzii (ex: /IF, /ECHO)
                size_t nextSpace = line.find_first_of(L" \t\n\r", i);
                std::wstring fullWord = line.substr(i, nextSpace - i);

                // Verificăm dacă este un /IF (care are nevoie de tratament special)
                if (vOliKeyWords::is(fullWord, L"/IF")) {
                    int depth = 1;
                    i += fullWord.length(); // trecem de /IF

                    while (i < line.size() && depth > 0) {
                        // Ignorăm conținutul din ghilimele ca să nu detectăm /IF în texte
                        if (line[i] == L'"') {
                            i++; while (i < line.size() && line[i] != L'"') i++;
                            if (i < line.size()) i++; continue;
                        }

                        // Verificăm dacă urmează un sub-keyword
                        std::wstring remainder = line.substr(i);
                        if (startsWith(remainder, L"/IF", true)) depth++;
                        if (startsWith(remainder, L"/ENDIF", true)) {
                            depth--;
                            if (depth == 0) {
                                i += 6; // lungimea lui "/endif"
                                break;
                            }
                        }
                        i++;
                    }
                    commands.push_back(line.substr(start, i - start));
                }
                else {
                    // Comandă simplă (/SET, /ECHO, /BEEP)
                    // Mergem până la următorul '/' care este început de Shell Command VALID
                    i++; // trecem de '/' de start
                    while (i < line.size()) {
                        if (line[i] == L'"') {
                            i++; while (i < line.size() && line[i] != L'"') i++;
                            if (i < line.size()) i++; continue;
                        }

                        if (line[i] == L'/') {
                            // Verificăm dacă acest '/' este o comandă nouă sau doar un operator de împărțire
                            size_t nextS = line.find_first_of(L" \t\n\r", i);
                            std::wstring checkCmd = line.substr(i, nextS - i);

                            // Dacă e o comandă cunoscută (ex: /SET), tăiem aici
                            // DAR dacă este /THEN, /ELSE sau /ENDIF, nu tăiem (aparțin de /IF)
                            if (vOliKeyWords::isShellCommand(checkCmd)) {
                                if (!vOliKeyWords::is(checkCmd, L"/THEN") &&
                                    !vOliKeyWords::is(checkCmd, L"/ELSE") &&
                                    !vOliKeyWords::is(checkCmd, L"/ENDIF")) {
                                    break; // E o comandă nouă independentă
                                }
                            }
                        }
                        i++;
                    }
                    commands.push_back(line.substr(start, i - start));
                }
            }
            else {
                i++;
            }
        }
        return commands;
    }

    */
    /*
    std::vector<std::wstring> vOliEngine::preParse(const std::wstring& line) {
        std::vector<std::wstring> commands;
        size_t i = 0;

        while (i < line.size()) {
            while (i < line.size() && iswspace(line[i])) i++;
            if (i >= line.size()) break;

            if (line[i] == L'/') {
                size_t start = i;
                size_t nextSpace = line.find_first_of(L" \t\n\r", i);
                std::wstring fullWord = line.substr(i, nextSpace - i);

                // Verificăm dacă este o comandă de tip "container"
                if (vOliKeyWords::is(fullWord, L"/IF") || vOliKeyWords::is(fullWord, L"/WHILE")) {
                    int depth = 1;
                    i += fullWord.length();

                    while (i < line.size() && depth > 0) {
                        // Sărim peste ghilimele
                        if (line[i] == L'"') {
                            i++; while (i < line.size() && line[i] != L'"') i++;
                            if (i < line.size()) i++; continue;
                        }

                        std::wstring_view rem(&line[i], line.size() - i);

                        // Orice structură nouă crește adâncimea
                        if (rem.starts_with(L"/IF ") || rem.starts_with(L"/WHILE ")) {
                            depth++;
                            i += (rem.starts_with(L"/IF ") ? 3 : 6);
                        }
                        // Orice închidere scade adâncimea
                        else if (rem.starts_with(L"/ENDIF")) {
                            depth--;
                            if (depth == 0) { i += 6; break; }
                            i += 5;
                        }
                        else if (rem.starts_with(L"/ENDWHILE")) {
                            depth--;
                            if (depth == 0) { i += 9; break; }
                            i += 8;
                        }
                        else {
                            i++;
                        }
                    }
                    commands.push_back(line.substr(start, i - start));
                }
                else {
                    // COMENZI SIMPLE
                    i++;
                    while (i < line.size()) {
                        if (line[i] == L'"') {
                            i++; while (i < line.size() && line[i] != L'"') i++;
                            if (i < line.size()) i++; continue;
                        }
                        if (line[i] == L'/') {
                            size_t nextS = line.find_first_of(L" \t\n\r", i);
                            std::wstring checkCmd = line.substr(i, nextS - i);
                            if (vOliKeyWords::isShellCommand(checkCmd)) {
                                // Cuvintele interne nu rup comanda
                                if (!vOliKeyWords::is(checkCmd, L"/THEN") &&
                                    !vOliKeyWords::is(checkCmd, L"/ELSE") &&
                                    !vOliKeyWords::is(checkCmd, L"/ENDIF") &&
                                    !vOliKeyWords::is(checkCmd, L"/DO") &&
                                    !vOliKeyWords::is(checkCmd, L"/ENDWHILE")) {
                                    break;
                                }
                            }
                        }
                        i++;
                    }
                    commands.push_back(line.substr(start, i - start));
                }
            }
            else {
                i++;
            }
        }
        return commands;
    }
    */
/*
std::vector<std::wstring> vOliEngine::preParse(const std::wstring& line) {
    std::vector<std::wstring> commands;
    size_t i = 0;

    while (i < line.size()) {
        while (i < line.size() && iswspace(line[i])) i++;
        if (i >= line.size()) break;

        if (line[i] == L'/') {
            size_t start = i;
            size_t nextSpace = line.find_first_of(L" \t\n\r", i);
            std::wstring fullWord = line.substr(i, nextSpace - i);

            // 1. Verificăm dacă este un container (IF, WHILE sau CYCLE)
            if (vOliKeyWords::is(fullWord, L"/IF") ||
                vOliKeyWords::is(fullWord, L"/WHILE") ||
                vOliKeyWords::is(fullWord, L"/CYCLE")) {

                int depth = 1;
                i += fullWord.length();

                while (i < line.size() && depth > 0) {
                    // Sărim peste ghilimele pentru a nu detecta slash-uri false în text
                    if (line[i] == L'"') {
                        i++; while (i < line.size() && line[i] != L'"') i++;
                        if (i < line.size()) i++; continue;
                    }

                    std::wstring_view rem(&line[i], line.size() - i);

                    // Detectăm început de sub-bloc
                    //if (rem.starts_with(L"/IF ") || rem.starts_with(L"/WHILE ") || rem.starts_with(L"/CYCLE ")) {
                    //    depth++;
                    //    if (rem.starts_with(L"/IF ")) i += 3;
                    //    else i += 6; // și WHILE și CYCLE au 6 caractere
                    //}
                    // Detectăm început de sub-bloc (fără să depindem de un spațiu fix la final)
                    

                    auto checkStart = [&](const std::wstring& cmd, size_t len) {
                        if (rem.starts_with(cmd)) {
                            // Verificăm dacă e cuvânt întreg: urmează final de string sau non-alfanumeric
                            if (rem.size() == len || !iswalnum(rem[cmd.length()])) return true;
                        }
                        return false;
                    };

                    if (checkStart(L"/IF", 3) || checkStart(L"/WHILE", 6) || checkStart(L"/CYCLE", 6)) {
                        depth++;
                        i += (rem.starts_with(L"/IF") ? 3 : 6);
                        continue;
                    }
                    // Detectăm sfârșit de sub-bloc
                    else if (rem.starts_with(L"/ENDIF")) {
                        depth--;
                        if (depth == 0) { i += 6; break; }
                        i += 5;
                    }
                    else if (rem.starts_with(L"/ENDWHILE")) {
                        depth--;
                        if (depth == 0) { i += 9; break; }
                        i += 8;
                    }
                    else if (rem.starts_with(L"/ENDCYCLE")) { // <--- ADAUGAT
                        depth--;
                        if (depth == 0) { i += 9; break; }
                        i += 8;
                    }
                    else {
                        i++;
                    }
                }
                commands.push_back(line.substr(start, i - start));
            }
            else {
                // 2. COMENZI SIMPLE (Ex: /echo, /set)
                i++;
                while (i < line.size()) {
                    if (line[i] == L'"') {
                        i++; while (i < line.size() && line[i] != L'"') i++;
                        if (i < line.size()) i++; continue;
                    }
                    if (line[i] == L'/') {
                        size_t nextS = line.find_first_of(L" \t\n\r", i);
                        std::wstring checkCmd = line.substr(i, nextS - i);

                        if (vOliKeyWords::isShellCommand(checkCmd)) {
                            // Cuvintele interne ale blocurilor nu trebuie să rupă comanda simplă
                            // (De exemplu, dacă /echo e în interiorul unui /if)
                            if (!vOliKeyWords::is(checkCmd, L"/THEN") &&
                                !vOliKeyWords::is(checkCmd, L"/ELSE") &&
                                !vOliKeyWords::is(checkCmd, L"/ENDIF") &&
                                !vOliKeyWords::is(checkCmd, L"/DO") &&
                                !vOliKeyWords::is(checkCmd, L"/ENDWHILE") &&
                                !vOliKeyWords::is(checkCmd, L"/ENDCYCLE") && // <--- ADAUGAT
                                !vOliKeyWords::is(checkCmd, L"/AS")) {        // <--- ADAUGAT
                                break;
                            }
                        }
                    }
                    i++;
                }
                commands.push_back(line.substr(start, i - start));
            }
        }
        else {
            i++;
        }
    }
    return commands;
}
*/

std::vector<std::wstring> vOliEngine::preParse(const std::wstring& line) {
    std::vector<std::wstring> commands;
    size_t i = 0;

    while (i < line.size()) {
        // 1. Sărim peste spațiile inițiale
        while (i < line.size() && iswspace(line[i])) i++;
        if (i >= line.size()) break;

        size_t start = i;

        if (line[i] == L'/') {
            // Extragem primul cuvânt pentru a identifica tipul comenzii (ex: /WHILE)
            size_t nextSpace = line.find_first_of(L" \t\n\r($", i);
            std::wstring firstWord = line.substr(i, (nextSpace == std::wstring::npos ? line.size() : nextSpace) - i);

            // Transformăm în UpperCase doar pentru verificare
            std::wstring upperWord = firstWord;
            std::transform(upperWord.begin(), upperWord.end(), upperWord.begin(), ::towupper);

            // --- SECȚIUNEA 1: CONTAINERE (Blocuri care au un început și un sfârșit) ---
            if (upperWord == L"/IF" || upperWord == L"/WHILE" || upperWord == L"/CYCLE" || upperWord ==  L"/FOR") {
                int depth = 0;

                // Parcurgem linia pentru a găsi perechea de închidere corespunzătoare
                while (i < line.size()) {
                    // Sărim peste string-uri (ghilimele) pentru a nu detecta slash-uri false
                    if (line[i] == L'"') {
                        i++;
                        while (i < line.size() && line[i] != L'"') i++;
                        if (i < line.size()) i++;
                        continue;
                    }

                    std::wstring_view rem(&line[i], line.size() - i);

                    // Verificăm început de bloc (IF, WHILE, CYCLE)
                    if (rem.starts_with(L"/")) {
                        // Extragem cuvântul curent de control
                        size_t endWord = 0;
                        while (endWord < rem.size() && iswalnum(rem[endWord + 1])) endWord++;
                        std::wstring currentCmd(rem.substr(0, endWord + 1));
                        std::transform(currentCmd.begin(), currentCmd.end(), currentCmd.begin(), ::towupper);

                        if (currentCmd == L"/IF" || currentCmd == L"/WHILE" || currentCmd == L"/CYCLE" || currentCmd == L"/FOR") {
                            depth++;
                            i += currentCmd.length();
                            continue;
                        }
                        else if (currentCmd == L"/ENDIF" || currentCmd == L"/ENDWHILE" || currentCmd == L"/ENDCYCLE" || currentCmd == L"/ENDFOR") {
                            depth--;
                            i += currentCmd.length();
                            if (depth == 0) break; // Am găsit închiderea blocului principal
                            continue;
                        }
                    }
                    i++;
                }
                commands.push_back(line.substr(start, i - start));
            }
            // --- SECȚIUNEA 2: COMENZI SIMPLE (/SET, /ECHO, /WAIT, etc.) ---
            else {
                i++; // Sărim peste '/' inițial
                while (i < line.size()) {
                    // Sărim peste string-uri
                    if (line[i] == L'"') {
                        i++;
                        while (i < line.size() && line[i] != L'"') i++;
                        if (i < line.size()) i++;
                        continue;
                    }

                    // Dacă întâlnim un nou '/', verificăm dacă este o comandă nouă sau un sub-keyword
                    if (line[i] == L'/') {
                        size_t endToken = i + 1;
                        while (endToken < line.size() && iswalnum(line[endToken])) endToken++;
                        std::wstring checkCmd = line.substr(i, endToken - i);
                        std::transform(checkCmd.begin(), checkCmd.end(), checkCmd.begin(), ::towupper);

                        // Dacă este o comandă de sistem care NU face parte dintr-o structură, rupem aici
                        if (vOliKeyWords::isShellCommand(checkCmd)) {
                            if (checkCmd != L"/THEN" && checkCmd != L"/ELSE" &&
                                checkCmd != L"/DO" && checkCmd != L"/AS") {
                                break; // Este o comandă nouă (ex: /SET), deci terminăm comanda curentă
                            }
                        }
                    }
                    i++;
                }
                commands.push_back(line.substr(start, i - start));
            }
        }
        else {
            i++; // Ignorăm caracterele care nu încep cu '/' la nivel de top
        }
    }
    return commands;
}


/*
    void vOliEngine::handleWhileCommand(const std::wstring& fullLine) {
        // Exemplu fullLine: /while $i < 5 /do /echo $i /set i = $i + 1 /endwhile

        // 1. Găsim delimitatorii la nivelul de top pentru acest /WHILE specific
        size_t posDo = findTopLevelKeyword(fullLine, L"/DO", L"/WHILE");
        size_t posEnd = findTopLevelKeyword(fullLine, L"/ENDWHILE", L"/WHILE");

        if (posDo == std::wstring::npos || posEnd == std::wstring::npos) {
            LOG_ERROR(L"Malformed /WHILE: Missing /DO or /ENDWHILE at top level");
            return;
        }

        // 2. Extragem Condiția (între /while și /do)
        // /while are 6 caractere. Adăugăm 1 pentru spațiu dacă e cazul.
        std::wstring conditionPart = fullLine.substr(6, posDo - 6);
        conditionPart = normalizeSpaces(conditionPart);

        // 3. Extragem Corpul buclei (între /do și /endwhile)
        // /do are 3 caractere ("/do ")
        size_t startBody = posDo + 3;
        std::wstring bodyCommand = fullLine.substr(startBody, posEnd - startBody);
        bodyCommand = normalizeSpaces(bodyCommand);

        // 4. Bucla de execuție
        int safetyBreak = 0;
        const int MAX_ITERATIONS = 1000; // Protecție esențială pentru OLI



        while (true) {
            // Evaluăm condiția la fiecare iterație pentru a vedea modificările variabilelor
            vData result = evaluateExpression(conditionPart);

            // Verificăm dacă evaluarea condiției a produs o eroare (ex: variabilă inexistentă)
            if (result.isString()) {
                const std::wstring& val = std::get<std::wstring>(result.value);
                if (val.find(L"Error:") == 0) {
                    LOG_ERROR(val);
                    break;
                }
            }

            // Convertim rezultatul în boolean (0, "", moonstate sunt false)
            if (!vDataToBool(result)) {
                break; // Condiția a devenit false, ieșim din buclă
            }

            // 5. Executăm corpul buclei
            std::vector<std::wstring> instructions = preParse(bodyCommand);
            if (!bodyCommand.empty()) {
                // Trimitem corpul buclei înapoi în execute() 
                // PreParser-ul va sparge automat multiplele comenzi din interior
                //this->execute(bodyCommand);
                
                for (const auto& instr : instructions) {
                    if (!instr.empty()) {
                        this->execute(instr);
                        if (m_executionStatus == OliStatus::BREAK_REQUESTED) {
                            // "Consumăm" statusul pentru a nu opri și bucla părinte (dacă există)
                            m_executionStatus = OliStatus::RUNNING;

                            // Ieșim forțat din WHILE-ul curent
                            return; // sau break din while(true)
                        }

                        if (m_executionStatus == OliStatus::CONTINUE_REQUESTED) {
                            m_executionStatus = OliStatus::RUNNING;

                            // Sărim peste restul instrucțiunilor din acest 'for'
                            // și trecem direct la următoarea iterație a buclei 'while(true)'
                            goto next_iteration;
                        }
                    }
                next_iteration:
                }
                
            }

            // 6. Verificare Loop Infinit
            if (++safetyBreak > MAX_ITERATIONS) {
                LOG_ERROR(L"Error: Infinite loop detected or too many iterations (>1000)");
                break;
            }
        }
    }
*/
void vOliEngine::handleWhileCommand(const std::wstring& fullLine) {
    // 1. Găsim delimitatorii (logică neschimbată)
    size_t posDo = findTopLevelKeyword(fullLine, L"/DO", L"/WHILE");
    size_t posEnd = findTopLevelKeyword(fullLine, L"/ENDWHILE", L"/WHILE");

    if (posDo == std::wstring::npos || posEnd == std::wstring::npos) {
        LOG_ERROR(L"Malformed /WHILE: Missing /DO or /ENDWHILE");
        return;
    }

    // 2. Extragem Condiția și Corpul
    std::wstring conditionPart = normalizeSpaces(fullLine.substr(6, posDo - 6));
    std::wstring bodyCommand = normalizeSpaces(fullLine.substr(posDo + 3, posEnd - (posDo + 3)));

    // --- OPTIMIZARE: Pre-parsăm corpul O SINGURĂ DATĂ înainte de loop ---
    std::vector<std::wstring> instructions = preParse(bodyCommand);

    // 4. Bucla de execuție
    int safetyBreak = 0;
    const int MAX_ITERATIONS = 1000;

    while (true) {
        vData result = evaluateExpression(conditionPart);

        // Verificare erori evaluare
        if (result.isString()) {
            const std::wstring& val = std::get<std::wstring>(result.value);
            if (val.find(L"Error:") == 0) { LOG_ERROR(val); break; }
        }

        if (!vDataToBool(result)) break;

        // 5. Executăm instrucțiunile pre-calculate
        std::wstring current_instr;
        for (const auto& instr : instructions) {
            current_instr = instr;
            if (!instr.empty()) {
                this->execute(instr);

                if (m_executionStatus == OliStatus::BREAK_REQUESTED) {
                    m_executionStatus = OliStatus::RUNNING;
                    return; // Ieșim din WHILE
                }

                if (m_executionStatus == OliStatus::CONTINUE_REQUESTED) {
                    //LOG_ERROR(L"AICI");
                    m_executionStatus = OliStatus::RUNNING;
                    goto next_iteration; // Sărim la următoarea verificare a condiției
                    //break;
                }
            }
        }

    next_iteration:
        if (++safetyBreak > MAX_ITERATIONS) {
            LOG_ERROR(L"Error: Infinite loop detected");
            break;
        }
        else {
            //LOG_DEBUG(L"Continue! " + current_instr);
        }
    }
}
/*
    size_t vOliEngine::findTopLevelKeyword(const std::wstring& line, const std::wstring& keyword, const std::wstring& startCommand) {
        LOG_DEBUG(line);
        int depth = 0;
        bool inQuotes = false;

        std::wstring upperLine = line;
        std::transform(upperLine.begin(), upperLine.end(), upperLine.begin(), ::towupper);
        std::wstring upperKey = keyword;
        std::transform(upperKey.begin(), upperKey.end(), upperKey.begin(), ::towupper);
        std::wstring upperStart = startCommand;
        std::transform(upperStart.begin(), upperStart.end(), upperStart.begin(), ::towupper);

        // Găsim prima apariție a comenzii de start (ex: /IF sau /WHILE)
        size_t startPos = upperLine.find(upperStart);
        if (startPos == std::wstring::npos) return std::wstring::npos;

        // Începem căutarea după comanda de start
        for (size_t i = startPos + upperStart.length(); i < upperLine.size(); ++i) {
            if (upperLine[i] == L'"') { inQuotes = !inQuotes; continue; }
            if (inQuotes) continue;

            std::wstring_view rem(&upperLine[i], upperLine.size() - i);

            // 1. Verificăm dacă am găsit keyword-ul căutat la adâncimea corectă
            if (depth == 0 && rem.starts_with(upperKey)) {
                size_t nextIdx = i + upperKey.length();
                // Verificăm să fie un cuvânt de sine stătător (urmat de spațiu, alt slash sau final)
                if (nextIdx >= upperLine.size() || iswspace(upperLine[nextIdx]) || upperLine[nextIdx] == L'/') {
                    return i;
                }
            }

            // 2. Gestionăm ierarhia pentru ORICE structură de control (Nested Blocks)

            // Dacă intrăm într-un sub-bloc (IF sau WHILE)
            if (rem.starts_with(L"/IF") || rem.starts_with(L"/WHILE") || rem.starts_with(L"/CYCLE")) {
                size_t len = rem.starts_with(L"/IF") ? 3 : 6;
                // Verificăm să nu fie parte dintr-un cuvânt mai lung (ex: /IF_DATA)
                if (rem.size() == len || iswspace(rem[len]) || rem[len] == L'$' || rem[len] == L'(') {
                    depth++;
                    i += (len - 1);
                    continue;
                }
            }

            if (rem.starts_with(L"/ENDIF") || rem.starts_with(L"/ENDWHILE") || rem.starts_with(L"/ENDCYCLE")) {
                depth--;
                if (rem.starts_with(L"/ENDIF")) i += 5;
                else if (rem.starts_with(L"/ENDWHILE")) i += 8;
                else if (rem.starts_with(L"/ENDCYCLE")) i += 8; // /ENDCYCLE are 9 caractere (cu /), deci i += 8 e ok pentru i++ din for
                continue;
            }
        }
        return std::wstring::npos;
    }
    */

size_t vOliEngine::findTopLevelKeyword(const std::wstring& line, const std::wstring& keyword, const std::wstring& startCommand) {
    int depth = 0;
    bool inQuotes = false;

    // Normalizăm pentru case-insensitivity
    std::wstring upperLine = line;
    std::transform(upperLine.begin(), upperLine.end(), upperLine.begin(), ::towupper);
    std::wstring upperKey = keyword;
    std::transform(upperKey.begin(), upperKey.end(), upperKey.begin(), ::towupper);
    std::wstring upperStart = startCommand;
    std::transform(upperStart.begin(), upperStart.end(), upperStart.begin(), ::towupper);

    // Găsim poziția de start a comenzii mamă (ex: unde începe /FOR-ul curent)
    size_t startPos = upperLine.find(upperStart);
    if (startPos == std::wstring::npos) return std::wstring::npos;

    // Începem căutarea imediat după cuvântul de start (ex: după "/FOR")
    for (size_t i = startPos + upperStart.length(); i < upperLine.size(); ++i) {

        // 1. Ignorăm tot ce e în ghilimele
        if (upperLine[i] == L'"') {
            inQuotes = !inQuotes;
            continue;
        }
        if (inQuotes) continue;

        std::wstring_view rem(&upperLine[i], upperLine.size() - i);

        // 2. Dacă suntem la adâncimea 0, verificăm dacă am găsit keyword-ul (ex: /TO, /DO, /ENDFOR)
        if (depth == 0 && rem.starts_with(upperKey)) {
            size_t nextIdx = i + upperKey.length();
            // Verificăm să fie cuvânt întreg (urmat de spațiu, slash, paranteză sau final de linie)
            if (nextIdx >= upperLine.size() || iswspace(upperLine[nextIdx]) ||
                upperLine[nextIdx] == L'/' || upperLine[nextIdx] == L'(' || upperLine[nextIdx] == L'$') {
                return i;
            }
        }

        // 3. LOGICA DE ADÂNCIME (Nested Blocks)
        if (rem.starts_with(L"/")) {
            // Extragem cuvântul care începe cu / (ex: /IF, /ENDIF, /FOR, /ENDFOR)
            size_t wordLen = 1;
            while (wordLen < rem.size() && iswalnum(rem[wordLen])) {
                wordLen++;
            }
            std::wstring currentCmd(rem.substr(0, wordLen));

            // Verificăm dacă este un început de bloc nou (/IF, /WHILE, /FOR, /CYCLE)
            // Excludem comenzile simple ca /SET, /ECHO folosind o logică de tip "starts with /END"
            if (currentCmd.length() > 3 && currentCmd.substr(0, 4) == L"/END") {
                // Este o comandă de închidere (ex: /ENDFOR)
                depth--;
                i += (currentCmd.length() - 1);
                continue;
            }
            else if (currentCmd == L"/IF" || currentCmd == L"/WHILE" ||
                currentCmd == L"/FOR" || currentCmd == L"/CYCLE") {
                // Este o comandă de deschidere a unui sub-bloc
                depth++;
                i += (currentCmd.length() - 1);
                continue;
            }
        }
    }

    return std::wstring::npos;
}





    void vOliEngine::handleRunCommand(const ShellCommand& sc) {
        if (sc.args.empty()) return;

        std::wstring pathStr = sc.args[0];
        if (pathStr.size() >= 2 && pathStr.front() == L'"' && pathStr.back() == L'"') {
            pathStr = pathStr.substr(1, pathStr.size() - 2);
        }

        // Deschidem ca ifstream NORMAL (nu wifstream), citim octeți bruti (UTF-8)
        std::string utf8 = wstring_to_utf8(pathStr);
        std::ifstream file(utf8);
        if (!file.is_open()) {
            LOG_ERROR(L"Could not open script: " + pathStr);
            return;
        }

        std::string lineA; // Buffer pentru textul ANSI/UTF-8 (octeți)
        bool firstLine = true;

        while (std::getline(file, lineA)) {
            // 1. Convertim din UTF-8 (string) în UTF-16 (wstring) folosind Windows API
            if (lineA.empty()) continue;
#ifdef _WIN32
            int size_needed = MultiByteToWideChar(CP_UTF8, 0, &lineA[0], (int)lineA.size(), NULL, 0);
            std::wstring lineW(size_needed, 0);
            MultiByteToWideChar(CP_UTF8, 0, &lineA[0], (int)lineA.size(), &lineW[0], size_needed);
#else
            std::wstring lineW = utf8_to_wstring(lineA);
#endif
            // 2. Curățăm BOM-ul dacă e prima linie
            if (firstLine && !lineW.empty() && lineW[0] == 0xFEFF) {
                lineW.erase(0, 1);
            }
            firstLine = false;

            // 3. Normalizăm și executăm
            std::wstring finalLine = normalizeSpaces(lineW);
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
        return { 0LL }; // Returnăm ceva neutru
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
#ifdef _WIN32
        FILE* pipe = _wpopen(command.c_str(), L"r");
        if (!pipe) return { L"ERROR" };

        wchar_t buffer[128];
        while (fgetws(buffer, 128, pipe)) {
            output += buffer;
        }

        _pclose(pipe);
#else
        // ---------------- LINUX ----------------
    // Convertim comanda la UTF-8
        std::string utf8cmd = wstring_to_utf8(command);

        FILE* pipe = popen(utf8cmd.c_str(), "r");
        if (!pipe) return { L"ERROR" };

        char buffer[256];
        std::string utf8out;

        while (fgets(buffer, sizeof(buffer), pipe)) {
            utf8out += buffer;
        }

        pclose(pipe);

        // Convertim output-ul UTF-8 → UTF-16
        output = utf8_to_wstring(utf8out);
#endif
        // 3. Returnăm rezultatul ca STRING în Oli
        //return vData(L"\"" + output + L"\"");
        return vData( output );
    }


    void vOliEngine::handleSysCommand(const ShellCommand& sc) {
        if (sc.args.empty()) {
            LOG_ERROR(L"Usage: /sys <system_command>");
            return;
        }

        // 1. Construim comanda
        std::wstring fullCommand;
        for (const auto& arg : sc.args) fullCommand += arg + L" ";

        // Eliminăm spațiul de la final adăugat de loop
        if (!fullCommand.empty()) fullCommand.pop_back();

        fullCommand = substituteVariables(fullCommand, m_variables);

        // Curățare ghilimele
        if (fullCommand.size() >= 2 && fullCommand.front() == L'"' && fullCommand.back() == L'"') {
            fullCommand = fullCommand.substr(1, fullCommand.size() - 2);
        }

        LOG_INFO(L"Executing: " + fullCommand);

        // --- CRITIC: Golim bufferele înainte de a preda controlul pipe-ului ---
        std::wcout.flush();
        fflush(stdout);
#ifdef _WIN32
        // 2. Executăm. Folosim "r" și setăm modul pipe-ului manual dacă e nevoie.
        // Pe unele versiuni de Windows, "rt" (read text) poate fi problematic cu _O_U16TEXT
        FILE* pipe = _wpopen(fullCommand.c_str(), L"r");

        if (!pipe) {
            LOG_ERROR(L"Could not execute system command.");
            return;
        }

        wchar_t buffer[128];
        // Folosim fgetws pentru a citi Unicode
        while (fgetws(buffer, 128, pipe)) {
            std::wcout << buffer;
            std::wcout.flush(); // Afișăm în timp real, să nu pară blocat
        }

        int returnCode = _pclose(pipe);
#else
        // ---------------- LINUX ----------------
    // Convertim comanda wide → UTF-8
        std::string utf8cmd = wstring_to_utf8(fullCommand);

        FILE* pipe = popen(utf8cmd.c_str(), "r");
        if (!pipe) {
            LOG_ERROR(L"Could not execute system command.");
            return;
        }

        char buffer[256];
        std::string utf8out;

        while (fgets(buffer, sizeof(buffer), pipe)) {
            utf8out = buffer;

            // Convertim linia la wide
            std::wstring wline = utf8_to_wstring(utf8out);

            std::wcout << wline;
            std::wcout.flush();
        }

        int returnCode = pclose(pipe);
#endif
        if (returnCode == 0) {
            LOG_SUCCESS(L"Command finished.");
            return;
        }
        else {
            LOG_ERROR(L"Command failed with code: " + std::to_wstring(returnCode));
            return;
        }
    }

    std::wstring vOliEngine::substituteVariables(const std::wstring& input, const std::map<std::wstring, vData>& vars) {
        std::wstring result = input;
        size_t pos = 0;

        while ((pos = result.find(L'$', pos)) != std::wstring::npos) {
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
                while (endName < result.length() && (iswalnum(result[endName]) || result[endName] == L'_')) {
                    endName++;
                }
            }

            if (endName != std::wstring::npos && endName > startName) {
                std::wstring varName = result.substr(startName, endName - startName);

                if (vars.count(varName)) {
                    std::wstring varValue = vDataToWString(vars.at(varName));

                    // Curățăm ghilimelele dacă e string
                    if (varValue.size() >= 2 && varValue.front() == L'"' && varValue.back() == L'"') {
                        varValue = varValue.substr(1, varValue.size() - 2);
                    }

                    size_t totalLenToReplace = hasBraces ? (endName - pos + 1) : (endName - pos);
                    result.replace(pos, totalLenToReplace, varValue);
                    pos += varValue.length();
                    continue;
                }
            }
            pos++; // Trecem peste '$' dacă nu am înlocuit nimic
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
        /*
        if (vOliKeyWords::isShellCommand(procName)) {
            LOG_ERROR(L"Cannot shadow internal command: " + procName);
            return;
        }
        */
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


    // Returnează true dacă trebuie să dăm BREAK la bucla principală C++
    bool vOliEngine::executeCycleStep(const std::wstring& iterName, const vData& value, const std::vector<std::wstring>& instrs) {
        m_variables[iterName] = value;

        for (const auto& instr : instrs) {
            if (instr.empty()) continue;

            this->execute(instr);

            // 1. Verificăm BREAK
            if (m_executionStatus == OliStatus::BREAK_REQUESTED) {
                m_executionStatus = OliStatus::RUNNING; // Consumăm semnalul
                return true; // Spunem buclei for/while din C++ să se oprească (BREAK)
            }

            // 2. Verificăm CONTINUE
            if (m_executionStatus == OliStatus::CONTINUE_REQUESTED) {
                m_executionStatus = OliStatus::RUNNING; // Consumăm semnalul
                return false; // Ieșim din acest for, dar NU oprim bucla mare (CONTINUE)
            }
        }
        return false;
    }


    void vOliEngine::handleCycleCommand(const std::wstring& fullLine) {
        // 1. Găsim poziția de start și delimitatorii principali
        std::wstring upperLine = fullLine;
        std::transform(upperLine.begin(), upperLine.end(), upperLine.begin(), ::towupper);
        size_t cyclePos = upperLine.find(L"/CYCLE");

        size_t posDo = findTopLevelKeyword(fullLine, L"/DO", L"/CYCLE");
        size_t posEnd = findTopLevelKeyword(fullLine, L"/ENDCYCLE", L"/CYCLE");

        if (posDo == std::wstring::npos || posEnd == std::wstring::npos) {
            LOG_ERROR(L"Malformed /CYCLE: Missing /DO or /ENDCYCLE");
            return;
        }

        // 2. Extragere Header (între /CYCLE și /DO)
        size_t headerStart = cyclePos + 6;
        std::wstring header = normalizeSpaces(fullLine.substr(headerStart, posDo - headerStart));

        // Parsăm header: "expresie_sursa /as iterator"
        std::wstring upperHeader = header;
        std::transform(upperHeader.begin(), upperHeader.end(), upperHeader.begin(), ::towupper);
        size_t asPos = upperHeader.find(L"/AS");

        if (asPos == std::wstring::npos) {
            LOG_ERROR(L"/CYCLE requires '/as' to define the iterator variable.");
            return;
        }

        std::wstring sourceExpr = normalizeSpaces(header.substr(0, asPos));
        std::wstring iteratorName = normalizeSpaces(header.substr(asPos + 3));

        // Curățăm doar numele iteratorului (pentru a-l folosi ca cheie în m_variables)
        if (!iteratorName.empty() && iteratorName[0] == L'$') iteratorName.erase(0, 1);

        // 3. Extragere Corp (între /DO și /ENDCYCLE)
        size_t bodyStart = posDo + 3;
        std::wstring bodyCommand = normalizeSpaces(fullLine.substr(bodyStart, posEnd - bodyStart));

        // 4. EVALUARE SURSĂ (Permite $inventar[$numeObiect])
        vData sourceData = evaluateExpression(sourceExpr);

        if (std::holds_alternative<std::monostate>(sourceData.value)) {
            LOG_ERROR(L"Cycle error: Source '" + sourceExpr + L"' evaluated to null.");
            return;
        }

        // 5. Shadowing protection
        vData oldVal = m_variables.count(iteratorName) ? m_variables[iteratorName] : vData{ std::monostate{} };
        bool existed = m_variables.count(iteratorName);

        // 6. Execuția în funcție de tipul rezultat de evaluare

        std::vector<std::wstring> instructions = preParse(bodyCommand);

        
        if (sourceData.isArray() || sourceData.isMap()) {

            // Extragem o listă de vData pentru a itera uniform (sau iterăm direct)
            if (sourceData.isArray()) {
                const vDataArray& items = std::get<vDataArray>(sourceData.value);
                for (const auto& item : items) {
                    if (executeCycleStep(iteratorName, item, instructions)) break;
                }
            }
            else if (sourceData.isMap()) {
                const vDataMap& mapItems = std::get<vDataMap>(sourceData.value);
                for (const auto& pair : mapItems) {
                    // Trimitem cheia ca fiind valoarea iteratorului
                    if (executeCycleStep(iteratorName, vData{ pair.first }, instructions)) break;
                }
            }
        }
        /*
        if (sourceData.isArray()) {
            const vDataArray& items = std::get<vDataArray>(sourceData.value);
            for (const auto& item : items) {
                m_variables[iteratorName] = item;
                if (!bodyCommand.empty()) this->execute(bodyCommand);
            }
        }
        else if (sourceData.isMap()) {
            const vDataMap& mapItems = std::get<vDataMap>(sourceData.value);
            for (const auto& pair : mapItems) {
                m_variables[iteratorName] = vData{ pair.first }; // Cheia ca iterator
                if (!bodyCommand.empty()) this->execute(bodyCommand);
            }
        }
        else {
            LOG_ERROR(L"Cycle error: Evaluated source is not an array or a map.");
        }
        */
        // 7. Restaurare context
        if (existed) m_variables[iteratorName] = oldVal;
        else m_variables.erase(iteratorName);
    }
/*
  void vOliEngine::callProcedure(const Procedure& proc, const std::vector<std::wstring>& passedArgs) {
      //LOG_DEBUG(L"Calling procedure: " + proc.name);

      // Snapshot pentru Local Scope
      std::map<std::wstring, vData> globalContextBackup = m_variables;

      for (size_t i = 0; i < proc.params.size(); ++i) {
          if (i < passedArgs.size()) {
              // --- MODIFICARE AICI ---
              // În loc de vData(passedArgs[i]), folosim evaluateExpression
              // pentru ca [1,2,3] să devină ARRAY, nu textul "[1,2,3]"
              m_variables[proc.params[i]] = evaluateExpression(passedArgs[i]);
          }
          else {
              m_variables[proc.params[i]] = vData{ std::monostate{} }; // Null/Empty
          }
      }

      for (const auto& line : proc.body) {
          execute(line);
      }

      m_variables = globalContextBackup;
      LOG_DEBUG(L"Procedure " + proc.name + L" finished.");
  }
  */

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

/*
  void vOliEngine::handlePluginCommand(const ShellCommand& sc) {
      // 1. Verificăm dacă avem calea către DLL
      if (sc.args.empty()) {
          LOG_ERROR(L"Usage: /plugin \"path/to/plugin.dll\"");
          return;
      }

      // Luăm primul argument (calea). Parserul tău ar trebui să o curețe de ghilimele.
      std::wstring dllPath = sc.args[0];
      LOG_DEBUG(dllPath);
      // 2. Încărcăm DLL-ul (Windows API)
      HMODULE hLib = LoadLibraryW(dllPath.c_str());
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
        if (sc.args.empty()) {
            LOG_ERROR(L"Usage: /plugin \"path/to/plugin\"");
            return;
        }

        std::wstring pluginPath = sc.args[0];
        LOG_DEBUG(pluginPath);

#ifdef _WIN32
        // ---------------- WINDOWS ----------------
        HMODULE hLib = LoadLibraryW(pluginPath.c_str());
        if (!hLib) {
            DWORD lastError = GetLastError();
            LOG_ERROR(L"Could not load DLL: " + pluginPath +
                L" (Code: " + std::to_wstring(lastError) + L")");
            return;
        }

        typedef void (*RegisterFunc)(std::map<std::wstring, OliFunctionHandler>&);
        RegisterFunc regFunc = (RegisterFunc)GetProcAddress(hLib, "LoadOliPlugin");

        if (!regFunc) {
            LOG_ERROR(L"Invalid Plugin: Export 'LoadOliPlugin' not found in " + pluginPath);
            FreeLibrary(hLib);
            return;
        }

        regFunc(this->m_functionsHandlers);
        LOG_SUCCESS(L"Plugin loaded: " + pluginPath);

#else
        // ---------------- LINUX ----------------
        // Convertim calea la UTF-8
        std::string utf8Path = wstring_to_utf8(pluginPath);

        // Încărcăm .so
        void* handle = dlopen(utf8Path.c_str(), RTLD_NOW);
        if (!handle) {
            std::string err = dlerror();
            LOG_ERROR(L"Could not load plugin: " + pluginPath +
                L" (" + utf8_to_wstring(err) + L")");
            return;
        }

        // Semnătura funcției exportate
        typedef void (*RegisterFunc)(std::map<std::wstring, OliFunctionHandler>&);

        // Căutăm simbolul
        dlerror(); // resetăm starea
        RegisterFunc regFunc = (RegisterFunc)dlsym(handle, "LoadOliPlugin");

        const char* dlsymError = dlerror();
        if (dlsymError) {
            LOG_ERROR(L"Invalid Plugin: Export 'LoadOliPlugin' not found in " + pluginPath);
            dlclose(handle);
            return;
        }

        // Executăm funcția
        regFunc(this->m_functionsHandlers);
        LOG_SUCCESS(L"Plugin loaded: " + pluginPath);

#endif
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
          LOG_ERROR(L"Usage: /func name [param1, param2...]");
          return;
      }

      std::wstring funcName = sc.args[0];
      // Opțional: forțăm uppercase pentru a fi case-insensitive în expresii
      // std::transform(funcName.begin(), funcName.end(), funcName.begin(), ::towupper);

      m_activeFuncName = funcName;
      Procedure newFunc;
      newFunc.name = m_activeFuncName;

      // Extragem parametrii (curățăm parantezele sau virgulele dacă există)
      for (size_t i = 1; i < sc.args.size(); ++i) {
          std::wstring arg = sc.args[i];
          arg.erase(std::remove_if(arg.begin(), arg.end(), [](wchar_t c) {
              return c == L'[' || c == L']' || c == L',';
              }), arg.end());
          if (!arg.empty()) newFunc.params.push_back(arg);
      }

      m_userFunctions[m_activeFuncName] = newFunc;
      m_isRecordingFunc = true;

      LOG_INFO(L"Started recording function: " + m_activeFuncName);
  }

  /*
  vData vOliEngine::callUserFunction(const std::wstring& funcName, const std::vector<vData>& args) {
      auto it = m_userFunctions.find(funcName);
      if (it == m_userFunctions.end()) return vData{ std::monostate{} };

      const Procedure& func = it->second;

      // 1. Snapshot pentru protejarea variabilelor globale/exterioare
      std::map<std::wstring, vData> backup;
      for (const auto& p : func.params) {
          if (m_variables.count(p)) backup[p] = m_variables[p];
      }
      if (m_variables.count(L"return")) backup[L"return"] = m_variables[L"return"];

      // 2. Mapăm argumentele primite la parametrii funcției
      for (size_t i = 0; i < func.params.size(); ++i) {
          if (i < args.size()) {
              m_variables[func.params[i]] = args[i];
          }
          else {
              m_variables[func.params[i]] = vData{ std::monostate{} }; // Parametru lipsă
          }
      }

      // Inițializăm return cu 0 sau null
      m_variables[L"return"] = vData{ 0LL };

      // 3. Executăm liniile din funcție
      m_shouldReturn = false;
      for (const auto& line : func.body) {
          execute(line);
          if (m_shouldReturn) break;
      }
      m_shouldReturn = false;

      // 4. Salvăm rezultatul final
      vData result = m_variables[L"return"];

      // 5. Curățăm parametrii și restaurăm backup-ul
      //for (const auto& p : func.params) m_variables.erase(p);
      //for (auto const& [name, val] : backup) m_variables[name] = val;

      for (const auto& p : func.params) {
          auto itB = backup.find(p);
          if (itB != backup.end()) {
              m_variables[p] = itB->second;
          }
          else {
              m_variables.erase(p);
          }
      }
      // Restaurăm și variabila return dacă era în backup, altfel o curățăm
      if (backup.count(L"return")) m_variables[L"return"] = backup[L"return"];
      else m_variables.erase(L"return");

      return result;
  }
  */

  vData vOliEngine::callUserFunction(const std::wstring& funcName, const std::vector<vData>& args) {
      auto it = m_userFunctions.find(funcName);
      if (it == m_userFunctions.end()) return vData{ std::monostate{} };

      const Procedure& func = it->second;

      // --- MODIFICARE AICI: Facem un snapshot la TOATĂ memoria curentă ---
      // Sau, mai eficient, salvăm starea întregului map
      std::map<std::wstring, vData> oldVariables = m_variables;

      // 2. Mapăm argumentele primite la parametrii funcției
      // Atenție: lucrăm pe m_variables care acum conține tot, 
      // dar vom suprascrie doar ce e nevoie pentru contextul funcției
      for (size_t i = 0; i < func.params.size(); ++i) {
          if (i < args.size()) {
              m_variables[func.params[i]] = args[i];
          }
          else {
              m_variables[func.params[i]] = vData{ std::monostate{} };
          }
      }

      // Inițializăm return special (izolat)
      m_variables[L"return"] = vData{ 0LL };

      // 3. Executăm corpul funcției
      bool prevShouldReturn = m_shouldReturn; // Salvăm starea flag-ului (recursivitate)
      m_shouldReturn = false;

      for (const auto& line : func.body) {
          execute(line);
          if (m_shouldReturn) break;
      }

      // 4. Salvăm rezultatul
      vData result = m_variables[L"return"];

      // 5. RESTAURARE TOTALĂ
      // Ștergem tot ce a făcut funcția și punem înapoi ce era înainte
      m_variables = std::move(oldVariables);

      m_shouldReturn = prevShouldReturn;

      return result;
  }

  void vOliEngine::handleReturnCommand(const ShellCommand& sc) {
      // Dacă nu avem argumente (/return simplu), returnăm 0 sau null
      if (sc.args.empty()) {
          m_variables[L"return"] = vData{ 0LL };
          return;
      }

      // Reconstruim expresia din argumente
      // De exemplu, pentru "/return $a + 5", sc.args este ["$a", "+", "5"]
      std::wstring expression;
      for (size_t i = 0; i < sc.args.size(); ++i) {
          expression += sc.args[i];
          if (i < sc.args.size() - 1) expression += L" ";
      }

      try {
          // Evaluăm expresia folosind motorul tău de parsing existent
          vData result = evaluateExpression(expression);
          m_variables[L"return"] = result;

          // OPȚIONAL: Dacă vrei "Early Return" (să se oprească imediat funcția)
           m_shouldReturn = true; 

      }
      catch (const std::exception& e) {
          LOG_ERROR(L"Return error: " + std::wstring(e.what(), e.what() + strlen(e.what())));
      }
  }


  void vOliEngine::handleForCommand(const std::wstring& fullLine) {
      // Delimitatori obligatorii
      size_t posTo = findTopLevelKeyword(fullLine, L"/TO", L"/FOR");
      size_t posDo = findTopLevelKeyword(fullLine, L"/DO", L"/FOR");
      size_t posEnd = findTopLevelKeyword(fullLine, L"/ENDFOR", L"/FOR");

      if (posTo == std::wstring::npos || posDo == std::wstring::npos || posEnd == std::wstring::npos) {
          LOG_ERROR(L"Malformed /FOR: Missing /TO, /DO or /ENDFOR");
          return;
      }

      // Segmentul opțional /BY
      size_t posBy = findTopLevelKeyword(fullLine, L"/BY", L"/FOR");

      // 2. Extragem bucățile de cod
      // Inițializare: tot ce e între /FOR (6 caractere) și /TO
      std::wstring initPart = normalizeSpaces(fullLine.substr(4, posTo - 4)); // ex: "$i = 1"

      // Limita: între /TO și (/BY sau /DO)
      size_t limitEnd = (posBy != std::wstring::npos) ? posBy : posDo;
      std::wstring limitExpr = normalizeSpaces(fullLine.substr(posTo + 3, limitEnd - (posTo + 3)));

      // Pasul: între /BY și /DO (sau default "1")
      std::wstring stepExpr = L"1";
      if (posBy != std::wstring::npos) {
          stepExpr = normalizeSpaces(fullLine.substr(posBy + 3, posDo - (posBy + 3)));
      }

      // Corpul: între /DO și /ENDFOR
      std::wstring bodyCommand = fullLine.substr(posDo + 3, posEnd - (posDo + 3));
      std::vector<std::wstring> instructions = preParse(bodyCommand);

      // 3. Extragem numele variabilei de control (ex: din "$i = 1" luăm "$i")
      size_t eqPos = initPart.find(L'=');
      if (eqPos == std::wstring::npos) { LOG_ERROR(L"For init must be assignment: $var = val"); return; }

      std::wstring varRaw = trim(initPart.substr(0, eqPos)); // Aici e "$i"

      // --- FIX AICI ---
      // Dacă numele începe cu $, îl eliminăm pentru a obține cheia reală din map
      std::wstring varName = varRaw;
      if (!varName.empty() && varName[0] == L'$') {
          varName = varName.substr(1);
      }

      // --- EXECUȚIA ---

      // A. Initializare
      LOG_DEBUG(L"initPart :" + initPart);
      this->execute(L"/SET " + initPart);
      //this->handleSetCommand(initPart);
      
      LOG_DEBUG(L"-------------------------------------------------");
      int safetyBreak = 0;
      while (true) {
          // B. Verificăm Condiția (Control vs Limită)
          vData currentVal = resolveVariable(varName);
          vData limitVal = evaluateExpression(limitExpr);

          // Dacă i > limită, ne oprim (presupunem pas pozitiv pentru simplitate momentan)
          double step = vDataToDouble(evaluateExpression(stepExpr));
          double current = vDataToDouble(currentVal);
          double limit = vDataToDouble(limitVal);

          if (step >= 0 && current > limit) break;
          if (step < 0 && current < limit) break;

          // C. Executăm instrucțiunile
          for (const auto& instr : instructions) {
              this->execute(instr);

              if (m_executionStatus == OliStatus::BREAK_REQUESTED) {
                  m_executionStatus = OliStatus::RUNNING;
                  return;
              }
              if (m_executionStatus == OliStatus::CONTINUE_REQUESTED) {
                  m_executionStatus = OliStatus::RUNNING;
                  goto perform_step; // Sărim direct la incrementare
              }
          }

      perform_step:
          // Folosim o abordare care forțează evaluarea corectă a variabilei
          // Construim: /SET i = $i + 1 
          // (Stânga fără $, dreapta cu $ pentru a cere valoarea)

          std::wstring varNameClean = varName;
          if (!varNameClean.empty() && varNameClean[0] == L'$')
              varNameClean.erase(0, 1);

          std::wstring customSet = L"/SET " + varNameClean + L" = $" + varNameClean + L" + (" + stepExpr + L")";

          LOG_DEBUG(L"incrementPart :" + customSet);
          this->execute(customSet);
      }
  }