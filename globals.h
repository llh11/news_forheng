#ifndef GLOBALS_H
#define GLOBALS_H

#include <string>
#include <windows.h> // ���� Windows �ض���ͷ�ļ������� HWND
#include <atomic>    // �����̰߳�ȫ��ԭ�Ӳ���

// Ӧ�ó��������Ϣ
extern std::wstring g_appName;          // Ӧ�ó������� (ʹ�� wstring ��֧�� Unicode)
extern std::wstring g_appVersion;       // Ӧ�ó���汾
extern std::wstring g_configFilePath;   // �����ļ�·��
extern std::wstring g_logFilePath;      // ��־�ļ�·��
extern std::wstring g_appDataDir;       // Ӧ�ó�������Ŀ¼

// ����״̬�����
extern std::atomic<bool> g_isRunning;   // ������ѭ���Ƿ�������У�ԭ�����ͱ�֤�̰߳�ȫ
extern bool g_debugMode;                // ����ģʽ����

// ���ھ�� (�������Ӧ���� GUI)
extern HWND g_hMainWnd;                 // �����ھ��

// �������ܵ�ȫ������
// extern int g_someGlobalSetting;

/**
 * @brief ��ʼ��ȫ�ֱ���
 * @note �������Ӧ���ڳ������������ڱ����á�
 */
void InitializeGlobals();

/**
 * @brief ����ȫ����Դ�������Ҫ��
 * @note ������������ڳ����˳�ǰ���á�
 */
void CleanupGlobals();

#endif // GLOBALS_H
