// ========================================================================
// update.h - ����Ӧ�ó�����¹���
// ========================================================================
#pragma once

#include <string>

// ʹ����ɾ��������ű�ִ��Ӧ�ó�����¡�
void PerformUpdate(const std::wstring& downloadedFilePath);

// ��� 'update_pending.inf' �Ƿ���ڣ���ʾ�û���������Ӧ�ø��¡�
bool CheckAndApplyPendingUpdate();

// �����صĸ����ļ���·�����浽 'update_pending.inf'��
void SavePendingUpdateInfo(const std::wstring& filePath);

// ɾ�� 'update_pending.inf' �ļ���
void ClearPendingUpdateInfo();

// �Ӻ��԰汾�ļ��ж�ȡ�汾�ַ�����
std::wstring ReadIgnoredVersion();

// ���汾�ַ���д����԰汾�ļ���
void WriteIgnoredVersion(const std::wstring& version);

// ɾ�����԰汾�ļ���
void ClearIgnoredVersion();

