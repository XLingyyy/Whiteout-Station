"""Iteration 3: refine existing eye surfaces, without regenerating the face."""
from pathlib import Path
import bpy,math
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'SourceAssets/Characters/YeChengLookdev'
bpy.ops.wm.open_mainfile(filepath=str(OUT/'candidate_02.blend'))
def front(x,z):
    t=(z-.834)/(.855-.834);rx=.083+.001*t;ry=.091+.004*t;cy=.010+.002*t
    f=math.sqrt(max(0,1-(x/rx)**2))
    nose=.015*math.exp(-(x/.014)**2-((z-.815)/.010)**2)
    bridge=.009*math.exp(-(x/.013)**2-((z-.836)/.025)**2)
    muzzle=.015*math.exp(-(x/.029)**2-((z-.785)/.022)**2)
    cheeks=.004*sum(math.exp(-((x-s*.047)/.026)**2-((z-.813)/.027)**2) for s in [-1,1])
    return cy-ry*f-f**3*(nose+bridge+muzzle+cheeks)
for side,name in [(1,'L'),(-1,'R')]:
    cx=side*.043
    eye=bpy.data.objects['LD '+name+' curved eye panel']
    for v in eye.data.vertices:
        x,y,z=v.co;q=((x-cx)/.024)**2+((z-.842-side*.04*(x-cx))/(.0095 if z>.842 else .0065))**2
        v.co.y+=.003*max(0,1-q)
    iris=bpy.data.objects['LD '+name+' iris']
    for v in iris.data.vertices:
        dx=v.co.x-cx;dz=v.co.z-.843;r=math.hypot(dx,dz)
        target=.003 if r<.004 else .009 if r<.0078 else .0099
        scale=target/r if r>.00001 else 1
        x=cx+dx*scale;z=.844+dz*scale
        q=((x-cx)/.024)**2+((z-.842-side*.04*(x-cx))/(.0095 if z>.842 else .0065))**2
        y=front(x,z)-.002-.003*max(0,1-q)-.0006
        if q>1:y=front(x,z)+.001
        v.co=(x,y,z)
for name,color in [('LD iris bluegray',(.12,.25,.43)),('LD iris lower',(.20,.39,.57)),('LD pupil',(.013,.024,.055))]:
    m=bpy.data.materials[name];p=m.node_tree.nodes.get('Principled BSDF');p.inputs['Base Color'].default_value=(*color,1);p.inputs['Roughness'].default_value=.3
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'candidate_03.blend'))
print('LOOKDEV_THIRD_LOCAL_ITERATION_COMPLETE',flush=True)
