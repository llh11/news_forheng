#ifndef UPDATE_H
#define UPDATE_H

#include <string>
#include <functional> // For std::function

namespace Update {

    struct VersionInfo {
        std::wstring versionString; // ���� "1.2.3"
        std::wstring downloadUrl;   // ���°������ص�ַ
        std::wstring releaseNotes;  // ������־������
        // ������������ֶΣ��緢�����ڡ��ļ���С��У��͵�
        // int major, minor, patch, build; // Parsed version numbers
    };

    /**
     * @brief ����Ƿ����°汾��
     * @param currentVersion ��ǰӦ�ó���İ汾�ַ��� (���� "1.0.0")��
     * @param updateCheckUrl ���ڻ�ȡ���°汾��Ϣ�� URL (����һ�� JSON �ļ�)��
     * @param outVersionInfo [out] ������°汾�����������°汾����Ϣ��
     * @return ������°汾�򷵻� true�����򷵻� false��
     * ������ʧ�ܣ�������󡢽�������ȣ���Ҳ���� false��
     */
    bool CheckForUpdates(
        const std::wstring& currentVersion,
        const std::string& updateCheckUrl, // URL ͨ���� ASCII/UTF-8
        VersionInfo& outVersionInfo
    );

    /**
     * @brief ���ز�Ӧ�ø��¡�
     * @param versionToUpdate Ҫ���µ��İ汾��Ϣ (ͨ���� CheckForUpdates ��ȡ)��
     * @param progressCallback ���Ȼص� (�������ֽ�, ���ֽ�)��
     * @param restartAppCallback ������ɺ���������Ӧ�ó���������Ӧ�ø��µĻص���
     * �˻ص�Ӧ����رյ�ǰʵ�������������صĸ��³���/��װ����
     * @return ������غ�׼�����³ɹ��򷵻� true��ʵ��Ӧ�ø���ͨ�����������ɸ��³�����ɡ�
     *
     * @note ����һ���߶ȼ򻯵�ģ�͡�ʵ�ʵĸ��¹��̷ǳ����ӣ��漰��
     * 1. ���ظ��°� (�����ǰ�װ�����ѹ���ļ�)��
     * 2. ��֤���°��������Ժ�ǩ�� (��ȫ��)��
     * 3. ��ѹ/׼�����¡�
     * 4. (�ؼ�) ��ǰ���еĳ���ͨ���޷�ֱ���滻�����ļ���
     * ͨ���������ǣ�
     * a. ����һ��С�͵ĸ����������� (updater.exe)��
     * b. �������˳��������� updater.exe��
     * c. updater.exe �滻�������ļ���
     * d. updater.exe ����������������°汾��
     * e. updater.exe �����˳� (������ɾ��)��
     * ���ߣ����°�������һ����װ�������������ز����иð�װ����
     */
    bool DownloadAndApplyUpdate(
        const VersionInfo& versionToUpdate,
        std::function<void(long long, long long)> progressCallback,
        std::function<bool()> restartAppCallback // Returns true if restart was initiated
    );


    /**
     * @brief �Ƚ������汾�ַ�����
     * @param version1 �汾�ַ���1 (���� "1.2.3")��
     * @param version2 �汾�ַ���2 (���� "1.2.10")��
     * @return  0 ��� version1 == version2
     * >0 ��� version1 > version2
     * <0 ��� version1 < version2
     * Throws std::invalid_argument on parse error.
     */
    int CompareVersions(const std::wstring& version1, const std::wstring& version2);

} // namespace Update

#endif // UPDATE_H
