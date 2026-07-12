"""Post-processing for generated assets (Pillow).

- Resize/crop the raw generation to the exact size the doc asks for.
- RGBA assets: keep a real alpha channel; verify it's non-constant.
- RGB assets: flatten onto the UI navy matte (opaque background).
- Optional PNG crush via pngquant / oxipng when they're on PATH.
"""

from __future__ import annotations

import io
import shutil
import subprocess
from pathlib import Path

from PIL import Image

from manifest import Asset


def _hex_to_rgb(h: str) -> tuple[int, int, int]:
    h = h.lstrip("#")
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))  # type: ignore[return-value]


def _fit_cover(img: Image.Image, w: int, h: int) -> Image.Image:
    """Scale to cover, then centre-crop to exactly w x h."""
    src_w, src_h = img.size
    scale = max(w / src_w, h / src_h)
    new = (max(1, round(src_w * scale)), max(1, round(src_h * scale)))
    img = img.resize(new, Image.LANCZOS)
    left = (img.width - w) // 2
    top = (img.height - h) // 2
    return img.crop((left, top, left + w, top + h))


def _fit_contain(img: Image.Image, w: int, h: int) -> Image.Image:
    """Scale to fit inside w x h (preserve alpha), centre on transparent canvas."""
    img.thumbnail((w, h), Image.LANCZOS)
    canvas = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    canvas.paste(img, ((w - img.width) // 2, (h - img.height) // 2), img)
    return canvas


def process(raw: bytes, asset: Asset) -> bytes:
    img = Image.open(io.BytesIO(raw))
    img.load()

    if asset.rgba:
        img = img.convert("RGBA")
        # contain-fit keeps the whole subject visible with its transparency
        out = _fit_contain(img, asset.width, asset.height)
    else:
        img = img.convert("RGBA")
        filled = Image.new("RGBA", img.size, (*_hex_to_rgb(asset.background_color), 255))
        filled.paste(img, (0, 0), img)
        out = _fit_cover(filled.convert("RGB"), asset.width, asset.height)

    buf = io.BytesIO()
    out.save(buf, format="PNG", optimize=True)
    return buf.getvalue()


def has_alpha(png_bytes: bytes) -> bool:
    """True when the PNG has a non-constant alpha channel."""
    img = Image.open(io.BytesIO(png_bytes))
    if img.mode != "RGBA":
        return False
    alpha = img.getchannel("A")
    lo, hi = alpha.getextrema()
    return lo < hi or lo < 255  # varying alpha, or fully-below-opaque


def crush(path: Path) -> None:
    """Best-effort lossless-ish PNG crush; preserves the alpha channel."""
    if shutil.which("oxipng"):
        subprocess.run(["oxipng", "-o", "4", "--strip", "safe", "-q", str(path)],
                       check=False)
    if shutil.which("pngquant"):
        subprocess.run(
            ["pngquant", "--force", "--skip-if-larger", "--strip",
             "--output", str(path), "--", str(path)],
            check=False,
        )
