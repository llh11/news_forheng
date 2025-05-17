// main.cpp : 定义应用程序的入口点。
//

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // 从 Windows 头文件中排除极少使用的内容
#endif

#include <windows.h>
#include <string>
#include <iostream> // 用于调试输出 (如果需要控制台)

#include "globals.h"    // 全局变量和函数
#include "config.h"     // 配置管理
#include "log.h"        // 日志系统
#include "utils.h"      // 实用工具函数
#include "network.h"    // 网络功能
#include "ui.h"         // 用户界面
#include "update.h"     // 更新检查与应用
#include "threads.h"    // 线程池 (如果需要后台任务)
// #include "registry.h"   // 注册表操作 (如果需要)
// #include "system_ops.h" // 系统操作 (如果需要)


// 全局 Config 实例 (或者作为单例管理)
Config g_appConfig;
ThreadPool* g_pThreadPool = nullptr; // 全局线程池指针

// --- 应用程序逻辑函数 ---
void PerformBackgroundUpdateCheck() {
    if (!g_pThreadPool) return;

    g_pThreadPool->enqueue([] {
        LOG_INFO(L"Background thread: Starting update check...");
        UI::UpdateStatusText(L"Checking for updates...");
        Update::VersionInfo newVersion;
        // TODO: 从配置中读取 updateCheckUrl
        std::string updateUrl = "YOUR_UPDATE_CHECK_URL_HERE"; // 例如 https://example.com/update.json
        // 或者一个简单的文本文件 URL
        // "https://pastebin.com/raw/XXXXXXXX"; (用你的 pastebin raw link 替换)

// 确保 URL 是有效的，并且服务器返回预期的格式
// 示例: 服务器返回 "version|url|notes"
// updateUrl = "http://127.0.0.1:8000/update_info.txt"; // 本地测试用
// 创建一个 update_info.txt 文件，内容如: 1.0.1|http://127.0.0.1:8000/NewsForHeng_1.0.1.zip|Bug fixes and new features.

        if (updateUrl == "YOUR_UPDATE_CHECK_URL_HERE") {
            LOG_WARNING(L"Update check URL is not configured. Skipping update check.");
            UI::UpdateStatusText(L"Update URL not configured.");
            return;
        }


        if (Update::CheckForUpdates(g_appVersion, updateUrl, newVersion)) {
            LOG_INFO(L"New version available: ", newVersion.versionString);
            UI::UpdateStatusText(L"New version " + newVersion.versionString + L" available!");
            // 提示用户更新
            std::wstring prompt = L"A new version (" + newVersion.versionString + L") is available.\n\nRelease Notes:\n" + newVersion.releaseNotes + L"\n\nDo you want to download and install it now?";
            if (MessageBoxW(g_hMainWnd, prompt.c_str(), g_appName.c_str(), MB_YESNO | MB_ICONINFORMATION) == IDYES) {
                UI::UpdateStatusText(L"Downloading update " + newVersion.versionString + L"...");
                Update::DownloadAndApplyUpdate(newVersion,
                    [](long long downloaded, long long total) { // Progress callback
                        std::wstringstream wss;
                        if (total > 0) {
                            wss << L"Downloading: " << (downloaded * 100 / total) << L"% ("
                                << downloaded / (1024 * 1024) << L"MB / " << total / (1024 * 1024) << L"MB)";
                        }
                        else {
                            wss << L"Downloading: " << downloaded / (1024 * 1024) << L"MB";
                        }
                        UI::UpdateStatusText(wss.str());
                    },
                    []() -> bool { // Restart callback
                        LOG_INFO(L"Update downloaded. Requesting application restart.");
                        // 通知主循环退出，并在退出前启动更新程序
                        // 这通常通过设置一个全局标志，然后在主消息循环之后处理
                        // 或者直接 PostQuitMessage，然后在 WinMain 返回前处理
                        if (g_hMainWnd) SendMessage(g_hMainWnd, WM_CLOSE, 0, 0); // Politely ask to close
                        // Actual restart logic (launching updater) should happen after main loop exits.
                        // For now, this callback signals that restart is desired.
                        return true; // Signify restart was initiated from UI's perspective
                    }
                );
            }
        }
        else {
            LOG_INFO(L"No new updates found or failed to check.");
            UI::UpdateStatusText(L"Application is up to date.");
        }
        });
}

bool AppExitRequested() {
    // 在这里可以执行退出前的清理工作或确认
    // 例如，如果线程池中有正在运行的关键任务，可能需要等待或提示用户
    if (g_pThreadPool && g_pThreadPool->GetTaskQueueSize() > 0) {
        if (MessageBoxW(g_hMainWnd, L"Tasks are still running in the background. Are you sure you want to exit?", g_appName.c_str(), MB_YESNO | MB_ICONWARNING) == IDNO) {
            return false; // 阻止退出
        }
    }
    return true; // 允许退出
}


// 主函数 WinMain
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance); // 标记未使用的参数
    UNREFERENCED_PARAMETER(lpCmdLine);

    // 1. 初始化全局变量
    InitializeGlobals(); // 这会设置 g_configFilePath, g_logFilePath 等

    // 2. 初始化日志系统
    // 确保日志目录存在 (InitializeGlobals 应该已经处理了 g_appDataDir 的创建)
    Logger::GetInstance().SetLogFile(g_logFilePath); // 使用 globals 中设置的路径
    Logger::GetInstance().SetLogLevel(g_debugMode ? LogLevel::DEBUG : LogLevel::INFO);
    LOG_INFO(L"--------------------------------------------------");
    LOG_INFO(g_appName, L" version ", g_appVersion, L" started.");
    LOG_INFO(L"Command line: ", lpCmdLine);


    // 3. 加载配置
    if (g_appConfig.Load(g_configFilePath)) {
        LOG_INFO(L"Configuration loaded from: ", g_configFilePath);
        g_debugMode = g_appConfig.GetBool(L"Settings", L"DebugMode", g_debugMode);
        Logger::GetInstance().SetLogLevel(g_debugMode ? LogLevel::DEBUG : LogLevel::INFO); // 根据配置重设日志级别
        // 读取其他配置...
    }
    else {
        LOG_WARNING(L"Failed to load configuration or config file not found. Using default settings.");
        // 可以考虑保存一份默认配置
        // g_appConfig.SetBool(L"Settings", L"DebugMode", g_debugMode);
        // g_appConfig.Save(g_configFilePath);
    }

    // 4. 初始化网络 (Winsock)
    if (!Network::Initialize()) {
        LOG_FATAL(L"Failed to initialize Winsock. Application cannot continue.");
        MessageBoxW(NULL, L"Network initialization failed. Please check your network configuration.", L"Fatal Error", MB_ICONERROR | MB_OK);
        return 1;
    }

    // 5. 初始化线程池 (如果需要)
    g_pThreadPool = new ThreadPool(); // 使用默认线程数

    // 6. 创建主窗口
    UI::onCheckForUpdatesClicked = PerformBackgroundUpdateCheck; // 设置UI回调
    UI::onExitRequested = AppExitRequested;                     // 设置退出确认回调

    if (!UI::CreateMainWindow(hInstance, nCmdShow, g_appName, 800, 600)) {
        LOG_FATAL(L"Failed to create main window. Application cannot continue.");
        // 清理已初始化的部分
        if (g_pThreadPool) delete g_pThreadPool;
        Network::Cleanup();
        CleanupGlobals();
        return 1;
    }

    LOG_INFO(L"Main window created. Entering message loop.");
    UI::UpdateStatusText(L"Welcome to " + g_appName + L"!");

    // 7. 启动时自动检查更新 (可选, 可以在后台线程执行)
    // PerformBackgroundUpdateCheck(); // 或者通过UI按钮触发

    // 8. 运行主消息循环
    int exitCode = UI::RunMessageLoop();
    LOG_INFO(L"Message loop exited with code: ", exitCode);

    // 9. 清理工作
    LOG_INFO(L"Application shutting down...");

    // 保存配置 (可选)
    // g_appConfig.SetBool(L"Settings", L"DebugMode", g_debugMode); // 保存最终的调试模式状态
    // if (!g_appConfig.Save()) { // 保存到上次加载的路径
    //     LOG_ERROR(L"Failed to save configuration to: ", g_configFilePath);
    // } else {
    //     LOG_INFO(L"Configuration saved to: ", g_configFilePath);
    // }

    if (g_pThreadPool) {
        delete g_pThreadPool;
        g_pThreadPool = nullptr;
    }
    Network::Cleanup(); // 清理 Winsock
    CleanupGlobals();   // 清理全局资源

    LOG_INFO(g_appName, L" exited gracefully. Exit code: ", exitCode);
    Logger::GetInstance().SetLogFile(L""); // 关闭日志文件句柄 (Logger析构函数也会做)

    return exitCode;
}
