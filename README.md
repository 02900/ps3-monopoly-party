# Monopoly Party — PS3

A PS3 homebrew **Monopoly** for 2–4 players (hot-seat), with the rules and controls of
**Monopoly Party (PS2)**. Built on the [Tiny3D](https://github.com/wargio/tiny3d) 2D
renderer via the Dockerized [`ps3-toolchain-tiny3d`](https://github.com/02900/ps3-toolchain)
image.

The game **rules engine is reused essentially verbatim** from
[`jemeador/monopoly`](https://github.com/jemeador/monopoly) (MIT) — a headless C++17
Monopoly engine. Only the PS3 layer (rendering, pad input, the TCP test server) is new.
See [`source/engine/UPSTREAM.md`](source/engine/UPSTREAM.md) for the (minimal) port changes.

## What's implemented

- **Board & turns** — the 40-space board (property squares tinted by colour group), a
  token per player, hot-seat turn order (Red → Green → Blue → Yellow).
- **Buy / auction** — land on an unowned property to buy it, or decline to send it to a
  live bidding auction; owned squares show an owner marker and collect rent.
- **Property management (L2)** — mortgage / unmortgage, and build / sell houses & hotels
  (full-group ownership and even-building enforced by the engine).
- **Jail** — pay the $50 bail, use a Get-Out-of-Jail-Free card, or roll for doubles.
- **Chance & Community Chest**, income & luxury **taxes**, **debt settlement**
  (raise cash via L2 or declare **bankruptcy**), and the **win** screen.
- **Trading** — offer one of your deeds to another player for cash; they accept or decline.
- Pause menu (Start), MikMod sound, and a per-session randomized dice seed.

Not in scope: the simultaneous *Party* mode and AI opponents (hot-seat only) — see
[`ideas/monopoly-ps3-port.md`](ideas/monopoly-ps3-port.md).

## Controls

| Button | Action |
|---|---|
| **Cross (X)** | Roll dice · confirm · buy · accept trade · end turn |
| **Circle (O)** | Decline / auction · cancel · pass · declare bankruptcy |
| **L2** | Open the property-management window (mortgage / build) |
| **Triangle** | Open the trade builder (on your turn) · in jail: use a jail card |
| **Square** | In management: sell · in trade: change target player · in jail: pay bail |
| **D-pad** | Navigate menus, adjust bids / cash |
| **Start** | Pause menu (resume / new game) |

## Build & run

```bash
# On Apple Silicon prefix with DOCKER_DEFAULT_PLATFORM=linux/amd64
./scripts/build.sh                 # -> src.self (Dockerized toolchain, no local install)
./scripts/build.sh pkg             # -> installable PKG

# RPCS3: boot the self directly (Configuration → Network can stay default)
/Applications/RPCS3.app/Contents/MacOS/rpcs3 src.self
```

## Automated tests

An opt-in TCP command server + a Python/pytest suite verify the rules end-to-end over
the network (deterministic on a fixed seed). See [`tests/README.md`](tests/README.md).

```bash
./scripts/build-test.sh            # src.self with the test server (port 9010)
cd tests && python3 -m venv .venv && . .venv/bin/activate && pip install pytest
PS3_IP=127.0.0.1 python -m pytest  # movement, buy/auction, management, jail/cards/tax, trade
```

## Credits & license

- Rules engine: [jemeador/monopoly](https://github.com/jemeador/monopoly) — MIT,
  © 2021 Jeremy Meador. JSON: [nlohmann/json](https://github.com/nlohmann/json) — MIT.
- Rules/controls reference: the *Monopoly Party* PS2 FAQ by Menji (GameFAQs).
- MONOPOLY is a trademark of Hasbro; this is a non-commercial homebrew project.
- PS3 port code under this repo's [`LICENSE`](LICENSE).
