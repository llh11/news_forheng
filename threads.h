// ========================================================================
// threads.h - 声明后台线程的主函数
// ========================================================================
#pragma once

// SchedulerThread: 处理基于时间的事件，如打开 URL 和触发关机。
void SchedulerThread();

// UpdateCheckThread: 定期检查应用程序更新并处理用户交互。
void UpdateCheckThread();

// HeartbeatThread: 定期向服务器发送心跳。
void HeartbeatThread();
