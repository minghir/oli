#ifndef DB_MANAGER_HPP
#define DB_MANAGER_HPP  
#include "dbConnection.hpp"
#include <unordered_map>
#include <memory>
#include <mutex>

class DbManager {
private:
    std::unordered_map<std::wstring, std::unique_ptr<dbConnection>> connections;
    std::mutex mtx;

public:
    static DbManager& instance() {
        static DbManager inst;
        return inst;
    }

    bool addConnection(const std::wstring& name, std::unique_ptr<dbConnection> conn) {
        std::lock_guard<std::mutex> lock(mtx);
        if (connections.find(name) != connections.end()) return false;
        connections[name] = std::move(conn);
        return true;
    }

    dbConnection* getConnection(const std::wstring& name) {
        std::lock_guard<std::mutex> lock(mtx);
        auto it = connections.find(name);
        return (it != connections.end()) ? it->second.get() : nullptr;
    }

    bool hasConnection(const std::wstring& name) {
        std::lock_guard<std::mutex> lock(mtx);
        return connections.find(name) != connections.end();
    }

    bool removeConnection(const std::wstring& name) {
    std::lock_guard<std::mutex> lock(mtx);
    return connections.erase(name) > 0;
}

    void clearConnections() {
        std::lock_guard<std::mutex> lock(mtx);
        connections.clear();
    }   


};
#endif // DB_MANAGER_HPP