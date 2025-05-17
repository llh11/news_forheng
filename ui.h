#ifndef UI_H
#define UI_H

#include <string>
#include <windows.h> // For HWND, window creation, messages etc.
#include <functional> // For std::function

namespace UI {

    // 主窗口过程函数声明
    LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    /**
     * @brief 创建并显示主应用程序窗口。
     * @param hInstance 应用程序实例句柄。
     * @param nCmdShow 窗口显示命令。
     * @param windowTitle 窗口标题。
     * @param width 窗口宽度。
     * @param height 窗口高度。
     * @return 成功则返回主窗口句柄，失败则返回 NULL。
     *
     * @note 此函数会注册窗口类、创建窗口，并将窗口句柄存储在全局变量 g_hMainWnd (来自 globals.h)。
     */
    HWND CreateMainWindow(
        HINSTANCE hInstance,
        int nCmdShow,
        const std::wstring& windowTitle,
        int width,
        int height
    );

    /**
     * @brief 运行主消息循环。
     * @return 消息循环的退出代码 (通常是 WM_QUIT 的 wParam)。
     */
    int RunMessageLoop();

    // --- 示例 UI 元素和交互 ---
    // 您需要根据您的应用程序具体需求来添加控件和处理逻辑。
    // 例如，按钮、文本框、列表视图等。

    // 控件 ID 定义 (示例)
    constexpr int IDC_BUTTON_UPDATE = 1001;
    constexpr int IDC_STATUS_TEXT = 1002;
    // ... 其他控件 ID

    /**
     * @brief 在主窗口上创建控件 (示例)。
     * @param hWndParent 父窗口句柄 (通常是主窗口)。
     * @param hInstance 应用程序实例句柄。
     */
    void CreateControls(HWND hWndParent, HINSTANCE hInstance);

    /**
     * @brief 更新状态栏文本 (示例)。
     * @param newStatus 新的状态文本。
     */
    void UpdateStatusText(const std::wstring& newStatus);


    // --- 回调函数类型定义 (示例) ---
    // 这些回调可以从 UI 事件中触发，并连接到应用程序逻辑。

    // 当“检查更新”按钮被点击时调用的回调
    extern std::function<void()> onCheckForUpdatesClicked;
    // 当用户请求退出程序时 (例如点击关闭按钮，或Alt+F4)
    extern std::function<bool()> onExitRequested; // 返回 true 表示允许退出, false 阻止


} // namespace UI

#endif // UI_H
