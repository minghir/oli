#ifndef DBFCONNECTION_HPP
#define DBFCONNECTION_HPP
#include <cstdint> // <--- Aceasta rezolvă eroarea C4430
#include <vector>
#include <string>
#include <fstream>

#include "dbConnection.hpp"

#include <memory>

class QueryTable; // Forward declaration

class dbfConnection : public dbConnection {
private:

    std::wstring m_dirPath; // Acum e calea către folder
    std::map<std::string, std::shared_ptr<TableContext>> m_statements;

    std::string	type;
    std::wstring m_filePath;
    std::ifstream m_file;
    DBF_Header m_header;
    //std::vector<DBF_FieldDescriptor> m_fields;
    //std::vector<std::wstring> m_colNames;

    int m_currentRowIndex = -1;
    std::vector<char> m_rowBuffer; // Buffer pentru rândul curent citit
    std::wstring m_error;

    vConResult m_lastResult;

    bool m_isConnected = false;

public:
    dbfConnection(const std::string& type, const std::wstring& path);
    
    ~dbfConnection() override;

    bool openDatabase() override;
    void closeDatabase() override;
    bool isConnected() const override;// { return m_isConnected; }
    bool reconnect() override { closeDatabase(); return openDatabase(); }
    bool testConnection() override { return isConnected(); }

    // DBF-ul nativ nu are SQL, deci execQuery va "selecta" tabelul
    bool execQuery(const std::wstring& query, std::string stm_name = "default") override;

    long long execCountQuery(const std::wstring& countQuery) override { return m_header.numRecords; }
    int getRowCount(std::string stm_name = "default") override { return m_header.numRecords; }

    const std::vector<std::wstring>& getColumnNames(std::string stm_name = "default") override;
    const std::vector<vNativeDataType> getColumnTypes(std::string stm_name = "default") override;
    const std::vector<vExternalColumnInfo> getColumnsInfo(std::string stm_name = "default") override;

    //bool setColNames(std::string stm_name = "default") override;

    bool fetchNextRow(std::string stm_name = "default") override;
    std::wstring fetchFieldByNumber(int fieldNo, std::string stm_name = "default") override;

    // Restul metodelor din interfață...
    std::vector<std::wstring> fetchRow(std::string stm_name = "default") override;
    std::wstring fetchFieldByName(const std::wstring& fieldName, std::string stm_name = "default") override;
    std::map<std::wstring, std::wstring> fetchMap(std::string stm_name = "default") override;

    std::wstring getError() override { return m_error; }
    void clearError() override { m_error = L""; }
    std::string getConnectionType() override { return "DBF_NATIVE"; }
    std::wstring getConnectionDSN() override { return m_filePath; }
    void setConnectionDSN(const std::wstring& txt) override { m_filePath = txt; }

    vConResult getLastQueryResult() override { return m_lastResult; }
    std::vector<vExternalColumnInfo> getTableSchema(const std::wstring& tableName);
private:
    std::vector<std::string> extractTableNames(const std::string& query);
    vConTable loadTable(const QueryTable& tableInfo);

    bool saveFile(const std::wstring& filename, const vConTable& table);

    // Funcție utilitară pentru formatarea binară a rândurilor
    std::string formatFieldForDbf(const std::wstring& val, int width, const std::wstring& type);
    bool appendRecords(const std::wstring& tableName, const vConTable& dataToInsert);
    bool deleteRecords(const std::wstring& tableName, const std::vector<int>& indices);
    bool updateRecords(const std::wstring& tableName, const std::map<int, std::vector<std::wstring>>& updates, const vConTable& tableMeta);
    void createBackup(const std::wstring& fullPath);
    bool packTable(const std::wstring& tableName);
    bool createTable(const std::wstring& query);
    bool dropTable(const std::wstring& query);
    bool showTables(std::string stm_name);
    bool describeTable(const std::wstring& query, std::string stm_name);
    bool alterTable(const std::wstring& query);
    bool alterDropColumn(const std::wstring& tableName, const std::wstring& colName);
    bool alterAddColumn(const std::wstring& tableName, const std::wstring& query);
    bool alterModifyColumn(const std::wstring& tableName, const std::wstring& query);

    void clearStatement(std::string stm_name);

};


#endif