"""Round-trip the LOD baking tool using a real textured vegetation mesh."""
import pathlib
import re
import subprocess
import sys
import tempfile


def run(*args):
    return subprocess.run(args, check=True, text=True, capture_output=True).stdout


def package(root):
    folders = sorted(root.glob("*_lod*/object.lod"))
    assert len(folders) == 1, folders
    return folders[0]


def check_package(description, expected_counts):
    text = description.read_text()
    assert text.startswith("YMGRE_LOD 1\nhysteresis 0.10\n")
    assert [float(value) for value in re.findall(r"level mesh ([\d.]+)", text)] == [120, 40, 0]
    assets = re.findall(r"level mesh [\d.]+ (\S+)", text)
    assert len(assets) == 3
    referenced = []
    for name, asset in zip(("near", "middle", "far"), assets):
        mesh = description.parent / asset
        assert mesh.name.endswith(f"_{name}.mesh") and mesh.is_file()
        material = mesh.with_suffix(".material")
        assert material.is_file()
        referenced.append(set(re.findall(r"\btexture (texture_\w+\.bmp)", material.read_text())))
    assert referenced[0] == referenced[1] == referenced[2]
    assert referenced[0]
    assert {p.name for p in description.parent.glob("texture_*.bmp")} == referenced[0]
    assert not any((description.parent / name).exists() for name in ("near", "middle", "far"))
    return [int(n) for n in expected_counts]


def check_preview(exe, description, prefix, expected, near_pixels=120):
    output = run(exe, "preview", str(description), str(prefix))
    rows = re.findall(r"frame (\d+): projected ([\d.]+) px, tier (\d) \((\d+) triangles\), rebuilt (\d)", output)
    assert len(rows) == 33, output
    assert float(rows[0][1]) >= near_pixels * 1.5
    assert float(rows[-1][1]) >= near_pixels * 1.5
    tiers = [int(row[2]) for row in rows]
    assert tiers[0] == tiers[-1] == 0 and 1 in tiers and 2 in tiers
    assert tiers[16] == 2
    assert all(int(row[3]) == expected[int(row[2])] for row in rows)
    assert any(row[4] == "0" for row in rows) and sum(row[4] == "1" for row in rows) >= 5
    assert len(list(prefix.parent.glob(prefix.name + "_*.bmp"))) == 33


def main(exe, source, tree, birch):
    with tempfile.TemporaryDirectory(prefix="ymgre-lod-baker-") as temporary:
        root = pathlib.Path(temporary)
        generated = root / "generated"
        generated.mkdir()
        output = run(exe, "export", str(generated), source)
        counts = [int(n) for n in re.findall(r"(?:near|middle|far): (\d+) triangles", output)]
        assert len(counts) == 3 and counts[0] > counts[1] > counts[2], counts
        first = package(generated)
        assert first.parent.name == "grass_green_low_lod"
        check_package(first, counts)
        check_preview(exe, first, root / "generated-preview", counts)
        edited = first.parent / "tuned.lod"
        edited.write_text(first.read_text().replace("hysteresis 0.10", "hysteresis 0.20")
                          .replace("level mesh 120", "level mesh 140")
                          .replace("level mesh 40", "level mesh 60"))
        check_preview(exe, edited, root / "tuned-preview", counts, near_pixels=140)

        supplied = root / "supplied"
        supplied.mkdir()
        paths = [first.parent / asset for asset in re.findall(r"level mesh [\d.]+ (\S+)", first.read_text())]
        output = run(exe, "export", str(supplied), *map(str, paths))
        assert [int(n) for n in re.findall(r"(?:near|middle|far): (\d+) triangles", output)] == counts
        second = package(supplied)
        check_package(second, counts)
        check_preview(exe, second, root / "supplied-preview", counts)

        run(exe, "export", str(generated), source)
        assert (generated / "grass_green_low_lod_2" / "object.lod").is_file()
        assert first.is_file()

        tree_root = root / "tree"
        tree_root.mkdir()
        output = run(exe, "export", str(tree_root), tree)
        tree_counts = [int(n) for n in re.findall(r"(?:near|middle|far): (\d+) triangles", output)]
        assert len(tree_counts) == 3 and tree_counts[0] > tree_counts[1] > tree_counts[2]
        tree_description = package(tree_root)
        check_package(tree_description, tree_counts)
        check_preview(exe, tree_description, root / "tree-preview", tree_counts)

        birch_root = root / "birch"
        birch_root.mkdir()
        output = run(exe, "export", str(birch_root), birch)
        birch_counts = [int(n) for n in re.findall(r"(?:near|middle|far): (\d+) triangles", output)]
        assert len(birch_counts) == 3 and birch_counts[0] > birch_counts[1] > birch_counts[2]
        birch_description = package(birch_root)
        check_package(birch_description, birch_counts)
        assert len(list(birch_description.parent.glob("texture_*.bmp"))) == 3
        check_preview(exe, birch_description, root / "birch-preview", birch_counts)
        print("LOD grass generated/supplied, oak and birch generated: PASS", counts, tree_counts, birch_counts)


if __name__ == "__main__":
    main(*sys.argv[1:])
