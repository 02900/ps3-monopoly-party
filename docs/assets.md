# Image assets — spec for ComfyUI

The Clay UI is built with **image slots** that currently render as flat colour
placeholders. This doc is the shopping list to generate the real art (ComfyUI), then
optimise and drop in. Each asset lists its **id** (matches the `UI_IMG_*` registry in
`source/ui/theme.{c,h}`), purpose, pixel size, format, and a prompt seed.

## Art direction

- **Cohesive board-game look**: clean, semi-flat vector/illustrative style, bold
  readable shapes, soft studio lighting — think a modern Hasbro re-skin, not photoreal.
- **Palette**: tie to the property-group colours already in the game (brown, light
  blue, magenta, orange, red, yellow, green, blue) over a deep navy UI (`#0E1A2E`) with
  a gold accent (`#E8B84B`). Keep it consistent across every asset.
- **Readable at 720p** on a TV from the couch: high contrast, no fine text baked in.
- **Transparency**: tokens, icons and the logo are **PNG RGBA with a real alpha
  channel** (no white/black matte — the renderer alpha-blends). Backgrounds are opaque.
- **Framing**: tokens are a single centred object, ~10% padding, facing camera / 3/4
  view, consistent scale and eye-level across a theme so they read as a set.

## How the art gets in

- Small/UI art (logo, tokens, icons) → drop the PNG in `data/` and it's embedded by the
  Makefile's `bin2o` rule (`data/foo.png` → symbol `foo_png` / `foo_png_size`). Then wire
  it in `ui_images_load()` (`source/ui/theme.c`) via
  `ya2d_loadPNGfromBuffer(foo_png, foo_png_size)` into the matching `UI_IMG_*` slot.
- XMB presentation art (icon, background, startup sound) → drop in `pkgfiles/` (they're
  packaged into the PKG, not embedded in the ELF).
- **Keep textures small** — ya2d/pngdec decode into RSX memory; see budgets below.

## Assets

### Branding / UI (`data/`, embedded)

| id | file | purpose | size (px) | format | notes / prompt seed |
|---|---|---|---|---|---|
| `UI_IMG_LOGO` | `logo.png` | title-screen logo | 512×160 | PNG RGBA | "MONOPOLY PARTY" wordmark, gold on transparent, playful 3D-ish letters, subtle bevel |
| `UI_IMG_BG` | `menu_bg.png` | menu backdrop / hero key art | 1280×720 | PNG RGB | **cinematic game-menu key art** — a rich Monopoly-style board table receding in 3/4 view into soft focus, warm golden rim-light, scattered metal tokens + houses/hotels + dice catching the light, teal→deep-navy graded vignette, gentle bokeh; keep the **upper-centre calmer/emptier** for the logo and the **mid band** un-busy for the menu panel. Detailed and moody is good — the engine composites a ~40% dark navy scrim on top, so it won't fight the text. Avoid text/wordmarks in the art. |
| — | `board_face.png` | (future) board centre art | 512×512 | PNG RGBA | classic Monopoly centre: diagonal "MONOPOLY PARTY" banner, corner flourishes |
| — | `dice_1..6.png` | dice faces (optional; drawn now) | 48×48 | PNG RGBA | white die, rounded corners, black pips, soft shadow |
| — | `icon_jail/tax/rr/util.png` | space icons (optional) | 40×40 | PNG RGBA | simple flat glyphs matching the palette |

### Token sprites (`data/`, embedded) — `UI_IMG_TOKEN + theme*6 + token`

30 sprites, **128×128 PNG RGBA**, one object each, consistent set per theme. Names match
`ui_token_name()` in `source/ui/theme.c`:

| theme | tokens (6) |
|---|---|
| Classic | Car · Boot · Hat · Iron · Battleship · Dog |
| Fantasy | Dwarf · Wizard · Troll · Knight · Orc · Elf |
| Sci-Fi | Robot · Rover · Fighter · Alien · Astronaut · UFO |
| Ancient | Boat · Warship · Urn · Sandal · Chariot · Camel |
| Prehistoric | Caveman · Triceratops · Archaeopteryx · Mammoth · Sabertooth · Cavewoman |

Prompt seed per token: *"a single {token} game piece, glossy pewter/enamel miniature,
centred, 3/4 view, transparent background, soft studio light, consistent scale, clean
silhouette, {theme} styling"*. Keep all six of a theme in one style/material so they read
as a family. Suggested file names: `tok_classic_car.png`, `tok_fantasy_wizard.png`, …

### PS3 PKG presentation (`pkgfiles/`)

| file | purpose | size (px) | format | notes |
|---|---|---|---|---|
| `ICON0.PNG` | XMB game icon (exists; redo) | 320×176 | PNG RGB | logo + a token or board corner, readable tiny |
| `PIC1.PNG` | XMB background | 1920×1080 | PNG RGB | the menu backdrop, full-bleed, darker toward centre |
| `PIC0.PNG` | (optional) XMB overlay art | 1000×560 | PNG RGBA | — |
| `SND0.AT3` | (optional) XMB startup jingle | — | ATRAC3 | short loop; convert with an AT3 tool |

## Optimisation

- **Tokens ≤ 128², board ≤ 512², logo ≤ 512×160.** ya2d/pngdec decode to RSX memory; the
  full 30-token set at 128²·RGBA ≈ 2 MB decoded — fine, but don't oversize.
- Prefer **power-of-two-ish** dimensions; strip metadata; 8-bit RGBA (no 16-bit).
- Quantise/crush PNGs (pngquant/oxipng) — smaller ELF, faster load. The alpha channel
  must survive (no matte).
- Only embed what a screen shows; the 30 tokens are the biggest budget — if memory is
  tight, load a theme's 6 tokens lazily when that theme is picked.
