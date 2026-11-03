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

COLORREF g_clr_label = CLR_DARK_LABEL;
COLORREF g_clr_value = CLR_DARK_VALUE;

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
        g_font = CreateFontW(-g_font_h, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    }
}

/* ================================================================
 *  Formatting & Strings
 * ================================================================ */
static void fmt_bytes(double b, wchar_t *buf, int n)
{
    if (b >= 1073741824.0)
        swprintf(buf, n, L"%.2f GB", b / 1073741824.0);
    else if (b >= 1048576.0)
        swprintf(buf, n, L"%.2f MB", b / 1048576.0);
    else
        swprintf(buf, n, L"%.2f KB", b / 1024.0);
}

static void fmt_speed(double bps, wchar_t *buf, int n)
{
    if (bps >= 1073741824.0)
        swprintf(buf, n, L"%.2f GB/s", bps / 1073741824.0);
    else if (bps >= 1048576.0)
        swprintf(buf, n, L"%.2f MB/s", bps / 1048576.0);
    else
        swprintf(buf, n, L"%.2f KB/s", bps / 1024.0);
}

static void get_metric_strings(int id, int is_bottom, wchar_t *lbl, int lbl_sz, wchar_t *val, int val_sz, wchar_t *worst, int worst_sz)
{
    lbl[0] = 0;
    val[0] = 0;
    worst[0] = 0;

    switch (id)
    {
    case M_NET_DL:
        wcsncpy(lbl, L"\u2193: ", lbl_sz);
        fmt_speed(g_metrics.net_dl_speed, val, val_sz);
        wcsncpy(worst, L"999.99 MB/s", worst_sz);
        break;
    case M_NET_UL:
        wcsncpy(lbl, L"\u2191: ", lbl_sz);
        fmt_speed(g_metrics.net_ul_speed, val, val_sz);
        wcsncpy(worst, L"999.99 MB/s", worst_sz);
        break;
    case M_NET_ALL:
        wcsncpy(lbl, L"\u2191\u2193: ", lbl_sz);
        fmt_speed(g_metrics.net_speed, val, val_sz);
        wcsncpy(worst, L"999.99 MB/s", worst_sz);
        break;
    case M_NET_TOT:
        if (!is_bottom)
        {
            wcsncpy(lbl, L"Total traffic:", lbl_sz);
            val[0] = 0;
            worst[0] = 0;
        }
        else
        {
            lbl[0] = 0;
            fmt_bytes(g_metrics.total_traffic, val, val_sz);
            wcsncpy(worst, L"999.99 GB", worst_sz);
        }
        break;
    case M_CPU_TEMP:
        wcsncpy(lbl, L"CPU: ", lbl_sz);
        if (g_metrics.cpu_temp >= 0)
            swprintf(val, val_sz, L"%.0f \u00B0C", g_metrics.cpu_temp);
        else
            wcsncpy(val, L"-- \u00B0C", val_sz);
        wcsncpy(worst, L"100 \u00B0C", worst_sz);
        break;
    case M_CPU_USAGE:
        wcsncpy(lbl, L"CPU: ", lbl_sz);
        swprintf(val, val_sz, L"%.0f %%", g_metrics.cpu_pct);
        wcsncpy(worst, L"100 %", worst_sz);
        break;
    case M_CPU_FREQ:
        wcsncpy(lbl, L"CPU: ", lbl_sz);
        swprintf(val, val_sz, L"%.2f GHz", g_metrics.cpu_ghz);
        wcsncpy(worst, L"9.99 GHz", worst_sz);
        break;
    case M_RAM_USAGE:
        wcsncpy(lbl, L"MEM: ", lbl_sz);
        swprintf(val, val_sz, L"%.0f %%", g_metrics.mem_pct);
        wcsncpy(worst, L"100 %", worst_sz);
        break;
    case M_GPU_TEMP:
        wcsncpy(lbl, L"GPU: ", lbl_sz);
        if (g_metrics.gpu_temp >= 0)
            swprintf(val, val_sz, L"%.0f \u00B0C", g_metrics.gpu_temp);
        else
            wcsncpy(val, L"-- \u00B0C", val_sz);
        wcsncpy(worst, L"100 \u00B0C", worst_sz);
        break;
    case M_GPU_USAGE:
        wcsncpy(lbl, L"GPU: ", lbl_sz);
        if (g_metrics.gpu_pct >= 0)
            swprintf(val, val_sz, L"%.0f %%", g_metrics.gpu_pct);
        else
            wcsncpy(val, L"0 %", val_sz);
        wcsncpy(worst, L"100 %", worst_sz);
        break;
    case M_GPU_VRAM:
        wcsncpy(lbl, L"VRAM: ", lbl_sz);
        if (g_metrics.gpu_vram_used >= 0)
            swprintf(val, val_sz, L"%.1f GB", g_metrics.gpu_vram_used / 1073741824.0);
        else
            wcsncpy(val, L"-- GB", val_sz);
        wcsncpy(worst, L"99.9 GB", worst_sz);
        break;
    case M_DISK_USAGE:
        wcsncpy(lbl, L"DSK: ", lbl_sz);
        if (g_metrics.disk_pct >= 0)
            swprintf(val, val_sz, L"%.0f %%", g_metrics.disk_pct);
        else
            wcsncpy(val, L"-- %", val_sz);
        wcsncpy(worst, L"100 %", worst_sz);
        break;
    }
}

/* ================================================================
 *  Layout & Measurement
 * ================================================================ */
typedef struct
{
    int top_id;
    int bottom_id;
    BOOL is_tot_traffic;
    int width;
} WidgetCol;

static int measure_item_width(HDC hdc, int id, int is_bottom)
{
    wchar_t lbl[32], val[32], worst[32];
    get_metric_strings(id, is_bottom, lbl, 32, val, 32, worst, 32);

    SIZE sz_lbl = {0}, sz_val = {0}, sz_worst = {0};
    if (lbl[0])
        GetTextExtentPoint32W(hdc, lbl, (int)wcslen(lbl), &sz_lbl);
    if (val[0])
        GetTextExtentPoint32W(hdc, val, (int)wcslen(val), &sz_val);
    if (worst[0])
        GetTextExtentPoint32W(hdc, worst, (int)wcslen(worst), &sz_worst);

    int val_w = (sz_val.cx > sz_worst.cx) ? sz_val.cx : sz_worst.cx;
    return sz_lbl.cx + val_w;
}

static int build_columns(HDC hdc, WidgetCol *cols, int max_cols)
{
    int col_cnt = 0;
    BOOL has_top = FALSE;

    for (int i = 0; i < cfg_count; i++)
    {
        if (!cfg_items[i].active)
            continue;

        int id = cfg_items[i].id;
        if (id == M_NET_TOT)
        {
            if (has_top)
            {
                cols[col_cnt].bottom_id = -1;
                cols[col_cnt].is_tot_traffic = FALSE;
                col_cnt++;
                has_top = FALSE;
                if (col_cnt >= max_cols)
                    break;
            }
            cols[col_cnt].top_id = M_NET_TOT;
            cols[col_cnt].bottom_id = M_NET_TOT;
            cols[col_cnt].is_tot_traffic = TRUE;
            col_cnt++;
            if (col_cnt >= max_cols)
                break;
            continue;
        }

        if (!has_top)
        {
            if (col_cnt >= max_cols)
                break;
            cols[col_cnt].top_id = id;
            cols[col_cnt].bottom_id = -1;
            cols[col_cnt].is_tot_traffic = FALSE;
            has_top = TRUE;
        }
        else
        {
            cols[col_cnt].bottom_id = id;
            col_cnt++;
            has_top = FALSE;
            if (col_cnt >= max_cols)
                break;
        }
    }

    if (has_top && col_cnt < max_cols)
    {
        cols[col_cnt].bottom_id = -1;
        cols[col_cnt].is_tot_traffic = FALSE;
        col_cnt++;
    }

    for (int c = 0; c < col_cnt; c++)
    {
        int w_top = 0, w_bot = 0;
        if (cols[c].is_tot_traffic)
        {
            w_top = measure_item_width(hdc, M_NET_TOT, 0);
            w_bot = measure_item_width(hdc, M_NET_TOT, 1);
        }
        else
        {
            if (cols[c].top_id >= 0)
                w_top = measure_item_width(hdc, cols[c].top_id, 0);
            if (cols[c].bottom_id >= 0)
                w_bot = measure_item_width(hdc, cols[c].bottom_id, 1);
        }
        cols[c].width = (w_top > w_bot) ? w_top : w_bot;
    }

    return col_cnt;
}

int get_widget_width(HDC dc)
{
    WidgetCol cols[16];
    int col_cnt = build_columns(dc, cols, 16);
    if (col_cnt == 0)
        return g_pad_x * 2;

    int total_w = g_pad_x;
    for (int i = 0; i < col_cnt; i++)
    {
        total_w += cols[i].width;
        if (i < col_cnt - 1)
            total_w += g_col_gap;
    }
    total_w += g_pad_x;
    return total_w;
}

static void draw_item(HDC hdc, int x, int y, int id, int is_bottom)
{
    wchar_t lbl[32], val[32], worst[32];
    get_metric_strings(id, is_bottom, lbl, 32, val, 32, worst, 32);

    SIZE sz_lbl = {0};
    if (lbl[0])
    {
        SetTextColor(hdc, g_clr_label);
        TextOutW(hdc, x, y, lbl, (int)wcslen(lbl));
        GetTextExtentPoint32W(hdc, lbl, (int)wcslen(lbl), &sz_lbl);
    }
    if (val[0])
    {
        SetTextColor(hdc, g_clr_value);
        TextOutW(hdc, x + sz_lbl.cx, y, val, (int)wcslen(val));
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

    HBRUSH bg = CreateSolidBrush(KEY_COLOR);
    FillRect(s_mem_dc, &rc, bg);
    DeleteObject(bg);

    SelectObject(s_mem_dc, g_font);
    SetBkMode(s_mem_dc, TRANSPARENT);

    WidgetCol cols[16];
    int col_cnt = build_columns(s_mem_dc, cols, 16);

    int cur_x = g_pad_x;
    int y_top = g_pad_y;
    int y_bot = g_pad_y + g_font_h + g_row_gap;

    for (int i = 0; i < col_cnt; i++)
    {
        if (cols[i].is_tot_traffic)
        {
            draw_item(s_mem_dc, cur_x, y_top, M_NET_TOT, 0);
            draw_item(s_mem_dc, cur_x, y_bot, M_NET_TOT, 1);
        }
        else
        {
            if (cols[i].top_id >= 0)
                draw_item(s_mem_dc, cur_x, y_top, cols[i].top_id, 0);
            if (cols[i].bottom_id >= 0)
                draw_item(s_mem_dc, cur_x, y_bot, cols[i].bottom_id, 1);
        }
        cur_x += cols[i].width + g_col_gap;
    }

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
    if (use_default || cfg_count <= 0)
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
