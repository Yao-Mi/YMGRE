#!/usr/bin/env python3
"""Render one grass or tree moving away from the camera through all mesh LODs."""

import argparse
from collections import Counter
from pathlib import Path
import re
import subprocess
import tempfile

from PIL import Image, ImageDraw, ImageFont


def make_gif(executable: Path, destination: Path, species: str) -> None:
    try:
        font = ImageFont.truetype("DejaVuSans.ttf", 22)
    except OSError:
        font = ImageFont.load_default()
    frames = []
    counts = Counter()
    if species == "grass":
        distances = ([-3 + 8 * i / 9 for i in range(10)] +
                     [6 + 18 * i / 12 for i in range(13)] +
                     [27 + 35 * i / 12 for i in range(13)])
    else:
        distances = ([5 + 9 * i / 9 for i in range(10)] +
                     [17 + 17 * i / 12 for i in range(13)] +
                     [38 + 29 * i / 12 for i in range(13)])
    with tempfile.TemporaryDirectory(prefix="ymgre-lod-frames-") as temp:
        for index, distance in enumerate(distances):
            image_path = Path(temp) / f"frame-{index:02d}.bmp"
            result = subprocess.run(
                [str(executable), f"--frame-{species}", str(image_path), f"{distance:.3f}"],
                check=True,
                capture_output=True,
                text=True,
            )
            match = re.search(r"tier (\d+), (\d+) triangles", result.stdout)
            if not match:
                raise RuntimeError(f"Missing LOD diagnostics: {result.stdout}")
            level, triangles = map(int, match.groups())
            counts[level] += 1
            frame = Image.open(image_path).convert("RGB").resize((800, 500))
            draw = ImageDraw.Draw(frame)
            label = f"{species.title()}  |  {('NEAR', 'MID', 'FAR')[level]}  |  {triangles} triangles  |  z={distance:.1f}"
            draw.rectangle((12, 12, 12 + draw.textbbox((0, 0), label, font=font)[2] + 20, 55), fill=(22, 27, 20))
            draw.text((22, 20), label, fill=(245, 245, 235), font=font)
            frames.append(frame.quantize(colors=96))
    if set(counts) != {0, 1, 2} or min(counts.values()) < 5:
        raise RuntimeError(f"LOD animation needs at least five frames per tier: {counts}")
    destination.parent.mkdir(parents=True, exist_ok=True)
    sequence = [frames[0]] * 4 + frames + [frames[-1]] * 6
    sequence[0].save(destination, save_all=True, append_images=sequence[1:],
                     duration=110, loop=0, optimize=True, disposal=2)
    print(f"{species}: near/mid/far frames {counts[0]}/{counts[1]}/{counts[2]}; wrote {destination}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path, help="built lod_mesh_preview executable")
    parser.add_argument("output_directory", type=Path)
    args = parser.parse_args()
    for species in ("grass", "tree"):
        make_gif(args.executable.resolve(), args.output_directory / f"lod-{species}.gif", species)


if __name__ == "__main__":
    main()
