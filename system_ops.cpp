#include "system_ops.h"
#include "log.h"
#include "utils.h" // For GetExecutablePath
#include "registry.h" // For startup operations

#include <shellapi.h> // For ShellExecuteExW
#include <VersionHelpers.h> // For IsWindowsXPOrGreater etc. (if needed, or use RtlGetVersion)
#include <securitybaseapi.h> // For GetTokenInformation
#include <processthreadsapi.h> // For OpenProcessToken

namespace SystemOps {

    bool RestartAsAdmin() {
        wchar_t szPath[MAX_PATH];
        if (GetModuleFileNameW(NULL, szPath, MAX_PATH) == 0) {
            LOG_ERROR(L"Failed to get module file name for restart. Error: ", GetLastError());
            return false;
        }

        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.lpVerb = L"runas"; // �������ԱȨ��
        sei.lpFile = szPath;
        sei.hwnd = NULL;
        sei.nShow = SW_NORMAL; // ���� SW_SHOWNORMAL

        if (!ShellExecuteExW(&sei)) {
            DWORD dwError = GetLastError();
            if (dwError == ERROR_CANCELLED) {
                // �û�ȡ���� UAC ��ʾ
                LOG_INFO(L"User cancelled UAC prompt for restart as admin.");
            }
            else {
                LOG_ERROR(L"ShellExecuteExW failed to restart as admin. Error: ", dwError);
            }
            return false;
        }
        // ��� ShellExecuteExW �ɹ�����ǰ���̿����˳��ˣ���Ϊ�½����Ѿ�����
        // PostQuitMessage(0); // or some other way to signal application exit
        return true;
    }

    bool IsRunningAsAdmin() {
        BOOL fIsAdmin = FALSE;
        HANDLE hToken = NULL;
        TOKEN_ELEVATION elevation;
        DWORD dwSize;

        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
            LOG_WARNING(L"Failed to open process token. Error: ", GetLastError());
            return false; // ����ȷ�������ط��� false
        }

        if (GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &dwSize)) {
            fIsAdmin = elevation.TokenIsElevated;
        }
        else {
            LOG_WARNING(L"Failed to get token elevation information. Error: ", GetLastError());
            // ��ĳЩ����� (���� UAC �ر�)��TokenElevation ���ܲ����á�
            // ��ʱ���Գ��Լ�����Ա SID��
            // SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
            // PSID pAdminGroup = NULL;
            // if (AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &pAdminGroup)) {
            //     if (!CheckTokenMembership(NULL, pAdminGroup, &fIsAdmin)) {
            //         fIsAdmin = FALSE; // Failed, assume not admin
            //     }
            //     FreeSid(pAdminGroup);
            // }
        }

        if (hToken) {
            CloseHandle(hToken);
        }
        return fIsAdmin == TRUE;
    }


    bool ExecuteProcess(
        const std::wstring& command,
        const std::wstring& params,
        const std::wstring& workingDirectory,
        bool waitForCompletion,
        PROCESS_INFORMATION* outProcessInfo,
        bool hidden)
    {
        STARTUPINFOW si;
        PROCESS_INFORMATION pi;

        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        if (hidden) {
            si.dwFlags |= STARTF_USESHOWWINDOW;
            si.wShowWindow = SW_HIDE;
        }
        ZeroMemory(&pi, sizeof(pi));

        std::wstring fullCommandLine = L"\"" + command + L"\" " + params;
        // CreateProcessW �޸������в�����������Ҫһ����д�� buffer
        std::vector<wchar_t> cmdLineVec(fullCommandLine.begin(), fullCommandLine.end());
        cmdLineVec.push_back(L'\0'); // Null-terminate

        const wchar_t* lpWorkingDirectory = workingDirectory.empty() ? NULL : workingDirectory.c_str();

        if (!CreateProcessW(
            NULL,               // lpApplicationName - use command line to specify executable
            cmdLineVec.data(),  // lpCommandLine
            NULL,               // lpProcessAttributes
            NULL,               // lpThreadAttributes
            FALSE,              // bInheritHandles
            hidden ? CREATE_NO_WINDOW : 0, // dwCreationFlags
            NULL,               // lpEnvironment
            lpWorkingDirectory, // lpCurrentDirectory
            &si,                // lpStartupInfo
            &pi                 // lpProcessInformation
        ))
        {
            LOG_ERROR(L"CreateProcess failed for command: ", command, L" Error: ", GetLastError());
            return false;
        }

        LOG_INFO(L"Process created. PID: ", pi.dwProcessId, L" Command: ", command);

        if (outProcessInfo) {
            *outProcessInfo = pi; // �����������Ϣ
        }

        if (waitForCompletion) {
            WaitForSingleObject(pi.hProcess, INFINITE);
            // �����Ҫ��ȡ�˳���:
            // DWORD exitCode = 0;
            // if (GetExitCodeProcess(pi.hProcess, &exitCode)) {
            //     LOG_INFO(L"Process ", pi.dwProcessId, L" exited with code: ", exitCode);
            // }
        }

        // ��������߲���Ҫ PROCESS_INFORMATION (���� !waitForCompletion && !outProcessInfo)
        // ���� waitForCompletion ��Ӧ�ùرվ��
        if (!outProcessInfo || waitForCompletion) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
        // ��� outProcessInfo ����䣬�����߸���ر� pi.hProcess �� pi.hThread

        return true;
    }


    std::wstring GetErrorMessage(DWORD errorCode) {
        LPVOID lpMsgBuf = nullptr;
        FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            errorCode,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPWSTR)&lpMsgBuf,
            0, NULL);

        std::wstring message = L"Unknown error";
        if (lpMsgBuf) {
            message = (LPWSTR)lpMsgBuf;
            LocalFree(lpMsgBuf);
            // ȥ��ĩβ�Ļ��з�
            if (!message.empty() && (message.back() == L'\n' || message.back() == L'\r')) {
                message.pop_back();
            }
            if (!message.empty() && (message.back() == L'\n' || message.back() == L'\r')) {
                message.pop_back();
            }
        }
        return message + L" (Code: " + std::to_wstring(errorCode) + L")";
    }

    std::wstring GetOSVersion() {
        // ʹ�� RtlGetVersion �ǻ�ȡ OS �汾��Ϣ���Ƽ���ʽ
        // ��Ҫ���� Ntdll.lib ���߶�̬����
        // typedef LONG (WINAPI *RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
        // RTL_OSVERSIONINFOW osvi = {0};
        // osvi.dwOSVersionInfoSize = sizeof(osvi);
        // HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
        // if (hNtdll) {
        //     RtlGetVersionPtr pRtlGetVersion = (RtlGetVersionPtr)GetProcAddress(hNtdll, "RtlGetVersion");
        //     if (pRtlGetVersion && pRtlGetVersion(&osvi) == 0 /* STATUS_SUCCESS */) {
        //         std::wstringstream wss;
        //         wss << L"Windows " << osvi.dwMajorVersion << L"." << osvi.dwMinorVersion
        //             << L" (Build " << osvi.dwBuildNumber << L")";
        //         if (osvi.szCSDVersion[0]) {
        //             wss << L" " << osvi.szCSDVersion;
        //         }
        //         return wss.str();
        //     }
        // }
        //
        // // ���˵� GetVersionEx (���Ƽ����� Win8.1 �����߰汾����Ϊ���ܷ��ز�׼ȷ��ֵ�������� manifest)
        // OSVERSIONINFOEXW osviEx;
        // ZeroMemory(&osviEx, sizeof(OSVERSIONINFOEXW));
        // osviEx.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXW);
        // #pragma warning(suppress : 4996) // ���� GetVersionExW �����þ���
        // if (!GetVersionExW((OSVERSIONINFOW*)&osviEx)) {
        //     return L"Unknown OS (GetVersionEx failed)";
        // }
        // std::wstringstream wss;
        // wss << L"Windows " << osviEx.dwMajorVersion << L"." << osviEx.dwMinorVersion
        //     << L" (Build " << osviEx.dwBuildNumber << L") SP " << osviEx.wServicePackMajor
        //     << L"." << osviEx.wServicePackMinor;
        // return wss.str();

        // ���򵥵ķ�ʽ�����ֻ��Ҫ�����жϣ�����ʹ�� VersionHelpers.h
        // ���ⲻֱ�ӷ����ַ����������ṩ IsWindowsXYOrGreater ϵ�к���

        // ��ʱ����һ��ͨ����ʾ��ʵ��Ӧ����Ӧʵ����������֮һ
        return L"Windows (Version detection not fully implemented)";
    }


    bool AddToStartup(const std::wstring& appName, const std::wstring& appPath, bool forCurrentUser) {
        HKEY rootKey = forCurrentUser ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
        std::wstring regPath = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run";

        std::wstring command = L"\"" + appPath + L"\""; // ȷ��·�����������������Է��пո�

        if (Registry::WriteString(rootKey, regPath, appName, command)) {
            LOG_INFO(L"Added '", appName, L"' to startup (", (forCurrentUser ? L"Current User" : L"All Users"), L"). Path: ", command);
            return true;
        }
        else {
            LOG_ERROR(L"Failed to add '", appName, L"' to startup. Ensure correct permissions if for All Users.");
            return false;
        }
    }

    bool RemoveFromStartup(const std::wstring& appName, bool forCurrentUser) {
        HKEY rootKey = forCurrentUser ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
        std::wstring regPath = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run";

        if (Registry::ValueExists(rootKey, regPath, appName)) {
            if (Registry::DeleteValue(rootKey, regPath, appName)) {
                LOG_INFO(L"Removed '", appName, L"' from startup (", (forCurrentUser ? L"Current User" : L"All Users"), L").");
                return true;
            }
            else {
                LOG_ERROR(L"Failed to remove '", appName, L"' from startup. Ensure correct permissions if for All Users.");
                return false;
            }
        }
        else {
            LOG_INFO(L"'", appName, L"' not found in startup (", (forCurrentUser ? L"Current User" : L"All Users"), L"). No action taken.");
            return true; // ֵ�����ڣ�Ҳ��ɹ��Ƴ�
        }
    }

    bool IsInStartup(const std::wstring& appName, bool forCurrentUser) {
        HKEY rootKey = forCurrentUser ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
        std::wstring regPath = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run";
        return Registry::ValueExists(rootKey, regPath, appName);
    }


} // namespace SystemOps
