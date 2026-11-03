#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <wchar.h>
#include "common.h"
#include "config.h"
#include "metrics.h"
#include "gui.h"

HFONT g_font = NULL;
int g_dpi = 96;
int g_font_h = FONT_H;
int g_pad_x = PAD_X;
int g_pad_y = PAD_Y;
int g_col_gap = COL_GAP;
int g_row_gap = ROW_GAP;

static HWND g_hSettings = NULL;
static HDC s_mem_dc = NULL;
static HBITMAP s_mem_bmp = NULL;
static HBITMAP s_old_bmp = NULL;
static int s_bmp_w = 0, s_bmp_h = 0;

void update_dpi_scale(HWND hwnd)
{
    typedef UINT(WINAPI *pfn_GetDpiForWindow)(HWND);
    HMODULE u32 = GetModuleHandleW(L"user32.dll");
    int dpi = 96;
    if (u32)
    {
        pfn_GetDpiForWindow get_dpi = (pfn_GetDpiForWindow)GetProcAddress(u32, "GetDpiForWindow");
        if (get_dpi && hwnd)
            dpi = get_dpi(hwnd);
    }
    if (dpi <= 0)
        dpi = 96;
    if (dpi != g_dpi || !g_font)
    {
        g_dpi = dpi;
        double scale = (double)dpi / 96.0;
        g_font_h = (int)(FONT_H * scale);
        g_pad_x = (int)(PAD_X * scale);
        g_pad_y = (int)(PAD_Y * scale);
        g_col_gap = (int)(COL_GAP * scale);
        g_row_gap = (int)(ROW_GAP * scale);

        if (g_font)
            DeleteObject(g_font);
        g_font = CreateFontW(-g_font_h, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    }
}

/* ================================================================
 *  Formatting & Colors
 * ================================================================ */
static void fmt_bytes(double b, wchar_t *buf, int n)
{
    if (b >= 1073741824.0)
        swprintf(buf, n, L"%.2f GB", b / 1073741824.0);
    else if (b >= 1048576.0)
        swprintf(buf, n, L"%.2f MB", b / 1048576.0);
    else if (b >= 1024.0)
        swprintf(buf, n, L"%.2f KB", b / 1024.0);
    else
        swprintf(buf, n, L"%.0f B", b);
}

static void fmt_speed(double bps, wchar_t *buf, int n)
{
    if (bps >= 1048576.0)
        swprintf(buf, n, L"%.2f MB/s", bps / 1048576.0);
    else if (bps >= 1024.0)
        swprintf(buf, n, L"%.2f KB/s", bps / 1024.0);
    else
        swprintf(buf, n, L"%.0f B/s", bps);
}

static COLORREF pct_color(double pct) { return pct >= 90.0 ? CLR_CRIT : pct >= 70.0 ? CLR_WARN
                                                                                    : CLR_GOOD; }
static COLORREF temp_color(double temp) { return temp >= 85.0 ? CLR_CRIT : temp >= 70.0 ? CLR_WARN
                                                                                        : CLR_GOOD; }

/* ================================================================
 *  Drawing Routines
 * ================================================================ */
static int worst_case_width(HDC hdc, int id)
{
    const wchar_t *s = L"";
    switch (id)
    {
    case M_NET_DL:
        s = L"\u25BC 999.99 MB/s";
        break;
    case M_NET_UL:
        s = L"\u25B2 999.99 MB/s";
        break;
    case M_NET_ALL:
        s = L"\u2195 999.99 MB/s";
        break;
    case M_NET_TOT:
        s = L"\u2211 9999.99 GB";
        break;
    case M_CPU_TEMP:
        s = L"CPU: 100 \u00B0C";
        break;
    case M_CPU_USAGE:
        s = L"CPU: 100 %";
        break;
    case M_CPU_FREQ:
        s = L"CPU: 9.99 GHz";
        break;
    case M_RAM_USAGE:
        s = L"RAM: 128.00 GB (100 %)";
        break;
    case M_GPU_TEMP:
        s = L"GPU: 100 \u00B0C";
        break;
    case M_GPU_USAGE:
        s = L"GPU: 100 %";
        break;
    case M_GPU_VRAM:
        s = L"VRAM: 24.0 GB";
        break;
    case M_DISK_USAGE:
        s = L"DSK: 100 %";
        break;
    }
    SIZE sz;
    GetTextExtentPoint32W(hdc, s, (int)wcslen(s), &sz);
    return sz.cx + g_col_gap;
}

int get_widget_width(HDC dc)
{
    int active_idx[32];
    int a_cnt = 0;
    for (int i = 0; i < cfg_count; i++)
    {
        if (cfg_items[i].active)
            active_idx[a_cnt++] = cfg_items[i].id;
    }
    int half = (a_cnt + 1) / 2;
    int w1 = g_pad_x, w2 = g_pad_x;
    for (int i = 0; i < half; i++)
        w1 += worst_case_width(dc, active_idx[i]);
    for (int i = half; i < a_cnt; i++)
        w2 += worst_case_width(dc, active_idx[i]);
    return (w1 > w2 ? w1 : w2) + g_pad_x;
}

static int draw_single(HDC hdc, int x, int y, const wchar_t *text, COLORREF vc, int gap)
{
    SIZE sz;
    SetTextColor(hdc, vc);
    TextOutW(hdc, x, y, text, (int)wcslen(text));
    GetTextExtentPoint32W(hdc, text, (int)wcslen(text), &sz);
    return x + sz.cx + gap;
}
static int draw_pair(HDC hdc, int x, int y, const wchar_t *label, const wchar_t *value, COLORREF vc)
{
    SIZE sz;
    SetTextColor(hdc, CLR_LABEL);
    TextOutW(hdc, x, y, label, (int)wcslen(label));
    GetTextExtentPoint32W(hdc, label, (int)wcslen(label), &sz);
    x += sz.cx;
    SetTextColor(hdc, vc);
    TextOutW(hdc, x, y, value, (int)wcslen(value));
    GetTextExtentPoint32W(hdc, value, (int)wcslen(value), &sz);
    return x + sz.cx + g_col_gap;
}

static void draw_metric(HDC mem, int *x, int y, int id)
{
    wchar_t buf[64], final[80];
    switch (id)
    {
    case M_NET_DL:
        fmt_speed(g_metrics.net_dl_speed, buf, 64);
        swprintf(final, 80, L"\u25BC %s", buf);
        *x = draw_single(mem, *x, y, final, CLR_NORMAL, g_col_gap);
        break;
    case M_NET_UL:
        fmt_speed(g_metrics.net_ul_speed, buf, 64);
        swprintf(final, 80, L"\u25B2 %s", buf);
        *x = draw_single(mem, *x, y, final, CLR_NORMAL, g_col_gap);
        break;
    case M_NET_ALL:
        fmt_speed(g_metrics.net_speed, buf, 64);
        swprintf(final, 80, L"\u2195 %s", buf);
        *x = draw_single(mem, *x, y, final, CLR_NORMAL, g_col_gap);
        break;
    case M_NET_TOT:
        fmt_bytes(g_metrics.total_traffic, buf, 64);
        swprintf(final, 80, L"\u2211 %s", buf);
        *x = draw_single(mem, *x, y, final, CLR_NORMAL, g_col_gap);
        break;
    case M_CPU_TEMP:
        if (g_metrics.cpu_temp >= 0)
            swprintf(buf, 64, L"%.0f \u00B0C", g_metrics.cpu_temp);
        else
            wcscpy(buf, L"N/A");
        *x = draw_pair(mem, *x, y, L"CPU: ", buf, temp_color(g_metrics.cpu_temp));
        break;
    case M_CPU_USAGE:
        swprintf(buf, 64, L"%.0f %%", g_metrics.cpu_pct);
        *x = draw_pair(mem, *x, y, L"CPU: ", buf, pct_color(g_metrics.cpu_pct));
        break;
    case M_CPU_FREQ:
        swprintf(buf, 64, L"%.2f GHz", g_metrics.cpu_ghz);
        *x = draw_pair(mem, *x, y, L"CPU: ", buf, CLR_NORMAL);
        break;
    case M_RAM_USAGE:
        swprintf(buf, 64, L"%.1f GB (%.0f %%)", g_metrics.mem_used / 1073741824.0, g_metrics.mem_pct);
        *x = draw_pair(mem, *x, y, L"RAM: ", buf, pct_color(g_metrics.mem_pct));
        break;
    case M_GPU_TEMP:
        if (g_metrics.gpu_temp >= 0)
            swprintf(buf, 64, L"%.0f \u00B0C", g_metrics.gpu_temp);
        else
            wcscpy(buf, L"N/A");
        *x = draw_pair(mem, *x, y, L"GPU: ", buf, temp_color(g_metrics.gpu_temp));
        break;
    case M_GPU_USAGE:
        if (g_metrics.gpu_pct >= 0)
            swprintf(buf, 64, L"%.0f %%", g_metrics.gpu_pct);
        else
            wcscpy(buf, L"N/A");
        *x = draw_pair(mem, *x, y, L"GPU: ", buf, g_metrics.gpu_pct >= 0 ? pct_color(g_metrics.gpu_pct) : CLR_LABEL);
        break;
    case M_GPU_VRAM:
        if (g_metrics.gpu_vram_used >= 0)
        {
            double pct = (g_metrics.gpu_vram_used / (g_metrics.gpu_vram_total > 0 ? g_metrics.gpu_vram_total : 1)) * 100.0;
            swprintf(buf, 64, L"%.1f GB", g_metrics.gpu_vram_used / 1073741824.0);
            *x = draw_pair(mem, *x, y, L"VRAM: ", buf, pct_color(pct));
        }
        else
        {
            *x = draw_pair(mem, *x, y, L"VRAM: ", L"N/A", CLR_LABEL);
        }
        break;
    case M_DISK_USAGE:
        if (g_metrics.disk_pct >= 0)
            swprintf(buf, 64, L"%.0f %%", g_metrics.disk_pct);
        else
            wcscpy(buf, L"N/A");
        *x = draw_pair(mem, *x, y, L"DSK: ", buf, pct_color(g_metrics.disk_pct));
        break;
    }
}

void paint_widget(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc;
    GetClientRect(hwnd, &rc);
    if (rc.right <= 0 || rc.bottom <= 0)
    {
        EndPaint(hwnd, &ps);
        return;
    }

    if (!s_mem_dc)
        s_mem_dc = CreateCompatibleDC(hdc);

    if (!s_mem_bmp || s_bmp_w < rc.right || s_bmp_h < rc.bottom)
    {
        if (s_old_bmp)
            SelectObject(s_mem_dc, s_old_bmp);
        if (s_mem_bmp)
            DeleteObject(s_mem_bmp);

        s_bmp_w = rc.right;
        s_bmp_h = rc.bottom;
        s_mem_bmp = CreateCompatibleBitmap(hdc, s_bmp_w, s_bmp_h);
        s_old_bmp = (HBITMAP)SelectObject(s_mem_dc, s_mem_bmp);
    }

    HBRUSH bg = CreateSolidBrush(BG_COLOR);
    FillRect(s_mem_dc, &rc, bg);
    DeleteObject(bg);
    SelectObject(s_mem_dc, g_font);
    SetBkMode(s_mem_dc, TRANSPARENT);

    int active_idx[32];
    int a_cnt = 0;
    for (int i = 0; i < cfg_count; i++)
    {
        if (cfg_items[i].active)
            active_idx[a_cnt++] = cfg_items[i].id;
    }
    int half = (a_cnt + 1) / 2;

    int x = g_pad_x, y = g_pad_y;
    for (int i = 0; i < half; i++)
        draw_metric(s_mem_dc, &x, y, active_idx[i]);

    x = g_pad_x;
    y = g_pad_y + g_font_h + g_row_gap;
    for (int i = half; i < a_cnt; i++)
        draw_metric(s_mem_dc, &x, y, active_idx[i]);

    BitBlt(hdc, 0, 0, rc.right, rc.bottom, s_mem_dc, 0, 0, SRCCOPY);
    EndPaint(hwnd, &ps);
}

void init_gui(void)
{
    update_dpi_scale(NULL);
}

void cleanup_gui(void)
{
    if (s_mem_dc)
    {
        if (s_old_bmp)
            SelectObject(s_mem_dc, s_old_bmp);
        DeleteDC(s_mem_dc);
        s_mem_dc = NULL;
    }
    if (s_mem_bmp)
    {
        DeleteObject(s_mem_bmp);
        s_mem_bmp = NULL;
    }
    if (g_font)
    {
        DeleteObject(g_font);
        g_font = NULL;
    }
}

/* ================================================================
 *  Settings GUI
 * ================================================================ */
#define IDC_LIST 101
#define IDC_UP 102
#define IDC_DOWN 103
#define IDC_DEF 104
#define IDC_OK 105
#define IDC_CANC 106

static void LV_SET_CHK(HWND hwnd, int i, int chk)
{
    LVITEMW lvi = {0};
    lvi.stateMask = LVIS_STATEIMAGEMASK;
    lvi.state = INDEXTOSTATEIMAGEMASK(chk ? 2 : 1);
    SendMessageW(hwnd, LVM_SETITEMSTATE, i, (LPARAM)&lvi);
}
#define LV_GET_CHK(hwnd, i) ((((UINT)(SendMessageW(hwnd, LVM_GETITEMSTATE, (WPARAM)(i), LVIS_STATEIMAGEMASK))) >> 12) - 1)

static void populate_list(HWND hList, int use_default)
{
    SendMessageW(hList, LVM_DELETEALLITEMS, 0, 0);
    if (use_default)
        load_default_config();
    for (int i = 0; i < cfg_count; i++)
    {
        LVITEMW lvi = {0};
        lvi.mask = LVIF_TEXT | LVIF_PARAM;
        lvi.iItem = i;
        lvi.pszText = (LPWSTR)get_metric_friendly(cfg_items[i].id);
        lvi.lParam = cfg_items[i].id;
        SendMessageW(hList, LVM_INSERTITEMW, 0, (LPARAM)&lvi);
        LV_SET_CHK(hList, i, cfg_items[i].active);
    }
}

static void save_list(HWND hList)
{
    cfg_count = (int)SendMessageW(hList, LVM_GETITEMCOUNT, 0, 0);
    for (int i = 0; i < cfg_count; i++)
    {
        LVITEMW lvi = {0};
        lvi.mask = LVIF_PARAM;
        lvi.iItem = i;
        SendMessageW(hList, LVM_GETITEMW, 0, (LPARAM)&lvi);
        cfg_items[i].id = (int)lvi.lParam;
        cfg_items[i].active = LV_GET_CHK(hList, i);
    }
    save_config();
}

static void move_list_item(HWND hList, int dir)
{
    int sel = (int)SendMessageW(hList, LVM_GETNEXTITEM, -1, LVNI_SELECTED);
    if (sel == -1)
        return;
    int target = sel + dir;
    if (target < 0 || target >= (int)SendMessageW(hList, LVM_GETITEMCOUNT, 0, 0))
        return;

    wchar_t t1[128], t2[128];
    LVITEMW lvi1 = {0}, lvi2 = {0};
    lvi1.mask = LVIF_TEXT | LVIF_PARAM;
    lvi1.iItem = sel;
    lvi1.pszText = t1;
    lvi1.cchTextMax = 128;
    lvi2.mask = LVIF_TEXT | LVIF_PARAM;
    lvi2.iItem = target;
    lvi2.pszText = t2;
    lvi2.cchTextMax = 128;
    SendMessageW(hList, LVM_GETITEMW, 0, (LPARAM)&lvi1);
    SendMessageW(hList, LVM_GETITEMW, 0, (LPARAM)&lvi2);

    int c1 = LV_GET_CHK(hList, sel), c2 = LV_GET_CHK(hList, target);

    lvi1.iItem = target;
    SendMessageW(hList, LVM_SETITEMW, 0, (LPARAM)&lvi1);
    lvi2.iItem = sel;
    SendMessageW(hList, LVM_SETITEMW, 0, (LPARAM)&lvi2);
    LV_SET_CHK(hList, target, c1);
    LV_SET_CHK(hList, sel, c2);

    SendMessageW(hList, LVM_SETITEMSTATE, sel, (LPARAM) & (LVITEMW){.stateMask = LVIS_SELECTED | LVIS_FOCUSED, .state = 0});
    SendMessageW(hList, LVM_SETITEMSTATE, target, (LPARAM) & (LVITEMW){.stateMask = LVIS_SELECTED | LVIS_FOCUSED, .state = LVIS_SELECTED | LVIS_FOCUSED});
    SendMessageW(hList, LVM_ENSUREVISIBLE, target, FALSE);
}

static LRESULT CALLBACK SettingsWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        HWND hList = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"", WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_NOCOLUMNHEADER | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
                                     15, 15, 180, 230, hwnd, (HMENU)IDC_LIST, GetModuleHandle(NULL), NULL);
        SendMessageW(hList, LVM_SETEXTENDEDLISTVIEWSTYLE, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);
        LVCOLUMNW lvc = {0};
        lvc.mask = LVCF_WIDTH;
        lvc.cx = 160;
        SendMessageW(hList, LVM_INSERTCOLUMNW, 0, (LPARAM)&lvc);

        HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        SendMessageW(hList, WM_SETFONT, (WPARAM)font, FALSE);

        HWND btn;
        btn = CreateWindowW(L"BUTTON", L"Move Up", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 210, 15, 115, 30, hwnd, (HMENU)IDC_UP, GetModuleHandle(NULL), NULL);
        SendMessageW(btn, WM_SETFONT, (WPARAM)font, FALSE);
        btn = CreateWindowW(L"BUTTON", L"Move Down", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 210, 55, 115, 30, hwnd, (HMENU)IDC_DOWN, GetModuleHandle(NULL), NULL);
        SendMessageW(btn, WM_SETFONT, (WPARAM)font, FALSE);
        btn = CreateWindowW(L"BUTTON", L"Restore Default", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 210, 95, 115, 30, hwnd, (HMENU)IDC_DEF, GetModuleHandle(NULL), NULL);
        SendMessageW(btn, WM_SETFONT, (WPARAM)font, FALSE);
        btn = CreateWindowW(L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 150, 260, 80, 30, hwnd, (HMENU)IDC_OK, GetModuleHandle(NULL), NULL);
        SendMessageW(btn, WM_SETFONT, (WPARAM)font, FALSE);
        btn = CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 240, 260, 80, 30, hwnd, (HMENU)IDC_CANC, GetModuleHandle(NULL), NULL);
        SendMessageW(btn, WM_SETFONT, (WPARAM)font, FALSE);

        populate_list(hList, 0);
        return 0;
    }
    case WM_COMMAND:
    {
        int id = LOWORD(wp);
        if (id == IDC_CANC)
        {
            DestroyWindow(hwnd);
            return 0;
        }
        if (id == IDC_OK)
        {
            save_list(GetDlgItem(hwnd, IDC_LIST));
            HWND hParent = GetWindow(hwnd, GW_OWNER);
            if (hParent)
                PostMessage(hParent, WM_APP_CONFIG_CHANGED, 0, 0);
            DestroyWindow(hwnd);
            return 0;
        }
        if (id == IDC_UP || id == IDC_DOWN)
        {
            move_list_item(GetDlgItem(hwnd, IDC_LIST), id == IDC_UP ? -1 : 1);
            return 0;
        }
        if (id == IDC_DEF)
        {
            populate_list(GetDlgItem(hwnd, IDC_LIST), 1);
            return 0;
        }
        break;
    }
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        g_hSettings = NULL;
        break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void show_settings_dialog(HINSTANCE inst, HWND parent)
{
    if (!g_hSettings)
    {
        WNDCLASSEXW sc = {0};
        sc.cbSize = sizeof(sc);
        sc.lpfnWndProc = SettingsWndProc;
        sc.hInstance = inst;
        sc.hCursor = LoadCursor(NULL, IDC_ARROW);
        sc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        sc.lpszClassName = L"ResMonSettingsClass";
        RegisterClassExW(&sc);

        g_hSettings = CreateWindowExW(0, L"ResMonSettingsClass", L"Display settings",
                                      (WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX) | WS_VISIBLE,
                                      CW_USEDEFAULT, CW_USEDEFAULT, 360, 340, parent, NULL, inst, NULL);
    }
    else
    {
        SetForegroundWindow(g_hSettings);
    }
}
