// ========================================================================
// registry.h - 处理 Windows 注册表操作，特别是启动设置
// ========================================================================
#pragma once

// 通过注册表启用或禁用应用程序在 Windows 启动时运行。
bool SetStartupRegistry(bool enable);

// 从 INI 文件读取启动首选项并相应地调用 SetStartupRegistry。
void ManageStartupSetting();

