#ifndef REGISTRY_H
#define REGISTRY_H

#include <string>
#include <windows.h> // HKEY, LSTATUS, RegOpenKeyExW etc.
#include <vector>

// ��װ Windows ע������
class Registry {
public:
    Registry(); // ���캯�����Բ�ָ���������ھ������ʱ����

    // ��ȡע����ַ���ֵ
    static bool ReadString(HKEY hKeyRoot, const std::wstring& subKey, const std::wstring& valueName, std::wstring& outValue);

    // д��ע����ַ���ֵ
    static bool WriteString(HKEY hKeyRoot, const std::wstring& subKey, const std::wstring& valueName, const std::wstring& value);

    // ��ȡע��� DWORD ֵ
    static bool ReadDword(HKEY hKeyRoot, const std::wstring& subKey, const std::wstring& valueName, DWORD& outValue);

    // д��ע��� DWORD ֵ
    static bool WriteDword(HKEY hKeyRoot, const std::wstring& subKey, const std::wstring& valueName, DWORD value);

    // ɾ��ע���� (��ɾ���ü��µ������Ӽ���ֵ�������ʹ��)
    static bool DeleteKey(HKEY hKeyRoot, const std::wstring& subKey, bool recursive = true); // recursive is for SHDeleteKey functionality

    // ɾ��ע���ֵ
    static bool DeleteValue(HKEY hKeyRoot, const std::wstring& subKey, const std::wstring& valueName);

    // ���ע�����Ƿ����
    static bool KeyExists(HKEY hKeyRoot, const std::wstring& subKey);

    // ���ע���ֵ�Ƿ����
    static bool ValueExists(HKEY hKeyRoot, const std::wstring& subKey, const std::wstring& valueName);

    // ö���Ӽ���
    static bool EnumSubKeys(HKEY hKeyRoot, const std::wstring& subKey, std::vector<std::wstring>& outSubKeyNames);

    // ö��ֵ��
    static bool EnumValueNames(HKEY hKeyRoot, const std::wstring& subKey, std::vector<std::wstring>& outValueNames);

private:
    // �����������򿪼�������Ȩ�޺ʹ���
    static HKEY OpenKey(HKEY hKeyRoot, const std::wstring& subKey, REGSAM samDesired, bool createIfNotExist = false);
    // �����������رռ�
    static void CloseKey(HKEY hKey);
};

#endif // REGISTRY_H
