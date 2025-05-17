// main.cpp : ����Ӧ�ó������ڵ㡣
//

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // �� Windows ͷ�ļ����ų�����ʹ�õ�����
#endif

#include <windows.h>
#include <string>
#include <iostream> // ���ڵ������ (�����Ҫ����̨)

#include "globals.h"    // ȫ�ֱ����ͺ���
#include "config.h"     // ���ù���
#include "log.h"        // ��־ϵͳ
#include "utils.h"      // ʵ�ù��ߺ���
#include "network.h"    // ���繦��
#include "ui.h"         // �û�����
#include "update.h"     // ���¼����Ӧ��
#include "threads.h"    // �̳߳� (�����Ҫ��̨����)
// #include "registry.h"   // ע������ (�����Ҫ)
// #include "system_ops.h" // ϵͳ���� (�����Ҫ)


// ȫ�� Config ʵ�� (������Ϊ��������)
Config g_appConfig;
ThreadPool* g_pThreadPool = nullptr; // ȫ���̳߳�ָ��

// --- Ӧ�ó����߼����� ---
void PerformBackgroundUpdateCheck() {
    if (!g_pThreadPool) return;

    g_pThreadPool->enqueue([] {
        LOG_INFO(L"Background thread: Starting update check...");
        UI::UpdateStatusText(L"Checking for updates...");
        Update::VersionInfo newVersion;
        // TODO: �������ж�ȡ updateCheckUrl
        std::string updateUrl = "YOUR_UPDATE_CHECK_URL_HERE"; // ���� https://example.com/update.json
        // ����һ���򵥵��ı��ļ� URL
        // "https://pastebin.com/raw/XXXXXXXX"; (����� pastebin raw link �滻)

// ȷ�� URL ����Ч�ģ����ҷ���������Ԥ�ڵĸ�ʽ
// ʾ��: ���������� "version|url|notes"
// updateUrl = "http://127.0.0.1:8000/update_info.txt"; // ���ز�����
// ����һ�� update_info.txt �ļ���������: 1.0.1|http://127.0.0.1:8000/NewsForHeng_1.0.1.zip|Bug fixes and new features.

        if (updateUrl == "YOUR_UPDATE_CHECK_URL_HERE") {
            LOG_WARNING(L"Update check URL is not configured. Skipping update check.");
            UI::UpdateStatusText(L"Update URL not configured.");
            return;
        }


        if (Update::CheckForUpdates(g_appVersion, updateUrl, newVersion)) {
            LOG_INFO(L"New version available: ", newVersion.versionString);
            UI::UpdateStatusText(L"New version " + newVersion.versionString + L" available!");
            // ��ʾ�û�����
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
                        // ֪ͨ��ѭ���˳��������˳�ǰ�������³���
                        // ��ͨ��ͨ������һ��ȫ�ֱ�־��Ȼ��������Ϣѭ��֮����
                        // ����ֱ�� PostQuitMessage��Ȼ���� WinMain ����ǰ����
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
    // ���������ִ���˳�ǰ����������ȷ��
    // ���磬����̳߳������������еĹؼ����񣬿�����Ҫ�ȴ�����ʾ�û�
    if (g_pThreadPool && g_pThreadPool->GetTaskQueueSize() > 0) {
        if (MessageBoxW(g_hMainWnd, L"Tasks are still running in the background. Are you sure you want to exit?", g_appName.c_str(), MB_YESNO | MB_ICONWARNING) == IDNO) {
            return false; // ��ֹ�˳�
        }
    }
    return true; // �����˳�
}


// ������ WinMain
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance); // ���δʹ�õĲ���
    UNREFERENCED_PARAMETER(lpCmdLine);

    // 1. ��ʼ��ȫ�ֱ���
    InitializeGlobals(); // ������� g_configFilePath, g_logFilePath ��

    // 2. ��ʼ����־ϵͳ
    // ȷ����־Ŀ¼���� (InitializeGlobals Ӧ���Ѿ������� g_appDataDir �Ĵ���)
    Logger::GetInstance().SetLogFile(g_logFilePath); // ʹ�� globals �����õ�·��
    Logger::GetInstance().SetLogLevel(g_debugMode ? LogLevel::DEBUG : LogLevel::INFO);
    LOG_INFO(L"--------------------------------------------------");
    LOG_INFO(g_appName, L" version ", g_appVersion, L" started.");
    LOG_INFO(L"Command line: ", lpCmdLine);


    // 3. ��������
    if (g_appConfig.Load(g_configFilePath)) {
        LOG_INFO(L"Configuration loaded from: ", g_configFilePath);
        g_debugMode = g_appConfig.GetBool(L"Settings", L"DebugMode", g_debugMode);
        Logger::GetInstance().SetLogLevel(g_debugMode ? LogLevel::DEBUG : LogLevel::INFO); // ��������������־����
        // ��ȡ��������...
    }
    else {
        LOG_WARNING(L"Failed to load configuration or config file not found. Using default settings.");
        // ���Կ��Ǳ���һ��Ĭ������
        // g_appConfig.SetBool(L"Settings", L"DebugMode", g_debugMode);
        // g_appConfig.Save(g_configFilePath);
    }

    // 4. ��ʼ������ (Winsock)
    if (!Network::Initialize()) {
        LOG_FATAL(L"Failed to initialize Winsock. Application cannot continue.");
        MessageBoxW(NULL, L"Network initialization failed. Please check your network configuration.", L"Fatal Error", MB_ICONERROR | MB_OK);
        return 1;
    }

    // 5. ��ʼ���̳߳� (�����Ҫ)
    g_pThreadPool = new ThreadPool(); // ʹ��Ĭ���߳���

    // 6. ����������
    UI::onCheckForUpdatesClicked = PerformBackgroundUpdateCheck; // ����UI�ص�
    UI::onExitRequested = AppExitRequested;                     // �����˳�ȷ�ϻص�

    if (!UI::CreateMainWindow(hInstance, nCmdShow, g_appName, 800, 600)) {
        LOG_FATAL(L"Failed to create main window. Application cannot continue.");
        // �����ѳ�ʼ���Ĳ���
        if (g_pThreadPool) delete g_pThreadPool;
        Network::Cleanup();
        CleanupGlobals();
        return 1;
    }

    LOG_INFO(L"Main window created. Entering message loop.");
    UI::UpdateStatusText(L"Welcome to " + g_appName + L"!");

    // 7. ����ʱ�Զ������� (��ѡ, �����ں�̨�߳�ִ��)
    // PerformBackgroundUpdateCheck(); // ����ͨ��UI��ť����

    // 8. ��������Ϣѭ��
    int exitCode = UI::RunMessageLoop();
    LOG_INFO(L"Message loop exited with code: ", exitCode);

    // 9. ������
    LOG_INFO(L"Application shutting down...");

    // �������� (��ѡ)
    // g_appConfig.SetBool(L"Settings", L"DebugMode", g_debugMode); // �������յĵ���ģʽ״̬
    // if (!g_appConfig.Save()) { // ���浽�ϴμ��ص�·��
    //     LOG_ERROR(L"Failed to save configuration to: ", g_configFilePath);
    // } else {
    //     LOG_INFO(L"Configuration saved to: ", g_configFilePath);
    // }

    if (g_pThreadPool) {
        delete g_pThreadPool;
        g_pThreadPool = nullptr;
    }
    Network::Cleanup(); // ���� Winsock
    CleanupGlobals();   // ����ȫ����Դ

    LOG_INFO(g_appName, L" exited gracefully. Exit code: ", exitCode);
    Logger::GetInstance().SetLogFile(L""); // �ر���־�ļ���� (Logger��������Ҳ����)

    return exitCode;
}
