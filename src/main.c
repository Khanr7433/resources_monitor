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

static void resize_and_position(HWND hwnd)
{
    HDC dc = GetDC(hwnd);
    HFONT old = (HFONT)SelectObject(dc, g_font);
    int w = get_widget_width(dc);
    SelectObject(dc, old);
    ReleaseDC(hwnd, dc);

    int h = FONT_H * 2 + ROW_GAP + PAD_Y * 2;
    if (w < 10)
        w = 10;

    HWND taskbar = FindWindowW(L"Shell_TrayWnd", NULL);
    if (taskbar)
    {
        RECT trc, nrc;
        GetWindowRect(taskbar, &trc);
        HWND tray = FindWindowExW(taskbar, NULL, L"TrayNotifyWnd", NULL);
        int y = trc.top + ((trc.bottom - trc.top) - h) / 2;
        int x = tray && GetWindowRect(tray, &nrc) ? nrc.left - w - 4 : trc.right - w - 200;
        SetWindowPos(hwnd, HWND_TOPMOST, x, y, w, h, SWP_NOACTIVATE);
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
    if (msg == WM_TIMER && wp == TIMER_ID)
    {
        update_metrics();
        InvalidateRect(h, NULL, FALSE);
        return 0;
    }
    if (msg == WM_APP_CONFIG_CHANGED)
    {
        resize_and_position(h);
        InvalidateRect(h, NULL, TRUE);
        return 0;
    }
    if (msg == WM_CONTEXTMENU || msg == WM_RBUTTONUP)
    {
        HMENU m = CreatePopupMenu();
        AppendMenuW(m, MF_STRING, 1, L"Task Manager");
        AppendMenuW(m, MF_STRING, 2, L"Settings");
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
    SetLayeredWindowAttributes(hwnd, 0, 230, LWA_ALPHA);

    load_config();
    resize_and_position(hwnd);
    ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    return hwnd;
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show)
{
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
