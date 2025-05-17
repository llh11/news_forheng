#ifndef UI_H
#define UI_H

#include <string>
#include <windows.h> // For HWND, window creation, messages etc.
#include <functional> // For std::function

namespace UI {

    // �����ڹ��̺�������
    LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    /**
     * @brief ��������ʾ��Ӧ�ó��򴰿ڡ�
     * @param hInstance Ӧ�ó���ʵ�������
     * @param nCmdShow ������ʾ���
     * @param windowTitle ���ڱ��⡣
     * @param width ���ڿ�ȡ�
     * @param height ���ڸ߶ȡ�
     * @return �ɹ��򷵻������ھ����ʧ���򷵻� NULL��
     *
     * @note �˺�����ע�ᴰ���ࡢ�������ڣ��������ھ���洢��ȫ�ֱ��� g_hMainWnd (���� globals.h)��
     */
    HWND CreateMainWindow(
        HINSTANCE hInstance,
        int nCmdShow,
        const std::wstring& windowTitle,
        int width,
        int height
    );

    /**
     * @brief ��������Ϣѭ����
     * @return ��Ϣѭ�����˳����� (ͨ���� WM_QUIT �� wParam)��
     */
    int RunMessageLoop();

    // --- ʾ�� UI Ԫ�غͽ��� ---
    // ����Ҫ��������Ӧ�ó��������������ӿؼ��ʹ����߼���
    // ���磬��ť���ı����б���ͼ�ȡ�

    // �ؼ� ID ���� (ʾ��)
    constexpr int IDC_BUTTON_UPDATE = 1001;
    constexpr int IDC_STATUS_TEXT = 1002;
    // ... �����ؼ� ID

    /**
     * @brief ���������ϴ����ؼ� (ʾ��)��
     * @param hWndParent �����ھ�� (ͨ����������)��
     * @param hInstance Ӧ�ó���ʵ�������
     */
    void CreateControls(HWND hWndParent, HINSTANCE hInstance);

    /**
     * @brief ����״̬���ı� (ʾ��)��
     * @param newStatus �µ�״̬�ı���
     */
    void UpdateStatusText(const std::wstring& newStatus);


    // --- �ص��������Ͷ��� (ʾ��) ---
    // ��Щ�ص����Դ� UI �¼��д����������ӵ�Ӧ�ó����߼���

    // ���������¡���ť�����ʱ���õĻص�
    extern std::function<void()> onCheckForUpdatesClicked;
    // ���û������˳�����ʱ (�������رհ�ť����Alt+F4)
    extern std::function<bool()> onExitRequested; // ���� true ��ʾ�����˳�, false ��ֹ


} // namespace UI

#endif // UI_H
