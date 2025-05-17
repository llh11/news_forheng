// update.cpp
// Implements the application update process (reading/writing status files, performing update).
#include "update.h"  // Function declarations for this file
#include "globals.h" // Access to paths (CONFIG_DIR, PENDING_UPDATE_INFO_PATH, etc.), g_hWnd, g_bRunning, PostMessage
#include "log.h"     // For Log function
#include "utils.h"   // For string conversions (wstring_to_utf8, utf8_to_wstring)
#include "ui.h"      // <<< Added include for PostProgressUpdate

#include <Windows.h>
#include <fstream>    // For std::wifstream, std::wofstream, std::ifstream, std::ofstream
#include <sstream>    // For std::wstringstream
#include <string>
#include <shellapi.h> // For ShellExecuteW
#include <vector>     // For std::vector (used in PerformUpdate batch script generation)
#include <system_error> // For std::system_category

// --- Ignored Version File Handling ---
// (ReadIgnoredVersion - unchanged from previous fix)
std::wstring ReadIgnoredVersion() {
    std::wifstream ignoreFile(IGNORED_UPDATE_VERSION_PATH);
    if (ignoreFile.is_open()) {
        std::wstring version;
        if (getline(ignoreFile, version)) {
            size_t first = version.find_first_not_of(L" \t\n\r");
            if (first == std::wstring::npos) return L"";
            size_t last = version.find_last_not_of(L" \t\n\r");
            return version.substr(first, (last - first + 1));
        }
    }
    return L"";
}
// (WriteIgnoredVersion - unchanged from previous fix)
void WriteIgnoredVersion(const std::wstring& version) {
    if (version.empty()) { Log("错误：尝试写入空的忽略版本。"); return; } // "Error: Attempting to write empty ignored version."
    std::wofstream ignoreFile(IGNORED_UPDATE_VERSION_PATH);
    if (ignoreFile.is_open()) {
        ignoreFile << version;
        ignoreFile.close();
        if (!ignoreFile.good()) { Log("错误：写入忽略版本文件时出错 (" + wstring_to_utf8(IGNORED_UPDATE_VERSION_PATH) + ")。"); } // "Error: Failed writing ignored version file (...)."
        else { Log("已忽略版本: " + wstring_to_utf8(version)); } // "Ignoring version: "
    }
    else { Log("错误：打开忽略版本文件进行写入时出错 (" + wstring_to_utf8(IGNORED_UPDATE_VERSION_PATH) + ")。"); } // "Error: Failed opening ignored version file for writing (...)."
}
// (ClearIgnoredVersion - unchanged from previous fix)
void ClearIgnoredVersion() {
    if (DeleteFileW(IGNORED_UPDATE_VERSION_PATH.c_str())) { Log("已清除忽略的版本信息。"); } // "Cleared ignored version info."
    else { DWORD error = GetLastError(); if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND) { Log("警告：清除忽略版本文件失败 (" + wstring_to_utf8(IGNORED_UPDATE_VERSION_PATH) + ")。错误代码: " + std::to_string(error)); } } // "Warning: Failed to clear ignored version file (...). Error Code: "
}

// --- Pending Update Info File Handling ---
// (SavePendingUpdateInfo - unchanged from previous fix)
void SavePendingUpdateInfo(const std::wstring& filePath) {
    if (filePath.empty()) { Log("错误：尝试保存空的待处理更新文件路径。"); return; } // "Error: Attempting to save empty pending update file path."
    std::ofstream infoFile(PENDING_UPDATE_INFO_PATH, std::ios::binary | std::ios::trunc);
    if (infoFile.is_open()) {
        std::string pathUtf8 = wstring_to_utf8(filePath);
        if (pathUtf8.empty() && !filePath.empty()) { Log("错误：无法将待处理更新路径转换为 UTF-8。"); return; } // "Error: Failed to convert pending update path to UTF-8."
        infoFile.write(pathUtf8.c_str(), pathUtf8.length());
        infoFile.close();
        if (!infoFile.good()) { Log("错误：写入待处理更新信息文件时出错 (" + wstring_to_utf8(PENDING_UPDATE_INFO_PATH) + ")。"); } // "Error: Failed writing pending update info file (...)."
        else { Log("已保存待处理更新信息，路径: " + pathUtf8); } // "Pending update info saved for path: "
    }
    else { Log("错误：打开待处理更新信息文件进行写入时出错 (" + wstring_to_utf8(PENDING_UPDATE_INFO_PATH) + ")。"); } // "Error: Failed opening pending update info file for writing (...)."
}
// (ClearPendingUpdateInfo - unchanged from previous fix)
void ClearPendingUpdateInfo() {
    if (DeleteFileW(PENDING_UPDATE_INFO_PATH.c_str())) { Log("已清除待处理更新信息文件。"); } // "Cleared pending update info file."
    else { DWORD error = GetLastError(); if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND) { Log("警告：清除待处理更新信息文件失败 (" + wstring_to_utf8(PENDING_UPDATE_INFO_PATH) + ")。错误代码: " + std::to_string(error)); } } // "Warning: Failed to clear pending update info file (...). Error Code: "
}
// (CheckAndApplyPendingUpdate - unchanged from previous fix)
bool CheckAndApplyPendingUpdate() {
    Log("正在检查待处理更新..."); // "Checking for pending update..."
    std::ifstream infoFile(PENDING_UPDATE_INFO_PATH, std::ios::binary | std::ios::ate);
    if (!infoFile.is_open()) { return false; }
    std::streamsize size = infoFile.tellg();
    if (size <= 0) { infoFile.close(); Log("待处理更新信息文件为空。正在清除。"); ClearPendingUpdateInfo(); return false; } // "Pending update info file is empty. Clearing."
    infoFile.seekg(0, std::ios::beg);
    std::string pathUtf8;
    try { pathUtf8.resize(static_cast<size_t>(size)); }
    catch (const std::bad_alloc&) { Log("错误：为待处理更新路径分配内存失败。"); infoFile.close(); ClearPendingUpdateInfo(); return false; } // "Error: Failed to allocate memory for pending update path."
    if (!infoFile.read(&pathUtf8[0], size)) { infoFile.close(); Log("错误：读取待处理更新信息文件内容失败。正在清除信息。"); ClearPendingUpdateInfo(); return false; } // "Error: Failed to read pending update info file content. Clearing info."
    infoFile.close();
    std::wstring downloadedFilePath = utf8_to_wstring(pathUtf8);
    if (downloadedFilePath.empty() && !pathUtf8.empty()) { Log("错误：无法将待处理更新路径从 UTF-8 转换。正在清除信息。"); ClearPendingUpdateInfo(); return false; } // "Error: Failed to convert pending update path from UTF-8. Clearing info."
    if (GetFileAttributesW(downloadedFilePath.c_str()) == INVALID_FILE_ATTRIBUTES) { Log("待处理更新引用的文件丢失: " + pathUtf8 + ". 正在清除信息。"); ClearPendingUpdateInfo(); return false; } // "Pending update referenced file is missing: ... Clearing info."
    Log("检测到待处理更新文件: " + pathUtf8 + ". 正在提示用户。"); // "Pending update file detected: ... Prompting user."
    std::wstringstream wss; wss << L"先前已下载更新并计划在启动时安装:\n\n" << L"文件: " << downloadedFilePath << L"\n\n" << L"您想现在安装此更新吗？\n" << L"（选择“否”将删除下载的更新文件）"; // "An update was previously downloaded...\n\nFile: ...\n\nInstall now?\n('No' deletes downloaded file)"
    int userChoice = MessageBoxW(nullptr, wss.str().c_str(), L"应用待处理更新", MB_YESNO | MB_ICONQUESTION | MB_TOPMOST | MB_SETFOREGROUND); // "Apply Pending Update"
    if (userChoice == IDYES) {
        Log("用户选择“是”以应用待处理更新。"); // "User chose YES to apply pending update."
        try { PerformUpdate(downloadedFilePath); Log("错误：PerformUpdate 函数返回，表示更新未能启动。"); ClearPendingUpdateInfo(); MessageBoxW(nullptr, L"启动更新过程失败。请重试或手动更新。", L"更新失败", MB_OK | MB_ICONERROR | MB_TOPMOST); return true; } // "Error: PerformUpdate function returned...""Failed to start update process...""Update Failed"
        catch (const std::exception& e) { Log("执行 PerformUpdate 时发生异常: " + std::string(e.what())); ClearPendingUpdateInfo(); MessageBoxW(nullptr, (L"应用更新时出错: " + utf8_to_wstring(e.what())).c_str(), L"更新失败", MB_OK | MB_ICONERROR | MB_TOPMOST); return true; } // "Exception during PerformUpdate: ...""Error applying update: ...""Update Failed"
        catch (...) { Log("执行 PerformUpdate 时发生未知异常。"); ClearPendingUpdateInfo(); MessageBoxW(nullptr, L"应用更新时发生未知错误。", L"更新失败", MB_OK | MB_ICONERROR | MB_TOPMOST); return true; } // "Unknown exception during PerformUpdate.""An unknown error occurred...""Update Failed"
    }
    else {
        Log("用户选择“否”以应用待处理更新或对话框失败。正在删除下载的文件和信息。"); // "User chose NO to apply pending update or dialog failed. Deleting downloaded file and info."
        if (!DeleteFileW(downloadedFilePath.c_str())) { DWORD error = GetLastError(); if (error != ERROR_FILE_NOT_FOUND) { Log("警告：删除待处理更新文件失败 (" + pathUtf8 + ")。错误代码: " + std::to_string(error)); } } // "Warning: Failed to delete pending update file (...). Error Code: "
        ClearPendingUpdateInfo(); return false;
    }
}


// Performs the application update using a self-deleting batch script.
void PerformUpdate(const std::wstring& downloadedFilePath) {
    Log("开始更新过程..."); // "Starting update process..."

    wchar_t currentExePathArr[MAX_PATH] = { 0 };
    DWORD pathLen = GetModuleFileNameW(nullptr, currentExePathArr, MAX_PATH);
    if (pathLen == 0 || pathLen == MAX_PATH) {
        DWORD error = GetLastError();
        Log("错误：无法获取当前应用程序路径以执行更新。GetModuleFileNameW 失败或路径过长。错误代码: " + std::to_string(error)); // "Error: Cannot get current app path for update. GetModuleFileNameW failed or path too long. Error code: "
        PostProgressUpdate(g_hProgressDlg, 0, L"更新失败：无法获取当前路径"); // "Update failed: Cannot get current path"
        return;
    }
    std::wstring currentExePath(currentExePathArr);
    std::wstring currentExeName = currentExePath.substr(currentExePath.find_last_of(L"\\/") + 1);
    Log("当前应用程序路径: " + wstring_to_utf8(currentExePath)); // "Current app path: "
    Log("下载的文件路径: " + wstring_to_utf8(downloadedFilePath)); // "Downloaded file path: "
    std::wstring batFilePath = CONFIG_DIR + L"update_" + std::to_wstring(GetCurrentProcessId()) + L".bat";

    std::ofstream batFile(batFilePath, std::ios::binary | std::ios::trunc);
    if (!batFile.is_open()) {
        Log("错误：无法创建更新脚本: " + wstring_to_utf8(batFilePath)); // "Error: Cannot create update script: "
        PostProgressUpdate(g_hProgressDlg, 0, L"更新失败：无法创建脚本"); // "Update failed: Cannot create script"
        return;
    }

    std::vector<std::string> commands;
    commands.push_back("@echo off");
    commands.push_back("chcp 65001 > nul");
    commands.push_back("echo 更新程序：正在等待旧应用程序关闭..."); // "Updater: Waiting for old application to close..."
    commands.push_back("timeout /t 4 /nobreak > nul");
    commands.push_back("echo 更新程序：正在终止旧进程（如果仍在运行）..."); // "Updater: Terminating old process (if still running)..."
    commands.push_back("taskkill /F /IM \"" + wstring_to_utf8(currentExeName) + "\" > nul 2>&1");
    commands.push_back("timeout /t 1 /nobreak > nul");
    commands.push_back("echo 更新程序：正在删除旧文件..."); // "Updater: Deleting old file..."
    commands.push_back("del \"" + wstring_to_utf8(currentExePath) + "\"");
    commands.push_back("echo 更新程序：正在将新文件移动到位..."); // "Updater: Moving new file into place..."
    commands.push_back("move /Y \"" + wstring_to_utf8(downloadedFilePath) + "\" \"" + wstring_to_utf8(currentExePath) + "\"");
    commands.push_back("echo 更新程序：正在启动更新后的应用程序..."); // "Updater: Starting updated application..."
    commands.push_back("start \"\" \"" + wstring_to_utf8(currentExePath) + "\"");
    commands.push_back("echo 更新程序：正在清理更新脚本..."); // "Updater: Cleaning up update script..."
    commands.push_back("del \"%~f0\"");
    for (const auto& cmd : commands) { batFile << cmd << "\r\n"; }
    batFile.close();

    if (!batFile.good()) {
        Log("错误：写入更新脚本时出错。"); // "Error: Error writing update script."
        DeleteFileW(batFilePath.c_str());
        PostProgressUpdate(g_hProgressDlg, 0, L"更新失败：写入脚本错误"); // "Update failed: Error writing script"
        return;
    }

    Log("更新脚本已创建: " + wstring_to_utf8(batFilePath)); // "Update script created: "
    Log("正在启动更新脚本并退出应用程序..."); // "Launching update script and exiting application..."

    PostProgressUpdate(g_hProgressDlg, 100, L"应用更新..."); // "Applying update..."
    Sleep(100);

    HINSTANCE result = ShellExecuteW(nullptr, L"open", batFilePath.c_str(), nullptr, CONFIG_DIR.c_str(), SW_HIDE);

    if (reinterpret_cast<INT_PTR>(result) > 32) {
        Log("更新脚本成功启动。应用程序现在将退出。"); // "Update script launched successfully. Application will now exit."
        g_bRunning = false;
        if (g_hWnd) { PostMessage(g_hWnd, WM_CLOSE, 0, 0); }
        else { exit(0); }
    }
    else {
        DWORD error = GetLastError();
        Log("错误：启动更新脚本失败。ShellExecuteW 结果: " + std::to_string(reinterpret_cast<uintptr_t>(result)) + ", GetLastError: " + std::to_string(error) + " (" + std::system_category().message(error) + ")"); // "Error: Failed to launch update script. ShellExecuteW result: ..., GetLastError: ..."
        DeleteFileW(batFilePath.c_str());
        PostProgressUpdate(g_hProgressDlg, 0, L"启动更新脚本失败"); // "Failed to start update script"
    }
}
