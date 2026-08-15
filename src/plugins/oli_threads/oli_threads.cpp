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
#include <memory>
#include <queue>
#include <condition_variable>
#include <chrono>

using PluginRegistry = std::unordered_map<std::wstring, std::function<vData(const std::vector<vData>&)>>;
using CommandRegistry = std::unordered_map<std::wstring, std::function<void(const std::wstring&)>>;

static IOliEngine* g_engine = nullptr;

// --- THREAD-LOCAL CANCELLATION TOKEN ---
thread_local std::shared_ptr<std::atomic<bool>> g_currentCancelFlag = nullptr;

// --- DUMMY EXPORTS ---
typedef void(*ConsoleLogFn)(const wchar_t*);
OLI_EXPORT void SetConsoleFn([[maybe_unused]] ConsoleLogFn fn) {}
OLI_EXPORT void setConsoleFn([[maybe_unused]] ConsoleLogFn fn) {}
OLI_EXPORT void SetPluginConsoleManager([[maybe_unused]] void* dummy) {}

OLI_EXPORT void LoadOliCommandPlugin([[maybe_unused]] CommandRegistry& handlers, IOliEngine* engine) {
    g_engine = engine;
}

// --- COADĂ DE MESAJE THREAD-SAFE ---
struct ThreadSafeQueue {
    std::queue<vData> queue;
    std::mutex mtx;
    std::condition_variable cv;

    void push(const vData& val) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            queue.push(val);
        }
        cv.notify_one();
    }

    vData pop() {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this]() { return !queue.empty(); });
        vData val = queue.front();
        queue.pop();
        return val;
    }

    bool popTimeout(long long timeoutMs, vData& result) {
        std::unique_lock<std::mutex> lock(mtx);
        bool hasData = cv.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this]() { 
            return !queue.empty(); 
        });

        if (!hasData) return false;

        result = queue.front();
        queue.pop();
        return true;
    }

    size_t size() {
        std::lock_guard<std::mutex> lock(mtx);
        return queue.size();
    }
};

class QueueManager {
private:
    std::unordered_map<std::wstring, std::shared_ptr<ThreadSafeQueue>> m_queues;
    std::mutex m_mapMutex;

public:
    static QueueManager& getInstance() {
        static QueueManager instance;
        return instance;
    }

    std::shared_ptr<ThreadSafeQueue> getQueue(const std::wstring& name) {
        std::lock_guard<std::mutex> lock(m_mapMutex);
        auto it = m_queues.find(name);
        if (it == m_queues.end()) {
            auto q = std::make_shared<ThreadSafeQueue>();
            m_queues[name] = q;
            return q;
        }
        return it->second;
    }
};

// --- GESTIONAR DE MUTEX-URI CU NUME ---
class MutexManager {
private:
    std::unordered_map<std::wstring, std::unique_ptr<std::recursive_mutex>> m_mutexes;
    std::mutex m_mapMutex;

public:
    static MutexManager& getInstance() {
        static MutexManager instance;
        return instance;
    }

    void lock(const std::wstring& name) {
        std::recursive_mutex* mtxPtr = nullptr;
        {
            std::lock_guard<std::mutex> lock(m_mapMutex);
            if (m_mutexes.find(name) == m_mutexes.end()) {
                m_mutexes[name] = std::make_unique<std::recursive_mutex>();
            }
            mtxPtr = m_mutexes[name].get();
        }
        mtxPtr->lock();
    }

    void unlock(const std::wstring& name) {
        std::recursive_mutex* mtxPtr = nullptr;
        {
            std::lock_guard<std::mutex> lock(m_mapMutex);
            auto it = m_mutexes.find(name);
            if (it != m_mutexes.end()) {
                mtxPtr = it->second.get();
            }
        }
        if (mtxPtr) {
            mtxPtr->unlock();
        }
    }
};

// --- GESTIONAR THREAD-URI CU SUPORT ANULARE ---
class ThreadManager {
private:
    struct TaskInfo {
        std::future<vData> fut;
        std::shared_ptr<std::atomic<bool>> cancelFlag;
    };

    std::unordered_map<long long, TaskInfo> m_tasks;
    std::mutex m_mutex;
    std::atomic<long long> m_nextHandle{ 1 };

public:
    static ThreadManager& getInstance() {
        static ThreadManager instance;
        return instance;
    }

    long long spawnTask(std::function<vData()> task) {
        long long handle = m_nextHandle++;
        auto cancelFlag = std::make_shared<std::atomic<bool>>(false);

        std::lock_guard<std::mutex> lock(m_mutex);
        m_tasks[handle] = TaskInfo{
            std::async(std::launch::async, [task, cancelFlag]() {
                g_currentCancelFlag = cancelFlag;
                vData res = task();
                g_currentCancelFlag = nullptr;
                return res;
            }),
            cancelFlag
        };
        return handle;
    }

    bool isTaskRunning(long long handle) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_tasks.find(handle);
        if (it == m_tasks.end() || !it->second.fut.valid()) {
            return false;
        }
        return it->second.fut.wait_for(std::chrono::seconds(0)) == std::future_status::timeout;
    }

    void cancelTask(long long handle) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_tasks.find(handle);
        if (it != m_tasks.end() && it->second.cancelFlag) {
            it->second.cancelFlag->store(true);
        }
    }

    vData joinTask(long long handle) {
        std::future<vData> fut;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_tasks.find(handle);
            if (it == m_tasks.end()) return vData{ 0LL };
            fut = std::move(it->second.fut);
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

// --- ÎNREGISTRARE FUNCȚII NATIVE OLI ---
OLI_EXPORT void LoadOliPlugin(PluginRegistry& registry) {

    // 1. THREAD_SPAWN("func_name", args...)
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

    // 2. THREAD_WAIT(handle)
    registry[L"THREAD_WAIT"] = [](const std::vector<vData>& a) -> vData {
        if (a.empty()) return vData{ 0LL };
        long long handle = toInt(a[0]);
        return ThreadManager::getInstance().joinTask(handle);
    };

    // 3. THREAD_SLEEP(ms)
    registry[L"THREAD_SLEEP"] = [](const std::vector<vData>& a) -> vData {
        if (!a.empty()) {
            long long ms = toInt(a[0]);
            std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        }
        return vData{ 1LL };
    };

    // 4. MUTEX_LOCK("nume_mutex")
    registry[L"MUTEX_LOCK"] = [](const std::vector<vData>& a) -> vData {
        std::wstring name = (!a.empty() && std::holds_alternative<std::wstring>(a[0].value))
            ? std::get<std::wstring>(a[0].value)
            : L"default_mutex";

        MutexManager::getInstance().lock(name);
        return vData{ 1LL };
    };

    // 5. MUTEX_UNLOCK("nume_mutex")
    registry[L"MUTEX_UNLOCK"] = [](const std::vector<vData>& a) -> vData {
        std::wstring name = (!a.empty() && std::holds_alternative<std::wstring>(a[0].value))
            ? std::get<std::wstring>(a[0].value)
            : L"default_mutex";

        MutexManager::getInstance().unlock(name);
        return vData{ 1LL };
    };

    // 6. QUEUE_PUSH("nume_coada", valoare)
    registry[L"QUEUE_PUSH"] = [](const std::vector<vData>& a) -> vData {
        if (a.size() < 2) return vData{ 0LL };

        std::wstring qName = std::holds_alternative<std::wstring>(a[0].value)
            ? std::get<std::wstring>(a[0].value)
            : L"default_queue";

        QueueManager::getInstance().getQueue(qName)->push(a[1]);
        return vData{ 1LL };
    };

    // 7. QUEUE_POP("nume_coada")
    registry[L"QUEUE_POP"] = [](const std::vector<vData>& a) -> vData {
        std::wstring qName = (!a.empty() && std::holds_alternative<std::wstring>(a[0].value))
            ? std::get<std::wstring>(a[0].value)
            : L"default_queue";

        return QueueManager::getInstance().getQueue(qName)->pop();
    };

    // 8. QUEUE_SIZE("nume_coada")
    registry[L"QUEUE_SIZE"] = [](const std::vector<vData>& a) -> vData {
        std::wstring qName = (!a.empty() && std::holds_alternative<std::wstring>(a[0].value))
            ? std::get<std::wstring>(a[0].value)
            : L"default_queue";

        size_t sz = QueueManager::getInstance().getQueue(qName)->size();
        return vData{ static_cast<long long>(sz) };
    };

    // 9. QUEUE_POP_TIMEOUT("nume_coada", ms_timeout)
    registry[L"QUEUE_POP_TIMEOUT"] = [](const std::vector<vData>& a) -> vData {
        if (a.empty()) return vData{ std::monostate{} };

        std::wstring qName = std::holds_alternative<std::wstring>(a[0].value)
            ? std::get<std::wstring>(a[0].value)
            : L"default_queue";

        long long ms = (a.size() >= 2) ? toInt(a[1]) : 1000;

        vData result;
        if (QueueManager::getInstance().getQueue(qName)->popTimeout(ms, result)) {
            return result;
        }

        return vData{ std::monostate{} };
    };

    // 10. IS_THREAD_RUNNING(handle)
    registry[L"IS_THREAD_RUNNING"] = [](const std::vector<vData>& a) -> vData {
        if (a.empty()) return vData{ 0LL };
        long long handle = toInt(a[0]);
        bool running = ThreadManager::getInstance().isTaskRunning(handle);
        return vData{ running ? 1LL : 0LL };
    };

    // 11. THREAD_CANCEL(handle) -> Semnalează anularea task-ului
    registry[L"THREAD_CANCEL"] = [](const std::vector<vData>& a) -> vData {
        if (a.empty()) return vData{ 0LL };
        long long handle = toInt(a[0]);
        ThreadManager::getInstance().cancelTask(handle);
        return vData{ 1LL };
    };

    // 12. IS_THREAD_CANCELLED() -> Apelat din interiorul thread-ului pentru a verifica dacă s-a cerut oprirea
    registry[L"IS_THREAD_CANCELLED"] = [](const std::vector<vData>&) -> vData {
        if (g_currentCancelFlag && g_currentCancelFlag->load()) {
            return vData{ 1LL };
        }
        return vData{ 0LL };
    };
}