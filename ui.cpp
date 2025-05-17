// ui.cpp
// Implements user interface functions like tray icon and progress dialog.
#include "ui.h"      // Function declarations for this file
#include "globals.h" // Access to global handles (g_hWnd, g_hProgressDlg), constants (WM_APP_*, ID_*, etc.), state (g_bAutoUpdatePaused, g_notifyIconData)
#include "log.h"     // For Log function

#include <Windows.h>
#include <commctrl.h> // For progress bar messages (PBM_*) and TaskDialog constants/functions if needed here
#include <shellapi.h> // For NOTIFYICONDATAW, Shell_NotifyIconW
#include <string>
#include <system_error> // For std::system_category
#include <algorithm>    // For std::max, std::min
#include <new>          // For std::nothrow

// (AddTrayIcon - unchanged from previous fix)
void AddTrayIcon(HWND hWnd) {
    Log("正在添加托盘图标..."); // "Adding tray icon..."
    ZeroMemory(&g_notifyIconData, sizeof(NOTIFYICONDATAW)); g_notifyIconData.cbSize = sizeof(NOTIFYICONDATAW); g_notifyIconData.hWnd = hWnd; g_notifyIconData.uID = 1; g_notifyIconData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP; g_notifyIconData.uCallbackMessage = WM_APP_TRAY_MSG;
    HICON hIcon = LoadIcon(NULL, IDI_APPLICATION); if (!hIcon) { DWORD error = GetLastError(); Log("错误：加载默认托盘图标失败。LoadIcon 错误代码: " + std::to_string(error)); g_notifyIconData.hIcon = NULL; }
    else { g_notifyIconData.hIcon = hIcon; } // "Error: Failed to load default tray icon. LoadIcon Error Code: "
    wcsncpy_s(g_notifyIconData.szTip, std::size(g_notifyIconData.szTip), L"新闻联播调度器", _TRUNCATE); // "News Broadcast Scheduler"
    if (!Shell_NotifyIconW(NIM_ADD, &g_notifyIconData)) {
        DWORD error = GetLastError(); if (error == ERROR_TIMEOUT) { Log("Shell_NotifyIconW(NIM_ADD) 超时，使用 NIM_MODIFY 重试..."); Sleep(100); if (!Shell_NotifyIconW(NIM_MODIFY, &g_notifyIconData)) { error = GetLastError(); Log("错误：超时后添加/修改托盘图标失败。错误代码: " + std::to_string(error)); } else { Log("初始超时后成功修改托盘图标。"); } } // "timed out, retrying with NIM_MODIFY..." "Error: Failed to add/modify tray icon after timeout. Error Code: " "Tray icon modified successfully after initial timeout."
        else { Log("错误：添加托盘图标失败。错误代码: " + std::to_string(error)); } // "Error: Failed to add tray icon. Error Code: "
    }
    else { g_notifyIconData.uVersion = NOTIFYICON_VERSION_4; Shell_NotifyIconW(NIM_SETVERSION, &g_notifyIconData); Log("托盘图标添加成功。"); } // "Tray icon added successfully."
}

// (RemoveTrayIcon - unchanged from previous fix)
void RemoveTrayIcon() {
    if (g_notifyIconData.hWnd) { Log("正在移除托盘图标..."); if (!Shell_NotifyIconW(NIM_DELETE, &g_notifyIconData)) { DWORD error = GetLastError(); if (IsWindow(g_notifyIconData.hWnd)) { Log("错误：删除托盘图标失败。错误代码: " + std::to_string(error)); } } else { Log("托盘图标删除成功。"); } g_notifyIconData.hWnd = NULL; g_notifyIconData.hIcon = NULL; } // "Removing tray icon..." "Error: Failed to delete tray icon. Error Code: " "Tray icon deleted successfully."
}

// (ShowTrayMenu - unchanged from previous fix)
void ShowTrayMenu(HWND hWnd) {
    POINT pt; if (!GetCursorPos(&pt)) { DWORD error = GetLastError(); Log("错误：无法获取光标位置以显示托盘菜单。错误代码: " + std::to_string(error)); return; } // "Error: Cannot get cursor position for tray menu. Error Code: "
    HMENU hMenu = CreatePopupMenu(); if (!hMenu) { DWORD error = GetLastError(); Log("错误：创建托盘弹出菜单失败。错误代码: " + std::to_string(error)); return; } // "Error: Failed to create tray popup menu. Error Code: "
    InsertMenuW(hMenu, 0, MF_BYPOSITION | MF_STRING, ID_TRAY_OPEN_LOG_COMMAND, L"打开日志文件(&L)"); InsertMenuW(hMenu, 1, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr); InsertMenuW(hMenu, 2, MF_BYPOSITION | MF_STRING, ID_TRAY_CHECK_NOW_COMMAND, L"立即检查更新(&C)"); std::wstring pauseResumeText = g_bAutoUpdatePaused.load() ? L"恢复自动更新(&R)" : L"暂停自动更新(&P)"; InsertMenuW(hMenu, 3, MF_BYPOSITION | MF_STRING, ID_TRAY_PAUSE_RESUME_COMMAND, pauseResumeText.c_str()); InsertMenuW(hMenu, 4, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr); InsertMenuW(hMenu, 5, MF_BYPOSITION | MF_STRING, ID_TRAY_EXIT_COMMAND, L"退出程序(&X)"); // "Open Log File (&L)" "Check Update Now (&C)" "Resume Auto Update (&R)" "Pause Auto Update (&P)" "Exit Application (&X)"
    SetForegroundWindow(hWnd); BOOL cmd = TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_BOTTOMALIGN | TPM_RIGHTBUTTON | TPM_VERPOSANIMATION, pt.x, pt.y, 0, hWnd, NULL); PostMessage(hWnd, WM_NULL, 0, 0);
    if (!DestroyMenu(hMenu)) { DWORD error = GetLastError(); Log("警告：销毁托盘菜单失败。错误代码: " + std::to_string(error)); } // "Warning: Failed to destroy tray menu. Error Code: "
}

// --- Progress Dialog Procedure ---
INT_PTR CALLBACK ProgressDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_INITDIALOG:
        SendDlgItemMessageW(hDlg, IDC_PROGRESS_BAR, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
        SendDlgItemMessageW(hDlg, IDC_PROGRESS_BAR, PBM_SETPOS, 0, 0);
        SetDlgItemTextW(hDlg, IDC_PROGRESS_TEXT, L"准备中..."); // "Preparing..."
        Log("进度对话框已初始化。"); // "Progress dialog initialized."
        return (INT_PTR)TRUE;

    case WM_APP_UPDATE_PROGRESS: {
        int percent = static_cast<int>(wParam);
        wchar_t* statusText = reinterpret_cast<wchar_t*>(lParam);

        // <<< FIX C2589/C2059: Use parentheses around std::max/min
        percent = (std::max)(0, (std::min)(100, percent));

        SendDlgItemMessageW(hDlg, IDC_PROGRESS_BAR, PBM_SETPOS, percent, 0);
        if (statusText) {
            SetDlgItemTextW(hDlg, IDC_PROGRESS_TEXT, statusText);
        }
        else {
            SetDlgItemTextW(hDlg, IDC_PROGRESS_TEXT, L"");
        }
        if (statusText) {
            delete[] statusText; // Memory allocated in PostProgressUpdate is deleted here
            statusText = nullptr;
        }
        return (INT_PTR)TRUE;
    }
    case WM_APP_SHOW_PROGRESS_DLG:
        Log("进度对话框：收到 WM_APP_SHOW_PROGRESS_DLG。"); // "Progress Dialog: Received WM_APP_SHOW_PROGRESS_DLG."
        ShowWindow(hDlg, SW_SHOWNORMAL);
        SetForegroundWindow(hDlg);
        return (INT_PTR)TRUE;

    case WM_APP_HIDE_PROGRESS_DLG:
        Log("进度对话框：收到 WM_APP_HIDE_PROGRESS_DLG。"); // "Progress Dialog: Received WM_APP_HIDE_PROGRESS_DLG."
        ShowWindow(hDlg, SW_HIDE);
        return (INT_PTR)TRUE;

    case WM_CLOSE:
        Log("进度对话框收到 WM_CLOSE（已忽略）。"); // "Progress dialog WM_CLOSE received (ignored)."
        return (INT_PTR)TRUE;

    case WM_DESTROY:
        Log("进度对话框收到 WM_DESTROY。"); // "Progress dialog WM_DESTROY received."
        return (INT_PTR)TRUE;
    }
    return (INT_PTR)FALSE;
}

// --- Helper Function Definition ---
// (PostProgressUpdate - unchanged from previous fix)
void PostProgressUpdate(HWND hProgressWnd, int percent, const std::wstring& status) {
    if (!hProgressWnd || !IsWindow(hProgressWnd)) return;
    wchar_t* msgCopy = new (std::nothrow) wchar_t[status.length() + 1];
    if (msgCopy) {
        wcscpy_s(msgCopy, status.length() + 1, status.c_str());
        if (!PostMessageW(hProgressWnd, WM_APP_UPDATE_PROGRESS, static_cast<WPARAM>(percent), reinterpret_cast<LPARAM>(msgCopy))) {
            DWORD error = GetLastError(); Log("错误：向进度对话框发送 WM_APP_UPDATE_PROGRESS 失败。错误代码: " + std::to_string(error)); delete[] msgCopy; // "Error: Failed to PostMessage WM_APP_UPDATE_PROGRESS to progress dialog. Error code: "
        }
    }
    else { Log("错误：为进度消息分配内存失败。"); } // "Error: Failed to allocate memory for progress message."
}
