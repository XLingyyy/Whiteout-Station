"""Iteration 2: edit the saved candidate, keeping its established topology."""
from pathlib import Path
import bpy,math
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'SourceAssets/Characters/YeChengLookdev'
bpy.ops.wm.open_mainfile(filepath=str(OUT/'candidate_01.blend'))
rings=[(.741,.007,.022,-.045),(.749,.023,.039,-.023),(.761,.041,.054,-.008),(.777,.058,.066,.002),(.794,.071,.076,.005),(.813,.080,.085,.008),(.834,.083,.091,.010),(.855,.084,.095,.012)]
def surface(x,z):
    for a,b in zip(rings,rings[1:]):
        if a[0]<=z<=b[0]:
            t=(z-a[0])/(b[0]-a[0]);rx,ry,cy=[a[k]*(1-t)+b[k]*t for k in [1,2,3]];break
    else:return None
    f=math.sqrt(max(0,1-(x/rx)**2))
    nose=.015*math.exp(-(x/.014)**2-((z-.815)/.010)**2)
    bridge=.009*math.exp(-(x/.013)**2-((z-.836)/.025)**2)
    muzzle=.012*math.exp(-(x/.036)**2-((z-.785)/.022)**2)
    cheeks=.004*sum(math.exp(-((x-s*.047)/.026)**2-((z-.813)/.027)**2) for s in [-1,1])
    return cy-ry*f-f**3*(nose+bridge+muzzle+cheeks)
face=bpy.data.objects['LD face with eyelid and mouth loops']
for v in face.data.vertices:
    x,y,z=v.co
    if y<0 and .761<z<.819:
        w=math.exp(-((z-.786)/.008)**2-(x/.035)**4)
        v.co.x=x*(1-.35*w)
        blend=min(1,(z-.761)/.008,(.819-z)/.008)
        target=surface(v.co.x,z)-.0012*math.exp(-(v.co.x/.017)**4-((z-.786)/.004)**2)
        v.co.y=y*(1-blend)+target*blend
for name in ['LD mouth cavity','LD upper lip border','LD lower lip border']:
    ob=bpy.data.objects[name]
    for v in ob.data.vertices:
        v.co.x*=.65
        v.co.y=surface(v.co.x,v.co.z)+(.002 if 'cavity' in name else -.0017)
face.modifiers['Editable facial surface'].levels=2;face.modifiers['Editable facial surface'].render_levels=2
# Match the already-present neck's skin tone; geometry and collar stay unchanged.
neck=bpy.data.objects['Neck'];neck.data.materials.clear();neck.data.materials.append(bpy.data.materials['LD skin'])
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'candidate_02.blend'))
print('LOOKDEV_LOCAL_REFINEMENT_COMPLETE',flush=True)
