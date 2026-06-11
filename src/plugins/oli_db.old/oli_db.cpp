#include "../../OliEngine.hpp"
#include "../../vData.hpp"
#include "../../ConsoleManager.hpp"
#include "dbConnection.hpp"
#include "dbManger.hpp"
#include "dbfConnection.hpp"
#include "csvConnection.hpp"

#if defined(_WIN32) || defined(_WIN64)
#define OLI_EXPORT extern "C" __declspec(dllexport)
#else
#define OLI_EXPORT extern "C" __attribute__((visibility("default")))
#endif

using PluginRegistry = std::unordered_map<std::wstring, OliFunctionHandler>;

// --- Helper pentru conversii ---
inline std::wstring vDataToWString(const vData &v)
{
    return v.toWString(); // Presupunând că vData are această metodă
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

        // 1. Verificăm dacă alias-ul este deja folosit
        if (DbManager::instance().hasConnection(alias))
        {
            return vData(L"ERR_ALIAS_EXISTS: Conexiunea '" + alias + L"' este deja deschisă.");
        }

        // 2. Instanțiem conexiunea folosind un unique_ptr local pentru siguranță
        std::unique_ptr<dbConnection> conn;

        if (type == L"DBF")
        {
            conn = std::make_unique<dbfConnection>("DBF_NATIVE", dsn);
        }
        else if (type == L"CSV")
        {
            conn = std::make_unique<csvConnection>("CSV_NATIVE", dsn);
        }
        else
        {
            return vData(L"ERR_UNKNOWN_TYPE");
        }

        // 3. Deschidere (folosim .get() pentru a accesa raw pointer-ul pentru openDatabase)
        if (!conn->openDatabase())
        {
            std::wstring err = conn->getError();
            return vData(L"ERR_OPEN_FAILED: " + err);
        }

        // 4. Înregistrare în manager (dacă addConnection reușește, managerul preia ownership-ul)
        if (!DbManager::instance().addConnection(alias, std::move(conn)))
        {
            return vData(L"ERR_INTERNAL: Nu s-a putut înregistra conexiunea.");
        }

        return vData(L"OK: Conectat ca '" + alias + L"'");
    };

    registry[L"DB_CLOSE"] = [](const std::vector<vData> &args) -> vData
    {
        // Dacă nu avem argumente, alias-ul este implicit "default"
        std::wstring alias = (args.size() == 0) ? L"default" : args[0].toWString();

        // Verificăm dacă există
        if (!DbManager::instance().hasConnection(alias))
        {
            return vData(L"ERR_NOT_FOUND: Conexiunea '" + alias + L"' nu există.");
        }

        // Eliminăm conexiunea (unique_ptr-ul din map va fi distrus automat,
        // deci destructorul dbConnection va fi apelat corect)
        if (DbManager::instance().removeConnection(alias))
        {
            return vData(L"OK: Conexiunea '" + alias + L"' a fost închisă.");
        }

        return vData(L"ERR_INTERNAL: Eroare la închiderea conexiunii.");
    };

    registry[L"DB_CLOSE"] = [](const std::vector<vData> &args) -> vData
    {
        std::wstring alias = (args.size() == 0) ? L"default" : args[0].toWString();

        // 1. Obținem conexiunea din manager
        dbConnection *conn = DbManager::instance().getConnection(alias);

        if (!conn)
        {
            return vData(L"ERR_NOT_FOUND: Conexiunea '" + alias + L"' nu există.");
        }

        // 2. Apelăm închiderea explicită (dacă nu e deja închisă)
        if (conn->isConnected())
        {
            conn->closeDatabase();
        }

        // 3. Eliminăm din manager (unique_ptr-ul se va ocupa de delete)
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
        if (!conn)
            return vData(L"ERR_NOT_FOUND");

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
        // Dacă nu are argumente, verifică alias-ul "default"
        std::wstring alias = (args.size() == 0) ? L"default" : args[0].toWString();

        dbConnection* conn = DbManager::instance().getConnection(alias);
        
        if (!conn) {
            return vData(false); // Nu există conexiunea, deci e deconectat
        }

        return vData(conn->isConnected());
    };

    registry[L"DB_STATUS"] = [](const std::vector<vData>& args) -> vData {
        std::wstring alias = (args.size() == 0) ? L"default" : args[0].toWString();
        dbConnection* conn = DbManager::instance().getConnection(alias);
        
        if (!conn) return vData(L"NOT_FOUND");
        return vData(conn->isConnected() ? L"CONNECTED" : L"DISCONNECTED");
    };

    registry[L"DB_EXEC"] = [](const std::vector<vData>& args) -> vData {
        // Sintaxă: db_query(query, [stm_name], [alias])
        // În acest fel, parametrii cei mai folosiți sunt primii
        
        if (args.size() < 1) {
            return vData(L"ERR_INVALID_ARGS: Cel puțin query-ul este necesar.");
        }

        std::wstring query = args[0].toWString();
        std::string stm_name = (args.size() >= 2) ? std::string(args[1].toWString().begin(), args[1].toWString().end()) : "default";
        std::wstring alias = (args.size() >= 3) ? args[2].toWString() : L"default";

        dbConnection* conn = DbManager::instance().getConnection(alias);
        if (!conn) {
            return vData(L"ERR_CONN_NOT_FOUND: '" + alias + L"'");
        }

        if (conn->execQuery(query, stm_name)) {
            // Returnăm numărul de rânduri, util pentru verificare imediată
            return vData((long long)conn->getRowCount(stm_name));
        } else {
            return vData(L"ERR_EXEC_FAILED: " + conn->getError());
        }
    };

    registry[L"DB_NEXT"] = [](const std::vector<vData>& args) -> vData {
        // Sintaxă: db_next([stm_name], [alias])
        std::string stm_name = (args.size() >= 1) ? std::string(args[0].toWString().begin(), args[0].toWString().end()) : "default";
        std::wstring alias = (args.size() >= 2) ? args[1].toWString() : L"default";

        dbConnection* conn = DbManager::instance().getConnection(alias);
        if (!conn) {
            return vData(L"ERR_CONN_NOT_FOUND: '" + alias + L"'");
        }

        // Returnăm true/false dacă s-a reușit avansarea
        return vData(conn->fetchNextRow(stm_name));
    };

    registry[L"DB_FETCH"] = [](const std::vector<vData>& args) -> vData {
        // Sintaxă: db_fetch([stm_name], [alias])
        std::string stm_name = (args.size() >= 1) ? std::string(args[0].toWString().begin(), args[0].toWString().end()) : "default";
        std::wstring alias = (args.size() >= 2) ? args[1].toWString() : L"default";

        dbConnection* conn = DbManager::instance().getConnection(alias);
        if (!conn) {
            return vData(L"ERR_CONN_NOT_FOUND");
        }

        // Obținem map-ul cu datele rândului curent
        std::map<std::wstring, std::wstring> row = conn->fetchMap(stm_name);
        
        // Convertim std::map-ul în vData (Map)
        // Presupunând că vData poate fi creat din Map-ul tău intern
        auto mapData = vData::CreateMap();
        auto* rawMap = mapData.rawMap(); 
        
        for (const auto& [key, value] : row) {
            (*rawMap)[key] = vData(value);
        }

        return mapData;
    };
}

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