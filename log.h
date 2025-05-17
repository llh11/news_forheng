// ========================================================================
// log.h - 处理日志记录到文件和内存缓存
// ========================================================================
#pragma once

#include <string>
#include <vector>
#include <Windows.h> // For HWND

// 使用时间戳将 UTF-8 消息记录到文件和内存缓存中。
void Log(const std::string& message_utf8);

// 从日志文件（UTF-8）读取最后 N 行并将其作为 wstring 返回。
std::vector<std::wstring> ReadLastLogLinesW(int maxLines);

// 从内存缓存加载日志条目并在编辑控件中显示它们。
void LoadAndDisplayLogs(HWND hEdit);

