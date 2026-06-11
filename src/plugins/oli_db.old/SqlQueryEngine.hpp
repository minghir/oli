#ifndef SQLQUERYENGINE_HPP
#define SQLQUERYENGINE_HPP

#include "SqlQueryParser.hpp"
#include "dbConnection.hpp"

#include <functional>
#include <vector>
#include <string>
#include <map>

// Tipul de funcție: primeste (valoare_stanga, valoare_dreapta)
using OperatorHandler = std::function<bool(const std::wstring&, const std::wstring&)>;

// Primesc o listă de argumente deja rezolvate și returnează rezultatul final
using FunctionHandler = std::function<std::wstring(const std::vector<std::wstring>&)>;

// (Valoare_Curentă, Valoare_Nouă) -> Rezultat_Nou
using AggregateHandler = std::function<std::wstring(const std::wstring&, const std::wstring&)>;

class vSqlEngine {
private:
    SqlQueryParser m_queryParser;
    std::vector<vConTable> m_sourceTables; // Tabelele de intrare
    vConTable resultTable;
    std::map<std::wstring, OperatorHandler> m_opHandlers;
    std::map<std::wstring, FunctionHandler> m_funcHandlers;
    std::map<std::wstring, AggregateHandler> m_aggHandlers;
    void registerHandlers();
    
 

public:
    // Constructorul primește "lumea" ta de date
    vSqlEngine(const std::vector<vConTable>& sources, SqlQueryParser& parser) : m_queryParser(parser), m_sourceTables(sources) {
        registerHandlers();
    }

    // Execută query-ul și returnează un tabel nou (rezultatul)
    //vTable execute(const std::wstring& query);
    vConResult execute(const SqlQueryParser& query);

private:
    // Metode interne pentru procesare
    //void parseQuery();

    vConResult executeSelect(const SqlQueryParser& query);
    vConResult executeInsert(const SqlQueryParser& query);
    vConResult executeUpdate(const SqlQueryParser& query);
    vConResult executeDelete(const SqlQueryParser& query);
    

    std::vector<QueryColumn> expandWildcards(const std::vector<QueryColumn>& columns, const vConTable& source);
    void applySorting(std::vector<const std::vector<std::wstring>*>& rows, const Query& q, const vConTable& source);
    void applyLimitOffset(std::vector<std::vector<std::wstring>>& records, int limit, int offset);

    vConTable* findTableInUniverse(const std::wstring& nameOrAlias);
    bool evaluateJoinCondition(const JoinClause& join, const std::vector<std::wstring>& combinedRow, const vConTable& joinedSchema);
    vConTable performJoin(const vConTable& left, const vConTable& right, const JoinClause& join);
    void applyFilters(vConTable& workTable);
    //void evaluateExpressions(vConTable& workTable, const vConTable& sourceRef);
    void evaluateExpressions(vConTable& workTable, const vConTable& sourceRef, const std::vector<QueryColumn>& projectedColumns);

    std::vector<std::wstring> splitSqlArguments(const std::wstring& s);
    std::wstring resolveExpression(std::wstring expr, const std::vector<std::wstring>& row, const vConTable& table);
    //bool evaluateCondition(std::shared_ptr<WhereClause> node, const std::vector<std::wstring>& row, const vConTable& table);
    bool evaluateCondition(
        std::shared_ptr<WhereClause> node,
        const std::vector<std::wstring>& row,
        const vConTable& table,
        const std::vector<std::wstring>& outerRow = {}, // <-- Adăugat pentru corelare
        const vConTable& outerTable = vConTable()              // <-- Adăugat pentru corelare
    );
    void projectColumns(vConTable& table);
    void printResult();

    double asNumber(const std::wstring& s);

    bool handleLikeOp(const std::wstring& left, const std::wstring& right);


    std::wstring resolveExpressionWithContext(
        std::wstring expr,
        const std::vector<std::wstring>& row, const vConTable& table,
        const std::vector<std::wstring>& outerRow, const vConTable& outerTable);

    std::wstring getValWithContext(std::wstring identifier,
        const std::vector<std::wstring>& row, const vConTable& table,
        const std::vector<std::wstring>& outerRow, const vConTable& outerTable);
};

#endif