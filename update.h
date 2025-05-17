// ========================================================================
// update.h - 处理应用程序更新过程
// ========================================================================
#pragma once

#include <string>

// 使用自删除批处理脚本执行应用程序更新。
void PerformUpdate(const std::wstring& downloadedFilePath);

// 检查 'update_pending.inf' 是否存在，提示用户，并可能应用更新。
bool CheckAndApplyPendingUpdate();

// 将下载的更新文件的路径保存到 'update_pending.inf'。
void SavePendingUpdateInfo(const std::wstring& filePath);

// 删除 'update_pending.inf' 文件。
void ClearPendingUpdateInfo();

// 从忽略版本文件中读取版本字符串。
std::wstring ReadIgnoredVersion();

// 将版本字符串写入忽略版本文件。
void WriteIgnoredVersion(const std::wstring& version);

// 删除忽略版本文件。
void ClearIgnoredVersion();

