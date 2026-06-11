#ifndef SQLQUERYBUILDER_HPP
#define SQLQUERYBUILDER_HPP

#include <string>
#include <algorithm>
#include <vector>
#include <memory>
#include <variant>

// =====================================================================
// QUERY BUILDER (Structura de bază)
// =====================================================================

// Interfața de bază pentru toate componentele SQL
class ISqlComponent {
public:
    virtual ~ISqlComponent() = default;
    virtual std::wstring toSql() const = 0;
};

// Clasa de bază pentru condiții (unde și having)
class ICondition : public ISqlComponent {};

// Alias-uri pentru smart pointers
using ComponentPtr = std::shared_ptr<ISqlComponent>;
using ConditionPtr = std::shared_ptr<ICondition>;

// --- Componenta Col (Coloană/Expresie) ---
class Col : public ISqlComponent {
private:
    std::wstring m_expression;
    std::wstring m_alias;

public:
    Col(std::wstring expr, std::wstring alias = L"")
        : m_expression(std::move(expr)), m_alias(std::move(alias)) {}

    std::wstring toSql() const override {
        std::wstring sql = m_expression;
        if (!m_alias.empty()) {
            // Folosim ghilimele duble pentru alias, ca în codul dumneavoastră din PHP
            sql += L" AS \"" + m_alias + L"\"";
        }
        return sql;
    }
};


// --- Componenta TableRef (Referință la Tabel/Sub-Interogare) ---
class TableRef : public ISqlComponent {
private:
    ComponentPtr m_source; // Poate fi un nume de tabel (Col) sau o sub-interogare (Query)
    std::wstring m_alias;

public:
    // Constructor pentru nume de tabel (folosind un Col simplu pentru uniformitate)
    TableRef(std::wstring name, std::wstring alias = L"")
        : m_source(std::make_shared<Col>(name)), m_alias(std::move(alias)) {}

    // Constructor pentru Sub-Query
    TableRef(ComponentPtr subQuery, std::wstring alias)
        : m_source(std::move(subQuery)), m_alias(std::move(alias)) {}

    std::wstring toSql() const override {
        std::wstring sql = m_source->toSql();
        if (m_alias.empty()) return sql;

        // Dacă m_source este un Query, trebuie încapsulat în paranteze
        if (std::dynamic_pointer_cast<class Query>(m_source)) {
            sql = L"(" + sql + L")";
        }

        return sql + L" " + m_alias;
    }
};


// --- Clasa Query (Builder-ul Principal - Fluid Interface) ---
class Query : public ISqlComponent {
private:
    std::vector<ComponentPtr> m_selectCols;
    std::vector<ComponentPtr> m_fromTables;
    ConditionPtr m_whereClause;
    // ... Alte clauze: m_orderBy, m_limit ...

public:
    // Deoarece Query poate fi folosit în TableRef ca Sub-Query,
    // definim explicit un constructor gol.
    Query() = default;

    // === Fluid Interface Methods ===
    Query& select(std::vector<ComponentPtr> columns) {
        m_selectCols = std::move(columns);
        return *this;
    }

    Query& from(std::vector<ComponentPtr> tables) {
        m_fromTables = std::move(tables);
        return *this;
    }

    Query& where(ConditionPtr condition) {
        m_whereClause = condition;
        return *this;
    }

    // === Metoda de generare SQL ===
    std::wstring toSql() const override {
        std::wstring sql;

        // 1. SELECT
        if (!m_selectCols.empty()) {
            sql += L"SELECT ";
            for (size_t i = 0; i < m_selectCols.size(); ++i) {
                sql += m_selectCols[i]->toSql();
                if (i < m_selectCols.size() - 1) sql += L", ";
            }
        }

        // 2. FROM
        if (!m_fromTables.empty()) {
            sql += L" FROM ";
            for (size_t i = 0; i < m_fromTables.size(); ++i) {
                sql += m_fromTables[i]->toSql();
                if (i < m_fromTables.size() - 1) sql += L", ";
            }
        }

        // 3. WHERE
        if (m_whereClause) {
            sql += L" WHERE " + m_whereClause->toSql();
        }

        // ... (Alte clauze: GROUP BY, HAVING, ORDER BY, LIMIT)

        return sql;
    }
};


#endif // SQLQUERYBUILDER_HPP