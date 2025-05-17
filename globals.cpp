#include "globals.h"
#include "utils.h" // 假设 utils.h 中有获取路径的帮助函数
#include <shlobj.h> // 用于 SHGetFolderPathW

// 初始化全局变量的定义
std::wstring g_appName = L"NewsForHeng";      // 应用程序名称 (使用 L"" 定义宽字符串)
std::wstring g_appVersion = L"1.0.0";         // 应用程序版本
std::wstring g_configFilePath = L"";          // 配置文件路径，将在 InitializeGlobals 中设置
std::wstring g_logFilePath = L"";             // 日志文件路径，将在 InitializeGlobals 中设置
std::wstring g_appDataDir = L"";              // 应用程序数据目录

std::atomic<bool> g_isRunning(true);          // 程序启动时默认为运行状态
bool g_debugMode = false;                     // 调试模式默认为关闭

HWND g_hMainWnd = NULL;                       // 主窗口句柄默认为 NULL

// int g_someGlobalSetting = 10;

// 初始化全局变量函数实现
void InitializeGlobals() {
    // 获取应用程序数据目录 (例如 C:\Users\YourUser\AppData\Roaming\NewsForHeng)
    // 这是存放配置文件和日志文件的推荐位置
    wchar_t szPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, szPath))) {
        g_appDataDir = std::wstring(szPath) + L"\\" + g_appName;
        // 如果目录不存在，则创建它
        if (!DirectoryExists(g_appDataDir)) { // 假设 DirectoryExists 在 utils.h/cpp 中
            CreateDirectoryRecursive(g_appDataDir); // 假设 CreateDirectoryRecursive 在 utils.h/cpp 中
        }
    }
    else {
        // 如果获取 AppData 失败，则退回到程序当前目录
        // 这不是最佳实践，但作为备选方案
        wchar_t currentDir[MAX_PATH];
        GetCurrentDirectoryW(MAX_PATH, currentDir);
        g_appDataDir = currentDir;
        // 也可以考虑记录一个错误或警告
    }

    g_configFilePath = g_appDataDir + L"\\config.ini";
    g_logFilePath = g_appDataDir + L"\\app.log";

    // 可以在此处从配置文件加载 g_debugMode 等设置
    // LoadConfig(); // 假设有这样一个函数
}

void CleanupGlobals() {
    // 如果有动态分配的全局资源，在此处释放
    // 例如：delete g_someGlobalPointer;
    // g_someGlobalPointer = nullptr;
}
