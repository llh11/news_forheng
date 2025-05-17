#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <map>
#include <mutex>

// �򵥵� INI �ļ�������ʾ��
// �����Ը�����Ҫ�滻Ϊ��ǿ��Ŀ⣬�� simpleini, inih ��

class Config {
public:
    Config();
    ~Config();

    // ��ֹ�����͸�ֵ
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;

    /**
     * @brief ��ָ���ļ��������á�
     * @param filePath �����ļ���·����
     * @return ������سɹ��򷵻� true�����򷵻� false��
     */
    bool Load(const std::wstring& filePath);

    /**
     * @brief ����ǰ���ñ��浽ָ���ļ���
     * @param filePath �����ļ���·����
     * @return �������ɹ��򷵻� true�����򷵻� false��
     */
    bool Save(const std::wstring& filePath = L""); // ��� filePath Ϊ�գ��򱣴浽�ϴμ��ص�·��

    // ��ȡ������
    std::wstring GetString(const std::wstring& section, const std::wstring& key, const std::wstring& defaultValue = L"") const;
    int GetInt(const std::wstring& section, const std::wstring& key, int defaultValue = 0) const;
    bool GetBool(const std::wstring& section, const std::wstring& key, bool defaultValue = false) const;

    // ����������
    void SetString(const std::wstring& section, const std::wstring& key, const std::wstring& value);
    void SetInt(const std::wstring& section, const std::wstring& key, int value);
    void SetBool(const std::wstring& section, const std::wstring& key, bool value);

private:
    // �޼��ַ������˵Ŀհ��ַ�
    std::wstring Trim(const std::wstring& str) const;

    // �洢�������ݣ�map<section_name, map<key, value>>
    std::map<std::wstring, std::map<std::wstring, std::wstring>> m_data;
    std::wstring m_filePath; // ��ǰ�����ļ���·��
    mutable std::mutex m_mutex; // ���ڱ��� m_data ���̰߳�ȫ����
};

#endif // CONFIG_H
