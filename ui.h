// ========================================================================
// ui.h - 处理用户界面元素，如托盘图标和进度对话框
// ========================================================================
#pragma once

#include <Windows.h>
#include <string> // Added for std::wstring used in PostProgressUpdate

// 将应用程序图标添加到系统托盘通知区域。
void AddTrayIcon(HWND hWnd);

// 从系统托盘中移除应用程序图标。
void RemoveTrayIcon();

// 创建并显示托盘图标的右键单击上下文菜单。
void ShowTrayMenu(HWND hWnd);

// 无模式进度对话框的窗口过程。
INT_PTR CALLBACK ProgressDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

// --- Helper Function Declaration ---
// Safely posts a progress update message to the progress dialog.
// Allocates memory for the status string (caller is lParam in ProgressDlgProc).
// Declaration moved here from network.cpp/ui.cpp definition.
// 安全地向进度对话框发送进度更新消息。
// 为状态字符串分配内存（调用者是 ProgressDlgProc 中的 lParam）。
// 声明从 network.cpp/ui.cpp 定义移至此处。
void PostProgressUpdate(HWND hProgressWnd, int percent, const std::wstring& status);

