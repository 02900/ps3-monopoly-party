# Plan: PS3 port of `jemeador/monopoly` — Monopoly Party rules/controls (tiny3d)

## Context

We want a playable **Monopoly** on PS3. Rather than write the rules from scratch, we reuse
[`jemeador/monopoly`](https://github.com/jemeador/monopoly) — a headless C++17 rules **engine**
(3820 LOC, STL-only) explicitly "designed to be used for any interface." The **rules and controls**
should match **Monopoly Party (PS2)** as documented in the
[GameFAQs FAQ](https://gamefaqs.gamespot.com/ps2/561606-monopoly-party/faqs/31292).

This is the same **"rind swap"** reuse pattern as `ps3-ray-chess` / `ps3-openfight`: keep the engine
verbatim, rewrite only the platform I/O layer (render `GameState` + translate the pad into engine
`Input`s). Confirmed scope (via the user): **tiny3d** template (reliable text for a number/menu-heavy
board — avoids the RSXGL heavy-text corruption we hit in mega-mario), **Classic mode only** (turn-based,
the engine's native model), **hot-seat local** 2–4 human players (the engine has no AI; AI is a later
milestone). New repo, from the `02900/ps3-boilerplate-tiny3d` template.

## Why it fits

- **Engine is portable as-is.** Only STL deps (`vector/string/set/variant/random/queue/map/array`),
  **no threads** (the `<thread>/<mutex>/<future>` includes in `src/Game.h` are vestigial — grep shows
  zero real use, safe to strip), no filesystem except `DisplayStrings.cpp`'s `<fstream>` read of
  `src/share/strings.json` (bake it, like OpenFight's YAML→C++). Exclude only `EmscriptenBindings.cpp`.
- **Clean frontend seam.** `monopoly::IInterface` (`src/IInterface.h`): `get_setup()` → `GameSetup`,
  `poll()` → `std::queue<PlayerIndexInputPair>` of typed inputs, `update(GameState)` → latest state to
  render. The `Game` driver (`src/Game.h`) is synchronous: construct `Game(&interface)`, loop
  `game.process()` until `get_state().is_game_over()` — exactly the CLI's `src/app/cli/main.cpp` shape.
  Our PS3 frontend subclasses `IInterface` the same way `SimpleInterface` does.
- **Rules already ≈ "US Monopoly Rules" preset.** Engine defaults match the FAQ's US preset: 32 houses /
  12 hotels (`Bank` in `src/GameState.h`), auctions on, even-build, mortgages, jail w/ bail + get-out
  card, trades, bankruptcy. The rules "gap" is tiny (see Rules reconciliation).

## Target: new repo `ps3-monopoly-party` (org 02900)

Scaffold from `ps3-boilerplate-tiny3d` (Makefile + Dockerized `ghcr.io/02900/ps3-toolchain-tiny3d` +
CI + PKG + `ttf_render.c` text + `audio.c` MikMod + `.claude/skills` submodule already wired).

### Layout
```
source/
  engine/            # jemeador/monopoly src/*.h + *.cpp, VERBATIM except:
                     #   - drop EmscriptenBindings.cpp
                     #   - strip vestigial threading includes from Game.h
                     #   - DisplayStrings: read baked strings (no <fstream>)
  strings_data.h     # baked src/share/strings.json (generated)
  PS3Interface.h/.cpp# IInterface impl: get_setup/poll/update; pad→Input queue
  ui/                # tiny3d rendering: board, tokens, HUD, menus, cards, auction/trade
  Main.cpp           # PSL1GHT init (from boilerplate) + Game loop + per-frame poll/render
  ttf_render.c audio.c ...   # kept from template
ideas/monopoly-ps3-port.md   # this plan, moved into the repo
```

## Frontend design (the only new code)

**`PS3Interface : monopoly::IInterface`** — the bridge, mirroring `SimpleInterface`:
- `get_setup()` → `GameSetup{ playerCount = chosen 2..4, startingFunds = 1500 }` (+ deterministic
  `seed` from the RTC at boot).
- `poll()` → drains a member `std::queue` that the **main loop** fills from pad edges (never touched off
  the main thread; single-threaded like the engine).
- `update(GameState)` → stores the latest `GameState` into a member the renderer reads each frame.

**Main loop** (`Main.cpp`, tiny3d frame): read pad → map to the *context-appropriate* `Input` (see
Controls) → push into `PS3Interface` queue → call `game.process()` → render `interface.latest_state()`.
`process()` returns when player intervention is needed, so the loop is naturally turn/prompt driven.

**Rendering** with tiny3d + `ttf_render` (all text via FreeType→texture — the reason we chose tiny3d):
1. **Board**: 11×11 classic layout, 40 spaces from the FAQ property list (names/colors/prices baked as a
   static table; the engine already owns the authoritative economic data via `Board.h`/`Cards.h`).
2. **Tokens**: one per active player at their `GameState` position; hop animation on move.
3. **HUD**: current player, everyone's cash, dice result, prompts.
4. **Menus** driven by `GameState`'s pending decision: buy/auction, build/sell, mortgage, jail, trade.
5. **Property Management window** (L2), **Title Deed** popup (Select), **auction** and **trade** panels.

## Controls (PS3, mapped from the Monopoly Party FAQ)

FAQ PS2 → PS3 (Cross=roll/confirm since FAQ "Press X to roll"; "1=YES"→Square as secondary confirm):
| Action | Button |
|---|---|
| Roll dice / Confirm "YES" / Select highlighted | **Cross** |
| Cancel / back | **Circle** |
| Change camera view (4 views) | **Triangle** |
| Property Management window on/off | **L2** |
| Special token animation | **R2** |
| Pause menu | **Start** |
| Title Deed (when property highlighted) | **Select** |
| Navigate / adjust options | **D-pad / left stick** |
| Move & zoom camera (free-cam) | **right stick** |

Menu/flow feel (roll → move → prompt → buy/auction → end turn; pass controller between turns) follows
the FAQ's "Playing the Game" and "In-Game Controls" sections.

## Rules reconciliation to Monopoly Party "US Monopoly Rules"

Engine defaults already satisfy the preset; only verify/tweak in `GameSetup`/engine constants — do **not**
fork the rules:
- **Houses 32 / Hotels 12**, **even build on**, **auction on**, **Free Parking neutral** (no jackpot),
  **double rent on full color group** — already the engine's behavior/defaults.
- **Income Tax**: FAQ US preset = "Fixed Fee ($200) or 10%". Check `Cards.cpp`/space handling; if the
  engine hard-codes one, expose the player choice (or pick $200 flat) — smallest possible change.
- **Landing on Go**: normal $200 (default). **Jail**: $50 bail / doubles / card — matches engine.
- Party-only "Custom Games" knobs (jackpot, delayed start, immunities, punishment) are **out of scope**
  for v1 (Classic + US preset only); note them as future config.

## Build gating

- Makefile: add `source/engine` (and `source`) to `SOURCES`; C++17 (`CXXFLAGS += -std=gnu++17`); link
  stays the tiny3d set (libstdc++ links via the C++ driver, as in ray-chess). No new libs.
- Build via `DOCKER_DEFAULT_PLATFORM=linux/amd64 ./scripts/build.sh` (Apple Silicon); ppu-gcc 7.2
  transient segfaults are retried by build.sh.

## Milestones (each: builds green + runs on RPCS3)

- **M0 — Engine compiles on PS3.** Scaffold repo; vendor engine (strip threading includes, bake
  `strings.json`, drop Emscripten); a throwaway `Main.cpp` runs a scripted game headless (TTY log) to
  prove the STL engine builds & runs under ppu-gcc. *Highest-risk step, front-loaded.*
- **M1 — Board + roll loop.** Draw board + tokens + cash HUD from `GameState`; Cross → `RollInput` →
  token moves; pass-turn between hot-seat players.
- **M2 — Buy / auction.** Land on unowned → buy (`BuyPropertyInput`) or decline→auction
  (`AuctionPropertyInput` + bidding UI: `BidInput`/`DeclineBidInput`). Rent auto-collected on owned.
- **M3 — Property management (L2).** Mortgage/unmortgage, build/sell houses & hotels
  (`BuyBuildingInput`/`SellBuildingInput`/`SellAllBuildingsInput`), Title Deed popup (Select).
- **M4 — Jail, cards, taxes, endgame.** Jail (`PayBailInput`/`UseGetOutOfJailFreeCardInput`/roll), Chance
  & Community Chest card display, tax spaces, bankruptcy (`ResignInput`) + win detection.
- **M5 — Trading + polish.** Trade UI (`OfferTradeInput`/`DeclineTradeInput`), MikMod audio, pause menu,
  camera views (Triangle), README + skills note.

## Verification (end-to-end)

1. `DOCKER_DEFAULT_PLATFORM=linux/amd64 ./scripts/build.sh` → `src.self` green (M0 proves engine builds).
2. Run on RPCS3 (`rpcs3 src.self`); play a full hot-seat game: roll → move → **buy** a property; decline
   another → **auction** it; build **houses/hotels** on a monopoly; **mortgage**; land in **jail** and
   pay bail; draw **Chance/Community Chest**; pay **rent/tax**; drive a player to **bankruptcy** → win
   screen. Assert each against the FAQ rules.
3. Optionally reuse the `ps3-ray-chess` **TCP nettest harness** pattern (mutex/cond command server +
   pytest) to script rule checks — a later, optional milestone, not v1.
4. CI (build.yml/lint.yml) green. Commits end with
   `Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>`; bump the `.claude/skills`
   submodule pointer; add a "reuse a headless STL engine as a PS3 frontend (IInterface seam)" note to the
   shared skills if it generalizes the ray-chess/openfight pattern further.

## Notes / risks

- **Engine build under ppu-gcc 7.2** (C++17 `<variant>`, `std::visit`) is the main unknown → M0 front-loads
  it. Fallback if `<variant>` misbehaves: the engine's `Input`/`Card` variants are small and could be
  lowered, but try verbatim first.
- **No AI**: v1 is hot-seat only; AI opponents are a clean future milestone (implement a second
  `IInterface`-driver that generates inputs from `GameState`).
- Engine stays **verbatim** — all Monopoly-Party-specific behavior lives in the frontend + `GameSetup`,
  keeping upstream rules intact and re-syncable.
- Attribution/licensing: check `jemeador/monopoly` `LICENSE` and credit upstream in README (as ray-chess
  credits its origin).
