#!/usr/bin/env python3
"""Generate/check the immutable sRGB input and fitted-filmic output tables."""
from pathlib import Path
import argparse


def tables():
    values=[]
    for i in range(256):
        x=i/255
        value=x/12.92 if x<=.04045 else ((x+.055)/1.055)**2.4
        text=format(value,'.9g')
        values.append(text+('f' if '.' in text or 'e' in text else '.0f'))
    output=[]
    for i in range(65537):
        x=i/4096
        value=min(1,max(0,x*(2.51*x+.03)/(x*(2.43*x+.59)+.14)))
        value=12.92*value if value<=.0031308 else 1.055*value**(1/2.4)-.055
        output.append(str(int(255*value+.5)))
    return ('/* Generated from standard sRGB transfer and the documented fitted filmic curve. */\n'
            'static const float decodeLut[256]={\n'+',\n'.join(','.join(values[i:i+8]) for i in range(0,256,8))+
            '\n};\nstatic const uint8 outputLut[65537]={\n'+',\n'.join(','.join(output[i:i+64]) for i in range(0,len(output),64))+'\n};\n')


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check',action='store_true')
    args=parser.parse_args()
    target=Path(__file__).resolve().parents[1]/'YMGRE/CORE/YMGRE_ColorLUT.inc'
    content=tables()
    if args.check:
        if target.read_text()!=content:raise SystemExit('LUT differs from generator')
        print('LUT reproducibility check passed')
    else:target.write_text(content)
