#ifndef MONOPOLY_UI_H
#define MONOPOLY_UI_H

// Plain-C bridge between the C++ game (Main.cpp) and the Clay-based UI (source/ui/*.c).
// The C++ side never includes clay.h — it only calls these functions and fills the
// plain-C structs. Clay's compound-literal macros stay in the C TUs.

#ifdef __cplusplus
extern "C" {
#endif

// Token themes (Monopoly Party). Full per-player token art lands in P4.
enum { UI_THEME_CLASSIC, UI_THEME_FANTASY, UI_THEME_SCIFI, UI_THEME_ANCIENT, UI_THEME_PREHISTORIC, UI_THEME_COUNT };

// The setup chosen in the menus, handed to the game when a match starts.
typedef struct {
    int playerCount;   // 2..4
    int theme[4];      // token theme per player (P4; defaults for now)
    int token[4];      // token index within the theme (P4)
} UiGameConfig;

// What the menu frame wants the game to do.
typedef enum { UI_ACT_NONE = 0, UI_ACT_START_GAME, UI_ACT_QUIT } UiAction;

// ---- in-game UI snapshot (filled from GameState each frame by the C++ game) ----
// Mirrors the engine's TurnPhase so the C UI stays free of engine types.
enum { UI_PHASE_ROLL, UI_PHASE_BUY, UI_PHASE_BIDS, UI_PHASE_MANAGE,
       UI_PHASE_TURNEND, UI_PHASE_DEBT, UI_PHASE_TRADE, UI_PHASE_GAMEOVER };

#define UI_MAX_DEEDS 28

typedef struct {
    int  count;                 // players in the match (2..4)
    int  funds[4];
    int  netWorth[4];
    int  jailed[4];             // turns remaining in jail (0 = free)
    int  eliminated[4];
    int  active;                // whose turn
    int  controlling;           // who the game is waiting on
    int  phase;                 // UI_PHASE_*
    int  gameOver;
    int  winner;                // -1 if none
    int  die1, die2;
    char spaceName[40];         // active player's current space

    // overlay payloads (read per phase / flag)
    char buyName[40];  int buyPrice;                            // BUY
    char aucName[40];  int aucHighBid; int aucBid; int aucBidder; // BIDS
    int  debtShortfall;                                          // DEBT
    int  jailTurns; int jailCanBail; int jailHasCard;           // jail (controlling)
    int  trOfferer; char trGive[72]; char trGet[72];            // TRADE response

    int  mgmtOpen; int mgmtSel; int mgmtCount;                  // L2 window
    char mgmtName[UI_MAX_DEEDS][22]; char mgmtTag[UI_MAX_DEEDS][6]; int mgmtMort[UI_MAX_DEEDS];

    int  tradeOpen; int tradeTarget; char tradeGive[40]; int tradeCash; // Triangle builder

    int  paused;
} UiSnapshot;

// Create Clay's arena + text-measure callback. Call once, after ya2d + ttf init.
void ui_init(int screen_w, int screen_h);

// Render the in-game HUD + the active overlay/modal with Clay. Call each frame in a
// match AFTER the raw board draw + tiny3d_Project2D() + reset_ttf_frame().
void ui_render_game(const UiSnapshot *snap);

// Are we in the menu system (title/menu/setup/...) rather than in a match?
int  ui_in_menu(void);
// Leave a match back to the main menu (e.g. pause -> quit to menu).
void ui_goto_menu(void);
// Force into a match (used by the nettest harness so `newgame` enters the game).
void ui_force_game(void);

// Run one menu frame: advance the screen state machine from the pad and render the
// current screen with Clay. Held D-pad (level) drives auto-repeat navigation; the
// face buttons are edge-triggered. Call each frame (menu mode) AFTER
// tiny3d_Project2D() + reset_ttf_frame(). On UI_ACT_START_GAME, *cfg is filled.
UiAction ui_menu_frame(int hUp, int hDown, int hLeft, int hRight,
                       int eCross, int eCircle, int eStart, UiGameConfig *cfg);

#ifdef __cplusplus
}
#endif

#endif // MONOPOLY_UI_H
