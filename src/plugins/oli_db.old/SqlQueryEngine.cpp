#include "SqlQueryEngine.hpp"
#include "../../StringUtils.hpp"
#include "../../ConsoleManager.hpp"
#include "vmath.hpp"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cwctype>


void vSqlEngine::registerHandlers() {
    // Egalitate
    m_opHandlers[L"="] = [](const std::wstring& l, const std::wstring& r) {
        return l == r;
    };

    // Diferit
    m_opHandlers[L"!="] = [](const std::wstring& l, const std::wstring& r) {
        return l != r;
    };
    m_opHandlers[L"<>"] = m_opHandlers[L"!="];


    // Operatori numerici (folosind asNumber definit anterior)
    m_opHandlers[L">"] = [this](const std::wstring& l, const std::wstring& r) {
        return asNumber(l) > asNumber(r);
    };


    m_opHandlers[L"<"] = [this](const std::wstring& l, const std::wstring& r) {
        return asNumber(l) < asNumber(r);
    };

    m_opHandlers[L"<="] = [this](const std::wstring& l, const std::wstring& r) {
        return asNumber(l) <= asNumber(r);
    };

    m_opHandlers[L">="] = [this](const std::wstring& l, const std::wstring& r) {
        return asNumber(l) >= asNumber(r);
    };

    // Operatorul LIKE
    m_opHandlers[L"LIKE"] = [this](const std::wstring& l, const std::wstring& r) {
        return this->handleLikeOp(l, r);
    };

    // Înregistrăm NOT LIKE (pur și simplu negăm rezultatul aceleiași metode)
    m_opHandlers[L"NOT LIKE"] = [this](const std::wstring& l, const std::wstring& r) {
        return !this->handleLikeOp(l, r);
    };


    // FUNCTII AGREGARE
    // COUNT: incrementăm valoarea numerică existentă
    m_aggHandlers[L"COUNT"] = [](const std::wstring& current, const std::wstring& next) -> std::wstring {
        // Dacă e prima dată (current e gol), pornim de la 0, altfel incrementăm
        long long count = current.empty() ? 0 : std::stoll(current);
        return std::to_wstring(count + 1);
    };

    m_aggHandlers[L"SUM"] = [this](const std::wstring& current, const std::wstring& next) {
        double sum = current.empty() ? 0.0 : asNumber(current);
        double val = next.empty() ? 0.0 : asNumber(next);

        // Formatare fără zerouri inutile
        std::wstringstream ss;
        ss << (sum + val);
        return ss.str();
    };

    m_aggHandlers[L"MIN"] = [this](const std::wstring& current, const std::wstring& next) -> std::wstring {
        if (next.empty()) return current;
        if (current.empty()) return next; // Prima valoare devine minimul curent

        double curMin = asNumber(current);
        double val = asNumber(next);

        std::wstringstream ss;
        ss << (val < curMin ? val : curMin);
        return ss.str();
    };

    m_aggHandlers[L"MAX"] = [this](const std::wstring& current, const std::wstring& next) -> std::wstring {
        if (next.empty()) return current;
        if (current.empty()) return next; // Prima valoare devine maximul curent

        double curMax = asNumber(current);
        double val = asNumber(next);

        std::wstringstream ss;
        ss << (val > curMax ? val : curMax);
        return ss.str();
    };

    m_aggHandlers[L"AVG"] = [this](const std::wstring& current, const std::wstring& next) -> std::wstring {
        double sum = 0.0;
        long long count = 0;

        if (!current.empty()) {
            size_t sep = current.find(L':');
            if (sep != std::wstring::npos) {
                sum = std::stod(current.substr(0, sep));
                count = std::stoll(current.substr(sep + 1));
            }
        }

        if (!next.empty()) {
            sum += asNumber(next);
            count++;
        }

        std::wstringstream ss;
        ss << sum << L":" << count;
        return ss.str();
    };

    m_aggHandlers[L"STRING_AGG"] = [](const std::wstring& current, const std::wstring& nextRaw) -> std::wstring {
        if (nextRaw.empty()) return current;

        std::wstring val = nextRaw;
        std::wstring sep = L", "; // Default

        // Căutăm delimitatorul nostru special
        size_t sepPos = nextRaw.find(L"|SEP|");
        if (sepPos != std::wstring::npos) {
            val = nextRaw.substr(0, sepPos);
            std::wstring rawSep = nextRaw.substr(sepPos + 5); // +5 pentru lungimea "|SEP|"

            // Curățăm separatorul de ghilimele (ex: ', ' devine , )
            if (rawSep.size() >= 2 && (rawSep.front() == L'\'' || rawSep.front() == L'\"')) {
                rawSep = rawSep.substr(1, rawSep.size() - 2);
            }
            sep = rawSep;
        }

        if (current.empty()) return val;
        return current + sep + val;
    };


    // --- Funcții scalare ---

    // UPPER(str)
    m_funcHandlers[L"UPPER"] = [](const std::vector<std::wstring>& args) -> std::wstring {
        if (args.empty()) return L"";
        std::wstring res = args[0];
        std::transform(res.begin(), res.end(), res.begin(), ::towupper);
        return res;
    };

    // UPPER(str)
    m_funcHandlers[L"LOWER"] = [](const std::vector<std::wstring>& args) -> std::wstring {
        if (args.empty()) return L"";
        std::wstring res = args[0];
        std::transform(res.begin(), res.end(), res.begin(), ::tolower);
        return res;
    };

    // SUBSTR(str, start, [len])
    m_funcHandlers[L"SUBSTR"] = [](const std::vector<std::wstring>& args) -> std::wstring {
        if (args.size() < 2) return L"";
        std::wstring str = args[0];
        try {
            int start = std::stoi(args[1]) - 1;
            int len = (args.size() > 2) ? std::stoi(args[2]) : (int)str.size();

            if (start < 0) start = 0;
            if (start >= (int)str.size()) return L"";

            return str.substr(start, len); // Acum compilatorul știe că vrei wstring
        }
        catch (...) {
            return L"";
        }
    };

    // TRIM(str)
    m_funcHandlers[L"TRIM"] = [](const std::vector<std::wstring>& args) -> std::wstring {
        if (args.empty()) return L"";
        return wstr_trim(args[0]);
    };

    // TYPE(val) - Returnează tipul datei
    m_funcHandlers[L"TYPE"] = [](const std::vector<std::wstring>& args) -> std::wstring {
        if (args.empty()) return L"U"; // Unknown
        return args[0]; // Returnăm tipul pe care resolveExpression l-a identificat deja
    };

    m_funcHandlers[L"CONCAT"] = [](const std::vector<std::wstring>& args) -> std::wstring {
        std::wstring result = L"";
        for (const auto& arg : args) {
            result += arg;
        }
        return result;
    };
}

vConTable* vSqlEngine::findTableInUniverse(const std::wstring& nameOrAlias) {
    std::wstring search = to_upper(nameOrAlias);
    for (auto& table : m_sourceTables) {
        if (to_upper(table.tableAlias) == search || to_upper(table.tableName) == search) {
            return &table;
        }
    }
    return nullptr;
}

vConResult vSqlEngine::execute(const SqlQueryParser& parser) {

    const Query& q = parser.getQuery();

    switch (q.type) {
    case QueryType::SELECT:
        return executeSelect(parser);
    case QueryType::INSERT:
        return executeInsert(parser);
    case QueryType::UPDATE:
        return executeUpdate(parser);
    case QueryType::DELETE_ROWS:
        return executeDelete(parser);
        // ...
    }
}

vConResult vSqlEngine::executeInsert(const SqlQueryParser& parser) {
    vConResult result;
    auto start = std::chrono::high_resolution_clock::now();

    try {
        const Query& q = parser.getQuery();
        if (m_sourceTables.empty())
            throw std::runtime_error("Tabelul tinta nu a putut fi incarcat.");

        // Tabelul în care inserăm este întotdeauna primul (fromTable)
        const vConTable& target = m_sourceTables[0];

        // Pregătim un vConTable care va conține DOAR rândurile noi
        // Acesta va fi "pachetul" pe care dbfConnection îl va scrie pe disc
        vConTable rowsToAdd;
        rowsToAdd.tableName = target.tableName;
        rowsToAdd.columns = target.columns;
        rowsToAdd.columnTypes = target.columnTypes;
        rowsToAdd.columnWidths = target.columnWidths;

        // Procesăm fiecare set de valori (INSERT INTO ... VALUES (...), (...))
        // q.insertValues este std::vector<std::vector<std::wstring>>
        for (const auto& valueRow : q.insertValues) {

            // Cream un rând gol cu dimensiunea corectă a tabelului țintă
            std::vector<std::wstring> newRecord(target.columns.size(), L"");

            // Dacă avem coloane specificate: INSERT INTO tab (col1, col3) VALUES (val1, val3)
            if (!q.columns.empty()) {
                for (size_t i = 0; i < q.columns.size(); ++i) {
                    // Găsim unde trebuie să ajungă valoarea în structura DBF
                    int targetIdx = target.getColumnIndex(to_upper(q.columns[i].rawExpression));
                    if (targetIdx != -1 && i < valueRow.size()) {
                        newRecord[targetIdx] = valueRow[i];
                    }
                }
            }
            else {
                // Dacă nu avem coloane: INSERT INTO tab VALUES (val1, val2, ...)
                // Valorile se mapează 1 la 1 în ordinea coloanelor din fișier
                for (size_t i = 0; i < valueRow.size() && i < newRecord.size(); ++i) {
                    newRecord[i] = valueRow[i];
                }
            }

            rowsToAdd.records.push_back(std::move(newRecord));
        }

        result.table = std::move(rowsToAdd);
        result.rowsAffected = result.table.records.size();
        result.success = true;
        result.message = L"Insert rows prepared successfully.";

    }
    catch (const std::exception& e) {
        result.success = false;
        result.message = L"Insert Error: " + str_to_wstr(e.what());
    }

    auto end = std::chrono::high_resolution_clock::now();
    result.executionTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    return result;
}


vConResult vSqlEngine::executeUpdate(const SqlQueryParser& parser) {
    vConResult result;
    const Query& q = parser.getQuery();
    const vConTable& target = m_sourceTables[0]; // Tabela deja încărcată

    // Colectăm rândurile care trebuie modificate și valorile lor noi
    // Map: Index_Rând -> Vector_Valori_Noi
    std::map<int, std::vector<std::wstring>> updates;

    for (int i = 0; i < (int)target.records.size(); ++i) {
        if (!q.whereRoot || evaluateCondition(q.whereRoot, target.records[i], target)) {
            std::vector<std::wstring> updatedRow = target.records[i];

            for (auto const& [colName, newValue] : q.updateSets) {
                int colIdx = target.getColumnIndex(colName);
                if (colIdx != -1) {
                    // Aici putem folosi loop-ul de re-evaluare pentru newValue!
                    updatedRow[colIdx] = resolveExpression(newValue, target.records[i], target);
                }
            }
            updates[i] = updatedRow;
        }
    }

    result.updatedRecordsMap = updates; // Ai nevoie de acest membru nou în vConResult
    result.rowsAffected = updates.size();
    result.success = true;
    return result;
}

vConResult vSqlEngine::executeDelete(const SqlQueryParser& parser) {
    vConResult result;
    auto start = std::chrono::high_resolution_clock::now();

    try {
        const Query& q = parser.getQuery();
        if (m_sourceTables.empty()) throw std::runtime_error("No target table.");

        const vConTable& target = m_sourceTables[0];
        std::vector<int> indicesToDelete;

        // Identificăm rândurile care respectă WHERE
        for (int i = 0; i < (int)target.records.size(); ++i) {
            if (!q.whereRoot || evaluateCondition(q.whereRoot, target.records[i], target)) {
                indicesToDelete.push_back(i);
            }
        }

        result.affectedIndices = indicesToDelete; // Adăugăm un nou membru în vConResult: std::vector<int>
        result.rowsAffected = indicesToDelete.size();
        result.success = true;
    }
    catch (const std::exception& e) {
        result.success = false;
        result.message = L"Delete Error: " + str_to_wstr(e.what());
    }

    auto end = std::chrono::high_resolution_clock::now();
    result.executionTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    return result;
}


vConResult vSqlEngine::executeSelect(const SqlQueryParser& parser) {
    vConResult result;
    auto start = std::chrono::high_resolution_clock::now();

    try {
        const Query& q = parser.getQuery();
        if (m_sourceTables.empty()) throw std::runtime_error("No source tables available");

        // --- STEP 0: CONSTRUCȚIA UNIVERSULUI (JOIN) ---
        // Începem cu primul tabel
        vConTable combinedSource = m_sourceTables[0];

        // Aplicăm JOIN-urile rând pe rând
        for (size_t i = 0; i < q.joins.size(); ++i) {
            const JoinClause& join = q.joins[i];

            // Căutăm tabelul din dreapta în universul nostru după alias/nume
            vConTable* rightTable = findTableInUniverse(join.table.getEffectiveName());
            if (!rightTable) {
                throw std::runtime_error("Table not found in universe: " + wstr_to_str(join.table.getEffectiveName()));
            }

            // Executăm îmbinarea (Nested Loop)
            combinedSource = performJoin(combinedSource, *rightTable, join);
        }

        // De acum înainte, tot motorul lucrează cu 'combinedSource' în loc de 'm_sourceTables[0]'
        const vConTable& workTable = combinedSource;

        // --- STEP 1: Pregătirea coloanelor (Expandare SELECT *) ---
        std::vector<QueryColumn> projectedColumns = expandWildcards(q.columns, workTable);
       

        // --- STEP 2: Filtrare (WHERE) ---
        std::vector<const std::vector<std::wstring>*> filteredRows;
        filteredRows.reserve(workTable.records.size());

        for (const auto& row : workTable.records) {
            if (!q.whereRoot || evaluateCondition(q.whereRoot, row, workTable)) {
                filteredRows.push_back(&row);
            }
        }

        // --- STEP 3: Sortare (ORDER BY + partial_sort optimization) ---
        if (!q.order_clauses.empty()) {
            applySorting(filteredRows, q, workTable);
        }

        // --- STEP 4: Pregătirea tabelului de lucru (workTable) ---
        vConTable finalTable;
        finalTable.tableName = workTable.tableName;

        // Stabilim coloanele finale (Headerele)
        for (const auto& col : projectedColumns) {
            
            finalTable.columns.push_back(col.alias.empty() ? col.rawExpression : col.alias);
            // Mapăm tipul de date (C, N, D etc.)

            int srcIdx = workTable.getColumnIndex(to_upper(col.rawExpression));
            if (srcIdx != -1) {
                finalTable.columnTypes.push_back(workTable.columnTypes[srcIdx]);
            }
            else {
                finalTable.columnTypes.push_back(L"C"); // Default Character
            }
        }

        // Copiem rândurile filtrate în tabelul temporar pentru procesare
        for (const auto* rowPtr : filteredRows) {
            finalTable.records.push_back(*rowPtr);
        }

        // --- STEP 5: EVALUAREA EXPRESIILOR (AICI ESTE CHEIA) ---
        // Această metodă va detecta singură dacă e AGREGAT (1 rând) sau NORMAL (N rânduri)
        evaluateExpressions(finalTable, workTable, projectedColumns);

        // --- STEP 6: LIMIT/OFFSET ---
        applyLimitOffset(finalTable.records, q.limit, q.offset);

        result.table = std::move(finalTable);
        result.rowsAffected = result.table.records.size();
        result.success = true;

        //return result;

    }
    catch (const std::exception& e) {
        result.success = false;
        result.message = L"Execution Error: " + str_to_wstr(e.what());
    }

    auto end = std::chrono::high_resolution_clock::now();
    result.executionTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    return result;
}


bool vSqlEngine::evaluateJoinCondition(const JoinClause& join, const std::vector<std::wstring>& combinedRow, const vConTable& joinedSchema) {
    if (join.on_conditions.empty()) return true;

    for (const auto& condition : join.on_conditions) {
        // 1. Extragem valorile folosind resolveExpression pe rândul combinat
        std::wstring leftVal = resolveExpression(condition.leftOperand.rawExpression, combinedRow, joinedSchema);
        std::wstring rightVal = resolveExpression(condition.rightOperand.rawExpression, combinedRow, joinedSchema);


        // LOG DE DEBUG:
         //LOG_INFO(L"JOIN Compare: [" + condition.leftOperand.rawExpression + L":" + leftVal + L"] == [" + 
           //       condition.rightOperand.rawExpression + L":" + rightVal + L"]");

        // 2. Căutăm handler-ul pentru operatorul specific (ex: "=", "LIKE", ">")
        std::wstring op = to_upper(condition.oper);
        if (m_opHandlers.count(op)) {
            // Executăm handler-ul (care știe deja să facă conversia la număr sau LIKE)
            if (!m_opHandlers[op](leftVal, rightVal)) {
                return false; // Una din condiții nu e satisfăcută
            }
        }
        else {
            // Dacă nu avem handler, putem decide să dăm eroare sau să comparăm ca string
            if (leftVal != rightVal) return false;
        }
    }

    return true; // Toate condițiile ON au trecut
}

/*
vConTable vSqlEngine::performJoin(const vConTable& left, const vConTable& right, const JoinClause& join) {
    vConTable res;
    res.tableName = L"JOIN_RESULT";

    // --- CONSTRUCȚIA SCHEMEI (O SINGURĂ DATĂ) ---

    // Tabel Stânga
    std::wstring lPrefix = left.tableAlias.empty() ? to_upper(left.tableName) : to_upper(left.tableAlias);
    for (size_t i = 0; i < left.columns.size(); ++i) {
        res.columns.push_back(lPrefix + L"." + to_upper(left.columns[i]));
        res.columnTypes.push_back(left.columnTypes[i]);
    }

    // Tabel Dreapta
    std::wstring rPrefix = right.tableAlias.empty() ? to_upper(right.tableName) : to_upper(right.tableAlias);
    for (size_t i = 0; i < right.columns.size(); ++i) {
        res.columns.push_back(rPrefix + L"." + to_upper(right.columns[i]));
        res.columnTypes.push_back(right.columnTypes[i]);
    }

    // --- CONSTRUCȚIA DATELOR (Nested Loop) ---
    for (const auto& lRow : left.records) {
        for (const auto& rRow : right.records) {
            std::vector<std::wstring> combinedRow = lRow;
            combinedRow.insert(combinedRow.end(), rRow.begin(), rRow.end());

            // Aici evaluateJoinCondition va primi schema cu prefixe (PERSOANE.ID)
            // și rândul de date combinat.
            if (evaluateJoinCondition(join, combinedRow, res)) {
                res.records.push_back(std::move(combinedRow));
            }
        }
    }
    return res;
}
*/
/*
vConTable vSqlEngine::performJoin(const vConTable& left, const vConTable& right, const JoinClause& join) {
    vConTable res;
    res.tableName = L"JOIN_RESULT";

    // 1. Construim Metadata (Schema combinată)
    // Păstrăm ordinea: coloanele din stânga, apoi cele din dreapta
    auto addColumns = [&](const vConTable& source) {
        // Folosim un prefix clar: TABEL.COLOANA
        std::wstring prefix = source.tableAlias.empty() ? to_upper(source.tableName) : to_upper(source.tableAlias);
        for (size_t i = 0; i < source.columns.size(); ++i) {
            // --- CORECȚIE AICI: L"." în loc de L" ISO." ---
            res.columns.push_back(prefix + L" ISO." + to_upper(source.columns[i])); // <--- ȘTERGE " ISO."
            res.columns.push_back(prefix + L"." + to_upper(source.columns[i]));     // <--- AȘA E CORECT

            res.columnTypes.push_back(source.columnTypes[i]);
            res.columnWidths.push_back(source.columnWidths[i]);
            res.columnDecimals.push_back(source.columnDecimals[i]);
        }
    };

    addColumns(left);
    addColumns(right);

    // 2. NESTED LOOP JOIN
    // Atenție la performanță: O(N * M)
    for (const auto& lRow : left.records) {
        for (const auto& rRow : right.records) {
            std::vector<std::wstring> combinedRow = lRow;
            combinedRow.insert(combinedRow.end(), rRow.begin(), rRow.end());

            // 3. Verificăm condiția ON (ex: a.id = b.id_pers)
            if (evaluateJoinCondition(join, combinedRow, res)) {
                res.records.push_back(std::move(combinedRow));
            }
        }
    }

    // 4. Dacă este LEFT JOIN și nu s-a găsit nicio potrivire pentru lRow? 
    // (Aici va trebui să adaugi logică pentru rânduri cu NULL-uri pe viitor)

    return res;
}
void vSqlEngine::applyFilters(vConTable& workTable) {
    
}
*/
vConTable vSqlEngine::performJoin(const vConTable& left, const vConTable& right, const JoinClause& join) {
    vConTable res;
    res.tableName = L"JOIN_RESULT";

    // 1. Construim Metadata (Header + Tipuri)
    auto addMeta = [&](const vConTable& src) {
        std::wstring pref = src.tableAlias.empty() ? to_upper(src.tableName) : to_upper(src.tableAlias);
        for (size_t i = 0; i < src.columns.size(); ++i) {
            res.columns.push_back(pref + L"." + to_upper(src.columns[i]));
            res.columnTypes.push_back(src.columnTypes[i]);
        }
    };
    addMeta(left);
    addMeta(right);

    // 2. Pregătim monitorizarea pentru Right/Full Join
    // rightMatched[j] va fi true dacă rândul j din right a fost folosit măcar o dată
    std::vector<bool> rightMatched(right.records.size(), false);

    // 3. Procesăm tabela din STÂNGA (Left Source)
    for (size_t i = 0; i < left.records.size(); ++i) {
        bool leftMatched = false;

        for (size_t j = 0; j < right.records.size(); ++j) {
            // Combinăm datele celor două rânduri
            std::vector<std::wstring> combinedRow = left.records[i];
            combinedRow.insert(combinedRow.end(), right.records[j].begin(), right.records[j].end());

            // Evaluăm condiția ON folosind schema tabelului rezultat (res)
            if (evaluateJoinCondition(join, combinedRow, res)) {
                res.records.push_back(combinedRow);
                leftMatched = true;
                rightMatched[j] = true; // Marcăm că acest rând din dreapta are pereche
            }
        }

        // Logică pentru LEFT / FULL JOIN: dacă rândul din stânga e "singur"
        if (!leftMatched && (join.type == JoinType::LEFT || join.type == JoinType::FULL)) {
            std::vector<std::wstring> nullRow = left.records[i];
            // Padding cu string-uri goale pentru toate coloanele din dreapta
            for (size_t k = 0; k < right.columns.size(); ++k) {
                nullRow.push_back(L"");
            }
            res.records.push_back(nullRow);
        }
    }

    // 4. Procesăm tabela din DREAPTA pentru RIGHT / FULL JOIN
    // Adăugăm rândurile din dreapta care NU au găsit pereche în stânga
    if (join.type == JoinType::RIGHT || join.type == JoinType::FULL) {
        for (size_t j = 0; j < right.records.size(); ++j) {
            if (!rightMatched[j]) {
                // Padding cu string-uri goale pentru toate coloanele din stânga
                std::vector<std::wstring> nullRow(left.columns.size(), L"");
                // Adăugăm datele reale din rândul de la dreapta
                nullRow.insert(nullRow.end(), right.records[j].begin(), right.records[j].end());
                res.records.push_back(nullRow);
            }
        }
    }

    return res;
}


/*
void vSqlEngine::evaluateExpressions(vConTable & workTable, const vConTable & sourceRef, const std::vector<QueryColumn>&projectedColumns) {
    const auto& query = m_queryParser.getQuery();

    // 1. Dacă NU avem GROUP BY și NU avem agregate, rămâne modul normal (deja implementat de tine)
    if (query.group_clauses.empty() && !query.isAggregate()) {
        for (auto& row : workTable.records) {
            std::vector<std::wstring> processedRow;
            for (const auto& col : projectedColumns) {
                processedRow.push_back(resolveExpression(col.rawExpression, row, sourceRef));
            }
            row = std::move(processedRow);
        }
        return;
    }

    // 2. LOGICA DE GRUPARE (Bucketizing)
    // Key: Valoarea grupului (ex: "YR"), Value: Listă de rânduri din acel grup
    std::map<std::wstring, std::vector<std::vector<std::wstring>>> groups;

    for (const auto& row : workTable.records) {
        std::wstring groupKey = L"";
        if (query.group_clauses.empty()) {
            groupKey = L"GLOBAL"; // Pentru SUM total fără GROUP BY
        }
        else {
            for (const auto& gc : query.group_clauses) {
                groupKey += resolveExpression(gc.column.rawExpression, row, sourceRef) + L"|";
            }
        }
        groups[groupKey].push_back(row);
    }

    // 3. GENERAREA REZULTATELOR DIN GRUPURI
    std::vector<std::vector<std::wstring>> finalRecords;

    for (auto& pair : groups) {
        auto& groupRows = pair.second;
        std::vector<std::wstring> resultRow(query.columns.size(), L"");

        // Pentru fiecare coloană din SELECT
        for (size_t i = 0; i < projectedColumns.size(); ++i) {
            const auto& col = projectedColumns[i];

            if (col.type == ColumnType::AGGREGATE) {
                // Rulăm agregatorul peste toate rândurile din grup
                std::wstring aggVal = L"";
                for (const auto& row : groupRows) {
                    std::wstring nextVal = (col.aggregateArg == L"*") ? L"" : resolveExpression(col.aggregateArg, row, sourceRef);
                    auto it = m_aggHandlers.find(to_upper(col.aggregateFunc));
                    if (it != m_aggHandlers.end()) {
                        aggVal = it->second(aggVal, nextVal);
                    }
                }
                resultRow[i] = aggVal;
            }
            else {
                // Pentru coloane normale, luăm valoarea din primul rând al grupului
                // (Standardul SQL cere ca aceste coloane să fie în GROUP BY)
                resultRow[i] = resolveExpression(col.rawExpression, groupRows[0], sourceRef);
            }
        }

        // --- FILTRARE HAVING (Pasul 4) ---
        // În evaluateExpressions, zona de HAVING
        if (query.havingRoot) {
            // 1. Creăm un "context" temporar pentru coloane
            // Mapăm numele expresiei la indexul ei din SELECT
            std::map<std::wstring, size_t> projectionIndices;
            for (size_t k = 0; k < projectedColumns.size(); ++k) {
                projectionIndices[to_upper(projectedColumns[k].rawExpression)] = k;
            }

            // 2. Transmitem acest context către evaluateCondition
            // Dacă evaluateCondition folosește un obiect de tip vConTable pentru structură:
            vConTable tempTable;
            for (auto& col : projectedColumns) tempTable.columns.push_back(col.rawExpression);

            if (evaluateCondition(query.havingRoot, resultRow, tempTable)) {
                finalRecords.push_back(resultRow);
            }
        }
        else {
            finalRecords.push_back(resultRow);
        }
    }

    workTable.records = std::move(finalRecords);
}
*/
/*
void vSqlEngine::evaluateExpressions(vConTable& workTable, const vConTable& sourceRef, const std::vector<QueryColumn>& projectedColumns) {
    const auto& query = m_queryParser.getQuery();

    // 1. Mod normal (Fără agregare și fără GROUP BY)
    if (query.group_clauses.empty() && !query.isAggregate()) {
        for (auto& row : workTable.records) {
            std::vector<std::wstring> processedRow;
            for (const auto& col : projectedColumns) {
                processedRow.push_back(resolveExpression(col.rawExpression, row, sourceRef));
            }
            row = std::move(processedRow);
        }
        return;
    }

    // 2. LOGICA DE GRUPARE (Bucketizing)
    std::map<std::wstring, std::vector<std::vector<std::wstring>>> groups;
    for (const auto& row : workTable.records) {
        std::wstring groupKey = query.group_clauses.empty() ? L"GLOBAL" : L"";
        if (!query.group_clauses.empty()) {
            for (const auto& gc : query.group_clauses) {
                groupKey += resolveExpression(gc.column.rawExpression, row, sourceRef) + L"|";
            }
        }
        groups[groupKey].push_back(row);
    }

    // 3. GENERAREA REZULTATELOR DIN GRUPURI
    std::vector<std::vector<std::wstring>> finalRecords;

    for (auto& pair : groups) {
        auto& groupRows = pair.second;
        std::vector<std::wstring> resultRow(projectedColumns.size(), L"");

        for (size_t i = 0; i < projectedColumns.size(); ++i) {
            const auto& col = projectedColumns[i];

            if (col.type == ColumnType::AGGREGATE) {
                // --- FAZA A: ACUMULARE ---
                std::wstring aggVal = L"";
                std::wstring funcUpper = to_upper(col.aggregateFunc);
                auto it = m_aggHandlers.find(funcUpper);

                for (const auto& row : groupRows) {
                    std::wstring nextVal = (col.aggregateArg == L"*") ? L"1" : resolveExpression(col.aggregateArg, row, sourceRef);
                    if (it != m_aggHandlers.end()) {
                        aggVal = it->second(aggVal, nextVal);
                    }
                }

                // --- FAZA B: FINALIZARE (Specific pentru AVG) ---
                if (funcUpper == L"AVG") {
                    size_t sep = aggVal.find(L':');
                    if (sep != std::wstring::npos) {
                        // Extragem "suma:numar" și convertim în "suma/numar" pentru vmath
                        std::string formula = std::string(aggVal.begin(), aggVal.end());
                        std::replace(formula.begin(), formula.end(), ':', '/');

                        // Apelăm engine-ul de math (evaluate_formula_fp)
                        double result = evaluate_formula_fp(formula);

                        // Formatăm rezultatul final
                        std::wstringstream ss;
                        ss << std::fixed << std::setprecision(4) << result;

                        // Curățăm zerourile inutile de la final (opțional)
                        std::wstring s = ss.str();
                        s.erase(s.find_last_not_of(L'0') + 1, std::wstring::npos);
                        if (!s.empty() && s.back() == L'.') s.pop_back();
                        aggVal = s;
                    }
                }
                resultRow[i] = aggVal;
            }
            else {
                resultRow[i] = resolveExpression(col.rawExpression, groupRows[0], sourceRef);
            }
        }

        // 4. FILTRARE HAVING
        if (query.havingRoot) {
            vConTable tempTable;
            // Schema temporară pentru HAVING (numele coloanelor sunt expresiile brute)
            for (auto& col : projectedColumns) tempTable.columns.push_back(col.rawExpression);

            if (evaluateCondition(query.havingRoot, resultRow, tempTable)) {
                finalRecords.push_back(resultRow);
            }
        }
        else {
            finalRecords.push_back(resultRow);
        }
    }

    workTable.records = std::move(finalRecords);
}
*/

void vSqlEngine::evaluateExpressions(vConTable& workTable, const vConTable& sourceRef, const std::vector<QueryColumn>& projectedColumns) {
    const auto& query = m_queryParser.getQuery();

    // 1. Mod normal (Fără agregare și fără GROUP BY)
    if (query.group_clauses.empty() && !query.isAggregate()) {
        for (auto& row : workTable.records) {
            std::vector<std::wstring> processedRow;
            for (const auto& col : projectedColumns) {
                processedRow.push_back(resolveExpression(col.rawExpression, row, sourceRef));
            }
            row = std::move(processedRow);
        }
        return;
    }

    // 2. LOGICA DE GRUPARE (Bucketizing)
    std::map<std::wstring, std::vector<std::vector<std::wstring>>> groups;
    for (const auto& row : workTable.records) {
        std::wstring groupKey = query.group_clauses.empty() ? L"GLOBAL" : L"";
        if (!query.group_clauses.empty()) {
            for (const auto& gc : query.group_clauses) {
                groupKey += resolveExpression(gc.column.rawExpression, row, sourceRef) + L"|";
            }
        }
        groups[groupKey].push_back(row);
    }

    // 3. GENERAREA REZULTATELOR DIN GRUPURI
    std::vector<std::vector<std::wstring>> finalRecords;

    for (auto& pair : groups) {
        auto& groupRows = pair.second;
        std::vector<std::wstring> resultRow(projectedColumns.size(), L"");

        for (size_t i = 0; i < projectedColumns.size(); ++i) {
            const auto& col = projectedColumns[i];

            if (col.type == ColumnType::AGGREGATE) {
                // --- FAZA A: ACUMULARE ---
                std::wstring aggVal = L"";
                std::wstring funcUpper = to_upper(col.aggregateFunc);
                auto it = m_aggHandlers.find(funcUpper);

                // Folosim noul splitSqlArguments pentru a separa argumentele (ex: nume, ', ')
                std::vector<std::wstring> args = splitSqlArguments(col.aggregateArg);

                for (const auto& row : groupRows) {
                    std::wstring nextVal;

                    if (funcUpper == L"STRING_AGG") {
                        // Rezolvăm coloana (primul argument)
                        std::wstring data = (args.size() > 0) ? resolveExpression(args[0], row, sourceRef) : L"";
                        // Separatorul rămâne brut (al doilea argument), va fi curățat în handler
                        std::wstring sep = (args.size() > 1) ? args[1] : L"', '";

                        // Împachetăm folosind un delimitator intern care nu e folosit în SQL
                        nextVal = data + L"|SEP|" + sep;
                    }
                    else {
                        // Logică standard pentru SUM, AVG, COUNT, MIN, MAX
                        nextVal = (col.aggregateArg == L"*") ? L"1" : resolveExpression(col.aggregateArg, row, sourceRef);
                    }

                    if (it != m_aggHandlers.end()) {
                        aggVal = it->second(aggVal, nextVal);
                    }
                }

                // --- FAZA B: FINALIZARE (Specific pentru AVG) ---
                if (funcUpper == L"AVG") {
                    size_t sep = aggVal.find(L':');
                    if (sep != std::wstring::npos) {
                        std::string formula = std::string(aggVal.begin(), aggVal.end());
                        std::replace(formula.begin(), formula.end(), ':', '/');

                        double result = evaluate_formula_fp(formula);

                        std::wstringstream ss;
                        ss << std::fixed << std::setprecision(4) << result;

                        std::wstring s = ss.str();
                        s.erase(s.find_last_not_of(L'0') + 1, std::wstring::npos);
                        if (!s.empty() && s.back() == L'.') s.pop_back();
                        aggVal = s;
                    }
                }
                resultRow[i] = aggVal;
            }
            else {
                resultRow[i] = resolveExpression(col.rawExpression, groupRows[0], sourceRef);
            }
        }

        // 4. FILTRARE HAVING
        if (query.havingRoot) {
            vConTable tempTable;
            for (auto& col : projectedColumns) tempTable.columns.push_back(col.rawExpression);
            if (evaluateCondition(query.havingRoot, resultRow, tempTable)) {
                finalRecords.push_back(resultRow);
            }
        }
        else {
            finalRecords.push_back(resultRow);
        }
    }

    workTable.records = std::move(finalRecords);
}

/*
bool vSqlEngine::evaluateCondition(std::shared_ptr<WhereClause> node, const std::vector<std::wstring>& row, const vConTable& table) {
    if (!node) return true;
    bool result = false;

    if (node->isGroup) {
        if (node->groupConnector == L"AND") {
            result = true; // Presupunem true pentru AND
            for (auto& child : node->subClauses) {
                if (!evaluateCondition(child, row, table)) {
                    result = false;
                    break;
                }
            }
        }
        else { // OR
            result = false;
            for (auto& child : node->subClauses) {
                if (evaluateCondition(child, row, table)) {
                    result = true;
                    break;
                }
            }
        }
    }
    else {
        // --- LOGICA PENTRU LEAF ---
        std::wstring left = wstr_trim(resolveExpression(node->leftOperand.rawExpression, row, table));
        std::wstring right = wstr_trim(resolveExpression(node->rightOperand.rawExpression, row, table));

        auto it = m_opHandlers.find(to_upper(node->oper));
        if (it != m_opHandlers.end()) {
            result = it->second(left, right);
        }
        else {
            // Dacă ai NOT LIKE implementat ca operator separat, 
            // el va fi găsit aici. Dacă nu, intră pe false.
            LOG_ERROR(L"Operator necunoscut: " + node->oper);
            result = false;
        }
    }

    // ACUM negația funcționează pentru TOATE tipurile de noduri
    return node->isNegated ? !result : result;
}
*/

// În SqlQueryEngine.cpp
bool vSqlEngine::evaluateCondition(
    std::shared_ptr<WhereClause> node,
    const std::vector<std::wstring>& row,
    const vConTable& table,
    const std::vector<std::wstring>& outerRow, // <-- Adăugat pentru corelare
    const vConTable& outerTable               // <-- Adăugat pentru corelare
) {
    if (!node) return true;
    bool result = false;

    if (node->isGroup) {
        if (node->groupConnector == L"AND") {
            result = true;
            for (auto& child : node->subClauses) {
                // Pasăm contextul mai departe recursiv
                if (!evaluateCondition(child, row, table, outerRow, outerTable)) {
                    result = false;
                    break;
                }
            }
        }
        else { // OR
            result = false;
            for (auto& child : node->subClauses) {
                if (evaluateCondition(child, row, table, outerRow, outerTable)) {
                    result = true;
                    break;
                }
            }
        }
    }
    else {
        // --- LOGICA PENTRU LEAF ---
        // Aici e cheia: resolveExpression trebuie să încerce să rezolve 
        // întâi în 'table', apoi în 'outerTable'
        std::wstring left = wstr_trim(resolveExpressionWithContext(node->leftOperand.rawExpression, row, table, outerRow, outerTable));
        std::wstring right = wstr_trim(resolveExpressionWithContext(node->rightOperand.rawExpression, row, table, outerRow, outerTable));

        auto it = m_opHandlers.find(to_upper(node->oper));
        if (it != m_opHandlers.end()) {
            result = it->second(left, right);
        }
        else {
            LOG_ERROR(L"Operator necunoscut: " + node->oper);
            result = false;
        }
    }

    return node->isNegated ? !result : result;
}

std::vector<std::wstring> vSqlEngine::splitSqlArguments(const std::wstring& s) {
    std::vector<std::wstring> args;
    std::wstring current;
    int parenLevel = 0;
    bool inQuotes = false;

    for (wchar_t c : s) {
        if (c == L'\'') inQuotes = !inQuotes;

        if (!inQuotes) {
            if (c == L'(') parenLevel++;
            else if (c == L')') parenLevel--;
        }

        if (c == L',' && parenLevel == 0 && !inQuotes) {
            args.push_back(wstr_trim(current));
            current.clear();
        }
        else {
            current += c;
        }
    }
    if (!current.empty()) args.push_back(wstr_trim(current));
    return args;
}

/*
std::wstring vSqlEngine::resolveExpression(std::wstring expr, const std::vector<std::wstring>& row, const vConTable& table) {
    // 1. Pregătire: curățăm spațiile albe
    expr = wstr_trim(expr);
    if (expr.empty()) return L"";

    // 2. Verificăm dacă este un literal STRING (între ghilimele simple sau duble)
    // Exemplu: 'Bucuresti' sau "airport" (ca valoare)
    if (expr.size() >= 2 &&
        ((expr.front() == L'\'' && expr.back() == L'\'') ||
            (expr.front() == L'\"' && expr.back() == L'\"'))) {
        return expr.substr(1, expr.size() - 2);
    }

    // 3. Detecție și Procesare FUNCȚII: NUME(...)
    size_t openParen = expr.find(L'(');
    size_t closeParen = expr.find_last_of(L')');

    if (openParen != std::wstring::npos && closeParen != std::wstring::npos && closeParen > openParen) {
        std::wstring funcName = to_upper(wstr_trim(expr.substr(0, openParen)));
        std::wstring inner = expr.substr(openParen + 1, closeParen - openParen - 1);

        // --- CAZ SPECIAL: TYPE() ---
        // Nu putem folosi handlerele normale aici pentru că avem nevoie de coloană, nu de valoare
        // -- - CAZ SPECIAL : TYPE()-- -
            if (funcName == L"TYPE") {
                // Folosim variabila 'inner' care este deja definită mai sus
                std::vector<std::wstring> rawArgs = splitSqlArguments(inner);
                if (rawArgs.empty()) return L"U";

                // 2. Curățăm numele coloanei (primul argument al funcției TYPE)
                std::wstring colName = wstr_trim(rawArgs[0]);
                std::wstring upperCol = to_upper(colName);

                // 3. Căutăm în schema tabelului
                int idx = table.getColumnIndex(upperCol);
                if (idx != -1) {
                    // Verificăm dacă avem tipuri de coloane definite
                    if (idx < (int)table.columnTypes.size()) {
                        wchar_t typeCode = table.columnTypes[idx][0];
                        switch (typeCode) {
                        case L'C': return L"Character";
                        case L'N': return L"Numeric";
                        case L'D': return L"Date";
                        case L'L': return L"Logical";
                        case L'M': return L"Memo";
                        default:   return table.columnTypes[idx];
                        }
                    }
                }

                // 4. Dacă nu e coloană, verificăm dacă e literal
                if (colName.size() >= 2 && (colName.front() == L'\'' || colName.front() == L'\"'))
                    return L"Literal String";

                // Verificare simplă dacă e număr
                if (!colName.empty() && (::iswdigit(colName[0]) || colName[0] == L'-'))
                    return L"Literal Number";

                return L"Unknown";
            }

        // --- RECURSIVITATE PENTRU ARGUMENTE ---
        // Împărțim "inner" (conținutul parantezelor) în argumente individuale
        std::vector<std::wstring> rawArgs = splitSqlArguments(inner);
        std::vector<std::wstring> resolvedArgs;

        for (const auto& arg : rawArgs) {
            // Fiecare argument este re-evaluat (permite funcții în funcții)
            resolvedArgs.push_back(resolveExpression(arg, row, table));
        }

        // --- EXECUTARE HANDLER ---
        // Căutăm în map-ul înregistrat în registerHandlers()
        auto it = m_funcHandlers.find(funcName);
        if (it != m_funcHandlers.end()) {
            return it->second(resolvedArgs);
        }

        // Dacă am ajuns aici, funcția nu este cunoscută
        return L"";
    }

    // 4. Căutare COLOANĂ (Case-Insensitive)
    // Normalizăm input-ul la UPPER pentru a se potrivi cu coloanele din DBF
    std::wstring upperExpr = expr;
    std::transform(upperExpr.begin(), upperExpr.end(), upperExpr.begin(), ::towupper);

    int colIdx = table.getColumnIndex(upperExpr);
    if (colIdx != -1) {
        // Dacă indexul coloanei este valid, returnăm valoarea din rândul curent
        return (colIdx < (int)row.size()) ? row[colIdx] : L"";
    }

    // 5. FALLBACK FINAL
    // Dacă nu e nici funcție, nici coloană, nici literal cu ghilimele,
    // este o constantă numerică (ex: 10) sau un literal ne-citat.
    return expr;
}
*/
/*
std::wstring vSqlEngine::resolveExpression(std::wstring expr, const std::vector<std::wstring>& row, const vConTable& table) {
    // 1. Pregătire: curățăm spațiile albe
    expr = wstr_trim(expr);
    if (expr.empty()) return L"";

    // 2. PRIORITATE: Căutare COLOANĂ (Case-Insensitive)
    // Verificăm dacă 'expr' este pur și simplu numele unei coloane din tabel.
    // Important pentru HAVING (unde COUNT(*) este deja un nume de coloană cu valoare calculată).
    std::wstring upperExpr = to_upper(expr);
    int colIdx = table.getColumnIndex(upperExpr);
    if (colIdx != -1) {
        return (colIdx < (int)row.size()) ? row[colIdx] : L"";
    }

    // 3. Verificăm dacă este un literal STRING (între ghilimele)
    if (expr.size() >= 2 &&
        ((expr.front() == L'\'' && expr.back() == L'\'') ||
            (expr.front() == L'\"' && expr.back() == L'\"'))) {
        return expr.substr(1, expr.size() - 2);
    }

    // 4. Detecție și Procesare FUNCȚII: NUME(...)
    size_t openParen = expr.find(L'(');
    size_t closeParen = expr.find_last_of(L')');

    if (openParen != std::wstring::npos && closeParen != std::wstring::npos && closeParen > openParen) {
    
        std::wstring funcName = to_upper(wstr_trim(expr.substr(0, openParen)));
        std::wstring inner = expr.substr(openParen + 1, closeParen - openParen - 1);
        

        // --- CAZ SPECIAL: TYPE() ---
        if (funcName == L"TYPE") {
            std::vector<std::wstring> rawArgs = splitSqlArguments(inner);
            if (rawArgs.empty()) return L"U";

            std::wstring arg = wstr_trim(rawArgs[0]);

            // 1. Dacă argumentul e o altă funcție (ex: TYPE(UPPER(...)))
            if (arg.find(L'(') != std::wstring::npos) {
                // O evaluăm, dar știm că majoritatea funcțiilor returnează Character
                // (Sau poți face o logică mai complexă aici ulterior)
                return L"Character";
            }

            // 2. Dacă e coloană
            int colIdx = table.getColumnIndex(to_upper(arg));
            if (colIdx != -1 && colIdx < (int)table.columnTypes.size()) {
                wchar_t typeCode = table.columnTypes[colIdx][0];
                switch (typeCode) {
                case L'C': return L"Character";
                case L'N': return L"Numeric";
                case L'D': return L"Date";
                case L'L': return L"Logical";
                case L'M': return L"Memo";
                default:   return table.columnTypes[colIdx];
                }
            }

            // 3. Fallback pentru literali
            if (arg.size() >= 1 && (arg.front() == L'\'' || arg.front() == L'\"'))
                return L"Literal String";
            if (!arg.empty() && (::iswdigit(arg[0]) || arg[0] == L'-'))
                return L"Literal Number";

            return L"Unknown";
        }

        // --- RECURSIVITATE PENTRU ARGUMENTE (Nested Functions) ---
        std::vector<std::wstring> rawArgs = splitSqlArguments(inner);
        std::vector<std::wstring> resolvedArgs;

        for (const auto& arg : rawArgs) {
            // Permite apeluri de tipul UPPER(SUBSTR(Column, 1, 3))
            resolvedArgs.push_back(resolveExpression(arg, row, table));
        }

        // --- EXECUTARE HANDLER FUNCȚIE ---
        auto it = m_funcHandlers.find(funcName);
        if (it != m_funcHandlers.end()) {
            return it->second(resolvedArgs);
        }

        return L""; // Funcție necunoscută
    }

    // 5. FALLBACK FINAL
    // Dacă nu e coloană, funcție sau string citat, e probabil un număr sau o constantă.
    return expr;
}
*/

/*
std::wstring vSqlEngine::resolveExpression(std::wstring expr, const std::vector<std::wstring>& row, const vConTable& table) {
    expr = wstr_trim(expr);
    if (expr.empty()) return L"";

    bool reevaluate = true;
    int depth = 0;

    while (reevaluate && depth < 10) {
        depth++;
        reevaluate = false;

        // 1. CURĂȚARE PARANTEZE "PARAZITE" (pentru cazul (p.ID = r.IDPERS) )
        bool changed = false;
        if (!expr.empty() && expr.front() == L'(') {
            // Verificăm dacă e început de funcție sau doar o paranteză de grupare rătăcită
            size_t openParen = expr.find(L'(');
            // Dacă paranteza e la început, verificăm dacă înainte de ea există un nume de funcție
            // În contextul în care expr începe cu '(', openParen e 0, deci nu e funcție
            if (openParen == 0) {
                expr.erase(0, 1);
                changed = true;
            }
        }
        if (!expr.empty() && expr.back() == L')') {
            // Scoatem paranteza de la final DOAR dacă nu am detectat o funcție validă mai jos
            // sau dacă am scos deja una din față
            expr.pop_back();
            changed = true;
        }

        if (changed) {
            expr = wstr_trim(expr);
            reevaluate = true;
            continue;
        }

        // 2. IDENTIFICARE COLOANĂ (Prioritate maximă după curățare)
        int colIdx = table.getColumnIndex(to_upper(expr));
        if (colIdx != -1) {
            // Returnăm valoarea sau string gol dacă rândul e de padding (JOIN)
            return (colIdx < (int)row.size()) ? row[colIdx] : L"";
        }

        // 3. LOGICĂ FUNCȚII (Inclusiv TYPE() și recursivitate)
        size_t openParen = expr.find(L'(');
        size_t lastParen = expr.find_last_of(L')');

        if (openParen != std::wstring::npos && lastParen != std::wstring::npos && lastParen > openParen) {
            std::wstring funcName = to_upper(wstr_trim(expr.substr(0, openParen)));
            std::wstring inner = expr.substr(openParen + 1, lastParen - openParen - 1);

            // --- IMPLEMENTARE TYPE() ---
            if (funcName == L"TYPE") {
                std::wstring arg = wstr_trim(inner);
                // Eliminăm ghilimelele dacă există: TYPE("NUME") -> NUME
                if (arg.size() >= 2 && (arg.front() == L'\'' || arg.front() == L'\"'))
                    arg = arg.substr(1, arg.size() - 2);

                int cIdx = table.getColumnIndex(to_upper(arg));
                if (cIdx != -1) {
                    // Dacă rândul e invalid/gol la acel index, e Undefined (specific JOIN-urilor)
                    if (cIdx >= row.size() || row[cIdx].empty()) return L"U";

                    // Returnăm tipul coloanei din metadata tabelului
                    if (!table.columnTypes.empty() && cIdx < table.columnTypes.size()) {
                        return table.columnTypes[cIdx]; // Returnează 'C', 'N', etc.
                    }
                }
                return L"U";
            }

            // --- RECURSIVITATE PENTRU ALTE FUNCȚII ---
            std::vector<std::wstring> rawArgs = splitSqlArguments(inner);
            std::vector<std::wstring> resolvedArgs;
            for (const auto& a : rawArgs) {
                resolvedArgs.push_back(resolveExpression(a, row, table));
            }

            auto it = m_funcHandlers.find(funcName);
            if (it != m_funcHandlers.end()) {
                return it->second(resolvedArgs);
            }
            return L""; // Funcție necunoscută
        }

        // 4. LITERALI STRING
        if (expr.size() >= 2 && ((expr.front() == L'\'' && expr.back() == L'\'') || (expr.front() == L'\"' && expr.back() == L'\"'))) {
            return expr.substr(1, expr.size() - 2);
        }
    }

    return expr; // Fallback: numere sau text neschimbat
}
*/
/*
std::wstring vSqlEngine::resolveExpression(std::wstring expr, const std::vector<std::wstring>& row, const vConTable& table) {
    expr = wstr_trim(expr);
    if (expr.empty()) return L"";

    bool reevaluate = true;
    int safety_net = 0;

    // Loop-ul de re-evaluare pentru a gestiona paranteze multiple sau funcții imbricate
    while (reevaluate && safety_net < 10) {
        reevaluate = false;
        safety_net++;

        // 1. CURĂȚARE PARANTEZE ORFANE (Protecție pentru JOIN ON (a=b) )
        // Dacă string-ul începe cu '(' dar nu se termină cu ')' (ex: "(persoane.ID")
        // Sau invers (ex: "rude.IDPERS)"), le eliminăm.
        if (!expr.empty() && expr.front() == L'(') {
            // Verificăm dacă e un apel de funcție: "UPPER(" nu trebuie tăiat
            // Dar "(ID" trebuie curățat
            size_t openP = expr.find(L'(');
            if (openP == 0 && (expr.find(L')') == std::wstring::npos || expr.find(L' ') != std::wstring::npos)) {
                expr.erase(0, 1);
                expr = wstr_trim(expr);
                reevaluate = true;
            }
        }
        if (!expr.empty() && expr.back() == L')') {
            if (expr.find(L'(') == std::wstring::npos) {
                expr.pop_back();
                expr = wstr_trim(expr);
                reevaluate = true;
            }
        }

        if (reevaluate) continue; // Reluăm dacă am curățat ceva

        // 2. IDENTIFICARE COLOANĂ (Cea mai importantă parte)
        int colIdx = table.getColumnIndex(to_upper(expr));
        if (colIdx != -1) {
            return (colIdx < (int)row.size()) ? row[colIdx] : L"";
        }

        // 3. LOGICĂ FUNCȚII (Inspecție vs Transformare)
        size_t openP = expr.find(L'(');
        size_t closeP = expr.find_last_of(L')');

        if (openP != std::wstring::npos && closeP != std::wstring::npos && closeP > openP) {
            std::wstring funcName = to_upper(wstr_trim(expr.substr(0, openP)));
            std::wstring inner = expr.substr(openP + 1, closeP - openP - 1);

            // --- EXCEPȚIA TYPE (Inspecție Metadata) ---
            if (funcName == L"TYPE") {
                std::wstring colName = wstr_trim(inner);
                // Scoatem ghilimelele: TYPE("NUME") -> NUME
                if (colName.size() >= 2 && (colName.front() == L'\"' || colName.front() == L'\''))
                    colName = colName.substr(1, colName.size() - 2);

                int cIdx = table.getColumnIndex(to_upper(colName));
                return (cIdx != -1) ? table.columnTypes[cIdx] : L"U";
            }

            // --- FUNCȚII DIN HANDLERE (Transformare Date) ---
            auto it = m_funcHandlers.find(funcName);
            if (it != m_funcHandlers.end()) {
                // RECURSIVITATE: Evaluăm argumentul (poate fi altă funcție sau coloană)
                // Exemplu: UPPER(LOWER(nume)) -> resolveExpression va fi apelat pentru LOWER(nume)
                std::wstring resolvedArg = resolveExpression(inner, row, table);

                std::vector<std::wstring> args = { resolvedArg };
                return it->second(args);
            }
        }

        // 4. LITERALI (String-uri între ghilimele)
        if (expr.size() >= 2 && (expr.front() == L'\"' || expr.front() == L'\'')) {
            return expr.substr(1, expr.size() - 2);
        }
    }

    // Dacă nimic nu a funcționat, returnăm expresia brută (fallback pentru numere sau constante)
    return expr;
}
*/
/*
std::wstring vSqlEngine::resolveExpression(std::wstring expr, const std::vector<std::wstring>& row, const vConTable& table) {
    expr = wstr_trim(expr);
    if (expr.empty()) return L"";

    // --- LOGICA NOUĂ: Detectare SUBQUERY din obiectul coloanei ---
    // Verificăm dacă expresia curentă corespunde unei coloane de tip SUBQUERY deja parsat
    // Notă: Ideal, ar trebui să pasăm obiectul QueryColumn dacă e disponibil, 
    // dar putem detecta și după string dacă acesta începe cu "(SELECT"
    std::wstring upperExpr = to_upper(expr);
    if (upperExpr.size() > 7 && upperExpr.substr(0, 7) == L"(SELECT") {

        // 1. Căutăm coloana corespunzătoare în table.columns pentru a accesa subSelect-ul gata parsat
        for (const auto& col : m_queryParser.getQuery().columns) { // currentQuery este query-ul principal


            std::wstring targetName = to_upper(col.subSelect->fromTable.name);
            vConTable* targetTable = nullptr;

            // 1. Căutăm tabelul în vectorul de surse
            for (auto& sourceTbl : m_sourceTables) {
                if (to_upper(sourceTbl.tableName) == targetName || to_upper(sourceTbl.tableAlias) == targetName) {
                    targetTable = &sourceTbl;
                    break;
                }
            }

            if (targetTable) {
                // 2. Scanăm tabelul țintă (ex: persoane) pentru corelare
                for (const auto& innerRow : targetTable->records) {

                    // 3. Evaluăm WHERE-ul subquery-ului în context dublu
                    // 'row' și 'table' sunt cele de la 'rude' (contextul exterior)
                    // 'innerRow' și '*targetTable' sunt contextul interior (persoane)
                    if (evaluateCondition(col.subSelect->whereRoot, innerRow, *targetTable, row, table)) {

                        // Dacă am găsit potrivirea, extragem prima coloană din SELECT-ul intern
                        // ex: SELECT nume FROM persoane... -> returnează valoarea coloanei 'nume'
                        return resolveExpression(col.subSelect->columns[0].rawExpression, innerRow, *targetTable);
                    }
                }
            }
            return L"NULL";
        }
    }
    ////////////////////////////

    bool reevaluate = true;
    int safety_net = 0;

    while (reevaluate && safety_net < 10) {
        reevaluate = false;
        safety_net++;

        // 1. CURĂȚARE PARANTEZE ORFANE
        if (!expr.empty() && expr.front() == L'(') {
            size_t openP = expr.find(L'(');
            if (openP == 0 && (expr.find(L')') == std::wstring::npos || expr.find(L' ') != std::wstring::npos)) {
                expr.erase(0, 1);
                expr = wstr_trim(expr);
                reevaluate = true;
            }
        }
        if (!expr.empty() && expr.back() == L')') {
            if (expr.find(L'(') == std::wstring::npos) {
                expr.pop_back();
                expr = wstr_trim(expr);
                reevaluate = true;
            }
        }
        if (reevaluate) continue;

        // 2. IDENTIFICARE COLOANĂ 
        int colIdx = table.getColumnIndex(to_upper(expr));
        if (colIdx != -1) {
            return (colIdx < (int)row.size()) ? row[colIdx] : L"";
        }

        // 3. LOGICĂ FUNCȚII
        size_t openP = expr.find(L'(');
        size_t closeP = expr.find_last_of(L')');

        if (openP != std::wstring::npos && closeP != std::wstring::npos && closeP > openP) {
            std::wstring funcName = to_upper(wstr_trim(expr.substr(0, openP)));
            std::wstring inner = expr.substr(openP + 1, closeP - openP - 1);

            // --- EXCEPȚIA TYPE (Inspecție Metadata) ---
            if (funcName == L"TYPE") {
                std::wstring colName = wstr_trim(inner);
                if (colName.size() >= 2 && (colName.front() == L'\"' || colName.front() == L'\''))
                    colName = colName.substr(1, colName.size() - 2);

                int cIdx = table.getColumnIndex(to_upper(colName));
                return (cIdx != -1) ? table.columnTypes[cIdx] : L"U";
            }

            // --- FUNCȚII DIN HANDLERE (Transformare Date) ---
            auto it = m_funcHandlers.find(funcName);
            if (it != m_funcHandlers.end()) {

                // --- MODIFICAREA AICI: Spargem argumentele ---
                std::vector<std::wstring> rawArgs = splitSqlArguments(inner);
                std::vector<std::wstring> resolvedArgs;

                for (const auto& arg : rawArgs) {
                    // Evaluăm recursiv fiecare argument (coloană, altă funcție, literal)
                    resolvedArgs.push_back(resolveExpression(arg, row, table));
                }

                // Apelăm handler-ul cu TOATE argumentele rezolvate
                return it->second(resolvedArgs);
            }
        }

        // 4. LITERALI
        if (expr.size() >= 2 && (expr.front() == L'\"' || expr.front() == L'\'')) {
            return expr.substr(1, expr.size() - 2);
        }
    }

    return expr;
}
*/
/*
std::wstring vSqlEngine::resolveExpression(std::wstring expr, const std::vector<std::wstring>& row, const vConTable& table) {
    expr = wstr_trim(expr);
    if (expr.empty()) return L"";

    // --- 1. LOGICA PENTRU SUBQUERY ---
    std::wstring upperExpr = to_upper(expr);
    // Verificăm dacă expresia pare a fi un subquery (începe cu paranteză și SELECT)
    if (upperExpr.size() > 7 && upperExpr.find(L"(SELECT") != std::wstring::npos) {

        // Căutăm în definiția query-ului coloana care are această expresie raw
        for (const auto& col : m_queryParser.getQuery().columns) {
            if (col.type == ColumnType::SUBQUERY && col.rawExpression == expr) {

                // VERIFICARE CRITICĂ: Dacă parserul nu a reușit să creeze obiectul subSelect, ieșim
                if (!col.subSelect) {
                    LOG_ERROR(L"Eroare: subSelect este null pentru " + expr);
                    continue;
                }

                std::wstring targetName = to_upper(col.subSelect->fromTable.name);
                vConTable* targetTable = nullptr;

                // Căutăm tabelul țintă în sursele încărcate
                for (auto& sourceTbl : m_sourceTables) {
                    if (to_upper(sourceTbl.tableName) == targetName || to_upper(sourceTbl.tableAlias) == targetName) {
                        targetTable = &sourceTbl;
                        break;
                    }
                }

                if (targetTable) {
                    // Scanăm rândurile tabelului din subquery (ex: persoane)
                    for (const auto& innerRow : targetTable->records) {
                        // Evaluăm condiția WHERE a subquery-ului folosind ambele contexte
                        // row/table = RUDE (exterior), innerRow/*targetTable = PERSOANE (interior)
                        if (evaluateCondition(col.subSelect->whereRoot, innerRow, *targetTable, row, table)) {

                            // Dacă am găsit potrivirea, returnăm prima coloană din SELECT-ul intern
                            // (Recursiv, pentru a permite funcții și în interiorul subquery-ului)
                            if (!col.subSelect->columns.empty()) {
                                return resolveExpression(col.subSelect->columns[0].rawExpression, innerRow, *targetTable);
                            }
                        }
                    }
                }
                // Dacă nu găsim nicio potrivire în subquery, returnăm NULL conform standardului SQL
                return L"NULL";
            }
        }
    }

    // --- 2. LOGICA DE EVALUARE STANDARD (Coloane, Funcții, Literali) ---
    bool reevaluate = true;
    int safety_net = 0;

    while (reevaluate && safety_net < 10) {
        reevaluate = false;
        safety_net++;

        // Curățare paranteze exterioare care nu sunt funcții
        if (!expr.empty() && expr.front() == L'(') {
            // Verificăm dacă e o paranteză de grupare, nu de funcție
            if (expr.back() == L')' && expr.find(L' ') != std::wstring::npos) {
                expr = wstr_trim(expr.substr(1, expr.size() - 2));
                reevaluate = true;
                continue;
            }
        }

        // Identificare Coloană în tabelul curent
        int colIdx = table.getColumnIndex(to_upper(expr));
        if (colIdx != -1) {
            return (colIdx < (int)row.size()) ? row[colIdx] : L"";
        }

        // Logică Funcții (SUM, UPPER, TYPE, etc.)
        size_t openP = expr.find(L'(');
        size_t closeP = expr.find_last_of(L')');

        if (openP != std::wstring::npos && closeP != std::wstring::npos && closeP > openP) {
            std::wstring funcName = to_upper(wstr_trim(expr.substr(0, openP)));
            std::wstring inner = expr.substr(openP + 1, closeP - openP - 1);

            // Handler special pentru TYPE() - Metadata
            if (funcName == L"TYPE") {
                std::wstring colName = wstr_trim(inner);
                if (colName.size() >= 2 && (colName.front() == L'\"' || colName.front() == L'\''))
                    colName = colName.substr(1, colName.size() - 2);

                int cIdx = table.getColumnIndex(to_upper(colName));
                return (cIdx != -1) ? table.columnTypes[cIdx] : L"U";
            }

            // Handlere funcții înregistrate
            auto it = m_funcHandlers.find(funcName);
            if (it != m_funcHandlers.end()) {
                std::vector<std::wstring> rawArgs = splitSqlArguments(inner);
                std::vector<std::wstring> resolvedArgs;
                for (const auto& arg : rawArgs) {
                    resolvedArgs.push_back(resolveExpression(arg, row, table));
                }
                return it->second(resolvedArgs);
            }
        }

        // Literali (String-uri între ghilimele)
        if (expr.size() >= 2 && (expr.front() == L'\"' || expr.front() == L'\'')) {
            return expr.substr(1, expr.size() - 2);
        }



    }


   


    return expr;
}
*/

std::wstring vSqlEngine::resolveExpression(std::wstring expr, const std::vector<std::wstring>& row, const vConTable& table) {
    expr = wstr_trim(expr);
    if (expr.empty()) return L"";

    // --- 1. LOGICA PENTRU SUBQUERY (Rămâne neschimbată) ---
    std::wstring upperExpr = to_upper(expr);
    // --- 1. LOGICA PENTRU SUBQUERY (Îmbunătățită) ---
    if (upperExpr.size() > 7 && upperExpr.find(L"(SELECT") != std::wstring::npos) {
        // Extragem subquery-ul dintre paranteze dacă e nevoie
        std::wstring subSql = expr;
        if (subSql.front() == L'(' && subSql.back() == L')') {
            subSql = subSql.substr(1, subSql.size() - 2);
        }

        // Creăm un parser temporar pentru acest subquery specific
        // sau refolosim obiectul col.subSelect dacă îl găsim parțial în expr
        for (const auto& col : m_queryParser.getQuery().columns) {
            // Verificăm dacă subquery-ul parsat este CONȚINUT în expresia curentă
            if (col.type == ColumnType::SUBQUERY && expr.find(col.rawExpression) != std::wstring::npos) {

                if (!col.subSelect) return L"NULL";

                std::wstring targetName = to_upper(col.subSelect->fromTable.name);
                vConTable* targetTable = nullptr;
                for (auto& sourceTbl : m_sourceTables) {
                    if (to_upper(sourceTbl.tableName) == targetName || to_upper(sourceTbl.tableAlias) == targetName) {
                        targetTable = &sourceTbl;
                        break;
                    }
                }

                if (targetTable) {
                    for (const auto& innerRow : targetTable->records) {
                        if (evaluateCondition(col.subSelect->whereRoot, innerRow, *targetTable, row, table)) {
                            if (!col.subSelect->columns.empty()) {
                                // Executăm și returnăm valoarea
                                return resolveExpression(col.subSelect->columns[0].rawExpression, innerRow, *targetTable);
                            }
                        }
                    }
                }
                return L"0"; // Returnăm 0 în loc de NULL pentru a nu strica aritmetica (+1)
            }
        }
    }

    // --- 2. LOGICA DE EVALUARE RECURSIVĂ ---
    bool reevaluate = true;
    int safety_net = 0;

    while (reevaluate && safety_net < 10) {
        reevaluate = false;
        safety_net++;

        // A. Curățare paranteze
        if (!expr.empty() && expr.front() == L'(' && expr.back() == L')') {
            // Verificăm dacă sunt paranteze de grupare (nu funcție)
            size_t openP = expr.find(L'(');
            if (openP == 0) {
                expr = wstr_trim(expr.substr(1, expr.size() - 2));
                reevaluate = true;
                continue;
            }
        }

        // B. Coloană directă
        int colIdx = table.getColumnIndex(to_upper(expr));
        if (colIdx != -1) return (colIdx < (int)row.size()) ? row[colIdx] : L"";

        // C. Funcții (UPPER, TYPE, etc.)
        size_t funcOpenP = expr.find(L'(');
        size_t funcCloseP = expr.find_last_of(L')');
        if (funcOpenP != std::wstring::npos && funcCloseP != std::wstring::npos && funcCloseP > funcOpenP) {
            std::wstring funcName = to_upper(wstr_trim(expr.substr(0, funcOpenP)));
            // Verificăm dacă e un nume de funcție valid (nu doar o paranteză matematică)
            if (!funcName.empty() && std::iswalpha(funcName[0])) {
                std::wstring inner = expr.substr(funcOpenP + 1, funcCloseP - funcOpenP - 1);

                if (funcName == L"TYPE") {
                    std::wstring colName = wstr_trim(inner);
                    if (colName.size() >= 2 && (colName.front() == L'\"' || colName.front() == L'\''))
                        colName = colName.substr(1, colName.size() - 2);
                    int cIdx = table.getColumnIndex(to_upper(colName));
                    return (cIdx != -1) ? table.columnTypes[cIdx] : L"U";
                }

                auto it = m_funcHandlers.find(funcName);
                if (it != m_funcHandlers.end()) {
                    std::vector<std::wstring> rawArgs = splitSqlArguments(inner);
                    std::vector<std::wstring> resolvedArgs;
                    for (const auto& arg : rawArgs) resolvedArgs.push_back(resolveExpression(arg, row, table));
                    return it->second(resolvedArgs);
                }
            }
        }

        // D. ARITMETICĂ (Mutată în interiorul buclei pentru a permite compunerea)
        // Căutăm operatorii în ordinea inversă a priorității (+/- apoi */)
        std::vector<std::wstring> ops = { L"+", L"-", L"*", L"/" };
        for (const auto& op : ops) {
            size_t opPos = expr.find(op);
            if (opPos != std::wstring::npos && opPos > 0) {
                std::wstring leftStr = wstr_trim(expr.substr(0, opPos));
                std::wstring rightStr = wstr_trim(expr.substr(opPos + 1));

                std::wstring leftVal = resolveExpression(leftStr, row, table);
                std::wstring rightVal = resolveExpression(rightStr, row, table);

                try {
                    double leftNum = std::stod(leftVal);
                    double rightNum = std::stod(rightVal);
                    double res = 0;
                    if (op == L"+") res = leftNum + rightNum;
                    else if (op == L"-") res = leftNum - rightNum;
                    else if (op == L"*") res = leftNum * rightNum;
                    else if (op == L"/") res = (rightNum != 0) ? leftNum / rightNum : 0;

                    std::wstring resStr = std::to_wstring(res);
                    resStr.erase(resStr.find_last_not_of(L'0') + 1, std::string::npos);
                    if (resStr.back() == L'.') resStr.pop_back();
                    return resStr;
                }
                catch (...) { /* Mergem mai departe dacă nu e numeric */ }
            }
        }

        // E. Literali
        if (expr.size() >= 2 && (expr.front() == L'\"' || expr.front() == L'\'')) {
            return expr.substr(1, expr.size() - 2);
        }
    }

    return expr;
}

void vSqlEngine::printResult() {
   
}

void vSqlEngine::projectColumns(vConTable& table) {
   
}


double vSqlEngine::asNumber(const std::wstring& s) {
    if (s.empty()) return 0.0;
    try {
        return std::stod(s);
    }
    catch (...) {
        return 0.0; // Sau NaN, depinde cum vrei să tratezi erorile
    }
}


bool vSqlEngine::handleLikeOp(const std::wstring& left, const std::wstring& right) {
    if (right.empty()) return false;

    std::wstring target = to_upper(left);
    std::wstring pattern = to_upper(right);

    bool startWild = (pattern.front() == L'%');
    bool endWild = (pattern.back() == L'%');

    if (startWild && endWild) {
        // %STRING% -> căutare oriunde
        std::wstring search = pattern.substr(1, pattern.size() - 2);
        return target.find(search) != std::wstring::npos;
    }
    if (startWild) {
        // %STRING -> se termină cu
        std::wstring search = pattern.substr(1);
        if (target.size() < search.size()) return false;
        return target.compare(target.size() - search.size(), search.size(), search) == 0;
    }
    if (endWild) {
        // STRING% -> începe cu
        std::wstring search = pattern.substr(0, pattern.size() - 1);
        return target.find(search) == 0;
    }

    // Fără wildcard-uri -> egalitate exactă
    return target == pattern;
}


std::vector<QueryColumn> vSqlEngine::expandWildcards(const std::vector<QueryColumn>& columns, const vConTable& source) {
    std::vector<QueryColumn> expanded;
    for (const auto& col : columns) {
        if (col.type == ColumnType::WILDCARD || col.rawExpression == L"*") {
            for (const auto& realColName : source.columns) {
                QueryColumn newCol;
                newCol.rawExpression = realColName;
                newCol.type = ColumnType::RAW_FIELD;
                expanded.push_back(newCol);
            }
        }
        else {
            expanded.push_back(col);
        }
    }
    return expanded;
}

void vSqlEngine::applySorting(std::vector<const std::vector<std::wstring>*>& rows, const Query& q, const vConTable& source) {
    size_t numToSort = rows.size();
    bool usePartial = false;

    if (q.limit >= 0) {
        numToSort = std::min<size_t>(rows.size(), static_cast<size_t>(q.offset + q.limit));
        usePartial = numToSort < rows.size();
    }

    auto comp = [&](const std::vector<std::wstring>* a, const std::vector<std::wstring>* b) {
        for (const auto& order : q.order_clauses) {
            std::wstring valA = resolveExpression(order.column.rawExpression, *a, source);
            std::wstring valB = resolveExpression(order.column.rawExpression, *b, source);
            if (valA == valB) continue;

            bool isLess = (isNumber(valA) && isNumber(valB)) ? std::stod(valA) < std::stod(valB) : valA < valB;
            return (order.direction == OrderDirection::ASC) ? isLess : !isLess;
        }
        return false;
    };

    if (usePartial) {
        std::partial_sort(rows.begin(), rows.begin() + numToSort, rows.end(), comp);
        rows.resize(numToSort);
    }
    else {
        std::stable_sort(rows.begin(), rows.end(), comp);
    }
}

void vSqlEngine::applyLimitOffset(std::vector<std::vector<std::wstring>>& records, int limit, int offset) {
    if (offset > 0) {
        if (static_cast<size_t>(offset) >= records.size()) {
            records.clear();
            return;
        }
        records.erase(records.begin(), records.begin() + offset);
    }
    if (limit >= 0 && static_cast<size_t>(limit) < records.size()) {
        records.resize(limit);
    }
}

std::wstring vSqlEngine::resolveExpressionWithContext(
    std::wstring expr,
    const std::vector<std::wstring>& row, const vConTable& table,
    const std::vector<std::wstring>& outerRow, const vConTable& outerTable)
{
    expr = wstr_trim(expr);
    std::wstring upperExpr = to_upper(expr);

    // --- LOGICA NOUĂ: Gestionare Prefix Tabel (Tabel.Coloana) ---
    size_t dotPos = upperExpr.find(L'.');
    if (dotPos != std::wstring::npos) {
        std::wstring tblPrefix = upperExpr.substr(0, dotPos);
        std::wstring colName = upperExpr.substr(dotPos + 1);

        // 1. Verificăm dacă prefixul e tabelul curent (ex: PERSOANE)
        if (tblPrefix == to_upper(table.tableName) || tblPrefix == to_upper(table.tableAlias)) {
            int idx = table.getColumnIndex(colName);
            if (idx != -1) return (idx < row.size()) ? row[idx] : L"";
        }

        // 2. Verificăm dacă prefixul e tabelul exterior (ex: RUDE)
        if (!outerRow.empty()) {
            if (tblPrefix == to_upper(outerTable.tableName) || tblPrefix == to_upper(outerTable.tableAlias)) {
                int oIdx = outerTable.getColumnIndex(colName);
                if (oIdx != -1) return (oIdx < outerRow.size()) ? outerRow[oIdx] : L"";
            }
        }
    }

    // --- LOGICA EXISTENTĂ: Căutare fără prefix ---
    int idx = table.getColumnIndex(upperExpr);
    if (idx != -1) return (idx < row.size()) ? row[idx] : L"";

    if (!outerRow.empty()) {
        int oIdx = outerTable.getColumnIndex(upperExpr);
        if (oIdx != -1) return (oIdx < outerRow.size()) ? outerRow[oIdx] : L"";
    }

    // 3. Dacă e funcție sau literal
    return resolveExpression(expr, row, table);
}

// O metodă internă pentru a extrage valoarea, ținând cont de contextul dublu
std::wstring vSqlEngine::getValWithContext(std::wstring identifier,
    const std::vector<std::wstring>& row, const vConTable& table,
    const std::vector<std::wstring>& outerRow, const vConTable& outerTable)
{
    identifier = to_upper(wstr_trim(identifier));

    // 1. Verificăm dacă avem formatul Tabel.Coloana (ex: RUDE.IDPERS)
    size_t dotPos = identifier.find(L'.');
    if (dotPos != std::wstring::npos) {
        std::wstring tblPart = identifier.substr(0, dotPos);
        std::wstring colPart = identifier.substr(dotPos + 1);

        // Este tabelul curent (interior)?
        if (tblPart == to_upper(table.tableName) || tblPart == to_upper(table.tableAlias)) {
            int idx = table.getColumnIndex(colPart);
            if (idx != -1) return row[idx];
        }

        // Este tabelul exterior (corelare)?
        if (!outerRow.empty()) {
            if (tblPart == to_upper(outerTable.tableName) || tblPart == to_upper(outerTable.tableAlias)) {
                int idx = outerTable.getColumnIndex(colPart);
                if (idx != -1) return outerRow[idx];
            }
        }
    }

    // 2. Dacă e un nume simplu de coloană, căutăm întâi în interior, apoi în exterior
    int idx = table.getColumnIndex(identifier);
    if (idx != -1) return row[idx];

    if (!outerRow.empty()) {
        idx = outerTable.getColumnIndex(identifier);
        if (idx != -1) return outerRow[idx];
    }

    // 3. Dacă nu e coloană, înseamnă că e un literal sau o funcție
    // Folosim resolveExpression-ul standard (fără contextul de corelare)
    return resolveExpression(identifier, row, table);
}