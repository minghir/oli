#include "vDataSerialize.hpp"
#include "OliEngine.hpp"
#include "PortTools.hpp"
#include "olic/OliBytecode.hpp"

#include <cmath>
/*
void vOliEngine::executeBytecode(const OliChunk& chunk) {
    this->m_executionStatus = OliStatus::RUNNING; //
    size_t ip = 0; // Instruction Pointer (unde ne aflăm în cod)
    std::vector<vData> stack; // Stiva pentru calcule

    while (ip < chunk.code.size()) {
        OpCode instruction = static_cast<OpCode>(chunk.code[ip++]);

        switch (instruction) {
        case OpCode::OP_CONSTANT: {
            // Citim indexul constantei (2 bytes)
            uint16_t idx = (chunk.code[ip] << 8) | chunk.code[ip + 1];
            ip += 2;
            stack.push_back(chunk.constants[idx]);
            break;
        }

        case OpCode::OP_SET_GLOBAL: {
            vData val = stack.back();
            stack.pop_back();

            uint16_t nameIdx = (chunk.code[ip] << 8) | chunk.code[ip + 1];
            ip += 2;

            // Folosim getTrueData() pentru a fi siguri că nu lucrăm cu un pointer la nume
            vData nameConst = chunk.constants[nameIdx].getTrueData();
            if (!nameConst.isString()) {
                this->logError(L"VM Error: OP_SET_GLOBAL target is not a string name.");
                break;
            }

            std::wstring varName = std::get<std::wstring>(nameConst.value);
            this->setVar(varName, val);
            break;
        }

        case OpCode::OP_ECHO: {
            if (stack.empty()) break;
            vData val = stack.back();
            stack.pop_back();

            // Acum vDataSerialize::stringify funcționează deoarece am creat namespace-ul
            std::wstring out = vDataSerialize::stringify(val);

            std::wcout << out << std::endl;
            std::wcout.flush(); // Siguranță pentru interfață interactivă
            break;
        }

        case OpCode::OP_RETURN:
            return;

        

     

        case OpCode::OP_GREATER: {
            vData b = stack.back(); stack.pop_back();
            vData a = stack.back(); stack.pop_back();

            // Convertim în double pentru o comparare universală (merge și pt int și pt float)
            double valA = a.isInt() ? (double)std::get<long long>(a.value) : std::get<double>(a.value);
            double valB = b.isInt() ? (double)std::get<long long>(b.value) : std::get<double>(b.value);

            // Împingem rezultatul logic (bool) înapoi pe stivă
            stack.push_back(vData(valA > valB));
            break;
        }

        case OpCode::OP_JUMP_IF_FALSE: {
            uint16_t offset = (chunk.code[ip] << 8) | chunk.code[ip + 1];
            ip += 2; // Trecem peste offset

            vData condition = stack.back();
            stack.pop_back();

            if (std::get<bool>(condition.value) == false) {
                ip += offset; // SĂRIM peste corpul IF-ului
            }
            break;
        }
        case OpCode::OP_JUMP: {
            uint16_t offset = (chunk.code[ip] << 8) | chunk.code[ip + 1];
            ip += 2;
            ip += offset; // Sarim mereu
            break;
        }
        case OpCode::OP_LOOP: {
            // Citim offset-ul de 2 bytes
            uint16_t offset = (chunk.code[ip] << 8) | chunk.code[ip + 1];
            ip += 2; // Trecem peste offset

            // Saltul înapoi: scădem offset-ul din Instruction Pointer
            ip -= offset;
            break;
        }

        case OpCode::OP_LESS: {
            vData b = stack.back(); stack.pop_back();
            vData a = stack.back(); stack.pop_back();
            double valA = a.isInt() ? (double)std::get<long long>(a.value) : std::get<double>(a.value);
            double valB = b.isInt() ? (double)std::get<long long>(b.value) : std::get<double>(b.value);
            stack.push_back(vData(valA < valB));
            break;
        }

        case OpCode::OP_EQUAL: {
            vData b = stack.back(); stack.pop_back();
            vData a = stack.back(); stack.pop_back();
            // Aici poți folosi direct operatorul de egalitate al vData dacă l-ai supraîncărcat
            stack.push_back(vData(a.toWString() == b.toWString()));
            break;
        }

        case OpCode::OP_SUB: {
            // Prima valoare scoasă de pe stivă este 'b' (scăzătorul)
            vData b = stack.back(); stack.pop_back();
            // A doua valoare scoasă este 'a' (descăzutul)
            vData a = stack.back(); stack.pop_back();

            LOG_DEBUG(L"SUB: " + a.toWString() + L" - " + b.toWString());

            if (a.isInt() && b.isInt()) {
                // Dacă ambii sunt întregi, rezultatul rămâne întreg
                stack.push_back(vData(std::get<long long>(a.value) - std::get<long long>(b.value)));
            }
            else {
                try {
                    // Conversie la double pentru operații cu virgulă sau mixte
                    double valA = a.isInt() ? (double)std::get<long long>(a.value) : std::get<double>(a.value);
                    double valB = b.isInt() ? (double)std::get<long long>(b.value) : std::get<double>(b.value);
                    stack.push_back(vData(valA - valB));
                }
                catch (...) {
                    this->logError(L"Crash la scădere! Tipuri incompatibile.");
                    return;
                }
            }
            break;
        }
        case OpCode::OP_DUP: {
            if (stack.empty()) {
                this->logError(L"Stack underflow la OP_DUP!");
                return;
            }
            // Punem pe stivă o copie a ultimului element
            stack.push_back(stack.back());
            break;
        }
        

        
        case OpCode::OP_JUMP_IF_TRUE: {
            // 1. Citim offset-ul (2 bytes)
            uint16_t offset = (uint16_t)((chunk.code[ip] << 8) | chunk.code[ip + 1]);
            ip += 2;

            // 2. Evaluăm condiția de pe stivă
            vData condition = stack.back();
            stack.pop_back();

            // 3. Dacă e TRUE, aplicăm saltul (ieșim din REPEAT)
            if (vDataToBool(condition)) {
                ip += offset;
            }
            break;
        }
        
        case OpCode::OP_GET_ADDR: {
            if (ip + 1 >= chunk.code.size()) return;
            uint16_t nameIdx = (chunk.code[ip] << 8) | chunk.code[ip + 1];
            ip += 2;

            std::wstring rawName = chunk.constants[nameIdx].toWString();
            std::wstring cleanName = this->cleanVariableName(rawName);

            vData* targetPtr = nullptr;

            // Căutăm în Stack-ul local (dacă există) sau în Globale
            if (!m_callStack.empty()) {
                auto& locals = m_callStack.back().localVariables;
                if (locals.count(cleanName)) targetPtr = &locals[cleanName];
            }

            if (!targetPtr) {
                if (m_globalVariables.count(cleanName)) {
                    targetPtr = &m_globalVariables[cleanName];
                }
                else {
                    // Auto-creare dacă variabila nu există
                    m_globalVariables[cleanName] = { std::monostate{} };
                    targetPtr = &m_globalVariables[cleanName];
                }
            }

            if (targetPtr) {
                vData addr;
                addr.value = targetPtr; // Stocăm pointerul C++ direct
                stack.push_back(addr);
            }
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
            if (valB == 0) {
                this->logError(L"Runtime Error: Division by zero!");
                return;
            }
            stack.push_back(vData(vDataToDouble(a) / valB));
            break;
        }

        case OpCode::OP_POW: {
            vData b = stack.back(); stack.pop_back();
            vData a = stack.back(); stack.pop_back();
            stack.push_back(vData(std::pow(vDataToDouble(a), vDataToDouble(b))));
            break;
        }

        case OpCode::OP_NULL_COALESCE: { // 0x20
            vData rhs = stack.back(); stack.pop_back();
            vData lhs = stack.back(); stack.pop_back();
            stack.push_back(!lhs.isNull() ? lhs : rhs);
            break;
        }

        
        
        case OpCode::OP_GET_GLOBAL: {
            if (ip + 1 >= chunk.code.size()) return;
            uint16_t nameIdx = (chunk.code[ip] << 8) | chunk.code[ip + 1];
            ip += 2;

            std::wstring varName = chunk.constants[nameIdx].toWString();

            // Obținem variabila (care în proiectul 'olish' poate fi un Map/Ierarhie)
            vData val = this->getVar(varName);

            // FIX: Folosim getScalarValue() pentru a trece de Map-ul ierarhiei 
            // și a pune pe stivă valoarea reală (sau pointerul real).
            stack.push_back(val.getFlattenedValue());
            break;
        }

        case OpCode::OP_ADD: {
            if (stack.size() < 2) {
                this->logError(L"Stack Underflow la OP_ADD!");
                return;
            }

            vData b = stack.back(); stack.pop_back();
            vData a = stack.back(); stack.pop_back();

            vData realA = a.getTrueData();
            vData realB = b.getTrueData();

            // --- MODIFICARE: Dacă unul dintre operanzi este String, Array sau Map, concatenăm textul ---
            if (realA.isString() || realB.isString() || realA.isArray() || realB.isArray() || realA.isMap() || realB.isMap()) {
                stack.push_back(vData(realA.toWString() + realB.toWString()));
            }
            else if (realA.isInt() && realB.isInt()) {
                stack.push_back(vData(std::get<long long>(realA.value) + std::get<long long>(realB.value)));
            }
            else {
                // Doar aici este permisă conversia la double (pentru matematică pură)
                double valA = vDataToDouble(realA);
                double valB = vDataToDouble(realB);
                stack.push_back(vData(valA + valB));
            }
            break;
        }

        case OpCode::OP_CONCAT: { // 0x22
            if (stack.size() < 2) {
                this->logError(L"Stack Underflow la OP_CONCAT!");
                return;
            }

            vData rhs = stack.back(); stack.pop_back();
            vData lhs = stack.back(); stack.pop_back();

            // toWString() rezolvă acum automat getTrueData() și recursivitatea pentru structuri.
            // Rezultatul va fi un string curat indiferent dacă ai numere, array-uri sau map-uri.
            stack.push_back(vData(lhs.toWString() + rhs.toWString()));
            break;
        }

        case OpCode::OP_GET_INDIRECT: {
            if (stack.empty()) {
                this->logError(L"Stack Underflow la OP_GET_INDIRECT!");
                return;
            }

            vData target = stack.back();
            stack.pop_back();

            // --- 1. PROTECȚIE ANTI-CRASH ($$$ptr pe valori scalare) ---
            // Dacă am ajuns la un număr, bool sau null, nu mai avem ce dereferenția.
            if (target.isInt() || target.isFloat() || target.isBool() || target.isNull()) {
                this->logError(L"Runtime Error: Nu se poate dereferenția o valoare scalară (" + target.toWString() + L")");
                this->m_executionStatus = OliStatus::ERR; // Oprim execuția pentru a preveni crash-ul
                return;
            }

            // --- 2. CAZ: POINTER DIRECT (vData*) ---
            if (target.isPointer()) {
                vData* ptr = std::get<vData*>(target.value);
                if (ptr) {
                    // Folosim getTrueData() pentru a naviga prin ierarhie, 
                    // dar NU folosim getScalarValue() pentru a nu pierde structura de Map/Array.
                    stack.push_back(ptr->getTrueData());
                }
                else {
                    stack.push_back(vData{ std::monostate{} });
                }
            }
            // --- 3. CAZ: INDIRAȚIE PRIN NUME (String, ex: "$a") ---
            else {
                // Obținem numele variabilei. toWString() este acum recursiv și safe.
                std::wstring varName = target.toWString();

                // getVar() returnează valoarea variabilei (care poate fi un alt string sau un pointer)
                vData val = this->getVar(varName);

                // Punem valoarea pe stivă. Dacă urmează un alt OP_GET_INDIRECT, 
                // procesul se va repeta (logica pentru $$$, $$$$, etc.)
                stack.push_back(val);
            }
            break;
        }

        case OpCode::OP_SET_INDIRECT: {
            if (stack.size() < 2) {
                this->logError(L"Stack Underflow la OP_SET_INDIRECT!");
                this->m_executionStatus = OliStatus::ERR; // Folosim noul status de eroare
                return;
            }

            // Ordinea pe stivă: [Valoare, Adresă/Nume]
            vData target = stack.back(); stack.pop_back();
            vData newValue = stack.back(); stack.pop_back();

            // 1. CAZ: POINTER DIRECT (vData*)
            if (target.isPointer()) {
                vData* ptr = std::get<vData*>(target.value);
                if (ptr) {
                    // Săpăm până la locația reală și salvăm valoarea brută
                    ptr->getTrueData().value = newValue.getTrueData().value;
                }
                else {
                    this->logError(L"Runtime Error: Încercare de scriere prin pointer null.");
                    this->m_executionStatus = OliStatus::ERR; //
                }
            }
            // 2. CAZ: INDIRAȚIE PRIN NUME ($$nume)
            else {
                // toWString() este acum recursiv și safe pentru orice tip de target
                std::wstring varName = target.toWString();

                if (varName.empty()) {
                    this->logError(L"Runtime Error: Nume de variabilă invalid pentru setare indirectă.");
                    this->m_executionStatus = OliStatus::ERR; //
                    return;
                }

                this->setVar(varName, newValue);
            }
            break;
        }

                                    // --- OPERAȚII DE STIVĂ ---
        case OpCode::OP_POP: {
            if (!stack.empty()) stack.pop_back();
            break;
        }

                           // --- ARITMETICĂ ---
        case OpCode::OP_MOD: {
            vData b = stack.back(); stack.pop_back();
            vData a = stack.back(); stack.pop_back();

            // Modulo funcționează de regulă pe întregi
            long long valA = a.getTrueData().toInt();
            long long valB = b.getTrueData().toInt();

            if (valB == 0) {
                this->logError(L"Runtime Error: Modulo by zero!");
                return;
            }
            stack.push_back(vData(valA % valB));
            break;
        }

        case OpCode::OP_NEGATE: {
            vData val = stack.back();
            stack.pop_back();

            if (val.isInt()) {
                stack.push_back(vData(-std::get<long long>(val.getTrueData().value)));
            }
            else {
                stack.push_back(vData(-vDataToDouble(val.getTrueData())));
            }
            break;
        }

                              // --- COMPARAȚII SUPLIMENTARE ---
        case OpCode::OP_NOT_EQUAL: {
            vData b = stack.back(); stack.pop_back();
            vData a = stack.back(); stack.pop_back();
            stack.push_back(vData(a.toWString() != b.toWString()));
            break;
        }

        case OpCode::OP_GREATER_EQUAL: {
            vData b = stack.back(); stack.pop_back();
            vData a = stack.back(); stack.pop_back();
            stack.push_back(vData(vDataToDouble(a.getTrueData()) >= vDataToDouble(b.getTrueData())));
            break;
        }

        case OpCode::OP_LESS_EQUAL: {
            vData b = stack.back(); stack.pop_back();
            vData a = stack.back(); stack.pop_back();
            stack.push_back(vData(vDataToDouble(a.getTrueData()) <= vDataToDouble(b.getTrueData())));
            break;
        }
                                  // --- În switch-ul din vOliEngine::executeBytecode ---

                          // 1. Bitwise AND (&)
        case OpCode::OP_BAND: {
            vData b = stack.back(); stack.pop_back();
            vData a = stack.back(); stack.pop_back();
            // Folosim toInt() pentru a asigura operația pe întregi
            stack.push_back(vData(a.getTrueData().toInt() & b.getTrueData().toInt()));
            break;
        }

                            // 2. Bitwise OR (|)
        case OpCode::OP_BOR: {
            vData b = stack.back(); stack.pop_back();
            vData a = stack.back(); stack.pop_back();
            stack.push_back(vData(a.getTrueData().toInt() | b.getTrueData().toInt()));
            break;
        }

                           // 3. Bitwise XOR (BXOR)
        case OpCode::OP_BXOR: {
            vData b = stack.back(); stack.pop_back();
            vData a = stack.back(); stack.pop_back();
            stack.push_back(vData(a.getTrueData().toInt() ^ b.getTrueData().toInt()));
            break;
        }

                            // 4. Bitwise NOT (~)
        case OpCode::OP_BNOT: {
            vData a = stack.back(); stack.pop_back();
            stack.push_back(vData(~a.getTrueData().toInt()));
            break;
        }

                            // 5. Shift Operations (<<, >>)
        case OpCode::OP_SHL: {
            vData b = stack.back(); stack.pop_back();
            vData a = stack.back(); stack.pop_back();
            stack.push_back(vData(a.getTrueData().toInt() << b.getTrueData().toInt()));
            break;
        }

        case OpCode::OP_SHR: {
            vData b = stack.back(); stack.pop_back();
            vData a = stack.back(); stack.pop_back();
            stack.push_back(vData(a.getTrueData().toInt() >> b.getTrueData().toInt()));
            break;
        }

                           // 6. Logical NOT (!)
        case OpCode::OP_LOGICAL_NOT: {
            vData a = stack.back(); stack.pop_back();
            // Invertim valoarea booleană
            stack.push_back(vData(!vDataToBool(a.getTrueData())));
            break;
        }
        case OpCode::OP_ARRAY: {
            // 1. Citim câte elemente trebuie să colectăm
            uint8_t count = chunk.code[ip++];

            // 2. Cream containerul pentru array (folosind vDataArray definit în proiectul tău)
            vDataArray elements = std::make_shared<std::vector<vData>>();
            elements->resize(count);

            // 3. Extragem elementele de pe stivă (în ordine inversă, deoarece stiva e LIFO)
            for (int i = (int)count - 1; i >= 0; --i) {
                (*elements)[i] = stack.back();
                stack.pop_back();
            }

            // 4. Punem obiectul Array înapoi pe stivă
            stack.push_back(vData(elements));
            break;
        }
        case OpCode::OP_MAP: {
            // 1. Citim numărul de perechi (key-value) ce trebuie colectate
            uint8_t pairCount = chunk.code[ip++];

            // 2. Cream containerul pentru Map (shared_ptr către unordered_map)
            vDataMap mapObj = std::make_shared<std::unordered_map<std::wstring, vData>>();

            // 3. Extragem perechile de pe stivă
            // Dacă compilatorul a pus: [Key1, Val1, Key2, Val2] -> pe stivă Val2 este în vârf
            for (int i = 0; i < (int)pairCount; ++i) {
                vData val = stack.back();
                stack.pop_back();

                vData keyData = stack.back();
                stack.pop_back();

                // Convertim cheia la string, asigurându-ne că rezolvăm eventualii pointeri
                std::wstring key = keyData.getTrueData().toWString();

                // Inserăm în map
                (*mapObj)[key] = val;
            }

            // 4. Punem obiectul Map înapoi pe stivă
            stack.push_back(vData(mapObj));
            break;
        }

        default:
            this->logError(L"Unknown OpCode in VM!");
            return;
        }

        if (this->m_executionStatus != OliStatus::RUNNING) { //
            if (this->m_executionStatus == OliStatus::ERR) {
                // Oprire imediată în caz de eroare critică
                return;
            }
            // Aici poți adăuga logica pentru BREAK_REQUESTED sau CONTINUE_REQUESTED
            // dacă ești în interiorul unei bucle OP_LOOP.
        }
    }
}
*/

void vOliEngine::executeBytecode(const OliChunk& chunk) {
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

        /*
        case OpCode::OP_GET_GLOBAL: {
            uint16_t nameIdx = (uint16_t)((chunk.code[ip] << 8) | chunk.code[ip + 1]);
            ip += 2;
            std::wstring varName = chunk.constants[nameIdx].toWString();

            vData entry = this->getVar(varName); // Aici primești { varName: data }

            // Verificăm dacă entry este un Map și conține cheia căutată
            if (entry.isMap()) {
                auto m = std::get<vDataMap>(entry.value);
                // Căutăm în map-ul ierarhiei valoarea reală
                if (m->count(varName)) {
                    stack.push_back((*m)[varName]); // Punem DOAR datele, fără nume
                }
                else {
                    stack.push_back(entry.getFlattenedValue()); // Fallback
                }
            }
            else {
                stack.push_back(entry);
            }
            break;
        }
        */

        case OpCode::OP_GET_GLOBAL: {
            uint16_t nameIdx = (uint16_t)((chunk.code[ip] << 8) | chunk.code[ip + 1]);
            ip += 2;
            std::wstring rawName = chunk.constants[nameIdx].toWString();

            // Curățăm numele (scoatem $ dacă există) pentru a potrivi cheile din Map
            std::wstring varName = (rawName[0] == L'$') ? rawName.substr(1) : rawName;

            vData val = this->getVar(rawName); // Căutăm cu numele original

            if (val.isMap()) {
                auto m = std::get<vDataMap>(val.value);
                // Căutăm în interiorul nodului folosind numele curat
                if (m->count(varName)) {
                    stack.push_back((*m)[varName]); // EXTRAGEM doar datele
                }
                else {
                    stack.push_back(val); // Fallback dacă nu e ierarhie standard
                }
            }
            else {
                stack.push_back(val);
            }
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

                                // --- 2. ARITMETICĂ ---
        /*
        case OpCode::OP_ADD: {
            vData b = stack.back(); stack.pop_back();
            vData a = stack.back(); stack.pop_back();
            vData rA = a.getTrueData(); vData rB = b.getTrueData();

            // IMPORTANT: Dacă avem obiecte complexe, forțăm concatenarea de string-uri
            if (rA.isString() || rB.isString() || rA.isArray() || rB.isArray() || rA.isMap() || rB.isMap()) {
                stack.push_back(vData(rA.toWString() + rB.toWString()));
            }
            else if (rA.isInt() && rB.isInt()) {
                stack.push_back(vData(std::get<long long>(rA.value) + std::get<long long>(rB.value)));
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

            vData rA = a.getTrueData();
            vData rB = b.getTrueData();

            // DACĂ avem Map sau Array, folosim obligatoriu toWString()
            if (rA.isMap() || rB.isMap() || rA.isArray() || rB.isArray() || rA.isString() || rB.isString()) {
                stack.push_back(vData(rA.toWString() + rB.toWString()));
            }
            else if (rA.isInt() && rB.isInt()) {
                stack.push_back(vData(std::get<long long>(rA.value) + std::get<long long>(rB.value)));
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
            uint16_t offset = (uint16_t)((chunk.code[ip] << 8) | chunk.code[ip + 1]);
            ip += 2; if (!vDataToBool(stack.back())) ip += offset;
            stack.pop_back(); break;
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

        
        // --- 7. POINTERI & INDIRAȚIE ---
        /*
        case OpCode::OP_GET_INDIRECT: {
            vData target = stack.back(); stack.pop_back();
            if (target.isInt() || target.isFloat() || target.isBool() || target.isNull()) {
                this->logError(L"Runtime Error: Nu se poate dereferenția o valoare scalară.");
                this->m_executionStatus = OliStatus::ERR; return;
            }
            if (target.isPointer()) {
                vData* ptr = std::get<vData*>(target.value);
                stack.push_back(ptr ? ptr->getTrueData() : vData{ std::monostate{} });
            }
            else {
                stack.push_back(this->getVar(target.toWString()));
            }
            break;
        }
        */
        case OpCode::OP_GET_INDIRECT: {
            if (stack.empty()) return;
            vData target = stack.back(); stack.pop_back();

            if (target.isPointer()) {
                stack.push_back(std::get<vData*>(target.value)->getTrueData());
            }
            else {
                std::wstring rawName = target.toWString();
                // Curățăm numele pentru lookup în ierarhie
                std::wstring varName = (rawName[0] == L'$') ? rawName.substr(1) : rawName;

                vData val = this->getVar(rawName);

                if (val.isMap()) {
                    auto m = std::get<vDataMap>(val.value);
                    if (m->count(varName)) {
                        stack.push_back((*m)[varName]);
                    }
                    else {
                        stack.push_back(val);
                    }
                }
                else {
                    stack.push_back(val);
                }
            }
            break;
        }
        
        // --- 8. SYSTEM & STRING ---
        case OpCode::OP_ECHO: {
            if (stack.empty()) break;
            vData val = stack.back(); stack.pop_back();
            std::wcout << vDataSerialize::stringify(val) << std::endl;
            std::wcout.flush(); break;
        }

        case OpCode::OP_CONCAT: {
            vData b = stack.back(); stack.pop_back();
            vData a = stack.back(); stack.pop_back();
            stack.push_back(vData(a.toWString() + b.toWString()));
            break;
        }

        case OpCode::OP_RETURN:
            this->m_executionStatus = OliStatus::RETURN_REQUESTED; return;

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
                this->logError(L"Runtime Error: Funcția '" + funcName + L"' nu a fost găsită.");
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

    OliChunk chunk;
    uint32_t constCount = 0;
    uint32_t codeSize = 0;

    // 1. Citim numărul de constante
    ifs.read(reinterpret_cast<char*>(&constCount), sizeof(constCount));

    // --- AICI DESERIALIZEZI ---
    // Reconstruim tabelul de constante citind fiecare vData din fișier
    for (uint32_t i = 0; i < constCount; ++i) {
        chunk.constants.push_back(vDataSerialize::deserializevData(ifs));
    }

    // 2. Citim dimensiunea codului
    ifs.read(reinterpret_cast<char*>(&codeSize), sizeof(codeSize));

    // 3. Citim codul binar
    chunk.code.resize(codeSize);
    ifs.read(reinterpret_cast<char*>(chunk.code.data()), codeSize);

    ifs.close();

    // 4. Executăm!
    this->executeBytecode(chunk);
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
    typedef void (*RegisterFunc)(std::unordered_map<std::wstring, OliFunctionHandler>&);
    RegisterFunc regFunc = (RegisterFunc)PortTools::getFunctionSymbol(hLib, "LoadOliPlugin");

    if (regFunc) {
        regFunc(this->m_functionsHandlers);
        LOG_SUCCESS(L"Functions injected from: " + dllPath);
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