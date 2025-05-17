#include "network.h"
#include "log.h"
#include "utils.h"   // For string conversions
#include <sstream>
#include <fstream>   // For DownloadFile
#include <algorithm> // For std::transform (tolower)
#include <iostream>  // For std::cout, std::cerr (debugging or fallback)

// <map> is now included in network.h

namespace Network {

    static bool g_winsockInitialized = false;

    bool Initialize() {
        if (g_winsockInitialized) {
            return true;
        }
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            LOG_ERROR(L"WSAStartup failed. Error: ", result);
            return false;
        }
        g_winsockInitialized = true;
        LOG_INFO(L"Winsock initialized.");
        return true;
    }

    void Cleanup() {
        if (g_winsockInitialized) {
            WSACleanup();
            g_winsockInitialized = false;
            LOG_INFO(L"Winsock cleaned up.");
        }
    }

    ParsedUrl ParseUrl(const std::string& urlString) {
        ParsedUrl parsed;
        if (urlString.empty()) {
            return parsed;
        }

        std::string tempUrl = urlString;

        // Scheme
        size_t schemeEnd = tempUrl.find("://");
        if (schemeEnd != std::string::npos) {
            parsed.scheme = tempUrl.substr(0, schemeEnd);
            std::transform(parsed.scheme.begin(), parsed.scheme.end(), parsed.scheme.begin(),
                [](unsigned char c) { return std::tolower(c); });
            tempUrl = tempUrl.substr(schemeEnd + 3);
        }
        else {
            // No scheme, try to infer or default
            // For robustness, might be better to reject if no scheme for http/https
            // Or assume http if it looks like a domain name
            // For now, let's assume http if it doesn't look like a file path
            if (tempUrl.find('/') == 0 || tempUrl.find(':') == 1) { // Looks like local path
                LOG_WARNING(L"URL parsing: No scheme and looks like a local path. URL: ", Utf8ToWide(urlString).c_str());
                // return parsed; // Invalid for network operations
            }
            parsed.scheme = "http"; // Default assumption
        }

        // Host and Port
        size_t pathStart = tempUrl.find('/');
        std::string hostPortPart = (pathStart == std::string::npos) ? tempUrl : tempUrl.substr(0, pathStart);

        size_t querySeparatorInHostPort = hostPortPart.find('?'); // Query params should not be in host/port part
        if (querySeparatorInHostPort != std::string::npos) {
            hostPortPart = hostPortPart.substr(0, querySeparatorInHostPort);
            // The rest is part of the path/query, adjust pathStart if necessary
            if (pathStart == std::string::npos || pathStart > querySeparatorInHostPort) {
                pathStart = querySeparatorInHostPort; // The actual path starts earlier
            }
        }


        size_t portColon = hostPortPart.find(':');
        if (portColon != std::string::npos) {
            parsed.host = hostPortPart.substr(0, portColon);
            std::string portStr = hostPortPart.substr(portColon + 1);
            if (portStr.empty()) { // e.g. "host:"
                LOG_WARNING(L"URL Parsing: Empty port after colon. URL: ", Utf8ToWide(urlString).c_str());
                // Assign default based on scheme
                if (parsed.scheme == "http") parsed.port = 80;
                else if (parsed.scheme == "https") parsed.port = 443;
                else parsed.port = 0; // Unknown
            }
            else {
                try {
                    int portVal = std::stoi(portStr);
                    if (portVal <= 0 || portVal > 65535) {
                        LOG_WARNING(L"URL Parsing: Invalid port number ", std::to_wstring(portVal).c_str(), L". URL: ", Utf8ToWide(urlString).c_str());
                        return parsed; // Invalid port
                    }
                    parsed.port = static_cast<unsigned short>(portVal);
                }
                catch (const std::exception& e) {
                    LOG_WARNING(L"Invalid port in URL '", Utf8ToWide(urlString).c_str(), L"': ", Utf8ToWide(e.what()).c_str());
                    return parsed; // Invalid port
                }
            }
        }
        else {
            parsed.host = hostPortPart;
            if (parsed.scheme == "http") parsed.port = 80;
            else if (parsed.scheme == "https") parsed.port = 443;
            else {
                LOG_WARNING(L"Unknown scheme '", Utf8ToWide(parsed.scheme).c_str(), L"' in URL, cannot determine default port.");
                parsed.port = 0; // Indicate unknown, or default to 80
            }
        }

        // Path and Query
        if (pathStart != std::string::npos) {
            std::string pathAndQuery = tempUrl.substr(pathStart);
            size_t queryStart = pathAndQuery.find('?');
            if (queryStart != std::string::npos) {
                parsed.path = pathAndQuery.substr(0, queryStart);
                parsed.query = pathAndQuery.substr(queryStart + 1);
            }
            else {
                parsed.path = pathAndQuery;
            }
        }
        else {
            parsed.path = "/"; // Default path
        }

        if (parsed.host.empty()) {
            LOG_WARNING(L"Could not parse host from URL: ", Utf8ToWide(urlString).c_str());
            return parsed;
        }
        if (parsed.port == 0 && (parsed.scheme == "http" || parsed.scheme == "https")) {
            LOG_WARNING(L"Port is 0 for known scheme, URL: ", Utf8ToWide(urlString).c_str());
            // This might be an error or an oversight if port couldn't be parsed.
            // Re-assign default if it was missed.
            if (parsed.scheme == "http") parsed.port = 80;
            else if (parsed.scheme == "https") parsed.port = 443;
        }


        parsed.isValid = !parsed.host.empty() && parsed.port != 0;
        return parsed;
    }


    bool HttpGet(
        const std::string& host,
        const std::string& path,
        unsigned short port,
        std::string& responseBody,
        std::map<std::string, std::string>* responseHeadersOutParam, // Renamed parameter
        bool useHTTPSParam, // Renamed parameter
        int timeoutMsParam   // Renamed parameter
    )
    {
        if (!g_winsockInitialized) {
            LOG_ERROR(L"Winsock not initialized. Call Network::Initialize() first.");
            return false;
        }

        if (useHTTPSParam) { // Use renamed parameter
            LOG_ERROR(L"HTTPS is not supported in this basic HttpGet implementation.");
            return false;
        }

        responseBody.clear();
        if (responseHeadersOutParam) { // Use renamed parameter
            responseHeadersOutParam->clear();
        }

        SOCKET sock = INVALID_SOCKET;
        addrinfo* result = nullptr, * ptr = nullptr, hints;

        ZeroMemory(&hints, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        std::string portStr = std::to_string(port);

        if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &result) != 0) {
            LOG_ERROR(L"getaddrinfo failed for host: ", Utf8ToWide(host).c_str(), L" Error: ", WSAGetLastError());
            return false;
        }

        for (ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
            sock = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
            if (sock == INVALID_SOCKET) {
                LOG_WARNING(L"socket() failed. Error: ", WSAGetLastError());
                continue;
            }

            if (timeoutMsParam > 0) { // Use renamed parameter
                DWORD timeoutVal = static_cast<DWORD>(timeoutMsParam); // Corrected name
                setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeoutVal, sizeof(timeoutVal));
                setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeoutVal, sizeof(timeoutVal));
            }

            if (connect(sock, ptr->ai_addr, (int)ptr->ai_addrlen) == SOCKET_ERROR) {
                LOG_WARNING(L"connect() failed to host ", Utf8ToWide(host).c_str(), L" on an address. Error: ", WSAGetLastError());
                closesocket(sock);
                sock = INVALID_SOCKET;
                continue;
            }
            break;
        }

        freeaddrinfo(result);

        if (sock == INVALID_SOCKET) {
            LOG_ERROR(L"Unable to connect to server: ", Utf8ToWide(host).c_str());
            return false;
        }

        std::ostringstream requestStream;
        requestStream << "GET " << (path.empty() ? "/" : path) << " HTTP/1.1\r\n"; // Ensure path is not empty
        requestStream << "Host: " << host << "\r\n";
        requestStream << "Connection: close\r\n";
        requestStream << "User-Agent: NewsForHeng/1.0 (Windows)\r\n"; // Added OS
        requestStream << "Accept: */*\r\n";
        requestStream << "Accept-Encoding: identity\r\n"; // Be explicit about not handling gzip etc.
        requestStream << "\r\n";

        std::string request = requestStream.str();

        if (send(sock, request.c_str(), (int)request.length(), 0) == SOCKET_ERROR) {
            LOG_ERROR(L"send() failed. Error: ", WSAGetLastError());
            closesocket(sock);
            return false;
        }

        std::string fullResponse;
        char buffer[4096];
        int bytesReceived;

        do {
            bytesReceived = recv(sock, buffer, sizeof(buffer) - 1, 0); // -1 to leave space for null terminator
            if (bytesReceived > 0) {
                // buffer[bytesReceived] = '\0'; // Not strictly necessary if appending with length
                fullResponse.append(buffer, bytesReceived);
            }
            else if (bytesReceived == 0) {
                LOG_INFO(L"Connection closed by peer during recv.");
            }
            else {
                int error = WSAGetLastError();
                if (error == WSAETIMEDOUT) {
                    LOG_ERROR(L"recv() timed out. Error: ", error);
                }
                else {
                    LOG_ERROR(L"recv() failed. Error: ", error);
                }
                closesocket(sock);
                return false;
            }
        } while (bytesReceived > 0);

        closesocket(sock);

        size_t headerEndPos = fullResponse.find("\r\n\r\n");
        if (headerEndPos == std::string::npos) {
            LOG_ERROR(L"Invalid HTTP response: no CR LF CR LF sequence found (end of headers).");
            // LOG_DEBUG(L"Full response received: ", Utf8ToWide(fullResponse).c_str()); // Log for debugging
            return false;
        }

        std::string headersPart = fullResponse.substr(0, headerEndPos);
        responseBody = fullResponse.substr(headerEndPos + 4);

        std::istringstream headersStream(headersPart);
        std::string statusLine;
        std::getline(headersStream, statusLine);
        if (!statusLine.empty() && statusLine.back() == '\r') statusLine.pop_back();

        std::string httpVersion;
        int statusCode = 0;
        // std::string reasonPhrase; // Not strictly needed for logic
        std::istringstream statusLineStream(statusLine);
        statusLineStream >> httpVersion >> statusCode;

        if (statusCode < 200 || statusCode >= 300) {
            LOG_WARNING(L"HTTP GET request failed with status code: ", statusCode, L" for ", Utf8ToWide(host).c_str(), Utf8ToWide(path).c_str());
            return false;
        }

        LOG_INFO(L"HTTP GET successful for ", Utf8ToWide(host).c_str(), Utf8ToWide(path).c_str(), L". Status: ", statusCode);

        if (responseHeadersOutParam) { // Use renamed parameter
            std::string headerLine;
            while (std::getline(headersStream, headerLine)) {
                if (!headerLine.empty() && headerLine.back() == '\r') headerLine.pop_back();
                if (headerLine.empty()) break;

                size_t colonPos = headerLine.find(':');
                if (colonPos != std::string::npos) {
                    std::string name = headerLine.substr(0, colonPos);
                    std::string value = headerLine.substr(colonPos + 1);

                    size_t first = value.find_first_not_of(" \t");
                    if (std::string::npos != first) {
                        size_t last = value.find_last_not_of(" \t");
                        value = value.substr(first, (last - first + 1));
                    }
                    else {
                        value.clear();
                    }
                    (*responseHeadersOutParam)[name] = value; // Use renamed parameter
                }
            }
        }
        return true;
    }


    bool DownloadFile(
        const std::string& url, // Expects std::string
        const std::wstring& outputPath,
        std::function<void(long long, long long)> progressCallback)
    {
        ParsedUrl purl = ParseUrl(url); // url is already std::string
        if (!purl.isValid) {
            LOG_ERROR(L"Invalid URL for download: ", Utf8ToWide(url).c_str());
            return false;
        }

        if (purl.scheme != "http") {
            LOG_ERROR(L"DownloadFile currently only supports HTTP. URL: ", Utf8ToWide(url).c_str());
            return false;
        }

        std::string responseBody;
        std::map<std::string, std::string> responseHeadersMap; // C2039, C2065 fixed by including <map> and correct naming

        LOG_INFO(L"Attempting to download file from URL: ", Utf8ToWide(url).c_str(), L" to: ", outputPath.c_str());

        // Construct full path for HttpGet
        std::string fullPath = purl.path;
        if (!purl.query.empty()) {
            fullPath += "?" + purl.query;
        }

        if (!HttpGet(purl.host, fullPath, purl.port, responseBody, &responseHeadersMap, (purl.scheme == "https"), 15000 /* 15 sec timeout */)) {
            LOG_ERROR(L"Failed to GET file content from URL: ", Utf8ToWide(url).c_str());
            return false;
        }

        long long totalSize = -1;
        // C3536 'it' error was because responseHeadersMap might not have been properly declared or map not included.
        auto itHeader = responseHeadersMap.find("Content-Length"); // Renamed 'it' to 'itHeader' for clarity
        if (itHeader != responseHeadersMap.end()) {
            try {
                totalSize = std::stoll(itHeader->second);
            }
            catch (const std::exception&) { /* ignore parse error */ }
        }

        size_t lastSlash = outputPath.find_last_of(L"\\/");
        if (lastSlash != std::wstring::npos) {
            std::wstring dir = outputPath.substr(0, lastSlash);
            if (!DirectoryExists(dir)) {
                if (!CreateDirectoryRecursive(dir)) {
                    LOG_ERROR(L"Failed to create directory for download: ", dir.c_str());
                    return false;
                }
            }
        }

        std::ofstream outFile(outputPath, std::ios::binary | std::ios::trunc);
        if (!outFile.is_open()) {
            LOG_ERROR(L"Failed to open output file for writing: ", outputPath.c_str());
            return false;
        }

        outFile.write(responseBody.data(), responseBody.size());
        if (outFile.fail()) {
            LOG_ERROR(L"Failed to write downloaded content to file: ", outputPath.c_str());
            outFile.close();
            DeleteFileW(outputPath.c_str()); // Clean up partial file
            return false;
        }

        outFile.close();

        if (progressCallback) {
            progressCallback(static_cast<long long>(responseBody.size()), totalSize > 0 ? totalSize : static_cast<long long>(responseBody.size()));
        }

        LOG_INFO(L"File downloaded successfully: ", outputPath.c_str(), L" (Size: ", responseBody.size(), L" bytes)");
        return true;
    }

} // namespace Network
