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
/*
        case OpCode::OP_GET_GLOBAL: {
            uint16_t nameIdx = (uint16_t)((chunk.code[ip] << 8) | chunk.code[ip + 1]);
            ip += 2;
            std::wstring rawName = chunk.constants[nameIdx].toWString();

            // Curățăm numele: eliminăm $ sau @ pentru a găsi cheia în ierarhia ta
            std::wstring varName = rawName;
            if (!varName.empty() && (varName[0] == L'$' || varName[0] == L'@')) {
                varName = varName.substr(1);
            }

            // Căutăm variabila în motor (presupunând că getVar gestionează lookup-ul global)
            vData val = this->getVar(rawName);

            if (val.isMap()) {
                auto m = std::get<vDataMap>(val.value);
                // Dacă e o ierarhie de tip "a.a", extragem valoarea
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
            break;
        }
*/

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

            stack.push_back(val.getScalarValue());
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
        */
        /*
        case OpCode::OP_ADD: {
            vData b = stack.back(); stack.pop_back();
            vData a = stack.back(); stack.pop_back();

            vData rA = a.getScalarValue().getTrueData();
            vData rB = b.getScalarValue().getTrueData();

            LOG_DEBUG(L"[VM] OP_ADD: " + rA.toWString() + L" + " + rB.toWString());

            if (rA.isInt() && rB.isInt()) {
                long long res = rA.toInt() + rB.toInt();
                stack.push_back(vData(res));
                LOG_DEBUG(L"   -> Rezultat INT: " + std::to_wstring(res));
            }
            else {
                double res = vDataToDouble(rA) + vDataToDouble(rB);
                stack.push_back(vData(res));
                LOG_DEBUG(L"   -> Rezultat DOUBLE: " + std::to_wstring(res));
            }
            break;
        }
        */
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

        
        /*
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
        */

        case OpCode::OP_GET_INDIRECT: {
            if (stack.empty()) return;
            vData target = stack.back(); stack.pop_back();

            if (target.isPointer()) {
                stack.push_back(std::get<vData*>(target.value)->getScalarValue());
            }
            else {
                vData val = this->getVar(target.toWString());
                stack.push_back(val.getScalarValue());
            }
            break;
        }

        // --- 8. SYSTEM & STRING ---
        /*
        case OpCode::OP_ECHO: {
            if (stack.empty()) break;
            vData val = stack.back(); stack.pop_back();
            std::wcout << vDataSerialize::stringify(val) << std::endl;
            std::wcout.flush(); break;
        }
        */
        case OpCode::OP_ECHO: {
            if (stack.empty()) break;
            vData val = stack.back();
            stack.pop_back();

            // Folosim getScalarValue() pentru a „aplatiza” ierarhia înainte de afișare
            std::wcout << val.getScalarValue().toWString() << std::endl;
            std::wcout.flush();
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