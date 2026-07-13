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
static const int   CELL      = 42;           // 11x11 grid -> 462px board
// Board is centred full-screen; the Clay HUD floats over the side margins/corners.
static const int   BOARD_X0  = (SCREEN_W - CELL * 11) / 2;   // 193
static const int   BOARD_Y0  = (SCREEN_H - CELL * 11) / 2;   // 25
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

// ---- small board TTF text (needs reset_ttf_frame()/set_ttf_window() this frame) --
// colour is opaque ARGB; sw/sh are the per-glyph width/height.
static void board_text(int x, int y, const char *str, u32 argb, int sw, int sh) {
    display_ttf_string(x, y, str, argb_to_rgba(argb), 0, sw, sh);
}
// Pixel width of `str` at (sw,sh) — colour 0 measures without drawing.
static int board_text_w(const char *str, int sw, int sh) {
    return display_ttf_string(0, 0, str, 0, 0, sw, sh);
}
// Copy as many leading chars of `in` as fit within `maxpx` into `out` (cap incl. NUL).
static void fit_text(const char *in, char *out, int cap, int maxpx, int sw, int sh) {
    int n = 0;
    out[0] = 0;
    while (in[n] && n < cap - 1) {
        out[n] = in[n]; out[n + 1] = 0;
        if (board_text_w(out, sw, sh) > maxpx) { out[n] = 0; break; }
        ++n;
    }
}
// Draw `str` horizontally centred within [x, x+w) at vertical `y`.
static void board_text_centered(int x, int w, int y, const char *str, u32 argb, int sw, int sh) {
    int tw = board_text_w(str, sw, sh);
    board_text(x + (w - tw) / 2, y, str, argb, sw, sh);
}

// ---- rendering (board = the raw "scene"; HUD/overlays are Clay, see source/ui) --
static void draw_board(const GameState &s, int playerCount, int theme) {
    // squares
    for (int i = 0; i < 40; ++i) {
        int c, r; cell_of(i, &c, &r);
        int x = BOARD_X0 + c * CELL;
        int y = BOARD_Y0 + r * CELL;
        Space sp = index_to_space(i);
        // Cell background is always the board colour; the 1px inset reads as a gridline.
        fill_rect(x + 1, y + 1, CELL - 2, CELL - 2, BOARD_TAN);

        if (space_is_property(sp)) {
            Property prop  = space_to_property(sp);
            PropertyGroup g = property_group(prop);
            int colored = (g != PropertyGroup::Railroad && g != PropertyGroup::Utility);

            // Coloured groups: a colour band across the top + the (abbreviated) name.
            if (colored) {
                const int BAND = 9;
                fill_rect(x + 1, y + 1, CELL - 2, BAND, group_color(g));
                char name[24];
                fit_text(to_string(prop).c_str(), name, sizeof name, CELL - 4, 5, 8);
                board_text_centered(x + 1, CELL - 2, y + BAND + 3, name, INK, 5, 8);
            }

            // Price at the bottom-centre for every property (colour groups + RR/util).
            char price[8];
            std::snprintf(price, sizeof price, "$%d", price_of_property(prop));
            board_text_centered(x + 1, CELL - 2, y + CELL - 12, price, INK, 6, 9);

            // owner marker (top-left corner in the owner's colour; dimmed if mortgaged)
            int owner = s.get_property_owner_index(prop);
            if (owner >= 0) {
                fill_rect(x + 3, y + 3, 10, 10, INK);
                fill_rect(x + 4, y + 4, 8, 8,
                          s.get_property_is_mortgaged(prop) ? 0xff707070 : PLAYER_COL[owner]);
            }
        }
        // special-square icon (jail / tax / railroad / utility), centred; no-op otherwise
        ui_draw_space_icon(i, x + (CELL - 24) / 2, y + (CELL - 24) / 2, 24);
    }

    // board-centre art (fills the inner 9x9 region, above the navy panel)
    ui_draw_board_center(BOARD_X0 + CELL + 1, BOARD_Y0 + CELL + 1, CELL * 9 - 2);

    // tokens (2x2 cluster inside the square): the theme sprite, or a coloured square
    for (int p = 0; p < playerCount; ++p) {
        if (s.get_player_eliminated(p)) continue;
        int i = space_to_index(s.get_player_position(p));
        int c, r; cell_of(i, &c, &r);
        int x = BOARD_X0 + c * CELL + 3 + (p % 2) * 19;
        int y = BOARD_Y0 + r * CELL + 3 + (p / 2) * 19;
        if (!ui_draw_token(theme, p, x, y, 18)) {
            fill_rect(x - 1, y - 1, 15, 15, INK);          // outline
            fill_rect(x, y, 13, 13, PLAYER_COL[p]);
        }
    }
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


// ---- property management window (L2) --------------------------------------

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
    // Rodin (the PS3 XMB UI font) — cleaner/more legible than the VR face.
    TTFLoadFont(0, (char *)"/dev_flash/data/font/SCE-PS3-RD-R-LATIN.TTF", NULL, 0);

    ui_init(SCREEN_W, SCREEN_H);        // Clay arena + text-measure callback
    ui_images_load();                   // embedded art (stub until the asset pass)

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
    int gameTheme = 0;             // token theme chosen in setup (UI_THEME_*)

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
            ui_begin_frame(CLEAR);
            ui_menu_bg();                    // full-screen art under the Clay menu
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
                gameTheme = cfg.theme[0];
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
        snap.tokenTheme = gameTheme;

        ui_begin_frame(CLEAR);
        reset_ttf_frame();                       // one TTF reset covers board text + Clay
        set_ttf_window(0, 0, SCREEN_W, SCREEN_H, WIN_SKIP_LF);
        draw_board(s, iface.player_count(), gameTheme);   // raw scene, under the Clay UI
        ui_render_game(&snap);                   // Clay HUD + overlays (does clay_render)

        tiny3d_Flip();
        audio_update();
    }

    audio_shutdown();
    return 0;
}
