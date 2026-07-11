"""M2: buying, declining -> auction, ownership, rent. Drives the real roll/land
pipeline (deterministic seed) rather than forcing engine internals."""

import board


def test_bank_owns_unbought_property(game):
    assert game.owner(board.BOARDWALK) == -1   # -1 == bank / unowned


def test_landing_on_unowned_property_offers_buy(game):
    assert game.drive_to_buy(), "active player should reach a buy decision"
    landed = game.landed()
    assert landed["property"] == "1"
    assert landed["owner"] == "-1"
    assert int(landed["price"]) > 0


def test_buy_sets_owner_and_charges_price(game):
    assert game.drive_to_buy()
    landed = game.landed()
    space, price = int(landed["space"]), int(landed["price"])
    p = game.controlling()
    before = game.funds(p)
    game.buy()
    assert game.owner(space) == p, "buyer should own the property"
    assert game.funds(p) == before - price, "price should be deducted"
    assert space in game.deeds(p)


def test_decline_starts_auction(game):
    assert game.drive_to_buy()
    space = int(game.landed()["space"])
    game.auction()                        # decline -> auction
    info = game.auctioninfo()
    assert info is not None, "declining should open an auction"
    assert int(info["property"]) == space


def test_auction_awards_highest_bidder(game):
    for p in range(4):
        game.setfunds(p, 1500)
    assert game.drive_to_buy()
    space = int(game.landed()["space"])
    game.auction()                        # active player declines -> auction

    winner = int(game.auctioninfo()["nextbidder"])
    game.bid(100)                         # first bidder bids $100
    for _ in range(8):                    # everyone else passes
        if game.phase() != "bids":
            break
        game.bid_pass()

    assert game.owner(space) == winner
    assert game.funds(winner) == 1500 - 100


def test_rent_charged_on_owned_property(game):
    # Player 1 owns Boardwalk; force player 0 to land on it -> pays rent to player 1.
    game.setfunds(0, 1500)
    game.setfunds(1, 1500)
    game.givedeed(1, board.BOARDWALK)
    assert game.owner(board.BOARDWALK) == 1
    game.land(0, board.BOARDWALK)
    assert game.funds(0) < 1500, "player 0 should have paid rent"
    assert game.funds(1) > 1500, "player 1 should have collected rent"
