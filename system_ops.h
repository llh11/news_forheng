#ifndef SYSTEM_OPS_H
#define SYSTEM_OPS_H

#include <string>
#include <windows.h> // For various system functions

namespace SystemOps {

    /**
     * @brief �Թ���ԱȨ������������ǰӦ�ó���
     * @return ����ɹ��������������򷵻� true�����򷵻� false��
     * @note �û��ῴ�� UAC ��ʾ��
     */
    bool RestartAsAdmin();

    /**
     * @brief ��鵱ǰ�����Ƿ��Թ���ԱȨ�����С�
     * @return ����ǹ���ԱȨ���򷵻� true�����򷵻� false��
     */
    bool IsRunningAsAdmin();

    /**
     * @brief ִ��һ���ⲿ����
     * @param command Ҫִ�еĳ���·����
     * @param params ���ݸ�����Ĳ�����
     * @param workingDirectory ����Ĺ���Ŀ¼�����Ϊ����ʹ�õ�ǰĿ¼��
     * @param waitForCompletion �Ƿ�ȴ�����ִ����ɡ�
     * @param processInfo [out] ��� waitForCompletion Ϊ true���������������Ϣ��
     * @param hidden �Ƿ����ش���
     * @return ����ɹ����������򷵻� true��
     */
    bool ExecuteProcess(
        const std::wstring& command,
        const std::wstring& params = L"",
        const std::wstring& workingDirectory = L"",
        bool waitForCompletion = false,
        PROCESS_INFORMATION* processInfo = nullptr,
        bool hidden = false
    );

    /**
     * @brief ��ȡϵͳ�������Ӧ��������Ϣ��
     * @param errorCode ϵͳ������ (���� GetLastError() �ķ���ֵ)��
     * @return ���������ַ�����
     */
    std::wstring GetErrorMessage(DWORD errorCode);

    /**
     * @brief ��ȡ��ǰ����ϵͳ�İ汾��Ϣ (��Ҫ)��
     * @return ����ϵͳ�汾�ַ�����
     */
    std::wstring GetOSVersion();


    /**
     * @brief ����ǰ������ӵ�ϵͳ�����
     * @param appName Ӧ�ó������������е����ơ�
     * @param appPath Ӧ�ó��������·����
     * @param forCurrentUser true Ϊ��ǰ�û���ӣ�false Ϊ�����û���� (��Ҫ����ԱȨ��)��
     * @return ��������ɹ����� true��
     */
    bool AddToStartup(const std::wstring& appName, const std::wstring& appPath, bool forCurrentUser = true);

    /**
     * @brief ��ϵͳ���������Ƴ���ǰ����
     * @param appName Ӧ�ó������������е����ơ�
     * @param forCurrentUser true Ϊ��ǰ�û��Ƴ���false Ϊ�����û��Ƴ� (��Ҫ����ԱȨ��)��
     * @return ��������ɹ����� true��
     */
    bool RemoveFromStartup(const std::wstring& appName, bool forCurrentUser = true);

    /**
     * @brief �������Ƿ������������С�
     * @param appName Ӧ�ó������������е����ơ�
     * @param forCurrentUser true ��鵱ǰ�û���false ��������û���
     * @return ������������з��� true��
     */
    bool IsInStartup(const std::wstring& appName, bool forCurrentUser = true);

} // namespace SystemOps

#endif // SYSTEM_OPS_H
