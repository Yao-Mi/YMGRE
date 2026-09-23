#!/usr/bin/env python3
"""Build four native RGB565/RGB888 and index16/index32 archives and a relocatable, source-free SDK."""
import argparse
import hashlib
import json
import os
import platform
import shutil
import subprocess
import tarfile
import tempfile
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/YMGRE_libs'
WORK = ROOT / 'build/_ymgre_sdk'
DEMOS = '''basic_shapes extended_shapes polygon_fill polygon_triangulation depth_overlap
frustum_clipping window_clipping backface_culling fragment_stitching shared_edge_stress
material_texture lighting render_target camera_viewport stereo_view
advanced_vertex advanced_texture advanced_perspective advanced_normal_map
advanced_specular advanced_render_modes advanced_clipping_planes
advanced_raytrace advanced_raytrace_object advanced_raytrace_shading
advanced_raytrace_shadow advanced_raytrace_mirror advanced_raytrace_refraction
advanced_raytrace_materials'''.split()

def run(args, log, cwd=ROOT, env=None):
    print(' '.join(map(str, args)), flush=True)
    log.parent.mkdir(parents=True, exist_ok=True)
    with log.open('w') as stream:
        subprocess.run(list(map(str,args)), cwd=cwd, env=env, stdout=stream,
                       stderr=subprocess.STDOUT, check=True)

def copy(source, dest):
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, dest)

VARIANTS = [(depth, bits) for depth in (16, 24) for bits in (16, 32)]


def format_name(depth):
    return 'rgb565' if depth == 16 else 'rgb888'


def variant_name(depth, bits):
    return f'{format_name(depth)}_index{bits}'


def build_package(out,args):
    for depth,bits in VARIANTS:
        tag=variant_name(depth,bits)
        build=WORK/'libraries'/tag
        run(['cmake','-S',ROOT/'sdk/library','-B',build,
             '-DCMAKE_BUILD_TYPE=Release',f'-DYMGRE_INDEX_BITS={bits}',
             f'-DYMGRE_CAMERA_COLOR_DEPTH={depth}'], WORK/f'configure-{tag}.log')
        run(['cmake','--build',build,'--target','ymgre',f'-j{args.jobs}'], WORK/f'build-{tag}.log')
        copy(build/'engine/libymgre.a', out/f'lib/libymgre_{tag}.a')
    for section in ('CONFIG','CORE','OPOBJ','IOFILE','DEBUG'):
        for header in (ROOT/'YMGRE'/section).glob('*.h'):
            copy(header,out/'include/YMGRE'/section/header.name)
    for name in ('demo_host.c','demo_host.h'):
        copy(ROOT/'Demo'/name,out/'examples/host'/name)
    copy(ROOT/'sdk/YMGREConfig.cmake',out/'cmake/YMGREConfig.cmake')
    for item in (ROOT/'sdk/examples').iterdir():
        if item.is_file():copy(item,out/'examples'/item.name)
    for name in DEMOS:copy(ROOT/f'Demo/demo_{name}.c',out/f'examples/demo_{name}.c')
    copy(ROOT/'sdk/MANUAL.md',out/'手册.md')
    copy(ROOT/'sdk/SDK.gitignore',out/'.gitignore')
    copy(ROOT/'sdk/build_demos.sh',out/'build_demos.sh')
    copy(ROOT/'LICENSE',out/'LICENSE')
    copy(ROOT/'LICENSE',out/'licenses/YMGRE-LICENSE')
    copy(ROOT/'sdk/README.md',out/'README.md')
    copy(ROOT/'docs/images/banner.svg',out/'docs/images/banner.svg')
    for name in ('advanced_raytrace_mirror','advanced_raytrace_refraction','advanced_perspective','advanced_normal_map',
                 'advanced_raytrace_shadow','basic_shapes','extended_shapes'):
        copy(ROOT/'docs/images'/f'{name}.png',out/'docs/images'/f'{name}.png')
    # Validate from a separate package copy: this must not resolve repository source paths.
    standalone=WORK/'standalone'
    if standalone.exists():shutil.rmtree(standalone)
    shutil.copytree(out,standalone,ignore=shutil.ignore_patterns('bin','verification','checksums.sha256'))
    for depth,bits in VARIANTS:
        tag=variant_name(depth,bits)
        consumer=WORK/'consumer'/tag
        run(['cmake','-S',standalone/'examples','-B',consumer,f'-DYMGRE_INDEX_BITS={bits}',
             f'-DYMGRE_CAMERA_COLOR_DEPTH={depth}',
             '-DCMAKE_BUILD_TYPE=Release','-DYMGRE_WINDOWED_DEMOS=OFF'],WORK/f'consumer-configure-{tag}.log')
        run(['cmake','--build',consumer,f'-j{args.jobs}'],WORK/f'consumer-build-{tag}.log')
        run(['ctest','--test-dir',consumer,'--output-on-failure'],out/f'verification/demos-{tag}.log')
        copy(consumer/'bin/demo_sdk_minimal',out/f'bin/{format_name(depth)}/index{bits}/demo_sdk_minimal')
    if args.ymgui_dir:
        for depth,bits in VARIANTS:
            tag=variant_name(depth,bits)
            consumer=WORK/'external-gui'/tag
            run(['cmake','-S',standalone/'examples','-B',consumer,f'-DYMGRE_INDEX_BITS={bits}',
                 f'-DYMGRE_CAMERA_COLOR_DEPTH={depth}',f'-DYMGUI_COLOR_DEPTH={depth}',
                 '-DYMGRE_WINDOWED_DEMOS=ON',f'-DYMGUI_DIR={args.ymgui_dir.resolve()}',
                 '-DCMAKE_BUILD_TYPE=Release'],WORK/f'gui-configure-{tag}.log')
            run(['cmake','--build',consumer,f'-j{args.jobs}'],WORK/f'gui-build-{tag}.log')
            run(['ctest','--test-dir',consumer,'--output-on-failure'],out/f'verification/external-gui-{tag}.log')
    # Core archives must not reference GUI/SDL symbols, even indirectly.
    for depth,bits in VARIANTS:
        tag=variant_name(depth,bits)
        symbols=subprocess.check_output(['nm','-u',str(out/f'lib/libymgre_{tag}.a')],text=True)
        if any(name in symbols for name in ('YMGUI_','SDL_','SDL_LCD_')):
            raise SystemExit('Unexpected GUI dependency in core archive')
    run(['python3',ROOT/'tests/test_sdk_variants.py',standalone/'cmake'],out/'verification/variant-selection.log')
    sources=sorted(p for folder in ('YMGRE','Demo','sdk') for p in (ROOT/folder).rglob('*')
                   if p.is_file() and p.suffix in ('.c','.h','.py','.cmake','.md','.txt','.sh'))
    sources.extend([ROOT/'CMakeLists.txt',ROOT/'tests/test_sdk_variants.py'])
    digest=hashlib.sha256()
    for p in sources:
        digest.update(str(p.relative_to(ROOT)).encode());digest.update(p.read_bytes())
    info={'built_at_utc':datetime.now(timezone.utc).isoformat(),'system':platform.system(),
          'machine':platform.machine(),'compiler':subprocess.check_output(['cc','--version'],text=True).splitlines()[0],
          'build_type':'Release','position_independent':True,'framebuffers':['RGB565','RGB888'],'color_depths':[16,24],
          'variants':[{'color_depth':depth,'index_bits':bits,
                       'library':f'lib/libymgre_{variant_name(depth,bits)}.a'} for depth,bits in VARIANTS],
          'index_bits':[16,32],'example_sources':len(DEMOS)+1,'prebuilt_demos_per_variant':1,
          'external_ymgui_validated':bool(args.ymgui_dir),'bundled_ymgui':False,'source_sha256':digest.hexdigest(),
          'git_head':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
          'source_includes_uncommitted_changes':bool(subprocess.check_output(
              ['git','status','--porcelain','--','YMGRE','Demo','sdk','CMakeLists.txt'],cwd=ROOT,text=True).strip()),
          'sdk_requires_engine_source':False,'rebuild_command':'./sdk/build.sh'}
    (out/'BUILD_INFO.json').write_text(json.dumps(info,ensure_ascii=False,indent=2)+'\n')
    (out/'checksums.sha256').write_text(''.join(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+str(p.relative_to(out))+'\n'
        for p in sorted(out.rglob('*')) if p.is_file() and p.name!='checksums.sha256'))


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ymgui-dir',type=Path,help='Validate window examples against an external YMGUI SDK without bundling it')
    parser.add_argument('--release',action='store_true',help='Write the verified archive and checksum to the tracked releases directory')
    parser.add_argument('--jobs',type=int,default=4,help='Parallel build jobs (default: 4)')
    args=parser.parse_args()
    if args.jobs < 1:
        parser.error('--jobs must be a positive integer')
    for command in ('cmake','ctest','cc','ar','nm'):
        if not shutil.which(command):raise SystemExit(f'Missing build dependency: {command}')
    if OUT.is_symlink() or (OUT.exists() and any(OUT.iterdir()) and not (OUT/'.ymgre-sdk').is_file()):
        raise SystemExit(f'Refusing to overwrite an unmanaged directory: {OUT}')
    WORK.mkdir(parents=True,exist_ok=True)
    # Build a clean package; failed validation must leave the previous SDK intact.
    with tempfile.TemporaryDirectory(prefix='package-',dir=WORK) as directory:
        stage=Path(directory)/'YMGRE_libs'
        stage.mkdir()
        (stage/'.ymgre-sdk').write_text('Generated by sdk/package.py\n')
        build_package(stage,args)
        if OUT.exists():shutil.rmtree(OUT)
        shutil.move(str(stage),OUT)
    archive_dir=ROOT/'releases' if args.release else OUT.parent
    archive_dir.mkdir(parents=True,exist_ok=True)
    archive=archive_dir/f'YMGRE_libs-{platform.system().lower()}-{platform.machine()}.tar.gz'
    checksum=write_archive(OUT,archive)
    print(f'SDK complete: {OUT}\nArchive: {archive}\nSHA256: {checksum}',flush=True)


def write_archive(source,archive):
    # Never truncate an existing release while compression is in progress.
    with tempfile.TemporaryDirectory(prefix='.ymgre-release-',dir=archive.parent) as directory:
        pending=Path(directory)/archive.name
        with tarfile.open(pending,'w:gz') as bundle:
            bundle.add(source,arcname='YMGRE_libs')
        checksum=hashlib.sha256(pending.read_bytes()).hexdigest()
        pending_checksum=pending.with_name(pending.name+'.sha256')
        pending_checksum.write_text(checksum+'  '+archive.name+'\n')
        pending.replace(archive)
        pending_checksum.replace(archive.with_name(archive.name+'.sha256'))
    return checksum


if __name__=='__main__':main()
