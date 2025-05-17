// ========================================================================
// system_ops.h - 处理系统级操作，如关机、音量控制
// ========================================================================
#pragma once

// 逐渐改变系统主音量。
void GradualVolumeControl(int startPercent, int endPercent, int durationMs);

// 尝试通过发送 WM_CLOSE 关闭常见 Web 浏览器的窗口。
void CloseBrowserWindows();

// 使用适当的权限启动系统关机。
bool SystemShutdown();

