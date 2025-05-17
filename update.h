#ifndef UPDATE_H
#define UPDATE_H

#include <string>
#include <functional> // For std::function

namespace Update {

    struct VersionInfo {
        std::wstring versionString; // 例如 "1.2.3"
        std::wstring downloadUrl;   // 更新包的下载地址
        std::wstring releaseNotes;  // 更新日志或描述
        // 可以添加其他字段，如发布日期、文件大小、校验和等
        // int major, minor, patch, build; // Parsed version numbers
    };

    /**
     * @brief 检查是否有新版本。
     * @param currentVersion 当前应用程序的版本字符串 (例如 "1.0.0")。
     * @param updateCheckUrl 用于获取最新版本信息的 URL (例如一个 JSON 文件)。
     * @param outVersionInfo [out] 如果有新版本，这里会填充新版本的信息。
     * @return 如果有新版本则返回 true，否则返回 false。
     * 如果检查失败（网络错误、解析错误等），也返回 false。
     */
    bool CheckForUpdates(
        const std::wstring& currentVersion,
        const std::string& updateCheckUrl, // URL 通常是 ASCII/UTF-8
        VersionInfo& outVersionInfo
    );

    /**
     * @brief 下载并应用更新。
     * @param versionToUpdate 要更新到的版本信息 (通常从 CheckForUpdates 获取)。
     * @param progressCallback 进度回调 (已下载字节, 总字节)。
     * @param restartAppCallback 更新完成后，用于请求应用程序重启以应用更新的回调。
     * 此回调应负责关闭当前实例并启动新下载的更新程序/安装包。
     * @return 如果下载和准备更新成功则返回 true。实际应用更新通常在重启后由更新程序完成。
     *
     * @note 这是一个高度简化的模型。实际的更新过程非常复杂，涉及：
     * 1. 下载更新包 (可能是安装程序或压缩文件)。
     * 2. 验证更新包的完整性和签名 (安全性)。
     * 3. 解压/准备更新。
     * 4. (关键) 当前运行的程序通常无法直接替换自身文件。
     * 通常的做法是：
     * a. 下载一个小型的更新引导程序 (updater.exe)。
     * b. 主程序退出，并运行 updater.exe。
     * c. updater.exe 替换主程序文件。
     * d. updater.exe 重新启动主程序的新版本。
     * e. updater.exe 自身退出 (可能自删除)。
     * 或者，更新包本身是一个安装程序，主程序下载并运行该安装程序。
     */
    bool DownloadAndApplyUpdate(
        const VersionInfo& versionToUpdate,
        std::function<void(long long, long long)> progressCallback,
        std::function<bool()> restartAppCallback // Returns true if restart was initiated
    );


    /**
     * @brief 比较两个版本字符串。
     * @param version1 版本字符串1 (例如 "1.2.3")。
     * @param version2 版本字符串2 (例如 "1.2.10")。
     * @return  0 如果 version1 == version2
     * >0 如果 version1 > version2
     * <0 如果 version1 < version2
     * Throws std::invalid_argument on parse error.
     */
    int CompareVersions(const std::wstring& version1, const std::wstring& version2);

} // namespace Update

#endif // UPDATE_H
