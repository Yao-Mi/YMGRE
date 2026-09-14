#!/usr/bin/env python3
"""Capture actual Demo windows as PNGs; no engine changes or image retouching."""
import argparse
import os
import shutil
from pathlib import Path
import subprocess
import tempfile
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
NAMES=('advanced_perspective','advanced_normal_map','advanced_raytrace_mirror','advanced_raytrace_refraction',
       'basic_shapes','extended_shapes','advanced_raytrace_shadow')
EDITOR_CAPTURES={'uv-tank': 'automatic_uv/tank-material-preview.png', 'uv-capsule': 'primitive_uv/capsule.png', 'texture-paint': 'texture_tools/paint-editor.png', 'texture-preview': 'texture_tools/paint-model-preview.png', 'scene-baker': 'export_preview_fix/editor.png', 'bake-before': 'export_preview_fix/before-bake.png', 'bake-no-lights': 'export_preview_fix/no-lights-preview.png', 'sphere-mesh': 'smooth_sphere/mesh.png', 'sphere-analytic': 'smooth_sphere/analytic.png'}
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--demo-dir',type=Path,required=True,help='Directory of compiled RGB565 window demos')
parser.add_argument('--earth-demo',type=Path,help='Optional RGB888 demo_advanced_earth_normal_map binary')
parser.add_argument('--editor-captures-dir',type=Path,help='Optional saved scene_baker capture directory; copies existing PNGs without operating the editor')
args=parser.parse_args()
out=ROOT/'docs/images';out.mkdir(parents=True,exist_ok=True)
with tempfile.TemporaryDirectory(prefix='ymgre-readme-') as directory:
    temp=Path(directory)
    # The interactive glass demo moves the sphere with the mouse. Fix that input
    # at the 560x420 window centre for repeatable headless captures.
    shim=temp/'pointer.c';shim.write_text('unsigned SDL_GetMouseState(int* x,int* y){if(x)*x=280;if(y)*y=210;return 0;}\n')
    subprocess.run(['cc','-shared','-fPIC',str(shim),'-o',str(temp/'pointer.so')],check=True)
    captures=[(name,(args.demo_dir/f'demo_{name}').resolve(),3) for name in NAMES]
    if args.earth_demo:captures.append(('earth',args.earth_demo.resolve(),1))
    for name,binary,frames in captures:
        bmp=temp/f'{name}.bmp'
        env=dict(os.environ,YMGUI_SHOT=str(bmp),YMGRE_HEADLESS='1',YMGRE_MAX_FRAMES=str(frames),YMGRE_WINDOW_SCALE='1')
        if name=='advanced_raytrace_refraction':env['LD_PRELOAD']=str(temp/'pointer.so')
        subprocess.run([str(binary)],cwd=ROOT,env=env,check=True,timeout=60)
        with Image.open(bmp) as image:image.save(out/f'{name}.png',optimize=True)
        print(out/f'{name}.png')

if args.editor_captures_dir:
    for name,relative in EDITOR_CAPTURES.items():
        source=args.editor_captures_dir/relative
        if not source.is_file():raise FileNotFoundError(source)
    for name,relative in EDITOR_CAPTURES.items():
        shutil.copy2(args.editor_captures_dir/relative,out/f"{name}.png")
        print(out/f"{name}.png")
