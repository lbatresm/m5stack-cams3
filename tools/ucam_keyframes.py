#!/usr/bin/env python3
"""
Decrypt a folder of *.ucam files and transform only JPEGs that differ enough from the
immediately previous frame (change detection on consecutive captures).

Example:
  python tools/ucam_keyframes.py D:/simple_picsaver -p YOUR_PASS -o D:/keyframes
  python tools/ucam_keyframes.py ... --debug
"""

from __future__ import annotations

import argparse
import io
import shutil
import sys
from pathlib import Path

# Import decrypt_ucam from sibling script
_TOOLS = Path(__file__).resolve().parent
if str(_TOOLS) not in sys.path:
    sys.path.insert(0, str(_TOOLS))

from decrypt_ucam import decrypt_ucam, ucam_to_jpg_filename  # noqa: E402


def _pil_configure() -> None:
    """Allow slightly truncated JPEGs (common with embedded cameras / incomplete SD writes)."""
    try:
        from PIL import ImageFile

        ImageFile.LOAD_TRUNCATED_IMAGES = True
    except ImportError as e:
        raise SystemExit("Missing dependency: pip install pillow") from e


def jpeg_is_readable(jpeg: bytes) -> bool:
    """Return True if Pillow can decode the JPEG (full load)."""
    try:
        from PIL import Image
    except ImportError:
        return False
    try:
        with Image.open(io.BytesIO(jpeg)) as im:
            im.load()
            im.convert("L")
        return True
    except OSError:
        return False


def mean_abs_diff_grayscale(jpeg_a: bytes, jpeg_b: bytes, max_side: int = 320) -> float:
    """Mean absolute pixel difference on resized grayscale images (0–255 scale)."""
    try:
        from PIL import Image, ImageChops, ImageStat
    except ImportError as e:
        raise SystemExit("Missing dependency: pip install pillow") from e

    a = Image.open(io.BytesIO(jpeg_a)).convert("L")
    b = Image.open(io.BytesIO(jpeg_b)).convert("L")
    a.thumbnail((max_side, max_side), Image.Resampling.LANCZOS)
    b.thumbnail((max_side, max_side), Image.Resampling.LANCZOS)
    if a.size != b.size:
        b = b.resize(a.size, Image.Resampling.LANCZOS)
    diff = ImageChops.difference(a, b)
    return float(ImageStat.Stat(diff).mean[0])


def save_debug_artifacts(
    diff_scores: list[float],
    tools_dir: Path,
    threshold: float,
) -> None:
    """Write tools/ucam_diff_scores.csv and tools/ucam_diff_histogram.png (needs matplotlib)."""
    csv_path = tools_dir / "ucam_diff_scores.csv"
    csv_path.write_text("\n".join(f"{s:.6f}" for s in diff_scores) + "\n", encoding="utf-8")
    print(f"Debug: wrote {len(diff_scores)} diff values to {csv_path}")

    png_path = tools_dir / "ucam_diff_histogram.png"
    try:
        import matplotlib

        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        print(
            "Debug: matplotlib not installed — skipped PNG histogram (pip install matplotlib)",
            file=sys.stderr,
        )
        return

    n = len(diff_scores)
    bins = min(50, max(10, n // 2)) if n >= 2 else 10
    plt.figure(figsize=(9, 5))
    plt.hist(diff_scores, bins=bins, edgecolor="black", alpha=0.75, color="steelblue")
    plt.axvline(threshold, color="red", linestyle="--", linewidth=2, label=f"threshold={threshold}")
    plt.xlabel("Mean abs diff (grayscale, 0–255)")
    plt.ylabel("Count")
    plt.title(f"Consecutive-frame diffs (n={n})")
    plt.legend()
    plt.tight_layout()
    plt.savefig(png_path, dpi=150)
    plt.close()
    print(f"Debug: histogram saved to {png_path}")


def main() -> None:
    p = argparse.ArgumentParser(
        description="Filter *.ucam sequence: keep JPEGs that differ from the previous frame"
    )
    p.add_argument(
        "input_dir",
        type=Path,
        help="Folder with *.ucam files (sorted by name)",
    )
    p.add_argument(
        "-p",
        "--password",
        required=True,
        help="Same passphrase as CONFIG_PICSAVER_ENCRYPTION_PASSWORD",
    )
    p.add_argument(
        "-o",
        "--output",
        type=Path,
        required=True,
        help="Folder to write selected .jpg files",
    )
    p.add_argument(
        "-t",
        "--threshold",
        type=float,
        default=8.0,
        help="Min mean abs diff (0–255) vs previous frame to keep (default: 8). Lower = more frames.",
    )
    p.add_argument(
        "--max-side",
        type=int,
        default=320,
        help="Max width/height for comparison resize (default: 320)",
    )
    p.add_argument(
        "--dry-run",
        action="store_true",
        help="Print decisions only, do not write files",
    )
    p.add_argument(
        "--copy-ucam",
        action="store_true",
        help="Also copy the source .ucam for each kept frame into the output folder",
    )
    p.add_argument(
        "--debug",
        action="store_true",
        help="Write tools/ucam_diff_scores.csv and tools/ucam_diff_histogram.png (needs matplotlib for PNG)",
    )
    args = p.parse_args()
    _pil_configure()

    indir: Path = args.input_dir
    if not indir.is_dir():
        print(f"Error: not a directory: {indir}", file=sys.stderr)
        sys.exit(1)

    files = sorted(indir.glob("*.ucam"))
    if not files:
        print(f"No *.ucam in {indir}", file=sys.stderr)
        sys.exit(1)

    out: Path = args.output
    if not args.dry_run:
        out.mkdir(parents=True, exist_ok=True)

    prev_jpeg: bytes | None = None
    kept = 0
    skipped = 0
    bad = 0
    diff_scores: list[float] = []

    for ucam_path in files:
        blob = ucam_path.read_bytes()
        try:
            jpeg = decrypt_ucam(blob, args.password)
        except Exception as ex:
            print(f"{ucam_path.name}: decrypt failed: {ex}", file=sys.stderr)
            sys.exit(1)

        if not jpeg_is_readable(jpeg):
            print(
                f"{ucam_path.name}: JPEG unreadable or corrupt (truncated write?) — skipped",
                file=sys.stderr,
            )
            bad += 1
            continue

        if prev_jpeg is None:
            decision = "keep (first readable frame)"
            keep = True
        else:
            try:
                score = mean_abs_diff_grayscale(
                    prev_jpeg, jpeg, max_side=args.max_side
                )
                if args.debug:
                    diff_scores.append(score)
            except OSError as ex:
                print(
                    f"{ucam_path.name}: diff failed ({ex}) — skipped",
                    file=sys.stderr,
                )
                bad += 1
                continue
            keep = score >= args.threshold
            decision = f"keep (diff={score:.2f})" if keep else f"skip (diff={score:.2f})"

        prev_jpeg = jpeg

        if not keep:
            skipped += 1
            print(f"{ucam_path.name}: {decision}")
            continue

        kept += 1
        jpg_name = ucam_to_jpg_filename(ucam_path.name)
        dest_jpg = out / jpg_name
        print(f"{ucam_path.name}: {decision} -> {dest_jpg.name}")

        if not args.dry_run:
            dest_jpg.write_bytes(jpeg)
            if args.copy_ucam:
                shutil.copy2(ucam_path, out / ucam_path.name)

    print(
        f"Done: kept {kept}, skipped {skipped}, bad {bad}, total {len(files)} (threshold={args.threshold})"
    )

    if args.debug:
        if not diff_scores:
            print(
                "Debug: no pairwise diffs (need at least two readable frames) — no files written",
                file=sys.stderr,
            )
        else:
            save_debug_artifacts(diff_scores, _TOOLS, args.threshold)


if __name__ == "__main__":
    main()
