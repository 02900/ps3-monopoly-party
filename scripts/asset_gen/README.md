# Asset generator (ComfyUI Cloud)

Generates the image assets described in [`docs/assets.md`](../../docs/assets.md)
by sending **Grok** (`GrokImageNode`) and **OpenAI GPT Image 2**
(`OpenAIGPTImage1`) workflows to **ComfyUI Cloud** (`cloud.comfy.org`), then
post-processing each result to the exact size/format and writing it into
`data/` (embedded assets) or `pkgfiles/` (PKG presentation).

Both Grok and OpenAI are comfy.org **API nodes** (cloud-only) — they proxy to
xAI/OpenAI and require a `COMFY_CLOUD_API_KEY`.

## Setup

The system Python on macOS (Homebrew) is externally managed, so use a venv.
One already lives at `scripts/asset_gen/.venv` (gitignored); to (re)create it:

```sh
python3 -m venv scripts/asset_gen/.venv
scripts/asset_gen/.venv/bin/pip install -r scripts/asset_gen/requirements.txt
export COMFY_CLOUD_API_KEY=sk-...   # from https://platform.comfy.org/login
```

Optional PNG crushers (used automatically if on `PATH`): `oxipng`, `pngquant`
(e.g. `brew install oxipng pngquant`).

## Usage

Run from the repo root, using the venv's Python:

```sh
PY=scripts/asset_gen/.venv/bin/python

# Preview the full plan, no API calls (also works with plain python3):
$PY scripts/gen_assets.py --dry-run

# Single asset:
$PY scripts/gen_assets.py --only logo

# One token theme:
$PY scripts/gen_assets.py --theme classic

# Everything (required only):
$PY scripts/gen_assets.py --all

# Everything incl. optional (board_face, dice, icons, PIC0):
$PY scripts/gen_assets.py --all --include-optional

# Force an engine / fixed seed:
$PY scripts/gen_assets.py --only menu_bg --engine grok --seed 42
```

Existing files are skipped unless `--force`. A record of what produced each
asset is written to `scripts/asset_gen/.cache/<id>.json`.

## Transparency

Tokens, logo and icons need a real alpha channel. They are generated with
OpenAI **gpt-image-1** using `background: "transparent"` (gpt-image-2 rejects
transparency). gpt-image-1 only accepts three fixed sizes (1024², 1536×1024,
1024×1536); the generator picks the nearest by aspect and the post-processor
downscales to the doc's size and verifies the alpha is non-constant (warns
otherwise).

Grok has no transparency support, so it's used only for the opaque backgrounds
(`menu_bg`, `ICON0`, `PIC1`). Its aspect ratio comes from the nearest preset
(e.g. 16:9) and the post-processor cover-crops to the exact size.

## Files

- `../gen_assets.py` — CLI orchestrator (lives at `scripts/gen_assets.py`).
- `manifest.py` — declarative list of every asset (id, path, size, prompt).
- `workflow_build.py` — injects prompt/seed/dimensions into a workflow JSON.
- `comfy_cloud.py` — Cloud client (submit / poll / history / download).
- `postprocess.py` — Pillow resize, alpha handling, PNG crush.
- `workflows/*.json` — Grok / OpenAI workflow templates.

## Wiring the PNGs into the game

This tool only produces the PNGs. To make them show up in-game:

1. `data/*.png` are embedded by the Makefile's `bin2o` rule
   (`data/foo.png` → `foo_png` / `foo_png_size`).
2. Load each into its `UI_IMG_*` slot in `ui_images_load()`
   (`source/ui/theme.c`) via
   `ya2d_loadPNGfromBuffer(foo_png, foo_png_size)`.
3. `pkgfiles/*.PNG` are packaged into the PKG (not embedded in the ELF).

See `docs/assets.md` for the full mapping and budgets.
