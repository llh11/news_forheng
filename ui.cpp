#include "ui.h"
#include "globals.h" 
#include "log.h"
// #include "resource.h" // C1083: If you don't have a resource.h or don't use resources, comment this out or remove it.
                       // For a typical VS project, if you add resources (icons, menus via designer), it's auto-generated.
                       // If not, it won't exist. For now, I'll comment it out.
                       // If you have one, make sure it's in the project's include paths.

// 全局/静态变量，用于存储控件句柄 (示例)
static HWND g_hStatusBar = NULL;
static HWND g_hUpdateButton = NULL;


// 回调函数定义
std::function<void()> UI::onCheckForUpdatesClicked = nullptr;
std::function<bool()> UI::onExitRequested = nullptr;


LRESULT CALLBACK UI::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        // CreateControls should be called here as hWnd is valid.
        CreateControls(hWnd, ((LPCREATESTRUCT)lParam)->hInstance);
        LOG_INFO(L"Main window created (WM_CREATE). Controls initialized.");
        break;

    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        switch (wmId) {
        case IDC_BUTTON_UPDATE:
            if (HIWORD(wParam) == BN_CLICKED) {
                LOG_INFO(L"Update button clicked.");
                if (onCheckForUpdatesClicked) {
                    onCheckForUpdatesClicked();
                }
                else {
                    MessageBoxW(hWnd, L"Update function not implemented.", L"Info", MB_OK | MB_ICONINFORMATION);
                }
            }
            break;
            // Example Menu items (if you add a menu resource and uncomment resource.h)
            // #define IDM_ABOUT 101 // Define these if not in resource.h
            // #define IDM_EXIT  102
            // case IDM_ABOUT: 
            //     MessageBoxW(hWnd, L"NewsForHeng App v1.0", L"About", MB_OK | MB_ICONINFORMATION);
            //     break;
            // case IDM_EXIT: 
            //     DestroyWindow(hWnd);
            //     break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
                   break;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        // Example: Draw some text if you want.
        // LPCWSTR text = L"Welcome to NewsForHeng!";
        // TextOutW(hdc, 10, 50, text, wcslen(text));
        EndPaint(hWnd, &ps);
    }
                 break;

    case WM_SIZE: {
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);
        if (g_hStatusBar) {
            // Move status bar to bottom, full width
            MoveWindow(g_hStatusBar, 0, height - 20, width, 20, TRUE);
        }
        if (g_hUpdateButton) {
            // Example: Keep button at top-left
            // MoveWindow(g_hUpdateButton, 10, 10, 120, 30, TRUE); 
        }
        break;
    }

    case WM_CLOSE:
        LOG_INFO(L"WM_CLOSE received. Requesting exit confirmation.");
        if (onExitRequested) {
            if (onExitRequested()) {
                LOG_INFO(L"Exit approved by callback. Destroying window.");
                DestroyWindow(hWnd);
            }
            else {
                LOG_INFO(L"Exit denied by callback.");
            }
        }
        else {
            if (MessageBoxW(hWnd, L"Are you sure you want to exit?", g_appName.c_str(), MB_YESNO | MB_ICONQUESTION) == IDYES) {
                LOG_INFO(L"User confirmed exit. Destroying window.");
                DestroyWindow(hWnd);
            }
            else {
                LOG_INFO(L"User cancelled exit.");
            }
        }
        return 0;

    case WM_DESTROY:
        LOG_INFO(L"Main window destroyed (WM_DESTROY). Posting quit message.");
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

HWND UI::CreateMainWindow(
    HINSTANCE hInstance,
    int nCmdShow,
    const std::wstring& windowTitle,
    int width,
    int height)
{
    const wchar_t CLASS_NAME[] = L"NewsForHengWindowClass";

    WNDCLASSW wc = {};
    wc.lpfnWndProc = UI::WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    // wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_YOUR_ICON)); // If you have an icon in resource.rc
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION); // Default application icon
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    // wc.lpszMenuName = MAKEINTRESOURCE(IDC_YOUR_MENU); // If you have a menu in resource.rc

    if (!RegisterClassW(&wc)) {
        LOG_FATAL(L"Failed to register window class. Error: ", GetLastError());
        MessageBoxW(NULL, L"Window Registration Failed!", L"Error", MB_ICONEXCLAMATION | MB_OK);
        return NULL;
    }

    g_hMainWnd = CreateWindowExW(
        0,
        CLASS_NAME,
        windowTitle.c_str(),
        WS_OVERLAPPEDWINDOW,

        CW_USEDEFAULT, CW_USEDEFAULT, width, height,

        NULL,
        NULL,       // No menu by default, can be added via WNDCLASS or SetMenu
        hInstance,
        NULL
    );

    if (g_hMainWnd == NULL) {
        LOG_FATAL(L"Failed to create main window. Error: ", GetLastError());
        MessageBoxW(NULL, L"Window Creation Failed!", L"Error", MB_ICONEXCLAMATION | MB_OK);
        return NULL;
    }

    // Controls are now created in WM_CREATE of WndProc

    ShowWindow(g_hMainWnd, nCmdShow);
    UpdateWindow(g_hMainWnd);

    LOG_INFO(L"Main window created and shown successfully.");
    return g_hMainWnd;
}

int UI::RunMessageLoop() {
    MSG msg = {};
    BOOL bRet;
    while ((bRet = GetMessage(&msg, NULL, 0, 0)) != 0) {
        if (bRet == -1) {
            LOG_ERROR(L"Error in GetMessage. Error code: ", GetLastError());
            return -1;
        }
        else {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    LOG_INFO(L"Message loop terminated. Exit code: ", (int)msg.wParam);
    return (int)msg.wParam;
}

void UI::CreateControls(HWND hWndParent, HINSTANCE hInstance) {
    int buttonWidth = 120;
    int buttonHeight = 30;
    int margin = 10;

    g_hUpdateButton = CreateWindowW(
        L"BUTTON",
        L"Check for Updates",
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, // Changed BS_DEFPUSHBUTTON to BS_PUSHBUTTON
        margin,
        margin,
        buttonWidth,
        buttonHeight,
        hWndParent,
        (HMENU)IDC_BUTTON_UPDATE,
        hInstance,
        NULL);

    if (!g_hUpdateButton) {
        LOG_ERROR(L"Failed to create update button. Error: ", GetLastError());
    }

    RECT rcParent;
    GetClientRect(hWndParent, &rcParent);
    int statusHeight = 20;

    g_hStatusBar = CreateWindowW(
        L"STATIC",
        L"Ready",
        WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP,
        0, rcParent.bottom - statusHeight,
        rcParent.right, statusHeight,
        hWndParent,
        (HMENU)IDC_STATUS_TEXT,
        hInstance,
        NULL
    );
    if (!g_hStatusBar) {
        LOG_ERROR(L"Failed to create status text. Error: ", GetLastError());
    }
}

void UI::UpdateStatusText(const std::wstring& newStatus) {
    if (g_hStatusBar) {
        SetWindowTextW(g_hStatusBar, newStatus.c_str());
    }
    else {
        // This might be called before g_hStatusBar is created if PerformBackgroundUpdateCheck is called too early
        LOG_WARNING(L"Attempted to update status text, but status bar handle is null. Status: ", newStatus.c_str());
    }
}

 // namespace UI
