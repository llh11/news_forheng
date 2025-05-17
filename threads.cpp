// threads.cpp
// Implements background thread functions for scheduling, updates, and heartbeats.
#include "threads.h" // Function declarations for this file

// Include necessary headers used within the thread functions
#include "globals.h"    // Access to global state (g_bRunning, atomics, handles), constants, RAII classes
#include "log.h"        // For Log function
#include "utils.h"      // For string conversions, ParseVersion, CompareVersions
#include "config.h"     // For ValidateConfig (if needed periodically)
#include "network.h"    // For CheckServerConnection, CheckUpdate, DownloadFile, SendHeartbeat
#include "system_ops.h" // For SystemShutdown, GradualVolumeControl, CloseBrowserWindows
#include "update.h"     // For update file handling (ReadIgnoredVersion, PerformUpdate, etc.)
#include "ui.h"         // For progress dialog messages (WM_APP_*) and TaskDialog constants, PostProgressUpdate

#include <Windows.h>
#include <commctrl.h>   // For TaskDialogIndirect, TASKDIALOGCONFIG, TaskDialog common icons/buttons
#include <shellapi.h>   // For ShellExecuteW
#include <thread>       // For std::thread, std::this_thread::sleep_for
#include <chrono>       // For time literals (e.g., ms, s) and time points
#include <string>
#include <vector>
#include <sstream>      // For wstringstream
#include <ctime>        // For time_t, tm, localtime_s
#include <atomic>       // Ensure std::atomic is included (already in globals.h)
#include <memory>       // For std::make_unique
#include <system_error> // For std::system_category

using namespace std::chrono_literals;

// --- Background Threads ---
// (SchedulerThread - unchanged from previous fix)
void SchedulerThread() {
    Log("调度器线程已启动。"); // "Scheduler thread started."
    struct ConfigCache { std::wstring newsUrl = L"", weatherUrl = L""; int noonShutdownHour = -1, noonShutdownMinute = -1, eveningShutdownHour = -1, eveningShutdownMinute = -1; bool loadedSuccessfully = false; } config;
    auto LoadIniConfig = [&]() -> bool { Log("调度器线程: 从 " + wstring_to_utf8(CONFIG_PATH) + " 加载配置..."); wchar_t buffer[MAX_URL_LENGTH]; std::wstring tempNewsUrl, tempWeatherUrl; int tempNoonHour, tempNoonMinute, tempEveningHour, tempEveningMinute; DWORD newsUrlLen = GetPrivateProfileStringW(APP_SECTION, NEWS_URL_KEY, L"", buffer, MAX_URL_LENGTH, CONFIG_PATH.c_str()); if (newsUrlLen > 0 && newsUrlLen < MAX_URL_LENGTH) tempNewsUrl = buffer; else tempNewsUrl.clear(); DWORD weatherUrlLen = GetPrivateProfileStringW(APP_SECTION, WEATHER_URL_KEY, L"", buffer, MAX_URL_LENGTH, CONFIG_PATH.c_str()); if (weatherUrlLen > 0 && weatherUrlLen < MAX_URL_LENGTH) tempWeatherUrl = buffer; else tempWeatherUrl.clear(); tempNoonHour = GetPrivateProfileIntW(APP_SECTION, NOON_SHUTDOWN_HOUR_KEY, -1, CONFIG_PATH.c_str()); tempNoonMinute = GetPrivateProfileIntW(APP_SECTION, NOON_SHUTDOWN_MINUTE_KEY, -1, CONFIG_PATH.c_str()); tempEveningHour = GetPrivateProfileIntW(APP_SECTION, EVENING_SHUTDOWN_HOUR_KEY, -1, CONFIG_PATH.c_str()); tempEveningMinute = GetPrivateProfileIntW(APP_SECTION, EVENING_SHUTDOWN_MINUTE_KEY, -1, CONFIG_PATH.c_str()); bool isValid = !tempNewsUrl.empty() && !tempWeatherUrl.empty() && (tempNoonHour >= 0 && tempNoonHour <= 23) && (tempNoonMinute >= 0 && tempNoonMinute <= 59) && (tempEveningHour >= 0 && tempEveningHour <= 23) && (tempEveningMinute >= 0 && tempEveningMinute <= 59); if (isValid) { config.newsUrl = tempNewsUrl; config.weatherUrl = tempWeatherUrl; config.noonShutdownHour = tempNoonHour; config.noonShutdownMinute = tempNoonMinute; config.eveningShutdownHour = tempEveningHour; config.eveningShutdownMinute = tempEveningMinute; config.loadedSuccessfully = true; Log("调度器线程: 配置加载并验证成功。"); Log(" - 新闻 URL: " + wstring_to_utf8(config.newsUrl)); Log(" - 天气 URL: " + wstring_to_utf8(config.weatherUrl)); Log(" - 午间关机: " + std::to_string(config.noonShutdownHour) + ":" + (config.noonShutdownMinute < 10 ? "0" : "") + std::to_string(config.noonShutdownMinute)); Log(" - 晚间关机: " + std::to_string(config.eveningShutdownHour) + ":" + (config.eveningShutdownMinute < 10 ? "0" : "") + std::to_string(config.eveningShutdownMinute)); } else { config.loadedSuccessfully = false; Log("调度器线程: 配置加载失败或包含无效值。计划事件可能无法运行。"); if (tempNewsUrl.empty()) Log(" - NewsUrl 缺失或无效。"); if (tempWeatherUrl.empty()) Log(" - WeatherUrl 缺失或无效。"); if (tempNoonHour < 0 || tempNoonHour > 23 || tempNoonMinute < 0 || tempNoonMinute > 59) Log(" - 午间关机时间无效。"); if (tempEveningHour < 0 || tempEveningHour > 23 || tempEveningMinute < 0 || tempEveningMinute > 59) Log(" - 晚间关机时间无效。"); } return config.loadedSuccessfully; }; // "Scheduler thread: Loading configuration from ..." "Scheduler thread: Configuration loaded and validated successfully." " - News URL: " " - Weather URL: " " - Noon Shutdown: " " - Evening Shutdown: " "Scheduler thread: Configuration loading failed or contains invalid values. Scheduled events may not run." " - NewsUrl missing or invalid." " - WeatherUrl missing or invalid." " - Noon shutdown time invalid." " - Evening shutdown time invalid."
    LoadIniConfig();
    struct EventState { bool noonShutdownTriggered = false, newsUrlOpened = false, weatherUrlOpened = false, browsersClosed = false, eveningShutdownTriggered = false; int currentDay = -1; } state;
    while (g_bRunning.load()) {
        try {
            auto now = std::chrono::system_clock::now(); auto now_c = std::chrono::system_clock::to_time_t(now); std::tm local_tm; if (localtime_s(&local_tm, &now_c) != 0) { Log("调度器错误：localtime_s 失败。跳过此迭代。"); std::this_thread::sleep_for(1s); continue; } // "Scheduler Error: localtime_s failed. Skipping iteration."
            if (state.currentDay != local_tm.tm_yday) { Log("调度器: 检测到新的一天 (Day " + std::to_string(local_tm.tm_yday) + ")。重置每日事件标志。"); state.noonShutdownTriggered = false; state.newsUrlOpened = false; state.weatherUrlOpened = false; state.browsersClosed = false; state.eveningShutdownTriggered = false; state.currentDay = local_tm.tm_yday; } // "Scheduler: New day detected (...). Resetting daily event flags."
            auto CheckTime = [&](int targetHour, int targetMinute) { return config.loadedSuccessfully && (targetHour >= 0 && targetHour <= 23) && (targetMinute >= 0 && targetMinute <= 59) && local_tm.tm_hour == targetHour && local_tm.tm_min == targetMinute; };
            auto HandleEvent = [&](const std::string& eventName, bool& eventFlag, int targetHour, int targetMinute, auto&& action) { if (!eventFlag && CheckTime(targetHour, targetMinute)) { eventFlag = true; Log("执行计划事件: " + eventName + " 在 " + std::to_string(targetHour) + ":" + (targetMinute < 10 ? "0" : "") + std::to_string(targetMinute)); std::thread([action = std::forward<decltype(action)>(action), eventName] { try { action(); Log("计划事件 '" + eventName + "' 的线程完成。"); } catch (const std::exception& e) { Log("错误：计划事件 '" + eventName + "' 的线程中发生异常: " + e.what()); } catch (...) { Log("错误：计划事件 '" + eventName + "' 的线程中发生未知异常。"); } }).detach(); } }; // "Executing scheduled event: ... at ..." "Thread for scheduled event '...' completed." "Error: Exception in thread for scheduled event '...': " "Error: Unknown exception in thread for scheduled event '...'."
            HandleEvent("午间系统关机", state.noonShutdownTriggered, config.noonShutdownHour, config.noonShutdownMinute, [] { Log("触发午间关机序列..."); SystemShutdown(); }); // "Noon System Shutdown" "Triggering noon shutdown sequence..."
            HandleEvent("打开新闻 URL", state.newsUrlOpened, 19, 0, [&] { Log("触发打开新闻 URL 序列..."); GradualVolumeControl(80, 20, 5000); if (!g_bRunning.load()) return; Log("打开新闻 URL: " + wstring_to_utf8(config.newsUrl)); ShellExecuteW(nullptr, L"open", config.newsUrl.c_str(), nullptr, nullptr, SW_SHOWMAXIMIZED); }); // "Open News URL" "Triggering open news URL sequence..." "Opening news URL: "
            HandleEvent("打开天气 URL", state.weatherUrlOpened, 19, 20, [&] { Log("触发打开天气 URL 序列..."); Log("打开天气 URL: " + wstring_to_utf8(config.weatherUrl)); ShellExecuteW(nullptr, L"open", config.weatherUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL); }); // "Open Weather URL" "Triggering open weather URL sequence..." "Opening weather URL: "
            HandleEvent("关闭浏览器窗口", state.browsersClosed, 19, 25, [] { Log("触发关闭浏览器序列..."); CloseBrowserWindows(); }); // "Close Browser Windows" "Triggering close browsers sequence..."
            HandleEvent("晚间系统关机", state.eveningShutdownTriggered, config.eveningShutdownHour, config.eveningShutdownMinute, [] { Log("触发晚间关机序列..."); SystemShutdown(); }); // "Evening System Shutdown" "Triggering evening shutdown sequence..."
        }
        catch (const std::exception& e) { Log("调度器线程主循环中发生异常: " + std::string(e.what())); } // "Exception in SchedulerThread main loop: "
        catch (...) { Log("调度器线程主循环中发生未知异常。"); } // "Unknown exception in SchedulerThread main loop."
        std::this_thread::sleep_for(1s);
    } Log("调度器线程正在退出。"); // "Scheduler thread exiting."
}


// (UpdateCheckThread - unchanged from previous fix, except C2110 fix below)
void UpdateCheckThread() {
    Log("自动更新检查线程已启动。间隔: " + std::to_string(CHECK_UPDATE_INTERVAL) + " 秒。"); // "Auto-update check thread started. Interval: ... seconds."
    auto lastCheckTime = std::chrono::steady_clock::now() - std::chrono::seconds(CHECK_UPDATE_INTERVAL);
    while (g_bRunning.load()) {
        bool performCheck = false; auto now = std::chrono::steady_clock::now();
        if (g_bCheckUpdateNow.exchange(false)) { Log("通过托盘菜单请求手动更新检查。"); performCheck = true; } // "Manual update check requested via tray menu."
        else if (!g_bAutoUpdatePaused.load() && (now - lastCheckTime >= std::chrono::seconds(CHECK_UPDATE_INTERVAL))) { Log("达到定期更新检查间隔。"); performCheck = true; } // "Periodic update check interval reached."
        if (performCheck) {
            lastCheckTime = now; Log("执行更新检查逻辑..."); // "Executing update check logic..."
            if (!CheckServerConnection(UPDATE_PING_URL)) { Log("服务器连接失败，跳过更新检查。"); if (g_bCheckUpdateNow.load()) { MessageBoxW(g_hWnd, L"无法连接到更新服务器。请检查您的网络连接。", L"检查更新失败", MB_OK | MB_ICONWARNING | MB_TOPMOST); } } // "Server connection failed, skipping update check." "Cannot connect to update server. Please check your network connection." "Check Update Failed"
            else {
                Log("服务器连接已验证。正在检查新版本..."); // "Server connection verified. Checking for new version..."
                std::wstring newVersion, downloadUrl; bool updateAvailable = false;
                try { updateAvailable = CheckUpdate(newVersion, downloadUrl); }
                catch (const std::exception& e) { Log("检查更新时发生异常: " + std::string(e.what())); }
                catch (...) { Log("检查更新时发生未知异常。"); } // "Exception occurred during CheckUpdate: " "Unknown exception occurred during CheckUpdate."
                if (updateAvailable) {
                    Log("检测到新版本 (" + wstring_to_utf8(newVersion) + ")。准备用户提示对话框。"); // "New version detected (...). Preparing user prompt dialog."
                    TASKDIALOGCONFIG config = { sizeof(config) }; config.hwndParent = g_hWnd; config.hInstance = GetModuleHandle(NULL); config.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW | TDF_SIZE_TO_CONTENT; config.dwCommonButtons = TDCBF_CLOSE_BUTTON; config.pszWindowTitle = L"应用程序更新可用"; config.pszMainIcon = TD_INFORMATION_ICON; std::wstringstream mainInstructionStream; mainInstructionStream << L"发现新版本 (" << newVersion << L")。\n" << L"您当前的应用程序版本为 " << CURRENT_VERSION << L"."; std::wstring mainInstruction = mainInstructionStream.str(); config.pszMainInstruction = mainInstruction.c_str(); config.pszContent = L"本次更新主要内容：\n\t? 增强网页打开配置的可选性\n\t? 提供开机自启动服务\n\t? 修复托盘图标错误，优化整体性能\n\t? 增强服务器端数据处理和图形化展示/操作能力\n\t? 修复特定情况下无法正确关机或关闭网页的错误\n\t? 更新配置文件格式\n\t? 新增 API 调用接口\n\t? 日志功能升级\n\t? 联网更新功能升级\n\t? 新增配置文件和配置程序\n\t? 修复已知错误\n\n\t\t\t\t作者：恒__heng\n\n请选择操作："; std::wstring ignoreButtonText = L"忽略此版本 (" + newVersion + L")\n跳过此特定版本更新。"; TASKDIALOG_BUTTON customButtons[] = { { ID_TASK_UPDATE_NOW, L"立即更新\n下载并准备立即安装。" }, { ID_TASK_UPDATE_ON_STARTUP, L"下次启动时更新\n下载更新，下次启动应用程序时安装。" }, { ID_TASK_NOT_NOW, L"暂不更新\n暂时跳过此更新。" }, { ID_TASK_IGNORE_VERSION, ignoreButtonText.c_str() } }; config.pButtons = customButtons; config.cButtons = ARRAYSIZE(customButtons); config.nDefaultButton = ID_TASK_UPDATE_NOW; // "Application Update Available" "New version found (...)." "Your current application version is ..." "Update contents:" ... "Author: ..." "Please choose an action:" "Ignore This Version (...)\nSkip this specific version update." "Update Now\nDownload and prepare for immediate installation." "Update on Next Startup\nDownload the update, install when the application starts next time." "Not Now\nSkip this update for now."
                    int buttonPressed = 0; HRESULT hr = TaskDialogIndirect(&config, &buttonPressed, NULL, NULL);
                    if (SUCCEEDED(hr)) {
                        std::wstring tempFileName = CONFIG_DIR + L"update_temp_" + newVersion + L".exe"; Log("更新临时文件路径设置为: " + wstring_to_utf8(tempFileName)); // "Update temporary file path set to: "
                        switch (buttonPressed) {
                        case ID_TASK_UPDATE_NOW: { Log("用户选择：立即更新。"); PostMessage(g_hWnd, WM_APP_SHOW_PROGRESS_DLG, 0, 0); std::thread([=] { bool downloadSuccess = false; try { downloadSuccess = DownloadFile(downloadUrl, tempFileName, g_hProgressDlg); } catch (const std::exception& e) { Log("错误：下载文件时发生异常（立即更新）: " + std::string(e.what())); PostProgressUpdate(g_hProgressDlg, 0, L"下载时出错"); } catch (...) { Log("错误：下载文件时发生未知异常（立即更新）。"); PostProgressUpdate(g_hProgressDlg, 0, L"下载时出错"); } if (downloadSuccess) { Log("“立即更新”的文件下载成功。正在执行更新..."); try { PerformUpdate(tempFileName); } catch (const std::exception& e) { Log("错误：执行更新时发生异常: " + std::string(e.what())); PostProgressUpdate(g_hProgressDlg, 0, L"更新应用失败"); PostMessage(g_hWnd, WM_APP_HIDE_PROGRESS_DLG, 0, 0); } catch (...) { Log("错误：执行更新时发生未知异常。"); PostProgressUpdate(g_hProgressDlg, 0, L"更新应用失败"); PostMessage(g_hWnd, WM_APP_HIDE_PROGRESS_DLG, 0, 0); } } else { Log("“立即更新”的文件下载失败。"); MessageBoxW(g_hWnd, L"下载更新文件失败。请检查您的网络连接并重试。", L"更新失败", MB_OK | MB_ICONERROR | MB_TOPMOST); PostMessage(g_hWnd, WM_APP_HIDE_PROGRESS_DLG, 0, 0); } }).detach(); break; } // "User choice: Update Now." "Error: Exception during file download (Update Now): " "Error during download" "Error: Unknown exception during file download (Update Now)." "'Update Now' file download successful. Performing update..." "Error: Exception during PerformUpdate: " "Failed to apply update" "Error: Unknown exception during PerformUpdate." "'Update Now' file download failed." "Failed to download update file. Please check your network connection and retry." "Update Failed"
                        case ID_TASK_UPDATE_ON_STARTUP: {
                            Log("用户选择：启动时更新。"); PostMessage(g_hWnd, WM_APP_SHOW_PROGRESS_DLG, 0, 0); std::thread([=] { bool downloadSuccess = false; try { downloadSuccess = DownloadFile(downloadUrl, tempFileName, g_hProgressDlg); }
                            catch (const std::exception& e) { Log("错误：下载文件时发生异常（启动时更新）: " + std::string(e.what())); PostProgressUpdate(g_hProgressDlg, 0, L"下载时出错"); }
                            catch (...) { Log("错误：下载文件时发生未知异常（启动时更新）。"); PostProgressUpdate(g_hProgressDlg, 0, L"下载时出错"); } if (downloadSuccess) {
                                Log("“启动时更新”的文件下载成功。"); SavePendingUpdateInfo(tempFileName);
                                // <<< FIX C2110: Use wstringstream for concatenation
                                std::wstringstream msg;
                                msg << L"新版本已成功下载。\n将在您下次启动应用程序时安装。\n\n文件保存到:\n" << tempFileName; // "New version downloaded successfully.\nIt will be installed the next time you start the application.\n\nFile saved to:\n"
                                MessageBoxW(g_hWnd, msg.str().c_str(), L"下载完成", MB_OK | MB_ICONINFORMATION | MB_TOPMOST); // "Download Complete"
                            }
                            else { Log("“启动时更新”的文件下载失败。"); MessageBoxW(g_hWnd, L"下载更新文件失败。请检查您的网络连接并重试。", L"下载失败", MB_OK | MB_ICONERROR | MB_TOPMOST); } PostMessage(g_hWnd, WM_APP_HIDE_PROGRESS_DLG, 0, 0); }).detach(); break;
                        } // "User choice: Update on Startup." "Error: Exception during file download (Update on Startup): " "Error during download" "Error: Unknown exception during file download (Update on Startup)." "'Update on Startup' file download successful." "'Update on Startup' file download failed." "Failed to download update file. Please check your network connection and retry." "Download Failed"
                        case ID_TASK_IGNORE_VERSION: Log("用户选择：忽略此版本 (" + wstring_to_utf8(newVersion) + ")。"); WriteIgnoredVersion(newVersion); break; // "User choice: Ignore this version (...)."
                        case ID_TASK_NOT_NOW: case IDCANCEL: case IDCLOSE: Log("用户选择：暂不更新、取消或关闭了更新对话框。"); break; // "User choice: Not Now, Cancel, or closed the update dialog."
                        default: Log("TaskDialog 中按下了未知按钮 ID: " + std::to_string(buttonPressed)); break; // "Unknown button ID pressed in TaskDialog: "
                        }
                    }
                    else { Log("TaskDialogIndirect 调用失败。HRESULT: 0x" + std::hex + hr); MessageBoxW(g_hWnd, L"无法显示更新通知对话框。", L"UI 错误", MB_OK | MB_ICONWARNING | MB_TOPMOST); } // "TaskDialogIndirect call failed. HRESULT: ..." "Could not display the update notification dialog." "UI Error"
                }
                else { Log("未找到新更新或更新检查失败。"); if (g_bCheckUpdateNow.load()) { MessageBoxW(g_hWnd, L"您的应用程序已是最新版本。", L"检查更新", MB_OK | MB_ICONINFORMATION | MB_TOPMOST); } } // "No new update found or update check failed." "Your application is up-to-date." "Check Update"
            }
        }
        std::this_thread::sleep_for(1s);
    }
    Log("自动更新检查线程正在退出。"); // "Auto-update check thread exiting."
}

// (HeartbeatThread - unchanged from previous fix)
void HeartbeatThread() {
    Log("心跳线程已启动。间隔: " + std::to_string(HEARTBEAT_INTERVAL_SECONDS) + " 秒。"); // "Heartbeat thread started. Interval: ... seconds."
    while (g_bRunning.load()) {
        auto wakeUpTime = std::chrono::steady_clock::now() + std::chrono::seconds(HEARTBEAT_INTERVAL_SECONDS);
        while (g_bRunning.load() && std::chrono::steady_clock::now() < wakeUpTime) { std::this_thread::sleep_for(500ms); }
        if (!g_bRunning.load()) break;
        try { if (!SendHeartbeat()) { Log("发送心跳失败（函数返回 false）。"); } } // "SendHeartbeat failed (function returned false)."
        catch (const std::exception& e) { Log("发送心跳时发生异常: " + std::string(e.what())); } // "Exception occurred during SendHeartbeat: "
        catch (...) { Log("发送心跳时发生未知异常。"); } // "Unknown exception occurred during SendHeartbeat."
    }
    Log("心跳线程正在退出。"); // "Heartbeat thread exiting."
}
