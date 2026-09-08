#include "odbcConnection.hpp"
#include "../../StringUtils.hpp"
#include "../../ConsoleManager.hpp" 

#include <iostream>
#include <iomanip> 

odbcConnection::odbcConnection(const std::string& type, const std::wstring& dsn)
    : type(type), dsn(dsn), henv(SQL_NULL_HENV), hdbc(SQL_NULL_HDBC), connected(false) {

}

odbcConnection::~odbcConnection() {
    closeDatabase();
}

bool odbcConnection::openDatabase() {
    // Declarații de logare
    std::wstring logMsg;

    logMsg = L"odbcConnection::openDatabase: Începe deschiderea conexiunii DSN: " + dsn;
    //LOG_INFO(logMsg);

    // 1. Alocare handler pentru mediu (henv)
    if (SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv) != SQL_SUCCESS) {
        LOG_FATAL(L"[FATAL ERROR] odbcConnection::openDatabase: Eșec la SQLAllocHandle(ENV).");
        // Nu avem handle valid de raportat
        return false;
    }

    // 2. Setare atribute mediu (ODBC Version)
    if (SQLSetEnvAttr(henv, SQL_ATTR_ODBC_VERSION, (void*)SQL_OV_ODBC3, 0) != SQL_SUCCESS) {
        logMsg = L"[ERROR] odbcConnection::openDatabase: Eșec la SQLSetEnvAttr(ODBC3).";
        LOG_ERROR(logMsg);
        showError(SQL_HANDLE_ENV, henv);
        SQLFreeHandle(SQL_HANDLE_ENV, henv); // Curățare
        return false;
    }

    // 3. Alocare handler pentru conexiune (hdbc)
    if (SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc) != SQL_SUCCESS) {
        logMsg = L"[ERROR] odbcConnection::openDatabase: Eșec la SQLAllocHandle(DBC).";
        LOG_ERROR(logMsg);
        showError(SQL_HANDLE_ENV, henv); // Eroarea se raportează pe handle-ul părinte (ENV)
        SQLFreeHandle(SQL_HANDLE_ENV, henv); // Curățare
        return false;
    }

    // 4. Setare atribute conexiune
    // Timeout de login (cât să aștepte deschiderea conexiunii)
    SQLSetConnectAttr(hdbc, SQL_LOGIN_TIMEOUT, (SQLPOINTER)5, 0);
    // Comenzi SQL sunt executate imediat (nu trebuie commit manual)
    SQLSetConnectAttr(hdbc, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER)SQL_AUTOCOMMIT_ON, 0);
    // IMPORTANT: Timeout de comandă (pentru interogări) pentru a evita blocarea pe conexiuni moarte
    SQLSetConnectAttr(hdbc, SQL_ATTR_QUERY_TIMEOUT, (SQLPOINTER)30, 0);

    // 5. Conectare la DSN
    std::wstring odbcDsn = L"DSN=" + dsn;
    SQLRETURN retcode = SQLDriverConnectW(
        hdbc,
        GetDesktopWindow(), // Fereastra părinte
        (SQLWCHAR*)odbcDsn.c_str(),
        SQL_NTS,
        NULL,
        0,
        NULL,
        SQL_DRIVER_NOPROMPT
    );

    // 6. Verificare rezultat
    if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
        connected = true;
        logMsg = L"odbcConnection::openDatabase: Conectare reușită la DSN: " + dsn;
        LOG_SUCCESS(logMsg);
        return true;
    }
    else {
        logMsg = L"[FATAL ERROR] odbcConnection::openDatabase: Eșec la SQLDriverConnectW.";
        LOG_FATAL(logMsg);
        showError(SQL_HANDLE_DBC, hdbc); // Eroarea de conectare se raportează pe handle-ul DBC
        closeDatabase(); // Funcție care trebuie să facă SQLFreeHandle pe ambele handle-uri
        return false;
    }
}

bool odbcConnection::allocStatementHandle(std::string stm_name) {
    // Conversia numelui statement-ului la wstring pentru logare
    std::wstring w_stm_name(stm_name.begin(), stm_name.end());
    std::wstring logMsg;

    // 1. Verifică dacă handle-ul există deja și este valid
    if (hstmts.count(stm_name) > 0 && hstmts.at(stm_name) != SQL_NULL_HSTMT) {
        logMsg = L"odbcConnection::allocStatementHandle: Handle-ul statement-ului '" + w_stm_name + L"' este deja alocat.";
        //LOG_INFO(logMsg);
        return true;
    }

    // 2. Alocă handler-ul direct în mapă.
    // Inițializăm intrarea în mapă cu cheia stm_name
    SQLRETURN retcode = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmts[stm_name]);

    // 3. Verificare rezultat
    if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
        logMsg = L"odbcConnection::allocStatementHandle: Alocare reușită pentru statement-ul: " + w_stm_name;
        //LOG_SUCCESS(logMsg);
        // Nu afectează 'connected'.
        return true;
    }
    else {
        // Alocarea a eșuat. Setează handle-ul la NULL și loghează eroarea.
        // Asigură-te că intrarea din mapă nu conține un pointer vechi invalid.
        hstmts[stm_name] = SQL_NULL_HSTMT;

        logMsg = L"odbcConnection::allocStatementHandle: Eșec la alocarea statement-ului: " + w_stm_name;
        LOG_ERROR(logMsg);

        // Eroarea de alocare a statement-ului este raportată pe handle-ul părinte (DBC)
        showError(SQL_HANDLE_DBC, hdbc);

        // Nu închide baza de date de aici, este o problemă specifică statement-ului.
        return false;
    }
}

void odbcConnection::closeDatabase() {
    // Declarații de logare (numele DSN este disponibil în membrul 'dsn')
    std::wstring w_dsn = dsn;
    std::wstring logMsg;

    logMsg = L"odbcConnection::closeDatabase: Inițiază închiderea conexiunii DSN: " + w_dsn;
    //LOG_INFO(logMsg);

    // 1. Eliberează handle-urile statement (HSTMT)
    // Se iterează prin mapă, eliberând handle-ul ODBC asociat.
    // CHEILE din mapă (numele statement-urilor) sunt păstrate pentru reconectare.
    int freed_stmts = 0;
    for (auto& pair : hstmts) {
        if (pair.second != SQL_NULL_HSTMT) {
            SQLFreeHandle(SQL_HANDLE_STMT, pair.second);
            pair.second = SQL_NULL_HSTMT; // Setează la NULL pentru a indica stare inactivă
            freed_stmts++;
        }
    }

    logMsg = L"odbcConnection::closeDatabase: Au fost eliberate " + std::to_wstring(freed_stmts) + L" handle-uri de statement.";
    //LOG_INFO(logMsg);


    // 2. Eliberează handle-ul conexiunii (HDBC)
    if (hdbc != SQL_NULL_HDBC) {
        // Deconectează-te explicit de la sursa de date
        SQLDisconnect(hdbc);
        //LOG_INFO(L"odbcConnection::closeDatabase: SQLDisconnect apelat.");

        // Eliberează handle-ul DBC
        SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
        hdbc = SQL_NULL_HDBC;
    }

    // 3. Eliberează handle-ul mediului (HENV)
    if (henv != SQL_NULL_HENV) {
        SQLFreeHandle(SQL_HANDLE_ENV, henv);
        henv = SQL_NULL_HENV;
    }

    // 4. Actualizează starea internă
    connected = false;
    LOG_SUCCESS(L"odbcConnection::closeDatabase: Conexiunea cu DSN " + w_dsn + L" a fost închisă și resursele eliberate.");


    // 5. Curăță Metadatele (le păstrăm doar pe durata vieții unei conexiuni)
    RowCountPtrs.clear();
    ColumnCountPtrs.clear();
    colNames.clear();
    colTypes.clear();
    colSizes.clear();
    colNameIndexes.clear();
    // NOTĂ: hstmts.clear() NU se face aici, deoarece vrem să păstrăm cheile (numele statement-urilor)
    // pentru a le realoca în reconnect().
}

bool odbcConnection::isConnected() const {
    return connected;
}

void odbcConnection::showError(SQLSMALLINT handleType, SQLHANDLE handle) {
    SQLWCHAR state[6], message[256];
    SQLINTEGER nativeError;
    SQLSMALLINT messageLen;

    // Asigură-te că error (membrul clasei) este curățat înainte de a începe
    clearError();

    // Obține înregistrarea de diagnosticare numărul 1
    SQLRETURN retcode = SQLGetDiagRecW(
        handleType,
        handle,
        1, // Numărul înregistrării (începe de la 1)
        state,
        &nativeError,
        message,
        sizeof(message) / sizeof(SQLWCHAR),
        &messageLen
    );

    // Verifică succesul apelului SQLGetDiagRecW
    if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
        // Creează mesajul de eroare complet (pentru membrul clasei 'error')
        std::wstring w_state(state);
        std::wstring w_message(message);

        // Formatează mesajul complet pentru logare internă (membrul 'error')
        error = L"ODBC State: " + w_state + L", Native: " + std::to_wstring(nativeError) + L", Message: " + w_message;

        // Logarea în consolă folosind macro-ul LOG_ERROR
        // Logarea include tipul de handle (pentru context) și detaliile erorii

        std::wstring logMsg = L"ODBC (" + std::wstring(handleType == SQL_HANDLE_DBC ? L"DBC" : (handleType == SQL_HANDLE_STMT ? L"STMT" : L"ENV")) +
            L"): State=" + w_state + L", Native=" + std::to_wstring(nativeError) + L", Msg=" + w_message;

        // Loghează eroarea folosind sistemul tău.
        // Mesajele de eroare sunt adesea multi-line în log-uri, deci putem folosi un singur LOG_ERROR.
        LOG_ERROR(logMsg);
    }
    else {
        // SQLGetDiagRecW a eșuat.
        error = L"Eroare necunoscută: SQLGetDiagRecW a eșuat. Retcode: " + std::to_wstring(retcode);
        LOG_FATAL(error); // Loghează eșecul de diagnoză ca o eroare fatală de sistem
    }
}


void odbcConnection::clearError(){
    error.clear();
}

std::wstring odbcConnection::getError(){
    return error;
}

/**
 * @brief Execută o interogare SQL directă (non-preparată). Include logică de reîncercare
 * în cazul în care serverul a închis conexiunea (timeout de inactivitate).
 * @param query Interogarea SQL de executat (std::wstring).
 * @param stm_name Numele statement-ului (cheia în mapa hstmts) de utilizat.
 * @return true dacă interogarea a fost executată cu succes și metadatele sunt setate, false altfel.
 */

/*
bool odbcConnection::execQuery(const std::wstring& query, std::string stm_name) {
    // Conversia numelui statement-ului pentru logare.
    std::wstring w_stm_name(stm_name.begin(), stm_name.end());
    std::wstring logMsg;

    // 1. Asigură-te că handle-ul de statement este alocat.
    if (hstmts.find(stm_name) == hstmts.end()) {
        logMsg = L"odbcConnection::execQuery: Statement-ul '" + w_stm_name + L"' nu este alocat. Încerc alocarea...";
        //LOG_INFO(logMsg);

        if (!allocStatementHandle(stm_name)) {
            LOG_ERROR(L"odbcConnection::execQuery: Eșec la alocarea handle-ului pentru: " + w_stm_name);
            return false;
        }
    }

    SQLHSTMT hstmt = hstmts[stm_name];
    SQLRETURN retcode;
    const int MAX_RETRIES = 1; // 0 = Încercare inițială; 1 = Reîncercare după reconectare

    // Buclă de reîncercare pentru a gestiona conexiunea pierdută.
    for (int retry = 0; retry <= MAX_RETRIES; ++retry) {

        // Curăță erorile din DBC și STM înainte de a încerca execuția.
        clearError();

        // 2. Închide orice rezultat anterior și execută interogarea.
        SQLFreeStmt(hstmt, SQL_CLOSE); // Eliberează rezultatele anterioare (rânduri, etc.)
        retcode = SQLExecDirectW(hstmt, (SQLWCHAR*)query.c_str(), SQL_NTS);

        if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
            // Pasul A: Succes. Procesează metadatele.
            logMsg = L"odbcConnection::execQuery: Execuție reușită (Tentativa " + std::to_wstring(retry) + L") pe " + w_stm_name;
            //LOG_SUCCESS(logMsg);

            // Extrage și stochează metadatele (număr de coloane, nume, număr de rânduri)
            SQLSMALLINT ColumnCountPtr;
            SQLNumResultCols(hstmt, &ColumnCountPtr);
            ColumnCountPtrs[stm_name] = ColumnCountPtr;

            colNames[stm_name].clear();
            if (!setColNames(stm_name)) return false; // Trebuie să implementezi setColNames!

            SQLLEN rowCount;
            SQLRowCount(hstmt, &rowCount);
            RowCountPtrs[stm_name] = rowCount;

            return true; // Ieși cu succes.
        }

        // 3. Eșec: Verifică dacă este o eroare de conexiune pierdută.
        if (retry < MAX_RETRIES) {

            // Verifică dacă eroarea indică o conexiune pierdută (ex: "server closed the connection unexpectedly")
            if (isConnectionError(SQL_HANDLE_STMT, hstmt)) {
                LOG_WARNING(L"odbcConnection::execQuery: Conexiune pierdută detectată. Se încearcă reconectarea...");

                // Încearcă reconectarea (aceasta ar trebui să realoce hdbc și hstmt-urile necesare)
                if (reconnect()) {
                    //LOG_INFO(L"odbcConnection::execQuery: Reconectare reușită. Reîncerc executarea interogării.");
                    continue; // Reia bucla for (retry devine 1)
                }
            }
            else {
                // Loghează eroarea ODBC care nu este legată de conexiune (ex: eroare SQL)
                LOG_ERROR(L"odbcConnection::execQuery: Eroare ODBC la execuția interogării pe " + w_stm_name + L" (Tentativa " + std::to_wstring(retry) + L").");
                showError(SQL_HANDLE_STMT, hstmt);
            }
        }

        // Dacă nu mai sunt reîncercări sau nu a fost eroare de conexiune, ieși din buclă.
        break;
    }

    // 4. Final: Eșec total
    LOG_FATAL(L"odbcConnection::execQuery: Eșec irecuperabil la executarea interogării pe " + w_stm_name + L".");
    showError(SQL_HANDLE_STMT, hstmt);
    LOG_ERROR(query);
    return false;
}
*/
bool odbcConnection::execQuery(const std::wstring& query, std::string stm_name) {
    // Conversia numelui statement-ului pentru logare.
    std::wstring w_stm_name(stm_name.begin(), stm_name.end());
    std::wstring logMsg;

    // 1. Asigură-te că handle-ul de statement este alocat.
    if (hstmts.find(stm_name) == hstmts.end()) {
        logMsg = L"odbcConnection::execQuery: Statement-ul '" + w_stm_name + L"' nu este alocat. Încerc alocarea...";
        //LOG_INFO(logMsg);

        if (!allocStatementHandle(stm_name)) {
            LOG_ERROR(L"odbcConnection::execQuery: Eșec la alocarea handle-ului pentru: " + w_stm_name);
            return false;
        }
    }

    SQLHSTMT hstmt = hstmts[stm_name];
    SQLRETURN retcode;
    const int MAX_RETRIES = 1; // 0 = Încercare inițială; 1 = Reîncercare după reconectare

    // Buclă de reîncercare pentru a gestiona conexiunea pierdută.
    for (int retry = 0; retry <= MAX_RETRIES; ++retry) {

        // Curăță erorile din DBC și STM înainte de a încerca execuția.
        clearError();

        // 2. Închide orice rezultat anterior și execută interogarea.
        SQLFreeStmt(hstmt, SQL_CLOSE); // Eliberează rezultatele anterioare (rânduri, etc.)
        retcode = SQLExecDirectW(hstmt, (SQLWCHAR*)query.c_str(), SQL_NTS);

        // --- MODIFICARE CRUCIALĂ AICI: Adăugat retcode == SQL_NO_DATA (100) ---
        if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO || retcode == SQL_NO_DATA) {
            // Pasul A: Succes. Procesează metadatele.
            logMsg = L"odbcConnection::execQuery: Execuție reușită (Tentativa " + std::to_wstring(retry) + L") pe " + w_stm_name;
            if (retcode == SQL_NO_DATA) {
                logMsg += L" (Atenție: 0 rânduri afectate/returnate)";
            }
            //LOG_SUCCESS(logMsg);

            // Extrage și stochează metadatele (număr de coloane, nume, număr de rânduri)
            SQLSMALLINT ColumnCountPtr = 0;
            SQLNumResultCols(hstmt, &ColumnCountPtr);
            ColumnCountPtrs[stm_name] = ColumnCountPtr;

            colNames[stm_name].clear();

            // PROTECȚIE: Apelăm setColNames DOAR dacă query-ul a returnat efectiv structură de tabelă (ex: un SELECT)
            if (ColumnCountPtr > 0) {
                if (!setColNames(stm_name)) return false;
            }

            SQLLEN rowCount = 0;
            SQLRowCount(hstmt, &rowCount);

            // Dacă retcode este SQL_NO_DATA, forțăm rowCount la 0 în caz că driverul raportează ciudat
            if (retcode == SQL_NO_DATA) {
                rowCount = 0;
            }
            RowCountPtrs[stm_name] = rowCount;

            return true; // Ieși cu succes deplin!
        }

        // 3. Eșec real: Verifică dacă este o eroare de conexiune pierdută.
        if (retry < MAX_RETRIES) {

            // Verifică dacă eroarea indică o conexiune pierdută (ex: "server closed the connection unexpectedly")
            if (isConnectionError(SQL_HANDLE_STMT, hstmt)) {
                LOG_WARNING(L"odbcConnection::execQuery: Conexiune pierdută detectată. Se încearcă reconectarea...");

                // Încearcă reconectarea (aceasta ar trebui să realoce hdbc și hstmt-urile necesare)
                if (reconnect()) {
                    continue; // Reia bucla for (retry devine 1)
                }
            }
            else {
                // Loghează eroarea ODBC care nu este legată de conexiune (ex: eroare SQL de sintaxă)
                LOG_ERROR(L"odbcConnection::execQuery: Eroare ODBC la execuția interogării pe " + w_stm_name + L" (Tentativa " + std::to_wstring(retry) + L").");
                showError(SQL_HANDLE_STMT, hstmt);
            }
        }

        // Dacă nu mai sunt reîncercări sau nu a fost eroare de conexiune, ieși din buclă.
        break;
    }

    // 4. Final: Eșec total (Doar pentru erori SQL reale, nu pentru SQL_NO_DATA)
    LOG_FATAL(L"odbcConnection::execQuery: Eșec irecuperabil la executarea interogării pe " + w_stm_name + L".");
    showError(SQL_HANDLE_STMT, hstmt);
    LOG_ERROR(query);
    return false;
}


/**
 * @brief Preia următorul rând din setul de rezultate al unui statement.
 *
 * @param stm_name Numele statement-ului (cheia HSTMT) de utilizat.
 * @return true dacă un rând a fost preluat cu succes, false dacă nu mai sunt rânduri (SQL_NO_DATA) sau a apărut o eroare.
 */

bool odbcConnection::fetchNextRow(std::string stm_name) {
    SQLHSTMT hstmt = hstmts[stm_name];
    SQLRETURN ret = SQLFetch(hstmt);
    std::wstring w_stm_name(stm_name.begin(), stm_name.end());

    // 1. Verifică succesul (rând preluat)
    if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO) {
       // LOG_INFO(L"odbcConnection::fetchNextRow: SQLFetch a REUSIT pentru " + w_stm_name); // <<< ADĂUGAT
        return true;
    }

    // 2. Verifică sfârșitul datelor
    if (ret == SQL_NO_DATA) {
        //LOG_INFO(L"odbcConnection::fetchNextRow: SQLFetch a returnat SQL_NO_DATA pentru " + w_stm_name); // <<< ADĂUGAT
        return false;
    }

    // 3. Eșec (Eroare reală)
    LOG_ERROR(L"odbcConnection::fetchNextRow: Eșec REAL la SQLFetch (Cod Retur: " + std::to_wstring(ret) + L") pentru " + w_stm_name); // <<< MODIFICAT
    showError(SQL_HANDLE_STMT, hstmt);
    return false;
}

int odbcConnection::getRowCount(std::string stm_name) {
    return RowCountPtrs[stm_name];
}

const std::vector<std::wstring>& odbcConnection::getColumnNames(std::string stm_name){
    return colNames[stm_name];
}

/**
 * @brief Extrage și stochează numele coloanelor pentru un statement executat.
 * * Această metodă iterează prin toate coloanele rezultatului folosind SQLDescribeColW
 * și stochează numele acestora în mapa 'colNames'.
 *
 * @param stm_name Numele statement-ului (cheia HSTMT) pentru care se extrag metadatele.
 * @return true dacă toate numele coloanelor au fost preluate cu succes, false altfel.
 */

bool odbcConnection::setColNames(std::string stm_name) {
    SQLHSTMT hstmt = hstmts[stm_name];
    std::wstring w_stm_name(stm_name.begin(), stm_name.end());
    std::wstring logMsg;

    // 1. Curățarea datelor vechi
    colNames[stm_name].clear();
    colTypes[stm_name].clear();
    colSizes[stm_name].clear();

    // 2. Obține numărul de coloane
    SQLSMALLINT column_count = ColumnCountPtrs[stm_name];
    logMsg = L"odbcConnection::setColNames: Începe preluarea numelor pentru " + w_stm_name +
        L" (Coloane: " + std::to_wstring(column_count) + L").";
    //LOG_INFO(logMsg);

    // 3. Iterează și preia detaliile fiecărei coloane
    for (SQLSMALLINT i = 1; i <= column_count; ++i) {
        SQLWCHAR columnName[256];
        SQLSMALLINT nameLength = 0;
        SQLSMALLINT colType = 0;
        SQLULEN colSize = 0;
        SQLSMALLINT decimalDigits = 0;
        SQLSMALLINT nullable = 0;

        SQLRETURN ret = SQLDescribeColW(
            hstmt,
            i,
            columnName,
            sizeof(columnName) / sizeof(SQLWCHAR),
            &nameLength,
            &colType,
            &colSize,
            &decimalDigits,
            &nullable
        );

        if (SQL_SUCCEEDED(ret)) {
            // Salvează numele, tipul și dimensiunea coloanei
            colNames[stm_name].push_back(std::wstring(columnName, nameLength));
            colTypes[stm_name].push_back(colType);
            colSizes[stm_name].push_back(colSize);
            colNameIndexes[stm_name][columnName] = i;
        }
        else {
            logMsg = L"odbcConnection::setColNames: Eșec la SQLDescribeColW pentru coloana " +
                std::to_wstring(i) + L" pe statement-ul: " + w_stm_name;
            LOG_ERROR(logMsg);
            showError(SQL_HANDLE_STMT, hstmt);
            return false;
        }
    }

    logMsg = L"odbcConnection::setColNames: Numele și tipurile coloanelor au fost preluate cu succes pentru: " + w_stm_name;
    //LOG_SUCCESS(logMsg);

    return true;
}



/**
 * @brief Preia valoarea unui câmp specificat dintr-un rând deja preluat de SQLFetch.
 *
 * @param fieldNo Numărul (indexul) câmpului de preluat (începe de la 1).
 * @param stm_name Numele statement-ului (cheia HSTMT) pentru a prelua valoarea.
 * @return std::wstring Valoarea câmpului sub formă de șir Unicode. Returnează un șir gol ("") în caz de eșec sau valoare NULL.
 */

std::wstring odbcConnection::fetchFieldByNumber(int fieldNo, std::string stm_name) {

    SQLHSTMT hstmt = hstmts[stm_name];
    std::wstring w_stm_name(stm_name.begin(), stm_name.end());

    if (!hstmt) {
        LOG_ERROR(L"odbcConnection::fetchFieldByNumber: Statement invalid pentru " + w_stm_name);
        return L"";
    }

    // Verifică existența tipului coloanei
    if (colTypes.find(stm_name) == colTypes.end() || fieldNo < 1 || fieldNo > static_cast<int>(colTypes[stm_name].size())) {
        LOG_ERROR(L"odbcConnection::fetchFieldByNumber: Tipul coloanei nu este disponibil pentru câmpul " + std::to_wstring(fieldNo) + L" din " + w_stm_name);
        return L"";
    }

    SQLSMALLINT colType = colTypes[stm_name][fieldNo - 1];
    SQLLEN indicator = 0;
    SQLRETURN ret;

    // 🔢 Tipuri numerice întregi 
    if (colType == SQL_INTEGER || colType == SQL_SMALLINT || colType == SQL_BIGINT ) { 
        SQLINTEGER value = 0;
        ret = SQLGetData(hstmt, fieldNo, SQL_C_LONG, &value, sizeof(value), &indicator);
        if ((ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO) && indicator != SQL_NULL_DATA)
            return std::to_wstring(value);
    }

    // 🔢 Tipuri numerice reale  dar SQL_NUMERIC AR TREBUII TRATAT SEPARAT
    else if (colType == SQL_DOUBLE || colType == SQL_REAL || colType == SQL_FLOAT ) {
        double value = 0.0;
        ret = SQLGetData(hstmt, fieldNo, SQL_C_DOUBLE, &value, sizeof(value), &indicator);
        if ((ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO) && indicator != SQL_NULL_DATA) {
            std::wstringstream ss;
            ss << value;
            return trim_zeros(ss.str(),2);
        }
    }
   

    // 📅 Tipuri de timp și dată
    else if (colType == SQL_TYPE_DATE || colType == SQL_TYPE_TIME || colType == SQL_TYPE_TIMESTAMP) {
        SQL_TIMESTAMP_STRUCT ts = {};
        ret = SQLGetData(hstmt, fieldNo, SQL_C_TYPE_TIMESTAMP, &ts, sizeof(ts), &indicator);
        if ((ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO) && indicator != SQL_NULL_DATA) {
            wchar_t buffer[64];
            swprintf_s(buffer, 64, L"%04d-%02d-%02d %02d:%02d:%02d",
                ts.year, ts.month, ts.day, ts.hour, ts.minute, ts.second);
            return std::wstring(buffer);
        }
    }

    // 🔤 Tipuri text
    /*
    else if (colType == SQL_VARCHAR || colType == SQL_WVARCHAR || colType == SQL_CHAR || colType == SQL_WCHAR) {
        // Verifică existența dimensiunii coloanei
        if (colSizes.find(stm_name) == colSizes.end() || fieldNo < 1 || fieldNo > static_cast<int>(colSizes[stm_name].size())) {
            LOG_ERROR(L"odbcConnection::fetchFieldByNumber: Dimensiunea coloanei nu este disponibilă pentru câmpul " + std::to_wstring(fieldNo) + L" din " + w_stm_name);
            return L"";
        }

        std::wstring result;
        std::vector<SQLWCHAR> buffer(512);

        do {
            ret = SQLGetData(hstmt, fieldNo, SQL_C_WCHAR, buffer.data(), buffer.size() * sizeof(SQLWCHAR), &indicator);
            if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO) {
                result += std::wstring(buffer.data());
            }
        } while (ret == SQL_SUCCESS_WITH_INFO && indicator != SQL_NULL_DATA);

        if (!result.empty()) return result;
    }*/
    else if (colType == SQL_VARCHAR || colType == SQL_WVARCHAR ||
        colType == SQL_CHAR || colType == SQL_WCHAR)
    {
        std::wstring result;
        SQLWCHAR chunk[256];   // citim în bucăți de 255 caractere
        SQLLEN indicator = 0;

        while (true)
        {
            SQLRETURN ret = SQLGetData(
                hstmt,
                fieldNo,
                SQL_C_WCHAR,
                chunk,
                sizeof(chunk),
                &indicator
            );

            if (indicator == SQL_NULL_DATA) return L""; 

            if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO)
            {
                // Asigurăm null-terminare manuală
                chunk[255] = 0;

                // Adăugăm bucata citită
                result.append(chunk);

                // Dacă am terminat, ieșim
                if (ret == SQL_SUCCESS)
                    break;
            }
            else if (ret == SQL_NO_DATA)
            {
                break;
            }
            else
            {
                LOG_ERROR(L"SQLGetData a eșuat pentru câmpul " +
                    std::to_wstring(fieldNo) + L" din " + w_stm_name);
                showError(SQL_HANDLE_STMT, hstmt);
                return L"";
            }
        }

        return result;
    }

    else if (colType == SQL_NUMERIC || colType == SQL_DECIMAL) {
        char buffer[64] = {}; // suficient pentru valori cu precizie mare
        ret = SQLGetData(hstmt, fieldNo, SQL_C_CHAR, buffer, sizeof(buffer), &indicator);

        if (indicator == SQL_NULL_DATA) return L"";

        if ((ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO) && indicator != SQL_NULL_DATA) {
            try {
                double val = std::stod(buffer); // conversie sigură
                std::wstringstream ss;
                ss << std::fixed << std::setprecision(4) << val; // ajustează precizia după caz
                //return ss.str();
                return trim_zeros(ss.str(),2);
            }
            catch (...) {
                return std::wstring(buffer, buffer + strlen(buffer)); // fallback ca text
            }
        }
    }
    // ❓ Tip necunoscut
    else {
        //LOG_INFO(L"odbcConnection::fetchFieldByNumber: Tipul de coloană " + std::to_wstring(colType) + L" nu este tratat explicit. Încercare de citire ca text (SQL_C_WCHAR) pentru " + w_stm_name);
        // Încercare de citire forțată ca Wide String (text)
        SQLULEN colSize = colSizes[stm_name][fieldNo - 1]; // Folosește dimensiunea salvată
        size_t bufferSize = (colSize + 1) * sizeof(SQLWCHAR);
        std::vector<SQLWCHAR> buffer(bufferSize / sizeof(SQLWCHAR));

        ret = SQLGetData(hstmt, fieldNo, SQL_C_WCHAR, buffer.data(), bufferSize, &indicator);

        if (indicator == SQL_NULL_DATA) return L"";

        if ((ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO) && indicator != SQL_NULL_DATA)
            return std::wstring(buffer.data());
    }

    // ⚠️ NULL sau eroare
    if (indicator == SQL_NULL_DATA) {
        //LOG_INFO(L"odbcConnection::fetchFieldByNumber: Câmpul " + std::to_wstring(fieldNo) + L" este NULL pentru " + w_stm_name);
    }
    else {
        LOG_ERROR(L"odbcConnection::fetchFieldByNumber: Eșec la SQLGetData pentru câmpul " + std::to_wstring(fieldNo) + L" din " + w_stm_name);
        showError(SQL_HANDLE_STMT, hstmt);
    }

    return L"";
}

std::vector<std::wstring> odbcConnection::fetchRow(std::string stm_name) {
    std::vector<std::wstring> rowValues;
    std::wstring w_stm_name(stm_name.begin(), stm_name.end());

    // Verifică dacă numele coloanelor sunt disponibile
    if (colNames.find(stm_name) == colNames.end() || colNames[stm_name].empty()) {
        //LOG_INFO(L"odbcConnection::fetchRow: Numele coloanelor nu sunt setate pentru " + w_stm_name + L". Se încearcă setarea...");
        if (!setColNames(stm_name)) {
            LOG_ERROR(L"odbcConnection::fetchRow: Eșec la setColNames pentru " + w_stm_name);
            showError(SQL_HANDLE_STMT, hstmts[stm_name]);
            return rowValues;
        }
    }

    // Obține numărul de coloane
    SQLSMALLINT columnCount = ColumnCountPtrs[stm_name];
    //LOG_INFO(L"odbcConnection::fetchRow: Preluare valori pentru " + std::to_wstring(columnCount) + L" coloane din statement-ul " + w_stm_name);

    // Iterează prin fiecare coloană și extrage valoarea
    for (SQLSMALLINT i = 1; i <= columnCount; ++i) {
        std::wstring value = fetchFieldByNumber(i, stm_name);
        std::wstring colName = colNames[stm_name][i - 1];

        //LOG_INFO(L"odbcConnection::fetchRow: Câmpul '" + colName + L"' = '" + value + L"'");
        rowValues.push_back(value);
    }

    return rowValues;
}

std::wstring odbcConnection::fetchFieldByName(const std::wstring& fieldName, std::string stm_name) {
    std::wstring result;
    std::wstring w_stm_name(stm_name.begin(), stm_name.end());

    // 1. Verifică dacă numele coloanelor sunt disponibile
    if (colNames.find(stm_name) == colNames.end() || colNames[stm_name].empty()) {
        //LOG_INFO(L"odbcConnection::fetchFieldByName: Numele coloanelor nu sunt setate pentru " + w_stm_name + L". Se încearcă setarea...");
        if (!setColNames(stm_name)) {
            LOG_ERROR(L"odbcConnection::fetchFieldByName: Eșec la setColNames pentru " + w_stm_name);
            showError(SQL_HANDLE_STMT, hstmts[stm_name]);
            return L"";
        }
    }

    // 2. Caută indexul coloanei direct în hartă (O(log N) sau O(1) in medie)
    auto it = colNameIndexes[stm_name].find(fieldName);

    if (it == colNameIndexes[stm_name].end()) {
        LOG_ERROR(L"odbcConnection::fetchFieldByName: Coloana '" + fieldName + L"' nu a fost găsită în statement-ul " + w_stm_name);
        return L"";
    }
    // Indexul este preluat direct din mapă (este deja 1-based)
    int fieldIndex = it->second;

    //LOG_INFO(L"odbcConnection::fetchFieldByName: Coloana '" + fieldName + L"' găsită la indexul " + std::to_wstring(fieldIndex));

    // 3. NU mai apelăm SQLFetch aici — presupunem că fetchNextRow() a fost apelat deja

    // 4. Preia valoarea câmpului folosind metoda existentă
    result = fetchFieldByNumber(fieldIndex, stm_name);

    // 5. Logare finală
    if (!result.empty()) {
        //LOG_INFO(L"odbcConnection::fetchFieldByName: Valoarea câmpului '" + fieldName + L"' este: " + result);
    }
    else {
        //LOG_INFO(L"odbcConnection::fetchFieldByName: Câmpul '" + fieldName + L"' este NULL sau gol.");
    }

    return result;
}

std::map<std::wstring, std::wstring> odbcConnection::fetchMap(std::string stm_name) {
    std::map<std::wstring, std::wstring> rowMap;
    std::wstring w_stm_name(stm_name.begin(), stm_name.end());

    // 1. Verifică dacă numele coloanelor sunt disponibile
    if (colNames.find(stm_name) == colNames.end() || colNames[stm_name].empty()) {
        //LOG_INFO(L"odbcConnection::fetchMap: Numele coloanelor nu sunt setate pentru " + w_stm_name + L". Se încearcă setarea...");
        if (!setColNames(stm_name)) {
            LOG_ERROR(L"odbcConnection::fetchMap: Eșec la setColNames pentru " + w_stm_name);
            showError(SQL_HANDLE_STMT, hstmts[stm_name]);
            return rowMap;
        }
    }

    // 2. Presupune că fetchNextRow() a fost apelat deja — nu mai apelăm SQLFetch aici

    // 3. Iterează prin toate coloanele și construiește map-ul
    SQLSMALLINT columnCount = ColumnCountPtrs[stm_name];
    //LOG_INFO(L"odbcConnection::fetchMap: Încep extragerea valorilor pentru " + std::to_wstring(columnCount) + L" coloane din " + w_stm_name);

    for (SQLSMALLINT i = 1; i <= columnCount; ++i) {
        std::wstring columnName = colNames[stm_name][i - 1]; // index 0-based
        std::wstring value = fetchFieldByNumber(i, stm_name);

        rowMap[columnName] = value;

        if (!value.empty()) {
            //LOG_INFO(L"odbcConnection::fetchMap: Câmpul '" + columnName + L"' = '" + value + L"'");
        }
        else {
            //LOG_INFO(L"odbcConnection::fetchMap: Câmpul '" + columnName + L"' este NULL sau gol.");
        }
    }

    //LOG_INFO(L"odbcConnection::fetchMap: Map-ul a fost construit cu succes pentru " + w_stm_name);
    return rowMap;
}

std::string odbcConnection::getConnectionType(){
    return type;
}

std::wstring odbcConnection::getConnectionDSN(){
    return dsn;
}

void odbcConnection::setConnectionDSN(const std::wstring& txt) {
    dsn = txt;
}


bool odbcConnection::testConnection(){
    if (!connected) return false;

    // 1. Verifică și alocă handle-ul 'test_alive' O SINGURĂ DATĂ
    if (hstmts.find("test_alive") == hstmts.end()) {
        if (!allocStatementHandle("test_alive")) {
            // Dacă alocarea a eșuat (ex: conexiunea e moartă), returnează false
            return false;
        }
    }

    SQLHSTMT hstmt_test = hstmts["test_alive"];

    // 2. Curăță statement-ul (necesar înainte de fiecare SQLExecDirect)
    SQLFreeStmt(hstmt_test, SQL_CLOSE);

    // 3. Execută interogarea de ping
    // Folosim SQLExecDirectW pentru consistență cu celelalte metode
    SQLRETURN ret = SQLExecDirectW(hstmt_test, (SQLWCHAR*)L"SELECT 1", SQL_NTS);

    if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO) {
        // Succes. Nu e nevoie de SQLFreeStmt(SQL_CLOSE) aici.
        return true;
    }
    else {
        // Eșec. Conexiunea este moartă sau are probleme.
        // NOTĂ: Poți afișa eroarea aici, dar nu închide conexiunea, doar o testezi.
        // showError(SQL_HANDLE_STMT, hstmt_test); 
        return false;
    }
}

// ** Adaugă această metodă în clasa ta odbcConnection **

bool odbcConnection::reconnect() {
    // 1. Verifică dacă ești deja conectat și conexiunea e vie
    if (isConnected() && testConnection()) {
        return true;
    }

    // 2. Închide orice conexiune moartă și eliberează resursele
    closeDatabase();

    // 3. Reîncearcă deschiderea bazei de date.
    // openDatabase folosește dsn-ul stocat.
    if (openDatabase()) {
        // Dacă reconectarea a reușit, realocă handle-urile statement
        for (const auto& pair : hstmts) {
            // Re-alocăm handle-urile statement pentru noua conexiune.
            // (Presupune că hstmts a fost golit doar de handle-uri în closeDatabase, 
            // dar map-ul a păstrat cheile (numele statement-urilor)).
            // Trebuie să fii atent la ce păstrează closeDatabase! 
            // Voi modifica closeDatabase pentru a păstra cheile.
            if (!allocStatementHandle(pair.first)) {
                // Dacă nu putem realoca un handle important, loghează și returnează false
                return false;
            }
        }
        return true;
    }

    return false; // Reconectare eșuată
}


bool odbcConnection::isConnectionError(SQLSMALLINT handleType, SQLHANDLE handle) {
    // Codurile de eroare ODBC pentru probleme de conexiune (SQLSTATE)
    // Acestea pot varia ușor, dar cele de mai jos sunt comune:
    // 08000: Conexiune nevalidă
    // 08001: Clientul nu poate stabili conexiunea
    // 08003: Nume de conexiune care nu există
    // 08007: Tranzacția eșuată
    // 08S01: Eroare de comunicare (cel mai comun 'conexiune pierdută')

    SQLWCHAR sqlState[6];
    SQLRETURN retcode = SQLGetDiagRecW(handleType, handle, 1, sqlState, NULL, NULL, 0, NULL);

    if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
        std::wstring state(sqlState);
        if (state == L"08001" || state == L"08003" || state == L"08006" || state == L"08007" || state == L"08S01") {
            return true;
        }
    }

    return false;
}

/*
// odbcConnection.cpp (Implementare)
long long odbcConnection::execCountQuery(const std::wstring& countQuery) {
    // Folosește un statement dedicat pentru COUNT, dacă nu ai deja unul
    std::string stm_name = "count_stmt";
    if (!allocStatementHandle(stm_name)) {
        LOG_ERROR(L"odbcConnection::execCountQuery: Eșec la alocarea handle-ului.");
        return -1;
    }

    SQLHSTMT hstmt = hstmts[stm_name];
    SQLRETURN retcode;

    SQLFreeStmt(hstmt, SQL_CLOSE);
    retcode = SQLExecDirectW(hstmt, (SQLWCHAR*)countQuery.c_str(), SQL_NTS);

    if (SQL_SUCCEEDED(retcode)) {
        // Dacă interogarea reușește, preia rezultatul (care este un singur rând/o singură coloană)
        if (SQLFetch(hstmt) == SQL_SUCCESS) {
            SQLLEN indicator;
            long long countResult = 0;
            // Preia rezultatul ca BIGINT (pentru a evita overflow pe INT)
            SQLGetData(hstmt, 1, SQL_C_SBIGINT, &countResult, sizeof(countResult), &indicator);

            if (indicator != SQL_NULL_DATA) {
                return countResult;
            }
        }
    }

    // În caz de eroare (inclusiv eroare de conexiune)
    LOG_ERROR(L"odbcConnection::execCountQuery: Eșec la execuția interogării COUNT.");
    showError(SQL_HANDLE_STMT, hstmt);
    return -1;
}
*/

long long odbcConnection::execCountQuery(const std::wstring& countQuery)
{
    if (!connected) {
        LOG_ERROR(L"execCountQuery: Conexiunea este închisă.");
        return -1;
    }

    std::string stm_name = "count_stmt";
    if (!allocStatementHandle(stm_name)) {
        LOG_ERROR(L"execCountQuery: Eșec la alocarea handle-ului.");
        return -1;
    }

    SQLHSTMT hstmt = hstmts[stm_name];

    SQLFreeStmt(hstmt, SQL_CLOSE);

    SQLRETURN retcode = SQLExecDirectW(hstmt, (SQLWCHAR*)countQuery.c_str(), SQL_NTS);
    if (!SQL_SUCCEEDED(retcode)) {
        showError(SQL_HANDLE_STMT, hstmt);
        return -1;
    }

    SQLRETURN fetchRet = SQLFetch(hstmt);
    if (fetchRet != SQL_SUCCESS && fetchRet != SQL_SUCCESS_WITH_INFO) {
        showError(SQL_HANDLE_STMT, hstmt);
        return -1;
    }

    SQLLEN indicator = 0;
    long long countResult = 0;

    SQLRETURN dataRet = SQLGetData(hstmt, 1, SQL_C_SBIGINT, &countResult, sizeof(countResult), &indicator);
    if (!SQL_SUCCEEDED(dataRet) || indicator == SQL_NULL_DATA) {
        showError(SQL_HANDLE_STMT, hstmt);
        return -1;
    }

    return countResult;
}


vNativeDataType odbcConnection::mapNativeOdbcToUniversal(SQLSMALLINT odbcType) {
    switch (odbcType) {
    case SQL_INTEGER:
    case SQL_SMALLINT:
    case SQL_TINYINT:  return vNativeDataType::V_INTEGER;

    case SQL_BIGINT:   return vNativeDataType::V_BIGINT;

    case SQL_DOUBLE:
    case SQL_FLOAT:
    case SQL_REAL:
    case SQL_NUMERIC:
    case SQL_DECIMAL:  return vNativeDataType::V_DOUBLE;

    case SQL_TYPE_DATE:
    case SQL_DATE:
    case SQL_TYPE_TIMESTAMP: return vNativeDataType::V_DATE;

    case SQL_BIT:      return vNativeDataType::V_BOOLEAN;

    default:           return vNativeDataType::V_TEXT;
    }
}

std::vector<vNativeDataType> odbcConnection::getColumnTypes(std::string stm_name) {
    std::vector<vNativeDataType> universalTypes;
    const auto& nativeOdbcTypes = colTypes[stm_name];

    for (SQLSMALLINT odbcType : nativeOdbcTypes) {
        universalTypes.push_back(mapNativeOdbcToUniversal(odbcType));
    }
    return universalTypes;
}

const std::vector<vExternalColumnInfo> odbcConnection::getColumnsInfo(std::string stm_name) {
    std::vector<vExternalColumnInfo> infoList;

    if (hstmts.find(stm_name) == hstmts.end()) return infoList;

    SQLHSTMT hstmt = hstmts[stm_name];
    SQLSMALLINT colCount;
    SQLNumResultCols(hstmt, &colCount);

    for (SQLSMALLINT i = 1; i <= colCount; ++i) {
        SQLWCHAR colName[256];
        SQLSMALLINT nameLen, dataType, decimalDigits, nullable;
        SQLULEN columnSize;

        // SQLDescribeColW scoate metadatele direct din driver
        SQLDescribeColW(hstmt, i, colName, 256, &nameLen, &dataType, &columnSize, &decimalDigits, &nullable);

        vExternalColumnInfo info;
        info.name = colName; // Conversie implicită de la SQLWCHAR* la std::wstring
        info.type = mapNativeOdbcToUniversal(dataType); // Folosim helper-ul aici
        info.length = static_cast<int>(columnSize);
        info.precision = static_cast<int>(decimalDigits);
        info.isNullable = (nullable != SQL_NO_NULLS);
        infoList.push_back(info);
    }
    return infoList;
}

std::vector<vExternalColumnInfo> odbcConnection::getTableSchema(const std::wstring& tableName) {
    std::vector<vExternalColumnInfo> columns;
    SQLHSTMT hstmtTemp = SQL_NULL_HSTMT;

    // 1. Despărțim Schema de Tabel (ex: "fsna.fsna_2025")
    std::wstring schemaPart = L"%"; // Default: orice schemă
    std::wstring tablePart = tableName;

    size_t dotPos = tableName.find(L'.');
    if (dotPos != std::wstring::npos) {
        schemaPart = tableName.substr(0, dotPos);
        tablePart = tableName.substr(dotPos + 1);
    }

    LOG_DEBUG(L"getTableSchema: Split detectat -> Schema: " + schemaPart + L", Table: " + tablePart);

    if (SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmtTemp) != SQL_SUCCESS) return columns;

    // 2. Prima încercare: Exact cum au fost scrise
    SQLRETURN ret = SQLColumnsW(hstmtTemp,
        NULL, 0,
        (SQLWCHAR*)schemaPart.c_str(), SQL_NTS,
        (SQLWCHAR*)tablePart.c_str(), SQL_NTS,
        NULL, 0);

    // 3. Fallback: Dacă nu găsește nimic, încercăm cu UPPERCASE (foarte comun în Oracle/Postgres/SQL Server)
    auto fetchAndProcess = [&]() -> bool {
        bool found = false;
        while (SQLFetch(hstmtTemp) == SQL_SUCCESS) {
            found = true;
            SQLWCHAR colName[256];
            SQLSMALLINT dataType, decimalDigits, nullable;
            SQLULEN columnSize;
            SQLLEN cb;

            SQLGetData(hstmtTemp, 4, SQL_C_WCHAR, colName, sizeof(colName), &cb);
            SQLGetData(hstmtTemp, 5, SQL_C_SSHORT, &dataType, 0, &cb);
            SQLGetData(hstmtTemp, 7, SQL_C_ULONG, &columnSize, 0, &cb);
            SQLGetData(hstmtTemp, 9, SQL_C_SSHORT, &decimalDigits, 0, &cb);
            SQLGetData(hstmtTemp, 11, SQL_C_SSHORT, &nullable, 0, &cb);

            vExternalColumnInfo info;
            info.name = colName;
            info.type = mapNativeOdbcToUniversal(dataType);
            info.length = static_cast<int>(columnSize);
            info.precision = static_cast<int>(decimalDigits);
            info.isNullable = (nullable != SQL_NO_NULLS);
            columns.push_back(info);
        }
        return found;
    };

    if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO) {
        if (!fetchAndProcess()) {
            // Nu am găsit nimic, încercăm UPPERCASE
            SQLFreeStmt(hstmtTemp, SQL_CLOSE);

            std::wstring uSchema = schemaPart;
            std::wstring uTable = tablePart;
            for (auto& c : uSchema) c = towupper(c);
            for (auto& c : uTable) c = towupper(c);

            LOG_DEBUG(L"Fallback: Încerc cu UPPERCASE -> " + uSchema + L"." + uTable);

            SQLColumnsW(hstmtTemp, NULL, 0,
                (SQLWCHAR*)uSchema.c_str(), SQL_NTS,
                (SQLWCHAR*)uTable.c_str(), SQL_NTS,
                NULL, 0);

            fetchAndProcess();
        }
    }

    SQLFreeHandle(SQL_HANDLE_STMT, hstmtTemp);
    return columns;
}

void odbcConnection::clearStatement(std::string stm_name = "default") {
    // 1. Verificăm dacă statement-ul există în mapă
    auto it = hstmts.find(stm_name);
    if (it != hstmts.end()) {
        // 2. Eliberăm handle-ul ODBC dacă este valid
        if (it->second != SQL_NULL_HSTMT) {
            SQLFreeHandle(SQL_HANDLE_STMT, it->second);
            it->second = SQL_NULL_HSTMT;
        }

        // 3. Ștergem intrarea din map-ul de handle-uri
        hstmts.erase(it);

        // 4. CURĂȚENIE GENERALĂ: Ștergem toate metadatele asociate acestui nume
        // Dacă nu facem asta, map-urile de colNames, colTypes etc. vor crește la infinit
        RowCountPtrs.erase(stm_name);
        ColumnCountPtrs.erase(stm_name);
        colNames.erase(stm_name);
        colTypes.erase(stm_name);
        colSizes.erase(stm_name);
        colNameIndexes.erase(stm_name);

        // LOG_DEBUG(L"odbcConnection::clearStatement: Resurse eliberate pentru " + str_to_wstr(stm_name));
    }
}