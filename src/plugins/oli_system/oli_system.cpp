#include "../../OliEngine.hpp"
#include "../../ConsoleManager.hpp"
#include "../../StringUtils.hpp"
#include <thread>
#include <iostream>
#include <cstdio>
#include <cstring>
#include <vector>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <cstdlib>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#endif

#if defined(_WIN32) || defined(_WIN64)
#define OLI_EXPORT extern "C" __declspec(dllexport)
#else
#define OLI_EXPORT extern "C" __attribute__((visibility("default")))
#endif

using PluginRegistry = std::unordered_map<std::wstring, OliFunctionHandler>;

// --- Helper pentru conversii ---
inline std::wstring vDataToWString(const vData& v) {
    return v.toWString(); // Presupunând că vData are această metodă
}

void RegisterSystemFunctions(PluginRegistry& registry) {

    // --- EXEC: Blocant (Așteaptă finalizarea, bun pentru compilare) ---
    registry[L"SYS"] = [](const std::vector<vData>& args) -> vData {
        if (args.empty()) return vData(L"");
        std::wstring command = args[0].toWString();

        std::wstring output;

#if defined(_WIN32) || defined(_WIN64)
        FILE* pipe = _wpopen(command.c_str(), L"r");
        if (!pipe) return vData(L"ERR_PIPE_FAILED");

        char buffer[128];
        while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            output += std::wstring(buffer, buffer + strlen(buffer));
        }

        _pclose(pipe);
#else
        std::string commandUtf8 = wstring_to_utf8(command);
        FILE* pipe = popen(commandUtf8.c_str(), "r");
        if (!pipe) return vData(L"ERR_PIPE_FAILED");

        char buffer[128];
        while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            output += utf8_to_wstring(std::string(buffer));
        }

        pclose(pipe);
#endif
        return vData(output); // Returnăm tot output-ul ca string
    };

    // --- START: Non-blocant (Lansează proces separat, bun pentru .exe-uri) ---
    /*
    registry[L"START_PROCESS"] = [](const std::vector<vData>& args) -> vData {
        if (args.empty()) return vData(0LL);
        std::wstring path = vDataToWString(args[0]);

#if defined(_WIN32) || defined(_WIN64)
        // ShellExecute returnează imediat, nu blochează UI-ul
        HINSTANCE hInst = ShellExecuteW(NULL, L"open", path.c_str(), NULL, NULL, SW_SHOWNORMAL);
        return vData((long long)((uintptr_t)hInst > 32));
#else
        // Implementare simplă pentru Unix
        return vData((long long)(system((std::string(path.begin(), path.end()) + " &").c_str()) == 0));
#endif
        };
        */

    registry[L"START_PROCESS"] = [](const std::vector<vData>& args) -> vData {
        if (args.empty()) {
            LOG_ERROR(L"START_PROCESS Error: Lipseste calea catre executabil.");
            return vData(0LL);
        }

        std::wstring path = vDataToWString(args[0]);
        std::string commandUtf8 = wstring_to_utf8(path);

#if defined(_WIN32) || defined(_WIN64)
        STARTUPINFOW si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));

        LOG_DEBUG(L"START_PROCESS: Path primit: " + path);

        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_SHOWNORMAL;

        std::vector<wchar_t> cmdLine(path.begin(), path.end());
        cmdLine.push_back(0);

        BOOL success = CreateProcessW(
            NULL,
            cmdLine.data(),
            NULL,
            NULL,
            FALSE,
            CREATE_NEW_CONSOLE,
            NULL,
            NULL,
            &si,
            &pi
        );

        if (success) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            LOG_DEBUG(L"START_PROCESS a pornit cu succes: " + path);
            return vData(1LL);
        }
        else {
            LOG_ERROR(L"START_PROCESS NU a reusit sa porneasca executabilul: " + path + L" (Cod eroare: " + std::to_wstring(GetLastError()) + L")");
            return vData(0LL);
        }
#else
        pid_t pid = fork();
        if (pid < 0) {
            LOG_ERROR(L"START_PROCESS Error: fork() a esuat.");
            return vData(0LL);
        }
        if (pid == 0) {
            setsid();
            execl("/bin/sh", "sh", "-c", commandUtf8.c_str(), (char*)NULL);
            _exit(EXIT_FAILURE);
        }
        return vData(1LL);
#endif
    };

    // --- THREAD_RUN: Execuție asincronă pentru logică internă (dacă ai nevoie) ---
    registry[L"THREAD_RUN"] = [](const std::vector<vData>& args) -> vData {
        if (args.empty()) return vData(0LL);

        std::wstring cmd = vDataToWString(args[0]);
        std::string cmdUtf8 = wstring_to_utf8(cmd);

#if defined(_WIN32) || defined(_WIN64)
        std::thread([cmd]() {
            _wsystem(cmd.c_str());
        }).detach();
#else
        std::thread([cmdUtf8]() {
            int result = system(cmdUtf8.c_str());
            (void)result;
        }).detach();
#endif

        return vData(1LL);
    };
	registry[L"SYS_WAIT"] = [](const std::vector<vData>& args) -> vData {
		if (args.empty()) return vData(L"ERR_NO_ARGS");
		
		std::wstring cmd = args[0].toWString();
		ConsoleManager::getInstance().log(L"[DEBUG] SYS_WAIT execută direct: " + cmd);

#if defined(_WIN32) || defined(_WIN64)
		HANDLE hRead, hWrite;
		SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
		if (!CreatePipe(&hRead, &hWrite, &sa, 0)) return vData(L"ERR_PIPE");

		STARTUPINFOW si;
		PROCESS_INFORMATION pi;
		ZeroMemory(&si, sizeof(si));
		si.cb = sizeof(si);
		si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
		si.hStdOutput = hWrite; 
		si.hStdError = hWrite;  
		si.wShowWindow = SW_HIDE; 
		ZeroMemory(&pi, sizeof(pi));

		std::vector<wchar_t> cmdLine(cmd.begin(), cmd.end());
		cmdLine.push_back(0);

		BOOL success = CreateProcessW(
			NULL, cmdLine.data(), NULL, NULL, TRUE, 
			DETACHED_PROCESS | CREATE_NO_WINDOW, 
			NULL, NULL, &si, &pi
		);
		
		CloseHandle(hWrite);

		if (!success) {
			CloseHandle(hRead);
			return vData(L"ERR_LAUNCH");
		}

		char buffer[256];
		DWORD bytesRead;
		MSG msg;

		while (true) {
			DWORD exitCode;
			if (GetExitCodeProcess(pi.hProcess, &exitCode) && exitCode != STILL_ACTIVE) break;

			if (PeekNamedPipe(hRead, NULL, 0, NULL, &bytesRead, NULL) && bytesRead > 0) {
				if (ReadFile(hRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL)) {
					buffer[bytesRead] = 0;
					ConsoleManager::getInstance().log(utf8_to_wstring(std::string(buffer)), LogLevel::INFO);
				}
			}

			if (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
			Sleep(10); 
		}

		while (ReadFile(hRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
			buffer[bytesRead] = 0;
			ConsoleManager::getInstance().log(utf8_to_wstring(std::string(buffer)), LogLevel::INFO);
		}

		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
		CloseHandle(hRead);

		return vData(L"DONE"); 
#else
		int pipefd[2];
		if (pipe(pipefd) == -1) {
			return vData(L"ERR_PIPE");
		}

		pid_t pid = fork();
		if (pid < 0) {
			close(pipefd[0]);
			close(pipefd[1]);
			return vData(L"ERR_FORK");
		}

		std::string cmdUtf8 = wstring_to_utf8(cmd);

		if (pid == 0) {
			close(pipefd[0]);
			dup2(pipefd[1], STDOUT_FILENO);
			dup2(pipefd[1], STDERR_FILENO);
			close(pipefd[1]);
			execl("/bin/sh", "sh", "-c", cmdUtf8.c_str(), (char*)NULL);
			_exit(EXIT_FAILURE);
		}

		close(pipefd[1]);

		char buffer[256];
		ssize_t bytesRead;

		while ((bytesRead = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
			buffer[bytesRead] = '\0';
			ConsoleManager::getInstance().log(utf8_to_wstring(std::string(buffer)), LogLevel::INFO);
		}

		int status = 0;
		waitpid(pid, &status, 0);
		close(pipefd[0]);

		return vData(L"DONE");
#endif
	};

	registry[L"OPEN_TERMINAL"] = []([[maybe_unused]] const std::vector<vData>& args) -> vData {
#if defined(_WIN32) || defined(_WIN64)
		// Aici folosim ShellExecute, care este mult mai prietenos cu Windows
		// și deschide automat o fereastră vizibilă.
		ShellExecuteW(NULL, L"open", L"cmd.exe", NULL, NULL, SW_SHOWNORMAL);
		return vData(1LL);
#else
		pid_t pid = fork();
		if (pid < 0) return vData(0LL);
		if (pid == 0) {
			if (execlp("x-terminal-emulator", "x-terminal-emulator", "-e", "sh", (char*)NULL) == -1) {
				execlp("gnome-terminal", "gnome-terminal", "--", "sh", (char*)NULL);
			}
			_exit(EXIT_FAILURE);
		}
		return vData(1LL);
#endif
		};
}

OLI_EXPORT void LoadOliPlugin(PluginRegistry& registry) {
    RegisterSystemFunctions(registry);
}

OLI_EXPORT void SetPluginConsoleManager(ConsoleManager* hostCm) {
    if (hostCm != nullptr) {
        ConsoleManager::setInstance(hostCm);
    }
}