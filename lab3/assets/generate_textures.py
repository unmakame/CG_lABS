#!/usr/bin/env python3
"""Generate deterministic P6 textures used by CG Lab 3."""
from pathlib import Path
import math, random
W, H = 256, 128
OUT = Path(__file__).resolve().parent
rng = random.Random(5)
perm = list(range(256)); rng.shuffle(perm); perm *= 2
grads = ((1,0),(-1,0),(0,1),(0,-1),(.707,.707),(-.707,.707),(.707,-.707),(-.707,-.707))
def fade(t): return t*t*t*(t*(t*6-15)+10)
def perlin(x,y):
    xi, yi = math.floor(x)&255, math.floor(y)&255
    xf, yf = x-math.floor(x), y-math.floor(y)
    def dot(ix,iy,dx,dy):
        gx,gy=grads[perm[(perm[ix&255]+iy)&255]%len(grads)]
        return gx*dx+gy*dy
    u,v=fade(xf),fade(yf)
    a=dot(xi,yi,xf,yf); b=dot(xi+1,yi,xf-1,yf)
    c=dot(xi,yi+1,xf,yf-1); d=dot(xi+1,yi+1,xf-1,yf-1)
    return (a+(b-a)*u)*(1-v)+(c+(d-c)*u)*v
def write(name,pixels):
    (OUT/name).write_bytes(f'P6\n{W} {H}\n255\n'.encode()+pixels)
albedo=bytearray(); marble=bytearray(); spec=bytearray(); emissive=bytearray()
for y in range(H):
    v=y/H
    for x in range(W):
        u=x/W
        continent=perlin(u*5.5,v*5.5)+.38*perlin(u*13,v*13)+.12*perlin(u*30,v*30)
        land=continent>.035
        if v<.09 or v>.91: rgb=(224,232,224)
        elif land: rgb=(50,142+int(35*max(0,continent)),73)
        else: rgb=(42,123,194)
        albedo.extend(rgb)
        wave=math.sin(21*u+5*math.sin(11*v)+3*perlin(u*8,v*8))
        c=int(max(0,min(255,145+70*wave)))
        marble.extend((c,int(c*.9),int(c*.8)))
        s=205 if not land else 32
        spec.extend((s,s,s))
        n=0.0; amp=1.0; norm=0.0
        for octave in range(5):
            n+=amp*perlin(u*(8<<octave),v*(8<<octave));norm+=amp;amp*=.5
        glow=max(0.0,min(1.0,(n/norm+.12)*2.8))**1.7
        emissive.extend((int(255*glow),int(104*glow),int(17*glow)))
write('sphere_albedo.ppm',albedo)
write('marble_albedo.ppm',marble)
write('sphere_specular.ppm',spec)
write('sphere_emissive.ppm',emissive)
