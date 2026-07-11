/*
 * ps3-monopoly-party — M0 headless smoke test
 *
 * Goal of this throwaway main: prove the vendored jemeador/monopoly engine
 * (source/engine, C++17, STL-only) compiles and RUNS under the PS3 ppu-gcc
 * toolchain. It plays a fully automated game — a tiny "bot" that answers each
 * TurnPhase with a sensible default input — and logs the result to the PS3 TTY
 * (visible in RPCS3.log). No graphics yet; the tiny3d frontend arrives in M1.
 *
 * Everything Monopoly-specific lives in the reused engine; this file only drives
 * it through the same monopoly::IInterface seam the web/CLI frontends use.
 */
#include "engine/Game.h"
#include "engine/GameState.h"
#include "engine/IInterface.h"
#include "engine/DisplayStrings.h"

#include <cstdio>

using namespace monopoly;

namespace {

// A minimal auto-player: subclasses the engine's SimpleInterface (which already
// implements poll() + all the input helpers) and just needs get_setup()/update().
class BotInterface : public SimpleInterface {
public:
    GameSetup get_setup() final {
        GameSetup s;
        s.playerCount = 4;
        s.startingFunds = 1500;
        s.seed = "ps3-m0";          // deterministic run
        return s;
    }
    void update(GameState state) final { latest = state; }
    GameState latest;
};

} // namespace

int main()
{
    std::printf("\n===== MONOPOLY-PS3 M0 headless smoke =====\n");

    BotInterface bot;
    Game game(&bot);

    int steps = 0;
    const int kMaxSteps = 2000;
    for (; steps < kMaxSteps; ++steps) {
        GameState s = game.get_state();
        if (s.is_game_over())
            break;

        const int p = s.get_controlling_player_index();
        switch (s.get_turn_phase()) {
        case TurnPhase::WaitingForRoll:                 bot.roll_dice(p);      break;
        case TurnPhase::WaitingForBuyPropertyInput:     bot.buy_property(p);   break;
        case TurnPhase::WaitingForAcquisitionManagement:bot.end_turn(p);       break;
        case TurnPhase::WaitingForTurnEnd:              bot.end_turn(p);       break;
        case TurnPhase::WaitingForBids:                 bot.decline_bid(p);    break;
        case TurnPhase::WaitingForDebtSettlement:       bot.resign(p);         break;
        case TurnPhase::WaitingForTradeOfferResponse:   bot.decline_trade(p);  break;
        case TurnPhase::GameOver:                                              break;
        }
        game.process();
    }

    GameState s = game.get_state();
    std::printf("---- ran %d engine steps; game_over=%d, players_remaining=%d ----\n",
                steps, (int)s.is_game_over(), s.get_players_remaining_count());
    for (int i = 0; i < 4; ++i) {
        std::printf("  player %d: funds=$%d  netWorth=$%d  pos=%s\n",
                    i, s.get_player_funds(i), s.get_net_worth(i),
                    to_string(s.get_player_position(i)).c_str());
    }
    std::printf("===== M0 smoke done =====\n");
    std::fflush(stdout);
    return 0;
}
