// ========================================================================
// system_ops.h - ����ϵͳ����������ػ�����������
// ========================================================================
#pragma once

// �𽥸ı�ϵͳ��������
void GradualVolumeControl(int startPercent, int endPercent, int durationMs);

// ����ͨ������ WM_CLOSE �رճ��� Web ������Ĵ��ڡ�
void CloseBrowserWindows();

// ʹ���ʵ���Ȩ������ϵͳ�ػ���
bool SystemShutdown();

