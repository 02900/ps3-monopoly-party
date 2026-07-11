#ifndef MONOPOLY_NETTEST_COMMANDS_H
#define MONOPOLY_NETTEST_COMMANDS_H
#ifdef NETTEST

#include <string>

namespace monopoly { class Game; }
namespace mp { class PS3Interface; }

// Interpret one line-based test command against the live game, returning the single
// line to send back. Runs on the main thread (called from the render loop after
// nettest::PopCommand), so it can freely touch the engine.
std::string handle_test_command(monopoly::Game& game, mp::PS3Interface& iface,
                                const std::string& cmd);

#endif  // NETTEST
#endif  // MONOPOLY_NETTEST_COMMANDS_H
