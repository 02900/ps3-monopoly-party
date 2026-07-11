"""M5: player-to-player trading. Player A offers; control passes to the considering
player B, who accepts (reciprocal offer) or declines. force_* sets up ownership/cash.
"""

import board


def test_offer_passes_control_to_considering_player(game):
    game.givedeed(0, board.BOARDWALK)
    game.offertrade(1, give_deed=board.BOARDWALK, get_cash=300)   # P0 offers Boardwalk for $300
    st = game.state()
    assert st["phase"] == "trade"
    assert st["controlling"] == "1"          # now player 1 must respond
    info = game.pendingtrade()
    assert info is not None
    assert info["from"] == "0" and info["to"] == "1"
    assert info["getcash"] == "300"


def test_accept_property_for_cash(game):
    game.givedeed(0, board.BOARDWALK)
    game.setfunds(0, 1000)
    game.setfunds(1, 1000)
    game.offertrade(1, give_deed=board.BOARDWALK, get_cash=300)
    game.accepttrade()
    assert game.owner(board.BOARDWALK) == 1          # Boardwalk moved to player 1
    assert game.funds(0) == 1000 + 300               # player 0 got the cash
    assert game.funds(1) == 1000 - 300               # player 1 paid


def test_decline_leaves_everything_unchanged(game):
    game.givedeed(0, board.BOARDWALK)
    game.setfunds(0, 1000)
    game.setfunds(1, 1000)
    game.offertrade(1, give_deed=board.BOARDWALK, get_cash=300)
    game.declinetrade()
    assert game.owner(board.BOARDWALK) == 0
    assert game.funds(0) == 1000
    assert game.funds(1) == 1000


def test_property_for_property_swap(game):
    game.givedeed(0, board.BOARDWALK)
    game.givedeed(1, board.PARK_PLACE)
    # P0 gives Boardwalk, wants Park Place from P1
    game.offertrade(1, give_deed=board.BOARDWALK, get_deed=board.PARK_PLACE)
    game.accepttrade()
    assert game.owner(board.BOARDWALK) == 1
    assert game.owner(board.PARK_PLACE) == 0
