// ========================================================================
// log.h - ������־��¼���ļ����ڴ滺��
// ========================================================================
#pragma once

#include <string>
#include <vector>
#include <Windows.h> // For HWND

// ʹ��ʱ����� UTF-8 ��Ϣ��¼���ļ����ڴ滺���С�
void Log(const std::string& message_utf8);

// ����־�ļ���UTF-8����ȡ��� N �в�������Ϊ wstring ���ء�
std::vector<std::wstring> ReadLastLogLinesW(int maxLines);

// ���ڴ滺�������־��Ŀ���ڱ༭�ؼ�����ʾ���ǡ�
void LoadAndDisplayLogs(HWND hEdit);

