#pragma once
#include <windows.h>

extern HFONT g_font;
extern int g_font_h, g_pad_x, g_pad_y, g_col_gap, g_row_gap;

void init_gui(void);
void cleanup_gui(void);
void update_dpi_scale(HWND hwnd);
void paint_widget(HWND hwnd);
int get_widget_width(HDC hdc);
void show_settings_dialog(HINSTANCE inst, HWND parent);
