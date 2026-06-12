#ifndef DBCONNECTION_HPP
#define DBCONNECTION_HPP


#include <cstdint> // <--- Aceasta rezolvă eroarea C4430
#include <vector>
#include <string>
#include <fstream>
#include <map>




enum class vTableTypes {
    Text,
    Integer,
    Double,
    Date,
    Boolean
};

enum class vNativeDataType {
    V_NULL,
    V_INTEGER,
    V_BIGINT,
    V_DOUBLE,
    V_TEXT,
    V_DATE,
    V_BOOLEAN,
    V_BLOB
};



struct vConTable {
    std::wstring tableName;
    std::wstring tableAlias;
    std::vector<std::wstring> columns;
    std::vector<vTableTypes> types;
    std::vector<std::vector<std::wstring>> records;
    std::vector<int> columnWidths;
    std::vector<int> columnDecimals;

    // Opțional: metadata pentru TYPE() - ex: "C", "N", "D"
    std::vector<std::wstring> columnTypes;
    std::vector<vNativeDataType> nativeTypes;
    int getColumnIndex(const std::wstring& name) const;
    
    
};


struct vConResult {
    vConTable table;               // Datele propriu-zise (dacă e un SELECT)
    long long executionTimeMs;  // Durata în milisecunde
    size_t rowsAffected;        // Nr. de rânduri (pt SELECT: câte au fost returnate, pt UPDATE: câte s-au schimbat)
    std::wstring message;       // Mesaj de succes/eroare (ex: "Table created successfully")
    bool success;               // Statusul operațiunii
    std::vector<int> affectedIndices;
    std::map<int, std::vector<std::wstring>> updatedRecordsMap;

    vConResult() : executionTimeMs(0), rowsAffected(0), success(true) {}
};


#pragma pack(push, 1)
struct DBF_Header {
    uint8_t version;         // Byte 0
    uint8_t lastUpdate[3];   // Bytes 1-3
    uint32_t numRecords;     // Bytes 4-7
    uint16_t headerLength;   // Bytes 8-9
    uint16_t recordLength;   // Bytes 10-11
    uint8_t reserved[17];    // Bytes 12-28
    uint8_t languageDriver;  // Byte 29 <--- ACESTA ESTE "VINOVATUL"
    uint16_t reserved2;      // Bytes 30-31
};
#pragma pack(pop)
#pragma pack(push, 1)
struct DBF_FieldDescriptor {
    char     fieldName[11];  // Nume coloană (terminat cu \0)
    char     fieldType;      // C (Character), N (Numeric), D (Date), L (Logical)
    uint32_t dataAddress;    // Rezervat
    uint8_t  fieldLength;    // Lungimea coloanei în bytes
    uint8_t  decimalCount;   // Precizia pentru N
    uint8_t  reserved[14];
};
#pragma pack(pop)

struct TableContext {
    std::ifstream file;
    DBF_Header header;
    std::vector<DBF_FieldDescriptor> fields;
    std::vector<std::wstring> colNames;
    std::vector<vNativeDataType> columnTypes;
    int currentRowIndex = -1;
    std::vector<char> rowBuffer;

    // Aici vom stoca rândurile care rămân după SELECT/WHERE/DISTINCT
    std::vector<std::map<std::wstring, std::wstring>> dbfResult;
};



struct vExternalColumnInfo {
    std::wstring name;
    vNativeDataType type;
    int length;
    int precision;
    bool isNullable;
};

class dbConnection {
private :
    
public:
    // Constructor
    dbConnection(const dbConnection&) = default;

    dbConnection() = default;

    dbConnection(const std::string& type, const std::wstring& dsn)      
    {
        (void)type;
        (void)dsn;
    }   

    // Destructor
    virtual ~dbConnection() {};

    // Metodă pentru deschiderea bazei de date
    virtual bool openDatabase() = 0;

    // Metodă pentru închiderea conexiunii
    virtual void closeDatabase() = 0;

    // Verifică dacă conexiunea este activă
    virtual bool isConnected() const = 0;

    virtual bool reconnect() = 0;
    virtual bool testConnection() = 0;
    //virtual const std::string& getType() const = 0;
    virtual bool execQuery(const std::wstring& query, std::string stm_name = "default" ) = 0;
    //virtual long long execCountQuery(const std::wstring& countQuery) = 0;

    virtual int getRowCount(std::string stm_name = "default") = 0;
    virtual const std::vector<std::wstring>& getColumnNames(std::string stm_name="default") = 0;
    virtual std::vector<vNativeDataType> getColumnTypes(std::string stm_name = "default") = 0;
    virtual const std::vector<vExternalColumnInfo> getColumnsInfo(std::string stm_name = "default") = 0;

    //virtual bool setColNames(std::string stm_name = "default") = 0;
    virtual std::wstring fetchFieldByNumber(int fieldNo, std::string stm_name = "default") = 0;
    virtual bool fetchNextRow(std::string stm_name = "default") = 0;
    virtual std::vector<std::wstring> fetchRow(std::string stm_name = "default") = 0; 
    virtual std::wstring fetchFieldByName(const std::wstring& fieldName,std::string stm_name = "default") = 0; 
    virtual std::map<std::wstring, std::wstring> fetchMap(std::string stm_name = "default") = 0;
    virtual std::wstring getError() = 0;
    virtual void clearError() =0;

    virtual std::string getConnectionType() = 0;
    virtual std::wstring getConnectionDSN() = 0;
    virtual void setConnectionDSN(const std::wstring& txt) = 0;

    virtual vConResult getLastQueryResult() = 0;
    virtual std::vector<vExternalColumnInfo> getTableSchema(const std::wstring& tableName) = 0;

    virtual void clearStatement(std::string stm_name = "default") = 0;
};

#endif // DBCONNECTION_HPP
