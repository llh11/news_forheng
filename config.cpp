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
    Log("���ڳ�ʼ���豸 ID..."); // "Initializing Device ID..."
    if (!EnsureDirectoryExists(CONFIG_DIR)) {
        Log("�����޷����ʻ򴴽�����Ŀ¼: " + wstring_to_utf8(CONFIG_DIR) + ". �޷���ʼ���豸 ID��"); // "Error: Cannot access or create config directory... Cannot initialize Device ID."
        g_deviceID = L"CONFIG_DIR_ERROR"; return;
    }
    const DWORD bufferSize = 100; wchar_t buffer[bufferSize];
    DWORD charsRead = GetPrivateProfileStringW(SYSTEM_SECTION, DEVICE_ID_KEY, L"", buffer, bufferSize, CONFIG_PATH.c_str());
    if (charsRead > 0) {
        if (wcslen(buffer) == 36) { g_deviceID = buffer; Log("�����ü����豸 ID: " + wstring_to_utf8(g_deviceID)); } // "Device ID loaded from config: "
        else { Log("���棺���������ҵ����豸 ID ��ʽ��Ч�����Ȳ��� 36�������������µ��豸 ID..."); goto GenerateNewID; } // "Warning: Device ID found in config has invalid format (length != 36). Generating new Device ID..."
    }
    else {
        DWORD lastError = GetLastError(); if (lastError != ERROR_SUCCESS) { Log("���󣺶�ȡ�豸 ID ʱ�����������: " + std::to_string(lastError) + " (" + std::system_category().message(lastError) + ")"); } // "Error: Failed reading Device ID. Error code: "
        Log("��������δ�ҵ��豸 ID�����������µ��豸 ID..."); goto GenerateNewID; // "Device ID not found in config. Generating new Device ID..."
    } return;
GenerateNewID:
    g_deviceID = GenerateUUIDString();
    if (!g_deviceID.empty()) {
        if (WritePrivateProfileStringW(SYSTEM_SECTION, DEVICE_ID_KEY, g_deviceID.c_str(), CONFIG_PATH.c_str())) { Log("�����ɲ������µ��豸 ID: " + wstring_to_utf8(g_deviceID)); } // "New Device ID generated and saved: "
        else { DWORD lastError = GetLastError(); Log("�����޷����µ��豸 ID ���浽�����ļ����������: " + std::to_string(lastError) + " (" + std::system_category().message(lastError) + ")"); } // "Error: Failed to save new Device ID to config file. Error code: "
    }
    else { Log("���ش��������µ��豸 ID ʧ�ܡ�Ӧ�ó�������޷��������С�"); g_deviceID = L"GENERATION_FAILED"; } // "CRITICAL Error: Failed to generate new Device ID. Application might not function correctly."
}


// Validates essential settings in the configuration file.
bool ValidateConfig() {
    Log("������֤�����ļ�: " + wstring_to_utf8(CONFIG_PATH)); // "Validating configuration file: "
    DWORD fileAttr = GetFileAttributesW(CONFIG_PATH.c_str());
    if (fileAttr == INVALID_FILE_ATTRIBUTES) { Log("�����ļ������ڡ���Ҫ���á������� Configurator.exe��"); return false; } // "Configuration file does not exist. Configuration needed. Please run Configurator.exe."
    if (fileAttr & FILE_ATTRIBUTE_DIRECTORY) { Log("��������·����һ��Ŀ¼���������ļ���"); return false; } // "Error: Configuration path is a directory, not a file."

    wchar_t buffer[MAX_URL_LENGTH];

    auto checkStringKey = [&](const wchar_t* keyName, const std::wstring& logKeyName) {
        DWORD result = GetPrivateProfileStringW(APP_SECTION, keyName, L"", buffer, MAX_URL_LENGTH, CONFIG_PATH.c_str());
        if (result == 0) {
            DWORD lastError = GetLastError(); if (lastError != ERROR_SUCCESS) { Log("���󣺶�ȡ���ü� '" + wstring_to_utf8(logKeyName) + "' ʱ�����������: " + std::to_string(lastError) + " (" + std::system_category().message(lastError) + ")"); } // "Error: Failed reading config key '...'. Error code: "
            else { Log("���ü� (" + wstring_to_utf8(logKeyName) + ") ȱʧ��Ϊ�ա���Ҫ�������á�"); } return false; // "Config key (...) missing or empty. Re-configuration needed."
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
            if (checkResult == 0 || wcscmp(checkBuffer, L"__NOT_FOUND__") == 0) { Log("���ü� (" + wstring_to_utf8(logKeyName) + ") ȱʧ����Ҫ�������á�"); return false; } // "Config key (...) missing. Re-configuration needed."
            Log("���ü� (" + wstring_to_utf8(logKeyName) + ") ������Ч������ֵ����Ҫ�������á�"); return false; // "Config key (...) contains invalid integer value. Re-configuration needed."
        }
        if (static_cast<int>(value) < minVal || static_cast<int>(value) > maxVal) {
            Log("���ü� (" + wstring_to_utf8(logKeyName) + ") ��ֵ (" + std::to_string(value) + ") ������Ч��Χ [" + std::to_string(minVal) + "-" + std::to_string(maxVal) + "]����Ҫ�������á�"); return false; // "Config key (...) value (...) out of valid range [...]. Re-configuration needed."
        } return true;
        };
    if (!checkIntKey(NOON_SHUTDOWN_HOUR_KEY, L"NoonShutdownHour", 0, 23)) return false;
    if (!checkIntKey(NOON_SHUTDOWN_MINUTE_KEY, L"NoonShutdownMinute", 0, 59)) return false;
    if (!checkIntKey(EVENING_SHUTDOWN_HOUR_KEY, L"EveningShutdownHour", 0, 23)) return false;
    if (!checkIntKey(EVENING_SHUTDOWN_MINUTE_KEY, L"EveningShutdownMinute", 0, 59)) return false;

    GetPrivateProfileIntW(SYSTEM_SECTION, RUN_ON_STARTUP_KEY, 0, CONFIG_PATH.c_str());
    Log("�����ļ���֤ͨ����"); // "Configuration file validation passed."
    return true;
}
