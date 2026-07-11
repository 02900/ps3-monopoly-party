// e2e test command interpreter (see commands.h). Compiled only under -DNETTEST.
//
// Commands are line-based ASCII. Two families:
//  * driving  — push an engine Input via the interface, then game.process():
//               roll / buy / auction / bid <n> / pass / endturn / mortgage <sp> /
//               unmortgage <sp> / build <sp> / sell <sp> / paybail / usejail
//  * setup    — deterministic scenario building via GameState::force_* through
//               get_state()/set_state(): setfunds / setpos / givedeed / offer /
//               gojail
//  * queries  — read the live state: state / funds / net / pos / owner / mortgaged
//               buildings / dice / landed / auctioninfo / deeds / jailed
//
// Property/space arguments are the board index 0..39 (space_to_property maps it).
#ifdef NETTEST

#include "commands.h"
#include "../PS3Interface.h"

#include "../engine/Game.h"
#include "../engine/GameState.h"
#include "../engine/Board.h"
#include "../engine/DisplayStrings.h"

#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

using namespace monopoly;

namespace {

std::vector<std::string> split(const std::string& s) {
    std::vector<std::string> out;
    std::istringstream iss(s);
    std::string tok;
    while (iss >> tok) out.push_back(tok);
    return out;
}

int as_int(const std::string& s) { return (int) std::strtol(s.c_str(), nullptr, 10); }

std::string err(const std::string& m) { return "err " + m; }

bool valid_player(const GameState& s, int p) { return p >= 0 && p < s.get_player_count(); }

// Board index 0..39 -> Property (or Invalid if that space is not a property).
Property prop_at(int idx) {
    if (idx < 0 || idx >= 40) return Property::Invalid;
    return space_to_property(index_to_space(idx));
}

DeckType deck_from_str(const std::string& s, bool& ok) {
    ok = true;
    if (s == "chance") return DeckType::Chance;
    if (s == "cc" || s == "communitychest") return DeckType::CommunityChest;
    ok = false;
    return DeckType::Chance;
}

// A small subset of cards, enough for deterministic card-effect tests.
bool card_from_key(const std::string& k, Card& out) {
    if (k == "Chance_AdvanceToGo")       { out = Card::Chance_AdvanceToGo;       return true; }
    if (k == "Chance_GoToJail")          { out = Card::Chance_GoToJail;          return true; }
    if (k == "Chance_Gain50")            { out = Card::Chance_Gain50;            return true; }
    if (k == "CommunityChest_Gain200")   { out = Card::CommunityChest_Gain200;   return true; }
    if (k == "CommunityChest_Pay50")     { out = Card::CommunityChest_Pay50;     return true; }
    return false;
}

const char* phase_name(TurnPhase p) {
    switch (p) {
        case TurnPhase::WaitingForRoll:                  return "roll";
        case TurnPhase::WaitingForBuyPropertyInput:      return "buy";
        case TurnPhase::WaitingForBids:                  return "bids";
        case TurnPhase::WaitingForAcquisitionManagement: return "manage";
        case TurnPhase::WaitingForTurnEnd:               return "turnend";
        case TurnPhase::WaitingForDebtSettlement:        return "debt";
        case TurnPhase::WaitingForTradeOfferResponse:    return "trade";
        case TurnPhase::GameOver:                        return "gameover";
    }
    return "?";
}

}  // namespace

std::string handle_test_command(Game& game, mp::PS3Interface& iface, const std::string& cmd) {
    std::vector<std::string> t = split(cmd);
    if (t.empty()) return err("empty");
    const std::string& op = t[0];

    GameState s = game.get_state();
    const int controlling = s.get_controlling_player_index();

    // ---- simple ----
    if (op == "ping") return "pong";
    if (op == "help")
        return "ping state funds net pos owner mortgaged buildings dice landed "
               "auctioninfo deeds jailed | newgame roll step buy auction bid pass "
               "endturn mortgage unmortgage build sell paybail usejail | "
               "setfunds setpos givedeed offer gojail";

    // ---- queries ----
    if (op == "state") {
        std::ostringstream o;
        o << "phase=" << phase_name(s.get_turn_phase())
          << " active=" << s.get_active_player_index()
          << " controlling=" << controlling
          << " gameover=" << (s.is_game_over() ? 1 : 0)
          << " remaining=" << s.get_players_remaining_count();
        return o.str();
    }
    if (op == "funds") {
        if (t.size() < 2 || !valid_player(s, as_int(t[1]))) return err("player");
        return std::to_string(s.get_player_funds(as_int(t[1])));
    }
    if (op == "net") {
        if (t.size() < 2 || !valid_player(s, as_int(t[1]))) return err("player");
        return std::to_string(s.get_net_worth(as_int(t[1])));
    }
    if (op == "pos") {
        if (t.size() < 2 || !valid_player(s, as_int(t[1]))) return err("player");
        Space sp = s.get_player_position(as_int(t[1]));
        return std::to_string(space_to_index(sp)) + " " + to_string(sp);
    }
    if (op == "owner") {
        if (t.size() < 2) return err("args");
        Property p = prop_at(as_int(t[1]));
        if (p == Property::Invalid) return err("notproperty");
        return std::to_string(s.get_property_owner_index(p));
    }
    if (op == "mortgaged") {
        if (t.size() < 2) return err("args");
        Property p = prop_at(as_int(t[1]));
        if (p == Property::Invalid) return err("notproperty");
        return s.get_property_is_mortgaged(p) ? "1" : "0";
    }
    if (op == "buildings") {
        if (t.size() < 2) return err("args");
        Property p = prop_at(as_int(t[1]));
        if (p == Property::Invalid) return err("notproperty");
        return std::to_string(s.get_building_level(p));
    }
    if (op == "dice") {
        std::pair<int,int> d = s.get_last_dice_roll();
        return std::to_string(d.first) + " " + std::to_string(d.second) + " "
             + std::to_string(d.first + d.second);
    }
    if (op == "landed") {
        int a = s.get_active_player_index();
        Space sp = s.get_player_position(a);
        Property p = space_to_property(sp);
        std::ostringstream o;
        o << "space=" << space_to_index(sp)
          << " property=" << (p != Property::Invalid ? 1 : 0);
        if (p != Property::Invalid)
            o << " price=" << price_of_property(p)
              << " owner=" << s.get_property_owner_index(p);
        return o.str();
    }
    if (op == "auctioninfo") {
        Auction a = s.get_current_auction();
        if (!a) return err("noauction");
        Property ap = a.property;
        std::ostringstream o;
        o << "property=" << space_to_index(property_to_space(ap))
          << " name=" << to_string(ap)
          << " highbid=" << a.highestBid
          << " nextbidder=" << (a.biddingOrder.empty() ? -1 : a.biddingOrder.front());
        return o.str();
    }
    if (op == "deeds") {
        if (t.size() < 2 || !valid_player(s, as_int(t[1]))) return err("player");
        std::set<Property> deeds = s.get_player_deeds(as_int(t[1]));
        if (deeds.empty()) return "(none)";
        std::ostringstream o;
        bool first = true;
        for (Property p : deeds) {
            if (!first) o << " ";
            o << space_to_index(property_to_space(p));
            first = false;
        }
        return o.str();
    }
    if (op == "jailed") {
        if (t.size() < 2 || !valid_player(s, as_int(t[1]))) return err("player");
        return std::to_string(s.get_player_turns_remaining_in_jail(as_int(t[1])));
    }
    if (op == "eliminated") {
        if (t.size() < 2 || !valid_player(s, as_int(t[1]))) return err("player");
        return s.get_player_eliminated(as_int(t[1])) ? "1" : "0";
    }
    if (op == "jailcards") {   // count of get-out-of-jail-free cards held
        if (t.size() < 2 || !valid_player(s, as_int(t[1]))) return err("player");
        return std::to_string((int) s.get_player_get_out_of_jail_free_cards(as_int(t[1])).size());
    }
    if (op == "winner") {
        if (!s.is_game_over()) return "-1";
        for (int i = 0; i < s.get_player_count(); ++i)
            if (!s.get_player_eliminated(i)) return std::to_string(i);
        return "-1";
    }

    // ---- driving (push an Input, then advance) ----
    if (op == "newgame") { game.reset(); return "ok"; }
    if (op == "roll")    { iface.roll_dice(controlling);      game.process(); return "ok"; }
    if (op == "step")    { game.process(); return "ok"; }
    if (op == "buy")     { iface.buy_property(controlling);   game.process(); return "ok"; }
    if (op == "auction" || op == "decline")
                         { iface.auction_property(controlling); game.process(); return "ok"; }
    if (op == "bid") {
        if (t.size() < 2) return err("args");
        iface.bid(controlling, as_int(t[1])); game.process(); return "ok";
    }
    if (op == "pass")    { iface.decline_bid(controlling);    game.process(); return "ok"; }
    if (op == "endturn") { iface.end_turn(controlling);       game.process(); return "ok"; }
    if (op == "paybail") { iface.pay_bail(controlling);       game.process(); return "ok"; }
    if (op == "usejail") { iface.use_get_out_of_jail_free_card(controlling); game.process(); return "ok"; }
    if (op == "resign")  { iface.resign(controlling);         game.process(); return "ok"; }
    if (op == "mortgage" || op == "unmortgage" || op == "build" || op == "sell") {
        if (t.size() < 2) return err("args");
        Property p = prop_at(as_int(t[1]));
        if (p == Property::Invalid) return err("notproperty");
        if      (op == "mortgage")   iface.mortgage_property(controlling, p);
        else if (op == "unmortgage") iface.unmortgage_property(controlling, p);
        else if (op == "build")      iface.buy_building(controlling, p);
        else                         iface.sell_building(controlling, p);
        game.process();
        return "ok";
    }

    // ---- setup (force_* via get_state/set_state) ----
    if (op == "setfunds") {
        if (t.size() < 3 || !valid_player(s, as_int(t[1]))) return err("args");
        s.force_funds(as_int(t[1]), as_int(t[2])); game.set_state(s); return "ok";
    }
    if (op == "setpos") {
        if (t.size() < 3 || !valid_player(s, as_int(t[1]))) return err("args");
        s.force_position(as_int(t[1]), index_to_space(as_int(t[2]))); game.set_state(s); return "ok";
    }
    if (op == "givedeed") {
        if (t.size() < 3 || !valid_player(s, as_int(t[1]))) return err("args");
        Property p = prop_at(as_int(t[2]));
        if (p == Property::Invalid) return err("notproperty");
        s.force_give_deed(as_int(t[1]), p); game.set_state(s); return "ok";
    }
    if (op == "offer") {
        if (t.size() < 3 || !valid_player(s, as_int(t[1]))) return err("args");
        Property p = prop_at(as_int(t[2]));
        if (p == Property::Invalid) return err("notproperty");
        s.force_property_offer_prompt(as_int(t[1]), p); game.set_state(s); return "ok";
    }
    if (op == "gojail") {
        if (t.size() < 2 || !valid_player(s, as_int(t[1]))) return err("player");
        s.force_go_to_jail(as_int(t[1])); game.set_state(s); return "ok";
    }
    if (op == "startturn") {   // make it player p's turn (WaitingForRoll)
        if (t.size() < 2 || !valid_player(s, as_int(t[1]))) return err("player");
        s.force_start_turn(as_int(t[1])); game.set_state(s); return "ok";
    }
    if (op == "land") {   // force player p to LAND on a space (applies rent/effects)
        if (t.size() < 3 || !valid_player(s, as_int(t[1]))) return err("args");
        s.force_land(as_int(t[1]), index_to_space(as_int(t[2]))); game.set_state(s); return "ok";
    }
    if (op == "givejailcard") {   // give player p a get-out-of-jail-free card (chance|cc)
        if (t.size() < 3 || !valid_player(s, as_int(t[1]))) return err("args");
        bool ok; DeckType d = deck_from_str(t[2], ok); if (!ok) return err("deck");
        s.force_give_get_out_of_jail_free_card(as_int(t[1]), d); game.set_state(s); return "ok";
    }
    if (op == "stackcard") {   // put a specific card on top of a deck (chance|cc)
        if (t.size() < 3) return err("args");
        bool ok; DeckType d = deck_from_str(t[1], ok); if (!ok) return err("deck");
        Card card; if (!card_from_key(t[2], card)) return err("card");
        s.force_stack_deck(d, DeckContainer{card}); game.set_state(s); return "ok";
    }
    if (op == "drawcard") {   // player p draws the top card of a deck (chance|cc) and applies it
        if (t.size() < 3 || !valid_player(s, as_int(t[1]))) return err("args");
        bool ok; DeckType d = deck_from_str(t[2], ok); if (!ok) return err("deck");
        s.force_draw_card(as_int(t[1]), d); game.set_state(s); return "ok";
    }

    return err("unknown '" + op + "'");
}

#endif  // NETTEST
