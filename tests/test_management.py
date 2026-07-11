"""M3: property management — mortgage/unmortgage and building (full group + even
build). Driven via the management commands (owner = controlling player at phase roll).

Blue group = Park Place(37) + Boardwalk(39), $200/house. Reading RR(5) mortgage = $100.
"""

import board

BLUE = [board.PARK_PLACE, board.BOARDWALK]
HOUSE_BLUE = 200
RR_MORTGAGE = 100


def test_mortgage_pays_and_flags(game):
    game.givedeed(0, board.READING_RR)
    game.setfunds(0, 1000)
    game.mortgage(board.READING_RR)
    assert game.mortgaged(board.READING_RR)
    assert game.funds(0) == 1000 + RR_MORTGAGE


def test_unmortgage_costs_with_interest(game):
    game.givedeed(0, board.READING_RR)
    game.setfunds(0, 1000)
    game.mortgage(board.READING_RR)
    game.setfunds(0, 1000)                       # reset for a clean delta
    game.unmortgage(board.READING_RR)
    assert not game.mortgaged(board.READING_RR)
    assert game.funds(0) == 1000 - int(RR_MORTGAGE * 1.1)   # +10% interest


def test_cannot_build_without_full_group(game):
    game.givedeed(0, board.PARK_PLACE)           # only one of the two blues
    game.setfunds(0, 2000)
    game.build(board.PARK_PLACE)
    assert game.buildings(board.PARK_PLACE) == 0


def test_build_requires_even_building(game):
    for sp in BLUE:
        game.givedeed(0, sp)
    game.setfunds(0, 2000)

    game.build(board.PARK_PLACE)                 # 0/0 -> 1/0
    assert game.buildings(board.PARK_PLACE) == 1
    game.build(board.PARK_PLACE)                 # 1/0 -> blocked (uneven)
    assert game.buildings(board.PARK_PLACE) == 1
    game.build(board.BOARDWALK)                  # 1/0 -> 1/1
    assert game.buildings(board.BOARDWALK) == 1
    game.build(board.PARK_PLACE)                 # 1/1 -> 2/1
    assert game.buildings(board.PARK_PLACE) == 2


def test_building_charges_per_house(game):
    for sp in BLUE:
        game.givedeed(0, sp)
    game.setfunds(0, 2000)
    before = game.funds(0)
    game.build(board.PARK_PLACE)
    assert game.funds(0) == before - HOUSE_BLUE


def test_sell_building_refunds_half(game):
    for sp in BLUE:
        game.givedeed(0, sp)
    game.setfunds(0, 2000)
    game.build(board.PARK_PLACE)
    game.build(board.BOARDWALK)
    funds = game.funds(0)
    game.sell(board.PARK_PLACE)                  # 2/1 -> refund half of a house
    assert game.buildings(board.PARK_PLACE) == 0
    assert game.funds(0) == funds + HOUSE_BLUE // 2


def test_cannot_mortgage_with_buildings_in_group(game):
    for sp in BLUE:
        game.givedeed(0, sp)
    game.setfunds(0, 2000)
    game.build(board.PARK_PLACE)
    game.build(board.BOARDWALK)
    game.mortgage(board.PARK_PLACE)              # blocked: group has buildings
    assert not game.mortgaged(board.PARK_PLACE)
