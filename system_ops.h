#ifndef SYSTEM_OPS_H
#define SYSTEM_OPS_H

#include <string>
#include <windows.h> // For various system functions

namespace SystemOps {

    /**
     * @brief 以管理员权限重新启动当前应用程序。
     * @return 如果成功发起重启请求则返回 true，否则返回 false。
     * @note 用户会看到 UAC 提示。
     */
    bool RestartAsAdmin();

    /**
     * @brief 检查当前进程是否以管理员权限运行。
     * @return 如果是管理员权限则返回 true，否则返回 false。
     */
    bool IsRunningAsAdmin();

    /**
     * @brief 执行一个外部程序。
     * @param command 要执行的程序路径。
     * @param params 传递给程序的参数。
     * @param workingDirectory 程序的工作目录，如果为空则使用当前目录。
     * @param waitForCompletion 是否等待程序执行完成。
     * @param processInfo [out] 如果 waitForCompletion 为 true，这里会填充进程信息。
     * @param hidden 是否隐藏窗口
     * @return 如果成功启动程序则返回 true。
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
     * @brief 获取系统错误码对应的描述信息。
     * @param errorCode 系统错误码 (例如 GetLastError() 的返回值)。
     * @return 错误描述字符串。
     */
    std::wstring GetErrorMessage(DWORD errorCode);

    /**
     * @brief 获取当前操作系统的版本信息 (简要)。
     * @return 操作系统版本字符串。
     */
    std::wstring GetOSVersion();


    /**
     * @brief 将当前程序添加到系统启动项。
     * @param appName 应用程序在启动项中的名称。
     * @param appPath 应用程序的完整路径。
     * @param forCurrentUser true 为当前用户添加，false 为所有用户添加 (需要管理员权限)。
     * @return 如果操作成功返回 true。
     */
    bool AddToStartup(const std::wstring& appName, const std::wstring& appPath, bool forCurrentUser = true);

    /**
     * @brief 从系统启动项中移除当前程序。
     * @param appName 应用程序在启动项中的名称。
     * @param forCurrentUser true 为当前用户移除，false 为所有用户移除 (需要管理员权限)。
     * @return 如果操作成功返回 true。
     */
    bool RemoveFromStartup(const std::wstring& appName, bool forCurrentUser = true);

    /**
     * @brief 检查程序是否已在启动项中。
     * @param appName 应用程序在启动项中的名称。
     * @param forCurrentUser true 检查当前用户，false 检查所有用户。
     * @return 如果在启动项中返回 true。
     */
    bool IsInStartup(const std::wstring& appName, bool forCurrentUser = true);

} // namespace SystemOps

#endif // SYSTEM_OPS_H
