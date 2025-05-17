// registry.cpp
// Implements Windows Registry operations, specifically for startup settings.
#include "registry.h" // Function declarations for this file
#include "globals.h"  // Access to global constants (REGISTRY_VALUE_NAME, RUN_REGISTRY_KEY_PATH), RAII class (RegKeyHandle), config path constants
#include "log.h"      // For Log function
#include "utils.h"    // For wstring_to_utf8

#include <Windows.h>
#include <string>
#include <system_error> // For std::system_category

// Enables or disables the application from running at Windows startup via the Registry.
// HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run
bool SetStartupRegistry(bool enable) {
    Log(enable ? "������������ע�����..." : "���ڽ�������ע�����..."); // "Enabling startup registry entry..." : "Disabling startup registry entry..."

    RegKeyHandle hKey; // RAII handle for the registry key
    HKEY rawHKey = NULL;

    // Open the Run key under HKEY_CURRENT_USER with write access
    LSTATUS status = RegOpenKeyExW(
        HKEY_CURRENT_USER,        // Root key
        RUN_REGISTRY_KEY_PATH,    // Subkey path
        0,                        // Reserved, must be zero
        KEY_WRITE,                // Desired access (write for setting/deleting value)
        &rawHKey                  // Pointer to receive the opened key handle
    );

    if (status != ERROR_SUCCESS) {
        Log("�����޷���ע��� Run ����RegOpenKeyExW ʧ�ܡ��������: " + std::to_string(status) + " (" + std::system_category().message(status) + ")"); // "Error: Cannot open Registry Run key. RegOpenKeyExW failed. Error code: "
        return false;
    }
    hKey = rawHKey; // Assign to RAII handle

    if (enable) {
        // Get the full path to the current executable
        wchar_t exePath[MAX_PATH] = { 0 };
        DWORD pathLen = GetModuleFileNameW(NULL, exePath, MAX_PATH);
        if (pathLen == 0 || pathLen == MAX_PATH) {
            DWORD error = GetLastError();
            Log("�����޷���ȡ��ǰ��ִ���ļ�·�������������GetModuleFileNameW ʧ�ܻ�·���������������: " + std::to_string(error)); // "Error: Cannot get current executable path for startup entry. GetModuleFileNameW failed or path too long. Error code: "
            return false; // Cannot enable without the path
        }

        // Add quotes around the path in case it contains spaces
        std::wstring quotedExePath = L"\"";
        quotedExePath += exePath;
        quotedExePath += L"\"";

        // Set the registry value
        // The data is the full quoted path to the executable.
        status = RegSetValueExW(
            hKey.get(),               // Handle to the opened key
            REGISTRY_VALUE_NAME,      // Name of the value to set
            0,                        // Reserved, must be zero
            REG_SZ,                   // Type of data (null-terminated string)
            reinterpret_cast<const BYTE*>(quotedExePath.c_str()), // Pointer to the data (quoted path)
            static_cast<DWORD>((quotedExePath.length() + 1) * sizeof(wchar_t)) // Size of the data in bytes (including null terminator)
        );

        if (status != ERROR_SUCCESS) {
            Log("�����޷���������ע���ֵ��RegSetValueExW ʧ�ܡ��������: " + std::to_string(status) + " (" + std::system_category().message(status) + ")"); // "Error: Cannot set startup registry value. RegSetValueExW failed. Error code: "
            return false;
        }
        else {
            Log("����ע������ѳɹ����á�ֵ: " + wstring_to_utf8(REGISTRY_VALUE_NAME) + " = " + wstring_to_utf8(quotedExePath)); // "Startup registry entry enabled successfully. Value: ... = ..."
        }
    }
    else {
        // Disable: Delete the registry value
        status = RegDeleteValueW(
            hKey.get(),           // Handle to the opened key
            REGISTRY_VALUE_NAME   // Name of the value to delete
        );

        if (status != ERROR_SUCCESS && status != ERROR_FILE_NOT_FOUND) {
            // Log error only if deletion failed for a reason other than "not found"
            Log("�����޷�ɾ������ע���ֵ��RegDeleteValueW ʧ�ܡ��������: " + std::to_string(status) + " (" + std::system_category().message(status) + ")"); // "Error: Cannot delete startup registry value. RegDeleteValueW failed. Error code: "
            return false;
        }
        else {
            if (status == ERROR_FILE_NOT_FOUND) {
                Log("����ע�����δ�ҵ���������ã���"); // "Startup registry entry not found (no need to disable)."
            }
            else {
                Log("����ע������ѳɹ����á�"); // "Startup registry entry disabled successfully."
            }
        }
    }

    // hKey is closed automatically by RAII destructor
    return true;
}

// Reads the startup preference from the INI file and calls SetStartupRegistry accordingly.
void ManageStartupSetting() {
    Log("���ڹ�����������..."); // "Managing startup setting..."

    // Read the "RunOnStartup" value from the [System] section of the config file
    // Default to 0 (disabled) if the key is not found or invalid.
    int runOnStartup = GetPrivateProfileIntW(
        SYSTEM_SECTION,       // Section name
        RUN_ON_STARTUP_KEY,   // Key name
        0,                    // Default value (0 = disabled)
        CONFIG_PATH.c_str()   // Path to the INI file
    );

    Log("�����ö�ȡ�� RunOnStartup ����: " + std::to_string(runOnStartup)); // "RunOnStartup setting read from config: "

    // Enable startup entry if the value is non-zero (typically 1)
    bool enable = (runOnStartup != 0);

    // Call SetStartupRegistry to apply the setting
    if (!SetStartupRegistry(enable)) {
        // Log if applying the setting failed
        Log("����Ӧ������ע�������ʧ�ܡ�"); // "Error: Failed to apply startup registry setting."
        // Consider notifying the user if this fails persistently.
    }
    else {
        Log("����ע��������Ѹ������óɹ�Ӧ�á�"); // "Startup registry setting applied successfully based on config."
    }
}