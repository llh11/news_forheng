// ========================================================================
// network.h - 处理网络操作，如更新检查、下载、心跳
// ========================================================================
#pragma once

#include <string>
#include <Windows.h> // For HWND

// 通过 ping 特定 URL 检查更新服务器是否可达。
bool CheckServerConnection(const std::wstring& pingUrl);

// 检查更新服务器是否有更新版本。如果找到，则输出版本和 URL。
bool CheckUpdate(std::wstring& outNewVersion, std::wstring& outDownloadUrl);

// 将文件从 URL 下载到本地路径，可选择更新进度对话框。
bool DownloadFile(const std::wstring& downloadUrl, const std::wstring& savePath, HWND hProgressWnd = NULL);

// 向服务器发送带有设备 ID 和版本的心跳 POST 请求。
bool SendHeartbeat();

// 从服务器获取给定类型（例如 "configurator"）的最新文件名。
bool GetLatestFilenameFromServer(const std::wstring& type, std::wstring& outFilename);

