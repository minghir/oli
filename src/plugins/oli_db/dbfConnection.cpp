#include "dbfConnection.hpp"
#include "stringUtils.hpp"
//#include "fileUtils.hpp"

#include "../../ConsoleManager.hpp" // Asigură-te că incluzi header-ul pentru logare
#include "SqlQueryEngine.hpp"

#include <ctime>
#include <iostream>
#include <filesystem>


dbfConnection::dbfConnection(const std::string& type, const std::wstring& dsn)
    : m_filePath(dsn) // dsn-ul este calea către fișier în cazul tău
{
    // Inițializări
    m_currentRowIndex = -1;
    m_error = L"";
    LOG_DEBUG(L"Instantiat dbfConnection pentru: " + dsn);
}


dbfConnection::~dbfConnection() {
    closeDatabase();
}

std::vector<std::string> dbfConnection::extractTableNames(const std::string& query) {
    std::vector<std::string> tableNames;
    std::istringstream stream(query);
    std::string word;
    bool foundFrom = false;

    while (stream >> word) {
        std::string lowerWord = word;
        std::transform(lowerWord.begin(), lowerWord.end(), lowerWord.begin(), ::tolower);

        if (lowerWord == "where")
            break;

        if (foundFrom) {
            // Începem să construim numele complet al tabelei (poate avea spații și ghilimele)
            std::string tableName = word;

            // Dacă începe cu ghilimele dar nu se termină cu ele — continuăm citirea
            if ((word.front() == '\'' || word.front() == '"') && word.back() != word.front()) {
                char quoteChar = word.front();
                while (stream >> word) {
                    tableName += " " + word;
                    if (!word.empty() && word.back() == quoteChar)
                        break;
                }
            }

            // Eliminăm ghilimelele și spațiile inutile
            tableName.erase(std::remove(tableName.begin(), tableName.end(), '\''), tableName.end());
            tableName.erase(std::remove(tableName.begin(), tableName.end(), '"'), tableName.end());
            //tableName.erase(std::remove_if(tableName.begin(), tableName.end(), ::isspace), tableName.end());

            tableNames.push_back(tableName);
            break;  // presupunem un singur nume de tabel (opțional: scoate această linie dacă vrei să suporte mai multe)
        }

        if (lowerWord == "from")
            foundFrom = true;
    }

    return tableNames;
}

bool dbfConnection::openDatabase() {
    if (std::filesystem::exists(m_filePath) && std::filesystem::is_directory(m_filePath)) {
        LOG_SUCCESS(L"DBF Directory opened: " + m_filePath);
        m_isConnected = true;
        return true;
    }
    m_error = L"Directorul nu exista: " + m_filePath;
    return false;
}


bool dbfConnection::fetchNextRow(std::string stm_name) {
    if (m_statements.find(stm_name) == m_statements.end()) {
        LOG_ERROR(L"STATEMENR UNKNOWN" + str_to_wstr(stm_name));
        return false; // Aici e buba dacă numele nu coincide
    }

    auto ctx = m_statements[stm_name];
    if (ctx->currentRowIndex + 1 < (int)ctx->dbfResult.size()) {
        ctx->currentRowIndex++;
        return true;
    }
    return false;
}

std::wstring dbfConnection::fetchFieldByNumber(int fieldNo, std::string stm_name) {
    auto it = m_statements.find(stm_name);
    if (it == m_statements.end()) return L"";

    auto ctx = it->second;
    if (fieldNo < 0 || fieldNo >= ctx->fields.size()) return L"";

    int offset = 1; // skip delete flag
    for (int i = 0; i < fieldNo; ++i) offset += ctx->fields[i].fieldLength;

    std::string rawVal(ctx->rowBuffer.data() + offset, ctx->fields[fieldNo].fieldLength);

    // Curățăm spațiile de la final (DBF pad-uiește cu spații)
    rawVal.erase(rawVal.find_last_not_of(" ") + 1);

    return str_to_wstr(rawVal);
}

std::map<std::wstring, std::wstring> dbfConnection::fetchMap(std::string stm_name) {
    if (m_statements.find(stm_name) == m_statements.end()) return {};

    auto ctx = m_statements[stm_name];
    if (ctx->currentRowIndex >= 0 && ctx->currentRowIndex < (int)ctx->dbfResult.size()) {
        return ctx->dbfResult[ctx->currentRowIndex];
    }
    return {};
}

std::vector<std::wstring> dbfConnection::fetchRow(std::string stm_name) {
    std::vector<std::wstring> rowData;
    auto ctx = m_statements[stm_name];

    if (ctx->dbfResult.empty()) return rowData;

    auto& currentMap = ctx->dbfResult[ctx->currentRowIndex];

    // DEBUG: Vedem ce e in Map vs ce cautam
    if (ctx->currentRowIndex == 0) { // printăm doar la primul rând
        std::wcout << L"Cautam coloanele din SELECT: ";
        for (auto& c : ctx->colNames) std::wcout << L"[" << c << L"] ";
        std::wcout << std::endl << L"In Map avem cheile: ";
        for (auto const& [key, val] : currentMap) std::wcout << L"[" << key << L"] ";
        std::wcout << std::endl;
    }

    for (const auto& colName : ctx->colNames) {
        // Folosim find pentru a fi siguri
        auto it = currentMap.find(colName);
        if (it != currentMap.end()) {
            rowData.push_back(it->second);
        }
        else {
            // Dacă nu găsește, punem un semn ca să știm care lipsește
            rowData.push_back(L"MISSING_" + colName);
        }
    }
    return rowData;
}

std::wstring dbfConnection::fetchFieldByName(const std::wstring& fieldName, std::string stm_name) {
    auto it = m_statements.find(stm_name);
    if (it == m_statements.end()) {
        LOG_ERROR(L"fetchFieldByName: Statement-ul '" + str_to_wstr(stm_name) + L"' nu a fost gasit.");
        return L"";
    }

    auto ctx = it->second;

    // Verificăm dacă avem un rând valid selectat
    if (ctx->currentRowIndex < 0 || ctx->currentRowIndex >= (int)ctx->dbfResult.size()) {
        return L"";
    }

    // Accesăm direct Map-ul rândului curent
    auto& row = ctx->dbfResult[ctx->currentRowIndex];

    // Căutare directă (O(log n) față de O(n) prin iterare)
    auto fieldIt = row.find(fieldName);
    if (fieldIt != row.end()) {
        return fieldIt->second;
    }

    // Dacă nu a fost găsit, încercăm o căutare case-insensitive (pentru siguranță)
    for (auto const& [key, val] : row) {
        if (wstrToLower(key) == wstrToLower(fieldName)) {
            return val;
        }
    }

    LOG_ERROR(L"Campul '" + fieldName + L"' nu a fost gasit in handle: " + str_to_wstr(stm_name));
    return L"";
}





bool dbfConnection::execQuery(const std::wstring& query, std::string stm_name) {
    try {

        // --- 0. RESETARE REZULTAT ANTERIOR ---
        m_lastResult = vConResult(); // Resetăm obiectul de rezultat
        if (m_statements.count(stm_name)) {
            m_statements.erase(stm_name); // Ștergem contextul vechi
        }

        // --- INTERCEPTARE DIRECTĂ PACK ---
        std::wstring trimmedQuery = wstr_trim(query);
        std::wstring upperQuery = to_upper(trimmedQuery);

        // --- BYPASS PENTRU DESCRIBE / DESC ---
        if (upperQuery.find(L"ALTER ") == 0 ) {
            return this->alterTable(trimmedQuery);
        }
        // --- BYPASS PENTRU DESCRIBE / DESC ---
        if (upperQuery.find(L"DESCRIBE ") == 0 || upperQuery.find(L"DESC ") == 0) {
            return this->describeTable(trimmedQuery, stm_name);
        }
        // --- BYPASS PENTRU SHOW TABLES ---
        if (upperQuery == L"SHOW TABLES") {
            return this->showTables(stm_name);
        }
        // --- BYPASS PENTRU DROP TABLE ---
        if (upperQuery.find(L"DROP TABLE") == 0) {
            return this->dropTable(trimmedQuery);
        }
        // --- BYPASS PENTRU COMENZI SPECIFICE FOXPRO ---
        if (upperQuery.find(L"CREATE TABLE") == 0) {
            bool success = this->createTable(trimmedQuery);
            if (success) {
                // Opțional: Creăm un context gol pentru a nu afișa nimic sau un mesaj de succes
                auto ctx = std::make_shared<TableContext>();
                m_statements[stm_name] = ctx;
            }
            return success;
        }
        if (upperQuery.find(L"PACK") == 0) {
            // Extragem numele tabelului (sărim peste primele 4 caractere "PACK" + spațiu)
            std::wstring tableName = wstr_trim(trimmedQuery.substr(4));

            // Curățăm eventualele ghilimele (ex: PACK 'persoane')
            if (!tableName.empty() && tableName.front() == L'\'')
                tableName = tableName.substr(1, tableName.size() - 2);

            return this->packTable(tableName);
        }

        SqlQueryParser parser(query);
        // .parse() ar trebui apelat dacă nu e deja în constructor
        if (!parser.parse()) {
            LOG_ERROR(parser.getLastError().message);
            return false;
        }

        // --- 1. Încărcare Univers ---
        auto participating = parser.getAllParticipatingTables();
        std::vector<vConTable> universe;

        for (const auto& qTable : participating) {
            universe.push_back(loadTable(qTable));
            //LOG_INFO(L"Tabel încărcat: " + qTable.name + L" (Alias: " + qTable.getEffectiveName() + L")");
        }

        if (universe.empty()) return false;

        // --- 2. Execuție Engine ---
        vSqlEngine engine(universe, parser);
        vConResult result = engine.execute(parser);
        m_lastResult = result;

        if (!result.success) return false;

        // --- 3. LOGICA NOUĂ: Dacă e INSERT/UPDATE/DELETE, salvăm rândurile noi/modificate ---
        if (parser.getQuery().type == QueryType::INSERT) {
            // Rezultatul unui INSERT în engine-ul nostru returnează în result.table 
            // DOAR rândurile noi care trebuie adăugate
            return appendRecords(parser.getQuery().fromTable.name, result.table);
        }
        if (parser.getQuery().type == QueryType::DELETE_ROWS && result.success) {
            if (this->deleteRecords(parser.getQuery().fromTable.name, result.affectedIndices)) {
                // Opțional: reîncărcăm tabelul dacă vrei să dispară imediat din memorie
                return true;
            }
        }
        else if (parser.getQuery().type == QueryType::UPDATE) {
            if (!result.success) return false;

            // Apelăm funcția care modifică DOAR rândurile vizate, nu rescrie tot fișierul
            // Trimitem: Nume tabel, Mapa cu modificări, și Structura coloanelor (pentru lungimi/tipuri)
            return this->updateRecords(
                parser.getQuery().fromTable.name,
                result.updatedRecordsMap,
                universe[0] // univers[0] este tabela target încărcată
            );
        }

        // --- 3. Mapare Rezultat (la fel ca înainte) ---
        auto ctx = std::make_shared<TableContext>();
        ctx->colNames = result.table.columns;
        for (const auto& record : result.table.records) {
            std::map<std::wstring, std::wstring> rowMap;
            for (size_t i = 0; i < result.table.columns.size(); ++i) {
                rowMap[result.table.columns[i]] = (i < record.size()) ? record[i] : L"";
            }
            ctx->dbfResult.push_back(rowMap);
        }

        m_statements[stm_name] = ctx;
        return true;

    }
    catch (const std::exception& e) {
        LOG_ERROR(L"Database Error: " + str_to_wstr(e.what()));
        return false;
    }
}

bool dbfConnection::isConnected() const {
    // O conexiune DBF este activa daca directorul radacina exista
    //return m_isConnected;
    return std::filesystem::exists(m_filePath) && std::filesystem::is_directory(m_filePath);
}

void dbfConnection::closeDatabase() {
    if (m_file.is_open()) {
        m_file.close();
        m_isConnected = false;
        //LOG_INFO(L"Fișierul DBF a fost închis: " + m_filePath);
    }
}

const std::vector<vNativeDataType> dbfConnection::getColumnTypes(std::string stm_name) {
    std::vector<vNativeDataType> universalTypes;

    // Luăm contextul pentru statement-ul cerut
    auto it = m_statements.find(stm_name);
    if (it == m_statements.end()) return universalTypes;
    auto ctx = it->second;

    for (const auto& field : ctx->fields) { // Folosim ctx->fields, nu m_fields!
        // field.fieldType este char (C, N, D, L, etc.)
        switch (field.fieldType) {
        case 'N': // Numeric
        case 'F': // Float
            if (field.decimalCount > 0) {
                universalTypes.push_back(vNativeDataType::V_DOUBLE);
            }
            else {
                // Dacă nu are zecimale, dBase îl folosește de obicei pentru Integer
                universalTypes.push_back(vNativeDataType::V_INTEGER);
            }
            break;

        case 'C': // Character
        case 'M': // Memo (Text lung)
            universalTypes.push_back(vNativeDataType::V_TEXT);
            break;

        case 'D': // Date (YYYYMMDD)
            universalTypes.push_back(vNativeDataType::V_DATE);
            break;

        case 'L': // Logical (T/F, Y/N)
            universalTypes.push_back(vNativeDataType::V_BOOLEAN);
            break;

        case 'I': // Integer (în versiuni mai noi de DBF)
            universalTypes.push_back(vNativeDataType::V_INTEGER);
            break;

        default:
            universalTypes.push_back(vNativeDataType::V_TEXT);
            break;
        }
    }

    return universalTypes;
}


const std::vector<vExternalColumnInfo> dbfConnection::getColumnsInfo(std::string stm_name) {
    std::vector<vExternalColumnInfo> infoList;

    // 1. Căutăm contextul statement-ului
    auto it = m_statements.find(stm_name);
    if (it == m_statements.end()) {
        LOG_ERROR(L"getColumnsInfo: Statement-ul '" + str_to_wstr(stm_name) + L"' nu a fost găsit.");
        return infoList;
    }

    auto ctx = it->second;

    // 2. Obținem tipurile (getColumnTypes este deja actualizat să folosească stm_name)
    const auto& types = getColumnTypes(stm_name);

    // 3. Iterăm prin descriptorii din contextul specific (ctx->fields)
    for (size_t i = 0; i < ctx->fields.size(); ++i) {
        vExternalColumnInfo info;

        // Folosim datele din ctx, nu din membrii clasei
        info.name = ctx->colNames[i];
        info.type = types[i];
        info.length = (int)ctx->fields[i].fieldLength;
        info.precision = (int)ctx->fields[i].decimalCount;
        info.isNullable = false; // DBF-ul clasic nu suportă nativ NULL în acest format

        infoList.push_back(info);
    }

    return infoList;
}

const std::vector<std::wstring>& dbfConnection::getColumnNames(std::string stm_name) {
    // 1. Căutăm contextul statement-ului
    auto it = m_statements.find(stm_name);

    if (it != m_statements.end()) {
        // Returnăm referința la vectorul de nume din contextul găsit
        return it->second->colNames;
    }

    // 2. Fallback: Dacă statement-ul nu există, returnăm un vector gol static
    // (Asta previne crash-ul și respectă semnătura care cere o referință)
    static const std::vector<std::wstring> emptyVec;
    LOG_ERROR(L"getColumnNames: Statement-ul '" + str_to_wstr(stm_name) + L"' nu a fost găsit.");
    return emptyVec;
}

bool dbfConnection::saveFile(const std::wstring& filename, const vConTable& table) {
    std::ofstream fs(std::filesystem::path(filename), std::ios::binary);
    //std::ofstream fs(filename, std::ios::binary);
    if (!fs.is_open()) return false;

    // 1. Pregătire Header
    DBF_Header header = { 0 };
    header.version = 0x03;
    // 0x03 este pentru Windows-1252 (Standard ANSI)
    // 0x64 (100) este pentru Windows-1250 (Central European - util pentru diacritice românești)
    header.languageDriver = 0x64;

    // 1. Obținem timpul curent
    std::time_t t = std::time(nullptr);
    std::tm now;
    // Folosim varianta safe pentru Visual Studio (localtime_s)
    localtime_s(&now, &t);

    // 2. Mapăm pe header-ul DBF
    // DBF stochează anul ca diferență față de 1900 (YY)
    // now.tm_year pentru 2026 este 126. Majoritatea implementărilor DBF 
    // folosesc modulo 100 pentru a scrie doar ultimele două cifre (26).
    header.lastUpdate[0] = static_cast<uint8_t>(now.tm_year % 100);
    header.lastUpdate[1] = static_cast<uint8_t>(now.tm_mon + 1); // tm_mon este 0-11
    header.lastUpdate[2] = static_cast<uint8_t>(now.tm_mday);

    header.numRecords = (uint32_t)table.records.size();

    // headerLength = 32 (header) + 32 * nr_coloane + 1 (terminator 0x0D)
    header.headerLength = (uint16_t)(32 + (32 * table.columns.size()) + 1);

    // recordLength = 1 (delete flag) + suma lățimilor coloanelor
    uint16_t recLen = 1;
    for (int w : table.columnWidths) recLen += (uint16_t)w;
    header.recordLength = recLen;

    fs.write(reinterpret_cast<char*>(&header), sizeof(DBF_Header));

    // 2. Scriem Descriptorii de câmpuri
    for (size_t i = 0; i < table.columns.size(); ++i) {
        DBF_FieldDescriptor fd = { 0 };

        // Numele coloanei (max 11 chars)
        std::string colName = wstr_to_str(table.columns[i]);
        strncpy_s(fd.fieldName, 11, colName.c_str(), _TRUNCATE);

        // Tipul ('C', 'N', etc.)
        fd.fieldType = (char)table.columnTypes[i][0];
        fd.fieldLength = (uint8_t)table.columnWidths[i];

        // Scriem zecimalele la Byte 17 al descriptorului
        if (i < table.columnDecimals.size()) {
            fd.decimalCount = (uint8_t)table.columnDecimals[i];
            // DIAGNOSTIC:
            LOG_DEBUG(L"Col: " + table.columns[i] + L" | Dec in Table: " + std::to_wstring(table.columnDecimals[i]));
            LOG_DEBUG(L"Col: " + table.columns[i] + L" | Dec in Struct: " + std::to_wstring((int)fd.decimalCount));
        }
        else {
            fd.decimalCount = 0;
        }

        fs.write(reinterpret_cast<char*>(&fd), sizeof(DBF_FieldDescriptor));
    }

    // Terminator descriptori
    char term = 0x0D;
    fs.write(&term, 1);

    // 3. Scriem Datele (Records)
    for (const auto& row : table.records) {
        fs.put(' '); // Flag pentru rând activ
        for (size_t i = 0; i < row.size(); ++i) {
            std::string formatted = formatFieldForDbf(row[i], table.columnWidths[i], table.columnTypes[i]);
            fs.write(formatted.c_str(), table.columnWidths[i]);
        }
    }

    // EOF marker
    char eof = 0x1A;
    fs.write(&eof, 1);

    fs.close();
    return true;
}
/*
std::string dbfConnection::formatFieldForDbf(const std::wstring& val, int width, const std::wstring& type) {
    std::string s = wstr_to_str(val);

    // Constrângere strictă la lățimea coloanei
    if (s.length() > (size_t)width) {
        return s.substr(0, width);
    }

    if (type == L"D") {
        // Dacă e dată, tăiem sau completăm la 8 caractere fără spații (standard DBF)
        if (s.length() > 8) return s.substr(0, 8);
        return s + std::string(8 - s.length(), '0');
    }

    if (type == L"N") {
        // Numerele: aliniate la dreapta, restul spații în stânga
        return std::string(width - s.length(), ' ') + s;
    }
    else {
        // Text: aliniat la stânga, restul spații în dreapta
        return s + std::string(width - s.length(), ' ');
    }
}
*/

std::string dbfConnection::formatFieldForDbf(const std::wstring& val, int width, const std::wstring& type) {
    std::string s = wstr_to_str(val);

    // 1. Constrângere strictă la lățimea coloanei (Trunchiere)
    if (s.length() > (size_t)width) {
        return s.substr(0, width);
    }

    // 2. Formatare specifică pe tipuri
    if (type == L"D") {
        // Standardul DBF pentru Date este YYYYMMDD (exact 8 caractere)
        // Dacă e goală sau incompletă, FoxPro preferă spații în loc de '0'
        if (s.empty()) return std::string(8, ' ');

        // Dacă e mai scurtă, completăm cu spații la dreapta până la 8
        if (s.length() < 8) return s + std::string(8 - s.length(), ' ');

        return s.substr(0, 8); // Ne asigurăm că returnăm fix 8
    }

    if (type == L"N" || type == L"F") { // N = Numeric, F = Float
        // Numerele: Aliniate la DREAPTA (important pentru sortare în FoxPro)
        return std::string(width - s.length(), ' ') + s;
    }
    else {
        // Character ('C') și altele: Aliniate la STÂNGA
        return s + std::string(width - s.length(), ' ');
    }
}

bool dbfConnection::appendRecords(const std::wstring& tableName, const vConTable& dataToInsert) {
    std::wstring fileName = ensureExtension(tableName, L".dbf");
    std::wstring fullPath = m_filePath + (m_filePath.back() == L'\\' ? L"" : L"\\") + fileName;

    // 1. Backup cu noul sistem (Timestamp: YYYYMMDD_HHMMSS)
    createBackup(fullPath);

    // 2. Deschidere fișier
    std::fstream fs(std::filesystem::path(fullPath), std::ios::binary | std::ios::in | std::ios::out);
    //std::fstream fs(fullPath, std::ios::binary | std::ios::in | std::ios::out);
    if (!fs.is_open()) {
        LOG_ERROR(L"Append failed: Nu s-a putut deschide fișierul " + fullPath);
        return false;
    }

    // 3. Citire Header
    DBF_Header header;
    if (!fs.read(reinterpret_cast<char*>(&header), sizeof(DBF_Header))) {
        LOG_ERROR(L"Eroare critică: Citirea header-ului a eșuat complet.");
        return false;
    }

    // --- LOGURI DE DEBUG PENTRU INVESTIGAȚIE ---
    LOG_DEBUG(L"--- [ DEBUG HEADER " + fileName + L" ] ---");
    LOG_DEBUG(L"Version byte: " + std::to_wstring(header.version) + L" (hex: 0x03?)");
    LOG_DEBUG(L"Records in file: " + std::to_wstring(header.numRecords));
    LOG_DEBUG(L"Header Length: " + std::to_wstring(header.headerLength));
    LOG_DEBUG(L"Record Length: " + std::to_wstring(header.recordLength));
    LOG_DEBUG(L"-----------------------------------------");

    // 4. Verificare validitate (Garda de siguranță)
    // Verificăm version != 3 dar și dacă headerLength este suspect de mică
   // 0x03 = dBase III, 0x30 = Visual FoxPro
    if (header.version != 0x03 && header.version != 0x30) {
        LOG_ERROR(L"Versiune DBF neacceptată: " + std::to_wstring(header.version) + L". (Se aștepta 3 sau 48)");
        return false;
    }

    // 5. Poziționare la finalul datelor (calculat)
    uint32_t writePos = header.headerLength + (header.numRecords * header.recordLength);
    fs.seekp(writePos, std::ios::beg);

    if (fs.fail()) {
        LOG_ERROR(L"Eroare: Seekp la poziția " + std::to_wstring(writePos) + L" a eșuat.");
        return false;
    }

    // 6. Scriem rândurile noi
    for (const auto& row : dataToInsert.records) {
        fs.put(' '); // Active record flag

        for (size_t i = 0; i < row.size(); ++i) {
            if (i >= dataToInsert.columnWidths.size()) break;

            std::string formatted = formatFieldForDbf(row[i], dataToInsert.columnWidths[i], dataToInsert.columnTypes[i]);
            fs.write(formatted.c_str(), dataToInsert.columnWidths[i]);
        }
    }

    // 7. Terminator EOF (0x1A)
    fs.put(0x1A);

    // 8. Actualizăm Header-ul în memorie
    header.numRecords += (uint32_t)dataToInsert.records.size();

    std::time_t t = std::time(nullptr);
    std::tm now;
    localtime_s(&now, &t);
    header.lastUpdate[0] = (uint8_t)(now.tm_year % 100);
    header.lastUpdate[1] = (uint8_t)(now.tm_mon + 1);
    header.lastUpdate[2] = (uint8_t)now.tm_mday;

    // 9. Salvăm Header-ul actualizat pe disc
    fs.seekp(0, std::ios::beg);
    fs.write(reinterpret_cast<char*>(&header), sizeof(DBF_Header));

    fs.flush(); // Ne asigurăm că datele sunt scrise fizic
    fs.close();

    LOG_SUCCESS(L"Insert finalizat cu succes în " + tableName);
    return true;
}

bool dbfConnection::deleteRecords(const std::wstring& tableName, const std::vector<int>& indices) {
    if (indices.empty()) return true;

    std::wstring fileName = ensureExtension(tableName, L".dbf");
    std::wstring fullPath = m_filePath + (m_filePath.back() == L'\\' ? L"" : L"\\") + fileName;

    // 1. Backup cu timestamp (folosim funcția creată anterior)
    createBackup(fullPath);

    // 2. Deschidem fișierul pentru citire/scriere binară
    std::fstream fs(std::filesystem::path(fullPath), std::ios::binary | std::ios::in | std::ios::out);
    //std::fstream fs(fullPath, std::ios::binary | std::ios::in | std::ios::out);
    if (!fs.is_open()) {
        LOG_ERROR(L"Delete failed: Nu s-a putut deschide fișierul " + fullPath);
        return false;
    }

    // 3. Citire și Validare Header
    DBF_Header header;
    if (!fs.read(reinterpret_cast<char*>(&header), sizeof(DBF_Header))) {
        LOG_ERROR(L"Eroare critică: Citirea header-ului a eșuat la DELETE.");
        return false;
    }

    // Log de debug pentru a monitoriza ce se întâmplă
    LOG_DEBUG(L"--- [ DELETE DEBUG: " + fileName + L" ] ---");
    LOG_DEBUG(L"Version: " + std::to_wstring(header.version) + L" | Records: " + std::to_wstring(header.numRecords));

    // 4. Garda de siguranță (0x03 = dBase III, 0x30 = FoxPro)
    if (header.version != 0x03 && header.version != 0x30) {
        LOG_ERROR(L"Eroare: Versiune DBF invalidă pentru DELETE (" + std::to_wstring(header.version) + L").");
        return false;
    }

    // 5. Marcare rânduri ca șterse
    int deletedCount = 0;
    for (int idx : indices) {
        // Validăm indexul rândului față de ce scrie în header
        if (idx < 0 || idx >= (int)header.numRecords) {
            LOG_DEBUG(L"Avertisment: Index " + std::to_wstring(idx) + L" în afara limitelor. Ignorat.");
            continue;
        }

        // Calculăm poziția byte-ului de flag (primul byte al fiecărui record)
        uint32_t recordPos = header.headerLength + (idx * header.recordLength);

        fs.seekp(recordPos, std::ios::beg);
        if (!fs.fail()) {
            fs.put('*'); // '*' în primul byte înseamnă "soft delete" în standardul DBF
            deletedCount++;
        }
    }

    // 6. Finalizare
    fs.flush();
    fs.close();

    LOG_SUCCESS(L"Delete finalizat. Rânduri marcate pentru ștergere: " + std::to_wstring(deletedCount));
    return true;
}

bool dbfConnection::updateRecords(const std::wstring& tableName, const std::map<int, std::vector<std::wstring>>& updates, const vConTable& tableMeta) {
    if (updates.empty()) return true;

    std::wstring fileName = ensureExtension(tableName, L".dbf");
    std::wstring fullPath = m_filePath + (m_filePath.back() == L'\\' ? L"" : L"\\") + fileName;

    // 1. Creăm backup-ul cu timestamp înainte de orice modificare
    createBackup(fullPath);

    // 2. Deschidere fișier în mod binar in/out
  //  std::fstream fs(fullPath, std::ios::binary | std::ios::in | std::ios::out);
    std::fstream fs(std::filesystem::path(fullPath), std::ios::binary | std::ios::in | std::ios::out);
    if (!fs.is_open()) {
        LOG_ERROR(L"Update failed: Nu s-a putut deschide fișierul " + fullPath);
        return false;
    }

    // 3. Citire și Validare Header
    DBF_Header header;
    if (!fs.read(reinterpret_cast<char*>(&header), sizeof(DBF_Header))) {
        LOG_ERROR(L"Eroare critică: Nu s-a putut citi header-ul pentru update.");
        return false;
    }

    // DEBUG pentru a monitoriza versiunea (FoxPro 0x30 = 48)
    LOG_DEBUG(L"--- [ UPDATE DEBUG: " + fileName + L" ] ---");
    LOG_DEBUG(L"Version: " + std::to_wstring(header.version) + L" | Records: " + std::to_wstring(header.numRecords));

    // 4. Garda de siguranță (Permitem 0x03 dBase și 0x30 FoxPro)
    if (header.version != 0x03 && header.version != 0x30) {
        LOG_ERROR(L"Update abortat: Versiune DBF neacceptată (" + std::to_wstring(header.version) + L").");
        return false;
    }

    // Verificăm integritatea structurii
    if (header.headerLength == 0 || header.recordLength == 0) {
        LOG_ERROR(L"Update failed: Structura header-ului indică lungimi nule.");
        return false;
    }

    // 5. Procesăm update-urile
    int successCount = 0;
    for (auto const& [idx, newRow] : updates) {
        // Validare index rând
        if (idx < 0 || idx >= (int)header.numRecords) {
            LOG_DEBUG(L"Update Skip: Index " + std::to_wstring(idx) + L" este în afara limitelor.");
            continue;
        }

        // Calculăm offset-ul rândului
        uint32_t recordPos = header.headerLength + (idx * header.recordLength);

        fs.seekp(recordPos, std::ios::beg);
        if (fs.fail()) {
            LOG_ERROR(L"Seek failed la indexul " + std::to_wstring(idx));
            continue;
        }

        // Scriem flag-ul de rând activ (spațiu)
        fs.put(' ');

        // Scriem datele formatate pentru fiecare coloană
        for (size_t i = 0; i < newRow.size(); ++i) {
            // Ne oprim dacă rândul primit are mai multe coloane decât structura tabelului
            if (i >= tableMeta.columnWidths.size()) break;

            std::string formatted = formatFieldForDbf(newRow[i], tableMeta.columnWidths[i], tableMeta.columnTypes[i]);
            fs.write(formatted.c_str(), tableMeta.columnWidths[i]);
        }
        successCount++;
    }

    // 6. Actualizăm data ultimei modificări în header
    std::time_t t = std::time(nullptr);
    std::tm now;
    localtime_s(&now, &t);
    header.lastUpdate[0] = (uint8_t)(now.tm_year % 100);
    header.lastUpdate[1] = (uint8_t)(now.tm_mon + 1);
    header.lastUpdate[2] = (uint8_t)now.tm_mday;

    fs.seekp(0, std::ios::beg);
    fs.write(reinterpret_cast<char*>(&header), sizeof(DBF_Header));

    fs.flush();
    fs.close();

    LOG_SUCCESS(L"Update finalizat cu succes în " + tableName + L" (" + std::to_wstring(successCount) + L" rânduri)");
    return true;
}

/*
vConTable dbfConnection::loadTable(const QueryTable& tableInfo) {
    vConTable table;

    // 1. Pregătim numele și calea
    std::wstring fileName = ensureExtension(tableInfo.name, L".dbf");
    std::wstring fullPath = m_filePath + (m_filePath.back() == L'\\' ? L"" : L"\\") + fileName;

    std::ifstream file(fullPath, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Nu s-a putut deschide tabela: " + wstr_to_str(fullPath));
    }

    // --- Citire Header ---
    DBF_Header header;
    file.read(reinterpret_cast<char*>(&header), sizeof(DBF_Header));

    // --- Citire Descriptori coloane ---
    std::vector<DBF_FieldDescriptor> fields;
    while (file.good()) {
        char nextByte = file.peek();
        if (nextByte == 0x0D) break; // Finalul listei de descriptori
        DBF_FieldDescriptor fd;
        file.read(reinterpret_cast<char*>(&fd), sizeof(DBF_FieldDescriptor));
        fields.push_back(fd);
    }
    file.seekg(header.headerLength, std::ios::beg);

    // --- Configurare vConTable ---
    table.tableName = fileName;
    table.tableAlias = tableInfo.alias;

    for (const auto& field : fields) {
        // Numele coloanei
        std::string rawName(field.fieldName, 11);
        std::wstring cleanName = to_upper(wstr_trim(str_to_wstr(rawName.c_str())));
        table.columns.push_back(cleanName);

        // Tipul coloanei (C, N, D, etc.)
        table.columnTypes.push_back(std::wstring(1, (wchar_t)field.fieldType));

        // AICI POPULĂM columnWidths - lungimea fizică în bytes
        table.columnWidths.push_back((int)field.fieldLength);
    }
    
    // --- Citire Date ---
    std::vector<char> rowBuffer(header.recordLength);
    for (int i = 0; i < (int)header.numRecords; ++i) {
        if (!file.read(rowBuffer.data(), header.recordLength)) break;

        // VALIDARE STRICTĂ: Un rând DBF valid începe DOAR cu ' ' (activ) sau '*' (șters)
        // Dacă rândul începe cu orice altceva (ca la 6ION Marian), înseamnă că datele sunt decalate.
        if (rowBuffer[0] != ' ' && rowBuffer[0] != '*') {
            LOG_DEBUG(L"Aliniere gresita detectata la randul " + std::to_wstring(i));
            // Aici poti decide daca sari peste el sau incerci sa te realiniezi
            continue;
        }
        
        if (rowBuffer[0] == '*') {
            continue; // Ignorăm înregistrările șterse
        }
        
        // Dacă vrei să fii extra-safe, poți verifica dacă rowBuffer[0] != ' ' 
        // dar deocamdată e suficient să filtrăm '*'

        std::vector<std::wstring> rowRecord;
        size_t offset = 1;
        for (const auto& field : fields) {
            std::string val(rowBuffer.data() + offset, field.fieldLength);
            rowRecord.push_back(wstr_trim(str_to_wstr(val)));
            offset += field.fieldLength;
        }
        table.records.push_back(std::move(rowRecord));
    }
    file.close();
    return table;
}
*/

vConTable dbfConnection::loadTable(const QueryTable& tableInfo) {
    vConTable table;

    // 1. Pregătim numele și calea
    std::wstring fileName = ensureExtension(tableInfo.name, L".dbf");
    std::wstring fullPath = m_filePath + (m_filePath.back() == L'\\' ? L"" : L"\\") + fileName;

    std::ifstream file{ std::filesystem::path(fullPath), std::ios::binary };
    //std::fstream file(std::filesystem::path(fullPath), std::ios::binary );
    //std::ifstream file(fullPath, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Nu s-a putut deschide tabela: " + wstr_to_str(fullPath));
    }

    // --- Citire Header ---
    DBF_Header header;
    file.read(reinterpret_cast<char*>(&header), sizeof(DBF_Header));

    // --- Citire Descriptori coloane ---
    std::vector<DBF_FieldDescriptor> fields;
    while (file.good()) {
        char nextByte = file.peek();
        if (nextByte == 0x0D) break; // Finalul listei de descriptori

        DBF_FieldDescriptor fd;
        file.read(reinterpret_cast<char*>(&fd), sizeof(DBF_FieldDescriptor));
        fields.push_back(fd);
    }

    // Ne mutăm la începutul datelor folosind headerLength (ignoram orice bytes extra intre descriptori si date)
    file.seekg(header.headerLength, std::ios::beg);

    // --- Configurare vConTable ---
    table.tableName = fileName;
    table.tableAlias = tableInfo.alias;

    for (const auto& field : fields) {
        // 1. Numele coloanei
        std::string rawName(field.fieldName, 11);
        std::wstring cleanName = to_upper(wstr_trim(str_to_wstr(rawName.c_str())));
        table.columns.push_back(cleanName);

        // 2. Tipul coloanei (C, N, D, etc.)
        table.columnTypes.push_back(std::wstring(1, (wchar_t)field.fieldType));

        // 3. Lungimea fizică în bytes
        table.columnWidths.push_back((int)field.fieldLength);

        // 4. Zecimale (Byte-ul 17 din descriptor)
        // Adăugăm valoarea în noul vector din vConTable
        table.columnDecimals.push_back((int)field.decimalCount);
    }

    // --- Citire Date ---
    std::vector<char> rowBuffer(header.recordLength);
    for (int i = 0; i < (int)header.numRecords; ++i) {
        if (!file.read(rowBuffer.data(), header.recordLength)) break;

        // Validare aliniere rând
        if (rowBuffer[0] != ' ' && rowBuffer[0] != '*') {
            LOG_DEBUG(L"Aliniere gresita detectata la randul " + std::to_wstring(i));
            continue;
        }

        // Ignorăm înregistrările șterse (PACK le va elimina fizic mai târziu)
        if (rowBuffer[0] == '*') {
            continue;
        }

        std::vector<std::wstring> rowRecord;
        size_t offset = 1; // Sărim peste flag-ul de delete (primul byte)

        for (const auto& field : fields) {
            // Extragem valoarea brută din buffer
            std::string val(rowBuffer.data() + offset, field.fieldLength);

            // Conversie în wstring și curățare spații
            rowRecord.push_back(wstr_trim(str_to_wstr(val)));

            offset += field.fieldLength;
        }
        table.records.push_back(std::move(rowRecord));
    }

    file.close();
    return table;
}

void dbfConnection::createBackup(const std::wstring& fullPath) {

    return;

    try {
        // 1. Obținem timpul curent
        auto acum = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(acum);

        std::tm bt{};
        localtime_s(&bt, &in_time_t); // Varianta safe pentru Windows/VS

        // 2. Formatăm timestamp-ul: YYYYMMDD_HHMMSS
        std::wstringstream ss;
        ss << std::put_time(&bt, L"%Y%m%d_%H%M%S");
        std::wstring timestamp = ss.str();

        // 3. Construim noul nume: tabela.dbf.20260211_160530.bak
        std::wstring backupPath = fullPath + L"." + timestamp + L".bak";

        // 4. Executăm copia
        if (std::filesystem::exists(fullPath)) {
            std::filesystem::copy_file(fullPath, backupPath, std::filesystem::copy_options::overwrite_existing);
            //LOG_INFO(L"Backup creat cu succes: " + backupPath);
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR(L"Eroare la crearea backup-ului: " + str_to_wstr(e.what()));
    }
}


bool dbfConnection::packTable(const std::wstring& tableName) {
    if (tableName.empty()) return false;

    std::wstring fileName = ensureExtension(tableName, L".dbf");
    std::wstring fullPath = m_filePath + (m_filePath.back() == L'\\' ? L"" : L"\\") + fileName;

    // 1. Backup
    createBackup(fullPath);

    // 2. Încărcare date într-un scope limitat
    vConTable liveData;
    {
        QueryTable qTable;
        qTable.name = tableName;
        qTable.alias = L"";

        // loadTable deschide, citește și ÎNCHIDE fișierul la finalul funcției sale
        liveData = loadTable(qTable);
    }

    if (liveData.columns.empty()) {
        LOG_ERROR(L"PACK Error: Nu s-au putut citi datele pentru " + tableName);
        return false;
    }

    // 3. Forțăm o mică pauză (opțional, dar util pentru lock-urile de sistem)
    // sau pur și simplu ne asigurăm că loadTable chiar a închis stream-ul.

    // 4. Salvare (Rescrie fișierul)
    bool success = saveFile(fullPath, liveData);

    if (success) {
        LOG_SUCCESS(L"PACK finalizat. Fișierul a fost reconstruit fizic.");
        // IMPORTANT: Dacă ai un sistem de caching în dbfConnection (ex: m_loadedTables), 
        // trebuie să ștergi cache-ul pentru acest tabel ca următorul SELECT să citească varianta nouă de pe disc.
    }

    return success;
}

bool dbfConnection::createTable(const std::wstring& query) {
    LOG_DEBUG(L"--- SUNT IN FISIERUL CORECT? DA! ---"); // <--- ADAUGA ASTA
    try {
        std::wstring upperQuery = to_upper(query);

        size_t tablePos = upperQuery.find(L"TABLE");
        size_t startParen = query.find(L'(');
        size_t endParen = query.find_last_of(L')');

        if (tablePos == std::wstring::npos || startParen == std::wstring::npos || endParen == std::wstring::npos) {
            LOG_ERROR(L"CREATE TABLE: Eroare de sintaxa.");
            return false;
        }

        size_t nameStart = tablePos + 5;
        std::wstring tableName = wstr_trim(query.substr(nameStart, startParen - nameStart));

        std::wstring fileName = ensureExtension(tableName, L".dbf");
        std::wstring fullPath = m_filePath + (m_filePath.back() == L'\\' ? L"" : L"\\") + fileName;

        if (std::filesystem::exists(fullPath)) {
            LOG_ERROR(L"CREATE TABLE: Tabelul '" + tableName + L"' exista deja!");
            return false;
        }

        std::wstring body = query.substr(startParen + 1, endParen - startParen - 1);
        vConTable newTable;
        //std::vector<std::wstring> colDefs = wexplodeQuoteSafe(body, L',');
        // Folosim o logică ce ignoră virgula dacă suntem în interiorul parantezelor:
        std::vector<std::wstring> colDefs;
        std::wstring currentDef;
        int parenLevel = 0;

        for (wchar_t c : body) {
            if (c == L'(') parenLevel++;
            if (c == L')') parenLevel--;

            if (c == L',' && parenLevel == 0) {
                colDefs.push_back(currentDef);
                currentDef.clear();
            }
            else {
                currentDef += c;
            }
        }
        if (!currentDef.empty()) colDefs.push_back(currentDef);

        for (const auto& def : colDefs) {
            std::wstring cleanDef = wstr_trim(def);
            if (cleanDef.empty()) continue;

            size_t firstSpace = cleanDef.find_first_of(L" \t");
            if (firstSpace == std::wstring::npos) continue;

            std::wstring colName = wstr_trim(cleanDef.substr(0, firstSpace));
            std::wstring rest = to_upper(wstr_trim(cleanDef.substr(firstSpace + 1)));

            LOG_DEBUG(L"DEBUG: colName=[" + colName + L"] rest=[" + rest + L"]");

            int finalW = 10, finalD = 0;
            std::wstring typeChar = rest.substr(0, 1);

            if (typeChar != L"D") {
                size_t lP = rest.find(L'(');
                size_t rP = rest.find(L')');

                LOG_DEBUG(L"DEBUG: lP=" + std::to_wstring(lP) + L" rP=" + std::to_wstring(rP));

                if (lP != std::wstring::npos && rP != std::wstring::npos) {
                    std::wstring inner = rest.substr(lP + 1, rP - lP - 1);
                    LOG_DEBUG(L" !!! PARSER ACTIV !!! Inner detectat: [" + inner + L"]");

                    size_t sep = inner.find_first_of(L",.");
                    if (sep != std::wstring::npos) {
                        finalW = std::stoi(inner.substr(0, sep));
                        finalD = std::stoi(inner.substr(sep + 1));
                    }
                    else {
                        finalW = std::stoi(inner);
                    }
                }
                else {
                    LOG_DEBUG(L"DEBUG: Nu am intrat in IF-ul de paranteze!");
                }
            }
            else {
                finalW = 8;
            }

            newTable.columns.push_back(colName);
            newTable.columnTypes.push_back(typeChar);
            newTable.columnWidths.push_back(finalW);
            newTable.columnDecimals.push_back(finalD);
        }

        // Verificare asimetrie inainte de log-ul final (safety check)
        if (newTable.columns.size() != newTable.columnDecimals.size()) {
            LOG_ERROR(L"CRITICAL: Eroare interna de structura. Vectori asimetrici!");
            return false;
        }

        // --- DEBUG LOG PENTRU VERIFICARE ---
        for (size_t i = 0; i < newTable.columns.size(); ++i) {
            LOG_DEBUG(L"Final Prep: " + newTable.columns[i] +
                L" Type: " + newTable.columnTypes[i] +
                L" Width: " + std::to_wstring(newTable.columnWidths[i]) +
                L" Decimals: " + std::to_wstring(newTable.columnDecimals[i]));
        }

        if (newTable.columns.empty()) {
            LOG_ERROR(L"CREATE TABLE: Nu a fost detectata nicio coloana valida.");
            return false;
        }

        bool ok = saveFile(fullPath, newTable);
        if (ok) LOG_SUCCESS(L"Tabel creat cu succes: " + fullPath);
        return ok;

    }
    catch (const std::exception& e) {
        LOG_ERROR(L"CREATE TABLE Exception: " + str_to_wstr(e.what()));
        return false;
    }
}

bool dbfConnection::dropTable(const std::wstring& query) {
    try {
        std::wstring upperQuery = to_upper(query);
        size_t tablePos = upperQuery.find(L"TABLE");

        if (tablePos == std::wstring::npos) {
            LOG_ERROR(L"DROP TABLE: Sintaxă incorectă.");
            return false;
        }

        // Extragem numele tabelului (tot ce urmează după "TABLE ")
        std::wstring tableName = wstr_trim(query.substr(tablePos + 5));

        // Curățăm ghilimelele dacă există
        if (!tableName.empty() && tableName.front() == L'\'')
            tableName = tableName.substr(1, tableName.size() - 2);

        if (tableName.empty()) {
            LOG_ERROR(L"DROP TABLE: Numele tabelului lipsește.");
            return false;
        }

        std::wstring fileName = ensureExtension(tableName, L".dbf");
        std::wstring fullPath = m_filePath + (m_filePath.back() == L'\\' ? L"" : L"\\") + fileName;

        // Verificăm dacă fișierul există înainte de a încerca să-l ștergem
        if (!std::filesystem::exists(fullPath)) {
            LOG_ERROR(L"DROP TABLE: Tabelul '" + tableName + L"' nu a fost găsit.");
            return false;
        }

        // Ștergerea fizică
        if (std::filesystem::remove(fullPath)) {
            LOG_SUCCESS(L"Tabelul '" + tableName + L"' a fost șters cu succes.");

            // OPȚIONAL: Ștergem și fișierul de index (.cdx / .idx) dacă există
            std::wstring indexFile = ensureExtension(tableName, L".cdx");
            std::wstring indexPath = m_filePath + (m_filePath.back() == L'\\' ? L"" : L"\\") + indexFile;
            if (std::filesystem::exists(indexPath)) {
                std::filesystem::remove(indexPath);
                //LOG_INFO(L"Indexul asociat a fost de asemenea șters.");
            }

            return true;
        }
        else {
            LOG_ERROR(L"DROP TABLE: Nu s-a putut șterge fișierul. Posibil să fie deschis în altă aplicație.");
            return false;
        }

    }
    catch (const std::exception& e) {
        LOG_ERROR(L"DROP TABLE Exception: " + str_to_wstr(e.what()));
        return false;
    }
}

bool dbfConnection::showTables(std::string stm_name) {
    try {
        // 1. Pregătim structura tabelului virtual
        vConTable showTable;
        showTable.tableName = L"System_Tables";
        showTable.columns = { L"Tables_in_database" };
        showTable.columnTypes = { L"C" };
        showTable.columnWidths = { 40 };

        // 2. Scanăm directorul și populăm randurile
        for (const auto& entry : std::filesystem::directory_iterator(m_filePath)) {
            if (entry.is_regular_file() && to_upper(entry.path().extension().wstring()) == L".DBF") {
                std::vector<std::wstring> row;
                row.push_back(entry.path().stem().wstring()); // Numele fișierului
                showTable.records.push_back(row);
            }
        }

        // Sortare alfabetică a rândurilor
        std::sort(showTable.records.begin(), showTable.records.end(),
            [](const auto& a, const auto& b) { return a[0] < b[0]; });

        // 3. Mapăm pe vConResult (pentru afișare imediată)
        vConResult res;
        res.success = true;
        res.table = showTable; // Acesta va fi desenat de consolă
        m_lastResult = res;

        // 4. Mapăm pe m_statements (pentru referințe ulterioare)
        auto ctx = std::make_shared<TableContext>();
        ctx->colNames = showTable.columns;
        for (const auto& record : showTable.records) {
            std::map<std::wstring, std::wstring> rowMap;
            rowMap[L"Tables_in_database"] = record[0];
            ctx->dbfResult.push_back(rowMap);
        }
        m_statements[stm_name] = ctx;

        //LOG_INFO(L"Found " + std::to_wstring(showTable.records.size()) + L" tables.");
        return true;

    }
    catch (const std::exception& e) {
        LOG_ERROR(L"SHOW TABLES Error: " + str_to_wstr(e.what()));
        return false;
    }
}

bool dbfConnection::describeTable(const std::wstring& query, std::string stm_name) {
    try {
        // 1. Extragem numele tabelului
        std::wstring tableName;
        if (to_upper(query).find(L"DESCRIBE ") == 0) tableName = wstr_trim(query.substr(9));
        else tableName = wstr_trim(query.substr(5));

        if (tableName.empty()) return false;

        // 2. Încărcăm tabela (doar structura ne interesează, dar loadTable o citește pe toată)
        QueryTable qt;
        qt.name = tableName;
        vConTable table = loadTable(qt);

        // 3. Pregătim rezultatul virtual
        vConTable descTable;
        descTable.tableName = L"Describe_" + tableName;
        descTable.columns = { L"Field", L"Type", L"Width", L"Decimals" };
        descTable.columnTypes = { L"C", L"C", L"N", L"N" };
        descTable.columnWidths = { 15, 10, 10, 10 };

        // 4. Populăm rândurile cu metadatele din vConTable
        for (size_t i = 0; i < table.columns.size(); ++i) {
            std::vector<std::wstring> row;
            row.push_back(table.columns[i]);                               // Field
            row.push_back(table.columnTypes[i]);                          // Type
            row.push_back(std::to_wstring(table.columnWidths[i]));        // Width
            row.push_back(std::to_wstring(table.columnDecimals[i]));      // Decimals
            descTable.records.push_back(row);
        }

        // 5. Mapăm pe vConResult și m_statements pentru afișare
        vConResult res;
        res.success = true;
        res.table = descTable;
        m_lastResult = res;

        auto ctx = std::make_shared<TableContext>();
        ctx->colNames = descTable.columns;
        for (const auto& r : descTable.records) {
            std::map<std::wstring, std::wstring> rowMap;
            for (size_t i = 0; i < descTable.columns.size(); ++i) rowMap[descTable.columns[i]] = r[i];
            ctx->dbfResult.push_back(rowMap);
        }
        m_statements[stm_name] = ctx;

        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR(L"DESCRIBE Error: " + str_to_wstr(e.what()));
        return false;
    }
}


bool dbfConnection::alterTable(const std::wstring& query) {
    std::wstring upperQuery = to_upper(query);

    // 1. Identificăm poziția tabelului
    size_t tablePos = upperQuery.find(L"TABLE");
    if (tablePos == std::wstring::npos) return false;

    // Căutăm unde începe acțiunea (ADD, DROP sau MODIFY) pentru a delimita numele tabelului
    size_t actionPos = std::wstring::npos;
    std::wstring action;

    if (upperQuery.find(L" ADD ") != std::wstring::npos) {
        actionPos = upperQuery.find(L" ADD ");
        action = L"ADD";
    }
    else if (upperQuery.find(L" DROP ") != std::wstring::npos) {
        actionPos = upperQuery.find(L" DROP ");
        action = L"DROP";
    }
    else if (upperQuery.find(L" MODIFY ") != std::wstring::npos) {
        actionPos = upperQuery.find(L" MODIFY ");
        action = L"MODIFY";
    }

    if (actionPos == std::wstring::npos) {
        LOG_ERROR(L"ALTER TABLE: Actiune necunoscuta (nevoie de ADD, DROP sau MODIFY).");
        return false;
    }

    // Extragem numele tabelului dintre "TABLE" și "ADD/DROP/MODIFY"
    size_t nameStart = tablePos + 5;
    std::wstring tableName = wstr_trim(query.substr(nameStart, actionPos - nameStart));

    // 2. Executăm acțiunea
    if (action == L"DROP") {
        // Căutăm "COLUMN" în restul query-ului
        size_t colKeywordPos = upperQuery.find(L"COLUMN", actionPos);
        size_t colNameStart;

        if (colKeywordPos != std::wstring::npos) {
            colNameStart = colKeywordPos + 6;
        }
        else {
            // Unii scriu ALTER TABLE nume DROP nume_coloana (fără cuvântul COLUMN)
            colNameStart = actionPos + 5;
        }

        std::wstring colName = wstr_trim(query.substr(colNameStart));
        // Eliminăm eventualul punct și virgulă de la final
        if (!colName.empty() && colName.back() == L';') colName.pop_back();

        return alterDropColumn(tableName, colName);
    }
    else if (action == L"ADD") {
        return alterAddColumn(tableName, query);
    }
    else if (action == L"MODIFY") {
        return alterModifyColumn(tableName, query);
    }

    return false;
}


bool dbfConnection::alterDropColumn(const std::wstring& tableName, const std::wstring& colName) {
    try {
        // 1. Încărcăm tabelul complet (Header + Descriptori + Date)
        // Folosim QueryTable pentru a reutiliza loadTable
        QueryTable qt;
        qt.name = tableName;
        vConTable table = loadTable(qt);

        // 2. Găsim indexul coloanei (case-insensitive)
        std::wstring targetCol = to_upper(wstr_trim(colName));
        int colIdx = -1;
        for (int i = 0; i < (int)table.columns.size(); ++i) {
            if (to_upper(table.columns[i]) == targetCol) {
                colIdx = i;
                break;
            }
        }

        if (colIdx == -1) {
            LOG_ERROR(L"ALTER TABLE: Coloana '" + colName + L"’ nu exista în tabelul " + tableName);
            return false;
        }

        LOG_DEBUG(L"ALTER TABLE: Eliminam coloana " + targetCol + L" de la indexul " + std::to_wstring(colIdx));

        // 3. Eliminăm metadata coloanei
        table.columns.erase(table.columns.begin() + colIdx);
        table.columnTypes.erase(table.columnTypes.begin() + colIdx);
        table.columnWidths.erase(table.columnWidths.begin() + colIdx);
        table.columnDecimals.erase(table.columnDecimals.begin() + colIdx);

        // 4. Eliminăm datele din fiecare rând (fizic, din vectorul de records)
        for (auto& row : table.records) {
            if (colIdx < (int)row.size()) {
                row.erase(row.begin() + colIdx);
            }
        }

        // 5. Resalvăm fișierul
        // saveFile va recalcula headerLength și recordLength pe baza noilor vectori
        std::wstring fileName = ensureExtension(tableName, L".dbf");
        std::wstring fullPath = m_filePath + (m_filePath.back() == L'\\' ? L"" : L"\\") + fileName;

        // Opțional: Backup la fișierul vechi înainte de overwrite
        // std::filesystem::copy_file(fullPath, fullPath + L".bak", std::filesystem::copy_options::overwrite_existing);

        bool ok = saveFile(fullPath, table);
        if (ok) {
            LOG_SUCCESS(L"Coloana '" + colName + L"' a fost eliminata cu succes.");
        }
        return ok;

    }
    catch (const std::exception& e) {
        LOG_ERROR(L"ALTER DROP Exception: " + str_to_wstr(e.what()));
        return false;
    }
}

bool dbfConnection::alterAddColumn(const std::wstring & tableName, const std::wstring & query) {
    try {
        // 1. Încărcăm tabelul existent
        QueryTable qt;
        qt.name = tableName;
        vConTable table = loadTable(qt);

        // 2. Extragem definiția coloanei din query
        // Căutăm după cuvântul "ADD" sau "ADD COLUMN"
        std::wstring upperQuery = to_upper(query);
        size_t addPos = upperQuery.find(L" ADD ");
        size_t colKeywordPos = upperQuery.find(L" COLUMN ", addPos);

        size_t defStart = (colKeywordPos != std::wstring::npos) ? colKeywordPos + 8 : addPos + 5;
        std::wstring colDef = wstr_trim(query.substr(defStart));
        if (!colDef.empty() && colDef.back() == L';') colDef.pop_back();

        // 3. Folosim logica de parser pentru a extrage Nume, Tip, Width, Dec
        size_t firstSpace = colDef.find_first_of(L" \t");
        if (firstSpace == std::wstring::npos) {
            LOG_ERROR(L"ALTER ADD: Definitie coloana invalida. Exemplu: ADD nume C(10)");
            return false;
        }

        std::wstring colName = to_upper(wstr_trim(colDef.substr(0, firstSpace)));
        std::wstring rest = to_upper(wstr_trim(colDef.substr(firstSpace + 1)));

        int finalW = 10, finalD = 0;
        std::wstring typeChar = rest.substr(0, 1);

        if (typeChar != L"D") {
            size_t lP = rest.find(L'(');
            size_t rP = rest.find(L')');
            if (lP != std::wstring::npos && rP != std::wstring::npos) {
                std::wstring inner = rest.substr(lP + 1, rP - lP - 1);
                size_t sep = inner.find_first_of(L",.");
                if (sep != std::wstring::npos) {
                    finalW = std::stoi(inner.substr(0, sep));
                    finalD = std::stoi(inner.substr(sep + 1));
                }
                else {
                    finalW = std::stoi(inner);
                }
            }
        }
        else {
            finalW = 8; // Data are mereu 8 caractere YYYYMMDD
        }

        // 4. Actualizăm Metadata
        table.columns.push_back(colName);
        table.columnTypes.push_back(typeChar);
        table.columnWidths.push_back(finalW);
        table.columnDecimals.push_back(finalD);

        // 5. Actualizăm datele existente (adăugăm celule goale pentru noua coloană)
        for (auto& row : table.records) {
            row.push_back(L""); // saveFile se va ocupa să pad-uiască cu spații conform finalW
        }

        // 6. Salvăm tabelul
        std::wstring fileName = ensureExtension(tableName, L".dbf");
        std::wstring fullPath = m_filePath + (m_filePath.back() == L'\\' ? L"" : L"\\") + fileName;

        bool ok = saveFile(fullPath, table);
        if (ok) LOG_SUCCESS(L"Coloana '" + colName + L"' a fost adaugata cu succes.");
        return ok;

    }
    catch (const std::exception& e) {
        LOG_ERROR(L"ALTER ADD Exception: " + str_to_wstr(e.what()));
        return false;
    }
}



bool dbfConnection::alterModifyColumn(const std::wstring& tableName, const std::wstring& query) {
    try {
        QueryTable qt;
        qt.name = tableName;
        vConTable table = loadTable(qt);

        // 1. Extragem definitia (similar cu ADD)
        std::wstring upperQuery = to_upper(query);
        size_t modPos = upperQuery.find(L" MODIFY ");
        size_t colKeywordPos = upperQuery.find(L" COLUMN ", modPos);
        size_t defStart = (colKeywordPos != std::wstring::npos) ? colKeywordPos + 8 : modPos + 8;

        std::wstring colDef = wstr_trim(query.substr(defStart));
        if (!colDef.empty() && colDef.back() == L';') colDef.pop_back();

        // 2. Parsam numele si noile caracteristici
        size_t firstSpace = colDef.find_first_of(L" \t");
        std::wstring targetCol = to_upper(wstr_trim(colDef.substr(0, firstSpace)));
        std::wstring rest = to_upper(wstr_trim(colDef.substr(firstSpace + 1)));

        // Identificam indexul coloanei vechi
        int colIdx = -1;
        for (int i = 0; i < (int)table.columns.size(); ++i) {
            if (to_upper(table.columns[i]) == targetCol) { colIdx = i; break; }
        }
        if (colIdx == -1) { LOG_ERROR(L"MODIFY: Coloana inexistenta."); return false; }

        // Aflam noile Width/Dec (refolosim logica de la ADD)
        int newW = 10, newD = 0;
        std::wstring newType = rest.substr(0, 1);
        if (newType != L"D") {
            size_t lP = rest.find(L'('), rP = rest.find(L')');
            if (lP != std::wstring::npos && rP != std::wstring::npos) {
                std::wstring inner = rest.substr(lP + 1, rP - lP - 1);
                size_t sep = inner.find_first_of(L",.");
                if (sep != std::wstring::npos) {
                    newW = std::stoi(inner.substr(0, sep));
                    newD = std::stoi(inner.substr(sep + 1));
                }
                else { newW = std::stoi(inner); }
            }
        }
        else { newW = 8; }

        // 3. ACTUALIZARE METADATA
        table.columnTypes[colIdx] = newType;
        table.columnWidths[colIdx] = newW;
        table.columnDecimals[colIdx] = newD;

        // 4. PROCESARE DATE (Truncate sau Padding)
        // Aici e magia: saveFile va scrie datele, dar noi trebuie sa ne asiguram 
        // ca string-urile din table.records nu sunt mai lungi decat noul Width
        for (auto& row : table.records) {
            if (colIdx < (int)row.size()) {
                if ((int)row[colIdx].length() > newW) {
                    row[colIdx] = row[colIdx].substr(0, newW); // Truncate
                }
                // Padding-ul cu spatii il face de obicei saveFile la scrierea binara
            }
        }

        std::wstring fileName = ensureExtension(tableName, L".dbf");
        std::wstring fullPath = m_filePath + (m_filePath.back() == L'\\' ? L"" : L"\\") + fileName;

        bool ok = saveFile(fullPath, table);
        if (ok) LOG_SUCCESS(L"Coloana '" + targetCol + L"' a fost modificata.");
        return ok;

    }
    catch (const std::exception& e) {
        LOG_ERROR(L"MODIFY Exception: " + str_to_wstr(e.what()));
        return false;
    }
}


std::vector<vExternalColumnInfo> dbfConnection::getTableSchema(const std::wstring& tableName) {
    std::vector<vExternalColumnInfo> columns;

    // 1. Construim calea către fișierul .dbf
    std::wstring fileName = tableName;
    if (fileName.find(L".dbf") == std::wstring::npos && fileName.find(L".DBF") == std::wstring::npos) {
        fileName += L".dbf";
    }

    // m_dirPath este directorul setat în constructor
    std::wstring fullPath = m_dirPath + (m_dirPath.back() == L'\\' ? L"" : L"\\") + fileName;

    // 2. Deschidem fișierul binar
    //std::ifstream file(fullPath, std::ios::binary);
    std::ifstream file(std::filesystem::path(fullPath), std::ios::binary);
    if (!file.is_open()) {
        m_error = L"getTableSchema: Nu s-a putut deschide fisierul: " + fullPath;
        return columns;
    }

    // 3. Citim Header-ul principal (primii 32 bytes)
    DBF_Header header;
    file.read(reinterpret_cast<char*>(&header), sizeof(DBF_Header));

    // 4. Citim descriptorii de câmpuri
    // Descriptorii încep de la offset-ul 32 și se termină când găsim caracterul 0x0D (CR)
    while (true) {
        char nextByte;
        if (!file.get(nextByte) || nextByte == 0x0D) break;

        // Punem byte-ul înapoi și citim descriptorul complet (32 bytes per câmp)
        file.unget();
        DBF_FieldDescriptor field;
        file.read(reinterpret_cast<char*>(&field), sizeof(DBF_FieldDescriptor));

        vExternalColumnInfo info;

        // Conversie nume câmp (char[11] terminat cu \0 sau spații)
        std::string rawName(field.fieldName, 11);
        rawName.erase(rawName.find_last_not_of(" \0") + 1);
        info.name = str_to_wstr(rawName);

        // Mapare Tipuri Native DBF -> vNativeDataType
        char type = field.fieldType;
        if (type == 'N' || type == 'F') {
            info.type = vNativeDataType::V_DOUBLE;
            info.precision = field.decimalCount;
        }
        else if (type == 'D') {
            info.type = vNativeDataType::V_DATE;
        }
        else if (type == 'L') {
            info.type = vNativeDataType::V_BOOLEAN;
        }
        else if (type == 'M') {
            info.type = vNativeDataType::V_TEXT; // Memo
        }
        else {
            info.type = vNativeDataType::V_TEXT; // Character ('C')
        }

        info.length = field.fieldLength;
        info.isNullable = true; // DBF-ul clasic nu are flag de NULL per coloană în header

        columns.push_back(info);
    }

    file.close();
    return columns;
}

void dbfConnection::clearStatement(std::string stm_name = "default") {
    // 1. Căutăm statement-ul în map-ul de contexte (m_statements)
    auto it = m_statements.find(stm_name);

    if (it != m_statements.end()) {
        // 2. Închidem fișierul dacă acesta este încă deschis în contextul respectiv
        // TableContext deține un std::ifstream file
        if (it->second && it->second->file.is_open()) {
            it->second->file.close();
        }

        // 3. Ștergem intrarea din mapă
        // Fiind un std::shared_ptr, dacă nicio altă parte a programului nu mai ține 
        // o referință la acest context, obiectul TableContext va fi distrus, 
        // eliberând automat vectorul dbfResult (RAM-ul).
        m_statements.erase(it);

        // LOG_DEBUG(L"dbfConnection::clearStatement: Memorie eliberată pentru contextul '" + str_to_wstr(stm_name) + L"'");
    }
}