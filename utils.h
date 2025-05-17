// ========================================================================
// utils.h - 包含各种实用函数
// ========================================================================
#pragma once

#include <string>
#include <vector>
#include <Windows.h> // For HWND

// 将 wstring (UTF-16) 转换为 UTF-8 编码的字符串。
std::string wstring_to_utf8(const std::wstring& wstr);

// 将 UTF-8 编码的字符串转换为 wstring (UTF-16)。
std::wstring utf8_to_wstring(const std::string& str);

// 常见 JSON 转义序列的基本反转义。
std::string UnescapeJsonString(const std::string& jsonEscaped);

// 生成标准 UUID 字符串。
std::wstring GenerateUUIDString();

// 确保指定的目录路径存在，必要时创建它。
bool EnsureDirectoryExists(const std::wstring& dirPath);

// 获取 Configurator.exe 可执行文件的完整路径（假设它在同一目录中）。
std::wstring GetConfiguratorPath();

// 启动 Configurator.exe 并等待其退出。
bool LaunchConfiguratorAndWait();

// 将版本字符串（例如 "1.2.3"）解析为整数向量。
std::vector<int> ParseVersion(const std::wstring& versionStr);

// 比较两个版本向量 (v1, v2)。如果 v1>v2 返回 >0，如果 v1<v2 返回 <0，如果 v1==v2 返回 0。
int CompareVersions(const std::vector<int>& v1, const std::vector<int>& v2);

