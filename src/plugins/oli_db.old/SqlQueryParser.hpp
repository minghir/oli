#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>


enum class QueryType {
    SELECT,  // DQL
    INSERT,  // DML
    UPDATE,  // DML
    DELETE_ROWS,  // DML
    CREATE,  // DDL
    DROP,    // DDL
    ALTER    // DDL
};

// Putem grupa și pe categorii dacă vrei să fii foarte organizat
enum class SqlStatementCategory {
    DQL, // Data Query Language
    DML, // Data Manipulation Language
    DDL  // Data Definition Language
};


enum class OpType {
    EQUALS, NOT_EQUALS, GREATER, LESS,
    GREATER_EQUAL, LESS_EQUAL, LIKE, IN
};


struct ParseError {
    bool hasError = false;
    std::wstring message;
    std::wstring fragment; // Partea de SQL care a cauzat problema
    int position = -1;     // Indexul caracterului în string-ul original
};

class Query;

enum class ColumnType {
    UNKNOWN,
    RAW_FIELD,   // coloană (ex: AIRPORT)
    LITERAL,     // constante (ex: 100, 'OTP')
    EXPRESSION,  // funcții/calcule (ex: TYPE(A) + 1)
    AGGREGATE,   // pentru COUNT, SUM, etc.
    SCALAR_FUNCTION, //functii scalare
    SUBQUERY,     // (SELECT ...)
    WILDCARD
};

enum class JoinType {
    INNER,
    LEFT,
    RIGHT,
    FULL
};

class QueryColumn {
public:
    ColumnType type = ColumnType::RAW_FIELD;
    std::wstring rawExpression; // Expresia brută
    std::wstring alias;         // AS "alias"


    // Numele funcției de agregare (ex: L"COUNT", L"SUM")
    std::wstring aggregateFunc;

    // Argumentul funcției (ex: L"*" sau L"SALARY")
    std::wstring aggregateArg;


    // Dacă coloana este un subquery, pointer către obiectul Query recursiv
    // Folosim un pointer pentru că clasa Query este deja definită
    std::shared_ptr<class Query> subSelect = nullptr;

    // Constructor/Destructor pentru managementul memoriei
    QueryColumn() {};//  : type(ColumnType::RAW_FIELD), subSelect(nullptr) {}
    ~QueryColumn() = default;

    void detectType();
};


class WhereClause {
public:

    std::vector<std::shared_ptr<WhereClause>> subClauses;
    std::wstring groupConnector; // AND / OR care leagă subClauses între ele

    // Partea stângă: poate fi coloană, funcție (UPPER) sau subquery
    QueryColumn leftOperand;

    // Operatorul: =, !=, IN, NOT IN, LIKE, IS, BETWEEN
    std::wstring oper;

    // Partea dreaptă: poate fi literal, altă coloană sau SUBQUERY
    QueryColumn rightOperand;

    // Pentru condiții complexe: AND / OR / NONE (dacă e singura clauză)
    std::wstring logicConnector;

    // Flag pentru clauze de tip "IS NULL" sau "IS NOT NULL"
    bool isUnary = false;
    bool isGroup = false; // Flag pentru a ști dacă citim subClauses sau operanzii
    bool isNegated = false; // Flag crucial pentru NOT
};


class QueryTable {
public:
    bool isSubquery = false;

    // Numele sursei: 'persoane.dbf' sau query-ul brut
    std::wstring name;

    // Alias-ul: 'a' sau 'p' (cum îl vei referi în restul SQL-ului)
    std::wstring alias;

    // Pointer recursiv către obiectul Query (dacă isSubquery == true)
    std::shared_ptr<class Query> subSelect = nullptr;

    QueryTable() {};// : subSelect(nullptr), isSubquery(false) {}

    // Metodă utilă pentru a obține identificatorul tabelului
    // Dacă are alias, returnăm alias-ul, altfel numele de bază
    std::wstring getEffectiveName() const {
        return alias.empty() ? name : alias;
    }
};



class JoinClause {
public:
    JoinType type = JoinType::INNER;

    // Tabela cu care ne unim (reutilizăm QueryTable pentru a suporta și subqueries în JOIN!)
    QueryTable table;

    // Condiția de unire (ex: a.id = b.id_persoana)
    // Folosim o listă de WhereClause pentru a suporta ON a.id = b.id AND a.activ = 1
    std::vector<WhereClause> on_conditions;
};

enum class OrderDirection { ASC, DESC };

class OrderClause {
public:
    // Folosim QueryColumn pentru a suporta: nume coloană, index (1, 2..) sau funcții
    QueryColumn column;

    // Direcția: true pentru ASC (default), false pentru DESC
    OrderDirection direction = OrderDirection::ASC;

    // Opțional: Gestionarea valorilor NULL (dacă vrei să fii pro)
    // NULLS FIRST sau NULLS LAST
    bool nullsFirst = false;
};

class GroupClause {
public:
    // Permite gruparea după coloană, index sau expresie
    QueryColumn column;

    // În SQL modern nu avem ASC/DESC aici, dar putem păstra oper 
    // pentru compatibilitate sau pentru indicii de sortare internă
    std::wstring oper;
};


class Query {
public:

    QueryType type = QueryType::SELECT;

    // Pentru SELECT / INSERT (coloanele în care inserăm)
    std::vector<QueryColumn> columns;

    // Pentru INSERT INTO ... VALUES (...)
     // Un vector de rânduri, unde fiecare rând e un vector de expresii/valori
    std::vector<std::vector<std::wstring>> insertValues;

    // Pentru UPDATE: mapăm numele coloanei la noua expresie/valoare
    // ex: SET nume = 'Ion', varsta = varsta + 1
    std::map<std::wstring, std::wstring> updateSets;

    QueryTable fromTable;          // Tabela principală (cea din FROM)
    std::vector<JoinClause> joins; // Restul tabelelor unite

    std::shared_ptr<WhereClause> whereRoot;
    std::vector<WhereClause> where_clauses;

    std::vector<GroupClause> group_clauses;
    std::vector<OrderClause> order_clauses;

    std::shared_ptr<WhereClause> havingRoot;
    std::vector<WhereClause> having_clauses;
    
    bool isDistinct = false;
    std::map<std::wstring, size_t> columnsIdx;

    // LIMIT / OFFSET
    int limit = -1;
    int offset = 0;

    void printColumns();
    void printJoins();
    void printWhere(std::shared_ptr<WhereClause> node, int level);
    void printWhere() { printWhere(whereRoot, 0); };
    void printOrder();
    void printGroup();
    void printHaving() { printWhere(havingRoot, 0); };

    void collectTablesRecursive(std::vector<QueryTable>& allTables);
    void collectTablesFromWhere(std::shared_ptr<WhereClause> node, std::vector<QueryTable>& allTables);

    bool isAggregate() const {
        for (const auto& col : columns) {
            if (col.type == ColumnType::AGGREGATE) return true;
        }
        return false;
    }
};

class SqlQueryParser {
private:
    
    std::wstring query_str;
    Query m_internalQuery; // 1. Obiectul real (trebuie declarat primul)
    Query& query;

    ParseError lastError;

    // Metode de ajutor pentru raportare
    bool setError(const std::wstring& msg, const std::wstring& frag = L"") {
        lastError.hasError = true;
        lastError.message = msg;
        lastError.fragment = frag;
        return false; // Returnăm false pentru a opri execuția ușor
    }

public:
    //SqlQueryParser() {}
    SqlQueryParser(std::wstring qry);

    SqlQueryParser(std::wstring qry, Query& targetQuery);

    bool parse();

    bool parse(const std::wstring qry) {

        query_str = qry;
        return parse();
    }

    bool parseSelect();
    bool parseInsert();
    bool parseDelete();
    bool parseUpdate();


    std::wstring getFirstToken(const std::wstring& s);


    ParseError getLastError() const { return lastError; }
    Query getQuery() const { return query; }

    bool parseSelect(std::wstring section);
    bool parseFrom(std::wstring section);

    size_t findFirstJoinKeyword(const std::wstring& upperStr);
    bool parseJoinsRecursive(std::wstring joinStr);
    std::vector<WhereClause> parseOnConditions(const std::wstring& conditionStr);
    bool parseSingleTableSource(const std::wstring& token, QueryTable& qt);

    bool parseWhere(std::wstring section);
    std::shared_ptr<WhereClause> parseRecursiveWhere(std::wstring section);
    void parseLeafCondition(std::shared_ptr<WhereClause> node, std::wstring condition);

    bool parseOrderBy(std::wstring section);
    bool parseGroupBy(std::wstring section);
    bool parseHaving(std::wstring section);
    void printStructure();

    std::vector<QueryColumn> getColumns() { return query.columns; }
    std::wstring getTableName() const { return query.fromTable.name; }

    std::vector<QueryTable> getAllParticipatingTables() {
        std::vector<QueryTable> result;
        query.collectTablesRecursive(result);
        return result;
    }

    size_t findOutsideParens(const std::wstring& haystack, const std::wstring& needle);
};