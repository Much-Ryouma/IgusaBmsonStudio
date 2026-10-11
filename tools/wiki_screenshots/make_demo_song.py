"""Generate the synthetic demo song used for the wiki screenshots (no third-party audio)."""

import os
import wave

import numpy as np
from PIL import Image, ImageDraw
os.makedirs('demo/img', exist_ok=True)
sr=44100; bpm=150; beat=60/bpm; bars=8; total=bars*4*beat+1.0
n=int(total*sr); rng=np.random.default_rng(1)
def write(name,x):
    x=np.clip(x,-1,1); s=(x*32767*0.4).astype(np.int16)
    st=np.stack([s,s],1).ravel()
    with wave.open('demo/'+name,'wb') as w:
        w.setnchannels(2); w.setsampwidth(2); w.setframerate(sr); w.writeframes(st.tobytes())
def env(length,decay): t=np.arange(length)/sr; return np.exp(-t/decay)
drums=np.zeros(n)
for b in range(bars*4):
    t0=int(b*beat*sr)
    if b%2==0:
        L=int(0.35*sr); t=np.arange(L)/sr; f=50+120*np.exp(-t/0.03)
        drums[t0:t0+L]+=0.9*np.sin(2*np.pi*np.cumsum(f)/sr)*env(L,0.12)
    else:
        L=int(0.25*sr); drums[t0:t0+L]+=0.5*rng.standard_normal(L)*env(L,0.06)+0.3*np.sin(2*np.pi*190*np.arange(L)/sr)*env(L,0.05)
    for h in (0,1):
        th=int((b+h*0.5)*beat*sr); L=int(0.06*sr)
        hp=rng.standard_normal(L); hp=np.diff(np.concatenate([[0],hp]))
        drums[th:th+L]+=0.18*hp*env(L,0.015)
write('drums.wav',drums*0.8)
notes=[40,40,43,45,40,40,47,45]*bars
bass=np.zeros(n)
for i,m in enumerate(notes):
    t0=int(i*beat/2*sr); L=int(beat/2*sr); f=440*2**((m-69)/12); t=np.arange(L)/sr
    saw=np.sin(2*np.pi*f*t)+0.4*np.sin(4*np.pi*f*t)+0.15*np.sin(6*np.pi*f*t)
    bass[t0:t0+L]+=0.5*saw*env(L,0.12)*np.minimum(1,t/0.002)*np.minimum(1,(L-np.arange(L))/(0.003*sr))
write('bass.wav',bass)
mel=[72,74,76,79,76,74,72,None,71,72,74,76,74,None,72,71]*bars
lead=np.zeros(n)
for i,m in enumerate(mel[:bars*8]):
    if m is None: continue
    t0=int(i*beat/2*sr); L=int(beat/2*sr*0.95); f=440*2**((m-69)/12); t=np.arange(L)/sr
    sq=np.sign(np.sin(2*np.pi*f*t))*0.5+0.5*np.sin(2*np.pi*f*2*t)
    lead[t0:t0+L]+=0.3*sq*env(L,0.18)*np.minimum(1,t/0.002)
write('lead.wav',lead)
im=Image.new('RGB',(300,80),(40,44,60)); d=ImageDraw.Draw(im)
for i in range(0,300,12): d.line((i,0,i+40,80),fill=(70,90,140))
im.save('demo/img/banner.png')
im=Image.new('RGB',(256,256),(20,20,30)); d=ImageDraw.Draw(im)
for r in range(120,0,-15): d.ellipse((128-r,128-r,128+r,128+r),outline=(90,160,220),width=3)
im.save('demo/img/bg.png')
print(f"wrote demo/ ({total:.1f} s)")
