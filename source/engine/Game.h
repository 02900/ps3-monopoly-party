#pragma once
#include"GameState.h"
#include"Input.h"

// PS3 port: <condition_variable>/<future>/<mutex>/<thread> were included but never
// used (the engine is single-threaded); dropped so it builds under PSL1GHT's
// libstdc++, which has no std::thread runtime.
#include<memory>
#include<queue>
#include<variant>
#include<vector>

namespace monopoly
{
    class IInterface;
    class Game
    {
    public:
        ~Game();
        Game(IInterface* interface);

        GameState get_state() const;
        void set_state(GameState state);

        // Game is started over with the same setup
        void reset();

        // Process inputs until further player intervention is required
        void process();

    private:
        void process_inputs();

        void process_input(int playerIndex, Input const& input);

        void start();
        void stop();

        IInterface* const interface;
        GameSetup const setup;

        int currentCycle;
        GameState state;
    };
}
