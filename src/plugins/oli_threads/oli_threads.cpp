#include "../../IOliEngine.hpp"
#include "../../vData.hpp"

#if defined(_WIN32) || defined(_WIN64)
#define OLI_EXPORT extern "C" __declspec(dllexport)
#else
#define OLI_EXPORT extern "C" __attribute__((visibility("default")))
#endif

#include <vector>
#include <unordered_map>
#include <functional>
#include <future>
#include <mutex>
#include <thread>
#include <atomic>
#include <variant>

using PluginRegistry = std::unordered_map<std::wstring, std::function<vData(const std::vector<vData>&)>>;
using CommandRegistry = std::unordered_map<std::wstring, std::function<void(const std::wstring&)>>;

static IOliEngine* g_engine = nullptr;

// ============================================================================
// EXPORTURI DUMMY (Satisfac verificările oli.exe fără a lega consola)
// ============================================================================
typedef void(*ConsoleLogFn)(const wchar_t*);

OLI_EXPORT void SetConsoleFn([[maybe_unused]] ConsoleLogFn fn) {}
OLI_EXPORT void setConsoleFn([[maybe_unused]] ConsoleLogFn fn) {}
OLI_EXPORT void SetPluginConsoleManager([[maybe_unused]] void* dummy) {}

// ============================================================================
// ENTRY POINTS PENTRU MOTOR
// ============================================================================

// Capturăm pointerul IOliEngine* pe care oli.exe îl transmite la încărcare
OLI_EXPORT void LoadOliCommandPlugin([[maybe_unused]] CommandRegistry& handlers, IOliEngine* engine) {
    g_engine = engine;
}

class ThreadManager {
private:
    std::unordered_map<long long, std::future<vData>> m_tasks;
    std::mutex m_mutex;
    std::atomic<long long> m_nextHandle{ 1 };

public:
    static ThreadManager& getInstance() {
        static ThreadManager instance;
        return instance;
    }

    long long spawnTask(std::function<vData()> task) {
        long long handle = m_nextHandle++;
        std::lock_guard<std::mutex> lock(m_mutex);
        m_tasks[handle] = std::async(std::launch::async, task);
        return handle;
    }

    vData joinTask(long long handle) {
        std::future<vData> fut;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_tasks.find(handle);
            if (it == m_tasks.end()) return vData{ 0LL };
            fut = std::move(it->second);
            m_tasks.erase(it);
        }
        if (fut.valid()) {
            return fut.get();
        }
        return vData{ 0LL };
    }
};

inline long long toInt(const vData& v) {
    if (std::holds_alternative<long long>(v.value)) return std::get<long long>(v.value);
    if (std::holds_alternative<double>(v.value)) return static_cast<long long>(std::get<double>(v.value));
    return 0;
}

// Înregistrarea funcțiilor apelabile din Oli (THREAD_SPAWN, THREAD_WAIT)
OLI_EXPORT void LoadOliPlugin(PluginRegistry& registry) {

    registry[L"THREAD_SPAWN"] = [](const std::vector<vData>& a) -> vData {
        if (a.empty()) return vData{ 0LL };

        std::wstring funcName = std::holds_alternative<std::wstring>(a[0].value)
            ? std::get<std::wstring>(a[0].value)
            : L"";

        if (funcName.empty() || !g_engine) return vData{ 0LL };

        std::vector<vData> funcArgs(a.begin() + 1, a.end());

        long long handle = ThreadManager::getInstance().spawnTask([funcName, funcArgs]() {
            return g_engine->callFunctionIsolated(funcName, funcArgs);
        });

        return vData{ handle };
    };

    registry[L"THREAD_WAIT"] = [](const std::vector<vData>& a) -> vData {
        if (a.empty()) return vData{ 0LL };
        long long handle = toInt(a[0]);
        return ThreadManager::getInstance().joinTask(handle);
    };
}