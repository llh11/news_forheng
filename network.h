#ifndef NETWORK_H
#define NETWORK_H

#include <string>
#include <vector>
#include <functional> // For std::function (callbacks)
#include <map>        // For std::map (was missing, caused C2039)

// ���� WinSock2.h ֮ǰ���������� windows.h
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h> // For getaddrinfo etc.

// ���� Ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

namespace Network {

    /**
     * @brief ��ʼ�� Winsock �⡣
     * @return ����ɹ����� true�����򷵻� false��
     * @note Ӧ�ó�������ʱӦ����һ�Ρ�
     */
    bool Initialize();

    /**
     * @brief ���� Winsock �⡣
     * @note Ӧ�ó����˳�ʱӦ����һ�Ρ�
     */
    void Cleanup();

    // �����ṹ�壬���ڽ��� URL
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
     * @brief ���� URL �ַ�����
     * @param urlString Ҫ������ URL��
     * @return ParsedUrl �ṹ�塣
     */
    ParsedUrl ParseUrl(const std::string& urlString);


    /**
     * @brief ִ�� HTTP GET ����
     * @param host ������ (���� "www.example.com")��
     * @param path ����·�� (���� "/index.html")��
     * @param port �˿ں� (ͨ���� 80 for HTTP, 443 for HTTPS)��
     * @param responseBody [out] �洢��������Ӧ���������ݡ�
     * @param responseHeadersOut [out] �洢��������Ӧ��ͷ����Ϣ (��ѡ, C2065 was here, renamed parameter)��
     * @param useHTTPS �Ƿ�ʹ�� HTTPS (��ǰʵ�ֽ�֧�ּ� HTTP)��
     * @param timeoutMs ��ʱʱ�䣨���룩 (C2065 was here, renamed parameter).
     * @return �������ɹ����յ� 2xx ��Ӧ�򷵻� true��
     *
     * @note ����һ���ǳ������� HTTP GET ʵ�֣��������ض���HTTPS (��Ҫ������� OpenSSL)��
     * ���ӵ�ͷ����Cookies �ȡ�������������������ʹ�ó���� HTTP �ͻ��˿� (�� cpr, libcurl, cpprestsdk)��
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
     * @brief �����ļ���ָ��·����
     * @param url �ļ��� URL��
     * @param outputPath �ļ�����ı���·����
     * @param progressCallback ���Ȼص����� (��ѡ)������Ϊ (��ǰ�������ֽ���, �ļ����ֽ���)��
     * ������ֽ���δ֪����Ϊ -1��
     * @return ������سɹ����� true��
     * @note �˺��������� URL����ʹ�� HttpGet (�������߼�) ����ȡ���ݡ�
     * ͬ��������һ������ʵ�֡�
     */
    bool DownloadFile(
        const std::string& url, // Expects std::string
        const std::wstring& outputPath,
        std::function<void(long long, long long)> progressCallback = nullptr
    );

} // namespace Network

#endif // NETWORK_H
