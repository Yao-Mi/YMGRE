"""Generate deterministic terrain data, not a painted normal/bump map."""
from pathlib import Path
import math
n = 257
pixels = bytearray()
for iz in range(n):
    z = iz * 2 - 256
    for ix in range(n):
        x = ix * 2 - 256
        h = (14 + 6*math.sin(x*.019+z*.009) + 4*math.cos(z*.025-x*.007)
             + 2.2*math.sin(x*.044+z*.032) + .8*math.cos(x*.1-z*.071))
        h += 15*math.exp(-((x-95)**2+(z-110)**2)/6000)
        h += 11*math.exp(-((x+140)**2+(z-125)**2)/4500)
        pixels.append(round(max(0,min(40,h))/40*255))
out = Path(__file__).parent / 'assets/heightmap.pgm'
out.write_bytes(b'P5\n257 257\n255\n'+pixels)
print(out)
