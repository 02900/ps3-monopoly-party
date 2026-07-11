/*
 * ps3-monopoly-party — M1: board + roll loop (tiny3d front end)
 *
 * Renders the reused monopoly engine's GameState with Tiny3D: the 40-space board
 * (property squares tinted by colour group), a token per player, and a HUD with
 * everyone's cash, whose turn it is, the last dice roll and a prompt. Hot-seat:
 * players share the pad and pass it each turn.
 *
 * Controls (M1): Cross = roll the dice / end turn (context) · Start = quit.
 * Buying, auctions, building, jail, trading arrive in later milestones; for now
 * unowned-property decisions auto-decline so turns keep flowing.
 */
#include <ppu-types.h>
#include <io/pad.h>
#include <sysutil/sysutil.h>
#include <lv2/systime.h>
#include <tiny3d.h>          // has its own extern "C" guard
#include "audio.h"           // has its own extern "C" guard

// ttf_render.h has no C-linkage guard of its own; wrap it. tiny3d.h (its only
// include) is already pulled in above, so the guarded re-include is a no-op and
// no system headers get dragged into this extern "C" block.
extern "C" {
#include "ttf_render.h"
void ya2d_init(void);          // ya2d.h lacks a C-linkage guard; declare what we use
}

#include "ui/ui.h"             // plain-C bridge to the Clay UI (has its own extern "C")

#include "PS3Interface.h"
#include "engine/Game.h"
#include "engine/GameState.h"
#include "engine/Board.h"
#include "engine/DisplayStrings.h"   // to_string(Property/Space/…)

#ifdef NETTEST
#include "nettest/nettest.h"
#include "nettest/commands.h"
#endif

#include <algorithm>
#include <cstring>
#include <set>
#include <string>
#include <vector>

using namespace monopoly;

// ---- 2D canvas + board geometry -------------------------------------------
static const int   SCREEN_W = 848;
static const int   SCREEN_H = 512;
static const u32   CLEAR     = 0xff0E1A2E;   // 0xAARRGGBB
static const int   BOARD_X0  = 24;
static const int   BOARD_Y0  = 26;
static const int   CELL      = 42;           // 11x11 grid -> 462px board
static const u32   BOARD_TAN = 0xffCFE0C3;
static const u32   INK       = 0xff10202E;

// Player token colours (red / green / blue / yellow).
static const u32 PLAYER_COL[4] = { 0xffE53935, 0xff43A047, 0xff1E88E5, 0xffFDD835 };

// RSX texture area for the TTF glyph cache (MAX_CHARS * 32*32 * u16).
static u32 *g_ttf_texture = 0;

static int g_running = 1;
static void sys_cb(u64 status, u64 param, void *ud) {
    (void)param; (void)ud;
    if (status == SYSUTIL_EXIT_GAME) g_running = 0;
}

// index (0..39) -> grid cell (col,row); GO at bottom-right, going counter-clockwise.
static void cell_of(int i, int *col, int *row) {
    if      (i == 0)  { *col = 10; *row = 10; }
    else if (i < 10)  { *row = 10; *col = 10 - i; }
    else if (i == 10) { *col = 0;  *row = 10; }
    else if (i < 20)  { *col = 0;  *row = 10 - (i - 10); }
    else if (i == 20) { *col = 0;  *row = 0; }
    else if (i < 30)  { *row = 0;  *col = i - 20; }
    else if (i == 30) { *col = 10; *row = 0; }
    else              { *col = 10; *row = i - 30; }
}

static u32 group_color(PropertyGroup g) {
    switch (g) {
        case PropertyGroup::Brown:     return 0xff8D5524;
        case PropertyGroup::LightBlue: return 0xffAED6F1;
        case PropertyGroup::Magenta:   return 0xffD65DB1;
        case PropertyGroup::Orange:    return 0xffE67E22;
        case PropertyGroup::Red:       return 0xffC0392B;
        case PropertyGroup::Yellow:    return 0xffF1C40F;
        case PropertyGroup::Green:     return 0xff27AE60;
        case PropertyGroup::Blue:      return 0xff2E5FA3;
        case PropertyGroup::Utility:   return 0xffBDC3C7;
        case PropertyGroup::Railroad:  return 0xff566573;
        default:                       return BOARD_TAN;
    }
}

// NOTE: tiny3d_VertexColor and display_ttf_string take colours as RGBA
// (0xRRGGBBAA) — unlike tiny3d_Clear, which is ARGB. Our colour constants are
// written as opaque ARGB (0xffRRGGBB) for readability; argb_to_rgba() reorders
// them at the point of use so both formats coexist.
static inline u32 argb_to_rgba(u32 argb) { return (argb << 8) | 0xff; }

static void fill_rect(int x, int y, int w, int h, u32 color) {
    u32 c = argb_to_rgba(color);
    tiny3d_SetPolygon(TINY3D_QUADS);
    tiny3d_VertexPos(x,     y,     0); tiny3d_VertexColor(c);
    tiny3d_VertexPos(x + w, y,     0); tiny3d_VertexColor(c);
    tiny3d_VertexPos(x + w, y + h, 0); tiny3d_VertexColor(c);
    tiny3d_VertexPos(x,     y + h, 0); tiny3d_VertexColor(c);
    tiny3d_End();
}

static void text(int x, int y, const char *s, u32 color, int sw, int sh) {
    display_ttf_string(x, y, s, argb_to_rgba(color), 0, sw, sh);
}

// ---- rendering -------------------------------------------------------------
static void draw_board(const GameState &s, int playerCount) {
    // squares
    for (int i = 0; i < 40; ++i) {
        int c, r; cell_of(i, &c, &r);
        int x = BOARD_X0 + c * CELL;
        int y = BOARD_Y0 + r * CELL;
        Space sp = index_to_space(i);
        u32 col = BOARD_TAN;
        if (space_is_property(sp))
            col = group_color(property_group(space_to_property(sp)));
        fill_rect(x + 1, y + 1, CELL - 2, CELL - 2, col);

        // owner marker (corner square in the owner's colour; dimmed if mortgaged)
        if (space_is_property(sp)) {
            Property prop = space_to_property(sp);
            int owner = s.get_property_owner_index(prop);
            if (owner >= 0) {
                fill_rect(x + 3, y + 3, 10, 10, INK);
                fill_rect(x + 4, y + 4, 8, 8,
                          s.get_property_is_mortgaged(prop) ? 0xff707070 : PLAYER_COL[owner]);
            }
        }
    }

    // tokens (2x2 cluster inside the owner's square)
    for (int p = 0; p < playerCount; ++p) {
        int i = space_to_index(s.get_player_position(p));
        int c, r; cell_of(i, &c, &r);
        int x = BOARD_X0 + c * CELL + 6 + (p % 2) * 16;
        int y = BOARD_Y0 + r * CELL + 6 + (p / 2) * 16;
        fill_rect(x - 1, y - 1, 15, 15, INK);              // outline
        fill_rect(x, y, 13, 13, PLAYER_COL[p]);
    }
}

static void draw_hud(const GameState &s, int playerCount) {
    const int HX = BOARD_X0 + 11 * CELL + 18;   // right of the board, in the free area
    int y = 40;

    text(HX, y, "MONOPOLY PARTY", 0xffFFFFFF, 15, 22); y += 40;

    int active = s.get_active_player_index();
    for (int p = 0; p < playerCount; ++p) {
        char line[80];
        const char *tag = s.get_player_eliminated(p) ? " OUT"
                        : s.get_player_turns_remaining_in_jail(p) > 0 ? " JAIL" : "";
        std::snprintf(line, sizeof line, "%s P%d   $%d%s",
                      p == active ? ">" : " ", p + 1, s.get_player_funds(p), tag);
        text(HX, y, line, s.get_player_eliminated(p) ? 0xff777777 : PLAYER_COL[p], 13, 20);
        y += 26;
    }
    y += 6;

    char onl[48];
    std::snprintf(onl, sizeof onl, "On: %s", to_string(s.get_player_position(active)).c_str());
    text(HX, y, onl, 0xffB0BEC5, 12, 18); y += 26;

    std::pair<int,int> d = s.get_last_dice_roll();
    if (d.first > 0) {
        char dl[48];
        std::snprintf(dl, sizeof dl, "Dice:  %d + %d = %d", d.first, d.second, d.first + d.second);
        text(HX, y, dl, 0xffFFFFFF, 13, 20);
    }
    y += 34;

    // prompt
    const char *prompt = "";
    if (s.is_game_over())                                         prompt = "GAME OVER";
    else switch (s.get_turn_phase()) {
        case TurnPhase::WaitingForRoll:             prompt = "Press X to ROLL";     break;
        case TurnPhase::WaitingForTurnEnd:          prompt = "Press X to end turn"; break;
        case TurnPhase::WaitingForBuyPropertyInput: prompt = "Buy or auction?";     break;
        case TurnPhase::WaitingForBids:             prompt = "Auction!";            break;
        default:                                    prompt = "...";                 break;
    }
    char pl[80];
    std::snprintf(pl, sizeof pl, "P%d: %s", s.get_controlling_player_index() + 1, prompt);
    text(HX, y, pl, 0xffFFE082, 12, 18); y += 24;

    TurnPhase ph = s.get_turn_phase();
    if (!s.is_game_over() &&
        (ph == TurnPhase::WaitingForRoll || ph == TurnPhase::WaitingForTurnEnd)) {
        text(HX, y, "L2 = manage   Tri = trade", 0xff8FB6C9, 11, 16); y += 20;
        text(HX, y, "Start = pause", 0xff8FB6C9, 11, 16);
    }
}

// ---- interactive overlays (buy / auction) ---------------------------------
// A centred modal in the (now clear) board centre. Split into background quads
// and foreground text so all quads precede all text in the frame.
static const int OV_X = BOARD_X0 + CELL + 6;
static const int OV_Y = BOARD_Y0 + 3 * CELL;
static const int OV_W = CELL * 9 - 12;
static const int OV_H = CELL * 4;

// The controlling player is taking a turn from inside jail.
static bool jailed_turn(const GameState &s) {
    return s.get_turn_phase() == TurnPhase::WaitingForRoll &&
           s.get_player_turns_remaining_in_jail(s.get_controlling_player_index()) > 0;
}

static bool overlay_active(const GameState &s) {
    TurnPhase ph = s.get_turn_phase();
    return s.is_game_over() ||
           ph == TurnPhase::WaitingForBuyPropertyInput ||
           ph == TurnPhase::WaitingForBids ||
           ph == TurnPhase::WaitingForDebtSettlement ||
           ph == TurnPhase::WaitingForTradeOfferResponse ||
           jailed_turn(s);
}

// Compact one-line summary of a trade side (what a Promise contains).
static void promise_summary(const monopoly::Promise &pr, char *out, int cap) {
    int n = 0;
    if (pr.cash > 0) n += std::snprintf(out + n, cap - n, "$%d ", pr.cash);
    for (Property d : pr.deeds) {
        if (n >= cap - 1) break;
        n += std::snprintf(out + n, cap - n, "%s ", to_string(d).c_str());
    }
    if (n == 0) std::snprintf(out, cap, "nothing");
}

static void draw_overlay_bg(const GameState &s) {
    if (!overlay_active(s)) return;
    fill_rect(OV_X - 2, OV_Y - 2, OV_W + 4, OV_H + 4, 0xffF5F0E1);   // border
    fill_rect(OV_X,     OV_Y,     OV_W,     OV_H,     0xff1B2A38);   // panel
}

static void draw_overlay_fg(const GameState &s, int bidAmount) {
    const int tx = OV_X + 16;
    int ty = OV_Y + 14;
    const int c = s.get_controlling_player_index();
    char l[80];

    if (s.is_game_over()) {
        int w = -1;
        for (int i = 0; i < s.get_player_count(); ++i)
            if (!s.get_player_eliminated(i)) { w = i; break; }
        text(tx, ty, "GAME OVER", 0xffFFFFFF, 16, 26); ty += 40;
        if (w >= 0) {
            std::snprintf(l, sizeof l, "PLAYER %d WINS!", w + 1);
            text(tx, ty, l, PLAYER_COL[w], 16, 26);
        }
        return;
    }

    if (s.get_turn_phase() == TurnPhase::WaitingForDebtSettlement) {
        int deficit = -s.get_player_funds(c);   // funds are negative while settling
        std::snprintf(l, sizeof l, "P%d IN DEBT", c + 1);
        text(tx, ty, l, 0xffE57373, 15, 24); ty += 32;
        std::snprintf(l, sizeof l, "Must raise $%d", deficit > 0 ? deficit : 0);
        text(tx, ty, l, 0xffFFFFFF, 14, 22); ty += 32;
        text(tx, ty, "L2 mortgage/sell", 0xff9FE6A0, 13, 20); ty += 26;
        text(tx, ty, "O = declare bankruptcy", 0xffE57373, 12, 18);
        return;
    }

    if (s.get_turn_phase() == TurnPhase::WaitingForTradeOfferResponse) {
        Trade tr = s.get_pending_trade_offer();
        char give[64], get[64];
        promise_summary(tr.offer, give, sizeof give);          // what the offerer gives you
        promise_summary(tr.consideration, get, sizeof get);    // what they want from you
        std::snprintf(l, sizeof l, "P%d offers you:", tr.offeringPlayer + 1);
        text(tx, ty, l, 0xffFFE082, 14, 22); ty += 30;
        text(tx, ty, give, 0xff9FE6A0, 13, 20); ty += 28;
        text(tx, ty, "in exchange for:", 0xffFFFFFF, 13, 20); ty += 26;
        text(tx, ty, get, 0xffE0B0B0, 13, 20); ty += 30;
        text(tx, ty, "X = Accept   O = Decline", 0xff9FE6A0, 13, 20);
        return;
    }

    if (jailed_turn(s)) {
        int turns = s.get_player_turns_remaining_in_jail(c);
        bool canBail = s.check_if_player_is_allowed_to_pay_bail(c);
        bool hasCard = !s.get_player_get_out_of_jail_free_cards(c).empty();
        std::snprintf(l, sizeof l, "P%d IN JAIL", c + 1);
        text(tx, ty, l, 0xffFFE082, 15, 24); ty += 30;
        std::snprintf(l, sizeof l, "Rolls left: %d", turns);
        text(tx, ty, l, 0xffFFFFFF, 13, 20); ty += 28;
        text(tx, ty, "X = roll for doubles", 0xff9FE6A0, 12, 18); ty += 22;
        if (canBail) { text(tx, ty, "Sq = pay $50 bail", 0xff9FE6A0, 12, 18); ty += 22; }
        if (hasCard) { text(tx, ty, "Tri = use jail card", 0xff9FE6A0, 12, 18); }
        return;
    }

    if (s.get_turn_phase() == TurnPhase::WaitingForBuyPropertyInput) {
        Property prop = space_to_property(s.get_player_position(s.get_active_player_index()));
        std::snprintf(l, sizeof l, "P%d landed on", c + 1);
        text(tx, ty, l, 0xffFFFFFF, 13, 20); ty += 30;
        text(tx, ty, to_string(prop).c_str(), 0xffFFE082, 15, 24); ty += 34;
        if (prop != Property::Invalid) {   // guard: price table is undefined for non-properties
            std::snprintf(l, sizeof l, "Buy for $%d ?", price_of_property(prop));
            text(tx, ty, l, 0xffFFFFFF, 14, 22);
        }
        ty += 34;
        text(tx, ty, "X = Buy     O = Auction", 0xff9FE6A0, 13, 20);
    } else if (s.get_turn_phase() == TurnPhase::WaitingForBids) {
        Auction a = s.get_current_auction();
        Property ap = a.property;
        std::snprintf(l, sizeof l, "AUCTION: %s", monopoly::to_string(ap).c_str());
        text(tx, ty, l, 0xffFFE082, 14, 22); ty += 30;
        std::snprintf(l, sizeof l, "High bid: $%d", a.highestBid);
        text(tx, ty, l, 0xffFFFFFF, 13, 20); ty += 28;
        std::snprintf(l, sizeof l, "P%d bid: $%d", c + 1, bidAmount);
        text(tx, ty, l, PLAYER_COL[c], 14, 22); ty += 30;
        text(tx, ty, "Up/Dn +-10   Lt/Rt +-50", 0xffCCCCCC, 12, 18); ty += 24;
        text(tx, ty, "X = Bid     O = Pass", 0xff9FE6A0, 13, 20);
    }
}

// ---- property management window (L2) --------------------------------------
static const int MG_X = BOARD_X0 + CELL;
static const int MG_Y = BOARD_Y0 + CELL;
static const int MG_W = CELL * 9;
static const int MG_H = CELL * 9;

// The controlling player's deeds, ordered by board position for stable navigation.
static std::vector<Property> deeds_of(const GameState &s, int p) {
    std::set<Property> d = s.get_player_deeds(p);
    std::vector<Property> v(d.begin(), d.end());
    std::sort(v.begin(), v.end(), [](Property a, Property b) {
        return space_to_index(property_to_space(a)) < space_to_index(property_to_space(b));
    });
    return v;
}

// Short buildings tag for a deed: "-", "H".."HHHH", "HOT", or "RR"/"UT".
static void buildings_tag(const GameState &s, Property p, char *out) {
    PropertyGroup g = property_group(p);
    if (g == PropertyGroup::Railroad) { std::strcpy(out, "RR"); return; }
    if (g == PropertyGroup::Utility)  { std::strcpy(out, "UT"); return; }
    int lvl = s.get_building_level(p);
    if (lvl == 0)          { std::strcpy(out, "-"); return; }
    if (lvl == HotelLevel) { std::strcpy(out, "HOT"); return; }
    int k = 0; for (; k < lvl && k < 4; ++k) out[k] = 'H';
    out[k] = 0;
}

static void draw_mgmt_bg() {
    fill_rect(MG_X - 2, MG_Y - 2, MG_W + 4, MG_H + 4, 0xffF5F0E1);
    fill_rect(MG_X,     MG_Y,     MG_W,     MG_H,     0xff14202B);
}

static void draw_mgmt_fg(const GameState &s, int sel) {
    const int c = s.get_controlling_player_index();
    std::vector<Property> deeds = deeds_of(s, c);
    const int tx = MG_X + 12;
    int ty = MG_Y + 12;
    char l[96];

    std::snprintf(l, sizeof l, "MANAGE  P%d   $%d", c + 1, s.get_player_funds(c));
    text(tx, ty, l, 0xffFFFFFF, 14, 22); ty += 32;

    int n = (int) deeds.size();
    if (n == 0) {
        text(tx, ty, "(no properties yet)", 0xffCCCCCC, 13, 20);
    } else {
        if (sel < 0) sel = 0;
        if (sel >= n) sel = n - 1;
        int first = sel - 3; if (first < 0) first = 0;
        int last = first + 7; if (last > n - 1) last = n - 1;
        for (int i = first; i <= last; ++i) {
            Property p = deeds[i];
            char bs[8]; buildings_tag(s, p, bs);
            std::snprintf(l, sizeof l, "%s %-15s %-4s %s",
                          i == sel ? ">" : " ", to_string(p).c_str(), bs,
                          s.get_property_is_mortgaged(p) ? "MORT" : "");
            text(tx, ty, l, i == sel ? 0xffFFE082 : 0xffDDDDDD, 12, 18);
            ty += 22;
        }
    }

    ty = MG_Y + MG_H - 46;
    text(tx, ty, "Up/Dn pick  Tri Build  Sq Sell", 0xff9FE6A0, 11, 16); ty += 20;
    text(tx, ty, "X Mortgage toggle   L2/O Close", 0xff9FE6A0, 11, 16);
}

// ---- trade builder (Triangle) ---------------------------------------------
// Offer one of your deeds to another player in exchange for cash.
static int next_other_player(const GameState &s, int from, int self) {
    int n = s.get_player_count();
    for (int k = 1; k <= n; ++k) {
        int cand = (from + k) % n;
        if (cand != self && !s.get_player_eliminated(cand)) return cand;
    }
    return self;
}

static void draw_trade_bg() { draw_mgmt_bg(); }   // same panel

static void draw_trade_fg(const GameState &s, int me, int target, int deedSel, int cash) {
    std::vector<Property> deeds = deeds_of(s, me);
    const int tx = MG_X + 12;
    int ty = MG_Y + 14;
    char l[80];

    text(tx, ty, "OFFER TRADE", 0xffFFFFFF, 15, 24); ty += 36;
    if (deeds.empty()) {
        text(tx, ty, "(you have no deeds to offer)", 0xffCCCCCC, 13, 20);
        return;
    }
    if (deedSel < 0) deedSel = 0;
    if (deedSel >= (int) deeds.size()) deedSel = (int) deeds.size() - 1;
    std::snprintf(l, sizeof l, "Give: %s", to_string(deeds[deedSel]).c_str());
    text(tx, ty, l, 0xff9FE6A0, 14, 22); ty += 24;
    text(tx, ty, "   (Lt/Rt)", 0xff888888, 11, 16); ty += 30;
    std::snprintf(l, sizeof l, "To:  P%d", target + 1);
    text(tx, ty, l, PLAYER_COL[target], 14, 22); ty += 24;
    text(tx, ty, "   (Square)", 0xff888888, 11, 16); ty += 30;
    std::snprintf(l, sizeof l, "Ask: $%d", cash);
    text(tx, ty, l, 0xffE0B0B0, 14, 22); ty += 24;
    text(tx, ty, "   (Up/Dn +-50)", 0xff888888, 11, 16); ty += 34;
    text(tx, ty, "X = Send    O = Cancel", 0xff9FE6A0, 13, 20);
}

// ---- GameState -> UiSnapshot (the plain-C bridge to the Clay in-game UI) ----
static int to_ui_phase(TurnPhase p) {
    switch (p) {
        case TurnPhase::WaitingForRoll:                  return UI_PHASE_ROLL;
        case TurnPhase::WaitingForBuyPropertyInput:      return UI_PHASE_BUY;
        case TurnPhase::WaitingForBids:                  return UI_PHASE_BIDS;
        case TurnPhase::WaitingForAcquisitionManagement: return UI_PHASE_MANAGE;
        case TurnPhase::WaitingForTurnEnd:               return UI_PHASE_TURNEND;
        case TurnPhase::WaitingForDebtSettlement:        return UI_PHASE_DEBT;
        case TurnPhase::WaitingForTradeOfferResponse:    return UI_PHASE_TRADE;
        case TurnPhase::GameOver:                        return UI_PHASE_GAMEOVER;
    }
    return UI_PHASE_ROLL;
}

static void fill_snapshot(UiSnapshot &u, const GameState &s, int count,
                          int mgmtOpen, int mgmtSel, int tradeOpen, int tradeTarget,
                          int tradeDeedSel, int tradeCash, int bidAmount, int paused) {
    std::memset(&u, 0, sizeof u);
    u.count       = count;
    u.active      = s.get_active_player_index();
    u.controlling = s.get_controlling_player_index();
    u.phase       = to_ui_phase(s.get_turn_phase());
    u.gameOver    = s.is_game_over() ? 1 : 0;
    u.winner      = -1;
    if (u.gameOver)
        for (int i = 0; i < count; ++i) if (!s.get_player_eliminated(i)) { u.winner = i; break; }

    for (int p = 0; p < count; ++p) {
        u.funds[p]      = s.get_player_funds(p);
        u.netWorth[p]   = s.get_net_worth(p);
        u.jailed[p]     = s.get_player_turns_remaining_in_jail(p);
        u.eliminated[p] = s.get_player_eliminated(p) ? 1 : 0;
    }
    std::pair<int,int> d = s.get_last_dice_roll();
    u.die1 = d.first; u.die2 = d.second;
    std::snprintf(u.spaceName, sizeof u.spaceName, "%s",
                  to_string(s.get_player_position(u.active)).c_str());

    const int c = u.controlling;
    if (u.phase == UI_PHASE_BUY) {
        Property prop = space_to_property(s.get_player_position(u.active));
        std::snprintf(u.buyName, sizeof u.buyName, "%s", to_string(prop).c_str());
        u.buyPrice = (prop != Property::Invalid) ? price_of_property(prop) : 0;
    }
    if (u.phase == UI_PHASE_BIDS) {
        Auction a = s.get_current_auction();
        Property ap = a.property;
        std::snprintf(u.aucName, sizeof u.aucName, "%s", to_string(ap).c_str());
        u.aucHighBid = a.highestBid;
        u.aucBid     = bidAmount;
        u.aucBidder  = a.biddingOrder.empty() ? -1 : a.biddingOrder.front();
    }
    if (u.phase == UI_PHASE_DEBT) u.debtShortfall = -s.get_player_funds(c);
    if (u.phase == UI_PHASE_TRADE) {
        Trade tr = s.get_pending_trade_offer();
        u.trOfferer = tr.offeringPlayer;
        promise_summary(tr.offer, u.trGive, sizeof u.trGive);
        promise_summary(tr.consideration, u.trGet, sizeof u.trGet);
    }
    if (u.phase == UI_PHASE_ROLL) {
        u.jailTurns   = s.get_player_turns_remaining_in_jail(c);
        u.jailCanBail = s.check_if_player_is_allowed_to_pay_bail(c) ? 1 : 0;
        u.jailHasCard = !s.get_player_get_out_of_jail_free_cards(c).empty() ? 1 : 0;
    }
    u.mgmtOpen = mgmtOpen;
    if (mgmtOpen) {
        std::vector<Property> dd = deeds_of(s, c);
        int n = (int) dd.size(); if (n > UI_MAX_DEEDS) n = UI_MAX_DEEDS;
        u.mgmtCount = n; u.mgmtSel = mgmtSel;
        for (int i = 0; i < n; ++i) {
            std::snprintf(u.mgmtName[i], sizeof u.mgmtName[i], "%s", to_string(dd[i]).c_str());
            buildings_tag(s, dd[i], u.mgmtTag[i]);
            u.mgmtMort[i] = s.get_property_is_mortgaged(dd[i]) ? 1 : 0;
        }
    }
    u.tradeOpen = tradeOpen;
    if (tradeOpen) {
        u.tradeTarget = tradeTarget; u.tradeCash = tradeCash;
        std::vector<Property> dd = deeds_of(s, c);
        if (!dd.empty()) {
            int i = tradeDeedSel; if (i < 0) i = 0; if (i >= (int) dd.size()) i = (int) dd.size() - 1;
            std::snprintf(u.tradeGive, sizeof u.tradeGive, "%s", to_string(dd[i]).c_str());
        }
    }
    u.paused = paused;
}

// ---- main ------------------------------------------------------------------
static padInfo  pad_info;
static padData  pad_data;

int main(void) {
    sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, sys_cb, NULL);

    tiny3d_Init(1024 * 1024);
    ya2d_init();                        // clay-ps3's renderer draws via ya2d
    audio_init();
    ioPadInit(7);

    // TTF glyph cache: reserve RSX texture memory, then load the PS3 system font.
    g_ttf_texture = (u32 *) tiny3d_AllocTexture(1600 * 32 * 32 * 2 + 4096);
    init_ttf_table((u16 *) g_ttf_texture);
    TTFLoadFont(0, (char *)"/dev_flash/data/font/SCE-PS3-VR-R-LATIN2.TTF", NULL, 0);

    ui_init(SCREEN_W, SCREEN_H);        // Clay arena + text-measure callback

    // Release builds seed the dice RNG from the clock so each session differs; the
    // NETTEST build keeps a fixed seed so the e2e suite stays deterministic.
#ifdef NETTEST
    std::string seed = "ps3-monopoly";
#else
    // sysGetSystemTime() = microseconds since boot; boot-timing jitter varies it enough
    // between sessions to reseed the dice RNG.
    char seedbuf[40];
    std::snprintf(seedbuf, sizeof seedbuf, "ps3-%lld", (long long) sysGetSystemTime());
    std::string seed = seedbuf;
#endif
    // Heap-held so a "New Game" from the menu can rebuild the match with the chosen
    // player count (the engine captures playerCount at Game construction). The loop
    // aliases them as references, so the game-body code below stays unchanged.
    mp::PS3Interface *ifacep = new mp::PS3Interface(4, seed);
    Game *gamep = new Game(ifacep);
    gamep->process();                   // settle to the first WaitingForRoll

#ifdef NETTEST
    nettest::Start();                   // opt-in TCP command server (port 9010)
#endif

    int prev[11] = {0};            // X, O, Up, Dn, Lt, Rt, L2, Triangle, Square, Start, Select
    int bidAmount = 0;             // current bidder's entry during an auction
    int mgmtOpen  = 0;             // property-management (L2) window open?
    int mgmtSel   = 0;             // selected deed row in that window
    int paused    = 0;             // pause menu open?
    int tradeOpen = 0;             // trade builder (Triangle) open?
    int tradeTarget = 1;           // player we're offering to
    int tradeDeedSel = 0;          // which of our deeds we offer
    int tradeCash = 100;           // cash we request in return

    while (g_running) {
        sysUtilCheckCallback();

        mp::PS3Interface &iface = *ifacep;   // aliases (re-bound each frame after any rebuild)
        Game &game = *gamep;

#ifdef NETTEST
        // Apply at most one queued test command before this frame's input.
        {
            std::string tcmd;
            if (nettest::PopCommand(tcmd))
                nettest::PostReply(handle_test_command(game, iface, tcmd));
        }
#endif

        int cur[11] = {0};
        ioPadGetInfo(&pad_info);
        if (pad_info.status[0]) {
            ioPadGetData(0, &pad_data);
            cur[0] = pad_data.BTN_CROSS;    cur[1] = pad_data.BTN_CIRCLE;
            cur[2] = pad_data.BTN_UP;       cur[3] = pad_data.BTN_DOWN;
            cur[4] = pad_data.BTN_LEFT;     cur[5] = pad_data.BTN_RIGHT;
            cur[6] = pad_data.BTN_L2;       cur[7] = pad_data.BTN_TRIANGLE;
            cur[8] = pad_data.BTN_SQUARE;   cur[9] = pad_data.BTN_START;
            cur[10] = pad_data.BTN_SELECT;
        }
        int edge[11];
        for (int i = 0; i < 11; ++i) { edge[i] = cur[i] && !prev[i]; prev[i] = cur[i]; }
        const int eX = edge[0], eO = edge[1], eUp = edge[2], eDn = edge[3],
                  eLt = edge[4], eRt = edge[5], eL2 = edge[6], eTri = edge[7], eSq = edge[8],
                  eStart = edge[9], eSelect = edge[10];

        // ---- menu system (title / main menu / setup / how-to) ----
        if (ui_in_menu()) {
            tiny3d_Clear(CLEAR, TINY3D_CLEAR_ALL);
            tiny3d_Project2D();
            reset_ttf_frame();
            set_ttf_window(0, 0, SCREEN_W, SCREEN_H, WIN_SKIP_LF);
            UiGameConfig cfg;
            UiAction act = ui_menu_frame(cur[2], cur[3], cur[4], cur[5],
                                         eX, eO, eStart, &cfg);
            tiny3d_Flip();
            audio_update();
            if (act == UI_ACT_START_GAME) {
                delete gamep; delete ifacep;
                ifacep = new mp::PS3Interface(cfg.playerCount, seed);
                gamep  = new Game(ifacep);
                gamep->process();
                paused = mgmtOpen = tradeOpen = 0;
            } else if (act == UI_ACT_QUIT) {
                g_running = 0;
            }
            continue;
        }

        // Pause (Start toggles). While paused, game input is suspended but the frame
        // still renders (the Clay UI shows the pause modal from the snapshot).
        if (eStart) paused = !paused;
        if (paused && eSelect) { ui_goto_menu(); paused = 0; mgmtOpen = 0; tradeOpen = 0; }

        // ---- drive the engine (skipped while paused) ----
        GameState s = game.get_state();
        const bool over = s.is_game_over();
        const TurnPhase ph = s.get_turn_phase();
        const int c = s.get_controlling_player_index();

        if (!paused) {
        // The management window is available on the controlling player's own turn,
        // outside mid-decision phases (build/mortgage are rejected there anyway).
        const bool mgmtAllowed = !over &&
            (ph == TurnPhase::WaitingForRoll || ph == TurnPhase::WaitingForTurnEnd ||
             ph == TurnPhase::WaitingForAcquisitionManagement ||
             ph == TurnPhase::WaitingForDebtSettlement);   // raise cash to pay a debt
        if (eL2) mgmtOpen = mgmtOpen ? 0 : (mgmtAllowed ? 1 : 0);
        if (mgmtOpen && !mgmtAllowed) mgmtOpen = 0;

        // Trade builder is available on your own (non-jailed) turn when you hold a deed.
        const bool tradeAllowed = !over && !mgmtOpen &&
            (ph == TurnPhase::WaitingForRoll || ph == TurnPhase::WaitingForTurnEnd) &&
            s.get_player_turns_remaining_in_jail(c) == 0 &&
            s.get_players_remaining_count() > 1 && !deeds_of(s, c).empty();
        if (tradeOpen && !tradeAllowed) tradeOpen = 0;

        if (mgmtOpen) {
            // ---- management window drives the pad ----
            std::vector<Property> deeds = deeds_of(s, c);
            int n = (int) deeds.size();
            if (n > 0) {
                if (eUp && mgmtSel > 0)      mgmtSel--;
                if (eDn && mgmtSel < n - 1)  mgmtSel++;
                if (mgmtSel >= n) mgmtSel = n - 1;
                Property p = deeds[mgmtSel];
                int advanced = 0;
                if (eTri)      { iface.buy_building(c, p);  advanced = 1; }
                else if (eSq)  { iface.sell_building(c, p); advanced = 1; }
                else if (eX) {
                    if (s.get_property_is_mortgaged(p)) iface.unmortgage_property(c, p);
                    else                                iface.mortgage_property(c, p);
                    advanced = 1;
                }
                if (advanced) { game.process(); audio_play_blip(); }
            }
            if (eO) mgmtOpen = 0;
        } else if (tradeOpen) {
            // ---- trade builder drives the pad ----
            std::vector<Property> deeds = deeds_of(s, c);
            int nd = (int) deeds.size();
            if (eSq) tradeTarget = next_other_player(s, tradeTarget, c);
            if (nd > 0) {
                if (eLt && tradeDeedSel > 0)      tradeDeedSel--;
                if (eRt && tradeDeedSel < nd - 1) tradeDeedSel++;
                if (tradeDeedSel >= nd) tradeDeedSel = nd - 1;
            }
            if (eUp) tradeCash += 50;
            if (eDn) { tradeCash -= 50; if (tradeCash < 0) tradeCash = 0; }
            if (eX && nd > 0) {
                Trade tr;
                tr.offeringPlayer    = c;
                tr.consideringPlayer = tradeTarget;
                tr.offer.deeds.insert(deeds[tradeDeedSel]);
                tr.consideration.cash = tradeCash;
                iface.propose_trade(tr); game.process();
                tradeOpen = 0; audio_play_blip();
            }
            if (eO) tradeOpen = 0;
        } else if (eTri && tradeAllowed) {
            // Triangle opens the trade builder (jail's Triangle=use-card is gated out
            // by tradeAllowed, so the two never clash)
            tradeOpen = 1; tradeTarget = next_other_player(s, c, c);
            tradeDeedSel = 0; tradeCash = 100;
        } else if (!over) {
            // ---- normal turn controls ----
            int advanced = 0;
            switch (ph) {
                case TurnPhase::WaitingForRoll:
                    if (s.get_player_turns_remaining_in_jail(c) > 0) {
                        // jailed: roll for doubles, pay bail, or use a jail card
                        if (eX)        { iface.roll_dice(c);                     advanced = 1; audio_play_blip(); }
                        else if (eSq)  { iface.pay_bail(c);                      advanced = 1; }
                        else if (eTri) { iface.use_get_out_of_jail_free_card(c); advanced = 1; }
                    } else if (eX) {
                        iface.roll_dice(c); advanced = 1; audio_play_blip();
                    }
                    break;
                case TurnPhase::WaitingForTurnEnd:
                    if (eX) { iface.end_turn(c); advanced = 1; }
                    break;
                case TurnPhase::WaitingForBuyPropertyInput:
                    if (eX)      { iface.buy_property(c);     advanced = 1; audio_play_blip(); }
                    else if (eO) { iface.auction_property(c); advanced = 1; }
                    break;
                case TurnPhase::WaitingForBids: {
                    Auction a = s.get_current_auction();
                    int minBid = a.highestBid + 1;
                    if (bidAmount < minBid) bidAmount = a.highestBid + 10;   // default raise
                    if (eUp) bidAmount += 10;
                    if (eDn) bidAmount -= 10;
                    if (eRt) bidAmount += 50;
                    if (eLt) bidAmount -= 50;
                    int funds = s.get_player_funds(c);
                    if (bidAmount < minBid) bidAmount = minBid;
                    if (bidAmount > funds)  bidAmount = funds;
                    if (eX)      { iface.bid(c, bidAmount); bidAmount = 0; advanced = 1; audio_play_blip(); }
                    else if (eO) { iface.decline_bid(c);    bidAmount = 0; advanced = 1; }
                    break;
                }
                case TurnPhase::WaitingForDebtSettlement:
                    // raise cash via L2 (mgmtAllowed); O declares bankruptcy
                    if (eO) { iface.resign(c); advanced = 1; }
                    break;
                case TurnPhase::WaitingForTradeOfferResponse:
                    if (eX) {   // accept: send the reciprocal (matching) offer back
                        Trade pend = s.get_pending_trade_offer();
                        Trade rec;
                        rec.offeringPlayer   = pend.consideringPlayer;
                        rec.consideringPlayer = pend.offeringPlayer;
                        rec.offer         = pend.consideration;
                        rec.consideration = pend.offer;
                        iface.propose_trade(rec); advanced = 1; audio_play_blip();
                    } else if (eO) { iface.decline_trade(c); advanced = 1; }
                    break;
                // auto-resolved for now:
                case TurnPhase::WaitingForAcquisitionManagement: iface.end_turn(c);      advanced = 1; break;
                default: break;
            }
            if (advanced) game.process();
        }
        } // if (!paused)

        // ---- render: raw board (scene) + Clay HUD/overlays from a snapshot ----
        s = game.get_state();
        UiSnapshot snap;
        fill_snapshot(snap, s, iface.player_count(), mgmtOpen, mgmtSel,
                      tradeOpen, tradeTarget, tradeDeedSel, tradeCash, bidAmount, paused);

        tiny3d_Clear(CLEAR, TINY3D_CLEAR_ALL);
        tiny3d_Project2D();
        draw_board(s, iface.player_count());     // raw scene, under the Clay UI
        reset_ttf_frame();
        set_ttf_window(0, 0, SCREEN_W, SCREEN_H, WIN_SKIP_LF);
        ui_render_game(&snap);                   // Clay HUD + overlays (does clay_render)

        tiny3d_Flip();
        audio_update();
    }

    audio_shutdown();
    return 0;
}
