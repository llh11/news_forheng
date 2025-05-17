// config.cpp
// Implements configuration handling.
#include "config.h"
#include "globals.h" // Access to config paths, keys, g_deviceID
#include "utils.h"   // For GenerateUUIDString, wstring_to_utf8, EnsureDirectoryExists
#include "log.h"     // For Log

#include <Windows.h>
#include <string>
#include <vector>
#include <system_error> // For std::error_code, std::system_category

// Loads the DeviceID from the config file or generates a new one if missing/invalid.
// (InitializeDeviceID - unchanged from previous fix)
void InitializeDeviceID() {
    Log("正在初始化设备 ID..."); // "Initializing Device ID..."
    if (!EnsureDirectoryExists(CONFIG_DIR)) {
        Log("错误：无法访问或创建配置目录: " + wstring_to_utf8(CONFIG_DIR) + ". 无法初始化设备 ID。"); // "Error: Cannot access or create config directory... Cannot initialize Device ID."
        g_deviceID = L"CONFIG_DIR_ERROR"; return;
    }
    const DWORD bufferSize = 100; wchar_t buffer[bufferSize];
    DWORD charsRead = GetPrivateProfileStringW(SYSTEM_SECTION, DEVICE_ID_KEY, L"", buffer, bufferSize, CONFIG_PATH.c_str());
    if (charsRead > 0) {
        if (wcslen(buffer) == 36) { g_deviceID = buffer; Log("从配置加载设备 ID: " + wstring_to_utf8(g_deviceID)); } // "Device ID loaded from config: "
        else { Log("警告：在配置中找到的设备 ID 格式无效（长度不是 36）。正在生成新的设备 ID..."); goto GenerateNewID; } // "Warning: Device ID found in config has invalid format (length != 36). Generating new Device ID..."
    }
    else {
        DWORD lastError = GetLastError(); if (lastError != ERROR_SUCCESS) { Log("错误：读取设备 ID 时出错。错误代码: " + std::to_string(lastError) + " (" + std::system_category().message(lastError) + ")"); } // "Error: Failed reading Device ID. Error code: "
        Log("在配置中未找到设备 ID。正在生成新的设备 ID..."); goto GenerateNewID; // "Device ID not found in config. Generating new Device ID..."
    } return;
GenerateNewID:
    g_deviceID = GenerateUUIDString();
    if (!g_deviceID.empty()) {
        if (WritePrivateProfileStringW(SYSTEM_SECTION, DEVICE_ID_KEY, g_deviceID.c_str(), CONFIG_PATH.c_str())) { Log("已生成并保存新的设备 ID: " + wstring_to_utf8(g_deviceID)); } // "New Device ID generated and saved: "
        else { DWORD lastError = GetLastError(); Log("错误：无法将新的设备 ID 保存到配置文件。错误代码: " + std::to_string(lastError) + " (" + std::system_category().message(lastError) + ")"); } // "Error: Failed to save new Device ID to config file. Error code: "
    }
    else { Log("严重错误：生成新的设备 ID 失败。应用程序可能无法正常运行。"); g_deviceID = L"GENERATION_FAILED"; } // "CRITICAL Error: Failed to generate new Device ID. Application might not function correctly."
}


// Validates essential settings in the configuration file.
bool ValidateConfig() {
    Log("正在验证配置文件: " + wstring_to_utf8(CONFIG_PATH)); // "Validating configuration file: "
    DWORD fileAttr = GetFileAttributesW(CONFIG_PATH.c_str());
    if (fileAttr == INVALID_FILE_ATTRIBUTES) { Log("配置文件不存在。需要配置。请运行 Configurator.exe。"); return false; } // "Configuration file does not exist. Configuration needed. Please run Configurator.exe."
    if (fileAttr & FILE_ATTRIBUTE_DIRECTORY) { Log("错误：配置路径是一个目录，而不是文件。"); return false; } // "Error: Configuration path is a directory, not a file."

    wchar_t buffer[MAX_URL_LENGTH];

    auto checkStringKey = [&](const wchar_t* keyName, const std::wstring& logKeyName) {
        DWORD result = GetPrivateProfileStringW(APP_SECTION, keyName, L"", buffer, MAX_URL_LENGTH, CONFIG_PATH.c_str());
        if (result == 0) {
            DWORD lastError = GetLastError(); if (lastError != ERROR_SUCCESS) { Log("错误：读取配置键 '" + wstring_to_utf8(logKeyName) + "' 时出错。错误代码: " + std::to_string(lastError) + " (" + std::system_category().message(lastError) + ")"); } // "Error: Failed reading config key '...'. Error code: "
            else { Log("配置键 (" + wstring_to_utf8(logKeyName) + ") 缺失或为空。需要重新配置。"); } return false; // "Config key (...) missing or empty. Re-configuration needed."
        } return true;
        };
    if (!checkStringKey(NEWS_URL_KEY, L"NewsUrl")) return false;
    if (!checkStringKey(WEATHER_URL_KEY, L"WeatherUrl")) return false;

    auto checkIntKey = [&](const wchar_t* keyName, const std::wstring& logKeyName, int minVal, int maxVal) {
        // <<< FIX lnt-uninitialized-local: Initialize value
        UINT value = static_cast<UINT>(-1); // Use -1 as sentinel
        value = GetPrivateProfileIntW(APP_SECTION, keyName, static_cast<UINT>(-1), CONFIG_PATH.c_str());

        if (value == static_cast<UINT>(-1)) {
            wchar_t checkBuffer[10]; DWORD checkResult = GetPrivateProfileStringW(APP_SECTION, keyName, L"__NOT_FOUND__", checkBuffer, 10, CONFIG_PATH.c_str());
            if (checkResult == 0 || wcscmp(checkBuffer, L"__NOT_FOUND__") == 0) { Log("配置键 (" + wstring_to_utf8(logKeyName) + ") 缺失。需要重新配置。"); return false; } // "Config key (...) missing. Re-configuration needed."
            Log("配置键 (" + wstring_to_utf8(logKeyName) + ") 包含无效的整数值。需要重新配置。"); return false; // "Config key (...) contains invalid integer value. Re-configuration needed."
        }
        if (static_cast<int>(value) < minVal || static_cast<int>(value) > maxVal) {
            Log("配置键 (" + wstring_to_utf8(logKeyName) + ") 的值 (" + std::to_string(value) + ") 超出有效范围 [" + std::to_string(minVal) + "-" + std::to_string(maxVal) + "]。需要重新配置。"); return false; // "Config key (...) value (...) out of valid range [...]. Re-configuration needed."
        } return true;
        };
    if (!checkIntKey(NOON_SHUTDOWN_HOUR_KEY, L"NoonShutdownHour", 0, 23)) return false;
    if (!checkIntKey(NOON_SHUTDOWN_MINUTE_KEY, L"NoonShutdownMinute", 0, 59)) return false;
    if (!checkIntKey(EVENING_SHUTDOWN_HOUR_KEY, L"EveningShutdownHour", 0, 23)) return false;
    if (!checkIntKey(EVENING_SHUTDOWN_MINUTE_KEY, L"EveningShutdownMinute", 0, 59)) return false;

    GetPrivateProfileIntW(SYSTEM_SECTION, RUN_ON_STARTUP_KEY, 0, CONFIG_PATH.c_str());
    Log("配置文件验证通过。"); // "Configuration file validation passed."
    return true;
}
