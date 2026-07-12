#!/usr/bin/env python3
"""Generate PS3 Monopoly Party image assets via ComfyUI Cloud.

Sends Grok / OpenAI GPT Image workflows to cloud.comfy.org, post-processes the
result to the exact size/format docs/assets.md asks for, and writes the PNGs
into data/ and pkgfiles/.

Examples
--------
  # See the full plan without hitting the API:
  python scripts/gen_assets.py --dry-run

  # Generate a single asset:
  python scripts/gen_assets.py --only logo

  # Only one token theme, forcing the Grok engine:
  python scripts/gen_assets.py --theme classic --engine grok

  # Everything, including the optional assets:
  python scripts/gen_assets.py --all --include-optional
"""

from __future__ import annotations

import argparse
import json
import os
import sys
import time
from pathlib import Path

# Make the sibling asset_gen/ modules importable regardless of CWD.
_HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(_HERE / "asset_gen"))

from manifest import MANIFEST, Asset  # noqa: E402
from workflow_build import build_workflow  # noqa: E402

REPO_ROOT = _HERE.parent
CACHE_DIR = _HERE / "asset_gen" / ".cache"


def select_assets(args) -> list[Asset]:
    assets = list(MANIFEST)

    if args.only:
        wanted = set(args.only)
        assets = [a for a in assets if a.id in wanted]
        missing = wanted - {a.id for a in assets}
        if missing:
            sys.exit(f"Unknown asset id(s): {', '.join(sorted(missing))}")

    if args.theme:
        prefix = f"tok_{args.theme}_"
        assets = [a for a in assets if a.id.startswith(prefix)]
        if not assets:
            sys.exit(f"No tokens for theme '{args.theme}'")

    if not args.include_optional and not args.only:
        assets = [a for a in assets if not a.optional]

    if args.engine:
        for a in assets:
            a.engine = args.engine

    return assets


def print_plan(assets: list[Asset]) -> None:
    print(f"Plan: {len(assets)} asset(s)\n")
    width = max((len(a.id) for a in assets), default=4)
    for a in assets:
        flags = []
        if a.rgba:
            flags.append("RGBA")
        else:
            flags.append("RGB")
        if a.optional:
            flags.append("optional")
        print(f"  {a.id.ljust(width)}  {a.width}x{a.height}  "
              f"{a.engine:<6}  [{','.join(flags)}]  -> {a.out}")


def run(args) -> int:
    assets = select_assets(args)

    if not assets:
        print("Nothing to generate.")
        return 0

    if args.dry_run:
        print_plan(assets)
        # Validate that each asset builds a workflow without error.
        for a in assets:
            build_workflow(a, seed=1)
        print("\nDry run OK (workflows build cleanly). No API calls made.")
        return 0

    # Import the cloud client lazily so --dry-run works without requests.
    from comfy_cloud import ComfyCloudClient, ComfyCloudError
    from postprocess import process, has_alpha, crush

    try:
        client = ComfyCloudClient(api_key=args.api_key)
    except ComfyCloudError as e:
        sys.exit(str(e))

    CACHE_DIR.mkdir(parents=True, exist_ok=True)
    print_plan(assets)
    print()

    ok = 0
    failed: list[str] = []
    for i, a in enumerate(assets, 1):
        dest = REPO_ROOT / a.out
        if dest.exists() and not args.force:
            print(f"[{i}/{len(assets)}] skip {a.id} (exists; use --force)")
            ok += 1
            continue

        print(f"[{i}/{len(assets)}] {a.id} ({a.engine}) -> {a.out}")
        try:
            wf = build_workflow(a, seed=args.seed)

            def _tick(elapsed, status, _id=a.id):
                # pad to clear any longer previous status when overwriting with \r
                print(f"    …{elapsed:>3}s  {status:<24}", end="\r", flush=True)

            raw = client.generate(
                wf,
                comfy_org_api_node=True,
                poll_interval=args.poll_interval,
                max_wait=args.max_wait,
                on_tick=_tick,
            )
            print()  # newline after the \r ticker

            out_bytes = process(raw, a)

            if a.rgba and not has_alpha(out_bytes):
                print(f"    WARNING: {a.id} expected alpha but result is opaque.")

            dest.parent.mkdir(parents=True, exist_ok=True)
            dest.write_bytes(out_bytes)

            if not args.no_crush:
                crush(dest)

            # Record what produced this asset.
            (CACHE_DIR / f"{a.id}.json").write_text(json.dumps({
                "id": a.id, "engine": a.engine, "out": a.out,
                "size": [a.width, a.height], "rgba": a.rgba,
                "prompt": a.prompt, "ts": time.time(),
            }, indent=2))

            print(f"    saved {dest.stat().st_size} bytes")
            ok += 1
        except Exception as e:  # keep going; report at the end
            print(f"\n    FAILED: {e}")
            failed.append(a.id)

    print(f"\nDone: {ok} ok, {len(failed)} failed.")
    if failed:
        print("Failed:", ", ".join(failed))
        return 1
    return 0


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    sel = p.add_argument_group("selection")
    sel.add_argument("--only", nargs="+", metavar="ID",
                     help="Generate only these asset ids (includes optionals).")
    sel.add_argument("--theme", metavar="SLUG",
                     help="Only tokens for a theme: classic|fantasy|scifi|ancient|prehistoric")
    sel.add_argument("--all", action="store_true",
                     help="Generate the full manifest (default when no filter).")
    sel.add_argument("--include-optional", action="store_true",
                     help="Include assets marked optional (board, dice, icons, PIC0).")

    gen = p.add_argument_group("generation")
    gen.add_argument("--engine", choices=["openai", "grok"],
                     help="Force an engine for all selected assets.")
    gen.add_argument("--seed", type=int, default=None,
                     help="Fixed seed (default: random per asset).")
    gen.add_argument("--api-key", default=os.environ.get("COMFY_CLOUD_API_KEY"),
                     help="COMFY_CLOUD_API_KEY (else read from env).")
    gen.add_argument("--poll-interval", type=float, default=3.0)
    gen.add_argument("--max-wait", type=float, default=600.0)

    out = p.add_argument_group("output")
    out.add_argument("--force", action="store_true",
                     help="Overwrite existing files.")
    out.add_argument("--no-crush", action="store_true",
                     help="Skip pngquant/oxipng optimisation.")
    out.add_argument("--dry-run", action="store_true",
                     help="Print the plan and validate workflows; no API calls.")
    return p


def main() -> int:
    args = build_parser().parse_args()
    return run(args)


if __name__ == "__main__":
    raise SystemExit(main())
