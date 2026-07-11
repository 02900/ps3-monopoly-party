#pragma once
//
// PS3Interface — the platform bridge between the reused monopoly engine and the
// PS3 front end. It subclasses the engine's SimpleInterface, which already
// implements poll() and every input helper (roll_dice, buy_property, bid,
// end_turn, …); we only supply get_setup() (the game configuration) and
// update() (which caches the latest GameState for the renderer to read).
//
// All engine state is owned by the main loop; this object is only ever touched
// on the main thread, matching the single-threaded engine.
//
#include "engine/IInterface.h"
#include "engine/GameState.h"

#include <string>

namespace mp {

class PS3Interface : public monopoly::SimpleInterface {
public:
    PS3Interface(int playerCount, std::string seed)
        : playerCount_(playerCount), seed_(std::move(seed)) {}

    monopoly::GameSetup get_setup() final {
        monopoly::GameSetup s;
        s.playerCount   = playerCount_;
        s.startingFunds = 1500;      // Monopoly Party / US rules starting cash
        s.seed          = seed_;
        return s;
    }

    void update(monopoly::GameState state) final { state_ = state; }

    const monopoly::GameState& state() const { return state_; }
    int player_count() const { return playerCount_; }

private:
    int playerCount_;
    std::string seed_;
    monopoly::GameState state_;
};

} // namespace mp
