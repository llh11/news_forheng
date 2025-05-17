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

    Log("��ʼ��ȡ HTTP ��Ӧ..."); // "Starting to read HTTP response..."
    while (InternetReadFile(hRequest, buffer.get(), bufferSize, &bytesRead) && bytesRead > 0) {
        try {
            outResponseBody.append(buffer.get(), bytesRead);
        }
        catch (const std::bad_alloc&) {
            Log("���󣺷����ڴ��Զ�ȡ HTTP ��Ӧʱ�ڴ治�㡣"); // "Error: Out of memory allocating memory for HTTP response."
            return false;
        }
        catch (...) {
            Log("���󣺸��� HTTP ��Ӧ����ʱ����δ֪����"); // "Error: Unknown error appending HTTP response data."
            return false;
        }
    }

    // Check for errors after the loop (InternetReadFile returns FALSE on error or 0 bytes read at end)
    DWORD lastError = GetLastError();
    if (bytesRead == 0 && lastError != ERROR_SUCCESS) { // bytesRead == 0 and success means end of file
        Log("���󣺶�ȡ HTTP ��Ӧʱ�����������: " + std::to_string(lastError) + " (" + GetWinInetErrorMessage(lastError) + ")"); // "Error: Failed reading HTTP response. Error code: "
        return false;
    }

    Log("HTTP ��Ӧ��ȡ��ɡ��ܴ�С: " + std::to_string(outResponseBody.length()) + " �ֽڡ�"); // "HTTP response read complete. Total size: ... bytes."
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
            Log("��������ȶԻ����� WM_APP_UPDATE_PROGRESS ʧ�ܡ��������: " + std::to_string(error)); // "Error: Failed to PostMessage WM_APP_UPDATE_PROGRESS to progress dialog. Error code: "
            delete[] msgCopy; // Clean up allocated memory
        }
        // If PostMessage succeeds, ProgressDlgProc owns msgCopy and must delete it.
    }
    else {
        Log("����Ϊ������Ϣ�����ڴ�ʧ�ܡ�"); // "Error: Failed to allocate memory for progress message."
        // Optionally post a generic "Error" message without dynamic allocation
        // PostMessage(hProgressWnd, WM_APP_UPDATE_PROGRESS, percent, (LPARAM)L"�ڴ����"); // "Memory Error"
    }
}


// --- Public Network Functions ---

// Checks if the update server is reachable by pinging a specific URL.
bool CheckServerConnection(const std::wstring& pingUrl) {
    Log("�����������ӵ�: " + wstring_to_utf8(pingUrl)); // "Checking server connection to: "

    HINTERNET rawHInternet = InternetOpenW(USER_AGENT, INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!rawHInternet) {
        DWORD error = GetLastError();
        Log("����InternetOpenW ʧ�ܡ��������: " + std::to_string(error) + " (" + std::system_category().message(error) + ")");
        return false;
    }
    InternetHandle hInternet(rawHInternet); // <<< FIX C2440

    InternetSetOptionW(hInternet.get(), INTERNET_OPTION_CONNECT_TIMEOUT, (LPVOID)&CONNECT_TIMEOUT_MS, sizeof(CONNECT_TIMEOUT_MS));

    HINTERNET rawHConnect = InternetOpenUrlW(hInternet.get(), pingUrl.c_str(), NULL, 0, INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_PRAGMA_NOCACHE, 0);
    if (!rawHConnect) {
        DWORD error = GetLastError();
        Log("����InternetOpenUrlW ʧ�� (" + wstring_to_utf8(pingUrl) + ")���������: " + std::to_string(error) + " (" + GetWinInetErrorMessage(error) + ")");
        return false;
    }
    InternetHandle hConnect(rawHConnect); // <<< FIX C2440

    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    if (HttpQueryInfoW(hConnect.get(), HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &statusCodeSize, NULL)) {
        Log("���������Ӳ��Գɹ���״̬��: " + std::to_string(statusCode));
        return (statusCode >= 200 && statusCode < 300);
    }
    else {
        DWORD error = GetLastError();
        Log("���棺�޷���ȡ���������Ӳ��Ե�״̬�롣�������: " + std::to_string(error) + " (" + GetWinInetErrorMessage(error) + ")");
        return true;
    }
}

// Checks the update server for a newer version.
bool CheckUpdate(std::wstring& outNewVersion, std::wstring& outDownloadUrl) {
    Log("���ڼ���������: " + wstring_to_utf8(UPDATE_CHECK_URL));
    outNewVersion.clear();
    outDownloadUrl.clear();

    HINTERNET rawHInternet = InternetOpenW(USER_AGENT, INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!rawHInternet) {
        DWORD error = GetLastError();
        Log("����InternetOpenW ʧ�ܡ��������: " + std::to_string(error) + " (" + std::system_category().message(error) + ")");
        return false;
    }
    InternetHandle hInternet(rawHInternet); // <<< FIX C2440

    InternetSetOptionW(hInternet.get(), INTERNET_OPTION_CONNECT_TIMEOUT, (LPVOID)&CONNECT_TIMEOUT_MS, sizeof(CONNECT_TIMEOUT_MS));
    InternetSetOptionW(hInternet.get(), INTERNET_OPTION_RECEIVE_TIMEOUT, (LPVOID)&DOWNLOAD_TIMEOUT_MS, sizeof(DOWNLOAD_TIMEOUT_MS));

    std::wstring checkUrlWithVersion = UPDATE_CHECK_URL + L"&current_version=" + CURRENT_VERSION;
    Log("���¼�� URL: " + wstring_to_utf8(checkUrlWithVersion));

    HINTERNET rawHConnect = InternetOpenUrlW(hInternet.get(), checkUrlWithVersion.c_str(), NULL, 0, INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_PRAGMA_NOCACHE, 0);
    if (!rawHConnect) {
        DWORD error = GetLastError();
        Log("����InternetOpenUrlW ʧ�� (" + wstring_to_utf8(checkUrlWithVersion) + ")���������: " + std::to_string(error) + " (" + GetWinInetErrorMessage(error) + ")");
        return false;
    }
    InternetHandle hConnect(rawHConnect); // <<< FIX C2440

    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    if (!HttpQueryInfoW(hConnect.get(), HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &statusCodeSize, NULL) || statusCode != 200) {
        DWORD error = GetLastError();
        Log("���󣺸��¼������ʧ�ܡ�HTTP ״̬��: " + std::to_string(statusCode) + (statusCode != 200 ? "" : ". HttpQueryInfoW ����: " + std::to_string(error)));
        return false;
    }

    std::string responseBody;
    if (!ReadHttpResponse(hConnect.get(), responseBody)) {
        Log("���󣺶�ȡ���¼����Ӧʧ�ܡ�");
        return false;
    }
    Log("���¼����Ӧ: " + responseBody);

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
            Log("���棺�޷�����Ӧ�н��� 'latest_version' �� 'download_url'��");
            return false;
        }
        std::wstring latestVersionW = utf8_to_wstring(latestVersionStr);
        std::wstring downloadUrlW = utf8_to_wstring(downloadUrlStr);
        if (latestVersionW.empty() || downloadUrlW.empty()) {
            Log("���棺����Ӧ������ 'latest_version' �� 'download_url' ��ת��Ϊ wstring ��Ϊ�ա�");
            return false;
        }

        std::vector<int> currentV = ParseVersion(CURRENT_VERSION);
        std::vector<int> latestV = ParseVersion(latestVersionW);
        if (currentV.empty() || latestV.empty()) {
            Log("�����޷�������ǰ�汾�����°汾�ַ����Խ��бȽϡ�");
            return false;
        }
        int comparison = CompareVersions(latestV, currentV);
        Log("�汾�Ƚϣ����� = " + latestVersionStr + ", ��ǰ = " + wstring_to_utf8(CURRENT_VERSION) + ", �ȽϽ�� = " + std::to_string(comparison));

        if (comparison > 0) {
            // <<< FIX C3861: Added include "update.h" at top
            std::wstring ignoredVersion = ReadIgnoredVersion();
            if (!ignoredVersion.empty() && ignoredVersion == latestVersionW) {
                Log("�ҵ��°汾 (" + latestVersionStr + ")��������ǰ�����ԡ�");
                return false;
            }
            else {
                Log("�ҵ��°汾���汾: " + latestVersionStr + ", URL: " + downloadUrlStr);
                outNewVersion = latestVersionW;
                outDownloadUrl = downloadUrlW;
                return true;
            }
        }
        else {
            Log("��ǰ�汾�������»���¡�");
            return false;
        }
    }
    catch (const std::exception& e) {
        Log("���󣺽������¼�� JSON ��Ӧʱ����: " + std::string(e.what()));
        return false;
    }
    catch (...) {
        Log("���󣺽������¼�� JSON ��Ӧʱ����δ֪����");
        return false;
    }
}

// Downloads a file from a URL to a local path.
bool DownloadFile(const std::wstring& downloadUrl, const std::wstring& savePath, HWND hProgressWnd /*= NULL*/) {
    Log("��ʼ�����ļ���: " + wstring_to_utf8(downloadUrl));
    Log("���浽: " + wstring_to_utf8(savePath));

    size_t lastSlash = savePath.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) {
        std::wstring dirPath = savePath.substr(0, lastSlash);
        if (!EnsureDirectoryExists(dirPath)) {
            Log("�����޷���������������ļ���Ŀ��Ŀ¼: " + wstring_to_utf8(dirPath));
            PostProgressUpdate(hProgressWnd, 0, L"����Ŀ¼����ʧ��");
            return false;
        }
    }
    if (DeleteFileW(savePath.c_str()) == 0) {
        DWORD error = GetLastError();
        if (error != ERROR_FILE_NOT_FOUND) {
            Log("���棺ɾ�����������ļ�ʧ�� (" + wstring_to_utf8(savePath) + ")���������: " + std::to_string(error));
        }
    }

    HINTERNET rawHInternet = InternetOpenW(USER_AGENT, INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!rawHInternet) {
        DWORD error = GetLastError();
        Log("����InternetOpenW ʧ�ܡ��������: " + std::to_string(error) + " (" + std::system_category().message(error) + ")");
        PostProgressUpdate(hProgressWnd, 0, L"���������ʼ��ʧ��");
        return false;
    }
    InternetHandle hInternet(rawHInternet); // <<< FIX C2440

    InternetSetOptionW(hInternet.get(), INTERNET_OPTION_CONNECT_TIMEOUT, (LPVOID)&CONNECT_TIMEOUT_MS, sizeof(CONNECT_TIMEOUT_MS));
    InternetSetOptionW(hInternet.get(), INTERNET_OPTION_RECEIVE_TIMEOUT, (LPVOID)&DOWNLOAD_TIMEOUT_MS, sizeof(DOWNLOAD_TIMEOUT_MS));

    HINTERNET rawHConnect = InternetOpenUrlW(hInternet.get(), downloadUrl.c_str(), NULL, 0, INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_PRAGMA_NOCACHE | INTERNET_FLAG_SECURE, 0);
    if (!rawHConnect) {
        DWORD error = GetLastError();
        Log("����InternetOpenUrlW ʧ�� (" + wstring_to_utf8(downloadUrl) + ")���������: " + std::to_string(error) + " (" + GetWinInetErrorMessage(error) + ")");
        PostProgressUpdate(hProgressWnd, 0, L"�����޷�����");
        return false;
    }
    InternetHandle hConnect(rawHConnect); // <<< FIX C2440

    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    if (!HttpQueryInfoW(hConnect.get(), HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &statusCodeSize, NULL) || statusCode != 200) {
        DWORD error = GetLastError();
        Log("������������ʧ�ܡ�HTTP ״̬��: " + std::to_string(statusCode) + (statusCode != 200 ? "" : ". HttpQueryInfoW ����: " + std::to_string(error)));
        std::wstring errorMsg = L"���󣺷�������Ӧ " + std::to_wstring(statusCode);
        PostProgressUpdate(hProgressWnd, 0, errorMsg); // Use helper
        return false;
    }

    DWORD totalSize = 0;
    DWORD totalSizeSize = sizeof(totalSize);
    if (HttpQueryInfoW(hConnect.get(), HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER, &totalSize, &totalSizeSize, NULL)) {
        Log("�ļ��ܴ�С: " + std::to_string(totalSize) + " �ֽڡ�");
    }
    else {
        Log("���棺�޷���ȡ�ļ����ݳ��ȡ����Ȱٷֱȿ��ܲ�׼ȷ��");
        totalSize = 0;
    }

    FileHandle hFile(CreateFileW(savePath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL));
    if (!hFile) {
        DWORD error = GetLastError();
        Log("�����޷�������򿪱����ļ�����д�� (" + wstring_to_utf8(savePath) + ")���������: " + std::to_string(error) + " (" + std::system_category().message(error) + ")");
        PostProgressUpdate(hProgressWnd, 0, L"�����ļ�����ʧ��");
        return false;
    }

    const DWORD bufferSize = 8192;
    auto buffer = std::make_unique<char[]>(bufferSize);
    DWORD bytesRead = 0;
    DWORD totalBytesWritten = 0;
    bool downloadOk = true;

    PostProgressUpdate(hProgressWnd, 0, L"��ʼ����...");

    while (InternetReadFile(hConnect.get(), buffer.get(), bufferSize, &bytesRead) && bytesRead > 0) {
        DWORD bytesWritten = 0;
        if (!WriteFile(hFile.get(), buffer.get(), bytesRead, &bytesWritten, NULL) || bytesWritten != bytesRead) {
            DWORD error = GetLastError();
            Log("����д�뱾���ļ�ʱ���� (" + wstring_to_utf8(savePath) + ")���������: " + std::to_string(error) + " (" + std::system_category().message(error) + ")");
            PostProgressUpdate(hProgressWnd, 0, L"�����ļ�д��ʧ��");
            downloadOk = false;
            break;
        }
        totalBytesWritten += bytesWritten;

        int percent = (totalSize > 0) ? static_cast<int>((static_cast<double>(totalBytesWritten) / totalSize) * 100.0) : 0;
        std::wstringstream ss;
        ss << L"������... " << percent << L"% (" << totalBytesWritten / 1024 << L" KB" << (totalSize > 0 ? L" / " + std::to_wstring(totalSize / 1024) + L" KB" : L"") << L")";
        PostProgressUpdate(hProgressWnd, percent, ss.str());
    }

    if (downloadOk) {
        DWORD lastError = GetLastError();
        if (bytesRead == 0 && lastError != ERROR_SUCCESS) {
            Log("���������ļ�ʱ��ȡ�����������������: " + std::to_string(lastError) + " (" + GetWinInetErrorMessage(lastError) + ")");
            PostProgressUpdate(hProgressWnd, 0, L"���������ȡʧ��");
            downloadOk = false;
        }
    }

    hFile = nullptr;

    if (downloadOk) {
        Log("�ļ����سɹ�����д���ֽ���: " + std::to_string(totalBytesWritten));
        PostProgressUpdate(hProgressWnd, 100, L"�������");
        return true;
    }
    else {
        Log("�ļ�����ʧ�ܡ�");
        if (!DeleteFileW(savePath.c_str())) {
            Log("���棺ɾ��ʧ�ܵĲ��������ļ�ʱ����");
        }
        return false;
    }
}

// Sends a heartbeat POST request.
bool SendHeartbeat() {
    if (g_deviceID.empty() || g_deviceID == L"CONFIG_DIR_ERROR" || g_deviceID == L"GENERATION_FAILED") {
        Log("����������Ч��δ��ʼ�����豸 ID������������");
        return false;
    }
    Log("��������...");

    HINTERNET rawHInternet = InternetOpenW(USER_AGENT, INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!rawHInternet) {
        DWORD error = GetLastError();
        Log("��������InternetOpenW ʧ�ܡ��������: " + std::to_string(error) + " (" + std::system_category().message(error) + ")");
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
        Log("��������InternetCrackUrlW ʧ�ܡ��������: " + std::to_string(error));
        return false;
    }

    HINTERNET rawHConnect = InternetConnectW(hInternet.get(), hostName, urlComp.nPort, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!rawHConnect) {
        DWORD error = GetLastError();
        Log("��������InternetConnectW ʧ�� (" + wstring_to_utf8(hostName) + ")���������: " + std::to_string(error) + " (" + GetWinInetErrorMessage(error) + ")");
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
        Log("��������HttpOpenRequestW ʧ�ܡ��������: " + std::to_string(error) + " (" + GetWinInetErrorMessage(error) + ")");
        return false;
    }
    InternetHandle hRequest(rawHRequest); // <<< FIX C2440

    std::string deviceIdUtf8 = wstring_to_utf8(g_deviceID);
    std::string versionUtf8 = wstring_to_utf8(CURRENT_VERSION);
    std::string postData = "device_id=" + deviceIdUtf8 + "&version=" + versionUtf8;
    const wchar_t* headers = L"Content-Type: application/x-www-form-urlencoded";
    if (!HttpSendRequestW(hRequest.get(), headers, (DWORD)wcslen(headers), (LPVOID)postData.c_str(), (DWORD)postData.length())) {
        DWORD error = GetLastError();
        Log("��������HttpSendRequestW ʧ�ܡ��������: " + std::to_string(error) + " (" + GetWinInetErrorMessage(error) + ")");
        return false;
    }

    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    if (!HttpQueryInfoW(hRequest.get(), HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &statusCodeSize, NULL) || statusCode != 200) {
        DWORD error = GetLastError();
        Log("�������󣺷�������Ӧ�� 200 OK��״̬��: " + std::to_string(statusCode) + (statusCode != 200 ? "" : ". HttpQueryInfoW ����: " + std::to_string(error)));
        return false;
    }
    Log("�������ͳɹ���");
    return true;
}

// Gets the latest filename for a given type from the server.
bool GetLatestFilenameFromServer(const std::wstring& type, std::wstring& outFilename) {
    Log("���ڻ�ȡ�������� '" + wstring_to_utf8(type) + "' �������ļ���...");
    outFilename.clear();
    std::wstring requestUrl = STATUS_API_URL + L"?action=get_latest&type=" + type;
    Log("���� URL: " + wstring_to_utf8(requestUrl));

    HINTERNET rawHInternet = InternetOpenW(USER_AGENT, INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!rawHInternet) {
        DWORD error = GetLastError();
        Log("����InternetOpenW ʧ�ܡ��������: " + std::to_string(error) + " (" + std::system_category().message(error) + ")");
        return false;
    }
    InternetHandle hInternet(rawHInternet); // <<< FIX C2440

    InternetSetOptionW(hInternet.get(), INTERNET_OPTION_CONNECT_TIMEOUT, (LPVOID)&CONNECT_TIMEOUT_MS, sizeof(CONNECT_TIMEOUT_MS));
    InternetSetOptionW(hInternet.get(), INTERNET_OPTION_RECEIVE_TIMEOUT, (LPVOID)&DOWNLOAD_TIMEOUT_MS, sizeof(DOWNLOAD_TIMEOUT_MS));

    HINTERNET rawHConnect = InternetOpenUrlW(hInternet.get(), requestUrl.c_str(), NULL, 0, INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_PRAGMA_NOCACHE, 0);
    if (!rawHConnect) {
        DWORD error = GetLastError();
        Log("����InternetOpenUrlW ʧ�� (" + wstring_to_utf8(requestUrl) + ")���������: " + std::to_string(error) + " (" + GetWinInetErrorMessage(error) + ")");
        return false;
    }
    InternetHandle hConnect(rawHConnect); // <<< FIX C2440

    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    if (!HttpQueryInfoW(hConnect.get(), HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &statusCodeSize, NULL) || statusCode != 200) {
        DWORD error = GetLastError();
        Log("���󣺻�ȡ�����ļ�������ʧ�ܡ�HTTP ״̬��: " + std::to_string(statusCode) + (statusCode != 200 ? "" : ". HttpQueryInfoW ����: " + std::to_string(error)));
        return false;
    }

    std::string responseBody;
    if (!ReadHttpResponse(hConnect.get(), responseBody)) {
        Log("���󣺶�ȡ�����ļ�����Ӧʧ�ܡ�");
        return false;
    }
    responseBody.erase(0, responseBody.find_first_not_of(" \t\n\r"));
    responseBody.erase(responseBody.find_last_not_of(" \t\n\r") + 1);
    Log("��������Ӧ���ļ�����: " + responseBody);

    if (responseBody.empty()) {
        Log("���󣺷������������ļ��������˿���Ӧ��");
        return false;
    }
    outFilename = utf8_to_wstring(responseBody);
    if (outFilename.empty() && !responseBody.empty()) {
        Log("�����޷�����������Ӧ���ļ�����ת��Ϊ wstring��");
        return false;
    }
    Log("�ɹ���ȡ�����ļ���: " + wstring_to_utf8(outFilename));
    return true;
}
