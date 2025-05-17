// ========================================================================
// ui.h - �����û�����Ԫ�أ�������ͼ��ͽ��ȶԻ���
// ========================================================================
#pragma once

#include <Windows.h>
#include <string> // Added for std::wstring used in PostProgressUpdate

// ��Ӧ�ó���ͼ����ӵ�ϵͳ����֪ͨ����
void AddTrayIcon(HWND hWnd);

// ��ϵͳ�������Ƴ�Ӧ�ó���ͼ�ꡣ
void RemoveTrayIcon();

// ��������ʾ����ͼ����Ҽ����������Ĳ˵���
void ShowTrayMenu(HWND hWnd);

// ��ģʽ���ȶԻ���Ĵ��ڹ��̡�
INT_PTR CALLBACK ProgressDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

// --- Helper Function Declaration ---
// Safely posts a progress update message to the progress dialog.
// Allocates memory for the status string (caller is lParam in ProgressDlgProc).
// Declaration moved here from network.cpp/ui.cpp definition.
// ��ȫ������ȶԻ����ͽ��ȸ�����Ϣ��
// Ϊ״̬�ַ��������ڴ棨�������� ProgressDlgProc �е� lParam����
// ������ network.cpp/ui.cpp ���������˴���
void PostProgressUpdate(HWND hProgressWnd, int percent, const std::wstring& status);

