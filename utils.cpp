#include "utils.h"
#include <shlwapi.h> // For PathFileExistsW, PathIsDirectoryW
#include <fstream>
#include <sstream>
#include <algorithm> // For std::replace on older compilers, or manual loop.

// 链接 Shlwapi.lib
#pragma comment(lib, "Shlwapi.lib")

std::wstring GetExecutablePath() {
    wchar_t path[MAX_PATH] = { 0 };
    if (GetModuleFileNameW(NULL, path, MAX_PATH) == 0) {
        // 错误处理，可以记录日志
        // LOG_ERROR(L"Failed to get executable path. Error code: ", GetLastError());
        return L"";
    }
    return std::wstring(path);
}

std::wstring GetExecutableDir() {
    std::wstring exePath = GetExecutablePath();
    if (exePath.empty()) {
        return L"";
    }
    size_t lastSlash = exePath.find_last_of(L"\\/");
    if (std::wstring::npos != lastSlash) {
        return exePath.substr(0, lastSlash);
    }
    return L""; // 不太可能发生，但作为健壮性检查
}

bool DirectoryExists(const std::wstring& dirPath) {
    DWORD attributes = GetFileAttributesW(dirPath.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        return false; // 路径无效或不可访问
    }
    return (attributes & FILE_ATTRIBUTE_DIRECTORY);
}

bool CreateDirectoryRecursive(const std::wstring& dirPath) {
    if (DirectoryExists(dirPath)) {
        return true;
    }

    // 尝试创建当前目录
    if (CreateDirectoryW(dirPath.c_str(), NULL)) {
        return true;
    }

    // 如果因为父目录不存在而失败 (ERROR_PATH_NOT_FOUND)
    if (GetLastError() == ERROR_PATH_NOT_FOUND) {
        size_t pos = dirPath.find_last_of(L"\\/");
        if (pos != std::wstring::npos) {
            std::wstring parentDir = dirPath.substr(0, pos);
            if (!parentDir.empty() && CreateDirectoryRecursive(parentDir)) {
                // 父目录创建成功后，再次尝试创建当前目录
                return CreateDirectoryW(dirPath.c_str(), NULL) != 0;
            }
        }
    }
    // LOG_ERROR(L"Failed to create directory: ", dirPath, L". Error: ", GetLastError());
    return false;
}

std::wstring Utf8ToWide(const std::string& utf8String) {
    if (utf8String.empty()) {
        return std::wstring();
    }
    int wideCharCount = MultiByteToWideChar(CP_UTF8, 0, utf8String.c_str(), -1, NULL, 0);
    if (wideCharCount == 0) {
        // LOG_ERROR(L"Failed to convert UTF-8 to WideChar (calculate length). Error: ", GetLastError());
        return std::wstring();
    }
    std::wstring wideString(wideCharCount, 0);
    if (MultiByteToWideChar(CP_UTF8, 0, utf8String.c_str(), -1, &wideString[0], wideCharCount) == 0) {
        // LOG_ERROR(L"Failed to convert UTF-8 to WideChar. Error: ", GetLastError());
        return std::wstring();
    }
    // MultiByteToWideChar with -1 for cchMultiByte null-terminates.
    // The size wideCharCount includes space for the null terminator.
    // If the string might have embedded nulls and you don't want that behavior, pass utf8String.length().
    // Then, resize wideString appropriately.
    // For typical C-style strings, -1 is fine.
    // The wstring constructor used (wideCharCount, 0) might create a string one char too long if wideCharCount includes the null.
    // Let's adjust:
    wideString.resize(wcslen(wideString.c_str())); // Trim to actual length if conversion includes terminator
    return wideString;
}

std::string WideToUtf8(const std::wstring& wideString) {
    if (wideString.empty()) {
        return std::string();
    }
    int utf8CharCount = WideCharToMultiByte(CP_UTF8, 0, wideString.c_str(), -1, NULL, 0, NULL, NULL);
    if (utf8CharCount == 0) {
        // LOG_ERROR(L"Failed to convert WideChar to UTF-8 (calculate length). Error: ", GetLastError());
        return std::string();
    }
    std::string utf8String(utf8CharCount, 0);
    if (WideCharToMultiByte(CP_UTF8, 0, wideString.c_str(), -1, &utf8String[0], utf8CharCount, NULL, NULL) == 0) {
        // LOG_ERROR(L"Failed to convert WideChar to UTF-8. Error: ", GetLastError());
        return std::string();
    }
    utf8String.resize(strlen(utf8String.c_str())); // Trim to actual length
    return utf8String;
}

void ReplaceAll(std::wstring& str, const std::wstring& from, const std::wstring& to) {
    if (from.empty())
        return;
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::wstring::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // 处理 'to' 包含 'from' 的情况，例如 ReplaceAll("aaaa", "aa", "aaa")
    }
}

bool FileExists(const std::wstring& filePath) {
    // PathFileExistsW 需要 shlwapi.h 和 Shlwapi.lib
    return PathFileExistsW(filePath.c_str()) == TRUE;
}

bool ReadFileToString(const std::wstring& filePath, std::string& content) {
    std::ifstream file(filePath, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        // LOG_WARNING(L"Failed to open file for reading (binary): ", filePath);
        return false;
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    content = ss.str();
    file.close();
    return true;
}

bool ReadFileToWString(const std::wstring& filePath, std::wstring& content) {
    // 这个函数假设文件是宽字符编码 (如 UTF-16 LE，Windows 常见)
    // 或者是一个可以安全地逐字节读入然后转换为 wstring 的编码 (不推荐)
    // 更健壮的方法是确定文件编码，然后进行转换
    std::wifstream file(filePath, std::ios::in); // 默认模式可能依赖于 locale
    if (!file.is_open()) {
        // LOG_WARNING(L"Failed to open file for reading (wide): ", filePath);
        return false;
    }

    // // 可选：设置 locale 以正确处理文件编码，例如 UTF-8
    // try {
    //     file.imbue(std::locale(std::locale(), new std::codecvt_utf8<wchar_t>));
    // } catch (const std::exception& e) {
    //     // LOG_ERROR(L"Failed to imbue locale for wifstream: ", e.what());
    //     // Fallback or error
    // }

    std::wstringstream wss;
    wss << file.rdbuf();
    content = wss.str();
    file.close();
    return true;
}

