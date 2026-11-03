#pragma once
#include <windows.h>

#define TIMER_ID 1
#define UPDATE_MS 1000

/* Transparency Key Color */
#define KEY_COLOR RGB(10, 10, 10)

/* Dark Theme Colors */
#define CLR_DARK_LABEL RGB(175, 175, 175)
#define CLR_DARK_VALUE RGB(255, 255, 255)

/* Light Theme Colors */
#define CLR_LIGHT_LABEL RGB(90, 90, 90)
#define CLR_LIGHT_VALUE RGB(20, 20, 20)

extern COLORREF g_clr_label;
extern COLORREF g_clr_value;

#define FONT_H 12
#define PAD_X 10
#define PAD_Y 4
#define COL_GAP 14
#define ROW_GAP 3
#define CLASS_NAME L"ResMonWidget"

#define WM_APP_CONFIG_CHANGED (WM_APP + 1)
