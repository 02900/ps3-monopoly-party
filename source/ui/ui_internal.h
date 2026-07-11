#ifndef MONOPOLY_UI_INTERNAL_H
#define MONOPOLY_UI_INTERNAL_H
// Shared helpers + palette for the Clay UI C TUs (screens.c, hud.c, overlays.c).
// C only — never included by the C++ game.
#include "clay.h"
#include <string.h>
#include <stdio.h>

// A Clay_String from a C string. CLAY_STRING only takes literals; runtime strings
// go through here. The pointer must stay alive through clay_render() — pass a
// buffer with a lifetime that spans the frame (a static or a stack var in the
// building function is fine since clay_render runs before it returns).
static inline Clay_String ui_str(const char *s) {
    Clay_String r; r.chars = s; r.length = (int32_t) strlen(s); r.isStaticallyAllocated = false;
    return r;
}

// 0xRRGGBBAA -> Clay_Color (float 0-255).
static inline Clay_Color ui_rgba(unsigned c) {
    Clay_Color r;
    r.r = (float)((c >> 24) & 0xff);
    r.g = (float)((c >> 16) & 0xff);
    r.b = (float)((c >>  8) & 0xff);
    r.a = (float)( c        & 0xff);
    return r;
}

// ---- palette (a cohesive Monopoly-ish board-game look) --------------------
#define UI_BG        ui_rgba(0x0E1A2Eff)   // deep navy backdrop
#define UI_PANEL     ui_rgba(0x1B2A38ff)   // panel fill
#define UI_PANEL2    ui_rgba(0x243647ff)   // raised row
#define UI_ROW       ui_rgba(0x2C4256ff)
#define UI_ROW_FOCUS ui_rgba(0x3E6EA5ff)   // focused row
#define UI_BORDER    ui_rgba(0xF5F0E1ff)   // cream border
#define UI_ACCENT    ui_rgba(0xE8B84Bff)   // gold accent
#define UI_TEXT      ui_rgba(0xFFFFFFff)
#define UI_TEXT_DIM  ui_rgba(0xAFC0D0ff)
#define UI_TEXT_MUTE ui_rgba(0x8090A0ff)
#define UI_GOOD      ui_rgba(0x9FE6A0ff)
#define UI_BAD       ui_rgba(0xE57373ff)

// Player token colours (match Main.cpp PLAYER_COL: red / green / blue / yellow).
static inline Clay_Color ui_player_color(int p) {
    switch (p) {
        case 0: return ui_rgba(0xE53935ff);
        case 1: return ui_rgba(0x43A047ff);
        case 2: return ui_rgba(0x1E88E5ff);
        default:return ui_rgba(0xFDD835ff);
    }
}

#endif // MONOPOLY_UI_INTERNAL_H
