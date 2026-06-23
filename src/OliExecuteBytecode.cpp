
#include "vDataSerialize.hpp"
#include "OliEngine.hpp"
#include "PortTools.hpp"
#include "OliBytecode.hpp"

#include <filesystem>
#include <cmath>
#ifndef _WIN32
#include <unistd.h>
#include <limits.h>
#endif


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

        
        
        

        case OpCode::OP_GET_GLOBAL: {
            uint16_t nameIdx = (uint16_t)((chunk.code[ip] << 8) | chunk.code[ip + 1]);
            ip += 2;
            std::wstring rawName = chunk.constants[nameIdx].toWString();
            std::wstring cleanName = this->cleanVariableName(rawName);

            vData val;

            // 1. Rezolvarea dinamică și nativă a contextului $this
            if (cleanName == L"this") {
                if (!this->m_methodContextStack.empty() && !this->m_methodContextStack.back().isNull() && !std::holds_alternative<std::monostate>(this->m_methodContextStack.back().value)) {
                    val = this->m_methodContextStack.back(); // Întoarce contextul din apelul dinamic
                }
                else if (framePtr < stack.size()) {
                    val = stack[framePtr]; // Fallback pentru metodele statice de tip Clasa::Metoda
                }
            }
            // 2. Căutăm direct în stack-ul local al interpretorului (dacă e cazul)
            else if (!m_callStack.empty() && m_callStack.back().localVariables.count(cleanName)) {
                val = m_callStack.back().localVariables[cleanName];
            }
            // 3. Căutăm în globalele motorului
            else if (m_globalVariables.count(cleanName)) {
                val = m_globalVariables[cleanName];
            }

            // 🔥 FIX CRITIC COMPLET PENTRU BUG-UL DE SCOPING AL COMPILATORULUI (@$tinta_nume)
            // Dacă variabila nu a fost găsită în globale, dar suntem în interiorul unei funcții din Bytecode,
            // verificăm dacă numele căutat aparține de fapt unui parametru local alocat pe stivă.
            if (val.isNull() || std::holds_alternative<std::monostate>(val.value)) {
                for (auto const& [funcName, proc] : this->m_bytecodeFunctions) {
                    if (proc.compiledBody.get() == &chunk) {
                        bool isMethod = (funcName.find(L"::") != std::wstring::npos);
                        for (size_t i = 0; i < proc.params.size(); ++i) {
                            std::wstring cleanParam = this->cleanVariableName(proc.params[i]);
                            if (cleanParam == cleanName) {
                                size_t slot = i + (isMethod ? 1 : 0);
                                if (framePtr + slot < stack.size()) {
                                    val = stack[framePtr + slot];
                                }
                                break;
                            }
                        }
                        break;
                    }
                }
            }

            // =========================================================================
            // 🔥 NOUA ZONĂ DE INTERCEPTARE: Căutăm în Comenzile de Plugin (Command Plugins)
            // =========================================================================
            // Dacă valoarea este în continuare un vData gol (null/monostate) după toate verificările,
            // înseamnă că am dat peste o comandă din plugin tratată pasiv de compilator ca identificator.
            if (val.isNull() || std::holds_alternative<std::monostate>(val.value)) {
                std::wstring upperCleanName = cleanName;
                for (auto& c : upperCleanName) c = std::towupper(c); // Forțăm UPPERCASE pentru map-ul de handlere

                if (this->m_commandHandlers.count(upperCleanName)) {
                    if (ConsoleManager::getInstance().getLogLevel() <= LogLevel::DEBUG) {
                        LOG_DEBUG(L"VM: Interceptat apel de comanda globala din bytecode: " + upperCleanName);
                    }

                    // 1. Executăm instant handlerul din plugin (îi pasăm o linie goală de argumente L"")
                    this->m_commandHandlers[upperCleanName](L"");

                    // 2. Lăsăm 'val' ca fiind null (monostate). 
                    // Când va fi împins pe stivă mai jos, va asigura că stiva de expresii a mașinii 
                    // virtuale rămâne perfect echilibrată, prevenind orice desincronizare.
                }
            }

            stack.push_back(val);
            break;
        }
        
case OpCode::OP_GET_INDIRECT: {
    if (stack.empty()) break;

    // 1. Calculăm dinamic baza stivei de expresii pentru a izola argumentele funcției
    size_t localsCount = 0;
    for (auto const& [funcName, proc] : this->m_bytecodeFunctions) {
        if (proc.compiledBody.get() == &chunk) {
            bool isMethod = (funcName.find(L"::") != std::wstring::npos);
            localsCount = proc.params.size() + (isMethod ? 1 : 0);
            break;
        }
    }
    size_t exprStackBase = framePtr + localsCount;

    // 2. Extragem Indexul/Cheia sau adresa pointerului (de deasupra stivei)
    vData indexOrPtrRaw = stack.back();
    stack.pop_back();
    vData indexOrPtr = indexOrPtrRaw.getTrueData();

    // Verificăm dacă indexul extras este numeric (pentru indexare stringuri/array-uri)
    bool isIndexNumeric = indexOrPtr.isInt() || indexOrPtr.isFloat();
    if (indexOrPtr.isString()) {
        std::wstring s = indexOrPtr.toWString();
        if (!s.empty()) {
            bool allDigits = true;
            for (wchar_t c : s) {
                if (!std::iswdigit(c)) { allDigits = false; break; }
            }
            if (allDigits) isIndexNumeric = true;
        }
    }

    // 3. 🔥 REGULA DE AUR: Determinăm tipul de acces (BINAR vs UNAR)
    bool isBinaryAccess = false;

    if (stack.size() > exprStackBase) {
        vData nextOnStack = stack.back().getTrueData();
        if (nextOnStack.isMap() || nextOnStack.isArray()) {
            isBinaryAccess = true; // Map-urile și Array-urile sunt containere binare clare
        }
        else if (nextOnStack.isString() && isIndexNumeric) {
            isBinaryAccess = true; // String-urile sunt containere doar dacă indexul este strict numeric
        }
    }

    // 4. Executăm logica în funcție de tipul de acces determinat
    if (isBinaryAccess) {
        // --- CAZUL A: ACCES BINAR PROP-ZIS (Obiect + Proprietate) ---
        vData containerRaw = stack.back();
        stack.pop_back();
        vData container = containerRaw.getTrueData();

        if (container.isMap()) {
            auto* m = container.rawMap();
            std::wstring key = indexOrPtr.toWString();
            if (m && m->count(key)) {
                stack.push_back((*m)[key]);
            }
            else {
                stack.push_back(vData{ std::monostate{} });
            }
        }
        else if (container.isArray()) {
            auto* arr = container.rawArray();
            long long idx = indexOrPtr.toInt();
            if (arr && idx >= 0 && idx < (long long)arr->size()) {
                stack.push_back((*arr)[idx]);
            }
            else {
                stack.push_back(vData{ std::monostate{} });
            }
        }
        else if (container.isString()) {
            std::wstring str = container.toWString();
            long long idx = indexOrPtr.toInt();
            if (idx >= 0 && idx < (long long)str.length()) {
                stack.push_back(vData{ std::wstring(1, str[idx]) });
            }
            else {
                stack.push_back(vData{ L"" });
            }
        }
    }
    else {
        // --- CAZUL B: DEREFERENȚIERE UNARĂ (*ptr sau $$var) ---
        if (indexOrPtrRaw.isString()) {
            std::wstring rawName = indexOrPtrRaw.toWString();
            std::wstring cleanName = this->cleanVariableName(rawName);

            vData val;
            if (m_globalVariables.count(cleanName)) {
                val = m_globalVariables[cleanName];
            }
            else {
                // Căutăm valoarea adresei în parametrii locali ai funcției active
                for (auto const& [funcName, proc] : this->m_bytecodeFunctions) {
                    if (proc.compiledBody.get() == &chunk) {
                        bool isMethod = (funcName.find(L"::") != std::wstring::npos);
                        for (size_t i = 0; i < proc.params.size(); ++i) {
                            if (this->cleanVariableName(proc.params[i]) == cleanName) {
                                size_t slot = i + (isMethod ? 1 : 0);
                                if (framePtr + slot < stack.size()) {
                                    val = stack[framePtr + slot];
                                }
                                break;
                            }
                        }
                        break;
                    }
                }
            }
            stack.push_back(val);
        }
        else {
            stack.push_back(indexOrPtrRaw.getTrueData());
        }
    }
    break;
}

case OpCode::OP_SET_INDIRECT: {
    if (stack.size() < 3) {
        LOG_ERROR(L"OP_SET_INDIRECT Error: Stiva are prea putine elemente!");
        this->m_executionStatus = OliStatus::ERR;
        break;
    }

    vData value = stack.back();
    stack.pop_back();
    vData keyRaw = stack.back();
    stack.pop_back();
    vData containerRaw = stack.back();
    stack.pop_back();

    vData container = containerRaw.getTrueData();
    vData key = keyRaw.getTrueData();

    // Dacă containerul este un string, înseamnă că referința brută a supraviețuit. O rezolvăm acum:
    if (container.isString()) {
        std::wstring cleanName = this->cleanVariableName(container.toWString());
        if (m_globalVariables.count(cleanName)) {
            container = m_globalVariables[cleanName];
        }
        else {
            for (auto const& [funcName, proc] : this->m_bytecodeFunctions) {
                if (proc.compiledBody.get() == &chunk) {
                    bool isMethod = (funcName.find(L"::") != std::wstring::npos);
                    for (size_t i = 0; i < proc.params.size(); ++i) {
                        if (this->cleanVariableName(proc.params[i]) == cleanName) {
                            size_t slot = i + (isMethod ? 1 : 0);
                            if (framePtr + slot < stack.size()) {
                                container = stack[framePtr + slot];
                            }
                            break;
                        }
                    }
                    break;
                }
            }
        }
    }

    if (container.isMap()) {
        auto* m = container.rawMap();
        std::wstring keyStr = key.toWString();
        if (m) {
            (*m)[keyStr] = value;
        }
    }
    else if (container.isArray()) {
        auto* arr = container.rawArray();
        long long idx = key.toInt();
        if (arr && idx >= 0 && idx < (long long)arr->size()) {
            (*arr)[idx] = value;
        }
        else {
            LOG_ERROR(L"OP_SET_INDIRECT Error: Index out of bounds in Array!");
            this->m_executionStatus = OliStatus::ERR;
        }
    }
    else {
        LOG_ERROR(L"OP_SET_INDIRECT Error: Obiectul nu este Map/Array! Tip actual: " + this->getVariantTypeName(container));
        this->m_executionStatus = OliStatus::ERR;
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

            // --- FIX: Activăm getTrueData() și dezactivăm getScalarValue() ---
            vData val = stack.back().getTrueData();
            // vData val = stack.back().getScalarValue();
            stack.pop_back();

            size_t targetIdx = framePtr + slot;

            // Asigurăm că stiva are loc pentru acest slot local
            if (targetIdx >= stack.size()) {
                stack.resize(targetIdx + 1);
            }

            stack[targetIdx] = val;
            break;
        }

        case OpCode::OP_SET_GLOBAL: {
            // --- FIX: Activăm getTrueData() și dezactivăm getScalarValue() ---
            vData val = stack.back().getTrueData();
            // vData val = stack.back().getScalarValue();
            stack.pop_back();

            uint16_t nameIdx = (uint16_t)((chunk.code[ip] << 8) | chunk.code[ip + 1]);
            ip += 2;
            vData nameConst = chunk.constants[nameIdx].getTrueData();
            this->setVar(nameConst.toWString(), val);
            break;
        }
        

        case OpCode::OP_GET_ADDR: {
            uint16_t nameIdx = (uint16_t)((chunk.code[ip] << 8) | chunk.code[ip + 1]);
            ip += 2;
            std::wstring rawName = chunk.constants[nameIdx].toWString();
            std::wstring cleanName = this->cleanVariableName(rawName);

            // 🔥 FIX FINAL POINTERI: În motorul Oli, adresa (&) este reprezentată prin numele variabilei ca STRING ("boss").
            // Astfel evităm complet riscul de dangling pointers (pointeri defecți) în cazul în care vectorul stivei m_stack s-ar realoca în memorie!
            // Codul nostru din OP_GET_INDIRECT și OP_SET_INDIRECT este deja echipat să recunoască acest string
            // și să facă auto-dereferențiere automată direct din tabela globală.
            stack.push_back(vData(cleanName));
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
            //else if (rA.isNumber() && rB.isNumber()) { //poate trebuie un flag pentru precize mai mare la intregi
			//	stack.push_back(vData(rA.toInt() + rB.toInt()));
			//}
            else {
                stack.push_back(vData(vDataToDouble(rA) + vDataToDouble(rB)));
            }
            break;
        }
        
        case OpCode::OP_SUB: {
            vData b = stack.back(); stack.pop_back();
            vData a = stack.back(); stack.pop_back();

            // 🔥 FIX CRITIC: Rezolvăm pointerii/referințele înainte de orice verificare!
            vData rA = a.getTrueData();
            vData rB = b.getTrueData();

            if (rA.isInt() && rB.isInt()) {
                stack.push_back(vData(rA.toInt() - rB.toInt())); // Folosește .toInt() safe
            }
            else {
                stack.push_back(vData(vDataToDouble(rA) - vDataToDouble(rB)));
            }
            break;
        }

        case OpCode::OP_MUL: {
            vData b = stack.back(); stack.pop_back();
            vData a = stack.back(); stack.pop_back();

            // 🔥 FIX CRITIC: Rezolvăm pointerii/referințele
            vData rA = a.getTrueData();
            vData rB = b.getTrueData();

            // Păstrăm înmulțirea între întregi ca INT (important pentru indici și culori hex)
            if (rA.isInt() && rB.isInt()) {
                stack.push_back(vData(rA.toInt() * rB.toInt()));
            }
            else {
                stack.push_back(vData(vDataToDouble(rA) * vDataToDouble(rB)));
            }
            break;
        }

        case OpCode::OP_DIV: {
            vData b = stack.back(); stack.pop_back();
            vData a = stack.back(); stack.pop_back();

            // 🔥 FIX CRITIC: Rezolvăm pointerii/referințele
            vData rA = a.getTrueData();
            vData rB = b.getTrueData();

            double valB = vDataToDouble(rB);
            if (valB == 0) {
                LOG_ERROR(L"Runtime Error: Div by zero!");
                this->m_executionStatus = OliStatus::ERR;
                return;
            }

            if (rA.isInt() && rB.isInt()) {
                stack.push_back(vData(rA.toInt() / rB.toInt())); // Împărțire între întregi
            }
            else {
                stack.push_back(vData(vDataToDouble(rA) / valB));
            }
            break;
        }                   // --- 3. BITWISE & LOGICĂ ---
        case OpCode::OP_BAND: { vData b = stack.back(); stack.pop_back(); vData a = stack.back(); stack.pop_back(); stack.push_back(vData(a.getTrueData().toInt() & b.getTrueData().toInt())); break; }
        case OpCode::OP_BOR: { vData b = stack.back(); stack.pop_back(); vData a = stack.back(); stack.pop_back(); stack.push_back(vData(a.getTrueData().toInt() | b.getTrueData().toInt())); break; }
        case OpCode::OP_LOGICAL_NOT: { vData a = stack.back(); stack.pop_back(); stack.push_back(vData(!vDataToBool(a))); break; }

       
        case OpCode::OP_EQUAL: {
            if (stack.size() < 2) throw std::runtime_error("Stack underflow at OP_EQUAL");
            vData b = stack.back(); stack.pop_back();
            vData a = stack.back(); stack.pop_back();

            vData rA = a.getTrueData();
            vData rB = b.getTrueData();

            // 🔥 FIX CRITIC: Tratarea corectă a tipului NULL (Monostate)
            if (rA.isNull() || rB.isNull()) {
                // Sunt egale doar dacă AMBELE sunt null
                stack.push_back(vData(rA.isNull() && rB.isNull()));
            }
            else if (rA.isString() && rB.isString()) {
                stack.push_back(vData(rA.toWString() == rB.toWString()));
            }
            else if (rA.isInt() && rB.isInt()) {
                stack.push_back(vData(rA.toInt() == rB.toInt()));
            }
            else {
                // Fallback pentru numere combinate (Int cu Float, sau Float cu Float)
                stack.push_back(vData(vDataToDouble(rA) == vDataToDouble(rB)));
            }
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

            // 🔥 FIX: Extragem valorile reale
            vData rA = a.getTrueData();
            vData rB = b.getTrueData();

            if (rA.isInt() && rB.isInt()) {
                stack.push_back(vData(rA.toInt() < rB.toInt()));
            }
            else {
                stack.push_back(vData(vDataToDouble(rA) < vDataToDouble(rB)));
            }
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
            //std::wstring upperName = funcName;
            //for (auto& c : upperName) c = std::towupper(c);
			std::wstring upperName;
            for (wchar_t c : funcName) {
                if (std::iswalnum(c) || c == L'_' || c == L':' || c == L':') {
                    upperName += std::towupper(c);
                }
            }

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
               LOG_ERROR(L"Runtime Error: Functia '" + upperName + L"' nu a fost gasita!");
                LOG_INFO(L"Lista de functii disponibile in motor (" + std::to_wstring(m_functionsHandlers.size()) + L"):");
                for (auto const& [name, handler] : m_functionsHandlers) {
                    // Verificăm lungimea și caracterele brute
                    LOG_INFO(L"  - [" + name + L"] (len: " + std::to_wstring(name.length()) + L")");
                }
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
        
        case OpCode::OP_CALL: {
            uint16_t nameIdx = (uint16_t)((chunk.code[ip] << 8) | chunk.code[ip + 1]);
            ip += 2;
            uint8_t argCount = chunk.code[ip++];

            std::wstring rawName = chunk.constants[nameIdx].toWString();
            std::wstring funcName = to_upper(rawName);

            // Colectăm argumentele de pe stivă (LIFO)
            std::vector<vData> callArgs(argCount);
            for (int i = argCount - 1; i >= 0; --i) {
                callArgs[i] = stack.back();
                stack.pop_back();
            }

            if (ConsoleManager::getInstance().getLogLevel() <= LogLevel::DEBUG) {
                LOG_DEBUG(L"VM_DEBUG: OP_CALL - Funcția apelată: " + funcName);
            }

            vData result;

            // 1. Executăm funcția nativă (dacă există)
            if (this->m_functionsHandlers.count(funcName)) {
                try {
                    result = this->m_functionsHandlers[funcName](callArgs);
                }
                catch (...) {
                    LOG_ERROR(L"VM: Crash in functia nativa apelata dinamic: " + funcName);
                    this->m_executionStatus = OliStatus::ERR;
                    return;
                }
            }
            // 2. Sau executăm funcția din Oli Bytecode
            else if (this->m_bytecodeFunctions.count(funcName)) {
                result = this->callUserByteCodeFunction(
                    funcName.c_str(),   // const wchar_t*
                    callArgs.data(),    // const vData*
                    callArgs.size(),    // size_t
                    vData()             // context implicit
                );
            }
            // 🔥 FIX CRITIC 1: Fallback pentru Constructorul Implicit de Structuri/Clase în VM
            else if (this->m_blueprints.count(funcName)) {
                auto& bp = this->m_blueprints[funcName];
                vDataMap mapObj = std::make_shared<std::unordered_map<std::wstring, vData>>();

                // Injectăm automat tipul în interiorul instanței instanțiate
                (*mapObj)[L"__type__"] = vData(bp.name);

                // Mapăm argumentele primite pe stivă la câmpurile definite în Blueprint
                for (size_t i = 0; i < bp.fields.size(); ++i) {
                    std::wstring fieldName = bp.fields[i];
                    if (i < callArgs.size()) {
                        (*mapObj)[fieldName] = callArgs[i];
                    }
                    else {
                        (*mapObj)[fieldName] = vData(); // NULL/Monostate implicit dacă lipsesc parametri
                    }
                }
                result = vData(mapObj);
            }
            else {
                LOG_ERROR(L"Runtime Error: Funcția '" + funcName + L"' nu este înregistrată (nici nativă, nici bytecode)!");
                this->m_executionStatus = OliStatus::ERR;
                return;
            }

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
            if (val.isPointer()) {
                typeStr = L"POINTER";
            }
            else {
                vData actual = val.getTrueData();
                if (actual.isInt())         typeStr = L"INT";
                else if (actual.isFloat())  typeStr = L"FLOAT";
                else if (actual.isBool())   typeStr = L"BOOL";
                else if (actual.isString()) typeStr = L"STRING";
                else if (actual.isArray())  typeStr = L"ARRAY";
                else if (actual.isMap()) {
                    // --- UPGRADE PENTRU DETECTARE CLASE NATIVE/DINAMICE ---
                    auto m = actual.rawMap();
                    if (m && m->count(L"__type__")) {
                        // Dacă are __type__, returnăm numele clasei cu litere mari (ex: WINWINDOW)
                        typeStr = to_upper((*m)[L"__type__"].toWString());
                    }
                    else {
                        typeStr = L"MAP"; // Rămâne MAP simplu dacă e doar un dicționar normal
                    }
                }
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

        // 1. Scoatem numele metodei de pe stivă
        vData methodNameData = stack.back(); stack.pop_back();
        std::wstring originalMethodName = methodNameData.toWString();
        std::wstring lowerMethodName = to_lower(originalMethodName);
        std::wstring upperMethodName = to_upper(originalMethodName);

        // 2. Scoatem OBIECTUL (Contextul)
        vData contextObjRaw = stack.back(); stack.pop_back();
        vData contextObj = contextObjRaw.getTrueData();

        if (ConsoleManager::getInstance().getLogLevel() <= LogLevel::DEBUG) {
            LOG_DEBUG(L"[VM] Apel metoda: " + originalMethodName + L" pe obiect de tip " + getVariantTypeName(contextObj));
        }

        if (contextObj.isMap()) {
            auto m = contextObj.rawMap();
            std::wstring finalFunc = L"";
            bool methodResolved = false;

            // --- PASUL A: Căutare în Blueprint-ul Static al Clasei ---
            if (m && m->count(L"__type__")) {
                std::wstring typeName = to_upper((*m)[L"__type__"].toWString());
                if (m_blueprints.count(typeName)) {
                    auto& bp = m_blueprints[typeName];
                    if (bp.methods.count(upperMethodName)) {
                        finalFunc = bp.methods[upperMethodName];
                        methodResolved = true;
                    }
                }
            }

            // --- PASUL B (🔥 FIX CRITIC): Fallback la Proprietăți Dinamice (Metode atașate la Runtime) ---
            if (!methodResolved && m) {
                std::wstring fieldKey = L"";
                if (m->count(originalMethodName)) fieldKey = originalMethodName;
                else if (m->count(lowerMethodName)) fieldKey = lowerMethodName;
                else if (m->count(upperMethodName)) fieldKey = upperMethodName;

                if (!fieldKey.empty()) {
                    vData fieldVal = (*m)[fieldKey].getTrueData();
                    if (fieldVal.isString()) {
                        finalFunc = fieldVal.toWString(); // Extragem string-ul (ex: "prt_linie")
                        methodResolved = true;
                    }
                }
            }

            // --- PASUL C: Executarea Funcției Găsite ---
            if (methodResolved) {
                std::wstring upperFinalFunc = to_upper(finalFunc); // Pentru lookup în handlerii înregistrați cu litere mari

                // Colectăm argumentele (LIFO)
                std::vector<vData> args(argCount);
                for (int i = argCount - 1; i >= 0; --i) {
                    args[i] = stack.back();
                    stack.pop_back();
                }

                // Interceptăm dacă e funcție Nativă C++ vs Funcție Oli Bytecode
                if (this->m_functionsHandlers.count(upperFinalFunc)) {
                    // Pasăm obiectul ca prim argument implicit ($this) pentru funcțiile native
                    args.insert(args.begin(), contextObj);
                    vData nativeResult = this->m_functionsHandlers[upperFinalFunc](args);
                    stack.push_back(nativeResult);
                    LOG_SUCCESS(L"[VM] Metoda NATIVA executata dinamic: " + finalFunc);
                }
                else if (this->m_bytecodeFunctions.count(upperFinalFunc)) {
                    // Apelăm funcția din Bytecode normalizată la caractere mari, pasând contextObj ca context de '$this'
                    vData result = this->callUserByteCodeFunction(
                        upperFinalFunc.c_str(),
                        args.data(),
                        args.size(),
                        contextObj
                    );
                    stack.push_back(result);
                    LOG_SUCCESS(L"[VM] Metoda SCRIPT executata dinamic: " + upperFinalFunc);
                }
                else {
                    // Fallback de siguranță cu numele original al funcției
                    vData result = this->callUserByteCodeFunction(
                        finalFunc.c_str(),
                        args.data(),
                        args.size(),
                        contextObj
                    );
                    stack.push_back(result);
                    LOG_SUCCESS(L"[VM] Metoda SCRIPT executata dinamic (nume brut): " + finalFunc);
                }

                break; // Ieșim cu succes din switch-ul principal pentru acest OpCode!
            }
        }

        // --- Eroare dacă nu s-a putut rezolva prin nicio metodă ---
        std::wstring actualType = getVariantTypeName(contextObj);
        LOG_ERROR(L"Runtime Error: Metoda '" + upperMethodName + L"' nu a putut fi rezolvata.");
        LOG_ERROR(L"Contextul primit ARE TIPUL: " + actualType + L" (Valoare: " + contextObj.toWString() + L")");

        this->m_executionStatus = OliStatus::ERR;
        break;
    }
    

    case OpCode::OP_SET_PTR: {
        // Stiva conține: [Valoare_Noua, Adresa/Numele_Variabilei] <- top
        if (stack.size() < 2) {
            LOG_ERROR(L"OP_SET_PTR Error: Stiva are prea putine elemente!");
            this->m_executionStatus = OliStatus::ERR;
            break;
        }

        vData ptrData = stack.back(); stack.pop_back();   // Extragem adresa (Pointer sau String)
        vData newValue = stack.back(); stack.pop_back(); // Extragem valoarea nouă

        // 1. Compatibilitate înapoi: Dacă este un pointer brut C++ (vData*)
        if (vData** addrPtr = std::get_if<vData*>(&ptrData.value)) {
            if (*addrPtr) {
                **addrPtr = newValue;
                if (ConsoleManager::getInstance().getLogLevel() <= LogLevel::DEBUG) {
                    LOG_DEBUG(L"[VM] Pointer brut Write SUCCESS: " + newValue.toWString());
                }
            }
            else {
                LOG_ERROR(L"Runtime Error: Încercare scriere prin Null Pointer brut.");
                this->m_executionStatus = OliStatus::ERR;
            }
        }
        // 2. 🔥 NOU: Suport pentru noua arhitectură dinamică bazată pe String-uri ("a")
        else if (ptrData.isString()) {
            std::wstring rawName = ptrData.toWString();
            std::wstring cleanName = this->cleanVariableName(rawName);

            bool assigned = false;

            // A. Încercăm să scriem în tabela de variabile globale
            if (m_globalVariables.count(cleanName)) {
                m_globalVariables[cleanName] = newValue;
                assigned = true;
            }
            // B. Altfel, căutăm variabila în parametrii locali ai funcției Bytecode curente
            else {
                for (auto const& [funcName, proc] : this->m_bytecodeFunctions) {
                    if (proc.compiledBody.get() == &chunk) {
                        bool isMethod = (funcName.find(L"::") != std::wstring::npos);
                        for (size_t i = 0; i < proc.params.size(); ++i) {
                            if (this->cleanVariableName(proc.params[i]) == cleanName) {
                                size_t slot = i + (isMethod ? 1 : 0);
                                if (framePtr + slot < stack.size()) {
                                    stack[framePtr + slot] = newValue;
                                    assigned = true;
                                }
                                break;
                            }
                        }
                        break;
                    }
                }
            }

            // C. Fallback: Dacă variabila nu a fost găsită (ex: pointer către o variabilă nouă), o creăm global
            if (!assigned) {
                m_globalVariables[cleanName] = newValue;
            }
        }
        else {
            LOG_ERROR(L"Runtime Error: Se astepta un Pointer sau String pentru OP_SET_PTR, dar s-a primit: " + getVariantTypeName(ptrData));
            this->m_executionStatus = OliStatus::ERR;
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

                            // --- OPERATORI BITWISE ---
        case OpCode::OP_BXOR: {
            if (stack.size() < 2) break;
            vData b = stack.back(); stack.pop_back();
            vData a = stack.back(); stack.pop_back();
            stack.push_back(vData{ static_cast<long long>(a.toInt() ^ b.toInt()) });
            break;
        }

        case OpCode::OP_BNOT: {
            if (stack.empty()) break;
            vData a = stack.back(); stack.pop_back();
            stack.push_back(vData{ static_cast<long long>(~a.toInt()) });
            break;
        }

        case OpCode::OP_SHL: {
            if (stack.size() < 2) break;
            vData b = stack.back(); stack.pop_back();
            vData a = stack.back(); stack.pop_back();
            stack.push_back(vData{ static_cast<long long>(a.toInt() << b.toInt()) });
            break;
        }

        case OpCode::OP_SHR: {
            if (stack.size() < 2) break;
            vData b = stack.back(); stack.pop_back();
            vData a = stack.back(); stack.pop_back();
            stack.push_back(vData{ static_cast<long long>(a.toInt() >> b.toInt()) });
            break;
        }
        case OpCode::OP_INC: {
            if (stack.empty()) break;
            vData a = stack.back(); stack.pop_back();
            if (a.isInt()) {
                stack.push_back(vData(a.toInt() + 1));
            }
            else {
                stack.push_back(vData(vDataToDouble(a) + 1.0));
            }
            break;
        }

        case OpCode::OP_DEC: {
            if (stack.empty()) break;
            vData a = stack.back(); stack.pop_back();
            if (a.isInt()) {
                stack.push_back(vData(a.toInt() - 1));
            }
            else {
                stack.push_back(vData(vDataToDouble(a) - 1.0));
            }
            break;
        }

        case OpCode::OP_LOGICAL_AND: {
            if (stack.size() < 2) break;
            vData b = stack.back(); stack.pop_back();
            vData a = stack.back(); stack.pop_back();
            stack.push_back(vData(vDataToBool(a) && vDataToBool(b)));
            break;
        }

        case OpCode::OP_LOGICAL_OR: {
            if (stack.size() < 2) break;
            vData b = stack.back(); stack.pop_back();
            vData a = stack.back(); stack.pop_back();
            stack.push_back(vData(vDataToBool(a) || vDataToBool(b)));
            break;
        }

        case OpCode::OP_NULL_COALESCE: {
            if (stack.size() < 2) break;
            vData b = stack.back(); stack.pop_back();
            vData a = stack.back(); stack.pop_back();

            // Dacă a este null/monostate, returnăm b, altfel rămâne a
            if (a.isNull() || std::holds_alternative<std::monostate>(a.value)) {
                stack.push_back(b);
            }
            else {
                stack.push_back(a);
            }
            break;
        }
        
        case OpCode::OP_CALL_DYNAMIC: {
            // 1. Extragem numărul de argumente din bytecode
            uint8_t argCount = chunk.code[ip++];

            if (stack.empty()) {
                LOG_ERROR(L"Runtime Error: Stiva este goala! Lipseste numele functiei pentru apelul dinamic.");
                this->m_executionStatus = OliStatus::ERR;
                break;
            }

            // 2. Extragem numele funcției (aflat în vârful stivei)
            vData funcVal = stack.back();
            stack.pop_back();

            std::wstring funcName = to_upper(funcVal.toWString());

            // 3. Extragem argumentele de pe stivă (Logica ta LIFO perfectă)
            std::vector<vData> args(argCount);
            for (int i = argCount - 1; i >= 0; --i) {
                if (stack.empty()) {
                    LOG_ERROR(L"Runtime Error: Stiva s-a golit prematur in timpul colectarii argumentelor pentru: " + funcName);
                    this->m_executionStatus = OliStatus::ERR;
                    break;
                }
                args[i] = stack.back();
                stack.pop_back();
            }
            if (this->m_executionStatus == OliStatus::ERR) break;

            vData result;

            // 4. 🔥 Rutare nativă direct prin map-ul clasei vOliEngine
            if (this->m_functionsHandlers.count(funcName)) {
                if (ConsoleManager::getInstance().getLogLevel() <= LogLevel::DEBUG) {
                    LOG_DEBUG(L"VM_DEBUG: OP_CALL_DYNAMIC (NATIV) - Funcția: " + funcName);
                }
                // Apelăm direct handlerul C++ înregistrat, trimițându-i vectorul de argumente
                result = this->m_functionsHandlers[funcName](args);
            }
            else {
                // Rutare către funcțiile utilizator (Bytecode)
                if (ConsoleManager::getInstance().getLogLevel() <= LogLevel::DEBUG) {
                    LOG_DEBUG(L"VM_DEBUG: OP_CALL_DYNAMIC (BYTECODE) - Funcția: " + funcName);
                }
                result = this->callUserByteCodeFunction(
                    funcName.c_str(),   // Transformă std::wstring în const wchar_t*
                    args.data(),        // Pointer brut către primul element (const vData*)
                    args.size(),        // Numărul de elemente (size_t)
                    vData()             // Contextul implicit rămâne neschimbat
                );
            }

            // 5. Împingem rezultatul înapoi pe stiva mașinii virtuale
            stack.push_back(result);
            break;
        }

        case OpCode::OP_HALT: {
            // Schimbăm starea instanței pentru a opri bucla instant
            this->m_executionStatus = OliStatus::RETURN_REQUESTED;
            return;
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
	 //ConsoleManager::getInstance().setMinLogLevel(LogLevel::DEBUG);
    LOG_INFO(L"=== internalLoadPlugin APELAT === Nume: " + pluginName);

    if (pluginName.empty()) {
        LOG_ERROR(L"EROARE: pluginName este gol!");
        return false;
    }

    // 1. Curățare ghilimele
    if (pluginName.size() >= 2 && pluginName.front() == L'"' && pluginName.back() == L'"') {
        pluginName = pluginName.substr(1, pluginName.size() - 2);
        LOG_INFO(L"Ghilimele curatate: " + pluginName);
    }

    // 2. Determinăm folderul executabilului principal (cross-platform)
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

#ifdef _WIN32
    LOG_INFO(L"Rezolutie cale: Exe=" + std::wstring(exePathBuffer) + L" | Dir=" + exeDir.wstring());
#else
    LOG_INFO(L"Rezolutie cale: Dir=" + exeDir.wstring());
#endif

    // 3. Extragem numele curat
    std::filesystem::path rawPath(pluginName);
    std::wstring pureName = rawPath.stem().wstring();
    LOG_INFO(L"Nume pur extras: " + pureName);

    // 4. Construim calea către subfolderul plugins
    std::filesystem::path finalDllPath = exeDir / "plugins" / pureName;
    std::wstring dllPath = finalDllPath.wstring();

    std::wstring ext = PortTools::getPluginExtension();
    if (dllPath.size() < ext.size() || dllPath.substr(dllPath.size() - ext.size()) != ext) {
        dllPath += ext;
    }

    LOG_INFO(L"Cale finala DLL: " + dllPath);

    // Verificare existență fișier
    if (std::filesystem::exists(dllPath)) {
        LOG_SUCCESS(L"Validare FS: DLL exista la locatie.");
    } else {
        LOG_ERROR(L"⚠️ ATENTIE: DLL NU EXISTA la calea: " + dllPath);
    }

    // 5. Încărcare bibliotecă
    LOG_INFO(L"Apel PortTools::loadDynamicLibrary...");
    PortTools::LibHandle hLib = PortTools::loadDynamicLibrary(dllPath);
    
    if (!hLib) {
        LOG_ERROR(L"❌ CRITICAL: Nu s-a putut incarca plugin-ul. Cod Eroare Win32: " + PortTools::getLastErrorString());
        return false;
    }

    LOG_SUCCESS(L"✅ DLL incarcat in memorie.");

    // Sincronizare consolă
    typedef void (*SetConsoleManagerFunc)(ConsoleManager*);
    SetConsoleManagerFunc setConsoleFn = (SetConsoleManagerFunc)PortTools::getFunctionSymbol(hLib, "SetPluginConsoleManager");
    
    if (setConsoleFn) {
        setConsoleFn(&ConsoleManager::getInstance());
        LOG_INFO(L"SetPluginConsoleManager apelat cu succes.");
    } else {
        LOG_ERROR(L"setConsoleFn lipseste! Plugin-ul nu poate partaja consola: " + pureName);
    }

    bool loadedAnything = false;

    // --- A. ÎNCĂRCARE FUNCȚII ---
    typedef void (*LoadFunctionsFunc)(std::unordered_map<std::wstring, OliFunctionHandler>&, void*);
    LoadFunctionsFunc regFuncs = (LoadFunctionsFunc)PortTools::getFunctionSymbol(hLib, "LoadOliPlugin");

    if (regFuncs) {
        LOG_INFO(L"Gasit LoadOliPlugin. Se injecteaza...");
        std::unordered_map<std::wstring, OliFunctionHandler> pluginFuncs;
        try {
            regFuncs(pluginFuncs, this);
            for (auto const& [name, handler] : pluginFuncs) {
                std::wstring upName = name;
                for (auto& c : upName) c = std::towupper(c);
                this->m_functionsHandlers[upName] = handler;
                vOliKeyWords::registerNativeFunction(upName);
                LOG_DEBUG(L"Injected: " + upName);
            }
            loadedAnything = true;
            LOG_SUCCESS(L"Functii native injectate.");
        }
        catch (...) { LOG_ERROR(L"Exception in LoadOliPlugin"); }
    } else {
        LOG_INFO(L"LoadOliPlugin nu a fost gasit (optional).");
    }

    // --- B. ÎNCĂRCARE COMENZI (Aliniat 100% cu interpretorul) ---
    LoadCommandsFunc regCmds = (LoadCommandsFunc)PortTools::getFunctionSymbol(hLib, "LoadOliCommandPlugin");

    if (regCmds) {
        LOG_INFO(L"Gasit LoadOliCommandPlugin. Se injecteaza direct...");
        try {
            // Trimitem direct map-ul principal al motorului, exact ca în handlePluginCommand!
            regCmds(this->m_commandHandlers, this);

            // Înregistrăm noile chei în vOliKeyWords
            for (auto const& [name, handler] : this->m_commandHandlers) {
                vOliKeyWords::registerDynamicCommand(name);
            }

            loadedAnything = true;
            LOG_SUCCESS(L"Comenzi injectate cu succes direct in structura VM.");
        }
        catch (...) {
            LOG_ERROR(L"Exception in LoadOliCommandPlugin");
        }
    }

    if (loadedAnything) {
        LOG_SUCCESS(L"Plugin '" + pureName + L"' operational.");
        return true;
    } else {
        LOG_ERROR(L"❌ EROARE: Niciun punct de export valid in " + dllPath);
        PortTools::freeDynamicLibrary(hLib);
        return false;
    }
}




vData vOliEngine::callUserByteCodeFunction(const wchar_t* funcName, const vData* argsArray, size_t argCount, vData context) {
    LOG_INFO(L"Sunt în callUserByteCodeFunction");
    std::wstring name(funcName);
    for (auto& c : name) c = std::towupper(c);
    std::vector<vData> args(argsArray, argsArray + argCount);

    LOG_INFO(L"[VM] Pregătire apel funcție: " + name + L" (" + std::to_wstring(args.size()) + L" argumente)");

    auto it = m_bytecodeFunctions.find(name);
    if (it == m_bytecodeFunctions.end()) {
        LOG_ERROR(L"-> EROARE: Funcția " + name + L" nu este înregistrată!");
        return vData();
    }

    const ByteCodeProcedure& func = it->second;
    size_t newFramePtr = this->m_stack.size();

    // Reținem dacă este o metodă nativă de clasă (compilată cu ::)
    bool isMethod = (name.find(L"::") != std::wstring::npos);

    if (isMethod) {
        this->m_stack.push_back(context);
    }

    // Maparea parametrilor standard ai funcției
    for (size_t i = 0; i < func.params.size(); ++i) {
        if (i < args.size()) {
            this->m_stack.push_back(args[i]);
        }
        else {
            this->m_stack.push_back(vData());
        }
    }

    if (!func.compiledBody) {
        LOG_ERROR(L"[VM] CRITIC: compiledBody este NULL pentru " + name);
        return vData();
    }

    OliStatus oldStatus = m_executionStatus;
    m_executionStatus = OliStatus::RUNNING;

    // 🔥 FIX CRITIC: Salvăm contextul curent pe stiva de metode înainte de execuție
    this->m_methodContextStack.push_back(context);

    this->executeBytecode(*(func.compiledBody), newFramePtr);

    // 🔥 FIX CRITIC: Eliminăm contextul după ce funcția și-a terminat execuția
    this->m_methodContextStack.pop_back();

    m_executionStatus = oldStatus;

    vData result = vData();
    if (this->m_executionStatus != OliStatus::ERR && !this->m_stack.empty()) {
        result = this->m_stack.back();
    }

    while (this->m_stack.size() > newFramePtr) {
        this->m_stack.pop_back();
    }

    LOG_SUCCESS(L"-> Funcția " + name + L" a returnat: " + result.toWString());
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