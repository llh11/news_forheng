#include "globals.h"
#include "utils.h" // ���� utils.h ���л�ȡ·���İ�������
#include <shlobj.h> // ���� SHGetFolderPathW

// ��ʼ��ȫ�ֱ����Ķ���
std::wstring g_appName = L"NewsForHeng";      // Ӧ�ó������� (ʹ�� L"" ������ַ���)
std::wstring g_appVersion = L"1.0.0";         // Ӧ�ó���汾
std::wstring g_configFilePath = L"";          // �����ļ�·�������� InitializeGlobals ������
std::wstring g_logFilePath = L"";             // ��־�ļ�·�������� InitializeGlobals ������
std::wstring g_appDataDir = L"";              // Ӧ�ó�������Ŀ¼

std::atomic<bool> g_isRunning(true);          // ��������ʱĬ��Ϊ����״̬
bool g_debugMode = false;                     // ����ģʽĬ��Ϊ�ر�

HWND g_hMainWnd = NULL;                       // �����ھ��Ĭ��Ϊ NULL

// int g_someGlobalSetting = 10;

// ��ʼ��ȫ�ֱ�������ʵ��
void InitializeGlobals() {
    // ��ȡӦ�ó�������Ŀ¼ (���� C:\Users\YourUser\AppData\Roaming\NewsForHeng)
    // ���Ǵ�������ļ�����־�ļ����Ƽ�λ��
    wchar_t szPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, szPath))) {
        g_appDataDir = std::wstring(szPath) + L"\\" + g_appName;
        // ���Ŀ¼�����ڣ��򴴽���
        if (!DirectoryExists(g_appDataDir)) { // ���� DirectoryExists �� utils.h/cpp ��
            CreateDirectoryRecursive(g_appDataDir); // ���� CreateDirectoryRecursive �� utils.h/cpp ��
        }
    }
    else {
        // �����ȡ AppData ʧ�ܣ����˻ص�����ǰĿ¼
        // �ⲻ�����ʵ��������Ϊ��ѡ����
        wchar_t currentDir[MAX_PATH];
        GetCurrentDirectoryW(MAX_PATH, currentDir);
        g_appDataDir = currentDir;
        // Ҳ���Կ��Ǽ�¼һ������򾯸�
    }

    g_configFilePath = g_appDataDir + L"\\config.ini";
    g_logFilePath = g_appDataDir + L"\\app.log";

    // �����ڴ˴��������ļ����� g_debugMode ������
    // LoadConfig(); // ����������һ������
}

void CleanupGlobals() {
    // ����ж�̬�����ȫ����Դ���ڴ˴��ͷ�
    // ���磺delete g_someGlobalPointer;
    // g_someGlobalPointer = nullptr;
}
