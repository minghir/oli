#include "../../OliEngine.hpp" 
//#include "../../ConsoleManager.hpp" 

#if defined(_WIN32) || defined(_WIN64)
#define OLI_EXPORT extern "C" __declspec(dllexport)
#include <windows.h>
#else
#define OLI_EXPORT extern "C" __attribute__((visibility("default")))
#include <unistd.h>
#endif

#include <memory>
#include <mutex>
#include <filesystem>

using PluginRegistry = std::unordered_map<std::wstring, OliFunctionHandler>;

void RegisterFileSystemFunctions(std::unordered_map<std::wstring, std::function<vData(const std::vector<vData>&)>>& registry) {

    // =================================================================
    // 🔥 REPARAT: FS_LIST_DIR (Alocare + Protecție Permisiuni Windows)
    // =================================================================
    registry[L"FS_LIST_DIR"] = [](const std::vector<vData>& args) -> vData {
        // Inițializăm pointerul/smart-pointerul pentru a nu fi NULL
        auto items = std::make_shared<std::vector<vData>>();
        vData result;

        if (args.empty() || !std::holds_alternative<std::wstring>(args[0].value)) {
            result.value = items;
            return result;
        }

        try {
            std::wstring path = std::get<std::wstring>(args[0].value);

            // 🔥 CRITIC: skip_permission_denied evită crash-ul pe "System Volume Information"
            auto options = std::filesystem::directory_options::skip_permission_denied;

            for (auto& entry : std::filesystem::directory_iterator(path, options)) {
                vData item;
                item.value = entry.path().filename().wstring();
                items->push_back(item); // Acum items NU mai este null, merge brici!
            }
        }
        catch (...) {
            // Dacă apare orice altă eroare de I/O, returnăm ce am colectat până acum
        }

        result.value = items;
        return result;
        };

    // =================================================================
    // 🔥 REPARAT: FS_LIST_FILES (Alocare + Protecție Permisiuni Windows)
    // =================================================================
    registry[L"FS_LIST_FILES"] = [](const std::vector<vData>& args) -> vData {
        auto files = std::make_shared<std::vector<vData>>();
        vData result;

        if (args.empty() || !std::holds_alternative<std::wstring>(args[0].value)) {
            result.value = files;
            return result;
        }

        try {
            std::wstring path = std::get<std::wstring>(args[0].value);
            auto options = std::filesystem::directory_options::skip_permission_denied;

            for (auto& entry : std::filesystem::directory_iterator(path, options)) {
                if (entry.is_regular_file()) {
                    vData f;
                    f.value = entry.path().filename().wstring();
                    files->push_back(f);
                }
            }
        }
        catch (...) {
        }

        result.value = files;
        return result;
        };

    registry[L"FS_FILE_EXISTS"] = [](const std::vector<vData>& args) -> vData {
        if (args.empty() || !std::holds_alternative<std::wstring>(args[0].value)) {
            return vData{ false };
        }
        std::wstring path = std::get<std::wstring>(args[0].value);
        bool exists = std::filesystem::exists(path) && std::filesystem::is_regular_file(path);
        vData result;
        result.value = exists;
        return result;
        };

    registry[L"FS_FILE_SIZE"] = [](const std::vector<vData>& args) -> vData {
        if (args.empty() || !std::holds_alternative<std::wstring>(args[0].value)) {
            return vData{ -1LL };
        }
        std::wstring path = std::get<std::wstring>(args[0].value);
        try {
            if (std::filesystem::exists(path) && std::filesystem::is_regular_file(path)) {
                auto size = std::filesystem::file_size(path);
                vData result;
                result.value = static_cast<long long>(size);
                return result;
            }
        }
        catch (...) {}
        return vData{ -1LL };
        };

    registry[L"FS_IS_FILE"] = [](const std::vector<vData>& args) -> vData {
        if (args.empty() || !std::holds_alternative<std::wstring>(args[0].value)) {
            return vData{ false };
        }
        std::wstring path = std::get<std::wstring>(args[0].value);
        bool isFile = false;
        try {
            isFile = std::filesystem::exists(path) && std::filesystem::is_regular_file(path);
        }
        catch (...) {}
        vData result;
        result.value = isFile;
        return result;
        };

    registry[L"FS_IS_DIR"] = [](const std::vector<vData>& args) -> vData {
        if (args.empty() || !std::holds_alternative<std::wstring>(args[0].value)) {
            return vData{ false };
        }
        std::wstring path = std::get<std::wstring>(args[0].value);
        bool isDir = false;
        try {
            isDir = std::filesystem::exists(path) && std::filesystem::is_directory(path);
        }
        catch (...) {}
        vData result;
        result.value = isDir;
        return result;
        };

    registry[L"FS_GET_FILE_STEM"] = [](const std::vector<vData>& args) -> vData {
        if (args.empty() || !std::holds_alternative<std::wstring>(args[0].value)) {
            return vData{ L"" };
        }
        std::wstring fullPath = std::get<std::wstring>(args[0].value);
        vData result;
        try {
            result.value = std::filesystem::path(fullPath).stem().wstring();
        }
        catch (...) { result.value = L""; }
        return result;
        };

    registry[L"FS_GET_FILENAME"] = [](const std::vector<vData>& args) -> vData {
        if (args.empty() || !std::holds_alternative<std::wstring>(args[0].value)) {
            return vData{ L"" };
        }
        std::wstring fullPath = std::get<std::wstring>(args[0].value);
        vData result;
        try {
            result.value = std::filesystem::path(fullPath).filename().wstring();
        }
        catch (...) { result.value = L""; }
        return result;
        };
}

OLI_EXPORT void LoadOliPlugin(PluginRegistry& registry) {
    RegisterFileSystemFunctions(registry);
}