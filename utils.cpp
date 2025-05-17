// utils.cpp
// Implements various utility functions.
#include "utils.h"
#include "globals.h" // For CONFIGURATOR_EXE_NAME, g_hWnd (used in LaunchConfiguratorAndWait MessageBox), Log
#include "log.h"     // For Log

#include <Windows.h>
#include <string>
#include <vector>
#include <sstream>    // <<< Added for stringstream
#include <stdexcept>  // For std::exception in ParseVersion
#include <rpcdce.h>   // For UUID functions
#include <shlobj.h>   // For SHCreateDirectoryExW
#include <processthreadsapi.h> // For CreateProcessW, GetExitCodeProcess
#include <synchapi.h>       // For WaitForSingleObject
#include <handleapi.h>      // For CloseHandle
#include <algorithm>        // For std::max (though not used here directly)
#include <system_error>     // For std::system_category

// --- String Conversion Utilities ---
// (wstring_to_utf8, utf8_to_wstring - unchanged from previous fix)
std::string wstring_to_utf8(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), NULL, 0, NULL, NULL);
    if (size_needed <= 0) {
        DWORD error = GetLastError();
        OutputDebugStringA(("错误：WideCharToMultiByte (计算大小) 失败。错误代码: " + std::to_string(error) + " (" + std::system_category().message(error) + ")\n").c_str()); // "Error: WideCharToMultiByte (size calculation) failed. Error code: "
        return "";
    }
    std::string strTo(size_needed, 0);
    int chars_converted = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    if (chars_converted <= 0) {
        DWORD error = GetLastError();
        OutputDebugStringA(("错误：WideCharToMultiByte (转换) 失败。错误代码: " + std::to_string(error) + " (" + std::system_category().message(error) + ")\n").c_str()); // "Error: WideCharToMultiByte (conversion) failed. Error code: "
        return "";
    }
    return strTo;
}
std::wstring utf8_to_wstring(const std::string& str) {
    if (str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
    if (size_needed <= 0) {
        DWORD error = GetLastError();
        OutputDebugStringA(("错误：MultiByteToWideChar (计算大小) 失败。错误代码: " + std::to_string(error) + " (" + std::system_category().message(error) + ")\n").c_str()); // "Error: MultiByteToWideChar (size calculation) failed. Error code: "
        return L"";
    }
    std::wstring wstrTo(size_needed, 0);
    int chars_converted = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstrTo[0], size_needed);
    if (chars_converted <= 0) {
        DWORD error = GetLastError();
        OutputDebugStringA(("错误：MultiByteToWideChar (转换) 失败。错误代码: " + std::to_string(error) + " (" + std::system_category().message(error) + ")\n").c_str()); // "Error: MultiByteToWideChar (conversion) failed. Error code: "
        return L"";
    }
    return wstrTo;
}

// --- JSON String Unescaping Utility ---
// (UnescapeJsonString - unchanged from previous fix)
std::string UnescapeJsonString(const std::string& jsonEscaped) {
    std::stringstream result_ss;
    bool escape = false;
    for (size_t i = 0; i < jsonEscaped.length(); ++i) {
        if (escape) {
            switch (jsonEscaped[i]) {
            case '"':  result_ss << '"'; break;
            case '\\': result_ss << '\\'; break;
            case '/':  result_ss << '/'; break;
            case 'b':  result_ss << '\b'; break;
            case 'f':  result_ss << '\f'; break;
            case 'n':  result_ss << '\n'; break;
            case 'r':  result_ss << '\r'; break;
            case 't':  result_ss << '\t'; break;
            case 'u':
                if (i + 4 < jsonEscaped.length()) {
                    try {
                        std::string hex_str = jsonEscaped.substr(i + 1, 4);
                        if (hex_str.length() != 4 || hex_str.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) { throw std::invalid_argument("Invalid hex sequence format"); }
                        unsigned int uc = std::stoul(hex_str, nullptr, 16);
                        if (uc <= 0x7F) { result_ss << static_cast<char>(uc); }
                        else if (uc <= 0x7FF) { result_ss << static_cast<char>(0xC0 | (uc >> 6)) << static_cast<char>(0x80 | (uc & 0x3F)); }
                        else { result_ss << static_cast<char>(0xE0 | (uc >> 12)) << static_cast<char>(0x80 | ((uc >> 6) & 0x3F)) << static_cast<char>(0x80 | (uc & 0x3F)); }
                        i += 4;
                    }
                    catch (const std::invalid_argument& ia) { OutputDebugStringA(("JSON 反转义错误：无效的 \\u hex 序列在索引 " + std::to_string(i) + ": " + ia.what() + "\n").c_str()); result_ss << "\\u"; } // "JSON Unescape Error: Invalid \\u hex sequence at index "
                    catch (const std::out_of_range& oor) { OutputDebugStringA(("JSON 反转义错误：\\u hex 序列值超出范围在索引 " + std::to_string(i) + ": " + oor.what() + "\n").c_str()); result_ss << "\\u"; } // "JSON Unescape Error: \\u hex sequence value out of range at index "
                    catch (const std::exception& e) { OutputDebugStringA(("JSON 反转义错误：处理 \\u 序列时出错在索引 " + std::to_string(i) + ": " + e.what() + "\n").c_str()); result_ss << "\\u"; } // "JSON Unescape Error: Error processing \\u sequence at index "
                    catch (...) { OutputDebugStringA(("JSON 反转义错误：处理 \\u 序列时发生未知错误在索引 " + std::to_string(i) + "\n").c_str()); result_ss << "\\u"; } // "JSON Unescape Error: Unknown error processing \\u sequence at index "
                }
                else { OutputDebugStringA(("JSON 反转义错误：不完整的 \\u 序列在索引 " + std::to_string(i) + "\n").c_str()); result_ss << "\\u"; } // "JSON Unescape Error: Incomplete \\u sequence at index "
                break;
            default: result_ss << '\\' << jsonEscaped[i]; break;
            }
            escape = false;
        }
        else if (jsonEscaped[i] == '\\') { escape = true; }
        else { result_ss << jsonEscaped[i]; }
    }
    if (escape) { result_ss << '\\'; }
    return result_ss.str();
}

// --- UUID Generation ---
std::wstring GenerateUUIDString() {
    UUID uuid;
    RPC_STATUS status = UuidCreate(&uuid);
    if (status != RPC_S_OK && status != RPC_S_UUID_LOCAL_ONLY && status != RPC_S_UUID_NO_ADDRESS) {
        std::stringstream ss_log;
        ss_log << "错误: UuidCreate 失败，状态 " << status; // "Error: UuidCreate failed, status "
        Log(ss_log.str());
        return L"";
    }
    RPC_WSTR uuidStrW = nullptr;
    status = UuidToStringW(&uuid, &uuidStrW);
    struct RpcStringFreer { RPC_WSTR& str; ~RpcStringFreer() { if (str) RpcStringFreeW(&str); } } freer{ uuidStrW };
    if (status != RPC_S_OK || uuidStrW == nullptr) {
        std::stringstream ss_log;
        ss_log << "错误: UuidToStringW 失败，状态 " << status; // "Error: UuidToStringW failed, status "
        Log(ss_log.str());
        return L"";
    }
    try {
        return std::wstring(reinterpret_cast<wchar_t*>(uuidStrW));
    }
    catch (const std::bad_alloc& ba) {
        Log("错误：为 UUID 字符串分配内存失败: " + std::string(ba.what())); // "Error: Failed to allocate memory for UUID string: "
        return L"";
    }
    catch (...) {
        Log("错误：将 RPC_WSTR 复制到 std::wstring 时发生未知错误。"); // "Error: Unknown error copying RPC_WSTR to std::wstring."
        return L"";
    }
}

// --- Helper Function: Ensure Directory Exists ---
// (EnsureDirectoryExists - unchanged from previous fix)
bool EnsureDirectoryExists(const std::wstring& dirPath) {
    if (dirPath.empty()) { OutputDebugStringW(L"错误：EnsureDirectoryExists 收到空路径。\n"); return false; } // "Error: EnsureDirectoryExists received empty path."
    DWORD fileAttr = GetFileAttributesW(dirPath.c_str());
    if (fileAttr != INVALID_FILE_ATTRIBUTES) {
        if (fileAttr & FILE_ATTRIBUTE_DIRECTORY) { return true; }
        else { OutputDebugStringW((L"错误：路径存在但不是目录: " + dirPath + L"\n").c_str()); return false; } // "Error: Path exists but is not a directory: "
    }
    DWORD lastError = GetLastError();
    if (lastError != ERROR_FILE_NOT_FOUND && lastError != ERROR_PATH_NOT_FOUND) { OutputDebugStringW((L"错误：检查目录属性时出错: " + dirPath + L" GetLastError: " + std::to_wstring(lastError) + L"\n").c_str()); } // "Error: Error checking directory attributes: ... GetLastError: "
    int result = SHCreateDirectoryExW(NULL, dirPath.c_str(), NULL);
    if (result == ERROR_SUCCESS) { return true; }
    else if (result == ERROR_ALREADY_EXISTS) {
        fileAttr = GetFileAttributesW(dirPath.c_str());
        if (fileAttr != INVALID_FILE_ATTRIBUTES && (fileAttr & FILE_ATTRIBUTE_DIRECTORY)) { return true; }
        else { OutputDebugStringW((L"错误：SHCreateDirectoryExW 返回 ERROR_ALREADY_EXISTS 但路径不是目录: " + dirPath + L"\n").c_str()); return false; } // "Error: SHCreateDirectoryExW returned ERROR_ALREADY_EXISTS but path is not a directory: "
    }
    else if (result == ERROR_FILE_EXISTS) {
        OutputDebugStringW((L"错误：无法创建目录，因为同名文件已存在: " + dirPath + L"\n").c_str()); return false; // "Error: Cannot create directory because a file with the same name exists: "
    }
    else {
        lastError = GetLastError(); OutputDebugStringW((L"错误：创建目录失败: " + dirPath + L" SHCreateDirectoryExW 返回: " + std::to_wstring(result) + L" GetLastError: " + std::to_wstring(lastError) + L"\n").c_str()); return false; // "Error: Failed to create directory: ... SHCreateDirectoryExW returned: ... GetLastError: "
    }
}

// --- Helper Function: Get Path to Configurator.exe ---
// (GetConfiguratorPath - unchanged from previous fix)
std::wstring GetConfiguratorPath() {
    wchar_t exePathBuffer[MAX_PATH]; DWORD pathLen = GetModuleFileNameW(NULL, exePathBuffer, MAX_PATH);
    if (pathLen == 0) { DWORD error = GetLastError(); Log("错误：GetModuleFileNameW 失败。无法获取当前应用程序路径。错误代码: " + std::to_string(error) + " (" + std::system_category().message(error) + ")"); return L""; } // "Error: GetModuleFileNameW failed. Cannot get current application path. Error code: "
    if (pathLen == MAX_PATH) { Log("错误：当前应用程序路径可能被截断（达到 MAX_PATH）。"); } // "Error: Current application path might be truncated (reached MAX_PATH)."
    wchar_t* lastBackslash = wcsrchr(exePathBuffer, L'\\');
    if (lastBackslash) {
        *(lastBackslash + 1) = L'\0'; std::wstring directoryPath = exePathBuffer;
        try { return directoryPath + CONFIGURATOR_EXE_NAME; }
        catch (const std::bad_alloc& ba) { Log("错误：为配置程序路径分配内存失败: " + std::string(ba.what())); return L""; } // "Error: Failed to allocate memory for configurator path: "
        catch (...) { Log("错误：组合配置程序路径时发生未知错误。"); return L""; } // "Error: Unknown error combining configurator path."
    }
    else { Log("警告：在当前应用程序路径中未找到反斜杠。假设在当前目录中。路径: " + wstring_to_utf8(exePathBuffer)); return CONFIGURATOR_EXE_NAME; } // "Warning: No backslash found in current application path. Assuming current directory. Path: "
}

// --- Helper Function: Launch Configurator.exe and Wait ---
bool LaunchConfiguratorAndWait() {
    std::wstring configuratorPath = GetConfiguratorPath();
    if (configuratorPath.empty()) { MessageBoxW(g_hWnd, L"无法确定配置程序 (Configurator.exe) 的路径。", L"配置错误", MB_OK | MB_ICONERROR | MB_TOPMOST); return false; } // "Cannot determine path for Configurator.exe." "Configuration Error"
    if (GetFileAttributesW(configuratorPath.c_str()) == INVALID_FILE_ATTRIBUTES) { Log("错误: Configurator.exe 不存在于路径: " + wstring_to_utf8(configuratorPath)); MessageBoxW(g_hWnd, (L"找不到配置程序:\n" + configuratorPath + L"\n\n请确保它与主应用程序在同一目录中。").c_str(), L"配置错误", MB_OK | MB_ICONERROR | MB_TOPMOST); return false; } // "Error: Configurator.exe does not exist at path: " "Cannot find Configurator:\n...\n\nEnsure it's in the same directory..." "Configuration Error"

    std::stringstream ss_log_launch;
    ss_log_launch << "启动配置程序: " << wstring_to_utf8(configuratorPath); // "Launching configurator: "
    Log(ss_log_launch.str());

    STARTUPINFOW si = { sizeof(si) }; PROCESS_INFORMATION pi = { 0 };
    BOOL success = CreateProcessW(configuratorPath.c_str(), NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    FileHandle hProcess(pi.hProcess); FileHandle hThread(pi.hThread);
    if (!success) { DWORD error = GetLastError(); Log("错误：启动 Configurator.exe 失败，错误代码: " + std::to_string(error) + " (" + std::system_category().message(error) + ")"); MessageBoxW(g_hWnd, (L"启动配置程序失败。\n错误代码: " + std::to_wstring(error)).c_str(), L"启动错误", MB_OK | MB_ICONERROR | MB_TOPMOST); return false; } // "Error: Failed to launch Configurator.exe, error code: " "Failed to launch Configurator.\nError code: " "Launch Error"

    std::stringstream ss_log_wait;
    ss_log_wait << "配置程序成功启动 (PID: " << pi.dwProcessId << ")。正在等待其退出..."; // "Configurator launched successfully (PID: ...). Waiting for exit..."
    Log(ss_log_wait.str());

    DWORD waitResult = WaitForSingleObject(hProcess.get(), INFINITE);
    if (waitResult == WAIT_OBJECT_0) {
        Log("配置程序已退出。"); // "Configurator process exited."
        DWORD exitCode = 0;
        if (GetExitCodeProcess(hProcess.get(), &exitCode)) {
            std::stringstream ss_log_exit;
            ss_log_exit << "配置程序退出代码: " << exitCode; // "Configurator exit code: "
            Log(ss_log_exit.str());
        }
        else {
            DWORD error = GetLastError(); Log("警告：无法获取配置程序的退出代码。错误代码: " + std::to_string(error) + " (" + std::system_category().message(error) + ")"); // "Warning: Failed to get configurator exit code. Error code: "
        }
    }
    else {
        // <<< FIX C2676: Use stringstream for Log message construction
        std::stringstream ss_log_wait_err;
        ss_log_wait_err << "错误：等待配置程序进程时出错。WaitForSingleObject 返回: " << waitResult
            << " 错误代码: " << GetLastError() << " (" << std::system_category().message(GetLastError()) << ")"; // "Error: Failed waiting for configurator process. WaitForSingleObject returned: ... Error code: "
        Log(ss_log_wait_err.str());
        return false;
    }
    return true;
}

// --- Version Comparison ---
// (ParseVersion - unchanged from previous fix)
std::vector<int> ParseVersion(const std::wstring& versionStr) {
    std::vector<int> parts; if (versionStr.empty()) { Log("版本解析警告：输入版本字符串为空。"); return parts; } // "Version parse warning: Input version string is empty."
    std::wstringstream ss(versionStr); std::wstring part; int partIndex = 0;
    while (getline(ss, part, L'.')) {
        partIndex++;
        try {
            if (part.empty()) { throw std::invalid_argument("空部分"); } // "Empty part"
            if (part.find_first_not_of(L"0123456789") != std::wstring::npos) { throw std::invalid_argument("非数字字符"); } // "Non-digit characters"
            size_t processedChars = 0; int value = std::stoi(part, &processedChars);
            if (processedChars != part.length()) { throw std::invalid_argument("无效字符（未完全处理）"); } // "Invalid characters (not fully processed)"
            if (value < 0) { throw std::out_of_range("负值"); } // "Negative value"
            parts.push_back(value);
        }
        catch (const std::invalid_argument& ia) {
            // <<< FIX C2676: Use stringstream for Log message construction
            std::stringstream ss_log;
            ss_log << "版本解析错误：在 '" << wstring_to_utf8(versionStr) << "' 的部分 " << partIndex
                << " ('" << wstring_to_utf8(part) << "') 中找到无效字符。错误: " << ia.what(); // "Version parse error: Invalid characters found in part ... of '...' ('...'). Error: "
            Log(ss_log.str());
            return {};
        }
        catch (const std::out_of_range& oor) {
            // <<< FIX C2676: Use stringstream for Log message construction
            std::stringstream ss_log;
            ss_log << "版本解析错误：在 '" << wstring_to_utf8(versionStr) << "' 的部分 " << partIndex
                << " ('" << wstring_to_utf8(part) << "') 中值超出范围。错误: " << oor.what(); // "Version parse error: Value out of range in part ... of '...' ('...'). Error: "
            Log(ss_log.str());
            return {};
        }
        catch (const std::exception& e) {
            // <<< FIX C2676: Use stringstream for Log message construction
            std::stringstream ss_log;
            ss_log << "版本解析错误：解析 '" << wstring_to_utf8(versionStr) << "' 的部分 " << partIndex
                << " ('" << wstring_to_utf8(part) << "') 时发生未知错误。错误: " << e.what(); // "Version parse error: Unknown error parsing part ... of '...' ('...'). Error: "
            Log(ss_log.str());
            return {};
        }
        catch (...) {
            // <<< FIX C2676: Use stringstream for Log message construction
            std::stringstream ss_log;
            ss_log << "版本解析错误：解析 '" << wstring_to_utf8(versionStr) << "' 的部分 " << partIndex << " 时发生未知异常。"; // "Version parse error: Unknown exception parsing part ... of '...'."
            Log(ss_log.str());
            return {};
        }
    }
    if (parts.empty() && !versionStr.empty()) { Log("版本解析错误：无法从 '" + wstring_to_utf8(versionStr) + "' 解析任何有效部分。"); return {}; } // "Version parse error: Could not parse any valid parts from '...'."
    return parts;
}
// (CompareVersions - unchanged from previous fix)
int CompareVersions(const std::vector<int>& v1, const std::vector<int>& v2) {
    if (v1.empty() && v2.empty()) return 0; if (v1.empty()) return -1; if (v2.empty()) return 1; size_t i = 0;
    while (i < v1.size() || i < v2.size()) { int num1 = (i < v1.size()) ? v1[i] : 0; int num2 = (i < v2.size()) ? v2[i] : 0; if (num1 < num2) return -1; if (num1 > num2) return 1; i++; } return 0;
}
