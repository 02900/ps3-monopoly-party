"""TCP client for the Monopoly PS3 e2e test harness.

The game (built with `./scripts/build-test.sh`) runs a line-based TCP command server
on port 9010. Each request is one line ("ping", "state", "roll", "buy", ...); each
reply is one line. This client is the thin transport used by the pytest suite.

Board references are the space index 0..39 (GO=0, counter-clockwise).
"""

import os
import socket

DEFAULT_PORT = 9010


class PS3Client:
    def __init__(self, host=None, port=DEFAULT_PORT, timeout=5.0):
        self.host = host or os.environ.get("PS3_IP") or "127.0.0.1"
        self.port = int(os.environ.get("PS3_PORT", port))
        self.timeout = timeout
        self.sock = None
        self._buf = b""

    # -- connection -----------------------------------------------------------
    def connect(self):
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(self.timeout)
        s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        s.connect((self.host, self.port))
        self.sock = s
        self._buf = b""
        return self

    def close(self):
        if self.sock:
            self.sock.close()
            self.sock = None

    def __enter__(self):
        return self.connect()

    def __exit__(self, *exc):
        self.close()

    # -- request/response -----------------------------------------------------
    def send(self, cmd):
        """Send one command line and return the single-line reply (stripped)."""
        if not self.sock:
            self.connect()
        self.sock.sendall((cmd.strip() + "\n").encode("ascii"))
        while b"\n" not in self._buf:
            chunk = self.sock.recv(4096)
            if not chunk:
                raise ConnectionError("server closed the connection")
            self._buf += chunk
        line, self._buf = self._buf.split(b"\n", 1)
        return line.decode("ascii", "replace").strip()

    @staticmethod
    def _kv(reply):
        out = {}
        for tok in reply.split():
            if "=" in tok:
                k, v = tok.split("=", 1)
                out[k] = v
        return out

    # -- queries --------------------------------------------------------------
    def ping(self):
        return self.send("ping")

    def state(self):
        return self._kv(self.send("state"))

    def funds(self, p):
        return int(self.send(f"funds {p}"))

    def net(self, p):
        return int(self.send(f"net {p}"))

    def pos(self, p):
        """Return (index, name) of player p's board position."""
        idx, name = self.send(f"pos {p}").split(" ", 1)
        return int(idx), name

    def pos_index(self, p):
        return self.pos(p)[0]

    def owner(self, space):
        return int(self.send(f"owner {space}"))

    def mortgaged(self, space):
        return self.send(f"mortgaged {space}") == "1"

    def buildings(self, space):
        return int(self.send(f"buildings {space}"))

    def dice(self):
        a, b, s = self.send("dice").split()
        return int(a), int(b), int(s)

    def landed(self):
        return self._kv(self.send("landed"))

    def auctioninfo(self):
        r = self.send("auctioninfo")
        return None if r.startswith("err") else self._kv(r)

    def deeds(self, p):
        r = self.send(f"deeds {p}")
        return [] if r == "(none)" or r.startswith("err") else [int(x) for x in r.split()]

    def jailed(self, p):
        return int(self.send(f"jailed {p}"))

    # -- driving --------------------------------------------------------------
    def newgame(self):
        return self.send("newgame")

    def roll(self):
        return self.send("roll")

    def step(self):
        return self.send("step")

    def buy(self):
        return self.send("buy")

    def auction(self):
        return self.send("auction")

    def bid(self, amount):
        return self.send(f"bid {amount}")

    def bid_pass(self):
        return self.send("pass")

    def endturn(self):
        return self.send("endturn")

    def mortgage(self, space):
        return self.send(f"mortgage {space}")

    def unmortgage(self, space):
        return self.send(f"unmortgage {space}")

    def build(self, space):
        return self.send(f"build {space}")

    def sell(self, space):
        return self.send(f"sell {space}")

    def paybail(self):
        return self.send("paybail")

    def usejail(self):
        return self.send("usejail")

    # -- deterministic setup (force_*) ----------------------------------------
    def setfunds(self, p, amount):
        return self.send(f"setfunds {p} {amount}")

    def setpos(self, p, space):
        return self.send(f"setpos {p} {space}")

    def givedeed(self, p, space):
        return self.send(f"givedeed {p} {space}")

    def offer(self, p, space):
        return self.send(f"offer {p} {space}")

    def gojail(self, p):
        return self.send(f"gojail {p}")

    def land(self, p, space):
        return self.send(f"land {p} {space}")

    # -- flow helper ----------------------------------------------------------
    def drive_to_buy(self, max_rolls=12):
        """Advance the game (resolving non-decision phases) until the active player
        faces a buy decision. Returns True if a buy phase was reached."""
        for _ in range(max_rolls):
            ph = self.phase()
            if ph == "buy":
                return True
            if ph == "roll":
                self.roll()
            elif ph == "turnend":
                self.endturn()
            elif ph == "manage":
                self.step()
            elif ph == "bids":
                self.bid_pass()
            else:
                return False
        return False

    # -- helpers --------------------------------------------------------------
    def phase(self):
        return self.state().get("phase")

    def active(self):
        return int(self.state()["active"])

    def controlling(self):
        return int(self.state()["controlling"])

    def assert_ok(self, reply, ctx=""):
        assert reply == "ok", f"expected ok, got {reply!r} {ctx}"
        return reply


if __name__ == "__main__":
    with PS3Client() as c:
        print("ping ->", c.ping())
        print("state ->", c.state())
        print("funds0 ->", c.funds(0))
