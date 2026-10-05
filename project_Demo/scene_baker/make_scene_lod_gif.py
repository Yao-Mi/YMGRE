"""Turn scene_lod_baker preview frames into a looping animation (requires Pillow)."""
import glob
import sys
from PIL import Image


def main(prefix, output):
    paths = sorted(glob.glob(prefix + "_*.bmp"))
    if len(paths) != 33:
        raise SystemExit(f"expected 33 LOD preview frames, found {len(paths)}")
    frames = [Image.open(path).convert("RGB") for path in paths]
    palette = frames[0].quantize(colors=192, dither=0)
    indexed = [frame.quantize(palette=palette, dither=0) for frame in frames]
    indexed[0].save(output, save_all=True, append_images=indexed[1:],
                    duration=90, loop=0, disposal=2, optimize=False)
    print(output)


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit("usage: make_scene_lod_gif.py FRAME_PREFIX OUTPUT.gif")
    main(sys.argv[1], sys.argv[2])
