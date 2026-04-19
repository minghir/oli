/*
#include "dbConnection.hpp"
#include "odbcConnection.hpp"
#include "../oli/OliEngine.hpp" 

#include <memory>
#include <mutex>

// Presupunem că folosim o clasă ipotetică de DB
// static asigura ca variabila e vizibila doar in acest fisier (capsulare)
static std::unique_ptr<dbConnection> g_dbConn = nullptr;
static std::mutex g_dbMutex; // Protecție pentru multi-threading


    void RegisterDBFunctions(std::map<std::wstring, std::function<vData(const std::vector<vData>&)>>& registry) {

        // Funcția: DB_CONNECT("connection_string")
        registry[L"DB_CONNECT"] = [](const std::vector<vData>& args) -> vData {
            std::lock_guard<std::mutex> lock(g_dbMutex);

            if (g_dbConn && g_dbConn->isConnected()) {
                return vData{ L"ALREADY_CONNECTED" };
            }

            if (args.empty()) return vData{ L"ERROR: No connection string" };

            // Extragem string-ul de conexiune
            std::wstring connType = std::get<std::wstring>(args[0].value);
            std::wstring connStr = std::get<std::wstring>(args[1].value);
            if(connType == L"odbc")
                g_dbConn = std::make_unique<odbcConnection>(wstr_to_str(connType), connStr);
            else return vData{ L"UNKNOWN DB TYPE" };

            if (g_dbConn->openDatabase()) {
                return vData{ L"SUCCESS" };
            }
            return vData{ L"FAILED" };
        };

        // Funcția: DB_QUERY("SELECT...")
        registry[L"DB_QUERY"] = [](const std::vector<vData>& args) -> vData {
            std::lock_guard<std::mutex> lock(g_dbMutex);

            if (!g_dbConn || !g_dbConn->isConnected()) {
                return vData{ L"ERROR: Not connected" };
            }

            std::wstring sql = std::get<std::wstring>(args[0].value);
            auto result = g_dbConn->execQuery(sql); // Returnează datele

            return vData{ result }; // Trimitem rezultatul înapoi în OliEngine
        };

        registry[L"DB_DISCONNECT"] = [](const std::vector<vData>& args) -> vData {
            std::lock_guard<std::mutex> lock(g_dbMutex);
            if (g_dbConn) {
                g_dbConn->closeDatabase();
                g_dbConn.reset(); // Eliberează memoria
            }
            return vData{ L"DISCONNECTED" };
        };

        // Funcția: DB_FETCH_NEXT_ROW()
        registry[L"DB_FETCH_NEXT_ROW"] = [](const std::vector<vData>& args) -> vData {
            std::lock_guard<std::mutex> lock(g_dbMutex);

            if (!g_dbConn || !g_dbConn->isConnected()) {
                return vData{ std::wstring(L"ERROR: Not connected") };
            }

            bool ok = g_dbConn->fetchNextRow();

            vData result;
            result.value = ok;   // întoarcem true / false
            return result;
        };

        registry[L"DB_FETCH_ROW"] = [](const std::vector<vData>& args) -> vData {
            std::lock_guard<std::mutex> lock(g_dbMutex);

            if (!g_dbConn || !g_dbConn->isConnected()) {
                return vData{ std::wstring(L"ERROR: Not connected") };
            }

            // Preluăm rândul curent ca vector<wstring>
            auto row = g_dbConn->fetchRow();

            // Convertim vector<wstring> → vDataArray
            vDataArray arr;
            arr.reserve(row.size());

            for (const auto& cell : row) {
                vData cellData;
                cellData.value = cell;   // fiecare celulă devine vData(wstring)
                arr.push_back(cellData);
            }

            // Returnăm vData care conține un vDataArray
            vData result;
            result.value = arr;
            return result;
        };
        registry[L"DB_FETCH_MAP"] = [](const std::vector<vData>& args) -> vData {
            std::lock_guard<std::mutex> lock(g_dbMutex);

            if (!g_dbConn || !g_dbConn->isConnected()) {
                return vData{ std::wstring(L"ERROR: Not connected") };
            }

            // Preluăm rândul curent ca map<wstring, wstring>
            auto rowMap = g_dbConn->fetchMap();

            // Convertim map<wstring, wstring> → vDataMap
            vDataMap resultMap;

            for (const auto& [key, value] : rowMap) {
                vData cell;
                cell.value = value; // fiecare valoare devine vData(wstring)
                resultMap[key] = cell;
            }

            // Returnăm vData care conține un vDataMap
            vData result;
            result.value = resultMap;
            return result;
        };


    }
*/