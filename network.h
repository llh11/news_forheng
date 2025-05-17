#ifndef NETWORK_H
#define NETWORK_H

#include <string>
#include <vector>
#include <functional> // For std::function (callbacks)
#include <map>        // For std::map (was missing, caused C2039)

// 引入 WinSock2.h 之前必须先引入 windows.h
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h> // For getaddrinfo etc.

// 链接 Ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

namespace Network {

    /**
     * @brief 初始化 Winsock 库。
     * @return 如果成功返回 true，否则返回 false。
     * @note 应用程序启动时应调用一次。
     */
    bool Initialize();

    /**
     * @brief 清理 Winsock 库。
     * @note 应用程序退出时应调用一次。
     */
    void Cleanup();

    // 辅助结构体，用于解析 URL
    struct ParsedUrl {
        std::string scheme;
        std::string host;
        unsigned short port;
        std::string path; // Should include leading slash if present
        std::string query; // Content after '?'
        bool isValid = false;

        ParsedUrl() : port(0), isValid(false) {} // Default constructor
    };

    /**
     * @brief 解析 URL 字符串。
     * @param urlString 要解析的 URL。
     * @return ParsedUrl 结构体。
     */
    ParsedUrl ParseUrl(const std::string& urlString);


    /**
     * @brief 执行 HTTP GET 请求。
     * @param host 主机名 (例如 "www.example.com")。
     * @param path 请求路径 (例如 "/index.html")。
     * @param port 端口号 (通常是 80 for HTTP, 443 for HTTPS)。
     * @param responseBody [out] 存储服务器响应的主体内容。
     * @param responseHeadersOut [out] 存储服务器响应的头部信息 (可选, C2065 was here, renamed parameter)。
     * @param useHTTPS 是否使用 HTTPS (当前实现仅支持简单 HTTP)。
     * @param timeoutMs 超时时间（毫秒） (C2065 was here, renamed parameter).
     * @return 如果请求成功且收到 2xx 响应则返回 true。
     *
     * @note 这是一个非常基础的 HTTP GET 实现，不处理重定向、HTTPS (需要额外库如 OpenSSL)、
     * 复杂的头部、Cookies 等。对于生产环境，建议使用成熟的 HTTP 客户端库 (如 cpr, libcurl, cpprestsdk)。
     */
    bool HttpGet(
        const std::string& host,
        const std::string& path,
        unsigned short port,
        std::string& responseBody,
        std::map<std::string, std::string>* responseHeadersOutParam = nullptr, // Renamed to avoid conflict
        bool useHTTPSParam = false, // Renamed
        int timeoutMsParam = 5000  // Renamed
    );

    /**
     * @brief 下载文件到指定路径。
     * @param url 文件的 URL。
     * @param outputPath 文件保存的本地路径。
     * @param progressCallback 进度回调函数 (可选)，参数为 (当前已下载字节数, 文件总字节数)。
     * 如果总字节数未知，则为 -1。
     * @return 如果下载成功返回 true。
     * @note 此函数将解析 URL，并使用 HttpGet (或类似逻辑) 来获取数据。
     * 同样，这是一个基础实现。
     */
    bool DownloadFile(
        const std::string& url, // Expects std::string
        const std::wstring& outputPath,
        std::function<void(long long, long long)> progressCallback = nullptr
    );

} // namespace Network

#endif // NETWORK_H
