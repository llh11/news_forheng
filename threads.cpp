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
    Log("�������߳���������"); // "Scheduler thread started."
    struct ConfigCache { std::wstring newsUrl = L"", weatherUrl = L""; int noonShutdownHour = -1, noonShutdownMinute = -1, eveningShutdownHour = -1, eveningShutdownMinute = -1; bool loadedSuccessfully = false; } config;
    auto LoadIniConfig = [&]() -> bool { Log("�������߳�: �� " + wstring_to_utf8(CONFIG_PATH) + " ��������..."); wchar_t buffer[MAX_URL_LENGTH]; std::wstring tempNewsUrl, tempWeatherUrl; int tempNoonHour, tempNoonMinute, tempEveningHour, tempEveningMinute; DWORD newsUrlLen = GetPrivateProfileStringW(APP_SECTION, NEWS_URL_KEY, L"", buffer, MAX_URL_LENGTH, CONFIG_PATH.c_str()); if (newsUrlLen > 0 && newsUrlLen < MAX_URL_LENGTH) tempNewsUrl = buffer; else tempNewsUrl.clear(); DWORD weatherUrlLen = GetPrivateProfileStringW(APP_SECTION, WEATHER_URL_KEY, L"", buffer, MAX_URL_LENGTH, CONFIG_PATH.c_str()); if (weatherUrlLen > 0 && weatherUrlLen < MAX_URL_LENGTH) tempWeatherUrl = buffer; else tempWeatherUrl.clear(); tempNoonHour = GetPrivateProfileIntW(APP_SECTION, NOON_SHUTDOWN_HOUR_KEY, -1, CONFIG_PATH.c_str()); tempNoonMinute = GetPrivateProfileIntW(APP_SECTION, NOON_SHUTDOWN_MINUTE_KEY, -1, CONFIG_PATH.c_str()); tempEveningHour = GetPrivateProfileIntW(APP_SECTION, EVENING_SHUTDOWN_HOUR_KEY, -1, CONFIG_PATH.c_str()); tempEveningMinute = GetPrivateProfileIntW(APP_SECTION, EVENING_SHUTDOWN_MINUTE_KEY, -1, CONFIG_PATH.c_str()); bool isValid = !tempNewsUrl.empty() && !tempWeatherUrl.empty() && (tempNoonHour >= 0 && tempNoonHour <= 23) && (tempNoonMinute >= 0 && tempNoonMinute <= 59) && (tempEveningHour >= 0 && tempEveningHour <= 23) && (tempEveningMinute >= 0 && tempEveningMinute <= 59); if (isValid) { config.newsUrl = tempNewsUrl; config.weatherUrl = tempWeatherUrl; config.noonShutdownHour = tempNoonHour; config.noonShutdownMinute = tempNoonMinute; config.eveningShutdownHour = tempEveningHour; config.eveningShutdownMinute = tempEveningMinute; config.loadedSuccessfully = true; Log("�������߳�: ���ü��ز���֤�ɹ���"); Log(" - ���� URL: " + wstring_to_utf8(config.newsUrl)); Log(" - ���� URL: " + wstring_to_utf8(config.weatherUrl)); Log(" - ���ػ�: " + std::to_string(config.noonShutdownHour) + ":" + (config.noonShutdownMinute < 10 ? "0" : "") + std::to_string(config.noonShutdownMinute)); Log(" - ���ػ�: " + std::to_string(config.eveningShutdownHour) + ":" + (config.eveningShutdownMinute < 10 ? "0" : "") + std::to_string(config.eveningShutdownMinute)); } else { config.loadedSuccessfully = false; Log("�������߳�: ���ü���ʧ�ܻ������Чֵ���ƻ��¼������޷����С�"); if (tempNewsUrl.empty()) Log(" - NewsUrl ȱʧ����Ч��"); if (tempWeatherUrl.empty()) Log(" - WeatherUrl ȱʧ����Ч��"); if (tempNoonHour < 0 || tempNoonHour > 23 || tempNoonMinute < 0 || tempNoonMinute > 59) Log(" - ���ػ�ʱ����Ч��"); if (tempEveningHour < 0 || tempEveningHour > 23 || tempEveningMinute < 0 || tempEveningMinute > 59) Log(" - ���ػ�ʱ����Ч��"); } return config.loadedSuccessfully; }; // "Scheduler thread: Loading configuration from ..." "Scheduler thread: Configuration loaded and validated successfully." " - News URL: " " - Weather URL: " " - Noon Shutdown: " " - Evening Shutdown: " "Scheduler thread: Configuration loading failed or contains invalid values. Scheduled events may not run." " - NewsUrl missing or invalid." " - WeatherUrl missing or invalid." " - Noon shutdown time invalid." " - Evening shutdown time invalid."
    LoadIniConfig();
    struct EventState { bool noonShutdownTriggered = false, newsUrlOpened = false, weatherUrlOpened = false, browsersClosed = false, eveningShutdownTriggered = false; int currentDay = -1; } state;
    while (g_bRunning.load()) {
        try {
            auto now = std::chrono::system_clock::now(); auto now_c = std::chrono::system_clock::to_time_t(now); std::tm local_tm; if (localtime_s(&local_tm, &now_c) != 0) { Log("����������localtime_s ʧ�ܡ������˵�����"); std::this_thread::sleep_for(1s); continue; } // "Scheduler Error: localtime_s failed. Skipping iteration."
            if (state.currentDay != local_tm.tm_yday) { Log("������: ��⵽�µ�һ�� (Day " + std::to_string(local_tm.tm_yday) + ")������ÿ���¼���־��"); state.noonShutdownTriggered = false; state.newsUrlOpened = false; state.weatherUrlOpened = false; state.browsersClosed = false; state.eveningShutdownTriggered = false; state.currentDay = local_tm.tm_yday; } // "Scheduler: New day detected (...). Resetting daily event flags."
            auto CheckTime = [&](int targetHour, int targetMinute) { return config.loadedSuccessfully && (targetHour >= 0 && targetHour <= 23) && (targetMinute >= 0 && targetMinute <= 59) && local_tm.tm_hour == targetHour && local_tm.tm_min == targetMinute; };
            auto HandleEvent = [&](const std::string& eventName, bool& eventFlag, int targetHour, int targetMinute, auto&& action) { if (!eventFlag && CheckTime(targetHour, targetMinute)) { eventFlag = true; Log("ִ�мƻ��¼�: " + eventName + " �� " + std::to_string(targetHour) + ":" + (targetMinute < 10 ? "0" : "") + std::to_string(targetMinute)); std::thread([action = std::forward<decltype(action)>(action), eventName] { try { action(); Log("�ƻ��¼� '" + eventName + "' ���߳���ɡ�"); } catch (const std::exception& e) { Log("���󣺼ƻ��¼� '" + eventName + "' ���߳��з����쳣: " + e.what()); } catch (...) { Log("���󣺼ƻ��¼� '" + eventName + "' ���߳��з���δ֪�쳣��"); } }).detach(); } }; // "Executing scheduled event: ... at ..." "Thread for scheduled event '...' completed." "Error: Exception in thread for scheduled event '...': " "Error: Unknown exception in thread for scheduled event '...'."
            HandleEvent("���ϵͳ�ػ�", state.noonShutdownTriggered, config.noonShutdownHour, config.noonShutdownMinute, [] { Log("�������ػ�����..."); SystemShutdown(); }); // "Noon System Shutdown" "Triggering noon shutdown sequence..."
            HandleEvent("������ URL", state.newsUrlOpened, 19, 0, [&] { Log("���������� URL ����..."); GradualVolumeControl(80, 20, 5000); if (!g_bRunning.load()) return; Log("������ URL: " + wstring_to_utf8(config.newsUrl)); ShellExecuteW(nullptr, L"open", config.newsUrl.c_str(), nullptr, nullptr, SW_SHOWMAXIMIZED); }); // "Open News URL" "Triggering open news URL sequence..." "Opening news URL: "
            HandleEvent("������ URL", state.weatherUrlOpened, 19, 20, [&] { Log("���������� URL ����..."); Log("������ URL: " + wstring_to_utf8(config.weatherUrl)); ShellExecuteW(nullptr, L"open", config.weatherUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL); }); // "Open Weather URL" "Triggering open weather URL sequence..." "Opening weather URL: "
            HandleEvent("�ر����������", state.browsersClosed, 19, 25, [] { Log("�����ر����������..."); CloseBrowserWindows(); }); // "Close Browser Windows" "Triggering close browsers sequence..."
            HandleEvent("���ϵͳ�ػ�", state.eveningShutdownTriggered, config.eveningShutdownHour, config.eveningShutdownMinute, [] { Log("�������ػ�����..."); SystemShutdown(); }); // "Evening System Shutdown" "Triggering evening shutdown sequence..."
        }
        catch (const std::exception& e) { Log("�������߳���ѭ���з����쳣: " + std::string(e.what())); } // "Exception in SchedulerThread main loop: "
        catch (...) { Log("�������߳���ѭ���з���δ֪�쳣��"); } // "Unknown exception in SchedulerThread main loop."
        std::this_thread::sleep_for(1s);
    } Log("�������߳������˳���"); // "Scheduler thread exiting."
}


// (UpdateCheckThread - unchanged from previous fix, except C2110 fix below)
void UpdateCheckThread() {
    Log("�Զ����¼���߳������������: " + std::to_string(CHECK_UPDATE_INTERVAL) + " �롣"); // "Auto-update check thread started. Interval: ... seconds."
    auto lastCheckTime = std::chrono::steady_clock::now() - std::chrono::seconds(CHECK_UPDATE_INTERVAL);
    while (g_bRunning.load()) {
        bool performCheck = false; auto now = std::chrono::steady_clock::now();
        if (g_bCheckUpdateNow.exchange(false)) { Log("ͨ�����̲˵������ֶ����¼�顣"); performCheck = true; } // "Manual update check requested via tray menu."
        else if (!g_bAutoUpdatePaused.load() && (now - lastCheckTime >= std::chrono::seconds(CHECK_UPDATE_INTERVAL))) { Log("�ﵽ���ڸ��¼������"); performCheck = true; } // "Periodic update check interval reached."
        if (performCheck) {
            lastCheckTime = now; Log("ִ�и��¼���߼�..."); // "Executing update check logic..."
            if (!CheckServerConnection(UPDATE_PING_URL)) { Log("����������ʧ�ܣ��������¼�顣"); if (g_bCheckUpdateNow.load()) { MessageBoxW(g_hWnd, L"�޷����ӵ����·����������������������ӡ�", L"������ʧ��", MB_OK | MB_ICONWARNING | MB_TOPMOST); } } // "Server connection failed, skipping update check." "Cannot connect to update server. Please check your network connection." "Check Update Failed"
            else {
                Log("��������������֤�����ڼ���°汾..."); // "Server connection verified. Checking for new version..."
                std::wstring newVersion, downloadUrl; bool updateAvailable = false;
                try { updateAvailable = CheckUpdate(newVersion, downloadUrl); }
                catch (const std::exception& e) { Log("������ʱ�����쳣: " + std::string(e.what())); }
                catch (...) { Log("������ʱ����δ֪�쳣��"); } // "Exception occurred during CheckUpdate: " "Unknown exception occurred during CheckUpdate."
                if (updateAvailable) {
                    Log("��⵽�°汾 (" + wstring_to_utf8(newVersion) + ")��׼���û���ʾ�Ի���"); // "New version detected (...). Preparing user prompt dialog."
                    TASKDIALOGCONFIG config = { sizeof(config) }; config.hwndParent = g_hWnd; config.hInstance = GetModuleHandle(NULL); config.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW | TDF_SIZE_TO_CONTENT; config.dwCommonButtons = TDCBF_CLOSE_BUTTON; config.pszWindowTitle = L"Ӧ�ó�����¿���"; config.pszMainIcon = TD_INFORMATION_ICON; std::wstringstream mainInstructionStream; mainInstructionStream << L"�����°汾 (" << newVersion << L")��\n" << L"����ǰ��Ӧ�ó���汾Ϊ " << CURRENT_VERSION << L"."; std::wstring mainInstruction = mainInstructionStream.str(); config.pszMainInstruction = mainInstruction.c_str(); config.pszContent = L"���θ�����Ҫ���ݣ�\n\t? ��ǿ��ҳ�����õĿ�ѡ��\n\t? �ṩ��������������\n\t? �޸�����ͼ������Ż���������\n\t? ��ǿ�����������ݴ����ͼ�λ�չʾ/��������\n\t? �޸��ض�������޷���ȷ�ػ���ر���ҳ�Ĵ���\n\t? ���������ļ���ʽ\n\t? ���� API ���ýӿ�\n\t? ��־��������\n\t? �������¹�������\n\t? ���������ļ������ó���\n\t? �޸���֪����\n\n\t\t\t\t���ߣ���__heng\n\n��ѡ�������"; std::wstring ignoreButtonText = L"���Դ˰汾 (" + newVersion + L")\n�������ض��汾���¡�"; TASKDIALOG_BUTTON customButtons[] = { { ID_TASK_UPDATE_NOW, L"��������\n���ز�׼��������װ��" }, { ID_TASK_UPDATE_ON_STARTUP, L"�´�����ʱ����\n���ظ��£��´�����Ӧ�ó���ʱ��װ��" }, { ID_TASK_NOT_NOW, L"�ݲ�����\n��ʱ�����˸��¡�" }, { ID_TASK_IGNORE_VERSION, ignoreButtonText.c_str() } }; config.pButtons = customButtons; config.cButtons = ARRAYSIZE(customButtons); config.nDefaultButton = ID_TASK_UPDATE_NOW; // "Application Update Available" "New version found (...)." "Your current application version is ..." "Update contents:" ... "Author: ..." "Please choose an action:" "Ignore This Version (...)\nSkip this specific version update." "Update Now\nDownload and prepare for immediate installation." "Update on Next Startup\nDownload the update, install when the application starts next time." "Not Now\nSkip this update for now."
                    int buttonPressed = 0; HRESULT hr = TaskDialogIndirect(&config, &buttonPressed, NULL, NULL);
                    if (SUCCEEDED(hr)) {
                        std::wstring tempFileName = CONFIG_DIR + L"update_temp_" + newVersion + L".exe"; Log("������ʱ�ļ�·������Ϊ: " + wstring_to_utf8(tempFileName)); // "Update temporary file path set to: "
                        switch (buttonPressed) {
                        case ID_TASK_UPDATE_NOW: { Log("�û�ѡ���������¡�"); PostMessage(g_hWnd, WM_APP_SHOW_PROGRESS_DLG, 0, 0); std::thread([=] { bool downloadSuccess = false; try { downloadSuccess = DownloadFile(downloadUrl, tempFileName, g_hProgressDlg); } catch (const std::exception& e) { Log("���������ļ�ʱ�����쳣���������£�: " + std::string(e.what())); PostProgressUpdate(g_hProgressDlg, 0, L"����ʱ����"); } catch (...) { Log("���������ļ�ʱ����δ֪�쳣���������£���"); PostProgressUpdate(g_hProgressDlg, 0, L"����ʱ����"); } if (downloadSuccess) { Log("���������¡����ļ����سɹ�������ִ�и���..."); try { PerformUpdate(tempFileName); } catch (const std::exception& e) { Log("����ִ�и���ʱ�����쳣: " + std::string(e.what())); PostProgressUpdate(g_hProgressDlg, 0, L"����Ӧ��ʧ��"); PostMessage(g_hWnd, WM_APP_HIDE_PROGRESS_DLG, 0, 0); } catch (...) { Log("����ִ�и���ʱ����δ֪�쳣��"); PostProgressUpdate(g_hProgressDlg, 0, L"����Ӧ��ʧ��"); PostMessage(g_hWnd, WM_APP_HIDE_PROGRESS_DLG, 0, 0); } } else { Log("���������¡����ļ�����ʧ�ܡ�"); MessageBoxW(g_hWnd, L"���ظ����ļ�ʧ�ܡ����������������Ӳ����ԡ�", L"����ʧ��", MB_OK | MB_ICONERROR | MB_TOPMOST); PostMessage(g_hWnd, WM_APP_HIDE_PROGRESS_DLG, 0, 0); } }).detach(); break; } // "User choice: Update Now." "Error: Exception during file download (Update Now): " "Error during download" "Error: Unknown exception during file download (Update Now)." "'Update Now' file download successful. Performing update..." "Error: Exception during PerformUpdate: " "Failed to apply update" "Error: Unknown exception during PerformUpdate." "'Update Now' file download failed." "Failed to download update file. Please check your network connection and retry." "Update Failed"
                        case ID_TASK_UPDATE_ON_STARTUP: {
                            Log("�û�ѡ������ʱ���¡�"); PostMessage(g_hWnd, WM_APP_SHOW_PROGRESS_DLG, 0, 0); std::thread([=] { bool downloadSuccess = false; try { downloadSuccess = DownloadFile(downloadUrl, tempFileName, g_hProgressDlg); }
                            catch (const std::exception& e) { Log("���������ļ�ʱ�����쳣������ʱ���£�: " + std::string(e.what())); PostProgressUpdate(g_hProgressDlg, 0, L"����ʱ����"); }
                            catch (...) { Log("���������ļ�ʱ����δ֪�쳣������ʱ���£���"); PostProgressUpdate(g_hProgressDlg, 0, L"����ʱ����"); } if (downloadSuccess) {
                                Log("������ʱ���¡����ļ����سɹ���"); SavePendingUpdateInfo(tempFileName);
                                // <<< FIX C2110: Use wstringstream for concatenation
                                std::wstringstream msg;
                                msg << L"�°汾�ѳɹ����ء�\n�������´�����Ӧ�ó���ʱ��װ��\n\n�ļ����浽:\n" << tempFileName; // "New version downloaded successfully.\nIt will be installed the next time you start the application.\n\nFile saved to:\n"
                                MessageBoxW(g_hWnd, msg.str().c_str(), L"�������", MB_OK | MB_ICONINFORMATION | MB_TOPMOST); // "Download Complete"
                            }
                            else { Log("������ʱ���¡����ļ�����ʧ�ܡ�"); MessageBoxW(g_hWnd, L"���ظ����ļ�ʧ�ܡ����������������Ӳ����ԡ�", L"����ʧ��", MB_OK | MB_ICONERROR | MB_TOPMOST); } PostMessage(g_hWnd, WM_APP_HIDE_PROGRESS_DLG, 0, 0); }).detach(); break;
                        } // "User choice: Update on Startup." "Error: Exception during file download (Update on Startup): " "Error during download" "Error: Unknown exception during file download (Update on Startup)." "'Update on Startup' file download successful." "'Update on Startup' file download failed." "Failed to download update file. Please check your network connection and retry." "Download Failed"
                        case ID_TASK_IGNORE_VERSION: Log("�û�ѡ�񣺺��Դ˰汾 (" + wstring_to_utf8(newVersion) + ")��"); WriteIgnoredVersion(newVersion); break; // "User choice: Ignore this version (...)."
                        case ID_TASK_NOT_NOW: case IDCANCEL: case IDCLOSE: Log("�û�ѡ���ݲ����¡�ȡ����ر��˸��¶Ի���"); break; // "User choice: Not Now, Cancel, or closed the update dialog."
                        default: Log("TaskDialog �а�����δ֪��ť ID: " + std::to_string(buttonPressed)); break; // "Unknown button ID pressed in TaskDialog: "
                        }
                    }
                    else { Log("TaskDialogIndirect ����ʧ�ܡ�HRESULT: 0x" + std::hex + hr); MessageBoxW(g_hWnd, L"�޷���ʾ����֪ͨ�Ի���", L"UI ����", MB_OK | MB_ICONWARNING | MB_TOPMOST); } // "TaskDialogIndirect call failed. HRESULT: ..." "Could not display the update notification dialog." "UI Error"
                }
                else { Log("δ�ҵ��¸��»���¼��ʧ�ܡ�"); if (g_bCheckUpdateNow.load()) { MessageBoxW(g_hWnd, L"����Ӧ�ó����������°汾��", L"������", MB_OK | MB_ICONINFORMATION | MB_TOPMOST); } } // "No new update found or update check failed." "Your application is up-to-date." "Check Update"
            }
        }
        std::this_thread::sleep_for(1s);
    }
    Log("�Զ����¼���߳������˳���"); // "Auto-update check thread exiting."
}

// (HeartbeatThread - unchanged from previous fix)
void HeartbeatThread() {
    Log("�����߳������������: " + std::to_string(HEARTBEAT_INTERVAL_SECONDS) + " �롣"); // "Heartbeat thread started. Interval: ... seconds."
    while (g_bRunning.load()) {
        auto wakeUpTime = std::chrono::steady_clock::now() + std::chrono::seconds(HEARTBEAT_INTERVAL_SECONDS);
        while (g_bRunning.load() && std::chrono::steady_clock::now() < wakeUpTime) { std::this_thread::sleep_for(500ms); }
        if (!g_bRunning.load()) break;
        try { if (!SendHeartbeat()) { Log("��������ʧ�ܣ��������� false����"); } } // "SendHeartbeat failed (function returned false)."
        catch (const std::exception& e) { Log("��������ʱ�����쳣: " + std::string(e.what())); } // "Exception occurred during SendHeartbeat: "
        catch (...) { Log("��������ʱ����δ֪�쳣��"); } // "Unknown exception occurred during SendHeartbeat."
    }
    Log("�����߳������˳���"); // "Heartbeat thread exiting."
}
