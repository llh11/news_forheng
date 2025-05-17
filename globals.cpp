// globals.cpp
// Defines global variables declared in globals.h and implements RAII classes
#include "globals.h" // Include header first

// Standard library includes needed for definitions here
#include <Windows.h>
#include <wininet.h> // Needed for HINTERNET and InternetCloseHandle
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <deque>
#include <limits> // Required for INVALID_HANDLE_VALUE comparison if not implicitly included

// --- WinINet 用户代理 ---
// Note: Consider making the version part dynamic if needed
const wchar_t* const USER_AGENT = L"NewsSchedulerApp/2.1.1"; // Updated version to match constant below

// --- 路径常量 ---
// IMPORTANT: Hardcoding "D:\\news\\" is generally bad practice.
// Consider using environment variables, registry, or known folders (like %APPDATA%).
// For now, keeping the original path but adding a note.
const std::wstring CONFIG_DIR = L"D:\\news\\"; // WARNING: Hardcoded path
const std::wstring CONFIG_FILE_NAME = L"config.ini";
const std::wstring LOG_FILE_NAME = L"scheduler.log";
const std::wstring CONFIG_PATH = CONFIG_DIR + CONFIG_FILE_NAME;
const std::wstring LOG_PATH = CONFIG_DIR + LOG_FILE_NAME;
const std::wstring CONFIGURATOR_EXE_NAME = L"Configurator.exe";
const std::wstring PENDING_UPDATE_INFO_PATH = CONFIG_DIR + L"update_pending.inf";
const std::wstring IGNORED_UPDATE_VERSION_PATH = CONFIG_DIR + L"update_ignored.txt";

// --- INI 文件常量 ---
const wchar_t APP_SECTION[] = L"Settings"; // Section for application-specific settings
const wchar_t NEWS_URL_KEY[] = L"NewsUrl";
const wchar_t WEATHER_URL_KEY[] = L"WeatherUrl";
const wchar_t NOON_SHUTDOWN_HOUR_KEY[] = L"NoonShutdownHour";
const wchar_t NOON_SHUTDOWN_MINUTE_KEY[] = L"NoonShutdownMinute";
const wchar_t EVENING_SHUTDOWN_HOUR_KEY[] = L"EveningShutdownHour";
const wchar_t EVENING_SHUTDOWN_MINUTE_KEY[] = L"EveningShutdownMinute";
const wchar_t SYSTEM_SECTION[] = L"System"; // Section for system-related settings
const wchar_t DEVICE_ID_KEY[] = L"DeviceID";
const wchar_t RUN_ON_STARTUP_KEY[] = L"RunOnStartup"; // Key for startup preference (e.g., "1" or "0")

// --- 注册表常量 ---
const wchar_t* const RUN_REGISTRY_KEY_PATH = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run";
const wchar_t* const REGISTRY_VALUE_NAME = L"NewsSchedulerApp"; // Name of the value under the Run key

// --- 自动更新配置 ---
// Consider moving URLs to the config file for flexibility.
const std::wstring UPDATE_SERVER_BASE_URL = L"http://news.lcez.fun"; // Base URL for the update server
const std::wstring UPDATE_CHECK_URL = UPDATE_SERVER_BASE_URL + L"/api/upload.php?action=latest"; // URL to check for latest version info
const std::wstring UPDATE_PING_URL = UPDATE_SERVER_BASE_URL + L"/api/upload.php?action=ping"; // URL to check server reachability
const std::wstring CURRENT_VERSION = L"2.1.1"; // Current version of *this* application
const int CHECK_UPDATE_INTERVAL = 3600; // Interval in seconds for checking updates (1 hour)
const int CONNECT_TIMEOUT_MS = 15000; // Timeout for network connections in milliseconds (15 seconds)
const int DOWNLOAD_TIMEOUT_MS = 60000; // Timeout for file downloads in milliseconds (60 seconds)
// MAX_URL_LENGTH is defined as constexpr in globals.h, NO definition needed here.

// --- 心跳配置 ---
// Consider moving URLs to the config file.
const std::wstring HEARTBEAT_API_URL = UPDATE_SERVER_BASE_URL + L"/api/heartbeat.php"; // URL for sending heartbeat pings
const std::wstring STATUS_API_URL = UPDATE_SERVER_BASE_URL + L"/api/status.php";       // URL for potentially fetching status info
const int HEARTBEAT_INTERVAL_SECONDS = 180; // Interval in seconds for sending heartbeats (3 minutes)

// --- 全局状态 ---
std::atomic_bool g_bRunning(true);                // Flag to signal threads to terminate
std::mutex g_logMutex;                            // Mutex for thread-safe writing to the log file
std::mutex g_logCacheMutex;                       // Mutex for thread-safe access to the log cache
std::deque<std::wstring> g_logCache;              // In-memory cache for recent log messages (for UI display)
HWND g_hWnd = nullptr;                            // Handle to the main application window
HWND g_hProgressDlg = nullptr;                    // Handle to the modeless progress dialog
HWND g_hLogEdit = nullptr;                        // Handle to the edit control displaying logs (if applicable)
NOTIFYICONDATAW g_notifyIconData = { sizeof(NOTIFYICONDATAW) }; // Structure for the system tray icon (initialized size)
std::atomic_bool g_bAutoUpdatePaused(false);      // Flag to pause/resume automatic update checks
std::atomic_bool g_bCheckUpdateNow(false);        // Flag to trigger an immediate update check
const int MAX_LOG_LINES_DISPLAY = 100;            // Maximum number of log lines to keep in the memory cache
std::wstring g_deviceID = L"";                    // Global device ID, initialized by InitializeDeviceID()

// --- RAII 类实现 ---

// InternetHandle Implementation
InternetHandle::InternetHandle() : m_handle(nullptr) {}
InternetHandle::InternetHandle(HINTERNET h) : m_handle(h) {}
InternetHandle::~InternetHandle() {
    if (m_handle) {
        InternetCloseHandle(m_handle);
        m_handle = nullptr; // Good practice to null after closing
    }
}
InternetHandle::InternetHandle(InternetHandle&& other) noexcept : m_handle(other.m_handle) {
    other.m_handle = nullptr; // Take ownership, leave other empty
}
InternetHandle& InternetHandle::operator=(InternetHandle&& other) noexcept {
    if (this != &other) {
        if (m_handle) { InternetCloseHandle(m_handle); } // Close existing handle
        m_handle = other.m_handle;                       // Take ownership
        other.m_handle = nullptr;                        // Leave other empty
    }
    return *this;
}
HINTERNET InternetHandle::get() const { return m_handle; }
InternetHandle::operator bool() const { return m_handle != nullptr; }
HINTERNET InternetHandle::release() {
    HINTERNET temp = m_handle;
    m_handle = nullptr; // Release ownership
    return temp;
}
InternetHandle& InternetHandle::operator=(HINTERNET h) {
    if (m_handle && m_handle != h) { // Avoid self-assignment and closing same handle
        InternetCloseHandle(m_handle);
    }
    m_handle = h;
    return *this;
}

// FileHandle Implementation
FileHandle::FileHandle() : m_handle(INVALID_HANDLE_VALUE) {}
FileHandle::FileHandle(HANDLE h) : m_handle(h) {}
FileHandle::~FileHandle() {
    if (m_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(m_handle);
        m_handle = INVALID_HANDLE_VALUE; // Good practice
    }
}
FileHandle::FileHandle(FileHandle&& other) noexcept : m_handle(other.m_handle) {
    other.m_handle = INVALID_HANDLE_VALUE;
}
FileHandle& FileHandle::operator=(FileHandle&& other) noexcept {
    if (this != &other) {
        if (m_handle != INVALID_HANDLE_VALUE) { CloseHandle(m_handle); }
        m_handle = other.m_handle;
        other.m_handle = INVALID_HANDLE_VALUE;
    }
    return *this;
}
HANDLE FileHandle::get() const { return m_handle; }
FileHandle::operator bool() const { return m_handle != INVALID_HANDLE_VALUE; }
HANDLE FileHandle::release() {
    HANDLE temp = m_handle;
    m_handle = INVALID_HANDLE_VALUE;
    return temp;
}
FileHandle& FileHandle::operator=(HANDLE h) {
    if (m_handle != INVALID_HANDLE_VALUE && m_handle != h) {
        CloseHandle(m_handle);
    }
    m_handle = h;
    return *this;
}

// RegKeyHandle Implementation
RegKeyHandle::RegKeyHandle() : m_hKey(NULL) {}
RegKeyHandle::RegKeyHandle(HKEY h) : m_hKey(h) {}
RegKeyHandle::~RegKeyHandle() {
    if (m_hKey) {
        RegCloseKey(m_hKey);
        m_hKey = NULL; // Good practice
    }
}
RegKeyHandle::RegKeyHandle(RegKeyHandle&& other) noexcept : m_hKey(other.m_hKey) {
    other.m_hKey = NULL;
}
RegKeyHandle& RegKeyHandle::operator=(RegKeyHandle&& other) noexcept {
    if (this != &other) {
        if (m_hKey) { RegCloseKey(m_hKey); }
        m_hKey = other.m_hKey;
        other.m_hKey = NULL;
    }
    return *this;
}
HKEY RegKeyHandle::get() const { return m_hKey; }
RegKeyHandle::operator bool() const { return m_hKey != NULL; }
HKEY RegKeyHandle::release() {
    HKEY temp = m_hKey;
    m_hKey = NULL;
    return temp;
}
RegKeyHandle& RegKeyHandle::operator=(HKEY h) {
    if (m_hKey && m_hKey != h) {
        RegCloseKey(m_hKey);
    }
    m_hKey = h;
    return *this;
}
