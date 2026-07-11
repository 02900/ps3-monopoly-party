"""M4: jail (bail / card), Chance & Community Chest card effects, taxes, and
bankruptcy -> elimination -> win. All deterministic via force_* commands."""

import board


# ---- jail ------------------------------------------------------------------
def test_gojail_sends_to_jail(game):
    game.gojail(0)
    assert game.jailed(0) > 0
    assert game.pos_index(0) == board.JAIL


def test_pay_bail_releases_and_charges_50(game):
    game.gojail(0)              # sending to jail ends the turn...
    game.startturn(0)          # ...so bring the turn back to the jailed player
    game.setfunds(0, 1000)
    game.paybail()
    assert game.jailed(0) == 0
    assert game.funds(0) == 1000 - 50


def test_jail_card_releases_without_charge(game):
    game.gojail(0)
    game.startturn(0)
    game.setfunds(0, 1000)
    game.givejailcard(0, "chance")
    assert game.send("jailcards 0") == "1"
    game.usejail()
    assert game.jailed(0) == 0
    assert game.funds(0) == 1000               # card, not cash
    assert game.send("jailcards 0") == "0"     # card spent (returned to deck)


# ---- cards -----------------------------------------------------------------
def test_chance_advance_to_go(game):
    game.setfunds(0, 500)
    game.setpos(0, board.FREE_PARKING)         # 20
    game.stackcard("chance", "Chance_AdvanceToGo")
    game.drawcard(0, "chance")
    assert game.pos_index(0) == board.GO
    assert game.funds(0) == 500 + 200


def test_chance_go_to_jail(game):
    game.setpos(0, board.FREE_PARKING)
    game.stackcard("chance", "Chance_GoToJail")
    game.drawcard(0, "chance")
    assert game.jailed(0) > 0
    assert game.pos_index(0) == board.JAIL


def test_community_chest_bank_error_collect_200(game):
    game.setfunds(0, 500)
    game.stackcard("cc", "CommunityChest_Gain200")
    game.drawcard(0, "cc")
    assert game.funds(0) == 700


def test_community_chest_doctors_fee_pay_50(game):
    game.setfunds(0, 500)
    game.stackcard("cc", "CommunityChest_Pay50")
    game.drawcard(0, "cc")
    assert game.funds(0) == 450


# ---- taxes -----------------------------------------------------------------
def test_income_tax_charges(game):
    game.setfunds(0, 2000)
    game.land(0, board.INCOME_TAX)
    assert game.funds(0) < 2000                # $200 or 10%


def test_luxury_tax_charges_100(game):
    # The engine uses the modern $100 Luxury Tax (the 2002 FAQ lists $75); we reuse
    # the engine verbatim, so the on-device value is $100.
    game.setfunds(0, 2000)
    game.land(0, board.LUXURY_TAX)
    assert game.funds(0) == 2000 - 100


# ---- bankruptcy / win ------------------------------------------------------
def test_unpayable_rent_forces_debt(game):
    game.givedeed(0, board.BOARDWALK)          # player 0 owns Boardwalk
    game.setfunds(1, 10)                        # player 1 can't afford rent
    game.land(1, board.BOARDWALK)
    st = game.state()
    assert st["phase"] == "debt"
    assert st["controlling"] == "1"


def test_resign_eliminates_debtor(game):
    game.givedeed(0, board.BOARDWALK)
    game.setfunds(1, 10)
    game.land(1, board.BOARDWALK)
    game.resign()                              # debtor (player 1) declares bankruptcy
    assert game.send("eliminated 1") == "1"


def test_last_player_standing_wins(game):
    game.givedeed(0, board.BOARDWALK)
    for p in (1, 2, 3):
        game.setfunds(p, 10)
        game.land(p, board.BOARDWALK)
        game.resign()
    st = game.state()
    assert st["gameover"] == "1"
    assert st["remaining"] == "1"
    assert game.send("winner") == "0"
