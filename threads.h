// ========================================================================
// threads.h - ������̨�̵߳�������
// ========================================================================
#pragma once

// SchedulerThread: �������ʱ����¼������ URL �ʹ����ػ���
void SchedulerThread();

// UpdateCheckThread: ���ڼ��Ӧ�ó�����²������û�������
void UpdateCheckThread();

// HeartbeatThread: ���������������������
void HeartbeatThread();
