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

// ---- HUD: player cards float over the board corners (see the PS Monopoly look) --
// The board is now drawn full-screen underneath; the HUD is four corner cards, a
// top banner and a bottom action bar, all floating on top with alpha blending.

// Which screen corner a player's card anchors to (0 TL, 1 TR, 2 BL, 3 BR) and the
// inset offset from that corner.
static void corner_attach(int p, Clay_FloatingAttachPointType *pt, float *ox, float *oy) {
    const float PAD = 10.0f;
    switch (p) {
        case 0:  *pt = CLAY_ATTACH_POINT_LEFT_TOP;     *ox =  PAD; *oy =  PAD; break;
        case 1:  *pt = CLAY_ATTACH_POINT_RIGHT_TOP;    *ox = -PAD; *oy =  PAD; break;
        case 2:  *pt = CLAY_ATTACH_POINT_LEFT_BOTTOM;  *ox =  PAD; *oy = -PAD; break;
        default: *pt = CLAY_ATTACH_POINT_RIGHT_BOTTOM; *ox = -PAD; *oy = -PAD; break;
    }
}

// A small pill (chip). idx keeps sibling ids unique within a card.
static void chip(int idx, const char *label, Clay_Color bg, Clay_Color fg) {
    CLAY(CLAY_IDI("Chip", idx), {
        .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(20) },
                    .padding = { 6, 6, 2, 2 },
                    .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER } },
        .backgroundColor = bg
    }) {
        TXT(label, fg, 13);
    }
}

// Action pills for the player the game is currently waiting on (the "actor").
static void actor_chips(const UiSnapshot *s) {
    const char *primary = 0;
    switch (s->phase) {
        case UI_PHASE_ROLL:    primary = s->jailTurns > 0 ? "X: Jail roll" : "X: Roll"; break;
        case UI_PHASE_TURNEND: primary = "X: End turn"; break;
        case UI_PHASE_BUY:     primary = "X Buy / O Auc"; break;
        case UI_PHASE_BIDS:    primary = "X Bid / O Pass"; break;
        case UI_PHASE_DEBT:    primary = "L2: raise cash"; break;
        case UI_PHASE_TRADE:   primary = "X/O respond"; break;
        default: break;
    }
    if (primary) chip(0, primary, UI_ROW_FOCUS, UI_TEXT);
    if (s->phase == UI_PHASE_ROLL || s->phase == UI_PHASE_TURNEND) {
        chip(1, "L2  Manage", UI_ROW, UI_TEXT_DIM);
        chip(2, "Tri  Trade", UI_ROW, UI_TEXT_DIM);
    }
}

static void player_card(const UiSnapshot *s, int p) {
    int actor = (p == s->controlling);
    Clay_FloatingAttachPointType pt; float ox, oy;
    corner_attach(p, &pt, &ox, &oy);
    Clay_Color frame = actor ? UI_ACCENT : UI_BORDER;
    CLAY(CLAY_IDI("Card", p), {
        .floating = { .attachTo = CLAY_ATTACH_TO_ROOT,
                      .attachPoints = { .element = pt, .parent = pt },
                      .offset = { ox, oy } },
        .layout = { .sizing = { CLAY_SIZING_FIXED(170), CLAY_SIZING_FIT(0) },
                    .padding = CLAY_PADDING_ALL(8), .childGap = 6,
                    .layoutDirection = CLAY_TOP_TO_BOTTOM },
        .backgroundColor = ui_rgba(0x14232Fe6),
        .border = { .color = frame, .width = CLAY_BORDER_OUTSIDE(actor ? 3 : 1) }
    }) {
        // header: token portrait + name / cash
        CLAY(CLAY_IDI("CardHdr", p), {
            .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
                        .childGap = 8, .childAlignment = { .y = CLAY_ALIGN_Y_CENTER } }
        }) {
            CLAY(CLAY_IDI("CardPic", p), {
                .layout = { .sizing = { CLAY_SIZING_FIXED(46), CLAY_SIZING_FIXED(46) } },
                .backgroundColor = s->eliminated[p] ? ui_rgba(0x555555ff) : ui_player_color(p),
                .border = { .color = UI_BORDER, .width = CLAY_BORDER_OUTSIDE(1) },
                .image = { .imageData = ui_image(ui_img_token(s->tokenTheme, p)) }
            }) {}
            CLAY(CLAY_IDI("CardTxt", p), {
                .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .childGap = 2 }
            }) {
                TXT(fmtl("PLAYER %d", p + 1), s->eliminated[p] ? UI_TEXT_MUTE : UI_TEXT, 15);
                TXT(fmtl("$%d", s->funds[p]), UI_BAD, 22);   // red cash, PS-Monopoly style
            }
        }
        if (s->eliminated[p])         TXT("BANKRUPT", UI_TEXT_MUTE, 13);
        else if (s->jailed[p] > 0)    TXT(fmtl("IN JAIL (%d)", s->jailed[p]), ui_rgba(0xFFD54Fff), 13);
        if (actor && !s->gameOver && !s->eliminated[p]) actor_chips(s);
    }
}

// Center-top banner: whose turn + the space they're on + the dice.
static void top_banner(const UiSnapshot *s) {
    CLAY(CLAY_ID("TopBanner"), {
        .floating = { .attachTo = CLAY_ATTACH_TO_ROOT,
                      .attachPoints = { .element = CLAY_ATTACH_POINT_CENTER_TOP,
                                        .parent  = CLAY_ATTACH_POINT_CENTER_TOP },
                      .offset = { 0, 6 } },
        .layout = { .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIXED(30) },
                    .padding = { 14, 14, 4, 4 }, .childGap = 10,
                    .childAlignment = { .y = CLAY_ALIGN_Y_CENTER } },
        .backgroundColor = ui_rgba(0x14232Fe6),
        .border = { .color = UI_ACCENT, .width = CLAY_BORDER_OUTSIDE(1) }
    }) {
        TXT(fmtl("P%d", s->active + 1), ui_player_color(s->active), 16);
        TXT(s->spaceName, UI_TEXT, 16);
        if (s->die1 > 0) {
            void *d1 = ui_image(ui_img_dice(s->die1));
            void *d2 = ui_image(ui_img_dice(s->die2));
            if (d1 && d2) {
                CLAY(CLAY_ID("BDie1"), { .layout = { .sizing = { CLAY_SIZING_FIXED(22), CLAY_SIZING_FIXED(22) } },
                                         .image = { .imageData = d1 } }) {}
                CLAY(CLAY_ID("BDie2"), { .layout = { .sizing = { CLAY_SIZING_FIXED(22), CLAY_SIZING_FIXED(22) } },
                                         .image = { .imageData = d2 } }) {}
            } else {
                TXT(fmtl("%d+%d", s->die1, s->die2), UI_ACCENT, 16);
            }
        }
    }
}

// Center-bottom action bar: the prompt for the current actor.
static void bottom_bar(const UiSnapshot *s) {
    const char *msg = "...";
    if (s->gameOver) msg = "Game over";
    else switch (s->phase) {
        case UI_PHASE_ROLL:    msg = s->jailTurns > 0 ? "In jail" : "Press X to roll"; break;
        case UI_PHASE_TURNEND: msg = "Press X to end turn"; break;
        case UI_PHASE_BUY:     msg = "Buy or auction?"; break;
        case UI_PHASE_BIDS:    msg = "Auction!"; break;
        case UI_PHASE_DEBT:    msg = "Settle the debt"; break;
        case UI_PHASE_TRADE:   msg = "Respond to trade"; break;
        default: break;
    }
    CLAY(CLAY_ID("BottomBar"), {
        .floating = { .attachTo = CLAY_ATTACH_TO_ROOT,
                      .attachPoints = { .element = CLAY_ATTACH_POINT_CENTER_BOTTOM,
                                        .parent  = CLAY_ATTACH_POINT_CENTER_BOTTOM },
                      .offset = { 0, -6 } },
        .layout = { .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIXED(30) },
                    .padding = { 16, 16, 4, 4 }, .childGap = 14,
                    .childAlignment = { .y = CLAY_ALIGN_Y_CENTER } },
        .backgroundColor = ui_rgba(0x14232Fe6),
        .border = { .color = UI_BORDER, .width = CLAY_BORDER_OUTSIDE(1) }
    }) {
        TXT(fmtl("P%d", s->controlling + 1), ui_player_color(s->controlling), 16);
        TXT(msg, UI_ACCENT, 16);
        TXT("Start: pause", UI_TEXT_MUTE, 12);
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
        .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) } }
    }) {
        // The full-screen board is drawn raw underneath; everything here floats on top.
        top_banner(s);
        for (int p = 0; p < s->count; ++p) player_card(s, p);
        bottom_bar(s);
        overlay_build(s);   // centered modal on top, if any
    }
    clay_render(Clay_EndLayout(0.0f));
}
