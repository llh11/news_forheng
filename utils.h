#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <vector>
#include <windows.h> // For MAX_PATH, GetModuleFileNameW etc.

/**
 * @brief 获取当前可执行文件的完整路径。
 * @return 可执行文件的路径，失败则返回空字符串。
 */
std::wstring GetExecutablePath();

/**
 * @brief 获取当前可执行文件所在的目录。
 * @return 可执行文件所在目录的路径，失败则返回空字符串。
 */
std::wstring GetExecutableDir();

/**
 * @brief 检查目录是否存在。
 * @param dirPath 目录路径。
 * @return 如果目录存在则返回 true，否则返回 false。
 */
bool DirectoryExists(const std::wstring& dirPath);

/**
 * @brief 递归创建目录。
 * @param dirPath 要创建的目录路径。
 * @return 如果成功创建或目录已存在则返回 true，否则返回 false。
 */
bool CreateDirectoryRecursive(const std::wstring& dirPath);

/**
 * @brief 将 UTF-8 编码的字符串转换为宽字符串 (wstring)。
 * @param utf8String UTF-8 字符串。
 * @return 转换后的宽字符串，失败则返回空字符串。
 */
std::wstring Utf8ToWide(const std::string& utf8String);

/**
 * @brief 将宽字符串 (wstring) 转换为 UTF-8 编码的字符串。
 * @param wideString 宽字符串。
 * @return 转换后的 UTF-8 字符串，失败则返回空字符串。
 */
std::string WideToUtf8(const std::wstring& wideString);

/**
 * @brief 替换字符串中所有出现的子串。
 * @param str 主字符串 (将被修改)。
 * @param from 要被替换的子串。
 * @param to 用于替换的子串。
 */
void ReplaceAll(std::wstring& str, const std::wstring& from, const std::wstring& to);

/**
 * @brief 检查文件是否存在。
 * @param filePath 文件路径。
 * @return 如果文件存在则返回 true，否则返回 false。
 */
bool FileExists(const std::wstring& filePath);

/**
 * @brief 读取整个文件的内容到字符串。
 * @param filePath 文件路径。
 * @param content 读取到的文件内容 (输出参数)。
 * @return 如果成功读取则返回 true，否则返回 false。
 */
bool ReadFileToString(const std::wstring& filePath, std::string& content); // 读取为 byte string
bool ReadFileToWString(const std::wstring& filePath, std::wstring& content); // 读取为 wide string (假设文件编码兼容)


#endif // UTILS_H
