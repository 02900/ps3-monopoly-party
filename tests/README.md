# Monopoly PS3 — e2e test harness

Drive the running game over TCP from Python and assert the rules behave correctly —
automated verification instead of eyeballing every rule on the console.

## How it works

The game, built with the **NETTEST** flag, runs a line-based TCP command server on
**port 9010** (`source/nettest/`). The Python client (`harness.py`) connects and sends
one command per line; the game replies with one line. Commands either **drive** the
game (`roll`, `buy`, `bid`, …) or **query** it (`state`, `funds`, `owner`, …). All game
state is touched only on the main thread, so this can't race the renderer. The dice RNG
is seeded from a fixed string, so a given command sequence is **deterministic**.

## 1. Build the test binary

```bash
DOCKER_DEFAULT_PLATFORM=linux/amd64 ./scripts/build-test.sh        # -> src.self
```

`build-test.sh` cleans first on purpose: the Makefile's incremental build doesn't track
the `NETTEST` flag, so a stale object compiled without it would leave the server dormant.
The normal `./scripts/build.sh` is unchanged and opens no port.

## 2. Boot it

```bash
# --no-gui forces RPCS3 to boot-and-run the self immediately (a plain `rpcs3 src.self`
# may just sit at the game list). RPCS3 exposes the guest's listener on the host.
/Applications/RPCS3.app/Contents/MacOS/rpcs3 --no-gui "$PWD/src.self" &
```

RPCS3's Network status must be **Connected** (Configuration → Network). Wait until the
server answers before running tests:

```bash
until python3 -c "import socket;s=socket.socket();s.settimeout(2);s.connect(('127.0.0.1',9010));s.sendall(b'ping\n');print(s.recv(8))" 2>/dev/null; do sleep 2; done
```

## 3. Run the suite

```bash
cd tests
python3 -m venv .venv && source .venv/bin/activate && pip install pytest
PS3_IP=127.0.0.1 python -m pytest -v
```

> Use a venv (or `python3 -m pytest`), not a bare system `pytest`: on macOS the system
> `pip` is externally managed (PEP 668). The suite skips (not fails) if the game is
> unreachable.

## Command reference

Board references are the **space index 0..39** (GO=0, counter-clockwise; see `board.py`).

| Command | Reply |
|---|---|
| `ping` | `pong` |
| `state` | `phase=roll/buy/bids/manage/turnend/… active=<i> controlling=<i> gameover=0/1 remaining=<n>` |
| `funds <p>` / `net <p>` | integer |
| `pos <p>` | `<idx> <name>` |
| `owner <sp>` | owner player index, or `-1` (bank) |
| `mortgaged <sp>` / `buildings <sp>` | `0/1` / building level |
| `dice` | `<a> <b> <sum>` |
| `landed` | `space=<i> property=0/1 [price=<n> owner=<i>]` (active player's square) |
| `auctioninfo` | `property=<i> name=<..> highbid=<n> nextbidder=<i>` or `err noauction` |
| `deeds <p>` | space indices owned, or `(none)` |
| `jailed <p>` | turns remaining in jail |
| `newgame` | `ok` — fresh game |
| `roll` / `step` | `ok` — roll for the controller / advance auto phases |
| `buy` / `auction` | `ok` — buy the landed property / decline → auction |
| `bid <n>` / `pass` | `ok` — bid / decline in an auction |
| `endturn` | `ok` |
| `mortgage/unmortgage/build/sell <sp>` | `ok` |
| `paybail` / `usejail` | `ok` |
| `setfunds <p> <n>` / `setpos <p> <sp>` | `ok` — deterministic setup (force_*) |
| `givedeed <p> <sp>` / `gojail <p>` / `land <p> <sp>` | `ok` — force ownership / jail / a landing (rent) |

## Notes

- The `[nettest]` diagnostics print to the PS3 TTY → RPCS3's log, not to stdout.
- This is opt-in tooling: the shipped build (plain `build.sh`) contains none of it.
- `land <p> <sp>` applies a landing's effects (e.g. rent) without needing the dice to
  cooperate; `setpos` only moves the token. Prefer driving through `roll`/`drive_to_buy`
  for realism where the dice can reach the target.
