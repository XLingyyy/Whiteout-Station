"""One local correction after viewing the saved eye study; no other facial features."""
from pathlib import Path
import bpy,bmesh,json,math
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'SourceAssets/Characters/YeChengLookdev/MainEyeStudy'
bpy.ops.wm.open_mainfile(filepath=str(OUT/'YeCheng_MainEye_Study.blend'))
data=json.loads((OUT/'eye_landmarks.json').read_text(encoding='utf-8'));UP=data['upper'];LOW=data['lower'];S=.0016
face=bpy.data.objects['LD face with eyelid and mouth loops']
gi=face.vertex_groups['L rim'].index;rim=[v.index for v in face.data.vertices if any(g.group==gi for g in v.groups)]
def basis(x,z):
    a=(x-.043)/.024;b=(z-.842)/.016
    return [1,a,b,a*a,a*b,b*b]
fit=np.linalg.lstsq(np.array([basis(face.data.vertices[i].co.x,face.data.vertices[i].co.z) for i in rim]),np.array([face.data.vertices[i].co.y for i in rim]),rcond=None)[0]
def surface(x,z):return float(np.dot(basis(x,z),fit))
def linear(points,x):return float(np.interp(x,[p[0] for p in points],[p[1] for p in points]))
def smooth(points,x):
    xs=[p[0] for p in points];ys=[p[1] for p in points]
    i=max(0,min(len(xs)-2,int(np.searchsorted(xs,x)-1)))
    slopes=[(ys[k+1]-ys[k])/(xs[k+1]-xs[k]) for k in range(len(xs)-1)]
    def tangent(k):
        if k==0:return slopes[0]
        if k==len(xs)-1:return slopes[-1]
        return 0 if slopes[k-1]*slopes[k]<=0 else 2/(1/slopes[k-1]+1/slopes[k])
    h=xs[i+1]-xs[i];t=max(0,min(1,(x-xs[i])/h))
    return (2*t**3-3*t*t+1)*ys[i]+(t**3-2*t*t+t)*h*tangent(i)+(-2*t**3+3*t*t)*ys[i+1]+(t**3-t*t)*h*tangent(i+1)
adj={i:set() for i in range(len(face.data.vertices))}
for e in face.data.edges:
    a,b=e.vertices;adj[a].add(b);adj[b].add(a)
dist={i:0 for i in rim};frontier=set(rim)
for depth in [1,2]:
    frontier={q for i in frontier for q in adj[i] if q not in dist};dist.update({q:depth for q in frontier})
for i,depth in dist.items():
    v=face.data.vertices[i];px=568+(v.co.x-.043)/S;py=130-(v.co.z-.842)/S
    if not 553<=px<=583:continue
    path=UP if abs(py-linear(UP,px))<abs(py-linear(LOW,px)) else LOW
    dy=smooth(path,px)-linear(path,px);v.co.z-=dy*S*(1-depth/3)
face.data.update();bpy.context.view_layer.update()
bvh=BVHTree.FromObject(face,bpy.context.evaluated_depsgraph_get())
def skin_y(x,z):
    hit=bvh.ray_cast(Vector((x,-.3,z)),Vector((0,1,0)),.5)
    return hit[0].y if hit[0] is not None else surface(x,z)
# The same panel extends slightly under the lid, with a recessed border.
panel=bpy.data.objects['MainEye curved surface - reference opening']
NX,NY=120,20
for i in range(NX+1):
    px=553+30*i/NX;cap=max(0,1-((px-568)/15)**2)
    top=smooth(UP,px)-.65*cap;bottom=smooth(LOW,px)+.45*cap
    for j in range(NY+1):
        t=j/NY;py=top+(bottom-top)*t;x=.043+(px-568)*S;z=.842-(py-130)*S
        y=surface(x,z)+.00065-.0024*math.sin(math.pi*t)*cap
        panel.data.vertices[i*(NY+1)+j].co=(x,y,z)
for li,loop in enumerate(panel.data.loops):
    co=panel.data.vertices[loop.vertex_index].co;px=568+(co.x-.043)/S;py=130-(co.z-.842)/S
    panel.data.uv_layers.active.data[li].uv=((px-550)/37,1-(py-108)/32)
panel.data.update()
# Subdivide the existing sparse triangles before conforming them. This fixes chords
# cutting through a convex face without replacing the approved contour vertices.
for name in ['MainEye upper lash shaped mass','MainEye visible brow contour']:
    ob=bpy.data.objects[name];bm=bmesh.new();bm.from_mesh(ob.data)
    bmesh.ops.subdivide_edges(bm,edges=list(bm.edges),cuts=9,use_grid_fill=True)
    bm.to_mesh(ob.data);bm.free()
    for v in ob.data.vertices:
        x,y,z=v.co;v.co.y=min(surface(x,z)-.0006,skin_y(x,z)-.00035) if 'lash' in name else skin_y(x,z)-.00035
    for p in ob.data.polygons:p.use_smooth=True
    ob.data.update()
for im in bpy.data.images:
    if im.name.startswith('MainEye_'):
        path=OUT/'Textures'/Path(im.filepath).name
        if path.exists():
            if im.packed_file:im.unpack(method='REMOVE')
            im.filepath=str(path);im.reload();im.pack()
scene=bpy.context.scene
scene['local_eye_iteration']=2
(OUT/'local_iteration.json').write_text(json.dumps({'iteration':2,'observed_first_render':['lash and brow triangle chords intersected convex skin','iris edge visibly polygonal'],'correction':['subdivided only existing lash/brow surfaces and reprojected to skin','shape-preserving interpolation inside traced eye bounds','eye panel extends under lid with recessed border','softened only local iris boundary paint'],'scope':'same one-eye study; no other facial feature edits'},indent=2),encoding='utf-8')
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'YeCheng_MainEye_Study.blend'))
print('MAIN_EYE_LOCAL_CORRECTION_READY',flush=True)
