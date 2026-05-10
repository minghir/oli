
#include "vDataSerialize.hpp"
#include "OliEngine.hpp"
#include "PortTools.hpp"
#include "olic/OliBytecode.hpp"

#include <cmath>


void vOliEngine::executeBytecode(const OliChunk& chunk) {

    // Înregistrăm funcțiile acestui chunk în motor înainte de rulare
    for (auto const& [name, proc] : chunk.procedures) {
        this->m_bytecodeFunctions[name] = proc;
    }

    this->m_executionStatus = OliStatus::RUNNING;
    size_t ip = 0;
    std::vector<vData> stack;

    while (ip < chunk.code.size() && this->m_executionStatus == OliStatus::RUNNING) {
        OpCode instruction = static_cast<OpCode>(chunk.code[ip++]);

        switch (instruction) {
            // --- 1. MEMORIE & VARIABILE ---
        case OpCode::OP_CONSTANT: {
            uint16_t idx = (uint16_t)((chunk.code[ip] << 8) | chunk.code[ip + 1]);
            ip += 2;
            stack.push_back(chunk.constants[idx]);
            break;
        }

        case OpCode::OP_SET_GLOBAL: {
            vData val = stack.back(); stack.pop_back();
            uint16_t nameIdx = (uint16_t)((chunk.code[ip] << 8) | chunk.code[ip + 1]);
            ip += 2;
            vData nameConst = chunk.constants[nameIdx].getTrueData();
            this->setVar(nameConst.toWString(), val);
            break;
        }

        case OpCode::OP_GET_GLOBAL: {
            uint16_t nameIdx = (uint16_t)((chunk.code[ip] << 8) | chunk.code[ip + 1]);
            ip += 2;
            std::wstring rawName = chunk.constants[nameIdx].toWString();
            std::wstring cleanName = this->cleanVariableName(rawName);

            LOG_DEBUG(L"[VM] OP_GET_GLOBAL: Caut '" + rawName + L"' (Clean: '" + cleanName + L"')");

            vData val;
            bool found = false;

            if (!m_callStack.empty()) {
                auto& locals = m_callStack.back().localVariables;
                if (locals.count(cleanName)) {
                    val = locals[cleanName];
                    found = true;
                    LOG_DEBUG(L"   -> Gasit in LOCALS: " + val.toWString());
                }
            }

            if (!found) {
                val = this->getVar(rawName);
                LOG_DEBUG(L"   -> Gasit in GLOBALS: " + val.toWString());
            }

            stack.push_back(val);
            
            break;
        }

        case OpCode::OP_GET_ADDR: {
            uint16_t nameIdx = (uint16_t)((chunk.code[ip] << 8) | chunk.code[ip + 1]);
            ip += 2;
            std::wstring cleanName = this->cleanVariableName(chunk.constants[nameIdx].toWString());
            vData* targetPtr = nullptr;
            if (!m_callStack.empty()) {
                auto& locals = m_callStack.back().localVariables;
                if (locals.count(cleanName)) targetPtr = &locals[cleanName];
            }
            if (!targetPtr) targetPtr = &m_globalVariables[cleanName];
            vData addr; addr.value = targetPtr;
            stack.push_back(addr);
            break;
        }
        /*
        case OpCode::OP_ADD: {
            vData b = stack.back(); stack.pop_back();
            vData a = stack.back(); stack.pop_back();

            vData rA = a.getScalarValue().getTrueData();
            vData rB = b.getScalarValue().getTrueData();

            // Dacă unul e string, facem concatenare string
            if (rA.isString() || rB.isString()) {
                stack.push_back(vData(rA.toWString() + rB.toWString()));
            }
            else if (rA.isInt() && rB.isInt()) {
                stack.push_back(vData(rA.toInt() + rB.toInt()));
            }
            else {
                stack.push_back(vData(vDataToDouble(rA) + vDataToDouble(rB)));
            }
            break;
        }
        */
        case OpCode::OP_ADD: {
            vData b = stack.back(); stack.pop_back();
            vData a = stack.back(); stack.pop_back();

            // Folosim getTrueData() pentru a rezolva pointerii, 
            // dar NU getScalarValue() care ar sapa in Map-uri.
            vData rA = a.getTrueData();
            vData rB = b.getTrueData();

            if (rA.isString() || rB.isString()) {
                stack.push_back(vData(rA.toWString() + rB.toWString()));
            }
            else if (rA.isInt() && rB.isInt()) {
                stack.push_back(vData(rA.toInt() + rB.toInt()));
            }
            else {
                stack.push_back(vData(vDataToDouble(rA) + vDataToDouble(rB)));
            }
            break;
        }

        case OpCode::OP_SUB: {
            vData b = stack.back(); stack.pop_back();
            vData a = stack.back(); stack.pop_back();
            if (a.isInt() && b.isInt()) stack.push_back(vData(std::get<long long>(a.value) - std::get<long long>(b.value)));
            else stack.push_back(vData(vDataToDouble(a) - vDataToDouble(b)));
            break;
        }

        case OpCode::OP_MUL: {
            vData b = stack.back(); stack.pop_back();
            vData a = stack.back(); stack.pop_back();
            stack.push_back(vData(vDataToDouble(a) * vDataToDouble(b)));
            break;
        }

        case OpCode::OP_DIV: {
            vData b = stack.back(); stack.pop_back();
            vData a = stack.back(); stack.pop_back();
            double valB = vDataToDouble(b);
            if (valB == 0) { this->logError(L"Runtime Error: Div by zero!"); this->m_executionStatus = OliStatus::ERR; return; }
            stack.push_back(vData(vDataToDouble(a) / valB));
            break;
        }

                           // --- 3. BITWISE & LOGICĂ ---
        case OpCode::OP_BAND: { vData b = stack.back(); stack.pop_back(); vData a = stack.back(); stack.pop_back(); stack.push_back(vData(a.getTrueData().toInt() & b.getTrueData().toInt())); break; }
        case OpCode::OP_BOR: { vData b = stack.back(); stack.pop_back(); vData a = stack.back(); stack.pop_back(); stack.push_back(vData(a.getTrueData().toInt() | b.getTrueData().toInt())); break; }
        case OpCode::OP_LOGICAL_NOT: { vData a = stack.back(); stack.pop_back(); stack.push_back(vData(!vDataToBool(a))); break; }

                                   // --- 4. COMPARAȚIE ---
        case OpCode::OP_EQUAL: {
            vData b = stack.back(); stack.pop_back();
            vData a = stack.back(); stack.pop_back();
            stack.push_back(vData(a == b));
            break;
        }
        case OpCode::OP_GREATER: {
            vData b = stack.back(); stack.pop_back();
            vData a = stack.back(); stack.pop_back();
            stack.push_back(vData(vDataToDouble(a) > vDataToDouble(b)));
            break;
        }
        case OpCode::OP_LESS: {
            vData b = stack.back(); stack.pop_back();
            vData a = stack.back(); stack.pop_back();
            stack.push_back(vData(vDataToDouble(a) < vDataToDouble(b)));
            break;
        }

                            // --- 5. CONTROL FLOW ---
        case OpCode::OP_JUMP: {
            uint16_t offset = (uint16_t)((chunk.code[ip] << 8) | chunk.code[ip + 1]);
            ip += 2 + offset; break;
        }
        case OpCode::OP_JUMP_IF_FALSE: {
            if (stack.empty()) { throw std::runtime_error("Stack underflow at JUMP_IF_FALSE"); }
            uint16_t offset = (uint16_t)((chunk.code[ip] << 8) | chunk.code[ip + 1]);
            ip += 2; if (!vDataToBool(stack.back())) ip += offset;
            stack.pop_back(); break;
        }
        case OpCode::OP_JUMP_IF_TRUE: {
            if (stack.empty()) { throw std::runtime_error("Stack underflow at JUMP_IF_TRUE"); }
            // 1. Citim offset-ul de 2 bytes
            uint16_t offset = (uint16_t)((chunk.code[ip] << 8) | chunk.code[ip + 1]);
            ip += 2;

            // 2. Sărim doar dacă valoarea de pe stivă este TRUE
            if (vDataToBool(stack.back())) ip += offset;

            // 3. Curățăm stiva și ieșim
            stack.pop_back();
            break;
        }
        case OpCode::OP_LOOP: {
            uint16_t offset = (uint16_t)((chunk.code[ip] << 8) | chunk.code[ip + 1]);
            ip += 2; ip -= offset; break;
        }

                            // --- 6. STRUCTURI DE DATE (CRITIC) ---
        case OpCode::OP_ARRAY: {
            uint8_t count = chunk.code[ip++];
            vDataArray elements = std::make_shared<std::vector<vData>>(count);
            for (int i = (int)count - 1; i >= 0; --i) {
                (*elements)[i] = stack.back(); stack.pop_back();
            }
            stack.push_back(vData(elements));
            break;
        }

        case OpCode::OP_MAP: {
            uint8_t pairCount = chunk.code[ip++];
            vDataMap mapObj = std::make_shared<std::unordered_map<std::wstring, vData>>();
            for (int i = 0; i < (int)pairCount; ++i) {
                vData val = stack.back(); stack.pop_back();
                vData key = stack.back(); stack.pop_back();
                (*mapObj)[key.getTrueData().toWString()] = val;
            }
            stack.push_back(vData(mapObj));
            break;
        }

        
        /*
        case OpCode::OP_GET_INDIRECT: {
            if (stack.size() < 1) break;

            // 1. Scoatem indexul (sau pointerul)
            vData indexOrPtr = stack.back();
            stack.pop_back();

            // 2. Verificăm dacă avem un container sub el pe stivă
            // Exemplu: [Array, Index] -> OP_GET_INDIRECT
            if (!stack.empty() && (stack.back().isArray() || stack.back().isMap())) {
                vData container = stack.back();
                stack.pop_back();

                if (container.isArray()) {
                    auto* arr = container.rawArray();
                    int idx = (int)indexOrPtr.toInt();
                    if (arr && idx >= 0 && idx < (int)arr->size()) {
                        stack.push_back((*arr)[idx]);
                    }
                    else {
                        stack.push_back(vData{ std::monostate{} }); // null dacă e out of bounds
                    }
                }
                else if (container.isMap()) {
                    auto* m = container.rawMap();
                    std::wstring key = indexOrPtr.toWString();
                    if (m && m->count(key)) {
                        stack.push_back((*m)[key]);
                    }
                    else {
                        stack.push_back(vData{ std::monostate{} });
                    }
                }
            }
            // 3. Altfel, este o dereferențiere simplă (*$ptr)
            else {
                // Folosim getScalarValue() din vData.hpp pentru a trece prin ierarhii
                stack.push_back(indexOrPtr.getScalarValue());
            }
            break;
        }
        */

        case OpCode::OP_GET_INDIRECT: {
            if (stack.size() < 1) break;

            vData indexOrPtr = stack.back();
            stack.pop_back();

            // 1. Cazul ACCES CONTAINER: [Container, Index]
            if (!stack.empty() && (stack.back().isArray() || stack.back().isMap())) {
                vData container = stack.back();
                stack.pop_back();

                if (container.isArray()) {
                    auto* arr = container.rawArray();
                    long long idx = indexOrPtr.toInt();
                    if (arr && idx >= 0 && idx < (long long)arr->size()) {
                        stack.push_back((*arr)[idx]);
                    }
                    else {
                        stack.push_back(vData{ std::monostate{} });
                    }
                }
                else if (container.isMap()) {
                    auto* m = container.rawMap();
                    std::wstring key = indexOrPtr.toWString();
                    if (m && m->count(key)) {
                        stack.push_back((*m)[key]);
                    }
                    else {
                        stack.push_back(vData{ std::monostate{} });
                    }
                }
            }
            // 2. Cazul DEREFERENȚIERE POINTER (*$ptr)
            else {
                // Folosim DOAR getTrueData(). Dacă e pointer, obținem spre ce arată.
                // Dacă e un Map cu un element, RĂMÂNE un Map cu un element.
                stack.push_back(indexOrPtr.getTrueData());
            }
            break;
        }

        case OpCode::OP_SET_INDIRECT: {
            if (stack.size() < 3) {
                LOG_ERROR(L"OP_SET_INDIRECT Error: Stack underflow (nevoie de 3 elemente, are " + std::to_wstring(stack.size()) + L")");
                break;
            }
            LOG_DEBUG(L"VM_DEBUG: SET_INDIRECT pe " + stack[stack.size() - 3].toWString());
            // 1. Extragem argumentele în ordinea LIFO (Last In, First Out)
            // Stiva la intrare: [Container, Index, Value] <- top
            vData value = stack.back(); stack.pop_back();
            vData index = stack.back(); stack.pop_back();
            vData container = stack.back(); stack.pop_back();

            // 2. Operăm pe Map (folosim rawMap() care face automat getTrueData())
            LOG_DEBUG(L"VM: Executing SET_INDIRECT. Container Type: " + getVariantTypeName( container));
            if (container.isMap()) {
                auto* m = container.rawMap();
                if (m) {
                    // Conversia indexului la string este vitală pentru cheile de Map
                    (*m)[index.toWString()] = value;
                }
            }
            // 3. Operăm pe Array
            else if (container.isArray()) {
                auto* arr = container.rawArray();
                long long idx = index.toInt();
                if (arr && idx >= 0 && idx < (long long)arr->size()) {
                    (*arr)[(size_t)idx] = value;
                }
                else {
                    LOG_ERROR(L"OP_SET_INDIRECT Error: Index out of bounds: " + std::to_wstring(idx));
                }
            }
            // 4. Cazul în care containerul a fost corupt (devenit string sau null)
            else {
                LOG_ERROR(L"OP_SET_INDIRECT Error: Obiectul nu este Map/Array! Tip actual: " + container.toWString());
                LOG_ERROR(L"DEZASTRU: Containerul a ajuns pe stiva ca fiind de tip: " + getVariantTypeName(container));
            }
            break;
        }
        // --- 8. SYSTEM & STRING ---
        /*
        case OpCode::OP_ECHO: {
            if (stack.empty()) break;
            vData val = stack.back();
            stack.pop_back();

            // Folosim getScalarValue() pentru a „aplatiza” ierarhia înainte de afișare
            //std::wcout << val.getScalarValue().toWString() << std::endl;
            //std::wcout.flush();
            LOG_RAW(val.getScalarValue().toWString());
            break;
        }
        */
        case OpCode::OP_ECHO: {
            if (stack.empty()) break;
            vData val = stack.back();
            stack.pop_back();

            // Aici e singurul loc unde getScalarValue() este binevenit, 
            // pentru că doar afișăm, nu modificăm flow-ul de date.
            LOG_RAW(val.getScalarValue().toWString());
            break;
        }
        case OpCode::OP_CONCAT: {
            vData b = stack.back(); stack.pop_back();
            vData a = stack.back(); stack.pop_back();
            stack.push_back(vData(a.toWString() + b.toWString()));
            break;
        }

        //case OpCode::OP_RETURN:
        //    this->m_executionStatus = OliStatus::RETURN_REQUESTED; return;

        case OpCode::OP_DUP: stack.push_back(stack.back()); break;
        case OpCode::OP_POP: if (!stack.empty()) stack.pop_back(); break;
        case OpCode::OP_UNSET: {
            uint16_t nameIdx = (uint16_t)((chunk.code[ip] << 8) | chunk.code[ip + 1]);
            ip += 2;

            std::wstring fullPath = chunk.constants[nameIdx].toWString();
            if (fullPath.empty()) break;

            // Verificăm atomic "all"
            std::wstring checkAll = fullPath;
            if (checkAll[0] == L'$' || checkAll[0] == L'@') checkAll.erase(0, 1);

            if (checkAll == L"all") {
                m_globalVariables.clear();
                break;
            }

            // Folosim noua funcție dedicată VM-ului
            auto path = parsePath(fullPath);
            bool forceGlobal = (fullPath[0] == L'@');

            if (path.indexes.empty()) {
                // Ștergere variabilă rădăcină
                if (forceGlobal) m_globalVariables.erase(path.rootName);
                else {
                    bool del = false;
                    if (!m_callStack.empty()) del = (m_callStack.back().localVariables.erase(path.rootName) > 0);
                    if (!del) m_globalVariables.erase(path.rootName);
                }
            }
            else {
                // Ștergere din container folosind resolveVMPath
                vData* parent = resolveVMPath(path.rootName, path.indexes, forceGlobal);
                if (parent) {
                    std::wstring lastKey = path.indexes.back();
                    if (lastKey.size() >= 2 && lastKey.front() == L'\"' && lastKey.back() == L'\"') {
                        lastKey = lastKey.substr(1, lastKey.size() - 2);
                    }

                    if (parent->isMap()) {
                        auto& m = std::get<vDataMap>(parent->value);
                        if (m) m->erase(lastKey);
                    }
                    else if (parent->isArray()) {
                        auto& v = std::get<vDataArray>(parent->value);
                        try {
                            size_t idx = static_cast<size_t>(std::stoll(lastKey));
                            if (v && idx < v->size()) v->erase(v->begin() + idx);
                        }
                        catch (...) {}
                    }
                }
            }
            break;
        }
        case OpCode::OP_CALL_NATIVE: {
            // 1. Citim metadatele
            uint16_t nameIdx = (uint16_t)((chunk.code[ip] << 8) | chunk.code[ip + 1]);
            ip += 2;
            uint8_t argCount = chunk.code[ip++];

            std::wstring rawName = chunk.constants[nameIdx].toWString();
            std::wstring funcName = rawName;

            // --- FIX CRITIC: DECOJIRE IERARHIE PENTRU APEL DINAMIC ---
            if (!funcName.empty() && (funcName[0] == L'$' || funcName[0] == L'@')) {
                // Numele curat pentru a căuta în interiorul Map-ului (ex: "funcName")
                std::wstring cleanVarName = funcName.substr(1);
                vData varContent = this->getVar(rawName);

                if (varContent.isMap()) {
                    auto m = std::get<vDataMap>(varContent.value);
                    // Dacă găsim cheia în ierarhie, extragem string-ul real ("RANDOM")
                    if (m && m->count(cleanVarName)) {
                        funcName = (*m)[cleanVarName].toWString();
                    }
                    else {
                        funcName = varContent.getTrueData().toWString();
                    }
                }
                else {
                    funcName = varContent.toWString();
                }
            }

            // 2. Normalizăm numele pentru lookup (ex: "random" -> "RANDOM")
            std::wstring upperName = funcName;
            for (auto& c : upperName) c = std::towupper(c);

            // 3. Colectăm argumentele (LIFO)
            std::vector<vData> args(argCount);
            for (int i = argCount - 1; i >= 0; --i) {
                if (!stack.empty()) {
                    args[i] = stack.back().getTrueData();
                    stack.pop_back();
                }
            }

            // 4. Executăm handler-ul nativ
            auto it = m_functionsHandlers.find(upperName);
            if (it != m_functionsHandlers.end()) {
                vData result = it->second(args);
                stack.push_back(result);
            }
            else {
                this->logError(L"Runtime Error: Functia '" + funcName + L"' nu a fost gasita.");
                this->m_executionStatus = OliStatus::ERR;
                return;
            }
            break;
        }
        case OpCode::OP_PLUGIN: {
            // Citim indexul din cod
            uint16_t pathIdx = (uint16_t)((chunk.code[ip] << 8) | chunk.code[ip + 1]);
            ip += 2;

            // Luăm calea din constante
            std::wstring pluginPath = chunk.constants[pathIdx].toWString();

            // Încărcăm plugin-ul folosind noua metodă internă
            if (!this->internalLoadPlugin(pluginPath)) {
                this->logError(L"VM Error: Failed to load plugin required by bytecode: " + pluginPath);
                this->m_executionStatus = OliStatus::ERR;
                return;
            }
            break;
        }
        case OpCode::OP_NOT_EQUAL: {
            vData b = stack.back(); stack.pop_back();
            vData a = stack.back(); stack.pop_back();
            stack.push_back(vData(!(a == b)));
            break;
        }
        case OpCode::OP_GREATER_EQUAL: {
            vData b = stack.back(); stack.pop_back();
            vData a = stack.back(); stack.pop_back();
            stack.push_back(vData(vDataToDouble(a) >= vDataToDouble(b)));
            break;
        }
        case OpCode::OP_LESS_EQUAL: { // <--- REPARAȚIA PENTRU EROAREA 0x32
            vData b = stack.back(); stack.pop_back();
            vData a = stack.back(); stack.pop_back();
            stack.push_back(vData(vDataToDouble(a) <= vDataToDouble(b)));
            break;
        }
        case OpCode::OP_MOD: {
            vData b = stack.back(); stack.pop_back();
            vData a = stack.back(); stack.pop_back();
            stack.push_back(vData((long long)vDataToDouble(a) % (long long)vDataToDouble(b)));
            break;
        }
        case OpCode::OP_POW: {
            vData b = stack.back(); stack.pop_back();
            vData a = stack.back(); stack.pop_back();
            stack.push_back(vData(std::pow(vDataToDouble(a), vDataToDouble(b))));
            break;
        }

        case OpCode::OP_NEGATE: {
            vData a = stack.back(); stack.pop_back();
            stack.push_back(vData(-vDataToDouble(a)));
            break;
        }
        case OpCode::OP_RETURN: {
            if (!m_callStack.empty()) {
                // Dacă există o valoare calculată pe stivă, ea este rezultatul!
                if (!stack.empty()) {
                    m_callStack.back().localVariables[L"return"] = stack.back();
                    stack.pop_back();
                }
            }
            // Semnalăm că execuția acestui chunk s-a terminat
            m_executionStatus = OliStatus::RETURN_REQUESTED;
            return;
        }
                              // În switch(op) din executeBytecode:
        case OpCode::OP_CALL: {
            uint16_t nameIdx = (uint16_t)((chunk.code[ip] << 8) | chunk.code[ip + 1]);
            ip += 2;
            uint8_t argCount = chunk.code[ip++];

            std::wstring funcName = to_upper(chunk.constants[nameIdx].toWString());

            // Colectăm argumentele de pe stivă
            std::vector<vData> callArgs(argCount);
            for (int i = argCount - 1; i >= 0; --i) {
                callArgs[i] = stack.back();
                stack.pop_back();
            }

            // Executăm funcția de bytecode
            vData result = this->callUserByteCodeFunction(funcName, callArgs, vData());

            // Punem rezultatul înapoi pe stivă
            stack.push_back(result);
            break;
        }

        case OpCode::OP_TYPE: {
            if (stack.empty()) break;
            vData val = stack.back();
            stack.pop_back();

            std::wstring typeStr = L"UNKNOWN";

            // Verificăm dacă este pointer ÎNAINTE de dereferențiere 
            // dacă vrem să știm că e o referință, sau DUPĂ dacă vrem tipul țintei.
            if (val.isPointer()) {
                typeStr = L"POINTER";
            }
            else {
                vData actual = val.getTrueData();
                if (actual.isInt())      typeStr = L"INT";
                if (actual.isFloat())    typeStr = L"FLOAT";
                if (actual.isBool())     typeStr = L"BOOL";
                else if (actual.isString()) typeStr = L"STRING";
                else if (actual.isArray())  typeStr = L"ARRAY";
                else if (actual.isMap())    typeStr = L"MAP";
                else if (actual.isNull())   typeStr = L"NULL";
            }

            stack.push_back(vData(typeStr));
            break;
        }
        case OpCode::OP_ITER_START: {
            // Stiva are sursa (Array/Map/String). 
            // Mai adăugăm un index de pornire (0).
            stack.push_back(vData(0LL));
            break;
        }

        case OpCode::OP_ITER_NEXT: {
            // Calculăm pozițiile
            size_t idxPos = stack.size() - 1;
            size_t srcPos = stack.size() - 2;

            // EXTRAGEM VALORILE PRIN COPIE (sau accesăm direct via index mai târziu)
            long long idx = std::get<long long>(stack[idxPos].value);

            // Nu folosim referință la srcData dacă urmează push_back!
            vData nextValue;
            bool isDone = true;

            // Accesăm sursa în siguranță (înainte de push_back)
            if (stack[srcPos].isArray()) {
                auto& arr = *std::get<vDataArray>(stack[srcPos].value);
                if (idx >= 0 && idx < (long long)arr.size()) {
                    nextValue = arr[idx];
                    isDone = false;
                }
            }
            else if (stack[srcPos].isMap()) {
                auto& m = *std::get<vDataMap>(stack[srcPos].value);
                if (idx >= 0 && idx < (long long)m.size()) {
                    auto it = m.begin();
                    std::advance(it, idx);
                    nextValue = vData(it->first);
                    isDone = false;
                }
            }
            else if (stack[srcPos].isString()) {
                const std::wstring& str = std::get<std::wstring>(stack[srcPos].value);
                if (idx >= 0 && idx < (long long)str.size()) {
                    nextValue = vData(std::wstring(1, str[idx]));
                    isDone = false;
                }
            }

            // Actualizăm indexul numeric
            stack[idxPos].value = idx + 1;

            // PUNEM REZULTATELE PE STIVĂ
            if (!isDone) {
                stack.push_back(nextValue); // Valoarea pentru iterator ($cat/$tip)
            }
            stack.push_back(vData(isDone)); // Flag-ul pentru JUMP_IF_TRUE
            break;
        }

        case OpCode::OP_ITER_FREE: {
            // Scoatem Indexul și Sursa de pe stivă la finalul buclei
            stack.pop_back();
            stack.pop_back();
            break;
        }
        case OpCode::OP_DEF_TYPE: {
            // 1. Citim numele tipului
            uint16_t nameIdx = (uint16_t)((chunk.code[ip] << 8) | chunk.code[ip + 1]);
            ip += 2;
            std::wstring typeName = chunk.constants[nameIdx].toWString();

            // 2. Citim restul metadatelor
            bool isClass = (chunk.code[ip++] == 1);
            uint8_t fieldCount = chunk.code[ip++];

            vTypeBlueprint bp;
            bp.name = typeName;
            bp.isClass = isClass;

            // 3. Citim indexul fiecărui câmp
            for (int i = 0; i < (int)fieldCount; ++i) {
                uint16_t fIdx = (uint16_t)((chunk.code[ip] << 8) | chunk.code[ip + 1]);
                ip += 2;
                bp.fields.push_back(chunk.constants[fIdx].toWString());
            }

            // 4. Înregistrăm în Motor
            this->m_blueprints[typeName] = bp;
            LOG_SUCCESS(L"[VM] Blueprint registered: " + typeName);
            break;
        }
        case OpCode::OP_CALL_METHOD: {
            // 1. Citim numărul de argumente (1 byte conform noului format de apel dinamic)
            uint8_t argCount = chunk.code[ip++];

            // 2. Extragem Numele Metodei de pe stivă (pus de OP_GET_INDIRECT anterior)
            vData funcNameData = stack.back();
            stack.pop_back();
            std::wstring funcName = to_upper(funcNameData.toWString());

            // 3. Extragem Obiectul Context ($p) care va deveni $this
            // Acesta a fost păstrat pe stivă prin OP_DUP înainte de a extrage numele metodei
            vData contextObj = stack.back();
            stack.pop_back();

            // 4. Colectăm argumentele (LIFO)
            std::vector<vData> args(argCount);
            for (int i = argCount - 1; i >= 0; --i) {
                if (!stack.empty()) {
                    args[i] = stack.back().getTrueData();
                    stack.pop_back();
                }
            }

            LOG_DEBUG(L"[VM] Apel Metoda: " + funcName + L" pe obiect de tip " + getVariantTypeName(contextObj));

            // 5. Executăm apelul căutând în ambele tabele
            if (m_bytecodeFunctions.count(funcName)) {
                // --- APEL USER FUNCTION (BYTECODE) ---
                // Transmitem contextObj care va fi mapat la $this în callUserByteCodeFunction
                vData result = this->callUserByteCodeFunction(funcName, args, contextObj);
                stack.push_back(result);
            }
            else if (m_functionsHandlers.count(funcName)) {
                // --- APEL NATIVE (C++) ---
                // Notă: Dacă vrei ca și funcțiile native să vadă 'this', 
                // ar trebui să modifici signatura OliFunctionHandler să accepte context.
                vData result = m_functionsHandlers[funcName](args);
                stack.push_back(result);
            }
            else {
                this->logError(L"Runtime Error: Metoda '" + funcName + L"' nu a fost găsită.");
                this->m_executionStatus = OliStatus::ERR;
                return;
            }
            break;
        }

        default:
            this->logError(L"VM Error: OpCode necunoscut [0x" + std::to_wstring((int)instruction) + L"] la IP: " + std::to_wstring(ip - 1));
            this->m_executionStatus = OliStatus::ERR; return;
        }

        if (this->m_executionStatus != OliStatus::RUNNING) {
            if (this->m_executionStatus == OliStatus::ERR) return;
        }
    }
}

void vOliEngine::loadAndRunBytecode(const std::string& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs.is_open()) {
        this->logError(L"Could not open bytecode file: " + str_to_wstr(path));
        return;
    }

    OliChunk mainChunk;

    // --- REPARAȚIA CRITICĂ ---
    // Apelăm funcția care știe să citească recursiv Constante + Cod + Proceduri
    vDataSerialize::deserializeChunkToEngine(ifs, mainChunk, this);

    ifs.close();

    // Resetăm motorul și executăm Main
    this->m_executionStatus = OliStatus::RUNNING;
    this->executeBytecode(mainChunk);
}


vData* vOliEngine::resolveVMPath(const std::wstring& rootName, const std::vector<std::wstring>& indexes, bool forceGlobal) {
    // 1. Curățăm numele rădăcinii
    std::wstring cleanRoot = rootName;
    if (!cleanRoot.empty() && (cleanRoot[0] == L'$' || cleanRoot[0] == L'@')) {
        cleanRoot = cleanRoot.substr(1);
    }

    vData* current = nullptr;

    // 2. Găsim variabila de bază (Scoping)
    if (forceGlobal) {
        if (m_globalVariables.count(cleanRoot)) current = &m_globalVariables[cleanRoot];
    }
    else {
        if (!m_callStack.empty() && m_callStack.back().localVariables.count(cleanRoot)) {
            current = &m_callStack.back().localVariables[cleanRoot];
        }
        else if (m_globalVariables.count(cleanRoot)) {
            current = &m_globalVariables[cleanRoot];
        }
    }

    if (!current) return nullptr;

    // 3. --- LOGICA DE IERARHIE (Boxing Peeling) ---
    // Dacă variabila este un Map care conține cheia cu propriul nume, "decojim" ierarhia.
    if (current->isMap()) {
        auto m = std::get<vDataMap>(current->value);
        if (m && m->count(cleanRoot)) {
            current = &((*m)[cleanRoot]);
        }
    }

    // 4. Navigăm prin indexuri (ne oprim înainte de ultimul)
    for (size_t i = 0; i < indexes.size() - 1; ++i) {
        std::wstring idx = indexes[i];
        // Curățare ghilimele pentru chei de map
        if (idx.size() >= 2 && idx.front() == L'\"' && idx.back() == L'\"') {
            idx = idx.substr(1, idx.size() - 2);
        }

        if (current->isMap()) {
            auto& m = std::get<vDataMap>(current->value);
            if (m && m->count(idx)) current = &((*m)[idx]);
            else return nullptr;
        }
        else if (current->isArray()) {
            auto& v = std::get<vDataArray>(current->value);
            try {
                size_t nIdx = static_cast<size_t>(std::stoll(idx));
                if (v && nIdx < v->size()) current = &((*v)[nIdx]);
                else return nullptr;
            }
            catch (...) { return nullptr; }
        }
        else return nullptr;
    }

    return current;
}
/*
bool vOliEngine::internalLoadPlugin(std::wstring pluginName) {
    if (pluginName.empty()) return false;

    // 1. Curățăm ghilimelele
    if (pluginName.size() >= 2 && pluginName.front() == L'"' && pluginName.back() == L'"') {
        pluginName = pluginName.substr(1, pluginName.size() - 2);
    }

    // 2. Determinăm calea finală (Logica ta de Path)
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
        return false;
    }

    bool loadedAnything = false;

    // --- A. Încărcare FUNCȚII (NATIVE CALLS) ---
   
    typedef void (*LoadFunctionsFunc)(std::unordered_map<std::wstring, OliFunctionHandler>&, void*);
    LoadFunctionsFunc regFuncs = (LoadFunctionsFunc)PortTools::getFunctionSymbol(hLib, "LoadOliFunctionPlugin");

    if (regFuncs) {
        std::unordered_map<std::wstring, OliFunctionHandler> dummyFuncs;
        try {
            // Trimitem nullptr pentru context, compilatorul vrea doar cheile (numele)
            regFuncs(dummyFuncs, nullptr);
            for (auto const& [name, handler] : dummyFuncs) {
                if (!name.empty()) {
                    vOliKeyWords::registerNativeFunction(name);
                    LOG_DEBUG(L"Compiler recognized native function: " + name);
                }
            }
            
        }
        catch (...) {
            LOG_ERROR(L"Failed to extract functions from plugin metadata.");
        }
        loadedAnything = true;
    }

    // --- B. Încărcare COMENZI (Sistem Nou) ---
    LoadCommandsFunc regCmds = (LoadCommandsFunc)PortTools::getFunctionSymbol(hLib, "LoadOliCommandPlugin");

    if (regCmds) {
        regCmds(this->m_commandHandlers, this);

        // Înregistrăm noile chei în vOliKeyWords pentru a fi recunoscute de parser
        for (auto const& [name, handler] : this->m_commandHandlers) {
            vOliKeyWords::registerDynamicCommand(name);
        }

        LOG_SUCCESS(L"Commands injected and registered: " + dllPath);
        loadedAnything = true;
    }

    // 5. Finalizare
    if (loadedAnything) {
        LOG_SUCCESS(L"Plugin '" + pluginName + L"' is fully operational.");
        return true;
    }
    else {
        LOG_ERROR(L"Invalid Plugin: No entry points found in " + dllPath);
        PortTools::freeDynamicLibrary(hLib);
        return false;
    }
}
*/

bool vOliEngine::internalLoadPlugin(std::wstring pluginName) {
    if (pluginName.empty()) return false;

    // 1. Curățare ghilimele
    if (pluginName.size() >= 2 && pluginName.front() == L'"' && pluginName.back() == L'"') {
        pluginName = pluginName.substr(1, pluginName.size() - 2);
    }

    // 2. Construire cale DLL
    std::wstring dllPath = (pluginName.find(L'/') == std::wstring::npos && pluginName.find(L'\\') == std::wstring::npos)
        ? m_pluginsPath + pluginName
        : pluginName;

    std::wstring ext = PortTools::getPluginExtension();
    if (dllPath.size() < ext.size() || dllPath.substr(dllPath.size() - ext.size()) != ext) {
        dllPath += ext;
    }

    // 3. Încărcare bibliotecă
    PortTools::LibHandle hLib = PortTools::loadDynamicLibrary(dllPath);
    if (!hLib) {
        LOG_ERROR(L"Could not load plugin: " + dllPath + L" (Error: " + PortTools::getLastErrorString() + L")");
        return false;
    }

    bool loadedAnything = false;

    // --- A. ÎNCĂRCARE FUNCȚII (NATIVE CALLS) ---
    
    typedef void (*LoadFunctionsFunc)(std::unordered_map<std::wstring, OliFunctionHandler>&, void*);
    LoadFunctionsFunc regFuncs = (LoadFunctionsFunc)PortTools::getFunctionSymbol(hLib, "LoadOliPlugin");

    if (regFuncs) {
        // 1. Map temporar pentru a izola funcțiile din acest plugin
        std::unordered_map<std::wstring, OliFunctionHandler> pluginFuncs;
        try {
            regFuncs(pluginFuncs, this); // Plugin-ul umple map-ul temporar

            // 2. Normalizăm și mutăm în map-ul principal al motorului
            for (auto const& [name, handler] : pluginFuncs) {
                std::wstring upName = name;
                for (auto& c : upName) c = std::towupper(c);

                // Acum înregistrăm în VM cu numele normalizat
                this->m_functionsHandlers[upName] = handler;

                // Spunem și parserului/compilatorului din VM că e funcție nativă
                vOliKeyWords::registerNativeFunction(upName);

                LOG_DEBUG(L"Injected function: " + upName);
            }
            loadedAnything = true;
            LOG_SUCCESS(L"Functions injected from: " + dllPath);
        }
        catch (...) { LOG_ERROR(L"Exception in LoadOliPlugin"); }
    }

    // --- B. ÎNCĂRCARE COMENZI (CU ADAPTARE LA std::function<void(const std::wstring&)>) ---
    typedef void (*LoadCommandsFunc)(std::unordered_map<std::wstring, OliCommandHandler>&, void*);
    LoadCommandsFunc regCmds = (LoadCommandsFunc)PortTools::getFunctionSymbol(hLib, "LoadOliCommandPlugin");

    if (regCmds) {
        // Map temporar pentru ce vrea plugin-ul (OliCommandHandler lucrează cu ShellCommand)
        std::unordered_map<std::wstring, OliCommandHandler> pluginCmds;
        try {
            regCmds(pluginCmds, this);

            for (auto const& [name, handler] : pluginCmds) {
                // ADAPTOR: Învelim comanda din plugin (ShellCommand) în formatul clasei tale (wstring)
                this->m_commandHandlers[name] = [handler, name](const std::wstring& line) {
                    ShellCommand cmd;
                    cmd.name = name; // Numele comenzii înregistrate
                    cmd.isValid = true;

                    // Parsare simplă a argumentelor (split by space)
                    // Dacă ai deja o funcție de split în motor, folosește-o pe aceea
                    std::wstringstream ss(line);
                    std::wstring arg;
                    while (ss >> arg) {
                        cmd.args.push_back(arg);
                    }

                    // Apelăm handler-ul din plugin cu obiectul populat
                    handler(cmd);
                    };

                vOliKeyWords::registerDynamicCommand(name);
            }
            loadedAnything = true;
            LOG_SUCCESS(L"Commands injected and adapted: " + dllPath);
        }
        catch (...) { LOG_ERROR(L"Exception in LoadOliCommandPlugin"); }
    }

    // 5. Finalizare
    if (loadedAnything) {
        LOG_SUCCESS(L"Plugin '" + pluginName + L"' is fully operational.");
        return true;
    }
    else {
        LOG_ERROR(L"Invalid Plugin: No entry points (LoadOliFunctionPlugin/LoadOliCommandPlugin) found in " + dllPath);
        PortTools::freeDynamicLibrary(hLib);
        return false;
    }
}


/*
vData vOliEngine::callUserByteCodeFunction(const std::wstring& funcName, const std::vector<vData>& args, vData context) {
    LOG_INFO(L"[VM] Apel functia: " + funcName + L" cu " + std::to_wstring(args.size()) + L" argumente.");

    auto it = m_bytecodeFunctions.find(funcName);
    if (it == m_bytecodeFunctions.end()) {
        LOG_ERROR(L"   -> EROARE: Functia " + funcName + L" nu este inregistrata!");
        return vData();
    }

    const ByteCodeProcedure& func = it->second;
    StackFrame frame;
    frame.functionName = funcName;

    for (size_t i = 0; i < func.params.size(); ++i) {
        std::wstring cleanPName = this->cleanVariableName(func.params[i]);
        vData argVal = (i < args.size()) ? args[i] : vData();
        frame.localVariables[cleanPName] = argVal;

        LOG_DEBUG(L"   -> Mapare Param: " + cleanPName + L" = " + argVal.toWString());
    }

    m_callStack.push_back(std::move(frame));

    OliStatus oldStatus = m_executionStatus; // Salvăm statusul (ex: RUNNING)
    m_executionStatus = OliStatus::RUNNING;  // Ne asigurăm că funcția pornește

    this->executeBytecode(*(func.compiledBody));
    m_executionStatus = oldStatus; // RESTAURĂM statusul pentru programul principal!


    vData result = vData();
    if (!m_callStack.empty()) {
        result = m_callStack.back().localVariables[L"return"];
        LOG_SUCCESS(L"   -> Functia " + funcName + L" a returnat: " + result.toWString());
        m_callStack.pop_back();
    }

    return result;
}
*/

vData vOliEngine::callUserByteCodeFunction(const std::wstring& funcName, const std::vector<vData>& args, vData context) {
    LOG_INFO(L"[VM] Apel functia: " + funcName + L" cu " + std::to_wstring(args.size()) + L" argumente.");

    // 1. Căutăm funcția în tabelul de bytecode
    auto it = m_bytecodeFunctions.find(funcName);
    if (it == m_bytecodeFunctions.end()) {
        LOG_ERROR(L"   -> EROARE: Functia " + funcName + L" nu este inregistrata!");
        return vData();
    }

    const ByteCodeProcedure& func = it->second;

    // 2. Pregătim noul cadru de stivă (StackFrame)
    StackFrame frame;
    frame.functionName = funcName;

    // --- INJECTARE CONTEXT 'this' ---
    // Aceasta permite funcției să acceseze obiectul apelant prin variabila $this
    frame.localVariables[L"this"] = context;

    // 3. Maparea parametrilor funcției
    for (size_t i = 0; i < func.params.size(); ++i) {
        std::wstring cleanPName = this->cleanVariableName(func.params[i]);
        // Dacă argumentul lipsește la apel, folosim o valoare default (null)
        vData argVal = (i < args.size()) ? args[i] : vData();
        frame.localVariables[cleanPName] = argVal;

        LOG_DEBUG(L"   -> Mapare Param: " + cleanPName + L" = " + argVal.toWString());
    }

    // Inițializăm variabila de return cu NULL pentru a evita accesul la memorie neinițializată
    frame.localVariables[L"return"] = vData();

    // 4. Gestionăm stiva și execuția
    m_callStack.push_back(std::move(frame));

    OliStatus oldStatus = m_executionStatus;
    m_executionStatus = OliStatus::RUNNING;

    // Executăm efectiv chunk-ul de bytecode al funcției
    this->executeBytecode(*(func.compiledBody));

    // Restaurăm statusul (pentru a nu opri execuția principală dacă funcția a dat return)
    m_executionStatus = oldStatus;

    // 5. Recuperăm rezultatul și curățăm stiva
    vData result = vData();
    if (!m_callStack.empty()) {
        result = m_callStack.back().localVariables[L"return"];
        LOG_SUCCESS(L"   -> Functia " + funcName + L" a returnat: " + result.toWString());
        m_callStack.pop_back();
    }

    return result;
}

void vOliEngine::registerBytecodeFunction(const std::wstring& name, const ByteCodeProcedure& proc) {
    // Folosim o conversie sigura la Uppercase
    std::wstring upperName = name;
    for (auto& c : upperName) c = std::towupper(c);

    this->m_bytecodeFunctions[upperName] = proc;
    LOG_DEBUG(L"[VM] Functie inregistrata in map: " + upperName);
}


// Această funcție citește un chunk și înregistrează funcțiile în Engine
// În OliEngine.cpp, la final unde ai pus implementarea:
void vDataSerialize::deserializeChunkToEngine(std::istream& in, OliChunk& outChunk, vOliEngine* engine) {
    // 1. Citim Constante
    uint32_t constCount = 0;
    in.read(reinterpret_cast<char*>(&constCount), sizeof(constCount));
    LOG_DEBUG(L"[SERIALIZE] Citim " + std::to_wstring(constCount) + L" constante.");

    for (uint32_t i = 0; i < constCount; ++i) {
        outChunk.constants.push_back(vDataSerialize::deserializevData(in));
    }

    // 2. Citim Cod
    uint32_t codeSize = 0;
    in.read(reinterpret_cast<char*>(&codeSize), sizeof(codeSize));
    LOG_DEBUG(L"[SERIALIZE] Citim " + std::to_wstring(codeSize) + L" bytes de cod.");

    outChunk.code.resize(codeSize);
    in.read(reinterpret_cast<char*>(outChunk.code.data()), codeSize);

    // 3. Citim Proceduri (Funcții)
    uint32_t procCount = 0;
    in.read(reinterpret_cast<char*>(&procCount), sizeof(procCount));
    LOG_DEBUG(L"[SERIALIZE] Detectat procCount: " + std::to_wstring(procCount));

    for (uint32_t i = 0; i < procCount; ++i) {
        ByteCodeProcedure proc;
        proc.name = vDataSerialize::deserializeWString(in);

        uint32_t paramCount = 0;
        in.read(reinterpret_cast<char*>(&paramCount), sizeof(paramCount));
        for (uint32_t j = 0; j < paramCount; ++j) {
            proc.params.push_back(vDataSerialize::deserializeWString(in));
        }

        uint8_t variadic = 0;
        in.read(reinterpret_cast<char*>(&variadic), 1);
        proc.isVariadic = (variadic == 1);

        proc.compiledBody = std::make_shared<OliChunk>();

        // RECURSIVITATE: Folosim explicit namespace-ul pentru a evita apelul global
        vDataSerialize::deserializeChunkToEngine(in, *proc.compiledBody, engine);

        if (engine) {
            LOG_SUCCESS(L"[SERIALIZE] Inregistram functia: " + proc.name);
            engine->registerBytecodeFunction(proc.name, proc);
        }
    }
}

void vOliEngine::assignToByteCodeVariable(const std::wstring& varName, const vData& newValue) {
    std::wstring trimmed = trim(varName);
    if (trimmed.empty()) return;

    // 1. Identificăm contextul (Global vs Local)
    bool forceGlobal = (trimmed[0] == L'@');
    if (forceGlobal) trimmed.erase(0, 1);

    // 2. Separăm rădăcina de eventuala cale (.prop sau [idx])
    size_t firstSep = trimmed.find_first_of(L"[.");
    std::wstring rootPart = (firstSep == std::wstring::npos) ? trimmed : trimmed.substr(0, firstSep);
    std::wstring pathRemainder = (firstSep == std::wstring::npos) ? L"" : trimmed.substr(firstSep);

    // Curățăm sigiliul $ dacă a mai rămas (VM-ul lucrează cu nume curate în chei)
    if (!rootPart.empty() && rootPart[0] == L'$') rootPart.erase(0, 1);

    // 3. Obținem referința către locația de memorie (L-Value)
    vData* rootPtr = nullptr;
    if (forceGlobal || m_callStack.empty()) {
        rootPtr = &m_globalVariables[rootPart];
    }
    else {
        auto& locals = m_callStack.back().localVariables;
        rootPtr = (locals.count(rootPart)) ? &locals[rootPart] : &m_globalVariables[rootPart];
    }

    // --- PASUL CRITIC: ATRIBUIRE DIRECTĂ vs NAVIGARE ---
    if (pathRemainder.empty()) {
        // Dacă nu avem cale, scriem valoarea DIRECT. 
        // Aici am eliminat crearea de Map-uri inutile (Matrioșka).
        *rootPtr = newValue;
        LOG_DEBUG(L"[VM] Atribuire directă: " + rootPart + L" = " + newValue.toWString());
        return;
    }

    // 4. Dacă există o cale (ex: .x sau [0]), folosim logica de navigare
    vData* target = navigateOrCreatePath(rootPtr, pathRemainder);

    if (target) {
        // Extragem cheia finală din cale (ex: "x" din "poz.x")
        size_t lastSep = trimmed.find_last_of(L".[");
        std::wstring field = trimmed.substr(lastSep + 1);
        if (!field.empty() && field.back() == L']') field.pop_back();

        // 5. Actualizăm în interiorul containerului (Map sau Array)
        if (target->isMap()) {
            auto* m = target->rawMap();
            if (m) {
                (*m)[field] = newValue;
                LOG_DEBUG(L"[VM] Actualizat câmp: " + rootPart + L" -> " + field);
            }
        }
        else if (target->isArray()) {
            auto* arr = target->rawArray();
            try {
                size_t idx = std::stoull(field);
                if (arr) {
                    if (idx >= arr->size()) arr->resize(idx + 1);
                    (*arr)[idx] = newValue;
                }
            }
            catch (...) { /* Log eroare index */ }
        }
        else {
            // Auto-vivificare: Dacă am ajuns aici și nu e Map, îl transformăm
            // (Se întâmplă pentru $a.b = 10 când $a nu exista)
            *target = vData::CreateMap();
            (*target->rawMap())[field] = newValue;
            LOG_DEBUG(L"[VM] Creat structură nouă pentru calea: " + rootPart);
        }
    }
}