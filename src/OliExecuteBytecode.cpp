
#include "vDataSerialize.hpp"
#include "OliEngine.hpp"
#include "PortTools.hpp"
#include "OliBytecode.hpp"

#include <cmath>


void vOliEngine::executeBytecode(const OliChunk& chunk,size_t framePtr) {
	// 1. Înregistrăm funcțiile (Logic deja existentă)
    for (auto const& [name, proc] : chunk.procedures) {
        this->m_bytecodeFunctions[name] = proc;
    }

    this->m_executionStatus = OliStatus::RUNNING;
    size_t ip = 0;

    // ATENȚIE: NU mai declarăm std::vector<vData> stack aici!
    // Folosim this->m_stack pentru a vedea ce a pregătit callUserByteCodeFunction
	// --- MAGIA ESTE AICI ---
    // Creăm o referință către m_stack. 
    // Acum 'stack' este doar un alt nume pentru 'this->m_stack'.
    std::vector<vData>& stack = this->m_stack;
	
	
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
            //vData val = stack.back().getTrueData(); 
            vData val = stack.back().getScalarValue();
            stack.pop_back();
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

            vData val;
            // 1. Căutăm direct în stack-ul local (fără resolveVariable)
            if (!m_callStack.empty() && m_callStack.back().localVariables.count(cleanName)) {
                val = m_callStack.back().localVariables[cleanName];
            }
            // 2. Căutăm direct în globale
            else if (m_globalVariables.count(cleanName)) {
                val = m_globalVariables[cleanName];
            }

            stack.push_back(val);
            break;
        }

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
            if (ConsoleManager::getInstance().getLogLevel() <= LogLevel::DEBUG) {
                LOG_DEBUG(L"VM_DEBUG: SET_INDIRECT pe " + stack[stack.size() - 3].toWString());
            }

            // 1. Extragem argumentele în ordinea LIFO (Last In, First Out)
            // Stiva la intrare: [Container, Index, Value] <- top
            vData value = stack.back(); stack.pop_back();
            vData index = stack.back(); stack.pop_back();
            vData container = stack.back(); stack.pop_back();

            //vData finalValue = value.getTrueData();
            vData finalValue = value.getScalarValue();

            // 2. Operăm pe Map (folosim rawMap() care face automat getTrueData())
            if (ConsoleManager::getInstance().getLogLevel() <= LogLevel::DEBUG) {
                LOG_DEBUG(L"VM: Executing SET_INDIRECT. Container Type: " + getVariantTypeName(container));
            }
            if (container.isMap()) {
                auto* m = container.rawMap();
                if (m) {
                    // Conversia indexului la string este vitală pentru cheile de Map
                    (*m)[index.toWString()] = finalValue;
                }
            }
            // 3. Operăm pe Array
            else if (container.isArray()) {
                auto* arr = container.rawArray();
                long long idx = index.toInt();
                if (arr && idx >= 0 && idx < (long long)arr->size()) {
                    (*arr)[(size_t)idx] = finalValue;
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
        


        case OpCode::OP_GET_LOCAL: {
            uint8_t slot = chunk.code[ip++];
            size_t targetIdx = framePtr + slot;

            if (targetIdx < stack.size()) {
                // Copiem valoarea într-o variabilă temporară pentru a evita 
                // problemele de invalidare a referinței în timpul push_back
                vData val = stack[targetIdx];
                stack.push_back(val);
            }
            else {
                // Dacă slotul nu există, punem un NULL/Monostate implicit
                stack.push_back(vData());
                LOG_ERROR(L"VM: Încercare citire slot local neinițializat: " + std::to_wstring(slot));
            }
            break;
        }

        case OpCode::OP_SET_LOCAL: {
            uint8_t slot = chunk.code[ip++];
            //vData val = stack.back().getTrueData();
            vData val = stack.back().getScalarValue();
            stack.pop_back();

            size_t targetIdx = framePtr + slot;

            // Asigurăm că stiva are loc pentru acest slot local
            if (targetIdx >= stack.size()) {
                stack.resize(targetIdx + 1);
            }

            stack[targetIdx] = val;
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
            if (valB == 0) { LOG_ERROR(L"Runtime Error: Div by zero!"); this->m_executionStatus = OliStatus::ERR; return; }
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

        
        
        
        // --- 8. SYSTEM & STRING ---
       
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
            // 1. Citim metadatele (indexul numelui funcției și numărul de argumente)
            if (ip + 2 >= chunk.code.size()) {
                LOG_ERROR(L"VM: Bytecode corupt la OP_CALL_NATIVE (lipsesc metadate).");
                this->m_executionStatus = OliStatus::ERR;
                return;
            }

            uint16_t nameIdx = (uint16_t)((chunk.code[ip] << 8) | chunk.code[ip + 1]);
            ip += 2;
            uint8_t argCount = chunk.code[ip++];

            // Validăm indexul constantelor pentru a evita crash la citire
            if (nameIdx >= chunk.constants.size()) {
                LOG_ERROR(L"VM: Index constanta invalid pentru numele functiei.");
                this->m_executionStatus = OliStatus::ERR;
                return;
            }

            std::wstring rawName = chunk.constants[nameIdx].toWString();
            std::wstring funcName = rawName;

            // --- FIX CRITIC: DECOJIRE IERARHIE PENTRU APEL DINAMIC ---
            if (!funcName.empty() && (funcName[0] == L'$' || funcName[0] == L'@')) {
                std::wstring cleanVarName = funcName.substr(1);
                vData varContent = this->getVar(rawName);

                if (varContent.isMap()) {
                    auto m = varContent.rawMap(); // Folosim helper-ul rawMap pentru siguranță
                    if (m && m->count(cleanVarName)) {
                        funcName = (*m).at(cleanVarName).toWString();
                    }
                    else {
                        funcName = varContent.toWString();
                    }
                }
                else {
                    funcName = varContent.toWString();
                }
            }

            // 2. Normalizăm numele pentru lookup (ex: "gl_init" -> "GL_INIT")
            std::wstring upperName = funcName;
            for (auto& c : upperName) c = std::towupper(c);

            // 3. Colectăm argumentele (LIFO)
            // IMPORTANT: Copiem valorile de pe stivă ÎNAINTE de a le scoate,
            // pentru a evita pointeri suspendați (dangling pointers).
            std::vector<vData> args;
            args.reserve(argCount);

            for (int i = 0; i < argCount; ++i) {
                if (stack.empty()) {
                    LOG_ERROR(L"Runtime Error: Stiva goala la colectarea argumentelor pentru " + funcName);
                    this->m_executionStatus = OliStatus::ERR;
                    return;
                }

                // Luăm elementul de sus, îl dereferențiem DE SALVARE și îl scoatem
                vData topElement = stack.back();
                stack.pop_back();

                // Verificăm dacă nu cumva Bytecode-ul corupt ne dă un pointer bizar
                if (topElement.isPointer()) {
                    // Dacă e pointer, încercăm să extragem datele cu mare atenție
                    args.insert(args.begin(), topElement.getTrueData());
                }
                else {
                    args.insert(args.begin(), topElement);
                }
            }

            // 4. Executăm handler-ul nativ
            auto it = m_functionsHandlers.find(upperName);
            if (it != m_functionsHandlers.end()) {
                try {
                    // Trimitem copia argumentelor către plugin
                    vData result = it->second(args);
                    stack.push_back(result);
                }
                catch (...) {
                    LOG_ERROR(L"VM: Crash detectat in interiorul functiei native: " + funcName);
                    this->m_executionStatus = OliStatus::ERR;
                    return;
                }
            }
            else {
                LOG_ERROR(L"Runtime Error: Functia '" + funcName + L"' nu a fost gasita.");
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
                LOG_ERROR(L"VM Error: Failed to load plugin required by bytecode: " + pluginPath);
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
			vData source = stack.back(); 
			stack.pop_back(); 

			// Folosim resolve() pe care l-am reparat la pasul 1
			vData realSource = source.resolve();

			// Adăugăm în stiva de iteratoare
			m_iterStack.push_back({ realSource, 0 }); 
			break;
		}
		
		
		case OpCode::OP_ITER_NEXT: {
			if (m_iterStack.empty()) {
				LOG_ERROR(L"Runtime Error: OP_ITER_NEXT fără un context de iterație activ.");
				this->m_executionStatus = OliStatus::ERR;
				break;
			}

			// 1. Accesăm starea izolată a iterației
			IterState& itState = m_iterStack.back();
			
			// 2. DEREFERENȚIERE: Obținem sursa reală (Array, Map sau String)
			// Chiar dacă în itState.source avem un Pointer, getTrueData ne dă obiectul final.
			vData& trueSource = itState.source.getTrueData();
			
			vData nextValue;
			bool isDone = true;

			// 3. Logica de extragere a valorii în funcție de tipul REAL al sursei
			if (trueSource.isArray()) {
				// Acum std::get este sigur, deoarece trueSource nu mai este un Pointer
				auto& arr = *std::get<vDataArray>(trueSource.value);
				if (itState.index >= 0 && itState.index < (long long)arr.size()) {
					nextValue = arr[itState.index];
					isDone = false;
				}
			}
			else if (trueSource.isMap()) {
				auto& m = *std::get<vDataMap>(trueSource.value);
				if (itState.index >= 0 && itState.index < (long long)m.size()) {
					auto it = m.begin();
					std::advance(it, itState.index);
					nextValue = vData(it->first); // Returnăm cheia pentru Map-uri
					isDone = false;
				}
			}
			else if (trueSource.isString()) {
				const std::wstring& str = std::get<std::wstring>(trueSource.value);
				if (itState.index >= 0 && itState.index < (long long)str.size()) {
					nextValue = vData(std::wstring(1, str[itState.index]));
					isDone = false;
				}
			}

			// 4. Actualizăm indexul în starea izolată (nu pe stiva de date!)
			itState.index++;

			// 5. Punem rezultatele pe stiva principală pentru restul scriptului
			if (!isDone) {
				stack.push_back(nextValue); // Valoarea care ajunge în $obj
			}
			
			// Flag-ul pentru JUMP_IF_TRUE (1/true oprește bucla, 0/false continuă)
			stack.push_back(vData(isDone)); 
			break;
		}
		
		
		case OpCode::OP_ITER_FREE: {
			if (!m_iterStack.empty()) {
				m_iterStack.pop_back();
			}
			break;
		}
		
		
        case OpCode::OP_DEF_TYPE: {
            // 1. Citim numele tipului curent (2 bytes)
            uint16_t nameIdx = (uint16_t)((chunk.code[ip] << 8) | chunk.code[ip + 1]);
            ip += 2;
            std::wstring typeName = to_upper(chunk.constants[nameIdx].toWString());

            // 2. NOU: Citim numele părintelui (2 bytes)
            uint16_t parentIdx = (uint16_t)((chunk.code[ip] << 8) | chunk.code[ip + 1]);
            ip += 2;
            std::wstring parentName = to_upper(chunk.constants[parentIdx].toWString());

            vTypeBlueprint bp;
            bp.name = typeName;
            bp.parentName = parentName; // Ar fi util să adaugi acest câmp în structura vTypeBlueprint

            // --- LOGICA DE MOȘTENIRE (HYDRATION) ---
            if (!parentName.empty()) {
                if (m_blueprints.count(parentName)) {
                    const vTypeBlueprint& parentBp = m_blueprints[parentName];

                    // Moștenim câmpurile
                    bp.fields = parentBp.fields;

                    // Moștenim metodele (inițial Boss::move va pointa către Inamic::move)
                    bp.methods = parentBp.methods;
                    if (ConsoleManager::getInstance().getLogLevel() <= LogLevel::DEBUG) {
                        LOG_DEBUG(L"[VM] " + typeName + L" mosteneste de la " + parentName);
                    }
                }
                else {
                    LOG_ERROR(L"[VM] Eroare: Clasa parinte '" + parentName + L"' nu a fost gasita!");
                }
            }

            // 3. Citim metadatele
            bp.isClass = (chunk.code[ip++] == 1);
            uint8_t fieldCount = chunk.code[ip++];
            uint8_t methodCount = chunk.code[ip++];

            if (ConsoleManager::getInstance().getLogLevel() <= LogLevel::DEBUG) {
                LOG_DEBUG(L"[VM] Inregistrare " + std::wstring(bp.isClass ? L"CLASS: " : L"STRUCT: ") + typeName);
            }

            // 4. Citim indicii Câmpurilor (Adăugare sau Overlap)
            for (int i = 0; i < fieldCount; ++i) {
                uint16_t fIdx = (uint16_t)((chunk.code[ip] << 8) | chunk.code[ip + 1]);
                ip += 2;
                std::wstring fieldName = to_lower(chunk.constants[fIdx].toWString());

                // Evităm duplicatele dacă câmpul există deja de la părinte
                if (std::find(bp.fields.begin(), bp.fields.end(), fieldName) == bp.fields.end()) {
                    bp.fields.push_back(fieldName);
                }
                if (ConsoleManager::getInstance().getLogLevel() <= LogLevel::DEBUG) {
                    LOG_DEBUG(L"    -> Field[" + std::to_wstring(i) + L"]: " + fieldName);
                }
            }

            // 5. Citim indicii Metodelor (Overriding magic happens here!)
            for (int i = 0; i < methodCount; ++i) {
                uint16_t mIdx = (uint16_t)((chunk.code[ip] << 8) | chunk.code[ip + 1]);
                ip += 2;
                std::wstring methodName = to_upper(chunk.constants[mIdx].toWString());

                // Mapăm metoda nouă. Dacă methodName exista deja în bp.methods (de la părinte),
                // acum va fi suprascrisă cu noua cale (typeName::methodName).
                std::wstring internalFuncName = typeName + L"::" + methodName;
                bp.methods[methodName] = internalFuncName;

                if (ConsoleManager::getInstance().getLogLevel() <= LogLevel::DEBUG) {
                    LOG_DEBUG(L"    -> Method[" + std::to_wstring(i) + L"]: " + methodName + L" (Target: " + internalFuncName + L")");
                }
            }

            // 6. Salvare Blueprint final
            m_blueprints[typeName] = bp;

            LOG_SUCCESS(L"Blueprint " + typeName + L" gata! " +
                (parentName.empty() ? L"" : L"(Mostenit din " + parentName + L")"));
            break;
        }
        
        case OpCode::OP_CALL_METHOD: {
            uint8_t argCount = chunk.code[ip++];

            // 1. Scoatem numele metodei
            vData methodNameData = stack.back(); stack.pop_back();
            std::wstring methodName = to_upper(methodNameData.toWString());

            // 2. Scoatem OBIECTUL (Contextul) - ACUM îi dăm pop!
            vData contextObj = stack.back(); stack.pop_back();

            if (ConsoleManager::getInstance().getLogLevel() <= LogLevel::DEBUG) {
                LOG_DEBUG(L"[VM] Apel metoda: " + methodName + L" pe obiect de tip " + getVariantTypeName(contextObj));
            }

            if (contextObj.isMap()) {
                auto m = contextObj.rawMap();
                if (m->count(L"__type__")) {
                    std::wstring typeName = (*m)[L"__type__"].toWString();

                    if (m_blueprints.count(typeName)) {
                        auto& bp = m_blueprints[typeName];
                        if (bp.methods.count(methodName)) {
                            std::wstring finalFunc = bp.methods[methodName];

                            // 3. Colectăm argumentele (care acum sunt în vârful stivei)
                            std::vector<vData> args(argCount);
                            for (int i = argCount - 1; i >= 0; --i) {
                                args[i] = stack.back();
                                stack.pop_back();
                            }

                            // 4. Executăm
                            vData result = this->callUserByteCodeFunction(finalFunc, args, contextObj);
                            stack.push_back(result);

                            LOG_SUCCESS(L"[VM] Metoda executata: " + finalFunc);
                            break;
                        }
                    }
                }
            }

            LOG_ERROR(L"Runtime Error: Metoda '" + methodName + L"' nu a putut fi rezolvata.");
            this->m_executionStatus = OliStatus::ERR;
            break;
        }

        case OpCode::OP_SET_PTR: {
            // Stiva la noi este: [Valoare_Noua, Adresa]
            if (stack.size() < 2) {
                LOG_ERROR(L"OP_SET_PTR: Stack underflow");
                break;
            }

            vData ptrData = stack.back(); stack.pop_back();   // Scoatem Adresa
            vData newValue = stack.back(); stack.pop_back(); // Scoatem Valoarea Nouă

            if (vData** addrPtr = std::get_if<vData*>(&ptrData.value)) {
                if (*addrPtr) {
                    **addrPtr = newValue;
                    if (ConsoleManager::getInstance().getLogLevel() <= LogLevel::DEBUG) {
                        LOG_DEBUG(L"[VM] Pointer Write SUCCESS: " + newValue.toWString());
                    }
                }
                else {
                    LOG_ERROR(L"Runtime Error: Null pointer write attempt.");
                }
            }
            else {
                LOG_ERROR(L"Runtime Error: Expected pointer for OP_SET_PTR, got " + getVariantTypeName(ptrData));
            }
            break;
        }
        case OpCode::OP_SWAP: {
            if (stack.size() < 2) break;
            vData a = stack.back(); stack.pop_back();
            vData b = stack.back(); stack.pop_back();
            stack.push_back(a);
            stack.push_back(b);
            break;
        }
		
        default:
            LOG_ERROR(L"VM Error: OpCode necunoscut [0x" + std::to_wstring((int)instruction) + L"] la IP: " + std::to_wstring(ip - 1));
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
        LOG_ERROR(L"Could not open bytecode file: " + str_to_wstr(path));
        return;
    }

    OliChunk mainChunk;

    // --- REPARAȚIA CRITICĂ ---
    // Apelăm funcția care știe să citească recursiv Constante + Cod + Proceduri
    vDataSerialize::deserializeChunkToEngine(ifs, mainChunk, this);

    ifs.close();

    // Resetăm motorul și executăm Main
    this->m_executionStatus = OliStatus::RUNNING;
    this->executeBytecode(mainChunk,0);
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

                if (ConsoleManager::getInstance().getLogLevel() <= LogLevel::DEBUG) {
                    LOG_DEBUG(L"Injected function: " + upName);
                }
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



vData vOliEngine::callUserByteCodeFunction(const std::wstring& funcName, const std::vector<vData>& args, vData context) {
    LOG_INFO(L"[VM] Pregătire apel funcție: " + funcName + L" (" + std::to_wstring(args.size()) + L" argumente)");

    // 1. Căutăm funcția în tabelul de bytecode
    auto it = m_bytecodeFunctions.find(funcName);
    if (it == m_bytecodeFunctions.end()) {
        LOG_ERROR(L"-> EROARE: Funcția " + funcName + L" nu este înregistrată!");
        return vData();
    }

    const ByteCodeProcedure& func = it->second;

    // --- LOGICA DE STIVĂ (VM MODE) ---
    
    // 2. Reținem baza noului cadru (Frame Pointer)
    size_t newFramePtr = this->m_stack.size();
    bool isMethod = (funcName.find(L"::") != std::wstring::npos);

    // 3. INJECTARE CONTEXT ($this) - CRITIC PENTRU METODE
    // Dacă este o metodă, Compilatorul a rezervat Slotul 0 pentru $this.
    if (isMethod) {
        this->m_stack.push_back(context);
        if (ConsoleManager::getInstance().getLogLevel() <= LogLevel::DEBUG) {
            LOG_DEBUG(L"[VM] Context '$this' injectat la Slotul 0 pentru metoda: " + funcName);
        }
    }

    // 4. Maparea parametrilor funcției
    // Aceștia vor ocupa sloturile imediat următoare (Slot 0+ pentru funcții, Slot 1+ pentru metode)
    for (size_t i = 0; i < func.params.size(); ++i) {
        if (i < args.size()) {
            this->m_stack.push_back(args[i]);
            if (ConsoleManager::getInstance().getLogLevel() <= LogLevel::DEBUG) {
                    (L"[VM] Parametru mapat: " + func.params[i] + L" = " + args[i].toWString());
            }
        } else {
            // Parametri lipsă primesc valoarea NULL (monostate)
            this->m_stack.push_back(vData());
            if (ConsoleManager::getInstance().getLogLevel() <= LogLevel::DEBUG) {
                LOG_DEBUG(L"[VM] Parametru lipsă (default NULL): " + func.params[i]);
            }
        }
    }

    // 5. Execuția efectivă a Bytecode-ului
    OliStatus oldStatus = m_executionStatus;
    m_executionStatus = OliStatus::RUNNING;

    // Transmitem framePtr pentru ca OP_GET_LOCAL să știe de unde să citească
    this->executeBytecode(*(func.compiledBody), newFramePtr);

    m_executionStatus = oldStatus;

    // 6. Recuperăm rezultatul returnat (lăsat de OP_RETURN în vârful stivei)
    vData result = vData();
    if (this->m_executionStatus != OliStatus::ERR && !this->m_stack.empty()) {
        result = this->m_stack.back();
        // Nu facem pop aici încă, pentru a nu altera stiva în timpul unwinding-ului
    }

    // 7. Stack Unwinding (Curățăm cadrul de stivă)
    // Eliminăm tot ce a fost local (context, parametri, variabile locale create în interior)
    while (this->m_stack.size() > newFramePtr) {
        this->m_stack.pop_back();
    }

    LOG_SUCCESS(L"-> Funcția " + funcName + L" a returnat: " + result.toWString());
    
    return result;
}

void vOliEngine::registerBytecodeFunction(const std::wstring& name, const ByteCodeProcedure& proc) {
    // Folosim o conversie sigura la Uppercase
    std::wstring upperName = name;
    for (auto& c : upperName) c = std::towupper(c);

    this->m_bytecodeFunctions[upperName] = proc;
    if (ConsoleManager::getInstance().getLogLevel() <= LogLevel::DEBUG) {
        LOG_DEBUG(L"[VM] Functie inregistrata in map: " + upperName);
    }
}


// Această funcție citește un chunk și înregistrează funcțiile în Engine
// În OliEngine.cpp, la final unde ai pus implementarea:
void vDataSerialize::deserializeChunkToEngine(std::istream& in, OliChunk& outChunk, vOliEngine* engine) {
    // 1. Citim Constante
    uint32_t constCount = 0;
    in.read(reinterpret_cast<char*>(&constCount), sizeof(constCount));
    if (ConsoleManager::getInstance().getLogLevel() <= LogLevel::DEBUG) {
        LOG_DEBUG(L"[SERIALIZE] Citim " + std::to_wstring(constCount) + L" constante.");
    }

    for (uint32_t i = 0; i < constCount; ++i) {
        outChunk.constants.push_back(vDataSerialize::deserializevData(in));
    }

    // 2. Citim Cod
    uint32_t codeSize = 0;
    in.read(reinterpret_cast<char*>(&codeSize), sizeof(codeSize));
    if (ConsoleManager::getInstance().getLogLevel() <= LogLevel::DEBUG) {
        LOG_DEBUG(L"[SERIALIZE] Citim " + std::to_wstring(codeSize) + L" bytes de cod.");
    }

    outChunk.code.resize(codeSize);
    in.read(reinterpret_cast<char*>(outChunk.code.data()), codeSize);

    // 3. Citim Proceduri (Funcții)
    uint32_t procCount = 0;
    in.read(reinterpret_cast<char*>(&procCount), sizeof(procCount));
    if (ConsoleManager::getInstance().getLogLevel() <= LogLevel::DEBUG) {
        LOG_DEBUG(L"[SERIALIZE] Detectat procCount: " + std::to_wstring(procCount));
    }

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
        if (ConsoleManager::getInstance().getLogLevel() <= LogLevel::DEBUG) {
            LOG_DEBUG(L"[VM] Atribuire directă: " + rootPart + L" = " + newValue.toWString());
        }
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
                if (ConsoleManager::getInstance().getLogLevel() <= LogLevel::DEBUG) {
                    LOG_DEBUG(L"[VM] Actualizat câmp: " + rootPart + L" -> " + field);
                }
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
            if (ConsoleManager::getInstance().getLogLevel() <= LogLevel::DEBUG) {
                LOG_DEBUG(L"[VM] Creat structură nouă pentru calea: " + rootPart);
            }
        }
    }
}