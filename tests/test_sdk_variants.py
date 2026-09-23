"""Check a generated SDK's CMake configuration matrix and reject mixed ABIs."""
from pathlib import Path
import subprocess
import sys
import tempfile

sdk_cmake = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path(__file__).resolve().parents[1] / 'build/YMGRE_libs/cmake'

with tempfile.TemporaryDirectory(prefix='ymgre-sdk-variants-') as directory:
    root = Path(directory)

    def configure(name, body='', flags=(), error=None, source=None):
        src = root / name
        src.mkdir()
        if source is None:
            (src / 'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.16)\nproject(VariantCheck C)\nfind_package(YMGRE CONFIG REQUIRED)\n' + body)
            source = src
        result = subprocess.run(['cmake', '-S', str(source), '-B', str(src / 'build'),
                                 f'-DYMGRE_DIR={sdk_cmake}', *flags], capture_output=True, text=True)
        output = result.stdout + result.stderr
        if error is None:
            assert result.returncode == 0, output
        else:
            assert result.returncode != 0 and error in output, output
        print('PASS', name)

    for depth, fmt in ((16, 'rgb565'), (24, 'rgb888')):
        for bits in (16, 32):
            expected = f'libymgre_{fmt}_index{bits}.a'
            configure(f'{fmt}-index{bits}', f'''
get_target_property(location YMGRE::ymgre IMPORTED_LOCATION)
get_filename_component(filename "${{location}}" NAME)
if(NOT filename STREQUAL "{expected}")
  message(FATAL_ERROR "Wrong library: ${{filename}}")
endif()
get_target_property(definitions YMGRE::ymgre INTERFACE_COMPILE_DEFINITIONS)
foreach(required IN ITEMS "YMGRE_INDEX_BITS={bits}" "YMGRE_CAMERA_COLOR_DEPTH={depth}")
  if(NOT required IN_LIST definitions)
    message(FATAL_ERROR "Missing ABI definition: ${{required}}")
  endif()
endforeach()
''', [f'-DYMGRE_CAMERA_COLOR_DEPTH={depth}', f'-DYMGRE_INDEX_BITS={bits}'])
    configure('invalid-depth', flags=['-DYMGRE_CAMERA_COLOR_DEPTH=8'], error='YMGRE_CAMERA_COLOR_DEPTH must be 16 or 24')
    configure('invalid-indices', flags=['-DYMGRE_INDEX_BITS=64'], error='YMGRE_INDEX_BITS must be 16 or 32')
    for name, variable, value in (('mixed-depth', 'YMGRE_CAMERA_COLOR_DEPTH', 24), ('mixed-indices', 'YMGRE_INDEX_BITS', 32)):
        configure(name, f'set({variable} {value})\nfind_package(YMGRE CONFIG REQUIRED)\n',
                  flags=['-DYMGRE_CAMERA_COLOR_DEPTH=16', '-DYMGRE_INDEX_BITS=16'],
                  error='Use one YMGRE SDK, index width and color depth')
    configure('mixed-gui-depth', source=sdk_cmake.parent / 'examples',
              flags=['-DYMGRE_CAMERA_COLOR_DEPTH=24', '-DYMGUI_COLOR_DEPTH=16', '-DYMGRE_WINDOWED_DEMOS=ON'],
              error='YMGUI_COLOR_DEPTH must match YMGRE_CAMERA_COLOR_DEPTH')
print('9 SDK configuration checks passed')
