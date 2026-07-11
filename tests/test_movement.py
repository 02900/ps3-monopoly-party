"""M1: connectivity, initial state, rolling and turn flow."""

import board


def test_ping(game):
    assert game.ping() == "pong"


def test_initial_state(game):
    st = game.state()
    assert st["phase"] == "roll"
    assert st["gameover"] == "0"
    assert st["active"] == "0"
    assert st["remaining"] == "4"
    for p in range(4):
        assert game.funds(p) == 1500, f"player {p} should start with $1500"
        assert game.pos_index(p) == board.GO, f"player {p} should start on GO"


def test_roll_produces_valid_dice(game):
    game.roll()
    a, b, s = game.dice()
    assert 1 <= a <= 6 and 1 <= b <= 6
    assert s == a + b


def test_roll_moves_active_player(game):
    game.roll()
    a, b, s = game.dice()
    pos = game.pos_index(0)
    # From GO(0) the dice sum (2..12) lands on non-"mover" spaces here, so the
    # position equals the roll total. (If a Chance/CC relocation is ever hit, this
    # is where it'd surface.)
    assert pos == s, f"expected to land on space {s}, got {pos}"


def test_only_active_player_moves(game):
    game.roll()
    for p in range(1, 4):
        assert game.pos_index(p) == board.GO, f"player {p} shouldn't have moved yet"
