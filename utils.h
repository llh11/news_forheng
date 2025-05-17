#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <vector>
#include <windows.h> // For MAX_PATH, GetModuleFileNameW etc.

/**
 * @brief ��ȡ��ǰ��ִ���ļ�������·����
 * @return ��ִ���ļ���·����ʧ���򷵻ؿ��ַ�����
 */
std::wstring GetExecutablePath();

/**
 * @brief ��ȡ��ǰ��ִ���ļ����ڵ�Ŀ¼��
 * @return ��ִ���ļ�����Ŀ¼��·����ʧ���򷵻ؿ��ַ�����
 */
std::wstring GetExecutableDir();

/**
 * @brief ���Ŀ¼�Ƿ���ڡ�
 * @param dirPath Ŀ¼·����
 * @return ���Ŀ¼�����򷵻� true�����򷵻� false��
 */
bool DirectoryExists(const std::wstring& dirPath);

/**
 * @brief �ݹ鴴��Ŀ¼��
 * @param dirPath Ҫ������Ŀ¼·����
 * @return ����ɹ�������Ŀ¼�Ѵ����򷵻� true�����򷵻� false��
 */
bool CreateDirectoryRecursive(const std::wstring& dirPath);

/**
 * @brief �� UTF-8 ������ַ���ת��Ϊ���ַ��� (wstring)��
 * @param utf8String UTF-8 �ַ�����
 * @return ת����Ŀ��ַ�����ʧ���򷵻ؿ��ַ�����
 */
std::wstring Utf8ToWide(const std::string& utf8String);

/**
 * @brief �����ַ��� (wstring) ת��Ϊ UTF-8 ������ַ�����
 * @param wideString ���ַ�����
 * @return ת����� UTF-8 �ַ�����ʧ���򷵻ؿ��ַ�����
 */
std::string WideToUtf8(const std::wstring& wideString);

/**
 * @brief �滻�ַ��������г��ֵ��Ӵ���
 * @param str ���ַ��� (�����޸�)��
 * @param from Ҫ���滻���Ӵ���
 * @param to �����滻���Ӵ���
 */
void ReplaceAll(std::wstring& str, const std::wstring& from, const std::wstring& to);

/**
 * @brief ����ļ��Ƿ���ڡ�
 * @param filePath �ļ�·����
 * @return ����ļ������򷵻� true�����򷵻� false��
 */
bool FileExists(const std::wstring& filePath);

/**
 * @brief ��ȡ�����ļ������ݵ��ַ�����
 * @param filePath �ļ�·����
 * @param content ��ȡ�����ļ����� (�������)��
 * @return ����ɹ���ȡ�򷵻� true�����򷵻� false��
 */
bool ReadFileToString(const std::wstring& filePath, std::string& content); // ��ȡΪ byte string
bool ReadFileToWString(const std::wstring& filePath, std::wstring& content); // ��ȡΪ wide string (�����ļ��������)


#endif // UTILS_H
