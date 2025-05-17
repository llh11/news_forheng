#include "registry.h"
#include "log.h" // 用于记录日志
#include <shlwapi.h> // For SHDeleteKeyW

#pragma comment(lib, "Shlwapi.lib") // For SHDeleteKeyW

Registry::Registry() {
    // 构造函数
}

HKEY Registry::OpenKey(HKEY hKeyRoot, const std::wstring& subKey, REGSAM samDesired, bool createIfNotExist) {
    HKEY hKey = NULL;
    LSTATUS status;

    if (createIfNotExist) {
        DWORD dwDisposition;
        status = RegCreateKeyExW(
            hKeyRoot,
            subKey.c_str(),
            0,          // Reserved
            NULL,       // Class
            REG_OPTION_NON_VOLATILE,
            samDesired,
            NULL,       // Security Attributes
            &hKey,
            &dwDisposition // Receives REG_CREATED_NEW_KEY or REG_OPENED_EXISTING_KEY
        );
    }
    else {
        status = RegOpenKeyExW(
            hKeyRoot,
            subKey.c_str(),
            0, // Options (0 or REG_OPTION_OPEN_LINK)
            samDesired,
            &hKey
        );
    }

    if (status != ERROR_SUCCESS) {
        // LOG_WARNING(L"Failed to open/create registry key: ", subKey, L". Error: ", status);
        if (hKey) RegCloseKey(hKey); // Defensive, though hKey should be NULL on failure
        return NULL;
    }
    return hKey;
}

void Registry::CloseKey(HKEY hKey) {
    if (hKey != NULL) {
        RegCloseKey(hKey);
    }
}

bool Registry::ReadString(HKEY hKeyRoot, const std::wstring& subKey, const std::wstring& valueName, std::wstring& outValue) {
    HKEY hKey = OpenKey(hKeyRoot, subKey, KEY_READ);
    if (!hKey) {
        return false;
    }

    DWORD dwType = 0;
    DWORD dwSize = 0; // Size in bytes

    // First, query for the size of the data
    LSTATUS status = RegQueryValueExW(hKey, valueName.c_str(), NULL, &dwType, NULL, &dwSize);
    if (status != ERROR_SUCCESS || dwType != REG_SZ) {
        // LOG_WARNING(L"Failed to query registry value size or type mismatch for: ", valueName, L" in ", subKey, L". Error: ", status);
        CloseKey(hKey);
        return false;
    }

    if (dwSize == 0) { // Empty string
        outValue = L"";
        CloseKey(hKey);
        return true;
    }

    // dwSize is in bytes. For wstring, we need to allocate characters.
    // std::vector<wchar_t> buffer(dwSize / sizeof(wchar_t) + 1); // +1 for safety, though dwSize should include null terminator
    outValue.resize(dwSize / sizeof(wchar_t));


    status = RegQueryValueExW(hKey, valueName.c_str(), NULL, &dwType, reinterpret_cast<LPBYTE>(&outValue[0]), &dwSize);
    CloseKey(hKey);

    if (status != ERROR_SUCCESS) {
        // LOG_WARNING(L"Failed to read registry string value: ", valueName, L" in ", subKey, L". Error: ", status);
        outValue.clear();
        return false;
    }

    // RegQueryValueExW (for REG_SZ) includes the null terminator in dwSize if the buffer is large enough.
    // The wstring.resize might make the string one char too long if dwSize included the null.
    // We need to ensure the string is correctly terminated.
    // If dwSize was the size *including* the null terminator, then outValue.resize(dwSize / sizeof(wchar_t))
    // might make a string that has a \0 in the middle and then garbage.
    // A safer way for wstring:
    outValue.resize(wcslen(outValue.c_str())); // Trim to actual length before any embedded nulls from over-allocation.

    return true;
}

bool Registry::WriteString(HKEY hKeyRoot, const std::wstring& subKey, const std::wstring& valueName, const std::wstring& value) {
    HKEY hKey = OpenKey(hKeyRoot, subKey, KEY_WRITE | KEY_CREATE_SUB_KEY, true); // Create key if it doesn't exist
    if (!hKey) {
        return false;
    }

    // Value data must include the null terminator, so (value.length() + 1) * sizeof(wchar_t)
    DWORD dwSize = (static_cast<DWORD>(value.length()) + 1) * sizeof(wchar_t);

    LSTATUS status = RegSetValueExW(
        hKey,
        valueName.c_str(),
        0, // Reserved
        REG_SZ,
        reinterpret_cast<const BYTE*>(value.c_str()),
        dwSize
    );
    CloseKey(hKey);

    if (status != ERROR_SUCCESS) {
        // LOG_ERROR(L"Failed to write registry string value: ", valueName, L" in ", subKey, L". Error: ", status);
        return false;
    }
    return true;
}

bool Registry::ReadDword(HKEY hKeyRoot, const std::wstring& subKey, const std::wstring& valueName, DWORD& outValue) {
    HKEY hKey = OpenKey(hKeyRoot, subKey, KEY_READ);
    if (!hKey) {
        return false;
    }

    DWORD dwType = 0;
    DWORD dwSize = sizeof(DWORD);
    LSTATUS status = RegQueryValueExW(hKey, valueName.c_str(), NULL, &dwType, reinterpret_cast<LPBYTE>(&outValue), &dwSize);
    CloseKey(hKey);

    if (status != ERROR_SUCCESS || dwType != REG_DWORD) {
        // LOG_WARNING(L"Failed to read registry DWORD value or type mismatch for: ", valueName, L" in ", subKey, L". Error: ", status);
        return false;
    }
    return true;
}

bool Registry::WriteDword(HKEY hKeyRoot, const std::wstring& subKey, const std::wstring& valueName, DWORD value) {
    HKEY hKey = OpenKey(hKeyRoot, subKey, KEY_WRITE | KEY_CREATE_SUB_KEY, true);
    if (!hKey) {
        return false;
    }

    LSTATUS status = RegSetValueExW(
        hKey,
        valueName.c_str(),
        0,
        REG_DWORD,
        reinterpret_cast<const BYTE*>(&value),
        sizeof(DWORD)
    );
    CloseKey(hKey);

    if (status != ERROR_SUCCESS) {
        // LOG_ERROR(L"Failed to write registry DWORD value: ", valueName, L" in ", subKey, L". Error: ", status);
        return false;
    }
    return true;
}

bool Registry::DeleteKey(HKEY hKeyRoot, const std::wstring& subKey, bool recursive) {
    LSTATUS status;
    if (recursive) {
        // SHDeleteKeyW deletes a key and all its subkeys and values.
        // It is available in shlwapi.dll.
        status = SHDeleteKeyW(hKeyRoot, subKey.c_str());
    }
    else {
        // RegDeleteKeyW only deletes a key if it has no subkeys.
        // For deleting a key that might have subkeys but you don't want to use SHDeleteKeyW,
        // you would need to recursively enumerate and delete subkeys first.
        status = RegDeleteKeyW(hKeyRoot, subKey.c_str());
    }

    if (status != ERROR_SUCCESS) {
        // ERROR_FILE_NOT_FOUND is common if the key doesn't exist, not necessarily an error to log loudly.
        // if (status != ERROR_FILE_NOT_FOUND) {
        //    LOG_WARNING(L"Failed to delete registry key: ", subKey, L". Error: ", status);
        // }
        return false;
    }
    return true;
}


bool Registry::DeleteValue(HKEY hKeyRoot, const std::wstring& subKey, const std::wstring& valueName) {
    HKEY hKey = OpenKey(hKeyRoot, subKey, KEY_SET_VALUE); // KEY_SET_VALUE is required to delete a value
    if (!hKey) {
        return false;
    }

    LSTATUS status = RegDeleteValueW(hKey, valueName.c_str());
    CloseKey(hKey);

    if (status != ERROR_SUCCESS) {
        // if (status != ERROR_FILE_NOT_FOUND) { // Value not found is not always an error
        //    LOG_WARNING(L"Failed to delete registry value: ", valueName, L" in ", subKey, L". Error: ", status);
        // }
        return false;
    }
    return true;
}

bool Registry::KeyExists(HKEY hKeyRoot, const std::wstring& subKey) {
    HKEY hKey = OpenKey(hKeyRoot, subKey, KEY_READ);
    if (hKey) {
        CloseKey(hKey);
        return true;
    }
    return false;
}

bool Registry::ValueExists(HKEY hKeyRoot, const std::wstring& subKey, const std::wstring& valueName) {
    HKEY hKey = OpenKey(hKeyRoot, subKey, KEY_READ);
    if (!hKey) {
        return false; // Key itself doesn't exist or cannot be opened
    }

    LSTATUS status = RegQueryValueExW(hKey, valueName.c_str(), NULL, NULL, NULL, NULL);
    CloseKey(hKey);

    return status == ERROR_SUCCESS;
}

bool Registry::EnumSubKeys(HKEY hKeyRoot, const std::wstring& subKey, std::vector<std::wstring>& outSubKeyNames) {
    outSubKeyNames.clear();
    HKEY hKey = OpenKey(hKeyRoot, subKey, KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE);
    if (!hKey) {
        return false;
    }

    wchar_t    achKey[MAX_PATH]; // Buffer for subkey name
    DWORD    cbName;             // Size of name string 
    DWORD    cSubKeys = 0;       // Number of subkeys 

    // Get the class name and the value count. 
    RegQueryInfoKey(
        hKey,                  // key handle 
        NULL,                // buffer for class name 
        NULL,                // size of class string 
        NULL,                // reserved 
        &cSubKeys,           // number of subkeys 
        NULL,                // longest subkey size 
        NULL,                // longest class string 
        NULL,                // number of values for this key 
        NULL,                // longest value name 
        NULL,                // longest value data 
        NULL,                // security descriptor 
        NULL);               // last write time 

    if (cSubKeys > 0) {
        for (DWORD i = 0; i < cSubKeys; i++) {
            cbName = MAX_PATH;
            LSTATUS status = RegEnumKeyExW(hKey, i,
                achKey,
                &cbName,
                NULL,
                NULL,
                NULL,
                NULL);
            if (status == ERROR_SUCCESS) {
                outSubKeyNames.push_back(achKey);
            }
            else {
                // LOG_WARNING(L"Failed to enumerate subkey at index ", i, L" for key ", subKey, L". Error: ", status);
                // Decide if to continue or bail out
            }
        }
    }
    CloseKey(hKey);
    return true; // Or return false if any RegEnumKeyExW failed
}

bool Registry::EnumValueNames(HKEY hKeyRoot, const std::wstring& subKey, std::vector<std::wstring>& outValueNames) {
    outValueNames.clear();
    HKEY hKey = OpenKey(hKeyRoot, subKey, KEY_QUERY_VALUE);
    if (!hKey) {
        return false;
    }

    wchar_t  achValue[MAX_PATH]; // Buffer for value name, MAX_VALUE_NAME from winnt.h is 16383
    DWORD    cchValue = MAX_PATH; // Size of value name buffer

    DWORD cValues;              // number of values for key 
    RegQueryInfoKey(
        hKey,                  // key handle 
        NULL,                // buffer for class name 
        NULL,                // size of class string 
        NULL,                // reserved 
        NULL,                // number of subkeys 
        NULL,                // longest subkey size 
        NULL,                // longest class string 
        &cValues,            // number of values for this key 
        NULL,                // longest value name 
        NULL,                // longest value data 
        NULL,                // security descriptor 
        NULL);               // last write time 

    if (cValues > 0) {
        for (DWORD i = 0; i < cValues; i++) {
            cchValue = MAX_PATH; // Reset buffer size for each call
            achValue[0] = L'\0'; // Clear buffer
            LSTATUS status = RegEnumValueW(hKey, i,
                achValue,
                &cchValue,
                NULL, // Reserved
                NULL, // Type
                NULL, // Data
                NULL  // Data size
            );

            if (status == ERROR_SUCCESS) {
                outValueNames.push_back(achValue);
            }
            else if (status != ERROR_NO_MORE_ITEMS) { // ERROR_NO_MORE_ITEMS can happen if count changes
                // LOG_WARNING(L"Failed to enumerate value name at index ", i, L" for key ", subKey, L". Error: ", status);
            }
        }
    }
    CloseKey(hKey);
    return true;
}
