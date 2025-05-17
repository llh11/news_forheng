// ========================================================================
// network.h - �����������������¼�顢���ء�����
// ========================================================================
#pragma once

#include <string>
#include <Windows.h> // For HWND

// ͨ�� ping �ض� URL �����·������Ƿ�ɴ
bool CheckServerConnection(const std::wstring& pingUrl);

// �����·������Ƿ��и��°汾������ҵ���������汾�� URL��
bool CheckUpdate(std::wstring& outNewVersion, std::wstring& outDownloadUrl);

// ���ļ��� URL ���ص�����·������ѡ����½��ȶԻ���
bool DownloadFile(const std::wstring& downloadUrl, const std::wstring& savePath, HWND hProgressWnd = NULL);

// ����������ʹ����豸 ID �Ͱ汾������ POST ����
bool SendHeartbeat();

// �ӷ�������ȡ�������ͣ����� "configurator"���������ļ�����
bool GetLatestFilenameFromServer(const std::wstring& type, std::wstring& outFilename);

