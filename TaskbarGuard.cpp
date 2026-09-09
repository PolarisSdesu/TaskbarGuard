#include <windows.h>
#include <shellapi.h>

constexpr wchar_t kClassName[] = L"TaskbarGuardWindow";
constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kRunValue[] = L"TaskbarGuard";
constexpr UINT kHotkeyId = 0x5447;
constexpr UINT kTrayMessage = WM_APP + 1;
constexpr UINT_PTR kGuardTimer = 1;
constexpr UINT kMenuToggle = 1001;
constexpr UINT kMenuStartup = 1002;
constexpr UINT kMenuExit = 1003;

HWND g_window = nullptr;
bool g_locked = false;
NOTIFYICONDATAW g_tray{};

// Win32 still has no public API for dark popup menus. These documented-by-use
// uxtheme entry points are what Explorer-compatible desktop apps use on
// Windows 10 1903+ and Windows 11. They are loaded dynamically so the app also
// remains compatible with older Windows versions.
enum class PreferredAppMode { Default, AllowDark, ForceDark, ForceLight, Max };
using SetPreferredAppModeFn = PreferredAppMode(WINAPI*)(PreferredAppMode);
using FlushMenuThemesFn = void(WINAPI*)();
FlushMenuThemesFn g_flushMenuThemes = nullptr;

void EnableSystemDarkMode()
{
    if (HMODULE theme = LoadLibraryW(L"uxtheme.dll")) {
        auto setPreferredAppMode = reinterpret_cast<SetPreferredAppModeFn>(
            GetProcAddress(theme, MAKEINTRESOURCEA(135)));
        g_flushMenuThemes = reinterpret_cast<FlushMenuThemesFn>(
            GetProcAddress(theme, MAKEINTRESOURCEA(136)));
        if (setPreferredAppMode)
            setPreferredAppMode(PreferredAppMode::AllowDark);
        if (g_flushMenuThemes)
            g_flushMenuThemes();
    }
}

BOOL CALLBACK HandleSecondaryTaskbar(HWND hwnd, LPARAM show)
{
    wchar_t name[64]{};
    GetClassNameW(hwnd, name, 64);
    if (lstrcmpW(name, L"Shell_SecondaryTrayWnd") == 0)
        ShowWindow(hwnd, show ? SW_SHOW : SW_HIDE);
    return TRUE;
}

void SetTaskbarsVisible(bool visible)
{
    if (HWND primary = FindWindowW(L"Shell_TrayWnd", nullptr))
        ShowWindow(primary, visible ? SW_SHOW : SW_HIDE);
    EnumWindows(HandleSecondaryTaskbar, visible ? 1 : 0);
}

void UpdateTrayText()
{
    lstrcpynW(g_tray.szTip,
        g_locked ? L"Taskbar Guard：强制隐藏中" : L"Taskbar Guard：普通自动隐藏",
        ARRAYSIZE(g_tray.szTip));
    Shell_NotifyIconW(NIM_MODIFY, &g_tray);
}

void ToggleMode()
{
    g_locked = !g_locked;
    if (g_locked) {
        SetTaskbarsVisible(false);
        SetTimer(g_window, kGuardTimer, 100, nullptr);
    } else {
        KillTimer(g_window, kGuardTimer);
        SetTaskbarsVisible(true);
    }
    UpdateTrayText();
}

bool StartupEnabled()
{
    HKEY key{};
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
        return false;
    const LONG result = RegQueryValueExW(key, kRunValue, nullptr, nullptr, nullptr, nullptr);
    RegCloseKey(key);
    return result == ERROR_SUCCESS;
}

void ToggleStartup()
{
    HKEY key{};
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRunKey, 0, nullptr, 0,
        KEY_SET_VALUE | KEY_QUERY_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS) {
        MessageBoxW(g_window, L"无法修改开机启动设置。", L"Taskbar Guard", MB_ICONERROR);
        return;
    }

    if (StartupEnabled()) {
        RegDeleteValueW(key, kRunValue);
    } else {
        wchar_t path[MAX_PATH]{};
        GetModuleFileNameW(nullptr, path, MAX_PATH);
        wchar_t quoted[MAX_PATH + 3]{};
        wsprintfW(quoted, L"\"%s\"", path);
        RegSetValueExW(key, kRunValue, 0, REG_SZ,
            reinterpret_cast<const BYTE*>(quoted),
            static_cast<DWORD>((lstrlenW(quoted) + 1) * sizeof(wchar_t)));
    }
    RegCloseKey(key);
}

void ShowTrayMenu()
{
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING | (g_locked ? MF_CHECKED : 0), kMenuToggle,
        g_locked ? L"强制隐藏中（点击恢复）" : L"强制隐藏任务栏");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING | MF_DISABLED, 0, L"快捷键：Ctrl + Alt + T");
    AppendMenuW(menu, MF_STRING | (StartupEnabled() ? MF_CHECKED : 0), kMenuStartup, L"开机自动启动");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuExit, L"退出并恢复任务栏");

    POINT point{};
    GetCursorPos(&point);
    SetForegroundWindow(g_window);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, point.x, point.y, 0, g_window, nullptr);
    DestroyMenu(menu);
}

void CleanExit()
{
    KillTimer(g_window, kGuardTimer);
    SetTaskbarsVisible(true);
    UnregisterHotKey(g_window, kHotkeyId);
    Shell_NotifyIconW(NIM_DELETE, &g_tray);
    DestroyWindow(g_window);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_HOTKEY:
        if (wParam == kHotkeyId) ToggleMode();
        return 0;
    case WM_TIMER:
        if (wParam == kGuardTimer && g_locked) SetTaskbarsVisible(false);
        return 0;
    case WM_COMMAND:
        if (LOWORD(wParam) == kMenuToggle) ToggleMode();
        else if (LOWORD(wParam) == kMenuStartup) ToggleStartup();
        else if (LOWORD(wParam) == kMenuExit) CleanExit();
        return 0;
    case kTrayMessage:
        if (LOWORD(lParam) == WM_LBUTTONDBLCLK) ToggleMode();
        else if (LOWORD(lParam) == WM_RBUTTONUP || LOWORD(lParam) == WM_CONTEXTMENU) ShowTrayMenu();
        return 0;
    case WM_SETTINGCHANGE:
        // Refresh popup-menu colors immediately after Windows switches theme.
        if (lParam && lstrcmpW(reinterpret_cast<LPCWSTR>(lParam), L"ImmersiveColorSet") == 0
            && g_flushMenuThemes)
            g_flushMenuThemes();
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    HANDLE mutex = CreateMutexW(nullptr, TRUE, L"Local\\TaskbarGuard.SingleInstance.53D3F96B");
    if (!mutex || GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(nullptr, L"Taskbar Guard 已经在运行。\n\n按 Ctrl + Alt + T 切换任务栏模式。",
            L"Taskbar Guard", MB_ICONINFORMATION);
        if (mutex) CloseHandle(mutex);
        return 0;
    }

    EnableSystemDarkMode();

    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = kClassName;
    RegisterClassW(&windowClass);
    // Keep an invisible top-level tool window so Windows broadcasts theme
    // changes to us. It never appears in the taskbar or Alt+Tab.
    g_window = CreateWindowExW(WS_EX_TOOLWINDOW, kClassName, L"Taskbar Guard", WS_POPUP,
        0, 0, 0, 0, nullptr, nullptr, instance, nullptr);

    g_tray.cbSize = sizeof(g_tray);
    g_tray.hWnd = g_window;
    g_tray.uID = 1;
    g_tray.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    g_tray.uCallbackMessage = kTrayMessage;
    g_tray.hIcon = LoadIconW(nullptr, IDI_SHIELD);
    lstrcpyW(g_tray.szTip, L"Taskbar Guard：普通自动隐藏");
    Shell_NotifyIconW(NIM_ADD, &g_tray);
    g_tray.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &g_tray);

    if (!RegisterHotKey(g_window, kHotkeyId, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'T'))
        MessageBoxW(nullptr, L"无法注册 Ctrl + Alt + T，可能已被占用。\n仍可双击托盘图标切换。",
            L"Taskbar Guard", MB_ICONWARNING);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    SetTaskbarsVisible(true);
    ReleaseMutex(mutex);
    CloseHandle(mutex);
    return 0;
}
