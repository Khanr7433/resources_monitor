#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE

#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <stdio.h>
#include "common.h"
#include "config.h"
#include "metrics.h"
#include "gui.h"

static HWND g_hwnd;

typedef struct {
    HWND self;
    HWND taskbar;
    RECT trc;
    int tray_left;
    int leftmost_neighbor_x;
} NeighborScanData;

static BOOL CALLBACK scan_neighbor_proc(HWND wnd, LPARAM lp)
{
    NeighborScanData *data = (NeighborScanData *)lp;
    if (wnd == data->self || wnd == data->taskbar)
        return TRUE;
    if (!IsWindowVisible(wnd))
        return TRUE;

    RECT rc;
    if (!GetWindowRect(wnd, &rc))
        return TRUE;

    if (rc.right <= rc.left || rc.bottom <= rc.top)
        return TRUE;

    int mid_y = (rc.top + rc.bottom) / 2;
    if (mid_y < data->trc.top || mid_y > data->trc.bottom)
        return TRUE;

    if (rc.right > (data->leftmost_neighbor_x - 10) && rc.left < data->leftmost_neighbor_x && rc.right <= (data->tray_left + 10))
    {
        int w = rc.right - rc.left;
        if (w >= 10 && w <= 450)
        {
            if (rc.left < data->leftmost_neighbor_x)
            {
                data->leftmost_neighbor_x = rc.left;
            }
        }
    }
    return TRUE;
}

static BOOL is_fullscreen_app_active(HWND self)
{
    HWND fg = GetForegroundWindow();
    if (!fg || fg == GetDesktopWindow() || fg == self)
        return FALSE;

    HWND taskbar = FindWindowW(L"Shell_TrayWnd", NULL);
    if (fg == taskbar)
        return FALSE;

    RECT rc;
    if (GetWindowRect(fg, &rc))
    {
        int sw = GetSystemMetrics(SM_CXSCREEN);
        int sh = GetSystemMetrics(SM_CYSCREEN);
        if (rc.left <= 0 && rc.top <= 0 && rc.right >= sw && rc.bottom >= sh)
        {
            LONG style = GetWindowLongW(fg, GWL_STYLE);
            if ((style & WS_VISIBLE) && !(style & WS_CHILD))
                return TRUE;
        }
    }
    return FALSE;
}

static void update_theme_colors(void)
{
    DWORD light_theme = 0;
    DWORD sz = sizeof(light_theme);
    HKEY hkey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                      0, KEY_READ, &hkey) == ERROR_SUCCESS)
    {
        RegQueryValueExW(hkey, L"SystemUsesLightTheme", NULL, NULL, (LPBYTE)&light_theme, &sz);
        RegCloseKey(hkey);
    }

    if (light_theme)
    {
        g_clr_label = CLR_LIGHT_LABEL;
        g_clr_value = CLR_LIGHT_VALUE;
    }
    else
    {
        g_clr_label = CLR_DARK_LABEL;
        g_clr_value = CLR_DARK_VALUE;
    }
}

static void resize_and_position(HWND hwnd)
{
    update_theme_colors();
    update_dpi_scale(hwnd);
    HDC dc = GetDC(hwnd);
    HFONT old = (HFONT)SelectObject(dc, g_font);
    int w = get_widget_width(dc);
    SelectObject(dc, old);
    ReleaseDC(hwnd, dc);

    int h = g_font_h * 2 + g_row_gap + g_pad_y * 2;
    if (w < 10)
        w = 10;

    HWND taskbar = FindWindowW(L"Shell_TrayWnd", NULL);
    if (taskbar)
    {
        RECT trc, nrc;
        GetWindowRect(taskbar, &trc);
        HWND tray = FindWindowExW(taskbar, NULL, L"TrayNotifyWnd", NULL);
        int y = trc.top + ((trc.bottom - trc.top) - h) / 2;
        int target_x = trc.right - w - 200;

        if (tray && GetWindowRect(tray, &nrc))
        {
            NeighborScanData data;
            data.self = hwnd;
            data.taskbar = taskbar;
            data.trc = trc;
            data.tray_left = nrc.left;
            data.leftmost_neighbor_x = nrc.left;

            EnumChildWindows(taskbar, scan_neighbor_proc, (LPARAM)&data);
            EnumWindows(scan_neighbor_proc, (LPARAM)&data);

            target_x = data.leftmost_neighbor_x - w - 4;
        }

        SetWindowPos(hwnd, HWND_TOPMOST, target_x, y, w, h, SWP_NOACTIVATE);
    }
    else
    {
        SetWindowPos(hwnd, HWND_TOPMOST, GetSystemMetrics(SM_CXSCREEN) - w - 200, GetSystemMetrics(SM_CYSCREEN) - h, w, h, SWP_NOACTIVATE);
    }
}

static LRESULT CALLBACK wndproc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_PAINT)
    {
        paint_widget(h);
        return 0;
    }
    if (msg == WM_ERASEBKGND)
        return 1;
    if (msg == WM_DPICHANGED)
    {
        update_dpi_scale(h);
        resize_and_position(h);
        InvalidateRect(h, NULL, TRUE);
        return 0;
    }
    if (msg == WM_SETTINGCHANGE)
    {
        update_theme_colors();
        InvalidateRect(h, NULL, TRUE);
        return 0;
    }
    if (msg == WM_TIMER && wp == TIMER_ID)
    {
        if (is_fullscreen_app_active(h))
        {
            if (IsWindowVisible(h))
                ShowWindow(h, SW_HIDE);
            return 0;
        }
        else
        {
            if (!IsWindowVisible(h))
                ShowWindow(h, SW_SHOWNOACTIVATE);
        }
        resize_and_position(h);
        update_metrics();
        InvalidateRect(h, NULL, FALSE);
        return 0;
    }
    if (msg == WM_APP_CONFIG_CHANGED)
    {
        resize_and_position(h);
        InvalidateRect(h, NULL, TRUE);
        SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
        return 0;
    }
    if (msg == WM_CONTEXTMENU || msg == WM_RBUTTONUP)
    {
        if (cfg_count <= 0)
            load_config();

        HMENU m = CreatePopupMenu();
        AppendMenuW(m, MF_STRING, 1, L"Task Manager");

        HMENU sub = CreatePopupMenu();
        for (int i = 0; i < cfg_count; i++)
        {
            UINT flags = MF_STRING | (cfg_items[i].active ? MF_CHECKED : MF_UNCHECKED);
            AppendMenuW(sub, flags, 100 + i, get_metric_friendly(cfg_items[i].id));
        }
        AppendMenuW(m, MF_POPUP, (UINT_PTR)sub, L"Metrics");
        AppendMenuW(m, MF_STRING, 2, L"Settings...");
        AppendMenuW(m, MF_SEPARATOR, 0, NULL);
        AppendMenuW(m, MF_STRING, 3, L"Exit");

        POINT pt;
        GetCursorPos(&pt);
        SetForegroundWindow(h);
        int cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, pt.x, pt.y, 0, h, NULL);
        PostMessage(h, WM_NULL, 0, 0);
        if (cmd == 1)
        {
            ShellExecuteW(NULL, L"open", L"taskmgr.exe", NULL, NULL, SW_SHOW);
        }
        else if (cmd == 2)
        {
            show_settings_dialog(GetModuleHandle(NULL), h);
        }
        else if (cmd == 3)
        {
            DestroyWindow(h);
        }
        else if (cmd >= 100 && cmd < 100 + cfg_count)
        {
            int idx = cmd - 100;
            cfg_items[idx].active = !cfg_items[idx].active;
            save_config();
            resize_and_position(h);
            InvalidateRect(h, NULL, TRUE);
            SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
        }
        DestroyMenu(m);
        return 0;
    }
    if (msg == WM_DESTROY)
    {
        KillTimer(h, TIMER_ID);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

static HWND create_widget(HINSTANCE inst)
{
    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = wndproc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = CLASS_NAME;
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
                                CLASS_NAME, NULL, WS_POPUP, 0, 0, 10, 10, NULL, NULL, inst, NULL);
    if (!hwnd)
    {
        printf("CreateWindowExW failed, error: %lu\n", GetLastError());
        return NULL;
    }
    SetLayeredWindowAttributes(hwnd, KEY_COLOR, 0, LWA_COLORKEY);

    load_config();
    resize_and_position(hwnd);
    ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    return hwnd;
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show)
{
    typedef BOOL(WINAPI *pfn_SetProcessDpiAwarenessContext)(HANDLE);
    HMODULE u32 = GetModuleHandleW(L"user32.dll");
    if (u32)
    {
        pfn_SetProcessDpiAwarenessContext set_dpi = (pfn_SetProcessDpiAwarenessContext)GetProcAddress(u32, "SetProcessDpiAwarenessContext");
        if (set_dpi)
            set_dpi((HANDLE)-4); /* DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 */
    }

    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icex);

    init_gui();
    init_metrics();

    if (!(g_hwnd = create_widget(inst)))
    {
        return 1;
    }

    SetTimer(g_hwnd, TIMER_ID, UPDATE_MS, NULL);
    InvalidateRect(g_hwnd, NULL, FALSE);
    SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    shutdown_metrics();
    cleanup_gui();
    return 0;
}
