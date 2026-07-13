"""Declarative asset manifest for PS3 Monopoly Party.

Built from docs/assets.md. Each Asset describes one output PNG: its target
file (relative to the repo root), pixel size, whether it needs a real alpha
channel, the preferred generation engine, and a prompt.

Token names come straight from TOKENS[][] in source/ui/theme.c so the file
names line up with ui_token_name() / ui_img_token().
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Literal

Engine = Literal["openai", "grok"]

# ── Shared art-direction preamble (docs/assets.md "Art direction") ──────────
ART_DIRECTION = (
    "Cohesive board-game look: clean semi-flat illustrative/vector style, bold "
    "readable shapes, soft studio lighting, a modern Hasbro re-skin, not photoreal. "
    "Palette tied to Monopoly property groups (brown, light blue, magenta, orange, "
    "red, yellow, green, blue) over deep navy UI #0E1A2E with a gold accent #E8B84B. "
    "High contrast, readable at 720p on a TV from the couch, no fine text baked in."
)

TRANSPARENT_NOTE = (
    "Transparent background, single centred subject, real alpha (no white or black "
    "matte), about 10% padding."
)

# ── Full-screen menu backgrounds (composited under a ~66% navy scrim in-engine) ─
# These are hero/key-art plates, NOT the flat "non-distracting" felt — the engine
# darkens them, so they can be rich. Each keeps a specific area calm for the UI.
BG_TAIL = (
    "Modern stylised 3D game render, clean bold shapes, cinematic warm key light with "
    "soft rim light and gentle depth-of-field bokeh. Palette tied to the Monopoly "
    "property groups (brown, light blue, magenta, orange, red, yellow, green, blue) "
    "over deep navy #0E1A2E with a gold #E8B84B accent. Rich and premium but not busy, "
    "readable at 720p on a TV from the couch. Absolutely no text, letters, numbers or "
    "words anywhere in the image. 16:9."
)


@dataclass
class Asset:
    id: str                       # logical id (also --only selector)
    out: str                      # path relative to repo root
    width: int
    height: int
    rgba: bool                    # True -> keep alpha; False -> flatten to RGB
    prompt: str
    engine: Engine = "openai"
    optional: bool = False
    background_color: str = "#0E1A2E"  # matte used when flattening RGB assets


# ── Token themes (must match source/ui/theme.c TOKENS[][]) ──────────────────
# (theme_slug, [(token_display, file_token_slug), ...])
TOKEN_THEMES: list[tuple[str, str, list[tuple[str, str]]]] = [
    ("classic", "Classic", [
        ("Car", "car"), ("Boot", "boot"), ("Hat", "hat"),
        ("Iron", "iron"), ("Battleship", "battleship"), ("Dog", "dog"),
    ]),
    ("fantasy", "Fantasy", [
        ("Dwarf", "dwarf"), ("Wizard", "wizard"), ("Troll", "troll"),
        ("Knight", "knight"), ("Orc", "orc"), ("Elf", "elf"),
    ]),
    ("scifi", "Sci-Fi", [
        ("Robot", "robot"), ("Rover", "rover"), ("Fighter", "fighter"),
        ("Alien", "alien"), ("Astronaut", "astronaut"), ("UFO", "ufo"),
    ]),
    ("ancient", "Ancient", [
        ("Boat", "boat"), ("Warship", "warship"), ("Urn", "urn"),
        ("Sandal", "sandal"), ("Chariot", "chariot"), ("Camel", "camel"),
    ]),
    ("prehistoric", "Prehistoric", [
        ("Caveman", "caveman"), ("Triceratops", "triceratops"),
        ("Archaeopteryx", "archaeopteryx"), ("Mammoth", "mammoth"),
        ("Sabertooth", "sabertooth"), ("Cavewoman", "cavewoman"),
    ]),
]

DICE_FACES = [1, 2, 3, 4, 5, 6]
SPACE_ICONS = [
    ("jail", "a jail cell door with iron bars"),
    ("tax", "a diamond ring / money bag tax symbol"),
    ("rr", "a locomotive / railroad icon"),
    ("util", "a lightbulb / water tap utility icon"),
]


def _token_prompt(theme_display: str, token_display: str) -> str:
    return (
        f"A single {token_display} game piece, glossy pewter/enamel miniature, "
        f"centred, 3/4 view, soft studio light, consistent scale, clean silhouette, "
        f"{theme_display} styling. {TRANSPARENT_NOTE} {ART_DIRECTION}"
    )


def build_manifest() -> list[Asset]:
    assets: list[Asset] = []

    # ── Branding / UI (data/, embedded) ─────────────────────────────────────
    assets.append(Asset(
        id="logo", out="data/logo.png", width=512, height=160, rgba=True,
        prompt=(
            "'MONOPOLY PARTY' wordmark logo, gold (#E8B84B) playful 3D-ish letters "
            "with a subtle bevel, no background. " + TRANSPARENT_NOTE + " " + ART_DIRECTION
        ),
        engine="openai",
    ))
    assets.append(Asset(
        id="menu_bg", out="data/menu_bg.png", width=1280, height=720, rgba=False,
        prompt=(
            "Menu backdrop: dark navy felt with a soft vignette, faint Monopoly board "
            "motifs in the corners, non-distracting, no text. " + ART_DIRECTION
        ),
        engine="grok",
    ))
    # Title screen: logo sits upper-centre, "PRESS START" lower-centre -> keep the
    # whole vertical centre column calm; put the drama in the lower half + sky.
    assets.append(Asset(
        id="bg_title", out="data/bg_title.png", width=1280, height=720, rgba=False,
        prompt=(
            "Cinematic title-screen key art for a Monopoly-style party board game. A "
            "luxurious game board sweeps across the lower half in dramatic 3/4 "
            "perspective; glossy pewter tokens (top hat, race car, terrier dog, "
            "battleship), a pair of dice mid-tumble, tiny green houses and red hotels "
            "catching the light; an art-deco city skyline softly bokeh'd behind; a warm "
            "golden glow and floating sparkles rising through a teal-to-deep-navy sky. "
            "Keep the upper-centre and the vertical centre calm and open for a logo and "
            "a call-to-action. " + BG_TAIL
        ),
        engine="grok",
    ))
    # Main menu: a ~460px UI panel sits dead-centre -> darken the centre, push the
    # props to the left/right edges and lower corners.
    assets.append(Asset(
        id="bg_menu", out="data/bg_menu.png", width=1280, height=720, rgba=False,
        prompt=(
            "Main-menu background for a Monopoly-style party board game. A rich "
            "board-game table seen from a low 3/4 angle; glossy metal tokens, dice, "
            "houses and hotels clustered along the left and right edges and the lower "
            "corners under a warm golden side light; a strong deep-navy vignette that "
            "darkens toward the middle so a UI panel can sit there. The central vertical "
            "third stays calm, dark and empty. " + BG_TAIL
        ),
        engine="grok",
    ))
    # How to Play: a wide rules panel fills the centre -> a tidy flat-lay hugging the
    # edges, big empty dark centre.
    assets.append(Asset(
        id="bg_howto", out="data/bg_howto.png", width=1280, height=720, rgba=False,
        prompt=(
            "Calm rules-screen background for a Monopoly-style board game. An elegant "
            "flat-lay hugging the bottom and side edges: a board corner, neat fanned "
            "title-deed cards, two dice, a few houses and hotels and a couple of metal "
            "tokens on deep-navy felt, softly lit, tidy and uncluttered, gentle "
            "vignette. Keep the whole centre large, dark and empty for a panel of rules "
            "text. " + BG_TAIL
        ),
        engine="grok",
    ))
    assets.append(Asset(
        id="board_face", out="data/board_face.png", width=512, height=512, rgba=True,
        prompt=(
            "Classic Monopoly board centre art: a diagonal 'MONOPOLY PARTY' banner with "
            "corner flourishes. " + TRANSPARENT_NOTE + " " + ART_DIRECTION
        ),
        engine="openai", optional=True,
    ))

    for face in DICE_FACES:
        assets.append(Asset(
            id=f"dice_{face}", out=f"data/dice_{face}.png", width=48, height=48, rgba=True,
            prompt=(
                f"A single white die showing the {face}-pip face, rounded corners, black "
                f"pips, soft drop shadow, front view. " + TRANSPARENT_NOTE + " " + ART_DIRECTION
            ),
            engine="openai", optional=True,
        ))

    for slug, desc in SPACE_ICONS:
        assets.append(Asset(
            id=f"icon_{slug}", out=f"data/icon_{slug}.png", width=40, height=40, rgba=True,
            prompt=(
                f"A simple flat glyph icon of {desc}, matching the game palette, bold "
                f"readable shape. " + TRANSPARENT_NOTE + " " + ART_DIRECTION
            ),
            engine="openai", optional=True,
        ))

    # ── Token sprites (data/, embedded) ─────────────────────────────────────
    for theme_slug, theme_display, tokens in TOKEN_THEMES:
        for token_display, token_slug in tokens:
            assets.append(Asset(
                id=f"tok_{theme_slug}_{token_slug}",
                out=f"data/tok_{theme_slug}_{token_slug}.png",
                width=128, height=128, rgba=True,
                prompt=_token_prompt(theme_display, token_display),
                engine="openai",
            ))

    # ── PS3 PKG presentation (pkgfiles/) ────────────────────────────────────
    assets.append(Asset(
        id="ICON0", out="pkgfiles/ICON0.PNG", width=320, height=176, rgba=False,
        prompt=(
            "XMB game icon: the 'MONOPOLY PARTY' logo with a token or board corner, "
            "readable when tiny, no small text. " + ART_DIRECTION
        ),
        engine="grok",
    ))
    assets.append(Asset(
        id="PIC1", out="pkgfiles/PIC1.PNG", width=1920, height=1080, rgba=False,
        prompt=(
            "XMB full-bleed background: the menu backdrop, dark navy felt darker toward "
            "the centre, faint board motifs, no text. " + ART_DIRECTION
        ),
        engine="grok",
    ))
    assets.append(Asset(
        id="PIC0", out="pkgfiles/PIC0.PNG", width=1000, height=560, rgba=True,
        prompt=(
            "XMB overlay art: 'MONOPOLY PARTY' logo with a couple of tokens, transparent "
            "background. " + TRANSPARENT_NOTE + " " + ART_DIRECTION
        ),
        engine="openai", optional=True,
    ))

    return assets


MANIFEST: list[Asset] = build_manifest()
