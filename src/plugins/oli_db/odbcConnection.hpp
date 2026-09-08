#ifndef ODBCCONNECTION_HPP
#define ODBCCONNECTION_HPP

#include "dbConnection.hpp"

#include <string>
#include <vector>
#include <map>
#include <algorithm> // pentru std::find

#include <windows.h> 
#include <sql.h>
#include <sqlext.h>


class odbcConnection : public dbConnection{
private:
    std::string	type; // poate fi odbc pgsql mysql oracle csv
    std::wstring dsn;        // Numele DSN
    std::wstring error;
    SQLHENV henv;            // Handler pentru mediu
    SQLHDBC hdbc;            // Handler pentru conexiune
    bool connected;          // Starea conexiunii
                             //

    std::map<std::string, SQLLEN> RowCountPtrs;
    std::map<std::string, SQLSMALLINT> ColumnCountPtrs;
    std::map<std::string,std::vector<std::wstring>> colNames;
    std::map<std::string, std::vector<SQLSMALLINT>> colTypes;
    std::map<std::string, std::vector<SQLULEN>> colSizes;
    std::map<std::string, std::map<std::wstring, int>> colNameIndexes;
    std::map<std::string,SQLHSTMT> hstmts;

    // Metodă auxiliară pentru afișarea erorilor
    void showError(SQLSMALLINT handleType, SQLHANDLE handle);
    bool allocStatementHandle(std::string stm_name = "default");
    bool isConnectionError(SQLSMALLINT handleType, SQLHANDLE handle);
    vNativeDataType mapNativeOdbcToUniversal(SQLSMALLINT odbcType);
    bool setColNames(std::string stm_name = "default");

    vConResult m_lastResult;

public:
    // Constructor
    odbcConnection(const std::string& type, const std::wstring& dsn);

    odbcConnection() = default;
    // Destructor
    ~odbcConnection() override;

    // Metodă pentru deschiderea bazei de date
    bool openDatabase() override;

    // Metodă pentru închiderea conexiunii
    void closeDatabase() override;

    // Verifică dacă conexiunea este activă
    bool isConnected() const override;

    bool reconnect() override;
    bool testConnection() override;

    // execut query
    bool execQuery(const std::wstring& query, std::string stm_name = "default" ) override;
    
    

    int getRowCount(std::string stm_name = "default") override;

    const std::vector<std::wstring>& getColumnNames(std::string stm_name="default") override;
    std::vector<vNativeDataType> getColumnTypes(std::string stm_name) override;
    const std::vector<vExternalColumnInfo> getColumnsInfo(std::string stm_name = "default") override;

    

    std::wstring fetchFieldByNumber(int fieldNo, std::string stm_name = "default") override;
    bool fetchNextRow(std::string stm_name = "default") override;
    std::vector<std::wstring> fetchRow(std::string stm_name = "default") override;
    std::wstring fetchFieldByName(const std::wstring& fieldName,std::string stm_name = "default") override;
    std::map<std::wstring, std::wstring> fetchMap(std::string stm_name = "default");
    std::wstring getError() override;
    void clearError() override;

    std::string getConnectionType() override;
    std::wstring getConnectionDSN() override;
    void setConnectionDSN(const std::wstring& txt) override;


    //ATENTIE NU E POPULAT...doar pt compatibilitate cu dbf si csv
    vConResult getLastQueryResult() override { 
            return m_lastResult; 
    }


    std::vector<vExternalColumnInfo> getTableSchema(const std::wstring& tableName);
    void clearStatement(std::string stm_name);
	
	long long execCountQuery(const std::wstring& countQuery);
};

#endif // DBCONNECTION_HPP
