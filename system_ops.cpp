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
    if (version.empty()) { Log("���󣺳���д��յĺ��԰汾��"); return; } // "Error: Attempting to write empty ignored version."
    std::wofstream ignoreFile(IGNORED_UPDATE_VERSION_PATH);
    if (ignoreFile.is_open()) {
        ignoreFile << version;
        ignoreFile.close();
        if (!ignoreFile.good()) { Log("����д����԰汾�ļ�ʱ���� (" + wstring_to_utf8(IGNORED_UPDATE_VERSION_PATH) + ")��"); } // "Error: Failed writing ignored version file (...)."
        else { Log("�Ѻ��԰汾: " + wstring_to_utf8(version)); } // "Ignoring version: "
    }
    else { Log("���󣺴򿪺��԰汾�ļ�����д��ʱ���� (" + wstring_to_utf8(IGNORED_UPDATE_VERSION_PATH) + ")��"); } // "Error: Failed opening ignored version file for writing (...)."
}
// (ClearIgnoredVersion - unchanged from previous fix)
void ClearIgnoredVersion() {
    if (DeleteFileW(IGNORED_UPDATE_VERSION_PATH.c_str())) { Log("��������Եİ汾��Ϣ��"); } // "Cleared ignored version info."
    else { DWORD error = GetLastError(); if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND) { Log("���棺������԰汾�ļ�ʧ�� (" + wstring_to_utf8(IGNORED_UPDATE_VERSION_PATH) + ")���������: " + std::to_string(error)); } } // "Warning: Failed to clear ignored version file (...). Error Code: "
}

// --- Pending Update Info File Handling ---
// (SavePendingUpdateInfo - unchanged from previous fix)
void SavePendingUpdateInfo(const std::wstring& filePath) {
    if (filePath.empty()) { Log("���󣺳��Ա���յĴ���������ļ�·����"); return; } // "Error: Attempting to save empty pending update file path."
    std::ofstream infoFile(PENDING_UPDATE_INFO_PATH, std::ios::binary | std::ios::trunc);
    if (infoFile.is_open()) {
        std::string pathUtf8 = wstring_to_utf8(filePath);
        if (pathUtf8.empty() && !filePath.empty()) { Log("�����޷������������·��ת��Ϊ UTF-8��"); return; } // "Error: Failed to convert pending update path to UTF-8."
        infoFile.write(pathUtf8.c_str(), pathUtf8.length());
        infoFile.close();
        if (!infoFile.good()) { Log("����д������������Ϣ�ļ�ʱ���� (" + wstring_to_utf8(PENDING_UPDATE_INFO_PATH) + ")��"); } // "Error: Failed writing pending update info file (...)."
        else { Log("�ѱ�������������Ϣ��·��: " + pathUtf8); } // "Pending update info saved for path: "
    }
    else { Log("���󣺴򿪴����������Ϣ�ļ�����д��ʱ���� (" + wstring_to_utf8(PENDING_UPDATE_INFO_PATH) + ")��"); } // "Error: Failed opening pending update info file for writing (...)."
}
// (ClearPendingUpdateInfo - unchanged from previous fix)
void ClearPendingUpdateInfo() {
    if (DeleteFileW(PENDING_UPDATE_INFO_PATH.c_str())) { Log("����������������Ϣ�ļ���"); } // "Cleared pending update info file."
    else { DWORD error = GetLastError(); if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND) { Log("���棺��������������Ϣ�ļ�ʧ�� (" + wstring_to_utf8(PENDING_UPDATE_INFO_PATH) + ")���������: " + std::to_string(error)); } } // "Warning: Failed to clear pending update info file (...). Error Code: "
}
// (CheckAndApplyPendingUpdate - unchanged from previous fix)
bool CheckAndApplyPendingUpdate() {
    Log("���ڼ����������..."); // "Checking for pending update..."
    std::ifstream infoFile(PENDING_UPDATE_INFO_PATH, std::ios::binary | std::ios::ate);
    if (!infoFile.is_open()) { return false; }
    std::streamsize size = infoFile.tellg();
    if (size <= 0) { infoFile.close(); Log("�����������Ϣ�ļ�Ϊ�ա����������"); ClearPendingUpdateInfo(); return false; } // "Pending update info file is empty. Clearing."
    infoFile.seekg(0, std::ios::beg);
    std::string pathUtf8;
    try { pathUtf8.resize(static_cast<size_t>(size)); }
    catch (const std::bad_alloc&) { Log("����Ϊ���������·�������ڴ�ʧ�ܡ�"); infoFile.close(); ClearPendingUpdateInfo(); return false; } // "Error: Failed to allocate memory for pending update path."
    if (!infoFile.read(&pathUtf8[0], size)) { infoFile.close(); Log("���󣺶�ȡ�����������Ϣ�ļ�����ʧ�ܡ����������Ϣ��"); ClearPendingUpdateInfo(); return false; } // "Error: Failed to read pending update info file content. Clearing info."
    infoFile.close();
    std::wstring downloadedFilePath = utf8_to_wstring(pathUtf8);
    if (downloadedFilePath.empty() && !pathUtf8.empty()) { Log("�����޷������������·���� UTF-8 ת�������������Ϣ��"); ClearPendingUpdateInfo(); return false; } // "Error: Failed to convert pending update path from UTF-8. Clearing info."
    if (GetFileAttributesW(downloadedFilePath.c_str()) == INVALID_FILE_ATTRIBUTES) { Log("������������õ��ļ���ʧ: " + pathUtf8 + ". ���������Ϣ��"); ClearPendingUpdateInfo(); return false; } // "Pending update referenced file is missing: ... Clearing info."
    Log("��⵽����������ļ�: " + pathUtf8 + ". ������ʾ�û���"); // "Pending update file detected: ... Prompting user."
    std::wstringstream wss; wss << L"��ǰ�����ظ��²��ƻ�������ʱ��װ:\n\n" << L"�ļ�: " << downloadedFilePath << L"\n\n" << L"�������ڰ�װ�˸�����\n" << L"��ѡ�񡰷񡱽�ɾ�����صĸ����ļ���"; // "An update was previously downloaded...\n\nFile: ...\n\nInstall now?\n('No' deletes downloaded file)"
    int userChoice = MessageBoxW(nullptr, wss.str().c_str(), L"Ӧ�ô��������", MB_YESNO | MB_ICONQUESTION | MB_TOPMOST | MB_SETFOREGROUND); // "Apply Pending Update"
    if (userChoice == IDYES) {
        Log("�û�ѡ���ǡ���Ӧ�ô�������¡�"); // "User chose YES to apply pending update."
        try { PerformUpdate(downloadedFilePath); Log("����PerformUpdate �������أ���ʾ����δ��������"); ClearPendingUpdateInfo(); MessageBoxW(nullptr, L"�������¹���ʧ�ܡ������Ի��ֶ����¡�", L"����ʧ��", MB_OK | MB_ICONERROR | MB_TOPMOST); return true; } // "Error: PerformUpdate function returned...""Failed to start update process...""Update Failed"
        catch (const std::exception& e) { Log("ִ�� PerformUpdate ʱ�����쳣: " + std::string(e.what())); ClearPendingUpdateInfo(); MessageBoxW(nullptr, (L"Ӧ�ø���ʱ����: " + utf8_to_wstring(e.what())).c_str(), L"����ʧ��", MB_OK | MB_ICONERROR | MB_TOPMOST); return true; } // "Exception during PerformUpdate: ...""Error applying update: ...""Update Failed"
        catch (...) { Log("ִ�� PerformUpdate ʱ����δ֪�쳣��"); ClearPendingUpdateInfo(); MessageBoxW(nullptr, L"Ӧ�ø���ʱ����δ֪����", L"����ʧ��", MB_OK | MB_ICONERROR | MB_TOPMOST); return true; } // "Unknown exception during PerformUpdate.""An unknown error occurred...""Update Failed"
    }
    else {
        Log("�û�ѡ�񡰷���Ӧ�ô�������»�Ի���ʧ�ܡ�����ɾ�����ص��ļ�����Ϣ��"); // "User chose NO to apply pending update or dialog failed. Deleting downloaded file and info."
        if (!DeleteFileW(downloadedFilePath.c_str())) { DWORD error = GetLastError(); if (error != ERROR_FILE_NOT_FOUND) { Log("���棺ɾ������������ļ�ʧ�� (" + pathUtf8 + ")���������: " + std::to_string(error)); } } // "Warning: Failed to delete pending update file (...). Error Code: "
        ClearPendingUpdateInfo(); return false;
    }
}


// Performs the application update using a self-deleting batch script.
void PerformUpdate(const std::wstring& downloadedFilePath) {
    Log("��ʼ���¹���..."); // "Starting update process..."

    wchar_t currentExePathArr[MAX_PATH] = { 0 };
    DWORD pathLen = GetModuleFileNameW(nullptr, currentExePathArr, MAX_PATH);
    if (pathLen == 0 || pathLen == MAX_PATH) {
        DWORD error = GetLastError();
        Log("�����޷���ȡ��ǰӦ�ó���·����ִ�и��¡�GetModuleFileNameW ʧ�ܻ�·���������������: " + std::to_string(error)); // "Error: Cannot get current app path for update. GetModuleFileNameW failed or path too long. Error code: "
        PostProgressUpdate(g_hProgressDlg, 0, L"����ʧ�ܣ��޷���ȡ��ǰ·��"); // "Update failed: Cannot get current path"
        return;
    }
    std::wstring currentExePath(currentExePathArr);
    std::wstring currentExeName = currentExePath.substr(currentExePath.find_last_of(L"\\/") + 1);
    Log("��ǰӦ�ó���·��: " + wstring_to_utf8(currentExePath)); // "Current app path: "
    Log("���ص��ļ�·��: " + wstring_to_utf8(downloadedFilePath)); // "Downloaded file path: "
    std::wstring batFilePath = CONFIG_DIR + L"update_" + std::to_wstring(GetCurrentProcessId()) + L".bat";

    std::ofstream batFile(batFilePath, std::ios::binary | std::ios::trunc);
    if (!batFile.is_open()) {
        Log("�����޷��������½ű�: " + wstring_to_utf8(batFilePath)); // "Error: Cannot create update script: "
        PostProgressUpdate(g_hProgressDlg, 0, L"����ʧ�ܣ��޷������ű�"); // "Update failed: Cannot create script"
        return;
    }

    std::vector<std::string> commands;
    commands.push_back("@echo off");
    commands.push_back("chcp 65001 > nul");
    commands.push_back("echo ���³������ڵȴ���Ӧ�ó���ر�..."); // "Updater: Waiting for old application to close..."
    commands.push_back("timeout /t 4 /nobreak > nul");
    commands.push_back("echo ���³���������ֹ�ɽ��̣�����������У�..."); // "Updater: Terminating old process (if still running)..."
    commands.push_back("taskkill /F /IM \"" + wstring_to_utf8(currentExeName) + "\" > nul 2>&1");
    commands.push_back("timeout /t 1 /nobreak > nul");
    commands.push_back("echo ���³�������ɾ�����ļ�..."); // "Updater: Deleting old file..."
    commands.push_back("del \"" + wstring_to_utf8(currentExePath) + "\"");
    commands.push_back("echo ���³������ڽ����ļ��ƶ���λ..."); // "Updater: Moving new file into place..."
    commands.push_back("move /Y \"" + wstring_to_utf8(downloadedFilePath) + "\" \"" + wstring_to_utf8(currentExePath) + "\"");
    commands.push_back("echo ���³��������������º��Ӧ�ó���..."); // "Updater: Starting updated application..."
    commands.push_back("start \"\" \"" + wstring_to_utf8(currentExePath) + "\"");
    commands.push_back("echo ���³�������������½ű�..."); // "Updater: Cleaning up update script..."
    commands.push_back("del \"%~f0\"");
    for (const auto& cmd : commands) { batFile << cmd << "\r\n"; }
    batFile.close();

    if (!batFile.good()) {
        Log("����д����½ű�ʱ����"); // "Error: Error writing update script."
        DeleteFileW(batFilePath.c_str());
        PostProgressUpdate(g_hProgressDlg, 0, L"����ʧ�ܣ�д��ű�����"); // "Update failed: Error writing script"
        return;
    }

    Log("���½ű��Ѵ���: " + wstring_to_utf8(batFilePath)); // "Update script created: "
    Log("�����������½ű����˳�Ӧ�ó���..."); // "Launching update script and exiting application..."

    PostProgressUpdate(g_hProgressDlg, 100, L"Ӧ�ø���..."); // "Applying update..."
    Sleep(100);

    HINSTANCE result = ShellExecuteW(nullptr, L"open", batFilePath.c_str(), nullptr, CONFIG_DIR.c_str(), SW_HIDE);

    if (reinterpret_cast<INT_PTR>(result) > 32) {
        Log("���½ű��ɹ�������Ӧ�ó������ڽ��˳���"); // "Update script launched successfully. Application will now exit."
        g_bRunning = false;
        if (g_hWnd) { PostMessage(g_hWnd, WM_CLOSE, 0, 0); }
        else { exit(0); }
    }
    else {
        DWORD error = GetLastError();
        Log("�����������½ű�ʧ�ܡ�ShellExecuteW ���: " + std::to_string(reinterpret_cast<uintptr_t>(result)) + ", GetLastError: " + std::to_string(error) + " (" + std::system_category().message(error) + ")"); // "Error: Failed to launch update script. ShellExecuteW result: ..., GetLastError: ..."
        DeleteFileW(batFilePath.c_str());
        PostProgressUpdate(g_hProgressDlg, 0, L"�������½ű�ʧ��"); // "Failed to start update script"
    }
}
