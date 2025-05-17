// main.cpp - Ӧ�ó�������ڵ�ʹ��ڹ���

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
#include "globals.h"    // ȫ�ֱ�����������RAII ��
#include "config.h"     // ���ô��� (InitializeDeviceID, ValidateConfig)
#include "log.h"        // ��־��¼ (Log, LoadAndDisplayLogs)
#include "utils.h"      // ʵ�ú��� (EnsureDirectoryExists, GetConfiguratorPath, etc.)
#include "network.h"    // ������� (CheckServerConnection, GetLatestFilenameFromServer, etc.)
#include "registry.h"   // ע������ (ManageStartupSetting)
#include "system_ops.h" // ϵͳ���� (unused directly in main, called by threads)
#include "threads.h"    // ��̨�߳� (SchedulerThread, UpdateCheckThread, HeartbeatThread)
#include "ui.h"         // UI ���� (AddTrayIcon, RemoveTrayIcon, ShowTrayMenu, ProgressDlgProc)
#include "update.h"     // ���´��� (CheckAndApplyPendingUpdate, PerformUpdate, etc.)

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
const wchar_t WND_CLASS_NAME[] = L"NewsSchedulerMainWindowClass"; // Ψһ�Ĵ�������

// --- Forward Declaration for Window Procedure ---
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

// --- Windows Program Entry Point ---
int WINAPI WinMain(
    _In_ HINSTANCE hInstance,         // ��ǰʵ�����
    _In_opt_ HINSTANCE hPrevInstance, // ��ǰʵ����� (�� Win32 ��ʼ��Ϊ NULL)
    _In_ LPSTR lpCmdLine,             // �����в��� (ANSI)
    _In_ int nCmdShow)                // ������ʾ״̬ (ͨ�����ɺ�̨Ӧ�ó���ʹ��)
{
    UNREFERENCED_PARAMETER(hPrevInstance); // ���Ϊδʹ��
    UNREFERENCED_PARAMETER(lpCmdLine);      // ���Ϊδʹ��
    UNREFERENCED_PARAMETER(nCmdShow);       // ���Ϊδʹ��

    // 1. ��ʼ��ͨ�ÿؼ��� (���� TaskDialog, ProgressBar ��)
    INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_STANDARD_CLASSES | ICC_PROGRESS_CLASS };
    if (!InitCommonControlsEx(&icex)) {
        MessageBoxW(nullptr, L"���ش����޷���ʼ�� Windows ͨ�ÿؼ��� (ComCtl32.dll v6)��Ӧ�ó����޷�������", L"��������", MB_OK | MB_ICONERROR); // "Fatal Error: Failed to initialize Windows Common Controls library (ComCtl32.dll v6). Application cannot start." "Startup Error"
        return 1;
    }

    // 2. ȷ������/��־Ŀ¼����
    if (!EnsureDirectoryExists(CONFIG_DIR)) {
        MessageBoxW(nullptr, (L"���ش����޷��������������Ŀ¼ " + CONFIG_DIR + L"������Ȩ�ޡ�Ӧ�ó����޷�������").c_str(), L"��������", MB_OK | MB_ICONERROR); // "Fatal Error: Cannot create or access the required directory D:\\news\\. Please check permissions. Application cannot start." "Startup Error"
        return 1;
    }

    // 3. ���翪ʼ��־��¼
    Log("============================================================");
    Log("Ӧ�ó������� - �汾 " + wstring_to_utf8(CURRENT_VERSION)); // "Application starting - Version "
    Log("============================================================");

    // 4. ��ʼ���豸 ID
    InitializeDeviceID();
    if (g_deviceID.empty() || g_deviceID == L"GENERATION_FAILED" || g_deviceID == L"CONFIG_DIR_ERROR") {
        Log("���棺��û����Ч�豸 ID ������¼����������������������������ܻ�ʧ�ܡ�"); // "Warning: Proceeding without a valid Device ID. Heartbeats and potentially other server interactions may fail."
        // ���Կ�����������ʾ���棬��Ŀǰ����¼��־
        // MessageBoxW(nullptr, L"���棺�޷���ʼ��Ψһ���豸 ID��", L"��������", MB_OK | MB_ICONWARNING); // "Warning: Could not initialize a unique Device ID." "Startup Warning"
    }

    // 5. ���� INI �ļ�����ע�����������
    ManageStartupSetting();

    // 6. ��֤�����ļ����������ô���
    bool configOk = ValidateConfig();
    if (!configOk) {
        Log("��ʼ������֤ʧ�ܡ������ļ����ܶ�ʧ������������Ч��"); // "Initial configuration validation failed. Config file might be missing, incomplete, or invalid."
        std::wstring configuratorPath = GetConfiguratorPath();
        bool configuratorExists = !configuratorPath.empty() && (GetFileAttributesW(configuratorPath.c_str()) != INVALID_FILE_ATTRIBUTES);

        if (!configuratorExists) {
            Log("�ڱ���δ�ҵ� Configurator.exe: " + wstring_to_utf8(configuratorPath) + "�����Դӷ���������..."); // "Configurator.exe not found locally at: ... Attempting to download from server..."
            MessageBoxW(nullptr, L"Ӧ�ó��������ļ���ʧ����Ч������δ�ҵ��������ó��� (Configurator.exe)��\n\n�����Դӷ������������ó���\n��ȷ����������������", L"���ô���", MB_OK | MB_ICONINFORMATION | MB_TOPMOST); // "Application configuration file is missing or invalid, and the local configurator (Configurator.exe) was not found.\n\nAttempting to download the configurator from the server.\nPlease ensure network connectivity." "Configuration Error"

            std::wstring latestConfiguratorFilename;
            if (GetLatestFilenameFromServer(L"configurator", latestConfiguratorFilename)) {
                // TODO: �Ľ� URL �����߼���������Ҫ�ӷ�������ȡ���� URL
                std::wstring downloadUrl = UPDATE_SERVER_BASE_URL + L"/uploads/" + latestConfiguratorFilename; // ����·��
                std::wstring savePath = configuratorPath;
                Log("���Դ�����λ������ Configurator: " + wstring_to_utf8(downloadUrl) + " �� " + wstring_to_utf8(savePath)); // "Attempting to download Configurator from: ... to ..."

                // ע�⣺��ʱû�н��ȶԻ���
                if (DownloadFile(downloadUrl, savePath)) {
                    Log("Configurator ���سɹ���"); // "Configurator downloaded successfully."
                    MessageBoxW(nullptr, L"���ó����ѳɹ����ء�", L"�������", MB_OK | MB_ICONINFORMATION | MB_TOPMOST); // "Configurator downloaded successfully." "Download Complete"
                    configuratorExists = true;
                }
                else {
                    Log("���� Configurator ʧ�ܡ�"); // "Failed to download Configurator."
                    MessageBoxW(nullptr, L"�޷��������ó���\n�����������ӻ���ϵ����Ա�ֶ����� Configurator.exe��", L"����ʧ��", MB_OK | MB_ICONERROR | MB_TOPMOST); // "Could not download the configurator.\nPlease check network connection or contact administrator to place Configurator.exe manually." "Download Failed"
                    return 1; // �������ʧ����������Ч�����˳�
                }
            }
            else {
                Log("�ӷ�������ȡ���µ� Configurator �ļ���ʧ�ܡ�"); // "Failed to get latest Configurator filename from server."
                MessageBoxW(nullptr, L"�޷��ӷ�������ȡ���ó�����Ϣ��\n�����������ӻ���ϵ����Ա�ֶ����� Configurator.exe��", L"����", MB_OK | MB_ICONERROR | MB_TOPMOST); // "Could not get configurator information from the server.\nPlease check network connection or contact administrator to place Configurator.exe manually." "Error"
                return 1; // ����޷���ȡ�ļ�����������Ч�����˳�
            }
        }

        if (configuratorExists) {
            Log("������Ч/��ʧ������ Configurator.exe �����û�����..."); // "Configuration invalid/missing. Launching Configurator.exe for user setup..."
            MessageBoxW(nullptr, L"Ӧ�ó�����Ҫ�������á�\n�����������ó��� (Configurator.exe)��\n�������ó�����������á�", L"��Ҫ����", MB_OK | MB_ICONINFORMATION | MB_TOPMOST); // "Application requires configuration.\nLaunching the configurator (Configurator.exe).\nPlease complete the setup in the configurator." "Configuration Required"

            if (!LaunchConfiguratorAndWait()) {
                Log("Configurator ����ʧ�ܻ�δ������Ч���á������˳�Ӧ�ó���"); // "Configurator launch failed or did not result in a valid configuration. Exiting application."
                return 1; // ���������ʧ�����˳�
            }

            Log("Configurator ��ɡ�������֤����..."); // "Configurator finished. Re-validating configuration..."
            configOk = ValidateConfig();
            if (!configOk) {
                Log("���� Configurator ��������Ȼ��Ч�������˳�Ӧ�ó���"); // "Configuration still invalid after running Configurator. Exiting application."
                MessageBoxW(nullptr, L"���ó������к������ļ���Ȼ��Ч��\nӦ�ó����޷��������С�", L"���ô���", MB_OK | MB_ICONERROR | MB_TOPMOST); // "Configuration file is still invalid after running the configurator.\nApplication cannot continue." "Configuration Error"
                return 1; // ���������Ȼ��Ч���˳�
            }
            Log("������������������֤�ɹ���"); // "Configuration validated successfully after running configurator."
            ManageStartupSetting(); // ����Ӧ����������
        }
    }

    // 7. ��鲢Ӧ�ô�����ĸ���
    if (CheckAndApplyPendingUpdate()) {
        Log("��Ӧ�ô�������»�Ӧ�ó����ڸ��¹������˳���"); // "Pending update applied or application exited during update process."
        // CheckAndApplyPendingUpdate ���� PerformUpdate������ɹ�Ӧ�˳�Ӧ�ó���
        // ��������� true ��Ӧ�ó���û���˳������ʾ���³���ʧ�ܡ�
        // �������˳���ȷ���ڸ��³���ʧ�ܺ����оɰ汾��
        return 0;
    }
    Log("δ�ҵ���Ӧ�ô�������¡�"); // "No pending updates found or applied."

    // 8. ע����������
    WNDCLASSEXW wc = { sizeof(WNDCLASSEX) };
    wc.style = 0; // ͨ����̨���ڲ���Ҫ CS_HREDRAW | CS_VREDRAW
    wc.lpfnWndProc = WndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION); // ��������Ϊ NULL
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);    // ����أ���Ϊ��������
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);       // �����
    wc.lpszMenuName = nullptr;
    wc.lpszClassName = WND_CLASS_NAME;
    wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION); // ��������Ϊ NULL

    if (!RegisterClassExW(&wc)) {
        Log("���ش�����������ע��ʧ�ܡ��������: " + std::to_string(GetLastError())); // "Fatal Error: Main window class registration failed. Error code: "
        MessageBoxW(nullptr, L"�޷�ע���������ࡣ", L"��������", MB_OK | MB_ICONERROR); // "Cannot register main window class." "Startup Error"
        return 1;
    }
    Log("��������ע��ɹ���"); // "Main window class registered successfully."

    // 9. ���������ڣ����أ�
    // WS_OVERLAPPEDWINDOW �������ش�����˵���Ǳ���ģ�����ʹ�� 0 �� WS_POPUP
    g_hWnd = CreateWindowExW(
        0,                // ��ѡ������ʽ
        WND_CLASS_NAME,   // ��������
        L"����������������̨", // ���ڱ��� (���û����ɼ�) "News Broadcast Scheduler Background"
        0,                // ������ʽ (0 ��Ϊ�������ص�)
        CW_USEDEFAULT, CW_USEDEFAULT, // Ĭ��λ��
        CW_USEDEFAULT, CW_USEDEFAULT, // Ĭ�ϴ�С
        HWND_MESSAGE,     // <<< ʹ�� HWND_MESSAGE ��������Ϣ����
        nullptr,          // �޲˵�
        hInstance,        // ʵ�����
        nullptr           // �޸��Ӳ���
    );

    if (g_hWnd == nullptr) {
        Log("���ش��������ڴ���ʧ�ܡ��������: " + std::to_string(GetLastError())); // "Fatal Error: Main window creation failed. Error code: "
        MessageBoxW(nullptr, L"�޷�������Ӧ�ó��򴰿ڡ�", L"��������", MB_OK | MB_ICONERROR); // "Cannot create main application window." "Startup Error"
        UnregisterClassW(WND_CLASS_NAME, hInstance); // ����ע�����
        return 1;
    }
    Log("��Ӧ�ó������Ϣ���ڴ����ɹ���"); // "Main application message-only window created successfully."
    // ע�⣺���ڽ���Ϣ���ڣ������� ShowWindow

    // 10. ������̨�߳�
    Log("����������̨�߳� (������, ���¼��, ����)..."); // "Starting background threads (Scheduler, Update Check, Heartbeat)..."
    std::vector<std::thread> threads;
    bool threads_started = true;
    try {
        // �����������룬�Ա��Ժ���Լ��� (�����Ҫ���ɿ�������)
        // Ϊ�˱�����ԭʼ����һ�£�������Ȼ��������
        std::thread(SchedulerThread).detach();
        std::thread(UpdateCheckThread).detach();
        std::thread(HeartbeatThread).detach();
    }
    catch (const std::system_error& e) {
        Log(std::string("���ش����޷�������̨�߳�: ") + e.what() + " (����: " + std::to_string(e.code().value()) + ")"); // "Fatal Error: Failed to start background threads: ... (Code: ...)"
        MessageBoxW(g_hWnd, L"�޷���������ĺ�̨�̡߳�Ӧ�ó����޷��������С�", L"��������", MB_OK | MB_ICONERROR); // "Cannot start required background threads. The application cannot function correctly." "Startup Error"
        threads_started = false;
    }
    catch (...) {
        Log("���ش���������̨�߳�ʱ����δ֪����"); // "Fatal Error: Unknown error starting background threads."
        MessageBoxW(g_hWnd, L"������̨�߳�ʱ����δ֪����", L"��������", MB_OK | MB_ICONERROR); // "Unknown error starting background threads." "Startup Error"
        threads_started = false;
    }

    if (!threads_started) {
        if (g_hWnd) DestroyWindow(g_hWnd); // ������
        UnregisterClassW(WND_CLASS_NAME, hInstance);
        return 1; // �޷�����
    }
    Log("��̨�߳������ɹ���"); // "Background threads started successfully."

    // 11. ����Ϣѭ��
    Log("��������Ϣѭ��..."); // "Entering main message loop..."
    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) { // > 0 ��ʾ�� WM_QUIT ��Ϣ
        // ������ȶԻ���������ǶԻ�����Ϣ������������
        if (!g_hProgressDlg || !IsDialogMessageW(g_hProgressDlg, &msg)) {
            TranslateMessage(&msg);  // ת���������Ϣ
            DispatchMessageW(&msg); // ������Ϣ�� WndProc
        }
    }
    Log("����Ϣѭ���˳� (�յ� WM_QUIT)��"); // "Main message loop exited (WM_QUIT received)."

    // --- ���� ---
    // �߳��ѷ��룬�޷����롣
    // g_bRunning �� WM_DESTROY ������Ϊ false ��֪ͨ�����˳���
    Log("============================================================");
    Log("Ӧ�ó��������˳���"); // "Application exiting cleanly."
    Log("============================================================");

    UnregisterClassW(WND_CLASS_NAME, hInstance); // ע��������

    return (int)msg.wParam; // ���� WM_QUIT ���˳�����
}

// --- �����ڹ��� ---
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        Log("�����ڴ��� (WM_CREATE)���������ͼ��..."); // "Main window created (WM_CREATE). Adding tray icon..."
        // ע�⣺���ڽ���Ϣ���ڣ�WM_CREATE ���ܲ�������ͨ�������������á�
        // ��ˣ�����ͼ���������� WinMain �д��ڴ����ɹ�֮��
        // AddTrayIcon(hWnd); // <<< ���� WinMain
        return 0;

    case WM_APP_TRAY_MSG: // ��������ͼ����Զ�����Ϣ
        switch (LOWORD(lParam)) {
        case WM_RBUTTONUP:
            Log("����ͼ���Ҽ���������ʾ�����Ĳ˵���"); // "Tray icon right-clicked. Showing context menu."
            ShowTrayMenu(hWnd);
            break;
        case WM_LBUTTONDBLCLK:
            Log("����ͼ��˫��������־�ļ���"); // "Tray icon double-clicked. Opening log file."
            EnsureDirectoryExists(CONFIG_DIR);
            ShellExecuteW(hWnd, L"open", LOG_PATH.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            break;
        }
        return 0;

    case WM_COMMAND: // �˵�������ؼ�֪ͨ
        switch (LOWORD(wParam)) {
        case ID_TRAY_OPEN_LOG_COMMAND:
            Log("���̲˵� '����־�ļ�' ��ѡ��"); // "Tray menu 'Open Log File' selected."
            EnsureDirectoryExists(CONFIG_DIR);
            ShellExecuteW(hWnd, L"open", LOG_PATH.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            break;
        case ID_TRAY_EXIT_COMMAND:
            Log("���̲˵� '�˳�����' ��ѡ�������˳����С�"); // "Tray menu 'Exit Application' selected. Initiating shutdown sequence."
            DestroyWindow(hWnd); // ���ٴ��ڽ����� WM_DESTROY
            break;
        case ID_TRAY_CHECK_NOW_COMMAND:
            Log("���̲˵� '����������' ��ѡ��"); // "Tray menu 'Check for Updates Now' selected."
            g_bCheckUpdateNow = true; // ������̷߳��ź�
            MessageBoxW(hWnd, L"���ں�̨�������¼�顣", L"������", MB_OK | MB_ICONINFORMATION | MB_TOPMOST); // "An update check has been initiated in the background." "Check for Updates"
            break;
        case ID_TRAY_PAUSE_RESUME_COMMAND:
            g_bAutoUpdatePaused = !g_bAutoUpdatePaused; // �л���ͣ״̬
            { // �����������Թ�����Ϣ
                std::wstring msg = g_bAutoUpdatePaused ? L"�Զ����¼������ͣ��" : L"�Զ����¼���ѻָ���"; // "Automatic update checks have been paused." : "Automatic update checks have been resumed."
                Log(std::string("���̲˵� '��ͣ/�ָ��Զ�����' ��ѡ���Զ�����״̬: ") + (g_bAutoUpdatePaused ? "����ͣ" : "�ѻָ�")); // "Tray menu 'Pause/Resume Auto Update' selected. Auto Update state: Paused/Resumed"
                MessageBoxW(hWnd, msg.c_str(), L"�Զ���������", MB_OK | MB_ICONINFORMATION | MB_TOPMOST); // "Auto Update Setting"
            }
            break;
        }
        return 0;

        // --- ���ȶԻ�����Ϣ���� ---
    case WM_APP_SHOW_PROGRESS_DLG:
        if (!g_hProgressDlg) {
            Log("�յ���ʾ���ȶԻ������� (WM_APP_SHOW_PROGRESS_DLG)��"); // "Received request to show progress dialog (WM_APP_SHOW_PROGRESS_DLG)."
            // ������ģʽ�Ի���
            // !!! ȷ�� IDD_PROGRESS (����ʹ�õ� ID) ������ .rc �ļ��ж��� !!!
            g_hProgressDlg = CreateDialogParamW(
                GetModuleHandle(NULL),
                MAKEINTRESOURCE(1),   // <<< ȷ��������ȷ����Դ ID
                hWnd,                 // ������ (������ NULL �� HWND_DESKTOP����ʹ�ý���Ϣ������Ϊ������ͨ������)
                ProgressDlgProc,
                0);
            if (g_hProgressDlg) {
                Log("���ȶԻ��򴴽��ɹ���"); // "Progress dialog created successfully."
                // ����Ҫ ShowWindow/UpdateWindow��CreateDialogParam ����ʾ��
            }
            else {
                Log("�����޷��������ȶԻ��򡣴������: " + std::to_string(GetLastError())); // "Error: Failed to create progress dialog. Error code: "
            }
        }
        else {
            Log("���棺�յ� WM_APP_SHOW_PROGRESS_DLG ���Ի����Ѵ��ڡ�"); // "Warning: Received WM_APP_SHOW_PROGRESS_DLG but dialog already exists."
            // ����ѡ�����жԻ������ǰ̨
            // ShowWindow(g_hProgressDlg, SW_RESTORE);
            // SetForegroundWindow(g_hProgressDlg);
        }
        return 0;

    case WM_APP_HIDE_PROGRESS_DLG:
        if (g_hProgressDlg) {
            Log("�յ����ؽ��ȶԻ������� (WM_APP_HIDE_PROGRESS_DLG)��"); // "Received request to hide progress dialog (WM_APP_HIDE_PROGRESS_DLG)."
            DestroyWindow(g_hProgressDlg);
            // ProgressDlgProc �е� WM_DESTROY �Ὣ g_hProgressDlg ��Ϊ nullptr
        }
        else {
            Log("���棺�յ� WM_APP_HIDE_PROGRESS_DLG ���Ի��򲻴��ڡ�"); // "Warning: Received WM_APP_HIDE_PROGRESS_DLG but dialog does not exist."
        }
        return 0;
        // --- �������ȶԻ����� ---

    case WM_CLOSE: // ���Թرս���Ϣ���� (��̫���ܷ��������Է���һ)
        Log("�������յ� WM_CLOSE�������˳���"); // "Main window received WM_CLOSE. Initiating exit."
        DestroyWindow(hWnd); // ���� WM_DESTROY
        return 0;

    case WM_DESTROY: // �������ڱ�����
        Log("�������������� (WM_DESTROY)����������..."); // "Main window destroying (WM_DESTROY). Cleaning up..."
        RemoveTrayIcon();    // �Ƴ�����ͼ��
        g_bRunning = false;  // ���̨�̷߳��ź�ֹͣ
        // ע�⣺������߳��޷���ȫ�ؼ��롣
        // ����� sleep ֻ��Ϊ�˸��߳�һ��ʱ������Ӧ g_bRunning ��־��
        Log("�ȴ��߳��˳� (�����ӳ�)..."); // "Waiting for threads to exit (short delay)..."
        std::this_thread::sleep_for(200ms);
        Log("���� WM_QUIT ��Ϣ��"); // "Posting WM_QUIT message."
        PostQuitMessage(0); // ��ֹ��Ϣѭ��
        return 0;
    }

    // ����δ�������Ϣ������Ĭ�ϴ��ڹ���
    return DefWindowProcW(hWnd, message, wParam, lParam);
}
