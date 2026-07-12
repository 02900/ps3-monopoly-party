// In-game HUD + overlays rendered with Clay from a plain-C UiSnapshot. The board
// itself stays raw tiny3d (drawn underneath before this runs). Compiled as C (gnu99).
#include "clay.h"
#include "clay_renderer.h"
#include "ui.h"
#include "ui_internal.h"
#include "theme.h"
#include <stdarg.h>

// Rotating scratch for runtime-formatted labels: a Clay_String points into one of
// these until clay_render() consumes it at the end of ui_render_game(). 48 slots
// comfortably covers one frame's labels (HUD + the busiest overlay).
static const char *fmtl(const char *f, ...) {
    static char ring[48][48];
    static int  i = 0;
    i = (i + 1) % 48;
    va_list ap; va_start(ap, f);
    vsnprintf(ring[i], sizeof ring[i], f, ap);
    va_end(ap);
    return ring[i];
}

#define TXT(s, col, sz) CLAY_TEXT(ui_str(s), CLAY_TEXT_CONFIG({ .textColor = (col), .fontSize = (sz) }))

// ---- HUD (right-side panel) ------------------------------------------------
static void hud_player_row(const UiSnapshot *s, int p) {
    int active = (p == s->active);
    CLAY(CLAY_IDI("HudP", p), {
        .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(34) },
                    .padding = { 8, 8, 4, 4 }, .childGap = 10,
                    .childAlignment = { .y = CLAY_ALIGN_Y_CENTER } },
        .backgroundColor = active ? UI_PANEL2 : ui_rgba(0x00000000),
        .border = { .color = UI_ACCENT, .width = CLAY_BORDER_OUTSIDE(active ? 2 : 0) }
    }) {
        CLAY(CLAY_IDI("HudChip", p), {
            .layout = { .sizing = { CLAY_SIZING_FIXED(24), CLAY_SIZING_FIXED(24) } },
            .backgroundColor = s->eliminated[p] ? ui_rgba(0x555555ff) : ui_player_color(p),
            .image = { .imageData = ui_image(ui_img_token(s->tokenTheme, p)) }
        }) {}
        TXT(fmtl("P%d %s", p + 1, ui_token_name(s->tokenTheme, p)),
            s->eliminated[p] ? UI_TEXT_MUTE : UI_TEXT, 16);
        TXT(fmtl("$%d", s->funds[p]), UI_ACCENT, 18);
        if (s->eliminated[p]) TXT("OUT", UI_BAD, 14);
        else if (s->jailed[p] > 0) TXT("JAIL", ui_rgba(0xFFD54Fff), 14);
    }
}

static void hud_build(const UiSnapshot *s) {
    CLAY(CLAY_ID("Hud"), {
        .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
                    .padding = CLAY_PADDING_ALL(14), .childGap = 6,
                    .layoutDirection = CLAY_TOP_TO_BOTTOM },
        .backgroundColor = ui_rgba(0x12202Cd8)
    }) {
        TXT("MONOPOLY PARTY", UI_TEXT, 22);
        CLAY(CLAY_ID("HudGap0"), { .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(6) } } }) {}
        for (int p = 0; p < s->count; ++p) hud_player_row(s, p);

        CLAY(CLAY_ID("HudGap1"), { .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(8) } } }) {}
        TXT(fmtl("On:  %s", s->spaceName), UI_TEXT_DIM, 15);
        if (s->die1 > 0) TXT(fmtl("Dice: %d + %d = %d", s->die1, s->die2, s->die1 + s->die2), UI_TEXT, 15);

        CLAY(CLAY_ID("HudGap2"), { .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(10) } } }) {}
        const char *prompt = "...";
        if (s->gameOver) prompt = "Game over";
        else switch (s->phase) {
            case UI_PHASE_ROLL:    prompt = s->jailTurns > 0 ? "In jail" : "Press X to ROLL"; break;
            case UI_PHASE_TURNEND: prompt = "Press X to end turn"; break;
            case UI_PHASE_BUY:     prompt = "Buy or auction?"; break;
            case UI_PHASE_BIDS:    prompt = "Auction!"; break;
            case UI_PHASE_DEBT:    prompt = "Settle debt"; break;
            case UI_PHASE_TRADE:   prompt = "Trade offer"; break;
        }
        TXT(fmtl("P%d: %s", s->controlling + 1, prompt), UI_ACCENT, 16);
        if (!s->gameOver && (s->phase == UI_PHASE_ROLL || s->phase == UI_PHASE_TURNEND)) {
            CLAY(CLAY_ID("HudGap3"), { .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(6) } } }) {}
            TXT("L2 manage   Tri trade", UI_TEXT_MUTE, 13);
            TXT("Start pause", UI_TEXT_MUTE, 13);
        }
    }
}

// ---- modal shell -----------------------------------------------------------
// Each overlay is a floating, screen-centered panel. Clay macros are scoped, so
// the panel is opened inline in each builder (can't factor open/close into a fn).
#define MODAL_OPEN(idname) \
    CLAY(CLAY_ID(idname), { \
        .floating = { .attachTo = CLAY_ATTACH_TO_ROOT, \
                      .attachPoints = { .element = CLAY_ATTACH_POINT_CENTER_CENTER, \
                                        .parent  = CLAY_ATTACH_POINT_CENTER_CENTER } }, \
        .layout = { .sizing = { CLAY_SIZING_FIXED(380), CLAY_SIZING_FIT(0) }, \
                    .padding = CLAY_PADDING_ALL(20), .childGap = 8, \
                    .layoutDirection = CLAY_TOP_TO_BOTTOM }, \
        .backgroundColor = UI_PANEL, \
        .border = { .color = UI_BORDER, .width = CLAY_BORDER_OUTSIDE(2) } \
    })

static void overlay_build(const UiSnapshot *s) {
    if (s->paused) {
        MODAL_OPEN("MPause") {
            TXT("PAUSED", UI_TEXT, 24);
            TXT("Start = resume", UI_GOOD, 16);
            TXT("Select = quit to menu", UI_GOOD, 16);
        }
        return;
    }
    if (s->gameOver) {
        MODAL_OPEN("MOver") {
            TXT("GAME OVER", UI_TEXT, 26);
            if (s->winner >= 0) TXT(fmtl("PLAYER %d WINS!", s->winner + 1), ui_player_color(s->winner), 24);
        }
        return;
    }
    if (s->mgmtOpen) {
        MODAL_OPEN("MMgmt") {
            TXT(fmtl("MANAGE  P%d   $%d", s->controlling + 1, s->funds[s->controlling]), UI_TEXT, 20);
            if (s->mgmtCount == 0) TXT("(no properties yet)", UI_TEXT_DIM, 15);
            for (int i = 0; i < s->mgmtCount; ++i) {
                int sel = (i == s->mgmtSel);
                CLAY(CLAY_IDI("MgRow", i), {
                    .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(24) },
                                .padding = { 6, 6, 2, 2 }, .childGap = 8, .childAlignment = { .y = CLAY_ALIGN_Y_CENTER } },
                    .backgroundColor = sel ? UI_ROW_FOCUS : ui_rgba(0x00000000)
                }) {
                    TXT(s->mgmtName[i], sel ? UI_TEXT : UI_TEXT_DIM, 15);
                    TXT(s->mgmtTag[i], UI_TEXT_DIM, 14);
                    if (s->mgmtMort[i]) TXT("MORT", UI_TEXT_MUTE, 13);
                }
            }
            CLAY(CLAY_ID("MgGap"), { .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(6) } } }) {}
            TXT("Up/Dn pick  Tri build  Sq sell", UI_GOOD, 13);
            TXT("X mortgage toggle   L2/O close", UI_GOOD, 13);
        }
        return;
    }
    if (s->tradeOpen) {
        MODAL_OPEN("MTrade") {
            TXT("OFFER TRADE", UI_TEXT, 22);
            TXT(fmtl("Give: %s", s->tradeGive), UI_GOOD, 16);
            TXT(fmtl("To:   P%d", s->tradeTarget + 1), ui_player_color(s->tradeTarget), 16);
            TXT(fmtl("Ask:  $%d", s->tradeCash), UI_BAD, 16);
            TXT("Lt/Rt deed  Sq target  Up/Dn $", UI_TEXT_MUTE, 12);
            TXT("X = send    O = cancel", UI_GOOD, 14);
        }
        return;
    }
    switch (s->phase) {
        case UI_PHASE_BUY:
            MODAL_OPEN("MBuy") {
                TXT(fmtl("P%d landed on", s->controlling + 1), UI_TEXT, 15);
                TXT(s->buyName, UI_ACCENT, 22);
                TXT(fmtl("Buy for $%d ?", s->buyPrice), UI_TEXT, 16);
                TXT("X = Buy     O = Auction", UI_GOOD, 15);
            }
            break;
        case UI_PHASE_BIDS:
            MODAL_OPEN("MAuc") {
                TXT(fmtl("AUCTION: %s", s->aucName), UI_ACCENT, 18);
                TXT(fmtl("High bid: $%d", s->aucHighBid), UI_TEXT, 15);
                TXT(fmtl("P%d bid: $%d", s->controlling + 1, s->aucBid), ui_player_color(s->controlling), 18);
                TXT("Up/Dn +-10   Lt/Rt +-50", UI_TEXT_MUTE, 13);
                TXT("X = Bid     O = Pass", UI_GOOD, 15);
            }
            break;
        case UI_PHASE_DEBT:
            MODAL_OPEN("MDebt") {
                TXT(fmtl("P%d IN DEBT", s->controlling + 1), UI_BAD, 20);
                TXT(fmtl("Must raise $%d", s->debtShortfall > 0 ? s->debtShortfall : 0), UI_TEXT, 16);
                TXT("L2 mortgage / sell", UI_GOOD, 14);
                TXT("O = declare bankruptcy", UI_BAD, 13);
            }
            break;
        case UI_PHASE_TRADE:
            MODAL_OPEN("MTradeR") {
                TXT(fmtl("P%d offers you:", s->trOfferer + 1), UI_ACCENT, 18);
                TXT(s->trGive, UI_GOOD, 15);
                TXT("in exchange for:", UI_TEXT, 14);
                TXT(s->trGet, ui_rgba(0xE0B0B0ff), 15);
                TXT("X = Accept   O = Decline", UI_GOOD, 15);
            }
            break;
        case UI_PHASE_ROLL:
            if (s->jailTurns > 0) {
                MODAL_OPEN("MJail") {
                    TXT(fmtl("P%d IN JAIL", s->controlling + 1), UI_ACCENT, 20);
                    TXT(fmtl("Rolls left: %d", s->jailTurns), UI_TEXT, 15);
                    TXT("X = roll for doubles", UI_GOOD, 13);
                    if (s->jailCanBail) TXT("Sq = pay $50 bail", UI_GOOD, 13);
                    if (s->jailHasCard) TXT("Tri = use jail card", UI_GOOD, 13);
                }
            }
            break;
        default: break;
    }
}

// ---- frame -----------------------------------------------------------------
void ui_render_game(const UiSnapshot *s) {
    Clay_SetLayoutDimensions((Clay_Dimensions){ 848, 512 });
    Clay_BeginLayout();
    CLAY(CLAY_ID("GameRoot"), {
        .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
                    .layoutDirection = CLAY_LEFT_TO_RIGHT }
    }) {
        // left: transparent board area (raw board drawn underneath this frame)
        CLAY(CLAY_ID("BoardArea"), { .layout = { .sizing = { CLAY_SIZING_FIXED(492), CLAY_SIZING_GROW(0) } } }) {}
        hud_build(s);
        overlay_build(s);   // floating modal on top, if any
    }
    clay_render(Clay_EndLayout(0.0f));
}
