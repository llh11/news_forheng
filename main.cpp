// main.cpp - 应用程序主入口点和窗口过程

// Define NOMINMAX *before* including Windows.h to prevent macro conflicts
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <commctrl.h>   // For InitCommonControlsEx
#include <shellapi.h>   // For ShellExecuteW
#include <thread>       // For std::thread
#include <vector>       // For std::vector (used indirectly)
#include <string>       // For std::wstring, std::string
#include <system_error> // For std::system_category

// --- Include Project Headers ---
// It's generally good practice to include globals.h first if it contains
// common prerequisites like Windows.h with NOMINMAX.
#include "globals.h"    // 全局变量、常量、RAII 类
#include "config.h"     // 配置处理 (InitializeDeviceID, ValidateConfig)
#include "log.h"        // 日志记录 (Log, LoadAndDisplayLogs)
#include "utils.h"      // 实用函数 (EnsureDirectoryExists, GetConfiguratorPath, etc.)
#include "network.h"    // 网络操作 (CheckServerConnection, GetLatestFilenameFromServer, etc.)
#include "registry.h"   // 注册表操作 (ManageStartupSetting)
#include "system_ops.h" // 系统操作 (unused directly in main, called by threads)
#include "threads.h"    // 后台线程 (SchedulerThread, UpdateCheckThread, HeartbeatThread)
#include "ui.h"         // UI 函数 (AddTrayIcon, RemoveTrayIcon, ShowTrayMenu, ProgressDlgProc)
#include "update.h"     // 更新处理 (CheckAndApplyPendingUpdate, PerformUpdate, etc.)

// --- Linker Dependencies (Optional - Usually handled in project settings) ---
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "rpcrt4.lib")

// --- Force Link Common Controls v6 (Needed for TaskDialog, modern controls) ---
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// --- Window Class Name ---
const wchar_t WND_CLASS_NAME[] = L"NewsSchedulerMainWindowClass"; // 唯一的窗口类名

// --- Forward Declaration for Window Procedure ---
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

// --- Windows Program Entry Point ---
int WINAPI WinMain(
    _In_ HINSTANCE hInstance,         // 当前实例句柄
    _In_opt_ HINSTANCE hPrevInstance, // 先前实例句柄 (在 Win32 中始终为 NULL)
    _In_ LPSTR lpCmdLine,             // 命令行参数 (ANSI)
    _In_ int nCmdShow)                // 窗口显示状态 (通常不由后台应用程序使用)
{
    UNREFERENCED_PARAMETER(hPrevInstance); // 标记为未使用
    UNREFERENCED_PARAMETER(lpCmdLine);      // 标记为未使用
    UNREFERENCED_PARAMETER(nCmdShow);       // 标记为未使用

    // 1. 初始化通用控件库 (用于 TaskDialog, ProgressBar 等)
    INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_STANDARD_CLASSES | ICC_PROGRESS_CLASS };
    if (!InitCommonControlsEx(&icex)) {
        MessageBoxW(nullptr, L"严重错误：无法初始化 Windows 通用控件库 (ComCtl32.dll v6)。应用程序无法启动。", L"启动错误", MB_OK | MB_ICONERROR); // "Fatal Error: Failed to initialize Windows Common Controls library (ComCtl32.dll v6). Application cannot start." "Startup Error"
        return 1;
    }

    // 2. 确保配置/日志目录存在
    if (!EnsureDirectoryExists(CONFIG_DIR)) {
        MessageBoxW(nullptr, (L"严重错误：无法创建或访问所需目录 " + CONFIG_DIR + L"。请检查权限。应用程序无法启动。").c_str(), L"启动错误", MB_OK | MB_ICONERROR); // "Fatal Error: Cannot create or access the required directory D:\\news\\. Please check permissions. Application cannot start." "Startup Error"
        return 1;
    }

    // 3. 尽早开始日志记录
    Log("============================================================");
    Log("应用程序启动 - 版本 " + wstring_to_utf8(CURRENT_VERSION)); // "Application starting - Version "
    Log("============================================================");

    // 4. 初始化设备 ID
    InitializeDeviceID();
    if (g_deviceID.empty() || g_deviceID == L"GENERATION_FAILED" || g_deviceID == L"CONFIG_DIR_ERROR") {
        Log("警告：在没有有效设备 ID 的情况下继续。心跳和其他服务器交互可能会失败。"); // "Warning: Proceeding without a valid Device ID. Heartbeats and potentially other server interactions may fail."
        // 可以考虑在这里显示警告，但目前仅记录日志
        // MessageBoxW(nullptr, L"警告：无法初始化唯一的设备 ID。", L"启动警告", MB_OK | MB_ICONWARNING); // "Warning: Could not initialize a unique Device ID." "Startup Warning"
    }

    // 5. 根据 INI 文件管理注册表启动设置
    ManageStartupSetting();

    // 6. 验证配置文件并处理配置错误
    bool configOk = ValidateConfig();
    if (!configOk) {
        Log("初始配置验证失败。配置文件可能丢失、不完整或无效。"); // "Initial configuration validation failed. Config file might be missing, incomplete, or invalid."
        std::wstring configuratorPath = GetConfiguratorPath();
        bool configuratorExists = !configuratorPath.empty() && (GetFileAttributesW(configuratorPath.c_str()) != INVALID_FILE_ATTRIBUTES);

        if (!configuratorExists) {
            Log("在本地未找到 Configurator.exe: " + wstring_to_utf8(configuratorPath) + "。尝试从服务器下载..."); // "Configurator.exe not found locally at: ... Attempting to download from server..."
            MessageBoxW(nullptr, L"应用程序配置文件丢失或无效，并且未找到本地配置程序 (Configurator.exe)。\n\n将尝试从服务器下载配置程序。\n请确保网络连接正常。", L"配置错误", MB_OK | MB_ICONINFORMATION | MB_TOPMOST); // "Application configuration file is missing or invalid, and the local configurator (Configurator.exe) was not found.\n\nAttempting to download the configurator from the server.\nPlease ensure network connectivity." "Configuration Error"

            std::wstring latestConfiguratorFilename;
            if (GetLatestFilenameFromServer(L"configurator", latestConfiguratorFilename)) {
                // TODO: 改进 URL 构建逻辑，可能需要从服务器获取基本 URL
                std::wstring downloadUrl = UPDATE_SERVER_BASE_URL + L"/uploads/" + latestConfiguratorFilename; // 假设路径
                std::wstring savePath = configuratorPath;
                Log("尝试从以下位置下载 Configurator: " + wstring_to_utf8(downloadUrl) + " 到 " + wstring_to_utf8(savePath)); // "Attempting to download Configurator from: ... to ..."

                // 注意：此时没有进度对话框
                if (DownloadFile(downloadUrl, savePath)) {
                    Log("Configurator 下载成功。"); // "Configurator downloaded successfully."
                    MessageBoxW(nullptr, L"配置程序已成功下载。", L"下载完成", MB_OK | MB_ICONINFORMATION | MB_TOPMOST); // "Configurator downloaded successfully." "Download Complete"
                    configuratorExists = true;
                }
                else {
                    Log("下载 Configurator 失败。"); // "Failed to download Configurator."
                    MessageBoxW(nullptr, L"无法下载配置程序。\n请检查网络连接或联系管理员手动放置 Configurator.exe。", L"下载失败", MB_OK | MB_ICONERROR | MB_TOPMOST); // "Could not download the configurator.\nPlease check network connection or contact administrator to place Configurator.exe manually." "Download Failed"
                    return 1; // 如果下载失败且配置无效，则退出
                }
            }
            else {
                Log("从服务器获取最新的 Configurator 文件名失败。"); // "Failed to get latest Configurator filename from server."
                MessageBoxW(nullptr, L"无法从服务器获取配置程序信息。\n请检查网络连接或联系管理员手动放置 Configurator.exe。", L"错误", MB_OK | MB_ICONERROR | MB_TOPMOST); // "Could not get configurator information from the server.\nPlease check network connection or contact administrator to place Configurator.exe manually." "Error"
                return 1; // 如果无法获取文件名且配置无效，则退出
            }
        }

        if (configuratorExists) {
            Log("配置无效/丢失。启动 Configurator.exe 进行用户设置..."); // "Configuration invalid/missing. Launching Configurator.exe for user setup..."
            MessageBoxW(nullptr, L"应用程序需要进行配置。\n即将启动配置程序 (Configurator.exe)。\n请在配置程序中完成设置。", L"需要配置", MB_OK | MB_ICONINFORMATION | MB_TOPMOST); // "Application requires configuration.\nLaunching the configurator (Configurator.exe).\nPlease complete the setup in the configurator." "Configuration Required"

            if (!LaunchConfiguratorAndWait()) {
                Log("Configurator 启动失败或未生成有效配置。正在退出应用程序。"); // "Configurator launch failed or did not result in a valid configuration. Exiting application."
                return 1; // 如果配置器失败则退出
            }

            Log("Configurator 完成。重新验证配置..."); // "Configurator finished. Re-validating configuration..."
            configOk = ValidateConfig();
            if (!configOk) {
                Log("运行 Configurator 后配置仍然无效。正在退出应用程序。"); // "Configuration still invalid after running Configurator. Exiting application."
                MessageBoxW(nullptr, L"配置程序运行后，配置文件仍然无效。\n应用程序无法继续运行。", L"配置错误", MB_OK | MB_ICONERROR | MB_TOPMOST); // "Configuration file is still invalid after running the configurator.\nApplication cannot continue." "Configuration Error"
                return 1; // 如果配置仍然无效则退出
            }
            Log("运行配置器后配置验证成功。"); // "Configuration validated successfully after running configurator."
            ManageStartupSetting(); // 重新应用启动设置
        }
    }

    // 7. 检查并应用待处理的更新
    if (CheckAndApplyPendingUpdate()) {
        Log("已应用待处理更新或应用程序在更新过程中退出。"); // "Pending update applied or application exited during update process."
        // CheckAndApplyPendingUpdate 调用 PerformUpdate，如果成功应退出应用程序。
        // 如果它返回 true 但应用程序没有退出，则表示更新尝试失败。
        // 在这里退出可确保在更新尝试失败后不运行旧版本。
        return 0;
    }
    Log("未找到或应用待处理更新。"); // "No pending updates found or applied."

    // 8. 注册主窗口类
    WNDCLASSEXW wc = { sizeof(WNDCLASSEX) };
    wc.style = 0; // 通常后台窗口不需要 CS_HREDRAW | CS_VREDRAW
    wc.lpfnWndProc = WndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION); // 可以设置为 NULL
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);    // 不相关，因为窗口隐藏
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);       // 不相关
    wc.lpszMenuName = nullptr;
    wc.lpszClassName = WND_CLASS_NAME;
    wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION); // 可以设置为 NULL

    if (!RegisterClassExW(&wc)) {
        Log("严重错误：主窗口类注册失败。错误代码: " + std::to_string(GetLastError())); // "Fatal Error: Main window class registration failed. Error code: "
        MessageBoxW(nullptr, L"无法注册主窗口类。", L"启动错误", MB_OK | MB_ICONERROR); // "Cannot register main window class." "Startup Error"
        return 1;
    }
    Log("主窗口类注册成功。"); // "Main window class registered successfully."

    // 9. 创建主窗口（隐藏）
    // WS_OVERLAPPEDWINDOW 对于隐藏窗口来说不是必需的，可以使用 0 或 WS_POPUP
    g_hWnd = CreateWindowExW(
        0,                // 可选窗口样式
        WND_CLASS_NAME,   // 窗口类名
        L"新闻联播调度器后台", // 窗口标题 (对用户不可见) "News Broadcast Scheduler Background"
        0,                // 窗口样式 (0 因为它是隐藏的)
        CW_USEDEFAULT, CW_USEDEFAULT, // 默认位置
        CW_USEDEFAULT, CW_USEDEFAULT, // 默认大小
        HWND_MESSAGE,     // <<< 使用 HWND_MESSAGE 创建仅消息窗口
        nullptr,          // 无菜单
        hInstance,        // 实例句柄
        nullptr           // 无附加参数
    );

    if (g_hWnd == nullptr) {
        Log("严重错误：主窗口创建失败。错误代码: " + std::to_string(GetLastError())); // "Fatal Error: Main window creation failed. Error code: "
        MessageBoxW(nullptr, L"无法创建主应用程序窗口。", L"启动错误", MB_OK | MB_ICONERROR); // "Cannot create main application window." "Startup Error"
        UnregisterClassW(WND_CLASS_NAME, hInstance); // 清理注册的类
        return 1;
    }
    Log("主应用程序仅消息窗口创建成功。"); // "Main application message-only window created successfully."
    // 注意：对于仅消息窗口，不调用 ShowWindow

    // 10. 启动后台线程
    Log("正在启动后台线程 (调度器, 更新检查, 心跳)..."); // "Starting background threads (Scheduler, Update Check, Heartbeat)..."
    std::vector<std::thread> threads;
    bool threads_started = true;
    try {
        // 创建但不分离，以便以后可以加入 (如果需要更可靠的清理)
        // 为了保持与原始代码一致，我们仍然分离它们
        std::thread(SchedulerThread).detach();
        std::thread(UpdateCheckThread).detach();
        std::thread(HeartbeatThread).detach();
    }
    catch (const std::system_error& e) {
        Log(std::string("严重错误：无法启动后台线程: ") + e.what() + " (代码: " + std::to_string(e.code().value()) + ")"); // "Fatal Error: Failed to start background threads: ... (Code: ...)"
        MessageBoxW(g_hWnd, L"无法启动所需的后台线程。应用程序无法正常运行。", L"启动错误", MB_OK | MB_ICONERROR); // "Cannot start required background threads. The application cannot function correctly." "Startup Error"
        threads_started = false;
    }
    catch (...) {
        Log("严重错误：启动后台线程时发生未知错误。"); // "Fatal Error: Unknown error starting background threads."
        MessageBoxW(g_hWnd, L"启动后台线程时发生未知错误。", L"启动错误", MB_OK | MB_ICONERROR); // "Unknown error starting background threads." "Startup Error"
        threads_started = false;
    }

    if (!threads_started) {
        if (g_hWnd) DestroyWindow(g_hWnd); // 清理窗口
        UnregisterClassW(WND_CLASS_NAME, hInstance);
        return 1; // 无法继续
    }
    Log("后台线程启动成功。"); // "Background threads started successfully."

    // 11. 主消息循环
    Log("进入主消息循环..."); // "Entering main message loop..."
    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) { // > 0 表示非 WM_QUIT 消息
        // 如果进度对话框存在且是对话框消息，则让它处理
        if (!g_hProgressDlg || !IsDialogMessageW(g_hProgressDlg, &msg)) {
            TranslateMessage(&msg);  // 转换虚拟键消息
            DispatchMessageW(&msg); // 分派消息到 WndProc
        }
    }
    Log("主消息循环退出 (收到 WM_QUIT)。"); // "Main message loop exited (WM_QUIT received)."

    // --- 清理 ---
    // 线程已分离，无法加入。
    // g_bRunning 在 WM_DESTROY 中设置为 false 以通知它们退出。
    Log("============================================================");
    Log("应用程序正常退出。"); // "Application exiting cleanly."
    Log("============================================================");

    UnregisterClassW(WND_CLASS_NAME, hInstance); // 注销窗口类

    return (int)msg.wParam; // 返回 WM_QUIT 的退出代码
}

// --- 主窗口过程 ---
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        Log("主窗口创建 (WM_CREATE)。添加托盘图标..."); // "Main window created (WM_CREATE). Adding tray icon..."
        // 注意：对于仅消息窗口，WM_CREATE 可能不会像普通窗口那样被调用。
        // 因此，托盘图标的添加移至 WinMain 中窗口创建成功之后。
        // AddTrayIcon(hWnd); // <<< 移至 WinMain
        return 0;

    case WM_APP_TRAY_MSG: // 来自托盘图标的自定义消息
        switch (LOWORD(lParam)) {
        case WM_RBUTTONUP:
            Log("托盘图标右键单击。显示上下文菜单。"); // "Tray icon right-clicked. Showing context menu."
            ShowTrayMenu(hWnd);
            break;
        case WM_LBUTTONDBLCLK:
            Log("托盘图标双击。打开日志文件。"); // "Tray icon double-clicked. Opening log file."
            EnsureDirectoryExists(CONFIG_DIR);
            ShellExecuteW(hWnd, L"open", LOG_PATH.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            break;
        }
        return 0;

    case WM_COMMAND: // 菜单项点击或控件通知
        switch (LOWORD(wParam)) {
        case ID_TRAY_OPEN_LOG_COMMAND:
            Log("托盘菜单 '打开日志文件' 已选择。"); // "Tray menu 'Open Log File' selected."
            EnsureDirectoryExists(CONFIG_DIR);
            ShellExecuteW(hWnd, L"open", LOG_PATH.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            break;
        case ID_TRAY_EXIT_COMMAND:
            Log("托盘菜单 '退出程序' 已选择。启动退出序列。"); // "Tray menu 'Exit Application' selected. Initiating shutdown sequence."
            DestroyWindow(hWnd); // 销毁窗口将触发 WM_DESTROY
            break;
        case ID_TRAY_CHECK_NOW_COMMAND:
            Log("托盘菜单 '立即检查更新' 已选择。"); // "Tray menu 'Check for Updates Now' selected."
            g_bCheckUpdateNow = true; // 向更新线程发信号
            MessageBoxW(hWnd, L"已在后台启动更新检查。", L"检查更新", MB_OK | MB_ICONINFORMATION | MB_TOPMOST); // "An update check has been initiated in the background." "Check for Updates"
            break;
        case ID_TRAY_PAUSE_RESUME_COMMAND:
            g_bAutoUpdatePaused = !g_bAutoUpdatePaused; // 切换暂停状态
            { // 创建作用域以构造消息
                std::wstring msg = g_bAutoUpdatePaused ? L"自动更新检查已暂停。" : L"自动更新检查已恢复。"; // "Automatic update checks have been paused." : "Automatic update checks have been resumed."
                Log(std::string("托盘菜单 '暂停/恢复自动更新' 已选择。自动更新状态: ") + (g_bAutoUpdatePaused ? "已暂停" : "已恢复")); // "Tray menu 'Pause/Resume Auto Update' selected. Auto Update state: Paused/Resumed"
                MessageBoxW(hWnd, msg.c_str(), L"自动更新设置", MB_OK | MB_ICONINFORMATION | MB_TOPMOST); // "Auto Update Setting"
            }
            break;
        }
        return 0;

        // --- 进度对话框消息处理 ---
    case WM_APP_SHOW_PROGRESS_DLG:
        if (!g_hProgressDlg) {
            Log("收到显示进度对话框请求 (WM_APP_SHOW_PROGRESS_DLG)。"); // "Received request to show progress dialog (WM_APP_SHOW_PROGRESS_DLG)."
            // 创建无模式对话框
            // !!! 确保 IDD_PROGRESS (或您使用的 ID) 在您的 .rc 文件中定义 !!!
            g_hProgressDlg = CreateDialogParamW(
                GetModuleHandle(NULL),
                MAKEINTRESOURCE(1),   // <<< 确保这是正确的资源 ID
                hWnd,                 // 父窗口 (可以是 NULL 或 HWND_DESKTOP，但使用仅消息窗口作为父窗口通常可以)
                ProgressDlgProc,
                0);
            if (g_hProgressDlg) {
                Log("进度对话框创建成功。"); // "Progress dialog created successfully."
                // 不需要 ShowWindow/UpdateWindow，CreateDialogParam 会显示它
            }
            else {
                Log("错误：无法创建进度对话框。错误代码: " + std::to_string(GetLastError())); // "Error: Failed to create progress dialog. Error code: "
            }
        }
        else {
            Log("警告：收到 WM_APP_SHOW_PROGRESS_DLG 但对话框已存在。"); // "Warning: Received WM_APP_SHOW_PROGRESS_DLG but dialog already exists."
            // 可以选择将现有对话框带到前台
            // ShowWindow(g_hProgressDlg, SW_RESTORE);
            // SetForegroundWindow(g_hProgressDlg);
        }
        return 0;

    case WM_APP_HIDE_PROGRESS_DLG:
        if (g_hProgressDlg) {
            Log("收到隐藏进度对话框请求 (WM_APP_HIDE_PROGRESS_DLG)。"); // "Received request to hide progress dialog (WM_APP_HIDE_PROGRESS_DLG)."
            DestroyWindow(g_hProgressDlg);
            // ProgressDlgProc 中的 WM_DESTROY 会将 g_hProgressDlg 设为 nullptr
        }
        else {
            Log("警告：收到 WM_APP_HIDE_PROGRESS_DLG 但对话框不存在。"); // "Warning: Received WM_APP_HIDE_PROGRESS_DLG but dialog does not exist."
        }
        return 0;
        // --- 结束进度对话框处理 ---

    case WM_CLOSE: // 尝试关闭仅消息窗口 (不太可能发生，但以防万一)
        Log("主窗口收到 WM_CLOSE。启动退出。"); // "Main window received WM_CLOSE. Initiating exit."
        DestroyWindow(hWnd); // 触发 WM_DESTROY
        return 0;

    case WM_DESTROY: // 窗口正在被销毁
        Log("主窗口正在销毁 (WM_DESTROY)。正在清理..."); // "Main window destroying (WM_DESTROY). Cleaning up..."
        RemoveTrayIcon();    // 移除托盘图标
        g_bRunning = false;  // 向后台线程发信号停止
        // 注意：分离的线程无法安全地加入。
        // 这里的 sleep 只是为了给线程一点时间来响应 g_bRunning 标志。
        Log("等待线程退出 (短暂延迟)..."); // "Waiting for threads to exit (short delay)..."
        std::this_thread::sleep_for(200ms);
        Log("发布 WM_QUIT 消息。"); // "Posting WM_QUIT message."
        PostQuitMessage(0); // 终止消息循环
        return 0;
    }

    // 对于未处理的消息，调用默认窗口过程
    return DefWindowProcW(hWnd, message, wParam, lParam);
}
