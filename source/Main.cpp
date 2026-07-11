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
#include <tiny3d.h>          // has its own extern "C" guard
#include "audio.h"           // has its own extern "C" guard

// ttf_render.h has no C-linkage guard of its own; wrap it. tiny3d.h (its only
// include) is already pulled in above, so the guarded re-include is a no-op and
// no system headers get dragged into this extern "C" block.
extern "C" {
#include "ttf_render.h"
}

#include "PS3Interface.h"
#include "engine/Game.h"
#include "engine/GameState.h"
#include "engine/Board.h"
#include "engine/DisplayStrings.h"   // to_string(Property/Space/…)

#ifdef NETTEST
#include "nettest/nettest.h"
#include "nettest/commands.h"
#endif

#include <string>

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
        char line[64];
        std::snprintf(line, sizeof line, "%s P%d   $%d",
                      p == active ? ">" : " ", p + 1, s.get_player_funds(p));
        text(HX, y, line, PLAYER_COL[p], 13, 20);
        y += 26;
    }
    y += 10;

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
    text(HX, y, pl, 0xffFFE082, 12, 18);
}

// ---- interactive overlays (buy / auction) ---------------------------------
// A centred modal in the (now clear) board centre. Split into background quads
// and foreground text so all quads precede all text in the frame.
static const int OV_X = BOARD_X0 + CELL + 6;
static const int OV_Y = BOARD_Y0 + 3 * CELL;
static const int OV_W = CELL * 9 - 12;
static const int OV_H = CELL * 4;

static void draw_overlay_bg(const GameState &s) {
    TurnPhase ph = s.get_turn_phase();
    if (ph != TurnPhase::WaitingForBuyPropertyInput && ph != TurnPhase::WaitingForBids)
        return;
    fill_rect(OV_X - 2, OV_Y - 2, OV_W + 4, OV_H + 4, 0xffF5F0E1);   // border
    fill_rect(OV_X,     OV_Y,     OV_W,     OV_H,     0xff1B2A38);   // panel
}

static void draw_overlay_fg(const GameState &s, int bidAmount) {
    const int tx = OV_X + 16;
    int ty = OV_Y + 14;
    const int c = s.get_controlling_player_index();

    if (s.get_turn_phase() == TurnPhase::WaitingForBuyPropertyInput) {
        Property prop = space_to_property(s.get_player_position(s.get_active_player_index()));
        char l[80];
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
        char l[80];
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

// ---- main ------------------------------------------------------------------
static padInfo  pad_info;
static padData  pad_data;

int main(void) {
    sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, sys_cb, NULL);

    tiny3d_Init(1024 * 1024);
    audio_init();
    ioPadInit(7);

    // TTF glyph cache: reserve RSX texture memory, then load the PS3 system font.
    g_ttf_texture = (u32 *) tiny3d_AllocTexture(1600 * 32 * 32 * 2 + 4096);
    init_ttf_table((u16 *) g_ttf_texture);
    TTFLoadFont(0, (char *)"/dev_flash/data/font/SCE-PS3-VR-R-LATIN2.TTF", NULL, 0);

    // Deterministic seed for now (varied seeding is a later-milestone polish item).
    mp::PS3Interface iface(4, "ps3-monopoly");
    Game game(&iface);
    game.process();                     // settle to the first WaitingForRoll

#ifdef NETTEST
    nettest::Start();                   // opt-in TCP command server (port 9010)
#endif

    int prev[6] = {0,0,0,0,0,0};    // X, O, Up, Dn, Lt, Rt
    int bidAmount = 0;              // current bidder's entry during an auction

    while (g_running) {
        sysUtilCheckCallback();

#ifdef NETTEST
        // Apply at most one queued test command before this frame's input.
        {
            std::string tcmd;
            if (nettest::PopCommand(tcmd))
                nettest::PostReply(handle_test_command(game, iface, tcmd));
        }
#endif

        int cur[6] = {0,0,0,0,0,0};
        ioPadGetInfo(&pad_info);
        if (pad_info.status[0]) {
            ioPadGetData(0, &pad_data);
            if (pad_data.BTN_START) g_running = 0;
            cur[0] = pad_data.BTN_CROSS;  cur[1] = pad_data.BTN_CIRCLE;
            cur[2] = pad_data.BTN_UP;     cur[3] = pad_data.BTN_DOWN;
            cur[4] = pad_data.BTN_LEFT;   cur[5] = pad_data.BTN_RIGHT;
        }
        int edge[6];
        for (int i = 0; i < 6; ++i) { edge[i] = cur[i] && !prev[i]; prev[i] = cur[i]; }
        const int eX = edge[0], eO = edge[1], eUp = edge[2], eDn = edge[3], eLt = edge[4], eRt = edge[5];

        // ---- drive the engine ----
        GameState s = game.get_state();
        if (!s.is_game_over()) {
            const int c = s.get_controlling_player_index();
            int advanced = 0;
            switch (s.get_turn_phase()) {
                case TurnPhase::WaitingForRoll:
                    if (eX) { iface.roll_dice(c); advanced = 1; audio_play_blip(); }
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
                // auto-resolved for now (interactive UI in later milestones):
                case TurnPhase::WaitingForAcquisitionManagement: iface.end_turn(c);      advanced = 1; break;
                case TurnPhase::WaitingForDebtSettlement:        iface.resign(c);        advanced = 1; break;
                case TurnPhase::WaitingForTradeOfferResponse:    iface.decline_trade(c); advanced = 1; break;
                default: break;
            }
            if (advanced) game.process();
        }

        // ---- render (all quads first, then all text) ----
        s = game.get_state();
        tiny3d_Clear(CLEAR, TINY3D_CLEAR_ALL);
        tiny3d_Project2D();

        draw_board(s, iface.player_count());
        draw_overlay_bg(s);

        reset_ttf_frame();
        set_ttf_window(0, 0, SCREEN_W, SCREEN_H, WIN_SKIP_LF);
        draw_hud(s, iface.player_count());
        draw_overlay_fg(s, bidAmount);

        tiny3d_Flip();
        audio_update();
    }

    audio_shutdown();
    return 0;
}
