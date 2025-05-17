#ifndef GLOBALS_H
#define GLOBALS_H

#include <string>
#include <windows.h> // 包含 Windows 特定的头文件，例如 HWND
#include <atomic>    // 用于线程安全的原子操作

// 应用程序基本信息
extern std::wstring g_appName;          // 应用程序名称 (使用 wstring 以支持 Unicode)
extern std::wstring g_appVersion;       // 应用程序版本
extern std::wstring g_configFilePath;   // 配置文件路径
extern std::wstring g_logFilePath;      // 日志文件路径
extern std::wstring g_appDataDir;       // 应用程序数据目录

// 运行状态与控制
extern std::atomic<bool> g_isRunning;   // 控制主循环是否继续运行，原子类型保证线程安全
extern bool g_debugMode;                // 调试模式开关

// 窗口句柄 (如果您的应用有 GUI)
extern HWND g_hMainWnd;                 // 主窗口句柄

// 其他可能的全局设置
// extern int g_someGlobalSetting;

/**
 * @brief 初始化全局变量
 * @note 这个函数应该在程序启动的早期被调用。
 */
void InitializeGlobals();

/**
 * @brief 清理全局资源（如果需要）
 * @note 这个函数可以在程序退出前调用。
 */
void CleanupGlobals();

#endif // GLOBALS_H
