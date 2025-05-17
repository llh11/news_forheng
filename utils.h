// ========================================================================
// utils.h - ��������ʵ�ú���
// ========================================================================
#pragma once

#include <string>
#include <vector>
#include <Windows.h> // For HWND

// �� wstring (UTF-16) ת��Ϊ UTF-8 ������ַ�����
std::string wstring_to_utf8(const std::wstring& wstr);

// �� UTF-8 ������ַ���ת��Ϊ wstring (UTF-16)��
std::wstring utf8_to_wstring(const std::string& str);

// ���� JSON ת�����еĻ�����ת�塣
std::string UnescapeJsonString(const std::string& jsonEscaped);

// ���ɱ�׼ UUID �ַ�����
std::wstring GenerateUUIDString();

// ȷ��ָ����Ŀ¼·�����ڣ���Ҫʱ��������
bool EnsureDirectoryExists(const std::wstring& dirPath);

// ��ȡ Configurator.exe ��ִ���ļ�������·������������ͬһĿ¼�У���
std::wstring GetConfiguratorPath();

// ���� Configurator.exe ���ȴ����˳���
bool LaunchConfiguratorAndWait();

// ���汾�ַ��������� "1.2.3"������Ϊ����������
std::vector<int> ParseVersion(const std::wstring& versionStr);

// �Ƚ������汾���� (v1, v2)����� v1>v2 ���� >0����� v1<v2 ���� <0����� v1==v2 ���� 0��
int CompareVersions(const std::vector<int>& v1, const std::vector<int>& v2);

