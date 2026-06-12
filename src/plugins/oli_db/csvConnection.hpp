#ifndef CSVCONNECTION_HPP
#define CSVCONNECTION_HPP
//#include <windows.h> 
#include <string>
#include <vector>
//#include <sql.h>
//#include <sqlext.h>
#include <map>
#include <algorithm> // pentru std::find
#include <cstdint> // <--- Aceasta rezolvă eroarea C4430
#include <fstream>
#include <memory>


#include "dbConnection.hpp"

class csvConnection : public dbConnection {
    
private:
    std::string	type; // poate fi odbc pgsql mysql oracle csv
    std::wstring dsn;        // Numele DSN
    std::wstring error;
    bool connected;          // Starea conexiunii


    //std::map<std::string, int> RowCountPtrs;
    //std::map<std::string, int> ColumnCountPtrs;
    //std::map<std::string, int> rowCounters;
    ///std::map<std::string,std::vector<std::wstring>> colNames;
    //std::map<std::string, std::vector<std::wstring>> csvLines;

    
    std::string csvPath;
    std::vector<std::string> csvFiles;
    std::map<std::string, std::shared_ptr<TableContext>> m_statements;

    vConResult m_lastResult;

public:
    // Constructor
    csvConnection(const std::string& type, const std::wstring& dsn);

    // Destructor
    ~csvConnection();


    bool readCSVFiles(const std::string& directoryPath);

    // Metodă pentru deschiderea bazei de date
    bool openDatabase() override;

    // Metodă pentru închiderea conexiunii
    void closeDatabase();

    // Verifică dacă conexiunea este activă
    bool isConnected() const override;

    bool reconnect() override;

    bool testConnection() override;

    // execut query
    bool execQuery(const std::wstring& query, std::string stm_name = "default" ) override;
    //long long execCountQuery(const std::wstring& countQuery) override;

    int getRowCount(std::string stm_name = "default") override;

    const std::vector<std::wstring>& getColumnNames(std::string stm_name="default") override;
    std::vector<vNativeDataType> getColumnTypes(std::string stm_name = "default") override;
    const std::vector<vExternalColumnInfo> getColumnsInfo(std::string stm_name = "default") override;

    //bool setColNames(std::string stm_name = "default") override;


std::wstring fetchFieldByNumber(int fieldNo, std::string stm_name = "default") override;
bool fetchNextRow(std::string stm_name = "default") override;
std::vector<std::wstring> fetchRow(std::string stm_name = "default") override;
std::wstring fetchFieldByName(const std::wstring& fieldName,std::string stm_name = "default") override;
std::map<std::wstring, std::wstring> fetchMap(std::string stm_name = "default") override;
std::wstring getError() override;
void clearError() override;

std::string getConnectionType() override;
std::wstring getConnectionDSN() override;
void setConnectionDSN(const std::wstring& txt) override;


void printFiles();
std::vector<std::string> extractTableNames(const std::string& query);
std::wstring fetchRawRow(std::string stm_name = "default");

const std::vector<std::string> getCsvFiles() {
    return csvFiles
        ;
}

vConResult getLastQueryResult() override { return m_lastResult; }
std::vector<vExternalColumnInfo> getTableSchema(const std::wstring& tableName);
//bool setCSVColNames(std::string stm_name = "default");

void clearStatement(std::string stm_name);

};

#endif // DBCONNECTION_HPP
