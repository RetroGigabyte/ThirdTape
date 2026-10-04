#pragma once

// abgr8888 color
// COLOR_GRAY(0xFF) = WHITE, COLOR_GRAY(0x00) = BLACK
#define COLOR_GRAY(x) (0xFF000000 | (x) << 16 | (x) << 8 | (x))
#define COLOR_RGB(r, g, b) (0xFF000000 | (b) << 16 | (g) << 8 | (r))

#define DEF_DRAW_RED 0xFF0000FF
#define DEF_DRAW_GREEN 0xFF00FF00
#define DEF_DRAW_BLUE 0xFFFF0000
#define DEF_DRAW_BLACK 0xFF000000
#define DEF_DRAW_WHITE 0xFFFFFFFF
#define DEF_DRAW_AQUA 0xFFFFFF00
#define DEF_DRAW_YELLOW 0xFF00C5FF
#define DEF_DRAW_LIGHT_GRAY 0xFFAAAAAA
#define DEF_DRAW_GRAY 0xFF777777
#define DEF_DRAW_DARK_GRAY 0xFF333333
#define DEF_DRAW_WEAK_RED 0x500000FF
#define DEF_DRAW_WEAK_ORANGE 0x500078FF
#define DEF_DRAW_WEAK_GREEN 0x5000FF00
#define DEF_DRAW_WEAK_BLUE 0x50FF0000
#define DEF_DRAW_WEAK_BLACK 0x50000000
#define DEF_DRAW_WEAK_WHITE 0x50FFFFFF
#define DEF_DRAW_WEAK_AQUA 0x50FFFF00
#define DEF_DRAW_WEAK_YELLOW 0x5000C5FF
#define DEF_DRAW_NO_COLOR 0x0

#define COLOR_LIGHT_BLUE 0xFFEEAAAA
#define COLOR_LIGHT_GREEN 0xFFAAEEAA

#define COLOR_LINK 0xFFCB6600

// 3DS System Settings look: white content on blue-grey chrome with the 3DS blue as accent
#define COLOR_ACCENT COLOR_RGB(0x2C, 0x9B, 0xE6)
#define DEFAULT_TEXT_COLOR (var_night_mode ? COLOR_GRAY(0xFF) : COLOR_RGB(0x1E, 0x24, 0x2B))
#define LIGHT0_TEXT_COLOR (var_night_mode ? COLOR_GRAY(0xCC) : COLOR_RGB(0x3A, 0x42, 0x4B))
#define LIGHT1_TEXT_COLOR (var_night_mode ? COLOR_GRAY(0xA0) : COLOR_RGB(0x6B, 0x75, 0x80))
#define DEFAULT_BACK_COLOR (var_night_mode ? COLOR_GRAY(0x00) : COLOR_GRAY(0xFF))
#define LIGHT0_BACK_COLOR (var_night_mode ? COLOR_GRAY(0x22) : COLOR_RGB(0xEE, 0xF2, 0xF6))
#define LIGHT1_BACK_COLOR (var_night_mode ? COLOR_GRAY(0x50) : COLOR_RGB(0xD3, 0xDB, 0xE3))
#define LIGHT2_BACK_COLOR (var_night_mode ? COLOR_GRAY(0x70) : COLOR_RGB(0x8A, 0x97, 0xA5))
#define LIGHT3_BACK_COLOR (var_night_mode ? COLOR_GRAY(0xA0) : COLOR_ACCENT)
// tab bars, separators, buttons
#define TAB_BAR_COLOR (var_night_mode ? COLOR_GRAY(0x30) : COLOR_RGB(0xDD, 0xE4, 0xEB))
#define TAB_SELECTED_COLOR (var_night_mode ? COLOR_GRAY(0x10) : COLOR_GRAY(0xFF))
#define TAB_BORDER_COLOR (var_night_mode ? COLOR_GRAY(0x60) : COLOR_RGB(0xB3, 0xBD, 0xC8))
#define TAB_TEXT_SELECTED_COLOR (var_night_mode ? COLOR_GRAY(0xFF) : COLOR_RGB(0x1B, 0x7F, 0xC4))
#define TAB_TEXT_COLOR (var_night_mode ? COLOR_GRAY(0xA0) : COLOR_RGB(0x55, 0x60, 0x6B))
#define SEPARATOR_COLOR (var_night_mode ? COLOR_GRAY(0x38) : COLOR_RGB(0xDD, 0xE3, 0xE9))
#define BUTTON_COLOR (var_night_mode ? COLOR_GRAY(0x40) : COLOR_RGB(0xF4, 0xF6, 0xF8))
