// Clay menu screens: title / main menu / player setup / how-to. Owns the screen
// state machine + gamepad focus (clay_nav). Compiled as C (gnu99).
#include "clay.h"
#include "clay_renderer.h"
#include "clay_nav.h"
#include "ui.h"
#include "ui_internal.h"

enum { SCR_TITLE, SCR_MENU, SCR_SETUP, SCR_HOWTO };

static int           g_screen = SCR_TITLE;
static int           g_inGame = 0;
static ClayNav       g_nav;
static ClayNavRepeat g_repV, g_repH;
static UiGameConfig  g_cfg = { 4, { 0, 1, 2, 3 }, { 0, 1, 2, 3 } };

int  ui_in_menu(void)   { return !g_inGame; }
void ui_goto_menu(void) { g_inGame = 0; g_screen = SCR_MENU; g_nav.has_focus = 0; }
void ui_force_game(void) { g_inGame = 1; }

// ---- small building blocks -------------------------------------------------
static void title_bar(const char *text) {
    CLAY(CLAY_ID("TitleBar"), {
        .layout = { .padding = { 16, 16, 10, 10 }, .childAlignment = { .x = CLAY_ALIGN_X_CENTER } }
    }) {
        CLAY_TEXT(ui_str(text), CLAY_TEXT_CONFIG({ .textColor = UI_ACCENT, .fontSize = 26 }));
    }
}

// A focusable menu row. Registers with nav, highlights if focused, and returns 1
// when activated (Cross pressed while focused).
static int menu_item(Clay_ElementId id, const char *label, int cross) {
    clay_nav_add(&g_nav, id);
    int focused = clay_nav_is_focused(&g_nav, id);
    CLAY(id, {
        .layout = {
            .sizing = { CLAY_SIZING_FIXED(340), CLAY_SIZING_FIXED(46) },
            .padding = CLAY_PADDING_ALL(10),
            .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER }
        },
        .backgroundColor = focused ? UI_ROW_FOCUS : UI_ROW,
        .border = { .color = UI_ACCENT, .width = CLAY_BORDER_OUTSIDE(focused ? 2 : 0) }
    }) {
        CLAY_TEXT(ui_str(label),
                  CLAY_TEXT_CONFIG({ .textColor = focused ? UI_TEXT : UI_TEXT_DIM, .fontSize = 22 }));
    }
    return focused && cross;
}

static void hint(const char *text) {
    CLAY_TEXT(ui_str(text), CLAY_TEXT_CONFIG({ .textColor = UI_TEXT_MUTE, .fontSize = 14 }));
}

// ---- screens ---------------------------------------------------------------
static void build_title(void) {
    CLAY(CLAY_ID("Root"), {
        .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
                    .layoutDirection = CLAY_TOP_TO_BOTTOM, .childGap = 14,
                    .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER } },
        .backgroundColor = UI_BG
    }) {
        CLAY_TEXT(ui_str("MONOPOLY"), CLAY_TEXT_CONFIG({ .textColor = UI_ACCENT, .fontSize = 30 }));
        CLAY_TEXT(ui_str("PARTY  -  PS3"), CLAY_TEXT_CONFIG({ .textColor = UI_TEXT, .fontSize = 24 }));
        CLAY(CLAY_ID("TitleGap"), { .layout = { .sizing = { CLAY_SIZING_FIXED(0), CLAY_SIZING_FIXED(30) } } }) {}
        hint("Press  START");
    }
}

static void build_menu(int cross, UiAction *result) {
    CLAY(CLAY_ID("Root"), {
        .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
                    .layoutDirection = CLAY_TOP_TO_BOTTOM, .childGap = 12,
                    .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER } },
        .backgroundColor = UI_BG
    }) {
        title_bar("MONOPOLY PARTY");
        if (menu_item(CLAY_ID("MenuNew"),  "New Game",    cross)) g_screen = SCR_SETUP;
        if (menu_item(CLAY_ID("MenuHow"),  "How to Play", cross)) g_screen = SCR_HOWTO;
        if (menu_item(CLAY_ID("MenuQuit"), "Quit",        cross)) *result = UI_ACT_QUIT;
        CLAY(CLAY_ID("MenuGap"), { .layout = { .sizing = { CLAY_SIZING_FIXED(0), CLAY_SIZING_FIXED(10) } } }) {}
        hint("D-pad: navigate    X: select");
    }
}

static void build_setup(int hdir, int cross, UiAction *result) {
    char players[32];
    snprintf(players, sizeof players, "Players:   < %d >", g_cfg.playerCount);

    CLAY(CLAY_ID("Root"), {
        .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
                    .layoutDirection = CLAY_TOP_TO_BOTTOM, .childGap = 12,
                    .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER } },
        .backgroundColor = UI_BG
    }) {
        title_bar("NEW GAME");

        // Players row (focusable; Left/Right adjust the count)
        clay_nav_add(&g_nav, CLAY_ID("SetupPlayers"));
        int pf = clay_nav_is_focused(&g_nav, CLAY_ID("SetupPlayers"));
        if (pf) {
            if (hdir == CLAY_NAV_LEFT  && g_cfg.playerCount > 2) g_cfg.playerCount--;
            if (hdir == CLAY_NAV_RIGHT && g_cfg.playerCount < 4) g_cfg.playerCount++;
        }
        CLAY(CLAY_ID("SetupPlayers"), {
            .layout = { .sizing = { CLAY_SIZING_FIXED(340), CLAY_SIZING_FIXED(46) },
                        .padding = CLAY_PADDING_ALL(10),
                        .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER } },
            .backgroundColor = pf ? UI_ROW_FOCUS : UI_ROW,
            .border = { .color = UI_ACCENT, .width = CLAY_BORDER_OUTSIDE(pf ? 2 : 0) }
        }) {
            CLAY_TEXT(ui_str(players),
                      CLAY_TEXT_CONFIG({ .textColor = pf ? UI_TEXT : UI_TEXT_DIM, .fontSize = 22 }));
        }

        if (menu_item(CLAY_ID("SetupStart"), "Start Game", cross)) *result = UI_ACT_START_GAME;

        CLAY(CLAY_ID("SetupGap"), { .layout = { .sizing = { CLAY_SIZING_FIXED(0), CLAY_SIZING_FIXED(10) } } }) {}
        hint("Left/Right: players    X: start    O: back");
    }
}

static void build_howto(void) {
    static const char *lines[] = {
        "Roll to move around the board.",
        "Land on an unowned space: X buy, O auction.",
        "Owned space: pay rent automatically.",
        "L2: manage (mortgage / build houses & hotels).",
        "Triangle: offer a trade to another player.",
        "In jail: X roll, Square pay bail, Triangle card.",
        "Bankrupt everyone else to win!",
    };
    CLAY(CLAY_ID("Root"), {
        .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
                    .padding = CLAY_PADDING_ALL(40), .layoutDirection = CLAY_TOP_TO_BOTTOM, .childGap = 10 },
        .backgroundColor = UI_BG
    }) {
        CLAY_TEXT(ui_str("HOW TO PLAY"), CLAY_TEXT_CONFIG({ .textColor = UI_ACCENT, .fontSize = 26 }));
        CLAY(CLAY_ID("HowGap"), { .layout = { .sizing = { CLAY_SIZING_FIXED(0), CLAY_SIZING_FIXED(12) } } }) {}
        for (int i = 0; i < (int)(sizeof lines / sizeof lines[0]); ++i)
            CLAY_TEXT(ui_str(lines[i]), CLAY_TEXT_CONFIG({ .textColor = UI_TEXT_DIM, .fontSize = 18 }));
        CLAY(CLAY_ID("HowGap2"), { .layout = { .sizing = { CLAY_SIZING_FIXED(0), CLAY_SIZING_FIXED(16) } } }) {}
        hint("O: back");
    }
}

// ---- frame -----------------------------------------------------------------
UiAction ui_menu_frame(int hUp, int hDown, int hLeft, int hRight,
                       int eCross, int eCircle, int eStart, UiGameConfig *cfg) {
    UiAction result = UI_ACT_NONE;

    ClayNavDir vdir = clay_nav_repeat(&g_repV, hUp ? CLAY_NAV_UP : hDown ? CLAY_NAV_DOWN : CLAY_NAV_NONE, 18, 6);
    ClayNavDir hdir = clay_nav_repeat(&g_repH, hLeft ? CLAY_NAV_LEFT : hRight ? CLAY_NAV_RIGHT : CLAY_NAV_NONE, 18, 6);

    g_nav.wrap = 1;
    clay_nav_move(&g_nav, vdir);       // vertical lists; horizontal handled per-screen
    clay_nav_begin(&g_nav);

    Clay_SetLayoutDimensions((Clay_Dimensions){ 848, 512 });
    Clay_BeginLayout();
    switch (g_screen) {
        case SCR_TITLE: build_title();                     if (eStart || eCross) { g_screen = SCR_MENU; g_nav.has_focus = 0; } break;
        case SCR_MENU:  build_menu(eCross, &result);       break;
        case SCR_SETUP: build_setup(hdir, eCross, &result); if (eCircle) { g_screen = SCR_MENU; g_nav.has_focus = 0; } break;
        case SCR_HOWTO: build_howto();                     if (eCircle || eCross) { g_screen = SCR_MENU; g_nav.has_focus = 0; } break;
    }
    Clay_RenderCommandArray cmds = Clay_EndLayout(0.0f);
    clay_render(cmds);

    if (result == UI_ACT_START_GAME) { g_inGame = 1; *cfg = g_cfg; }
    return result;
}
