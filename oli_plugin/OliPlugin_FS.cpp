#include "../src/OliEngine.hpp" 

#include <memory>
#include <mutex>
#include <filesystem>


void RegisterFileSystemFunctions(std::unordered_map<std::wstring, std::function<vData(const std::vector<vData>&)>>& registry) {

    registry[L"FS_LIST_DIR"] = [](const std::vector<vData>& args) -> vData {
        std::wstring path = std::get<std::wstring>(args[0].value);

        vDataArray items;

        for (auto& entry : std::filesystem::directory_iterator(path)) {
            vData item;
            item.value = entry.path().filename().wstring();
            items->push_back(item);
        }

        vData result;
        result.value = items;
        return result;
    };

    registry[L"FS_LIST_FILES"] = [](const std::vector<vData>& args) -> vData {
        vData result;
        try {
            std::wstring path = std::get<std::wstring>(args[0].value);

            vDataArray files;

            for (auto& entry : std::filesystem::directory_iterator(path)) {
                if (entry.is_regular_file()) {
                    vData f;
                    f.value = entry.path().filename().wstring();
                    files->push_back(f);
                }
            }

           
            result.value = files;
        }
        catch (...) {
        }
        return result;
    };

    registry[L"FS_FILE_EXISTS"] = [](const std::vector<vData>& args) -> vData {
        if (args.empty() || !std::holds_alternative<std::wstring>(args[0].value)) {
            return vData{ false }; // argument invalid
        }

        std::wstring path = std::get<std::wstring>(args[0].value);

        bool exists = std::filesystem::exists(path) &&
            std::filesystem::is_regular_file(path);

        vData result;
        result.value = exists;
        return result;
    };

    registry[L"FS_FILE_SIZE"] = [](const std::vector<vData>& args) -> vData {
        // Validare argument
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
        catch (...) {
            // orice eroare → returnăm -1
        }

        return vData{ -1LL };
    };

    registry[L"FS_IS_FILE"] = [](const std::vector<vData>& args) -> vData {
        if (args.empty() || !std::holds_alternative<std::wstring>(args[0].value)) {
            return vData{ false };
        }

        std::wstring path = std::get<std::wstring>(args[0].value);

        bool isFile = false;
        try {
            isFile = std::filesystem::exists(path) &&
                std::filesystem::is_regular_file(path);
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
            isDir = std::filesystem::exists(path) &&
                std::filesystem::is_directory(path);
        }
        catch (...) {}

        vData result;
        result.value = isDir;
        return result;
    };

}
