#pragma once
#include <windows.h>

extern HFONT g_font;

void init_gui(void);
void cleanup_gui(void);
void paint_widget(HWND hwnd);
int get_widget_width(HDC hdc);
void show_settings_dialog(HINSTANCE inst, HWND parent);
