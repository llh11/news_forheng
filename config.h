#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <map>
#include <mutex>

// 简单的 INI 文件解析器示例
// 您可以根据需要替换为更强大的库，如 simpleini, inih 等

class Config {
public:
    Config();
    ~Config();

    // 禁止拷贝和赋值
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;

    /**
     * @brief 从指定文件加载配置。
     * @param filePath 配置文件的路径。
     * @return 如果加载成功则返回 true，否则返回 false。
     */
    bool Load(const std::wstring& filePath);

    /**
     * @brief 将当前配置保存到指定文件。
     * @param filePath 配置文件的路径。
     * @return 如果保存成功则返回 true，否则返回 false。
     */
    bool Save(const std::wstring& filePath = L""); // 如果 filePath 为空，则保存到上次加载的路径

    // 获取配置项
    std::wstring GetString(const std::wstring& section, const std::wstring& key, const std::wstring& defaultValue = L"") const;
    int GetInt(const std::wstring& section, const std::wstring& key, int defaultValue = 0) const;
    bool GetBool(const std::wstring& section, const std::wstring& key, bool defaultValue = false) const;

    // 设置配置项
    void SetString(const std::wstring& section, const std::wstring& key, const std::wstring& value);
    void SetInt(const std::wstring& section, const std::wstring& key, int value);
    void SetBool(const std::wstring& section, const std::wstring& key, bool value);

private:
    // 修剪字符串两端的空白字符
    std::wstring Trim(const std::wstring& str) const;

    // 存储配置数据：map<section_name, map<key, value>>
    std::map<std::wstring, std::map<std::wstring, std::wstring>> m_data;
    std::wstring m_filePath; // 当前配置文件的路径
    mutable std::mutex m_mutex; // 用于保护 m_data 的线程安全访问
};

#endif // CONFIG_H
