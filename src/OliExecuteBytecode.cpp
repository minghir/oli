#include "vDataSerialize.hpp"
#include "OliEngine.hpp"
#include "olic/OliBytecode.hpp"

void vOliEngine::executeBytecode(const OliChunk& chunk) {
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

        case OpCode::OP_GET_GLOBAL: {
            uint16_t nameIdx = (chunk.code[ip] << 8) | chunk.code[ip + 1];
            ip += 2;

            std::wstring varName = std::get<std::wstring>(chunk.constants[nameIdx].getTrueData().value);
            vData val = this->getVar(varName);

            // FIX: Dacă valoarea returnată este o Mapă (ierarhie), 
            // înseamnă că am primit "containerul" variabilei, nu valoarea ei.
            if (val.isMap()) {
                auto* rawMap = val.rawMap();
                // Căutăm în mapă cheia care corespunde numelui variabilei (fără $)
                // Sau, dacă getVar ar trebui să returneze direct valoarea, verifică logica de acolo.

                // Dacă getVar-ul tău returnează mereu mapa părinte, extragem valoarea:
                std::wstring cleanName = varName;
                if (cleanName[0] == L'$') cleanName.erase(0, 1);

                if (rawMap && rawMap->count(cleanName)) {
                    val = (*rawMap)[cleanName];
                }
            }

            stack.push_back(val.getTrueData()); // Punem valoarea dereferențiată pe stivă
            break;
        }

        case OpCode::OP_SET_GLOBAL: {
            // 1. Valoarea este deja pe stivă (pusă de OP_CONSTANT anterior)
            vData val = stack.back();
            stack.pop_back();

            // 2. Citim indexul numelui (2 bytes)
            uint16_t nameIdx = (chunk.code[ip] << 8) | chunk.code[ip + 1];
            ip += 2; // ESENȚIAL: Sărim peste cei 2 octeți!

            std::wstring varName = std::get<std::wstring>(chunk.constants[nameIdx].getTrueData().value);
            this->setVar(varName, val);
            break;
        }

        case OpCode::OP_ECHO: {
            if (stack.empty()) return;
            vData val = stack.back();
            stack.pop_back();

            // Afișăm valoarea scalară (10 în loc de {a: 10})
            std::wcout << val.getScalarValue().toWString() << std::endl;
            break;
        }

        case OpCode::OP_RETURN:
            return;

        case OpCode::OP_ADD: {
            vData b = stack.back(); stack.pop_back();
            vData a = stack.back(); stack.pop_back();

            // Debug: să vedem ce încercăm să adunăm
            LOG_DEBUG(L"ADD: " + a.toWString() + L" + " + b.toWString());

            if (a.isInt() && b.isInt()) {
                stack.push_back(vData(std::get<long long>(a.value) + std::get<long long>(b.value)));
            }
            else {
                try {
                    double valA = a.isInt() ? (double)std::get<long long>(a.value) : std::get<double>(a.value);
                    double valB = b.isInt() ? (double)std::get<long long>(b.value) : std::get<double>(b.value);
                    stack.push_back(vData(valA + valB));
                }
                catch (...) {
                    this->logError(L"Crash la adunare! Tipuri incompatibile.");
                    return; // Oprim execuția grațios
                }
            }
            break;
        }

     

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
        case OpCode::OP_GET_INDIRECT: {
            if (stack.empty()) return;
            vData nameData = stack.back();
            stack.pop_back();

            // Folosim getScalarValue() pentru a obține "b" din {c: "b"}
            std::wstring varName = nameData.getScalarValue().toWString();

            if (!varName.empty() && varName[0] == L'$') varName = varName.substr(1);

            vData result = this->getVar(L"$" + varName);
            // Punem pe stivă rezultatul (care poate fi un alt Map/ierarhie)
            stack.push_back(result);
            break;
        }
        case OpCode::OP_SET_INDIRECT: {
            if (stack.size() < 2) {
                this->logError(L"Stack underflow la OP_SET_INDIRECT");
                return;
            }

            // 1. Scoatem numele (ținta)
            vData nameData = stack.back();
            stack.pop_back();

            // 2. Scoatem valoarea (ce scriem)
            vData newValue = stack.back();
            stack.pop_back();

            // 3. Extragem string-ul curat folosind logica ta de scalar
            std::wstring targetName = nameData.getScalarValue().toWString();

            if (!targetName.empty() && targetName[0] != L'$') {
                targetName = L"$" + targetName;
            }

            // 4. Executăm scrierea în sistemul de ierarhii existent
            this->setVar(targetName, newValue);
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

        default:
            this->logError(L"Unknown OpCode in VM!");
            return;
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
        chunk.constants.push_back(deserializevData(ifs));
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