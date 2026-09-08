#include "../../OliEngine.hpp"
#include "../../vData.hpp"
#include "../../ConsoleManager.hpp"
#include "dbConnection.hpp"
#include "dbManger.hpp"
#include "dbfConnection.hpp"
#include "csvConnection.hpp"
#include "odbcConnection.hpp"

#if defined(_WIN32) || defined(_WIN64)
#define OLI_EXPORT extern "C" __declspec(dllexport)
#else
#define OLI_EXPORT extern "C" __attribute__((visibility("default")))
#endif

std::wstring TypeToWString(vNativeDataType type) {
    switch (type) {
        case vNativeDataType::V_INTEGER: return L"INTEGER";
        case vNativeDataType::V_BIGINT:  return L"BIGINT";
        case vNativeDataType::V_DOUBLE:  return L"DOUBLE";
        case vNativeDataType::V_TEXT:    return L"TEXT";
        case vNativeDataType::V_DATE:    return L"DATE";
        case vNativeDataType::V_BOOLEAN: return L"BOOLEAN";
        case vNativeDataType::V_BLOB:    return L"BLOB";
        default:                         return L"UNKNOWN";
    }
}

using PluginRegistry = std::unordered_map<std::wstring, OliFunctionHandler>;

// --- Helper siguranță memorie C++ (Previne temporaries dangling iterators) ---
inline std::string wstrToStr(const std::wstring &w)
{
    return std::string(w.begin(), w.end());
}

inline std::wstring vDataToWString(const vData &v)
{
    return v.toWString();
}

void RegisterSystemFunctions(PluginRegistry &registry)
{

    registry[L"DB_CON"] = [](const std::vector<vData> &args) -> vData
    {
        if (args.size() < 2)
        {
            return vData(L"ERR_INVALID_ARGS: Necesită [tip, dsn] sau [nume, tip, dsn]");
        }

        std::wstring alias, type, dsn;

        if (args.size() == 2)
        {
            alias = L"default";
            type = to_upper(args[0].toWString());
            dsn = args[1].toWString();
        }
        else
        {
            alias = args[0].toWString();
            type = to_upper(args[1].toWString());
            dsn = args[2].toWString();
        }

        if (DbManager::instance().hasConnection(alias))
        {
            return vData(L"ERR_ALIAS_EXISTS: Conexiunea '" + alias + L"' este deja deschisă.");
        }

        std::unique_ptr<dbConnection> conn;

        if (type == L"DBF")
        {
            conn = std::make_unique<dbfConnection>("DBF_NATIVE", dsn);
        }
        else if (type == L"CSV")
        {
            conn = std::make_unique<csvConnection>("CSV_NATIVE", dsn);
        }
        else if (type == L"ODBC")
        {
            conn = std::make_unique<odbcConnection>("ODBC_NATIVE", dsn);
        }
        else
        {
            return vData(L"ERR_UNKNOWN_TYPE");
        }

        if (!conn->openDatabase())
        {
            std::wstring err = conn->getError();
            return vData(L"ERR_OPEN_FAILED: " + err);
        }

        if (!DbManager::instance().addConnection(alias, std::move(conn)))
        {
            return vData(L"ERR_INTERNAL: Nu s-a putut înregistra conexiunea.");
        }

        return vData(L"OK: Conectat ca '" + alias + L"'");
    };

    registry[L"DB_CLOSE"] = [](const std::vector<vData> &args) -> vData
    {
        std::wstring alias = (args.size() == 0) ? L"default" : args[0].toWString();

        dbConnection *conn = DbManager::instance().getConnection(alias);
        if (!conn)
        {
            return vData(L"ERR_NOT_FOUND: Conexiunea '" + alias + L"' nu există.");
        }

        if (conn->isConnected())
        {
            conn->closeDatabase();
        }

        if (DbManager::instance().removeConnection(alias))
        {
            return vData(L"OK: Conexiunea '" + alias + L"' a fost închisă.");
        }

        return vData(L"ERR_INTERNAL");
    };

    registry[L"DB_REOPEN"] = [](const std::vector<vData> &args) -> vData
    {
        std::wstring alias = (args.size() == 0) ? L"default" : args[0].toWString();

        dbConnection *conn = DbManager::instance().getConnection(alias);
        if (!conn) return vData(L"ERR_NOT_FOUND");

        if (conn->reconnect())
        {
            return vData(L"OK: Conexiune '" + alias + L"' restabilită.");
        }
        else
        {
            return vData(L"ERR_REOPEN_FAILED");
        }
    };

    registry[L"DB_IS_CONNECTED"] = [](const std::vector<vData>& args) -> vData {
        std::wstring alias = (args.size() == 0) ? L"default" : args[0].toWString();
        dbConnection* conn = DbManager::instance().getConnection(alias);
        
        if (!conn) return vData(false);
        return vData(conn->isConnected());
    };

    registry[L"DB_STATUS"] = [](const std::vector<vData>& args) -> vData {
        std::wstring alias = (args.size() == 0) ? L"default" : args[0].toWString();
        dbConnection* conn = DbManager::instance().getConnection(alias);
        
        if (!conn) return vData(L"NOT_FOUND");
        return vData(conn->isConnected() ? L"CONNECTED" : L"DISCONNECTED");
    };

    registry[L"DB_EXEC"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 1) {
            return vData(L"ERR_INVALID_ARGS: Cel puțin query-ul este necesar.");
        }

        std::wstring query = args[0].toWString();
        std::string stm_name = (args.size() >= 2) ? wstrToStr(args[1].toWString()) : "default";
        std::wstring alias = (args.size() >= 3) ? args[2].toWString() : L"default";

        dbConnection* conn = DbManager::instance().getConnection(alias);
        if (!conn) {
            return vData(L"ERR_CONN_NOT_FOUND: '" + alias + L"'");
        }

        if (conn->execQuery(query, stm_name)) {
            return vData((long long)conn->getRowCount(stm_name));
        } else {
            return vData(L"ERR_EXEC_FAILED: " + conn->getError());
        }
    };

    registry[L"DB_NEXT"] = [](const std::vector<vData>& args) -> vData {
        std::string stm_name = (args.size() >= 1) ? wstrToStr(args[0].toWString()) : "default";
        std::wstring alias = (args.size() >= 2) ? args[1].toWString() : L"default";

        dbConnection* conn = DbManager::instance().getConnection(alias);
        if (!conn) {
            return vData(L"ERR_CONN_NOT_FOUND: '" + alias + L"'");
        }

        return vData(conn->fetchNextRow(stm_name));
    };

    registry[L"DB_FETCH"] = [](const std::vector<vData>& args) -> vData {
        std::string stm_name = (args.size() >= 1) ? wstrToStr(args[0].toWString()) : "default";
        std::wstring alias = (args.size() >= 2) ? args[1].toWString() : L"default";

        dbConnection* conn = DbManager::instance().getConnection(alias);
        if (!conn) return vData(L"ERR_CONN_NOT_FOUND");

        std::map<std::wstring, std::wstring> row = conn->fetchMap(stm_name);
        
        auto mapData = vData::CreateMap();
        auto* rawMap = mapData.rawMap(); 
        
        for (const auto& [key, value] : row) {
            (*rawMap)[key] = vData(value);
        }

        return mapData;
    };

    registry[L"DB_COUNT"] = [](const std::vector<vData>& args) -> vData {
        std::string stm_name = (args.size() >= 1) ? wstrToStr(args[0].toWString()) : "default";
        std::wstring alias = (args.size() >= 2) ? args[1].toWString() : L"default";

        dbConnection* conn = DbManager::instance().getConnection(alias);
        if (!conn) return vData(L"ERR_CONN_NOT_FOUND");

        return vData((long long)conn->getRowCount(stm_name));
    };

    registry[L"DB_COLUMNS"] = [](const std::vector<vData>& args) -> vData {
        std::string stm_name = (args.size() >= 1) ? wstrToStr(args[0].toWString()) : "default";
        std::wstring alias = (args.size() >= 2) ? args[1].toWString() : L"default";

        dbConnection* conn = DbManager::instance().getConnection(alias);
        if (!conn) return vData(L"ERR_CONN_NOT_FOUND");

        const std::vector<std::wstring>& names = conn->getColumnNames(stm_name);
        
        auto result = vData::CreateArray();
        auto* arr = result.rawArray();
        
        for (const auto& name : names) {
            arr->push_back(vData(name));
        }
        
        return result;
    };

    registry[L"DB_TYPES"] = [](const std::vector<vData>& args) -> vData {
        std::string stm_name = (args.size() >= 1) ? wstrToStr(args[0].toWString()) : "default";
        std::wstring alias = (args.size() >= 2) ? args[1].toWString() : L"default";

        dbConnection* conn = DbManager::instance().getConnection(alias);
        if (!conn) return vData(L"ERR_CONN_NOT_FOUND");

        const auto& types = conn->getColumnTypes(stm_name);

        auto result = vData::CreateArray();
        auto* arr = result.rawArray();

        for (const auto& type : types) {
            switch (type) {
                case vNativeDataType::V_INTEGER: arr->push_back(vData(L"INTEGER")); break;
                case vNativeDataType::V_BIGINT:  arr->push_back(vData(L"BIGINT")); break;
                case vNativeDataType::V_DOUBLE:  arr->push_back(vData(L"DOUBLE")); break;
                case vNativeDataType::V_TEXT:    arr->push_back(vData(L"TEXT")); break;
                case vNativeDataType::V_DATE:    arr->push_back(vData(L"DATE")); break;
                case vNativeDataType::V_BOOLEAN: arr->push_back(vData(L"BOOLEAN")); break;
                case vNativeDataType::V_BLOB:    arr->push_back(vData(L"BLOB")); break;
                default:                         arr->push_back(vData(L"UNKNOWN")); break;
            }
        }
        return result;
    };

    registry[L"DB_COLUMNS_INFO"] = [](const std::vector<vData>& args) -> vData {
        std::string stm_name = (args.size() >= 1) ? wstrToStr(args[0].toWString()) : "default";
        std::wstring alias = (args.size() >= 2) ? args[1].toWString() : L"default";

        dbConnection* conn = DbManager::instance().getConnection(alias);
        if (!conn) return vData(L"ERR_CONN_NOT_FOUND");

        const auto& infoList = conn->getColumnsInfo(stm_name);
        
        auto resultArray = vData::CreateArray();
        auto* arr = resultArray.rawArray();

        for (const auto& info : infoList) {
            auto colMap = vData::CreateMap();
            auto* rawMap = colMap.rawMap();

            (*rawMap)[L"NAME"] = vData(info.name);
            (*rawMap)[L"TYPE"] = vData(TypeToWString(info.type)); 
            (*rawMap)[L"LENGTH"] = vData((long long)info.length);
            (*rawMap)[L"NULLABLE"] = vData(info.isNullable);

            arr->push_back(colMap);
        }

        return resultArray;
    };

    registry[L"DB_FETCH_FIELD"] = [](const std::vector<vData>& args) -> vData {
        if (args.empty()) return vData(L"");

        std::string stm_name = (args.size() >= 2) ? wstrToStr(args[1].toWString()) : "default";
        std::wstring alias = (args.size() >= 3) ? args[2].toWString() : L"default";

        dbConnection* conn = DbManager::instance().getConnection(alias);
        if (!conn) return vData(L"");

        if (args[0].isInt() || args[0].isFloat()) {
            int fieldNo = (int)args[0].toInt();
            return vData(conn->fetchFieldByNumber(fieldNo, stm_name));
        } 
        else {
            std::wstring fieldName = args[0].toWString();
            return vData(conn->fetchFieldByName(fieldName, stm_name));
        }
    };

    registry[L"DB_FETCH_ROW"] = [](const std::vector<vData>& args) -> vData {
        std::string stm_name = (args.size() >= 1) ? wstrToStr(args[0].toWString()) : "default";
        std::wstring alias = (args.size() >= 2) ? args[1].toWString() : L"default";

        dbConnection* conn = DbManager::instance().getConnection(alias);
        if (!conn) return vData::CreateArray();

        std::vector<std::wstring> row = conn->fetchRow(stm_name);
        
        auto resultArray = vData::CreateArray();
        auto* arr = resultArray.rawArray();
        
        for (const auto& val : row) {
            arr->push_back(vData(val));
        }
        
        return resultArray;
    };

    registry[L"DB_GET_ERROR"] = [](const std::vector<vData>& args) -> vData {
        std::wstring alias = (args.size() >= 1) ? args[0].toWString() : L"default";
        
        dbConnection* conn = DbManager::instance().getConnection(alias);
        if (!conn) return vData(L"Conexiune inexistentă");

        return vData(conn->getError());
    };

    registry[L"DB_FETCH_ALL"] = [](const std::vector<vData>& args) -> vData {
        std::string stm_name = (args.size() >= 1) ? wstrToStr(args[0].toWString()) : "default";
        std::wstring alias = (args.size() >= 2) ? args[1].toWString() : L"default";

        dbConnection* conn = DbManager::instance().getConnection(alias);
        if (!conn) return vData::CreateArray();

        auto resultArray = vData::CreateArray();
        auto* arr = resultArray.rawArray();

        vConResult res = conn->getLastQueryResult();
        if (!res.table.records.empty()) {
            for (const auto& record : res.table.records) {
                auto rowMap = vData::CreateMap();
                auto* rawMap = rowMap.rawMap();

                for (size_t i = 0; i < res.table.columns.size(); ++i) {
                    std::wstring colName = res.table.columns[i];
                    std::wstring val = (i < record.size()) ? record[i] : L"";
                    (*rawMap)[colName] = vData(val);
                }
                arr->push_back(rowMap);
            }
        } else {
            while (conn->fetchNextRow(stm_name)) {
                std::map<std::wstring, std::wstring> row = conn->fetchMap(stm_name);
                auto rowMap = vData::CreateMap();
                auto* rawMap = rowMap.rawMap();

                for (const auto& [key, value] : row) {
                    (*rawMap)[key] = vData(value);
                }
                arr->push_back(rowMap);
            }
        }

        return resultArray;
    };
}

// --- EXPORT INTERFACE ---
extern "C" {

    OLI_EXPORT void LoadOliPlugin(PluginRegistry &registry)
    {
        RegisterSystemFunctions(registry);
    }

    OLI_EXPORT void SetPluginConsoleManager(ConsoleManager *hostCm)
    {
        if (hostCm != nullptr)
        {
            ConsoleManager::setInstance(hostCm);
        }
    }

}