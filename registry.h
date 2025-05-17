#ifndef REGISTRY_H
#define REGISTRY_H

#include <string>
#include <windows.h> // HKEY, LSTATUS, RegOpenKeyExW etc.
#include <vector>

// 封装 Windows 注册表操作
class Registry {
public:
    Registry(); // 构造函数可以不指定根键，在具体操作时传入

    // 读取注册表字符串值
    static bool ReadString(HKEY hKeyRoot, const std::wstring& subKey, const std::wstring& valueName, std::wstring& outValue);

    // 写入注册表字符串值
    static bool WriteString(HKEY hKeyRoot, const std::wstring& subKey, const std::wstring& valueName, const std::wstring& value);

    // 读取注册表 DWORD 值
    static bool ReadDword(HKEY hKeyRoot, const std::wstring& subKey, const std::wstring& valueName, DWORD& outValue);

    // 写入注册表 DWORD 值
    static bool WriteDword(HKEY hKeyRoot, const std::wstring& subKey, const std::wstring& valueName, DWORD value);

    // 删除注册表键 (会删除该键下的所有子键和值，请谨慎使用)
    static bool DeleteKey(HKEY hKeyRoot, const std::wstring& subKey, bool recursive = true); // recursive is for SHDeleteKey functionality

    // 删除注册表值
    static bool DeleteValue(HKEY hKeyRoot, const std::wstring& subKey, const std::wstring& valueName);

    // 检查注册表键是否存在
    static bool KeyExists(HKEY hKeyRoot, const std::wstring& subKey);

    // 检查注册表值是否存在
    static bool ValueExists(HKEY hKeyRoot, const std::wstring& subKey, const std::wstring& valueName);

    // 枚举子键名
    static bool EnumSubKeys(HKEY hKeyRoot, const std::wstring& subKey, std::vector<std::wstring>& outSubKeyNames);

    // 枚举值名
    static bool EnumValueNames(HKEY hKeyRoot, const std::wstring& subKey, std::vector<std::wstring>& outValueNames);

private:
    // 辅助函数，打开键，处理权限和创建
    static HKEY OpenKey(HKEY hKeyRoot, const std::wstring& subKey, REGSAM samDesired, bool createIfNotExist = false);
    // 辅助函数，关闭键
    static void CloseKey(HKEY hKey);
};

#endif // REGISTRY_H
