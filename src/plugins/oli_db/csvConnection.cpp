#include "csvConnection.hpp"
#include "stringUtils.hpp"
#include "fileUtils.hpp"
#include "globals.hpp"

#include "ui/ConsoleManager.hpp"
#include "sql/SqlQueryEngine.hpp"

#include <iostream>
#include <filesystem>



csvConnection::csvConnection(const std::string& type, const std::wstring& dsn)
    : type(type), csvPath(wstr_to_str(dsn)), connected(false) {
    
    this->dsn = dsn;
}

csvConnection::~csvConnection() {
    closeDatabase();
}


bool csvConnection::readCSVFiles(const std::string& directoryPath) {
    csvFiles.clear();  // Curățăm lista înainte de populare
    //LOG_INFO(L"Citestc fisierele din director.");
    try {
       // for (const auto& entry : std::filesystem::directory_iterator(getGlobalReportPath()+directoryPath)) {
        for (const auto& entry : std::filesystem::directory_iterator(directoryPath)) {
            if (entry.is_regular_file() && entry.path().extension() == ".csv") {
                csvFiles.push_back(entry.path().filename().string());
            }
        }
        return !csvFiles.empty();  // Returnează true dacă a găsit fișiere .csv
    }
    catch (const std::filesystem::filesystem_error& e) {
        LOG_ERROR(L"Eroare: " + str_to_wstr( e.what()));
        return false;  // Returnează false dacă a apărut o eroare
    }
}

bool csvConnection::openDatabase() {
    /*
    LOG_INFO(L"FAC OPEN");
    bool connected = readCSVFiles(csvPath);
    for (auto ff : csvFiles)
        LOG_INFO(L"Am citit:" + str_to_wstr(ff));

    return connected;
    */

    if (std::filesystem::exists(csvPath) && std::filesystem::is_directory(csvPath)) {
        LOG_SUCCESS(L"CSV Directory opened: " + str_to_wstr(csvPath));
        return true;
    }
    error = L"Directorul nu exista: " + str_to_wstr(csvPath);
    return false;
}


void csvConnection::closeDatabase() {
    connected = false;
    m_statements.clear(); // Asta eliberează toate resursele procesate
    csvFiles.clear();
}

bool csvConnection::isConnected() const {
    return connected;
}

void csvConnection::clearError(){
    error.clear();
}

std::wstring csvConnection::getError(){
    return error;
}

std::vector<std::string> csvConnection::extractTableNames(const std::string& query) {
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

bool csvConnection::execQuery(const std::wstring& query, std::string stm_name) {
    //LOG_INFO(L"CSV Engine [parse]: " + query);

    // 1. Parsare Query
    SqlQueryParser parser(query);
    if (parser.getLastError().hasError) {
        error = parser.getLastError().message;
        LOG_ERROR(error);
        return false;
    }

    // 2. Identificare Tabel (Fișier CSV)
    auto participating = parser.getAllParticipatingTables();
    if (participating.empty()) return false;

    // Ne asigurăm că avem extensia .csv
    std::wstring tableName = participating[0].name;
    if (tableName.find(L".csv") == std::wstring::npos) tableName += L".csv";

    std::string fullPath = csvPath + (csvPath.back() == '\\' ? "" : "\\") + wstr_to_str(tableName);

    // 3. Citire fișier CSV și populare vTable
    // Folosim funcția ta existentă readCSVFile2 sau similar
    std::vector<std::wstring> rawLines = readCSVFile2(fullPath);
    if (rawLines.empty()) {
        error = L"Fisierul CSV este gol sau nu a fost gasit: " + str_to_wstr(fullPath);
        return false;
    }

    vConTable csvAsTable;
    csvAsTable.tableName = tableName;

    // Header-ul (prima linie)
    std::vector<std::wstring> headerCols = splitCSVLine(rawLines[0], ',');
    for (auto& col : headerCols) {
        csvAsTable.columns.push_back(wstr_trim(col));
        csvAsTable.columnTypes.push_back(L"C"); // CSV-ul este implicit text
    }

    // Datele (de la linia 1 încolo)
    for (size_t i = 1; i < rawLines.size(); ++i) {
        std::vector<std::wstring> row = splitCSVLine(rawLines[i], ',');
        // Padding dacă rândul e mai scurt decât header-ul
        while (row.size() < csvAsTable.columns.size()) row.push_back(L"");
        csvAsTable.records.push_back(row);
    }

    // 4. Engine Execution (Aici intervine uniformizarea!)
    std::vector<vConTable> universe = { csvAsTable };
    vSqlEngine engine(universe, parser);


    vConResult result = engine.execute(parser);
    m_lastResult = result;
    // 5. Salvare în TableContext (pentru fetchMap/fetchNextRow)
    auto ctx = std::make_shared<TableContext>();
    ctx->colNames = result.table.columns;
    ctx->columnTypes = result.table.columnTypes;
    ctx->currentRowIndex = -1;

    for (const auto& record : result.table.records) {
        std::map<std::wstring, std::wstring> rowMap;
        for (size_t i = 0; i < result.table.columns.size(); ++i) {
            rowMap[result.table.columns[i]] = (i < record.size()) ? record[i] : L"";
        }
        ctx->dbfResult.push_back(rowMap); // Folosim același vector din Context
    }

    // Înregistrăm contextul în map-ul de statement-uri
    // Trebuie să te asiguri că csvConnection are un membru m_statements similar cu dbfConnection
    // Dacă nu, poți folosi structura ta existentă csvLines/colNames
    this->m_statements[stm_name] = ctx;

    LOG_SUCCESS(L"CSV Query executat. Randuri: " + std::to_wstring(ctx->dbfResult.size()));
    return true;
}


int csvConnection::getRowCount(std::string stm_name) {
    auto it = m_statements.find(stm_name);
    if (it != m_statements.end()) {
        // Returnăm numărul de rânduri stocate în contextul rezultatului
        return (int)it->second->dbfResult.size();
    }

    LOG_ERROR(L"getRowCount: Statement-ul '" + str_to_wstr(stm_name) + L"' nu a fost gasit.");
    return 0;
}


const std::vector<std::wstring>& csvConnection::getColumnNames(std::string stm_name) {
    auto it = m_statements.find(stm_name);
    if (it != m_statements.end()) {
        return it->second->colNames;
    }
    static std::vector<std::wstring> empty;
    return empty;
}


/*
bool csvConnection::setColNames(std::string stm_name) {
    colNames[stm_name].clear(); 
    //colNames[stm_name] =  wexplode(csvLines[stm_name][0],',');
    colNames[stm_name] = splitCSVLine(csvLines[stm_name][0], ',');
    
    for (auto col : colNames[stm_name]) {
        col = wstr_trim(col);
        //std::wcout << L"Ultimul caracter: "<<col <<":" << static_cast<int>(col.back()) << std::endl;
        //std::wcout << L"AA:{" << col << "}" << std::endl;
    }
    //trim_wstr_vec(colNames[stm_name]);

    ColumnCountPtrs[stm_name] = colNames[stm_name].size();
    return true; 
}
*/

std::wstring csvConnection::fetchFieldByNumber(int fieldNo, std::string stm_name) {
    auto it = m_statements.find(stm_name);
    if (it == m_statements.end()) return L"";

    auto ctx = it->second;
    if (ctx->currentRowIndex >= 0 && ctx->currentRowIndex < (int)ctx->dbfResult.size()) {
        const auto& rowMap = ctx->dbfResult[ctx->currentRowIndex];
        // Trebuie să găsim numele coloanei corespunzător indexului fieldNo
        if (fieldNo >= 0 && fieldNo < (int)ctx->colNames.size()) {
            return rowMap.at(ctx->colNames[fieldNo]);
        }
    }
    return L"";
}

std::wstring csvConnection::fetchRawRow(std::string stm_name) {
    // În noul sistem, nu mai avem un "raw row" (string netăiat), 
    // dar putem reconstrui linia din map pentru compatibilitate
    auto row = fetchRow(stm_name);
    std::wstring raw = L"";
    for (size_t i = 0; i < row.size(); ++i) {
        raw += row[i] + (i < row.size() - 1 ? L"," : L"");
    }
    return raw;
}

/*
std::vector<std::wstring> csvConnection::fetchRow(std::string stm_name) {
    if (rowCounters[stm_name] >= (int)csvLines[stm_name].size()) return {};
    return splitCSVLine(csvLines[stm_name][rowCounters[stm_name]], ',');
}
*/

std::vector<std::wstring> csvConnection::fetchRow(std::string stm_name) {
    auto it = m_statements.find(stm_name);
    if (it == m_statements.end()) return {};

    auto ctx = it->second;
    if (ctx->currentRowIndex >= 0 && ctx->currentRowIndex < (int)ctx->dbfResult.size()) {
        const auto& rowMap = ctx->dbfResult[ctx->currentRowIndex];
        std::vector<std::wstring> result;
        // Reconstituim vectorul ordonat după colNames
        for (const auto& colName : ctx->colNames) {
            //result.push_back(rowMap.at(colName));
            result.push_back(rowMap.count(colName) ? rowMap.at(colName) : L"");
        }
        return result;
    }
    return {};
}


std::wstring csvConnection::fetchFieldByName(const std::wstring& fieldName, std::string stm_name) {
    // 1. Căutăm statement-ul în noul sistem de contexte
    auto it = m_statements.find(stm_name);
    if (it == m_statements.end()) {
        LOG_ERROR(L"fetchFieldByName: Statement-ul '" + str_to_wstr(stm_name) + L"' nu a fost gasit.");
        return L"";
    }

    auto ctx = it->second;

    // 2. Verificăm dacă suntem la un rând valid (stabilit de fetchNextRow)
    if (ctx->currentRowIndex >= 0 && ctx->currentRowIndex < (int)ctx->dbfResult.size()) {
        const auto& rowMap = ctx->dbfResult[ctx->currentRowIndex];

        // 3. Căutăm câmpul direct în map-ul rândului curent
        auto fieldIt = rowMap.find(fieldName);
        if (fieldIt != rowMap.end()) {
            return fieldIt->second;
        }
        else {
            LOG_ERROR(L"Coloana '" + fieldName + L"' nu exista in rezultatul query-ului.");
        }
    }

    return L"";
}



std::map<std::wstring, std::wstring> csvConnection::fetchMap(std::string stm_name) {
    auto it = m_statements.find(stm_name);
    if (it == m_statements.end()) return {};

    auto ctx = it->second;
    // Verificăm dacă indexul curent este valid în vectorul de rezultate
    if (ctx->currentRowIndex >= 0 && ctx->currentRowIndex < (int)ctx->dbfResult.size()) {
        return ctx->dbfResult[ctx->currentRowIndex];
    }

    return {};
}

std::string csvConnection::getConnectionType(){
    return type;
}

std::wstring csvConnection::getConnectionDSN(){
    return dsn;
}

void csvConnection::printFiles() {
    for (const auto& file : csvFiles) {
        std::wcout << str_to_wstr(file) << std::endl;
    }
}


void csvConnection::setConnectionDSN(const std::wstring& txt) {
    dsn = txt;
    csvPath = wstr_to_str(dsn);
}


/*
bool csvConnection::fetchNextRow(std::string stm_name) {
    return true;
}
*/
/*
bool csvConnection::fetchNextRow(std::string stm_name) {
    // Verificăm dacă statement-ul există
    if (csvLines.find(stm_name) == csvLines.end()) return false;

    // Incrementăm cursorul. 
    // Notă: Pornim de la 0 (header), deci primul rând de date este la index 1.
    // Dacă execQuery a setat rowCounters la 0, fetchNextRow îl duce la 1 la prima rulare.
    if (rowCounters[stm_name] + 1 < (int)csvLines[stm_name].size()) {
        rowCounters[stm_name]++;
        return true;
    }

    return false;
}
*/

bool csvConnection::fetchNextRow(std::string stm_name) {
    auto it = m_statements.find(stm_name);
    if (it == m_statements.end()) return false;

    auto ctx = it->second;
    if (ctx->currentRowIndex + 1 < (int)ctx->dbfResult.size()) {
        ctx->currentRowIndex++;
        return true;
    }
    return false;
}

bool csvConnection::reconnect() {
    return true;
}

bool csvConnection::testConnection() {
    return true;
}

// odbcConnection.cpp (Implementare)
long long csvConnection::execCountQuery(const std::wstring& countQuery) {
    // Folosește un statement dedicat pentru COUNT, dacă nu ai deja unul
    std::string stm_name = "count_stmt";
    return -1;
}

const std::vector<vNativeDataType> csvConnection::getColumnTypes(std::string stm_name) {
    std::vector<vNativeDataType> universalTypes;
    auto it = m_statements.find(stm_name);

    if (it != m_statements.end()) {
        auto ctx = it->second;
        for (const auto& typeStr : ctx->columnTypes) {
            if (typeStr == L"N") {
                // Mapăm tipul numeric la DOUBLE pentru a acoperi și întregi și zecimale
                universalTypes.push_back(vNativeDataType::V_DOUBLE);
            }
            else if (typeStr == L"D") {
                universalTypes.push_back(vNativeDataType::V_DATE);
            }
            else if (typeStr == L"L") {
                universalTypes.push_back(vNativeDataType::V_BOOLEAN);
            }
            else {
                universalTypes.push_back(vNativeDataType::V_TEXT);
            }
        }
    }
    return universalTypes;
}

const std::vector<vExternalColumnInfo> csvConnection::getColumnsInfo(std::string stm_name) {
    std::vector<vExternalColumnInfo> infoList;
    auto it = m_statements.find(stm_name);

    if (it != m_statements.end()) {
        auto ctx = it->second;
        for (size_t i = 0; i < ctx->colNames.size(); ++i) {
            vExternalColumnInfo info;
            info.name = ctx->colNames[i];

            std::wstring t = (i < ctx->columnTypes.size()) ? ctx->columnTypes[i] : L"C";

            // Mapare conform enum-ului tău real
            if (t == L"N") info.type = vNativeDataType::V_DOUBLE;
            else if (t == L"D") info.type = vNativeDataType::V_DATE;
            else if (t == L"L") info.type = vNativeDataType::V_BOOLEAN;
            else info.type = vNativeDataType::V_TEXT;

            info.length = 255;
            info.isNullable = true;
            infoList.push_back(info);
        }
    }
    return infoList;
}

std::vector<vExternalColumnInfo> csvConnection::getTableSchema(const std::wstring& tableName) {
    std::vector<vExternalColumnInfo> columns;

    // 1. Construim calea completă către fișierul CSV
    std::wstring fileName = tableName;
    if (fileName.find(L".csv") == std::wstring::npos) {
        fileName += L".csv";
    }

    std::string fullPath = csvPath + (csvPath.back() == '\\' ? "" : "\\") + wstr_to_str(fileName);

    // 2. Deschidem fișierul pentru citire
    std::ifstream file(fullPath);
    if (!file.is_open()) {
        error = L"getTableSchema: Nu s-a putut deschide fisierul: " + str_to_wstr(fullPath);
        LOG_ERROR(error);
        return columns;
    }

    // 3. Citim DOAR prima linie (Header-ul)
    std::string firstLine;
    if (std::getline(file, firstLine)) {
        // Eliminăm caracterele de control ascunse (BOM sau \r)
        if (!firstLine.empty() && (firstLine.back() == '\r' || firstLine.back() == '\n')) {
            firstLine.pop_back();
        }

        // 4. Folosim splitCSVLine-ul tău existent (presupunem că e varianta std::wstring)
        // Dacă splitCSVLine primește doar wstring, convertim linia
        std::vector<std::wstring> headerCols = splitCSVLine(str_to_wstr(firstLine), ',');

        for (const auto& colName : headerCols) {
            vExternalColumnInfo info;
            info.name = wstr_trim(colName);

            // În CSV, totul este tratat implicit ca Text (V_TEXT)
            // excepție făcă dacă ai o logică de scanare a primei linii de date
            info.type = vNativeDataType::V_TEXT;
            info.length = 255;  // Valoare default simbolică
            info.precision = 0;
            info.isNullable = true;

            columns.push_back(info);
        }
    }

    file.close();
    return columns;
}

void csvConnection::clearStatement(std::string stm_name = "default") {
    // 1. Căutăm statement-ul în map-ul de contexte
    auto it = m_statements.find(stm_name);

    if (it != m_statements.end()) {
        // 2. Eliminăm intrarea din mapă. 
        // Fiind un std::shared_ptr, dacă acesta este ultimul loc unde este referențiat,
        // obiectul TableContext va fi distrus automat, eliberând memoria RAM.
        m_statements.erase(it);

        // LOG_DEBUG(L"csvConnection::clearStatement: Memoria pentru contextul '" + str_to_wstr(stm_name) + L"' a fost eliberată.");
    }
}