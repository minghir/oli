#include "../../OliEngine.hpp"
#include "../../IOliEngine.hpp"

#if defined(_WIN32) || defined(_WIN64)
#define OLI_EXPORT extern "C" __declspec(dllexport)
#include <windows.h>
#else
#define OLI_EXPORT extern "C" __attribute__((visibility("default")))
#include <unistd.h>
#endif

#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>
#include <variant>

#include <cuda.h>
#include <nvrtc.h>

using PluginRegistry = std::unordered_map<std::wstring, OliFunctionHandler>;

inline vOliEngine* g_LinkedOliEngine = nullptr;
static bool g_HasConsoleManager = false;

inline std::string toNarrowString(const std::wstring& wstr) {
    return std::string(wstr.begin(), wstr.end());
}

OLI_EXPORT void SetPluginConsoleManager(ConsoleManager* hostCm) {
    if (hostCm != nullptr) {
        ConsoleManager::setInstance(hostCm);
        g_HasConsoleManager = true;
    }
}

OLI_EXPORT void setConsoleFn([[maybe_unused]] void* fn) {}

inline void SafeLogError(const std::wstring& msg) {
    if (g_HasConsoleManager) {
        LOG_ERROR(msg);
    }
    else {
        std::wcout << L"[ERROR] " << msg << std::endl;
    }
}

inline void SafeLogSuccess(const std::wstring& msg) {
    if (g_HasConsoleManager) {
        LOG_SUCCESS(msg);
    }
    else {
        std::wcout << L"[SUCCESS] " << msg << std::endl;
    }
}

struct OliCudaKernel {
    CUmodule module = nullptr;
    CUfunction function = nullptr;
};

static std::unordered_map<long long, OliCudaKernel> g_Kernels;
static long long g_NextKernelId = 1;
static CUcontext g_CudaContext = nullptr;
static CUdevice g_CudaDevice = 0;

// Helper intern: Se asigură că contextul CUDA este activ pe thread-ul curent
inline bool EnsureCudaContext() {
    if (!g_CudaContext) return false;
    return (cuCtxSetCurrent(g_CudaContext) == CUDA_SUCCESS);
}

OLI_EXPORT void LoadOliPlugin(PluginRegistry& registry, void* enginePtr) {

    if (g_LinkedOliEngine == nullptr && enginePtr != nullptr) {
        g_LinkedOliEngine = static_cast<vOliEngine*>(enginePtr);
        SafeLogSuccess(L"✅ [oli_cuda] Conexiune stabilă stabilită cu OliEngine!");
    }

    registry[L"CUDA_INIT"] = [](const std::vector<vData>& args) -> vData {
        int devId = args.empty() ? 0 : static_cast<int>(args[0].toInt());

        if (cuInit(0) != CUDA_SUCCESS) return vData{ 0LL };
        if (cuDeviceGet(&g_CudaDevice, devId) != CUDA_SUCCESS) return vData{ 0LL };

        if (cuDevicePrimaryCtxRetain(&g_CudaContext, g_CudaDevice) != CUDA_SUCCESS) return vData{ 0LL };
        if (cuCtxSetCurrent(g_CudaContext) != CUDA_SUCCESS) return vData{ 0LL };

        return vData{ 1LL };
        };

    registry[L"CUDA_MALLOC"] = [](const std::vector<vData>& args) -> vData {
        if (!EnsureCudaContext() || args.empty()) return vData{ 0LL };
        size_t bytes = static_cast<size_t>(args[0].toInt());

        CUdeviceptr d_ptr = 0;
        if (cuMemAlloc(&d_ptr, bytes) != CUDA_SUCCESS) return vData{ 0LL };

        return vData{ static_cast<long long>(d_ptr) };
        };

    registry[L"CUDA_FREE"] = [](const std::vector<vData>& args) -> vData {
        if (!EnsureCudaContext() || args.empty()) return vData{ 0LL };
        CUdeviceptr d_ptr = static_cast<CUdeviceptr>(args[0].toInt());

        if (d_ptr) {
            cuMemFree(d_ptr);
        }
        return vData{ 1LL };
        };

    registry[L"CUDA_MEMCPY_H2D"] = [](const std::vector<vData>& args) -> vData {
        if (!EnsureCudaContext() || args.size() < 2) return vData{ 0LL };
        CUdeviceptr d_dst = static_cast<CUdeviceptr>(args[0].toInt());

        std::vector<long long> hostBuffer;

        // Dacă al 2-lea argument este un array Oli
        if (args[1].isArray()) {
            auto arr = args[1].rawArray();
            for (const auto& item : *arr) {
                hostBuffer.push_back(item.toInt());
            }
        }
        else {
            for (size_t i = 1; i < args.size(); ++i) {
                hostBuffer.push_back(args[i].toInt());
            }
        }

        if (hostBuffer.empty()) return vData{ 0LL };

        size_t bytes = hostBuffer.size() * sizeof(long long);
        CUresult res = cuMemcpyHtoD(d_dst, hostBuffer.data(), bytes);
        return vData{ (res == CUDA_SUCCESS) ? 1LL : 0LL };
        };

    registry[L"CUDA_MEMCPY_D2H"] = [](const std::vector<vData>& args) -> vData {
        if (!EnsureCudaContext() || args.size() < 2) return vData{ 0LL };
        CUdeviceptr d_src = static_cast<CUdeviceptr>(args[0].toInt());
        size_t count = static_cast<size_t>(args[1].toInt());

        if (!d_src || count == 0) return vData{ 0LL };

        std::vector<long long> hostBuffer(count, 0);
        size_t bytes = count * sizeof(long long);

        if (cuMemcpyDtoH(hostBuffer.data(), d_src, bytes) != CUDA_SUCCESS) return vData{ 0LL };

        auto resultArray = std::make_shared<std::vector<vData>>();
        resultArray->reserve(count);
        for (long long val : hostBuffer) {
            resultArray->push_back(vData{ val });
        }

        return vData{ resultArray };
        };

    registry[L"CUDA_COMPILE_KERNEL"] = [](const std::vector<vData>& args) -> vData {
        if (!EnsureCudaContext() || args.size() < 2) return vData{ 0LL };

        std::string code = toNarrowString(args[0].toWString());
        std::string funcName = toNarrowString(args[1].toWString());

        if (code.empty() || funcName.empty()) {
            SafeLogError(L"[CUDA_COMPILE_KERNEL] Sursa sau numele functiei sunt goale.");
            return vData{ 0LL };
        }

        nvrtcProgram prog;
        nvrtcResult res = nvrtcCreateProgram(&prog, code.c_str(), "oli_kernel.cu", 0, NULL, NULL);
        if (res != NVRTC_SUCCESS) {
            SafeLogError(L"[NVRTC] Failed to create program.");
            return vData{ 0LL };
        }

        int major = 6, minor = 0;
        cuDeviceGetAttribute(&major, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR, g_CudaDevice);
        cuDeviceGetAttribute(&minor, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR, g_CudaDevice);
        std::string archOption = "--gpu-architecture=compute_" + std::to_string(major) + std::to_string(minor);
        const char* opts[] = { archOption.c_str() };

        nvrtcResult compileRes = nvrtcCompileProgram(prog, 1, opts);
        if (compileRes != NVRTC_SUCCESS) {
            size_t logSize = 0;
            nvrtcGetProgramLogSize(prog, &logSize);
            std::vector<char> log(logSize + 1, 0);
            nvrtcGetProgramLog(prog, log.data());

            std::string logStr(log.data());
            std::wstring wLog(logStr.begin(), logStr.end());
            SafeLogError(L"[NVRTC JIT COMPILATION ERROR]:\n" + wLog);

            nvrtcDestroyProgram(&prog);
            return vData{ 0LL };
        }

        size_t ptxSize = 0;
        nvrtcGetPTXSize(prog, &ptxSize);
        std::vector<char> ptx(ptxSize + 1, 0);
        nvrtcGetPTX(prog, ptx.data());
        nvrtcDestroyProgram(&prog);

        OliCudaKernel k;
        CUresult modRes = cuModuleLoadData(&k.module, ptx.data());
        if (modRes != CUDA_SUCCESS) {
            SafeLogError(L"[CUDA Driver] cuModuleLoadData failed with code: " + std::to_wstring(modRes));
            return vData{ 0LL };
        }

        CUresult funcRes = cuModuleGetFunction(&k.function, k.module, funcName.c_str());
        if (funcRes != CUDA_SUCCESS) {
            SafeLogError(L"[CUDA Driver] cuModuleGetFunction failed with code: " + std::to_wstring(funcRes));
            return vData{ 0LL };
        }

        long long id = g_NextKernelId++;
        g_Kernels[id] = k;

        return vData{ id };
        };

    registry[L"CUDA_RUN_KERNEL"] = [](const std::vector<vData>& args) -> vData {
        if (!EnsureCudaContext() || args.size() < 3) return vData{ 0LL };

        long long kernelId = args[0].toInt();
        unsigned int gridX = static_cast<unsigned int>(args[1].toInt());
        unsigned int blockX = static_cast<unsigned int>(args[2].toInt());

        if (g_Kernels.find(kernelId) == g_Kernels.end()) return vData{ 0LL };

        OliCudaKernel& k = g_Kernels[kernelId];

        std::vector<uint64_t> rawArgs;
        std::vector<void*> kernelParams;

        for (size_t i = 3; i < args.size(); ++i) {
            rawArgs.push_back(static_cast<uint64_t>(args[i].toInt()));
        }

        for (size_t i = 0; i < rawArgs.size(); ++i) {
            kernelParams.push_back(&rawArgs[i]);
        }

        CUresult launchRes = cuLaunchKernel(
            k.function,
            gridX, 1, 1,
            blockX, 1, 1,
            0, NULL,
            kernelParams.data(),
            NULL
        );

        if (launchRes != CUDA_SUCCESS) {
            SafeLogError(L"[CUDA Driver] cuLaunchKernel failed with code: " + std::to_wstring(launchRes));
            return vData{ 0LL };
        }

        cuCtxSynchronize();
        return vData{ 1LL };
        };
}