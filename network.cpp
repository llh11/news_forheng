// network.cpp
// Implements network operations like update checks, downloads, and heartbeats.
#include "network.h" // Function declarations for this file
#include "globals.h" // Access to global constants (URLs, timeouts, USER_AGENT, g_deviceID, CURRENT_VERSION), RAII classes, g_hWnd
#include "log.h"     // For Log function
#include "utils.h"   // For string conversions (wstring_to_utf8, utf8_to_wstring), UnescapeJsonString, ParseVersion, CompareVersions
#include "update.h"  // <<< Added include for ReadIgnoredVersion (Fixes C3861)

#include <Windows.h>
#include <wininet.h>  // For WinINet functions
#include <string>
#include <vector>
#include <sstream>    // For string streams
#include <memory>     // For std::unique_ptr
#include <system_error> // For std::system_category
#include <algorithm>    // For std::max, std::min (needed by PostProgressUpdate) - Will move PostProgressUpdate later

// --- Helper Function: Get WinINet Error Message ---
// Retrieves a descriptive error message for a WinINet error code.
static std::string GetWinInetErrorMessage(DWORD errorCode) {
    char buffer[512];
    DWORD bufferLen = sizeof(buffer);
    // Use InternetGetLastResponseInfoA for extended info if available
    if (InternetGetLastResponseInfoA(&errorCode, buffer, &bufferLen)) {
        // Successfully retrieved extended error info
        return std::string(buffer, bufferLen);
    }
    else {
        // Fallback to standard system error message if extended info fails
        return std::system_category().message(errorCode);
    }
}

// --- Helper Function: Read HTTP Response Body ---
// Reads the entire response body from an HINTERNET request handle.
static bool ReadHttpResponse(HINTERNET hRequest, std::string& outResponseBody) {
    outResponseBody.clear();
    const DWORD bufferSize = 4096; // Read in 4KB chunks
    // Use std::unique_ptr for automatic buffer cleanup
    auto buffer = std::make_unique<char[]>(bufferSize);
    DWORD bytesRead = 0;

    Log("开始读取 HTTP 响应..."); // "Starting to read HTTP response..."
    while (InternetReadFile(hRequest, buffer.get(), bufferSize, &bytesRead) && bytesRead > 0) {
        try {
            outResponseBody.append(buffer.get(), bytesRead);
        }
        catch (const std::bad_alloc&) {
            Log("错误：分配内存以读取 HTTP 响应时内存不足。"); // "Error: Out of memory allocating memory for HTTP response."
            return false;
        }
        catch (...) {
            Log("错误：附加 HTTP 响应数据时发生未知错误。"); // "Error: Unknown error appending HTTP response data."
            return false;
        }
    }

    // Check for errors after the loop (InternetReadFile returns FALSE on error or 0 bytes read at end)
    DWORD lastError = GetLastError();
    if (bytesRead == 0 && lastError != ERROR_SUCCESS) { // bytesRead == 0 and success means end of file
        Log("错误：读取 HTTP 响应时出错。错误代码: " + std::to_string(lastError) + " (" + GetWinInetErrorMessage(lastError) + ")"); // "Error: Failed reading HTTP response. Error code: "
        return false;
    }

    Log("HTTP 响应读取完成。总大小: " + std::to_string(outResponseBody.length()) + " 字节。"); // "HTTP response read complete. Total size: ... bytes."
    return true;
}

// --- Helper Function: Post Progress Update Message (TEMPORARY LOCATION) ---
// TODO: Move this function definition to ui.cpp and its declaration to ui.h
// Safely allocates memory for the status string and posts the message.
// The receiving ProgressDlgProc is responsible for deleting the string memory.
void PostProgressUpdate(HWND hProgressWnd, int percent, const std::wstring& status) {
    if (!hProgressWnd || !IsWindow(hProgressWnd)) return;

    // Allocate memory for the string to pass via PostMessage
    wchar_t* msgCopy = new (std::nothrow) wchar_t[status.length() + 1];
    if (msgCopy) {
        wcscpy_s(msgCopy, status.length() + 1, status.c_str());
        if (!PostMessageW(hProgressWnd, WM_APP_UPDATE_PROGRESS, static_cast<WPARAM>(percent), reinterpret_cast<LPARAM>(msgCopy))) {
            // If PostMessage fails, we need to delete the memory we allocated
            DWORD error = GetLastError();
            Log("错误：向进度对话框发送 WM_APP_UPDATE_PROGRESS 失败。错误代码: " + std::to_string(error)); // "Error: Failed to PostMessage WM_APP_UPDATE_PROGRESS to progress dialog. Error code: "
            delete[] msgCopy; // Clean up allocated memory
        }
        // If PostMessage succeeds, ProgressDlgProc owns msgCopy and must delete it.
    }
    else {
        Log("错误：为进度消息分配内存失败。"); // "Error: Failed to allocate memory for progress message."
        // Optionally post a generic "Error" message without dynamic allocation
        // PostMessage(hProgressWnd, WM_APP_UPDATE_PROGRESS, percent, (LPARAM)L"内存错误"); // "Memory Error"
    }
}


// --- Public Network Functions ---

// Checks if the update server is reachable by pinging a specific URL.
bool CheckServerConnection(const std::wstring& pingUrl) {
    Log("检查服务器连接到: " + wstring_to_utf8(pingUrl)); // "Checking server connection to: "

    HINTERNET rawHInternet = InternetOpenW(USER_AGENT, INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!rawHInternet) {
        DWORD error = GetLastError();
        Log("错误：InternetOpenW 失败。错误代码: " + std::to_string(error) + " (" + std::system_category().message(error) + ")");
        return false;
    }
    InternetHandle hInternet(rawHInternet); // <<< FIX C2440

    InternetSetOptionW(hInternet.get(), INTERNET_OPTION_CONNECT_TIMEOUT, (LPVOID)&CONNECT_TIMEOUT_MS, sizeof(CONNECT_TIMEOUT_MS));

    HINTERNET rawHConnect = InternetOpenUrlW(hInternet.get(), pingUrl.c_str(), NULL, 0, INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_PRAGMA_NOCACHE, 0);
    if (!rawHConnect) {
        DWORD error = GetLastError();
        Log("错误：InternetOpenUrlW 失败 (" + wstring_to_utf8(pingUrl) + ")。错误代码: " + std::to_string(error) + " (" + GetWinInetErrorMessage(error) + ")");
        return false;
    }
    InternetHandle hConnect(rawHConnect); // <<< FIX C2440

    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    if (HttpQueryInfoW(hConnect.get(), HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &statusCodeSize, NULL)) {
        Log("服务器连接测试成功。状态码: " + std::to_string(statusCode));
        return (statusCode >= 200 && statusCode < 300);
    }
    else {
        DWORD error = GetLastError();
        Log("警告：无法获取服务器连接测试的状态码。错误代码: " + std::to_string(error) + " (" + GetWinInetErrorMessage(error) + ")");
        return true;
    }
}

// Checks the update server for a newer version.
bool CheckUpdate(std::wstring& outNewVersion, std::wstring& outDownloadUrl) {
    Log("正在检查更新来自: " + wstring_to_utf8(UPDATE_CHECK_URL));
    outNewVersion.clear();
    outDownloadUrl.clear();

    HINTERNET rawHInternet = InternetOpenW(USER_AGENT, INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!rawHInternet) {
        DWORD error = GetLastError();
        Log("错误：InternetOpenW 失败。错误代码: " + std::to_string(error) + " (" + std::system_category().message(error) + ")");
        return false;
    }
    InternetHandle hInternet(rawHInternet); // <<< FIX C2440

    InternetSetOptionW(hInternet.get(), INTERNET_OPTION_CONNECT_TIMEOUT, (LPVOID)&CONNECT_TIMEOUT_MS, sizeof(CONNECT_TIMEOUT_MS));
    InternetSetOptionW(hInternet.get(), INTERNET_OPTION_RECEIVE_TIMEOUT, (LPVOID)&DOWNLOAD_TIMEOUT_MS, sizeof(DOWNLOAD_TIMEOUT_MS));

    std::wstring checkUrlWithVersion = UPDATE_CHECK_URL + L"&current_version=" + CURRENT_VERSION;
    Log("更新检查 URL: " + wstring_to_utf8(checkUrlWithVersion));

    HINTERNET rawHConnect = InternetOpenUrlW(hInternet.get(), checkUrlWithVersion.c_str(), NULL, 0, INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_PRAGMA_NOCACHE, 0);
    if (!rawHConnect) {
        DWORD error = GetLastError();
        Log("错误：InternetOpenUrlW 失败 (" + wstring_to_utf8(checkUrlWithVersion) + ")。错误代码: " + std::to_string(error) + " (" + GetWinInetErrorMessage(error) + ")");
        return false;
    }
    InternetHandle hConnect(rawHConnect); // <<< FIX C2440

    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    if (!HttpQueryInfoW(hConnect.get(), HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &statusCodeSize, NULL) || statusCode != 200) {
        DWORD error = GetLastError();
        Log("错误：更新检查请求失败。HTTP 状态码: " + std::to_string(statusCode) + (statusCode != 200 ? "" : ". HttpQueryInfoW 错误: " + std::to_string(error)));
        return false;
    }

    std::string responseBody;
    if (!ReadHttpResponse(hConnect.get(), responseBody)) {
        Log("错误：读取更新检查响应失败。");
        return false;
    }
    Log("更新检查响应: " + responseBody);

    try {
        std::string latestVersionStr, downloadUrlStr;
        // Basic JSON parsing... (same as before)
        size_t versionPos = responseBody.find("\"latest_version\"");
        if (versionPos != std::string::npos) {
            size_t colonPos = responseBody.find(':', versionPos);
            size_t quoteStart = responseBody.find('"', colonPos);
            size_t quoteEnd = responseBody.find('"', quoteStart + 1);
            if (colonPos != std::string::npos && quoteStart != std::string::npos && quoteEnd != std::string::npos) {
                latestVersionStr = responseBody.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                latestVersionStr = UnescapeJsonString(latestVersionStr);
            }
        }
        size_t urlPos = responseBody.find("\"download_url\"");
        if (urlPos != std::string::npos) {
            size_t colonPos = responseBody.find(':', urlPos);
            size_t quoteStart = responseBody.find('"', colonPos);
            size_t quoteEnd = responseBody.find('"', quoteStart + 1);
            if (colonPos != std::string::npos && quoteStart != std::string::npos && quoteEnd != std::string::npos) {
                downloadUrlStr = responseBody.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                downloadUrlStr = UnescapeJsonString(downloadUrlStr);
            }
        }
        if (latestVersionStr.empty() || downloadUrlStr.empty()) {
            Log("警告：无法从响应中解析 'latest_version' 或 'download_url'。");
            return false;
        }
        std::wstring latestVersionW = utf8_to_wstring(latestVersionStr);
        std::wstring downloadUrlW = utf8_to_wstring(downloadUrlStr);
        if (latestVersionW.empty() || downloadUrlW.empty()) {
            Log("警告：从响应解析的 'latest_version' 或 'download_url' 在转换为 wstring 后为空。");
            return false;
        }

        std::vector<int> currentV = ParseVersion(CURRENT_VERSION);
        std::vector<int> latestV = ParseVersion(latestVersionW);
        if (currentV.empty() || latestV.empty()) {
            Log("错误：无法解析当前版本或最新版本字符串以进行比较。");
            return false;
        }
        int comparison = CompareVersions(latestV, currentV);
        Log("版本比较：最新 = " + latestVersionStr + ", 当前 = " + wstring_to_utf8(CURRENT_VERSION) + ", 比较结果 = " + std::to_string(comparison));

        if (comparison > 0) {
            // <<< FIX C3861: Added include "update.h" at top
            std::wstring ignoredVersion = ReadIgnoredVersion();
            if (!ignoredVersion.empty() && ignoredVersion == latestVersionW) {
                Log("找到新版本 (" + latestVersionStr + ")，但它当前被忽略。");
                return false;
            }
            else {
                Log("找到新版本！版本: " + latestVersionStr + ", URL: " + downloadUrlStr);
                outNewVersion = latestVersionW;
                outDownloadUrl = downloadUrlW;
                return true;
            }
        }
        else {
            Log("当前版本已是最新或更新。");
            return false;
        }
    }
    catch (const std::exception& e) {
        Log("错误：解析更新检查 JSON 响应时出错: " + std::string(e.what()));
        return false;
    }
    catch (...) {
        Log("错误：解析更新检查 JSON 响应时发生未知错误。");
        return false;
    }
}

// Downloads a file from a URL to a local path.
bool DownloadFile(const std::wstring& downloadUrl, const std::wstring& savePath, HWND hProgressWnd /*= NULL*/) {
    Log("开始下载文件从: " + wstring_to_utf8(downloadUrl));
    Log("保存到: " + wstring_to_utf8(savePath));

    size_t lastSlash = savePath.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) {
        std::wstring dirPath = savePath.substr(0, lastSlash);
        if (!EnsureDirectoryExists(dirPath)) {
            Log("错误：无法创建或访问下载文件的目标目录: " + wstring_to_utf8(dirPath));
            PostProgressUpdate(hProgressWnd, 0, L"错误：目录创建失败");
            return false;
        }
    }
    if (DeleteFileW(savePath.c_str()) == 0) {
        DWORD error = GetLastError();
        if (error != ERROR_FILE_NOT_FOUND) {
            Log("警告：删除现有下载文件失败 (" + wstring_to_utf8(savePath) + ")。错误代码: " + std::to_string(error));
        }
    }

    HINTERNET rawHInternet = InternetOpenW(USER_AGENT, INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!rawHInternet) {
        DWORD error = GetLastError();
        Log("错误：InternetOpenW 失败。错误代码: " + std::to_string(error) + " (" + std::system_category().message(error) + ")");
        PostProgressUpdate(hProgressWnd, 0, L"错误：网络初始化失败");
        return false;
    }
    InternetHandle hInternet(rawHInternet); // <<< FIX C2440

    InternetSetOptionW(hInternet.get(), INTERNET_OPTION_CONNECT_TIMEOUT, (LPVOID)&CONNECT_TIMEOUT_MS, sizeof(CONNECT_TIMEOUT_MS));
    InternetSetOptionW(hInternet.get(), INTERNET_OPTION_RECEIVE_TIMEOUT, (LPVOID)&DOWNLOAD_TIMEOUT_MS, sizeof(DOWNLOAD_TIMEOUT_MS));

    HINTERNET rawHConnect = InternetOpenUrlW(hInternet.get(), downloadUrl.c_str(), NULL, 0, INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_PRAGMA_NOCACHE | INTERNET_FLAG_SECURE, 0);
    if (!rawHConnect) {
        DWORD error = GetLastError();
        Log("错误：InternetOpenUrlW 失败 (" + wstring_to_utf8(downloadUrl) + ")。错误代码: " + std::to_string(error) + " (" + GetWinInetErrorMessage(error) + ")");
        PostProgressUpdate(hProgressWnd, 0, L"错误：无法连接");
        return false;
    }
    InternetHandle hConnect(rawHConnect); // <<< FIX C2440

    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    if (!HttpQueryInfoW(hConnect.get(), HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &statusCodeSize, NULL) || statusCode != 200) {
        DWORD error = GetLastError();
        Log("错误：下载请求失败。HTTP 状态码: " + std::to_string(statusCode) + (statusCode != 200 ? "" : ". HttpQueryInfoW 错误: " + std::to_string(error)));
        std::wstring errorMsg = L"错误：服务器响应 " + std::to_wstring(statusCode);
        PostProgressUpdate(hProgressWnd, 0, errorMsg); // Use helper
        return false;
    }

    DWORD totalSize = 0;
    DWORD totalSizeSize = sizeof(totalSize);
    if (HttpQueryInfoW(hConnect.get(), HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER, &totalSize, &totalSizeSize, NULL)) {
        Log("文件总大小: " + std::to_string(totalSize) + " 字节。");
    }
    else {
        Log("警告：无法获取文件内容长度。进度百分比可能不准确。");
        totalSize = 0;
    }

    FileHandle hFile(CreateFileW(savePath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL));
    if (!hFile) {
        DWORD error = GetLastError();
        Log("错误：无法创建或打开本地文件进行写入 (" + wstring_to_utf8(savePath) + ")。错误代码: " + std::to_string(error) + " (" + std::system_category().message(error) + ")");
        PostProgressUpdate(hProgressWnd, 0, L"错误：文件创建失败");
        return false;
    }

    const DWORD bufferSize = 8192;
    auto buffer = std::make_unique<char[]>(bufferSize);
    DWORD bytesRead = 0;
    DWORD totalBytesWritten = 0;
    bool downloadOk = true;

    PostProgressUpdate(hProgressWnd, 0, L"开始下载...");

    while (InternetReadFile(hConnect.get(), buffer.get(), bufferSize, &bytesRead) && bytesRead > 0) {
        DWORD bytesWritten = 0;
        if (!WriteFile(hFile.get(), buffer.get(), bytesRead, &bytesWritten, NULL) || bytesWritten != bytesRead) {
            DWORD error = GetLastError();
            Log("错误：写入本地文件时出错 (" + wstring_to_utf8(savePath) + ")。错误代码: " + std::to_string(error) + " (" + std::system_category().message(error) + ")");
            PostProgressUpdate(hProgressWnd, 0, L"错误：文件写入失败");
            downloadOk = false;
            break;
        }
        totalBytesWritten += bytesWritten;

        int percent = (totalSize > 0) ? static_cast<int>((static_cast<double>(totalBytesWritten) / totalSize) * 100.0) : 0;
        std::wstringstream ss;
        ss << L"下载中... " << percent << L"% (" << totalBytesWritten / 1024 << L" KB" << (totalSize > 0 ? L" / " + std::to_wstring(totalSize / 1024) + L" KB" : L"") << L")";
        PostProgressUpdate(hProgressWnd, percent, ss.str());
    }

    if (downloadOk) {
        DWORD lastError = GetLastError();
        if (bytesRead == 0 && lastError != ERROR_SUCCESS) {
            Log("错误：下载文件时读取网络流出错。错误代码: " + std::to_string(lastError) + " (" + GetWinInetErrorMessage(lastError) + ")");
            PostProgressUpdate(hProgressWnd, 0, L"错误：网络读取失败");
            downloadOk = false;
        }
    }

    hFile = nullptr;

    if (downloadOk) {
        Log("文件下载成功。总写入字节数: " + std::to_string(totalBytesWritten));
        PostProgressUpdate(hProgressWnd, 100, L"下载完成");
        return true;
    }
    else {
        Log("文件下载失败。");
        if (!DeleteFileW(savePath.c_str())) {
            Log("警告：删除失败的部分下载文件时出错。");
        }
        return false;
    }
}

// Sends a heartbeat POST request.
bool SendHeartbeat() {
    if (g_deviceID.empty() || g_deviceID == L"CONFIG_DIR_ERROR" || g_deviceID == L"GENERATION_FAILED") {
        Log("心跳错误：无效或未初始化的设备 ID。跳过心跳。");
        return false;
    }
    Log("发送心跳...");

    HINTERNET rawHInternet = InternetOpenW(USER_AGENT, INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!rawHInternet) {
        DWORD error = GetLastError();
        Log("心跳错误：InternetOpenW 失败。错误代码: " + std::to_string(error) + " (" + std::system_category().message(error) + ")");
        return false;
    }
    InternetHandle hInternet(rawHInternet); // <<< FIX C2440

    InternetSetOptionW(hInternet.get(), INTERNET_OPTION_CONNECT_TIMEOUT, (LPVOID)&CONNECT_TIMEOUT_MS, sizeof(CONNECT_TIMEOUT_MS));
    InternetSetOptionW(hInternet.get(), INTERNET_OPTION_RECEIVE_TIMEOUT, (LPVOID)&CONNECT_TIMEOUT_MS, sizeof(CONNECT_TIMEOUT_MS));

    URL_COMPONENTSW urlComp = { sizeof(URL_COMPONENTSW) };
    wchar_t hostName[256] = { 0 };
    wchar_t urlPath[1024] = { 0 };
    urlComp.lpszHostName = hostName;
    urlComp.dwHostNameLength = std::size(hostName);
    urlComp.lpszUrlPath = urlPath;
    urlComp.dwUrlPathLength = std::size(urlPath);
    urlComp.nScheme = INTERNET_SCHEME_DEFAULT;
    if (!InternetCrackUrlW(HEARTBEAT_API_URL.c_str(), 0, 0, &urlComp)) {
        DWORD error = GetLastError();
        Log("心跳错误：InternetCrackUrlW 失败。错误代码: " + std::to_string(error));
        return false;
    }

    HINTERNET rawHConnect = InternetConnectW(hInternet.get(), hostName, urlComp.nPort, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!rawHConnect) {
        DWORD error = GetLastError();
        Log("心跳错误：InternetConnectW 失败 (" + wstring_to_utf8(hostName) + ")。错误代码: " + std::to_string(error) + " (" + GetWinInetErrorMessage(error) + ")");
        return false;
    }
    InternetHandle hConnect(rawHConnect); // <<< FIX C2440

    DWORD dwFlags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_PRAGMA_NOCACHE;
    if (urlComp.nScheme == INTERNET_SCHEME_HTTPS) {
        dwFlags |= INTERNET_FLAG_SECURE;
    }
    HINTERNET rawHRequest = HttpOpenRequestW(hConnect.get(), L"POST", urlPath, NULL, NULL, NULL, dwFlags, 0);
    if (!rawHRequest) {
        DWORD error = GetLastError();
        Log("心跳错误：HttpOpenRequestW 失败。错误代码: " + std::to_string(error) + " (" + GetWinInetErrorMessage(error) + ")");
        return false;
    }
    InternetHandle hRequest(rawHRequest); // <<< FIX C2440

    std::string deviceIdUtf8 = wstring_to_utf8(g_deviceID);
    std::string versionUtf8 = wstring_to_utf8(CURRENT_VERSION);
    std::string postData = "device_id=" + deviceIdUtf8 + "&version=" + versionUtf8;
    const wchar_t* headers = L"Content-Type: application/x-www-form-urlencoded";
    if (!HttpSendRequestW(hRequest.get(), headers, (DWORD)wcslen(headers), (LPVOID)postData.c_str(), (DWORD)postData.length())) {
        DWORD error = GetLastError();
        Log("心跳错误：HttpSendRequestW 失败。错误代码: " + std::to_string(error) + " (" + GetWinInetErrorMessage(error) + ")");
        return false;
    }

    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    if (!HttpQueryInfoW(hRequest.get(), HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &statusCodeSize, NULL) || statusCode != 200) {
        DWORD error = GetLastError();
        Log("心跳错误：服务器响应非 200 OK。状态码: " + std::to_string(statusCode) + (statusCode != 200 ? "" : ". HttpQueryInfoW 错误: " + std::to_string(error)));
        return false;
    }
    Log("心跳发送成功。");
    return true;
}

// Gets the latest filename for a given type from the server.
bool GetLatestFilenameFromServer(const std::wstring& type, std::wstring& outFilename) {
    Log("正在获取服务器上 '" + wstring_to_utf8(type) + "' 的最新文件名...");
    outFilename.clear();
    std::wstring requestUrl = STATUS_API_URL + L"?action=get_latest&type=" + type;
    Log("请求 URL: " + wstring_to_utf8(requestUrl));

    HINTERNET rawHInternet = InternetOpenW(USER_AGENT, INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!rawHInternet) {
        DWORD error = GetLastError();
        Log("错误：InternetOpenW 失败。错误代码: " + std::to_string(error) + " (" + std::system_category().message(error) + ")");
        return false;
    }
    InternetHandle hInternet(rawHInternet); // <<< FIX C2440

    InternetSetOptionW(hInternet.get(), INTERNET_OPTION_CONNECT_TIMEOUT, (LPVOID)&CONNECT_TIMEOUT_MS, sizeof(CONNECT_TIMEOUT_MS));
    InternetSetOptionW(hInternet.get(), INTERNET_OPTION_RECEIVE_TIMEOUT, (LPVOID)&DOWNLOAD_TIMEOUT_MS, sizeof(DOWNLOAD_TIMEOUT_MS));

    HINTERNET rawHConnect = InternetOpenUrlW(hInternet.get(), requestUrl.c_str(), NULL, 0, INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_PRAGMA_NOCACHE, 0);
    if (!rawHConnect) {
        DWORD error = GetLastError();
        Log("错误：InternetOpenUrlW 失败 (" + wstring_to_utf8(requestUrl) + ")。错误代码: " + std::to_string(error) + " (" + GetWinInetErrorMessage(error) + ")");
        return false;
    }
    InternetHandle hConnect(rawHConnect); // <<< FIX C2440

    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    if (!HttpQueryInfoW(hConnect.get(), HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &statusCodeSize, NULL) || statusCode != 200) {
        DWORD error = GetLastError();
        Log("错误：获取最新文件名请求失败。HTTP 状态码: " + std::to_string(statusCode) + (statusCode != 200 ? "" : ". HttpQueryInfoW 错误: " + std::to_string(error)));
        return false;
    }

    std::string responseBody;
    if (!ReadHttpResponse(hConnect.get(), responseBody)) {
        Log("错误：读取最新文件名响应失败。");
        return false;
    }
    responseBody.erase(0, responseBody.find_first_not_of(" \t\n\r"));
    responseBody.erase(responseBody.find_last_not_of(" \t\n\r") + 1);
    Log("服务器响应（文件名）: " + responseBody);

    if (responseBody.empty()) {
        Log("错误：服务器对最新文件名返回了空响应。");
        return false;
    }
    outFilename = utf8_to_wstring(responseBody);
    if (outFilename.empty() && !responseBody.empty()) {
        Log("错误：无法将服务器响应（文件名）转换为 wstring。");
        return false;
    }
    Log("成功获取最新文件名: " + wstring_to_utf8(outFilename));
    return true;
}
