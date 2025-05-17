// ========================================================================
// globals.h - 声明全局变量、常量和共享资源
// ========================================================================
#pragma once // Include guard

// Define NOMINMAX *before* including Windows.h to prevent macro conflicts
#ifndef NOMINMAX
#define NOMINMAX
#endif

// --- Minimal Required System/Standard Headers for Declarations ---
#include <Windows.h> // For HWND, HANDLE, HKEY, HICON, NOTIFYICONDATAW, UINT, WM_APP, etc.
#include <wininet.h> // For HINTERNET type
#include <string>    // For std::wstring declarations
#include <atomic>    // For std::atomic_bool declarations
// Note: <mutex> and <deque> are needed for g_logMutex and g_logCache declarations.
// If these caused issues, we could consider forward declaring std::mutex/std::deque
// or moving the declarations elsewhere, but let's keep them for now.
#include <mutex>     // For std::mutex declaration
#include <deque>     // For std::deque declaration

// --- Forward Declarations (if needed, none seem necessary here currently) ---
// Example: class SomeTypeDefinedElsewhere;

// --- 常量定义 ---
// Window messages, control IDs, task dialog button IDs
constexpr UINT WM_APP_TRAY_MSG = WM_APP + 1;
constexpr UINT WM_APP_UPDATE_PROGRESS = WM_APP + 2;
constexpr UINT WM_APP_SHOW_PROGRESS_DLG = WM_APP + 3;
constexpr UINT WM_APP_HIDE_PROGRESS_DLG = WM_APP + 4;
constexpr UINT ID_TRAY_EXIT_COMMAND = 1001;
constexpr UINT ID_TRAY_OPEN_LOG_COMMAND = 1002;
constexpr UINT ID_TRAY_CHECK_NOW_COMMAND = 1003;
constexpr UINT ID_TRAY_PAUSE_RESUME_COMMAND = 1004;
constexpr UINT IDC_LOG_EDIT = 1005;
constexpr UINT IDC_PROGRESS_BAR = 1006;
constexpr UINT IDC_PROGRESS_TEXT = 1007;
constexpr int ID_TASK_UPDATE_NOW = 101;
constexpr int ID_TASK_UPDATE_ON_STARTUP = 103;
constexpr int ID_TASK_NOT_NOW = 102;
constexpr int ID_TASK_IGNORE_VERSION = 104;

// --- WinINet 用户代理 ---
// Declaration only, definition is in globals.cpp
extern const wchar_t* const USER_AGENT;

// --- 路径常量 ---
// Declarations only, definitions are in globals.cpp
extern const std::wstring CONFIG_DIR;
extern const std::wstring CONFIG_FILE_NAME;
extern const std::wstring LOG_FILE_NAME;
extern const std::wstring CONFIG_PATH;
extern const std::wstring LOG_PATH;
extern const std::wstring CONFIGURATOR_EXE_NAME;
extern const std::wstring PENDING_UPDATE_INFO_PATH;
extern const std::wstring IGNORED_UPDATE_VERSION_PATH;

// --- INI 文件常量 ---
// Declarations only, definitions are in globals.cpp
extern const wchar_t APP_SECTION[];
extern const wchar_t NEWS_URL_KEY[];
extern const wchar_t WEATHER_URL_KEY[];
extern const wchar_t NOON_SHUTDOWN_HOUR_KEY[];
extern const wchar_t NOON_SHUTDOWN_MINUTE_KEY[];
extern const wchar_t EVENING_SHUTDOWN_HOUR_KEY[];
extern const wchar_t EVENING_SHUTDOWN_MINUTE_KEY[];
extern const wchar_t SYSTEM_SECTION[];
extern const wchar_t DEVICE_ID_KEY[];
extern const wchar_t RUN_ON_STARTUP_KEY[];

// --- 注册表常量 ---
// Declarations only, definitions are in globals.cpp
extern const wchar_t* const RUN_REGISTRY_KEY_PATH;
extern const wchar_t* const REGISTRY_VALUE_NAME;

// --- 自动更新配置 ---
// Declarations only, definitions are in globals.cpp
extern const std::wstring UPDATE_SERVER_BASE_URL;
extern const std::wstring UPDATE_CHECK_URL;
extern const std::wstring UPDATE_PING_URL;
extern const std::wstring CURRENT_VERSION;
extern const int CHECK_UPDATE_INTERVAL;
extern const int CONNECT_TIMEOUT_MS;
extern const int DOWNLOAD_TIMEOUT_MS;
// Max URL length constant - defined directly here as it's needed by other headers potentially
constexpr int MAX_URL_LENGTH = 2048; // Increased size slightly, ensure it's sufficient

// --- 心跳配置 ---
// Declarations only, definitions are in globals.cpp
extern const std::wstring HEARTBEAT_API_URL;
extern const std::wstring STATUS_API_URL;
extern const int HEARTBEAT_INTERVAL_SECONDS;

// --- 全局状态 ---
// Extern declarations only, definitions are in globals.cpp
extern std::atomic_bool g_bRunning;
extern std::mutex g_logMutex;             // Mutex for log file access
extern std::mutex g_logCacheMutex;        // Mutex for log cache access
extern std::deque<std::wstring> g_logCache; // In-memory log cache
extern HWND g_hWnd;                       // Main window handle
extern HWND g_hProgressDlg;               // Progress dialog handle
extern HWND g_hLogEdit;                   // Log edit control handle (if used)
extern NOTIFYICONDATAW g_notifyIconData;  // Tray icon data structure
extern std::atomic_bool g_bAutoUpdatePaused;
extern std::atomic_bool g_bCheckUpdateNow;
extern const int MAX_LOG_LINES_DISPLAY;   // Max lines for log cache
extern std::wstring g_deviceID;           // Global device ID

// --- RAII 包装类 (仅声明) ---
// These classes manage WinAPI handles. Their definitions are in globals.cpp.
// Other headers can use pointers/references to these without including globals.h
// if they forward-declare them. Including globals.h is needed for definitions.

class InternetHandle {
    HINTERNET m_handle;
public:
    InternetHandle(); // Default constructor
    explicit InternetHandle(HINTERNET h); // Constructor from handle
    ~InternetHandle(); // Destructor (closes handle)

    // --- Rule of Five: Delete copy operations, implement move operations ---
    InternetHandle(const InternetHandle&) = delete; // No copying
    InternetHandle& operator=(const InternetHandle&) = delete; // No copying
    InternetHandle(InternetHandle&& other) noexcept; // Move constructor
    InternetHandle& operator=(InternetHandle&& other) noexcept; // Move assignment

    // --- Member Functions ---
    HINTERNET get() const; // Access the raw handle
    explicit operator bool() const; // Check if handle is valid
    HINTERNET release(); // Release ownership of the handle
    InternetHandle& operator=(HINTERNET h); // Assign a new raw handle
};

class FileHandle {
    HANDLE m_handle;
public:
    FileHandle();
    explicit FileHandle(HANDLE h);
    ~FileHandle();

    // --- Rule of Five ---
    FileHandle(const FileHandle&) = delete;
    FileHandle& operator=(const FileHandle&) = delete;
    FileHandle(FileHandle&& other) noexcept;
    FileHandle& operator=(FileHandle&& other) noexcept;

    // --- Member Functions ---
    HANDLE get() const;
    explicit operator bool() const;
    HANDLE release();
    FileHandle& operator=(HANDLE h);
};

class RegKeyHandle {
    HKEY m_hKey;
public:
    RegKeyHandle();
    explicit RegKeyHandle(HKEY h);
    ~RegKeyHandle();

    // --- Rule of Five ---
    RegKeyHandle(const RegKeyHandle&) = delete;
    RegKeyHandle& operator=(const RegKeyHandle&) = delete;
    RegKeyHandle(RegKeyHandle&& other) noexcept;
    RegKeyHandle& operator=(RegKeyHandle&& other) noexcept;

    // --- Member Functions ---
    HKEY get() const;
    explicit operator bool() const;
    HKEY release();
    RegKeyHandle& operator=(HKEY h);
};