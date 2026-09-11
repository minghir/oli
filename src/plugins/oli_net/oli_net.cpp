#if defined(_WIN32) || defined(_WIN64)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define OLI_EXPORT extern "C" __declspec(dllexport)
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

using socket_t = SOCKET;
using ssize_t = SSIZE_T;
#define IS_INVALID_SOCKET(s) ((s) == INVALID_SOCKET)
#define CLOSE_SOCKET(s) closesocket(s)
#else
#define OLI_EXPORT extern "C" __attribute__((visibility("default")))
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>

using socket_t = int;
#define INVALID_SOCKET (-1)
#define IS_INVALID_SOCKET(s) ((s) < 0)
#define CLOSE_SOCKET(s) ::close(s)
#endif

#include "../../OliEngine.hpp"
#include "../../StringUtils.hpp"

using PluginRegistry = std::unordered_map<std::wstring, OliFunctionHandler>;

// Helper setare socket non-blocant
static bool set_nonblocking(socket_t sock) {
#ifdef _WIN32
    u_long mode = 1;
    return ioctlsocket(sock, FIONBIO, &mode) == 0;
#else
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags == -1) return false;
    return fcntl(sock, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
}

/*
// Helpers conversie std::wstring <-> std::string
static std::string wstr_to_str(const std::wstring& wstr) {
    return std::string(wstr.begin(), wstr.end());
}

static std::wstring str_to_wstr(const std::string& str) {
    return std::wstring(str.begin(), str.end());
}
*/
void RegisterNetFunctions(PluginRegistry& registry) {
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

    // 1. NET_BIND(port) -> Creează un socket UDP non-blocant pe portul specificat
    registry[L"NET_BIND"] = [](const std::vector<vData>& args) -> vData {
        if (args.empty()) return { -1LL };

        int port = static_cast<int>(args[0].toInt());
        socket_t sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (IS_INVALID_SOCKET(sock)) return { -1LL };

        int opt = 1;
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

        if (!set_nonblocking(sock)) {
            CLOSE_SOCKET(sock);
            return { -1LL };
        }

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(static_cast<uint16_t>(port));
        addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            CLOSE_SOCKET(sock);
            return { -1LL };
        }

        return { static_cast<long long>(sock) };
        };

    // 2. NET_SEND_TO(sock, ip, port, message) -> Trimite pachet UDP
    registry[L"NET_SEND_TO"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 4) return { 0LL };

        socket_t sock = static_cast<socket_t>(args[0].toInt());
        std::string ip = wstr_to_str(args[1].toWString());
        int port = static_cast<int>(args[2].toInt());
        std::string msg = wstr_to_str(args[3].toWString());

        sockaddr_in dest{};
        dest.sin_family = AF_INET;
        dest.sin_port = htons(static_cast<uint16_t>(port));
        inet_pton(AF_INET, ip.c_str(), &dest.sin_addr);

        ssize_t sent = sendto(sock, msg.c_str(), msg.length(), 0, (struct sockaddr*)&dest, sizeof(dest));
        return { static_cast<long long>(sent > 0 ? sent : 0) };
        };

    // 3. NET_RECV_FROM(sock) -> Returnează un Map {"data": msg, "ip": sender_ip, "port": sender_port}
    registry[L"NET_RECV_FROM"] = [](const std::vector<vData>& args) -> vData {
        if (args.empty()) return vData::CreateMap();

        socket_t sock = static_cast<socket_t>(args[0].toInt());
        char buffer[2048];

        sockaddr_in from_addr{};
        socklen_t from_len = sizeof(from_addr);

        ssize_t bytes = recvfrom(sock, buffer, sizeof(buffer) - 1, 0, (struct sockaddr*)&from_addr, &from_len);

        if (bytes > 0) {
            buffer[bytes] = '\0';
            char ip_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(from_addr.sin_addr), ip_str, INET_ADDRSTRLEN);

            vData resMap = vData::CreateMap();
            auto mapPtr = resMap.rawMap();
            if (mapPtr) {
                (*mapPtr)[L"data"] = vData{ str_to_wstr(std::string(buffer, bytes)) };
                (*mapPtr)[L"ip"] = vData{ str_to_wstr(std::string(ip_str)) };
                (*mapPtr)[L"port"] = vData{ static_cast<long long>(ntohs(from_addr.sin_port)) };
            }
            return resMap;
        }

        return vData::CreateMap();
        };

    // 4. NET_CLOSE(sock) -> Închide socket-ul
    registry[L"NET_CLOSE"] = [](const std::vector<vData>& args) -> vData {
        if (args.empty()) return { 0LL };
        socket_t sock = static_cast<socket_t>(args[0].toInt());
        if (!IS_INVALID_SOCKET(sock)) {
            CLOSE_SOCKET(sock);
            return { 1LL };
        }
        return { 0LL };
        };
}

OLI_EXPORT void LoadOliPlugin(PluginRegistry& registry) {
    RegisterNetFunctions(registry);
}