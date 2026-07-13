// Clay menu screens: title / main menu / player setup / how-to. Owns the screen
// state machine + gamepad focus (clay_nav). Compiled as C (gnu99).
#include "clay.h"
#include "clay_renderer.h"
#include "clay_nav.h"
#include "ui.h"
#include "ui_internal.h"
#include "theme.h"

enum { SCR_TITLE, SCR_MENU, SCR_SETUP, SCR_HOWTO };

static int           g_screen = SCR_TITLE;
static int           g_inGame = 0;
static int           g_theme  = UI_THEME_CLASSIC;   // chosen token theme
static ClayNav       g_nav;
static ClayNavRepeat g_repV, g_repH;
static UiGameConfig  g_cfg = { 4, { 0, 0, 0, 0 }, { 0, 1, 2, 3 } };

int  ui_in_menu(void)   { return !g_inGame; }
void ui_goto_menu(void) { g_inGame = 0; g_screen = SCR_MENU; g_nav.has_focus = 0; }
void ui_force_game(void) { g_inGame = 1; }

// ---- small building blocks -------------------------------------------------
static unsigned g_tick = 0;   // frame counter (blink/pulse); advanced each menu frame

// Draw the current screen's full-screen background art (called from the game
// loop before the Clay menu, since the menu roots are transparent). Falls back
// to the shared menu_bg plate if the per-screen one isn't loaded.
void ui_menu_bg(void) {
    int id;
    switch (g_screen) {
        case SCR_TITLE: id = UI_IMG_BG_TITLE; break;
        case SCR_MENU:  id = UI_IMG_BG_MENU;  break;
        case SCR_SETUP: id = UI_IMG_BG_MENU;  break;   // setup shares the menu plate
        default:        id = UI_IMG_BG_HOWTO; break;
    }
    if (!ui_draw_cover(id)) ui_draw_cover(UI_IMG_BG);
}

static void title_bar(const char *text) {
    CLAY(CLAY_ID("TitleBar"), {
        .layout = { .padding = { 16, 16, 10, 10 }, .childAlignment = { .x = CLAY_ALIGN_X_CENTER } }
    }) {
        CLAY_TEXT(ui_str(text), CLAY_TEXT_CONFIG({ .textColor = UI_ACCENT, .fontSize = 26 }));
    }
}

// Full-screen dark scrim over the background image so foreground UI reads clearly.
// zIndex < 0 keeps it behind the (non-floating) menu content.
static void scrim(void) {
    CLAY(CLAY_ID("Scrim"), {
        .floating = { .attachTo = CLAY_ATTACH_TO_ROOT,
                      .attachPoints = { CLAY_ATTACH_POINT_LEFT_TOP, CLAY_ATTACH_POINT_LEFT_TOP },
                      .offset = { 0, 0 }, .zIndex = -10 },
        .layout = { .sizing = { CLAY_SIZING_FIXED(848), CLAY_SIZING_FIXED(512) } },
        .backgroundColor = ui_rgba(0x0A132266)
    }) {}
}

// Bottom status bar: left blurb + right controls hint, with a gold top rule.
// zIndex > 0 keeps it above the scrim and content.
static void footer(const char *left, const char *right) {
    CLAY(CLAY_ID("Footer"), {
        .floating = { .attachTo = CLAY_ATTACH_TO_ROOT,
                      .attachPoints = { CLAY_ATTACH_POINT_LEFT_BOTTOM, CLAY_ATTACH_POINT_LEFT_BOTTOM },
                      .offset = { 0, 0 }, .zIndex = 10 },
        .layout = { .sizing = { CLAY_SIZING_FIXED(848), CLAY_SIZING_FIXED(32) },
                    .padding = { 20, 20, 6, 6 }, .childGap = 8,
                    .childAlignment = { .y = CLAY_ALIGN_Y_CENTER } },
        .backgroundColor = ui_rgba(0x0A1322d8),
        .border = { .color = UI_ACCENT, .width = { .top = 2 } }
    }) {
        CLAY_TEXT(ui_str(left), CLAY_TEXT_CONFIG({ .textColor = UI_TEXT_MUTE, .fontSize = 13 }));
        CLAY(CLAY_ID("FootSpacer"), { .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(0) } } }) {}
        CLAY_TEXT(ui_str(right), CLAY_TEXT_CONFIG({ .textColor = UI_TEXT_MUTE, .fontSize = 13 }));
    }
}

// A focusable menu row: left-aligned label with a gold caret when focused.
// Registers with nav and returns 1 when activated (Cross while focused).
static int menu_item(Clay_ElementId id, const char *label, int cross) {
    clay_nav_add(&g_nav, id);
    int focused = clay_nav_is_focused(&g_nav, id);
    CLAY(id, {
        .layout = {
            .sizing = { CLAY_SIZING_FIXED(360), CLAY_SIZING_FIXED(44) },
            .padding = { 18, 16, 10, 10 }, .childGap = 12,
            .childAlignment = { .y = CLAY_ALIGN_Y_CENTER }
        },
        .backgroundColor = focused ? UI_ROW_FOCUS : ui_rgba(0x18293Ae0),
        .border = { .color = UI_ACCENT, .width = CLAY_BORDER_OUTSIDE(focused ? 2 : 1) }
    }) {
        CLAY_TEXT(ui_str(focused ? ">" : " "),
                  CLAY_TEXT_CONFIG({ .textColor = UI_ACCENT, .fontSize = 22 }));
        CLAY_TEXT(ui_str(label),
                  CLAY_TEXT_CONFIG({ .textColor = focused ? UI_TEXT : UI_TEXT_DIM, .fontSize = 22 }));
    }
    return focused && cross;
}

// ---- screens ---------------------------------------------------------------
static void build_title(void) {
    CLAY(CLAY_ID("Root"), {
        .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
                    .layoutDirection = CLAY_TOP_TO_BOTTOM, .childGap = 10,
                    .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER } },
        .backgroundColor = ui_rgba(0x00000000)   // transparent: ya2d bg shows through
    }) {
        scrim();
        // logo image; falls back to the wordmark text if it isn't loaded
        if (ui_image(UI_IMG_LOGO)) {
            CLAY(CLAY_ID("Logo"), {
                .layout = { .sizing = { CLAY_SIZING_FIXED(440), CLAY_SIZING_FIXED(138) } },
                .image = { .imageData = ui_image(UI_IMG_LOGO) }
            }) {}
        } else {
            CLAY_TEXT(ui_str("MONOPOLY"), CLAY_TEXT_CONFIG({ .textColor = UI_ACCENT, .fontSize = 30 }));
            CLAY_TEXT(ui_str("PARTY"), CLAY_TEXT_CONFIG({ .textColor = UI_TEXT, .fontSize = 26 }));
        }
        // subtitle on a dark pill so it stays legible over the busy hero art
        CLAY(CLAY_ID("TitleSub"), {
            .layout = { .padding = { 14, 14, 6, 6 } },
            .backgroundColor = ui_rgba(0x0A1322b4)
        }) {
            CLAY_TEXT(ui_str("The classic board game   -   hot-seat for up to 4"),
                      CLAY_TEXT_CONFIG({ .textColor = UI_TEXT, .fontSize = 16 }));
        }
        CLAY(CLAY_ID("TitleGap"), { .layout = { .sizing = { CLAY_SIZING_FIXED(0), CLAY_SIZING_FIXED(30) } } }) {}
        // blinking call-to-action on a bordered pill (colour pulse -> no layout shift)
        Clay_Color cta = ((g_tick / 22) % 2 == 0) ? UI_ACCENT : ui_rgba(0xE8B84B88);
        CLAY(CLAY_ID("TitleCta"), {
            .layout = { .padding = { 20, 20, 9, 9 } },
            .backgroundColor = ui_rgba(0x0A1322c8),
            .border = { .color = UI_ACCENT, .width = CLAY_BORDER_OUTSIDE(1) }
        }) {
            CLAY_TEXT(ui_str("PRESS   START"), CLAY_TEXT_CONFIG({ .textColor = cta, .fontSize = 24 }));
        }
        footer("MONOPOLY PARTY   -   PS3 homebrew", "jemeador engine");
    }
}

static void build_menu(int cross, UiAction *result) {
    CLAY(CLAY_ID("Root"), {
        .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
                    .layoutDirection = CLAY_TOP_TO_BOTTOM, .childGap = 18,
                    .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER } },
        .backgroundColor = ui_rgba(0x00000000)
    }) {
        scrim();
        // logo (smaller) at the top of the stack; text wordmark as fallback
        if (ui_image(UI_IMG_LOGO)) {
            CLAY(CLAY_ID("MenuLogo"), {
                .layout = { .sizing = { CLAY_SIZING_FIXED(300), CLAY_SIZING_FIXED(94) } },
                .image = { .imageData = ui_image(UI_IMG_LOGO) }
            }) {}
        } else {
            title_bar("MONOPOLY PARTY");
        }
        // framed menu panel
        CLAY(CLAY_ID("MenuPanel"), {
            .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .childGap = 10,
                        .padding = CLAY_PADDING_ALL(18),
                        .childAlignment = { .x = CLAY_ALIGN_X_CENTER } },
            .backgroundColor = ui_rgba(0x0E1B29e0),
            .border = { .color = UI_BORDER, .width = CLAY_BORDER_OUTSIDE(2) }
        }) {
            if (menu_item(CLAY_ID("MenuNew"),  "New Game",    cross)) g_screen = SCR_SETUP;
            if (menu_item(CLAY_ID("MenuHow"),  "How to Play", cross)) g_screen = SCR_HOWTO;
            if (menu_item(CLAY_ID("MenuQuit"), "Quit",        cross)) *result = UI_ACT_QUIT;
        }
        footer("MAIN MENU", "D-pad: navigate    X: select");
    }
}

static void build_setup(int hdir, int cross, UiAction *result) {
    char players[32];
    snprintf(players, sizeof players, "Players:   < %d >", g_cfg.playerCount);

    CLAY(CLAY_ID("Root"), {
        .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
                    .layoutDirection = CLAY_TOP_TO_BOTTOM, .childGap = 12,
                    .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER } },
        .backgroundColor = ui_rgba(0x00000000)
    }) {
        scrim();
        title_bar("NEW GAME");

      CLAY(CLAY_ID("SetupPanel"), {
        .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .childGap = 12,
                    .padding = CLAY_PADDING_ALL(18),
                    .childAlignment = { .x = CLAY_ALIGN_X_CENTER } },
        .backgroundColor = ui_rgba(0x0E1B29e0),
        .border = { .color = UI_BORDER, .width = CLAY_BORDER_OUTSIDE(2) }
      }) {
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

        // Theme row (focusable; Left/Right cycle the token theme)
        char themerow[40];
        snprintf(themerow, sizeof themerow, "Theme:   < %s >", ui_theme_name(g_theme));
        clay_nav_add(&g_nav, CLAY_ID("SetupTheme"));
        int tf = clay_nav_is_focused(&g_nav, CLAY_ID("SetupTheme"));
        if (tf) {
            if (hdir == CLAY_NAV_LEFT)  g_theme = (g_theme + UI_THEME_COUNT - 1) % UI_THEME_COUNT;
            if (hdir == CLAY_NAV_RIGHT) g_theme = (g_theme + 1) % UI_THEME_COUNT;
        }
        CLAY(CLAY_ID("SetupTheme"), {
            .layout = { .sizing = { CLAY_SIZING_FIXED(340), CLAY_SIZING_FIXED(46) },
                        .padding = CLAY_PADDING_ALL(10),
                        .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER } },
            .backgroundColor = tf ? UI_ROW_FOCUS : UI_ROW,
            .border = { .color = UI_ACCENT, .width = CLAY_BORDER_OUTSIDE(tf ? 2 : 0) }
        }) {
            CLAY_TEXT(ui_str(themerow),
                      CLAY_TEXT_CONFIG({ .textColor = tf ? UI_TEXT : UI_TEXT_DIM, .fontSize = 22 }));
        }

        if (menu_item(CLAY_ID("SetupStart"), "Start Game", cross)) *result = UI_ACT_START_GAME;
      }
        footer("NEW GAME", "Left/Right: change    X: start    O: back");
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
                    .layoutDirection = CLAY_TOP_TO_BOTTOM, .childGap = 14,
                    .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER } },
        .backgroundColor = ui_rgba(0x00000000)
    }) {
        scrim();
        CLAY_TEXT(ui_str("HOW TO PLAY"), CLAY_TEXT_CONFIG({ .textColor = UI_ACCENT, .fontSize = 26 }));
        CLAY(CLAY_ID("HowPanel"), {
            .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .childGap = 9,
                        .padding = CLAY_PADDING_ALL(22) },
            .backgroundColor = ui_rgba(0x0E1B29e0),
            .border = { .color = UI_BORDER, .width = CLAY_BORDER_OUTSIDE(2) }
        }) {
            for (int i = 0; i < (int)(sizeof lines / sizeof lines[0]); ++i)
                CLAY_TEXT(ui_str(lines[i]), CLAY_TEXT_CONFIG({ .textColor = UI_TEXT_DIM, .fontSize = 18 }));
        }
        footer("HOW TO PLAY", "O: back");
    }
}

// ---- frame -----------------------------------------------------------------
UiAction ui_menu_frame(int hUp, int hDown, int hLeft, int hRight,
                       int eCross, int eCircle, int eStart, UiGameConfig *cfg) {
    UiAction result = UI_ACT_NONE;
    g_tick++;                          // drives the title's PRESS START blink

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

    if (result == UI_ACT_START_GAME) {
        for (int p = 0; p < 4; ++p) { g_cfg.theme[p] = g_theme; g_cfg.token[p] = p; }
        g_inGame = 1; *cfg = g_cfg;
    }
    return result;
}
