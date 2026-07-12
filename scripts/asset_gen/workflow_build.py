"""Turn an Asset into a ready-to-submit ComfyUI workflow dict.

Grok and OpenAI are comfy.org API nodes:
  - OpenAI GPT Image (OpenAIGPTImage1): free-form `custom_width/height` that
    must be multiples of 16, `size:"Custom"`, and `background` which we set to
    "transparent" for RGBA assets so we get a real alpha channel.
  - Grok (GrokImageNode): string presets only (`aspect_ratio`, `resolution`),
    no explicit dimensions and no transparency -> only used for opaque assets.
"""

from __future__ import annotations

import json
import random
from pathlib import Path
from typing import Any

from manifest import Asset

WORKFLOW_DIR = Path(__file__).parent / "workflows"

_OPENAI_FILE = "openai_gpt_image_2_t2i.json"
_GROK_FILE = "grok_text_to_image.json"

# We generate transparent assets with gpt-image-1 (gpt-image-2 rejects
# `background: "transparent"`). gpt-image-1 only accepts three fixed sizes, so
# we pick the nearest by aspect and downscale to the doc's size in postprocess.
_OPENAI_MODEL = "gpt-image-1"
_OPENAI_SIZES = {
    "square": "1024x1024",
    "landscape": "1536x1024",
    "portrait": "1024x1536",
}


def _load(name: str) -> dict[str, Any]:
    return json.loads((WORKFLOW_DIR / name).read_text())


def _openai_size_preset(asset: Asset) -> str:
    ratio = asset.width / asset.height
    if ratio > 1.15:
        return _OPENAI_SIZES["landscape"]
    if ratio < 0.87:
        return _OPENAI_SIZES["portrait"]
    return _OPENAI_SIZES["square"]


def _grok_aspect(asset: Asset) -> str:
    ratio = asset.width / asset.height
    presets = {
        "1:1": 1.0, "16:9": 16 / 9, "9:16": 9 / 16,
        "4:3": 4 / 3, "3:4": 3 / 4, "3:2": 3 / 2, "2:3": 2 / 3,
    }
    return min(presets, key=lambda k: abs(presets[k] - ratio))


def _random_seed() -> int:
    return random.randint(1, 2_147_483_646)


def build_workflow(asset: Asset, *, seed: int | None = None) -> dict[str, Any]:
    seed = seed if seed is not None else _random_seed()

    if asset.engine == "grok":
        wf = _load(_GROK_FILE)
        node = wf["2"]["inputs"]
        node["prompt"] = asset.prompt
        node["seed"] = seed
        node["aspect_ratio"] = _grok_aspect(asset)
        return wf

    # default: OpenAI gpt-image-1 (supports transparent background)
    wf = _load(_OPENAI_FILE)
    node = wf["268"]["inputs"]
    node["prompt"] = asset.prompt
    node["seed"] = seed
    node["model"] = _OPENAI_MODEL
    node["size"] = _openai_size_preset(asset)
    # custom_* only apply when size == "Custom"; harmless otherwise but drop
    # them so a stale template default can't confuse the node.
    node.pop("custom_width", None)
    node.pop("custom_height", None)
    node["background"] = "transparent" if asset.rgba else "opaque"
    return wf
