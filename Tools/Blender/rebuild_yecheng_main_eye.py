"""Surgical one-eye study in a copy. Never builds a face or modifies production assets."""
from pathlib import Path
import bpy,math,json
import numpy as np
from mathutils import Vector
from mathutils.geometry import tessellate_polygon
from mathutils.bvhtree import BVHTree

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'SourceAssets/Characters/YeChengLookdev/MainEyeStudy'
SOURCE=ROOT/'SourceAssets/Characters/YeChengLookdev/candidate_02_review.blend'
bpy.ops.wm.open_mainfile(filepath=str(SOURCE))
scene=bpy.context.scene
before={o.name:[tuple(v.co) for v in o.data.vertices] for o in scene.objects if o.type=='MESH'}
face=bpy.data.objects['LD face with eyelid and mouth loops']
rim_group=face.vertex_groups['L rim'].index
rim=[v.index for v in face.data.vertices if any(g.group==rim_group for g in v.groups)]
oldrim=[face.data.vertices[i].co.copy() for i in rim]
def basis(x,z):
    a=(x-.043)/.024;b=(z-.842)/.016
    return [1,a,b,a*a,a*b,b*b]
fit=np.linalg.lstsq(np.array([basis(p.x,p.z) for p in oldrim]),np.array([p.y for p in oldrim]),rcond=None)[0]
def surface(x,z): return float(np.dot(basis(x,z),fit))
S=.0016
def xz(px,py): return .043+(px-568)*S,.842-(py-130)*S
UP=[(553,129),(557,127),(562,125.8),(568,125.2),(574,125),(579,126),(583,127)]
LOW=[(553,129),(556,132),(560,134.5),(566,135.5),(572,135.5),(577,135),(580,132.5),(582,129),(583,127)]
LASH=[(553,129),(556,125),(561,123),(566,122),(572,122),(577,123),(582,124),(586,126),(583,127)]
def interp(points,x):
    for a,b in zip(points,points[1:]):
        if a[0]<=x<=b[0]:
            t=(x-a[0])/(b[0]-a[0]);return a[1]*(1-t)+b[1]*t
    return points[0 if x<points[0][0] else -1][1]
def opening(a):
    px=568+15*math.cos(a);py=interp(UP if math.sin(a)>0 else LOW,px)
    x,z=xz(px,py);return Vector((x,surface(x,z)-.0002,z))
# Keep the existing annular topology, moving just its inner three rings.
adj={i:set() for i in range(len(face.data.vertices))}
for e in face.data.edges:
    a,b=e.vertices;adj[a].add(b);adj[b].add(a)
dist={i:0 for i in rim};frontier=set(rim)
for depth in [1,2]:
    frontier={q for i in frontier for q in adj[i] if q not in dist}
    dist.update({q:depth for q in frontier})
for index,depth in dist.items():
    v=face.data.vertices[index];x,y,z=v.co
    dx=(x-.043)/.024;dz=(z-.842-.04*(x-.043))/(.0095 if z>.842+.04*(x-.043) else .0065)
    a=math.atan2(dz,dx)
    nearest=min(range(len(oldrim)),key=lambda k:abs(math.atan2(math.sin(a-math.atan2((oldrim[k].z-.842-.04*(oldrim[k].x-.043))/(.0095 if oldrim[k].z>.842+.04*(oldrim[k].x-.043) else .0065),(oldrim[k].x-.043)/.024)),math.cos(a-math.atan2((oldrim[k].z-.842-.04*(oldrim[k].x-.043))/(.0095 if oldrim[k].z>.842+.04*(oldrim[k].x-.043) else .0065),(oldrim[k].x-.043)/.024)))))
    p=oldrim[nearest]
    ar=math.atan2((p.z-.842-.04*(p.x-.043))/(.0095 if p.z>.842+.04*(p.x-.043) else .0065),(p.x-.043)/.024)
    delta=opening(ar)-p
    v.co+=delta*(1-depth/3)
crease=face.data.attributes.get('crease_edge') or face.data.attributes.new('crease_edge','FLOAT','EDGE')
rimset=set(rim)
for e in face.data.edges:
    if all(i in rimset for i in e.vertices): crease.data[e.index].value=.82
face.data.update();bpy.context.view_layer.update()
bvh=BVHTree.FromObject(face,bpy.context.evaluated_depsgraph_get())
def skin_y(x,z):
    hit=bvh.ray_cast(Vector((x,-.3,z)),Vector((0,1,0)),.5)
    return hit[0].y if hit[0] is not None else surface(x,z)
oldparts=['LD L curved eye panel','LD L iris','LD L upper eyelash','LD L lower eyelid edge','LD L eyebrow']
for name in oldparts:
    bpy.data.objects[name].hide_render=True;bpy.data.objects[name].hide_viewport=True
collection=bpy.data.collections.new('MAIN EYE ONLY - awaiting approval');scene.collection.children.link(collection)
def material(name,texture,alpha=False,rough=.65):
    m=bpy.data.materials.new(name);m.use_nodes=True;n=m.node_tree.nodes;l=m.node_tree.links
    p=n.get('Principled BSDF');p.inputs['Roughness'].default_value=rough;p.inputs['Specular IOR Level'].default_value=.14
    tex=n.new('ShaderNodeTexImage');tex.image=bpy.data.images.load(str(OUT/'Textures'/texture));tex.interpolation='Linear'
    l.new(tex.outputs['Color'],p.inputs['Base Color'])
    if alpha:l.new(tex.outputs['Alpha'],p.inputs['Alpha'])
    m['main_eye_alpha']=alpha
    return m
eye_mat=material('MainEye local iris and sclera','MainEye_IrisSclera.png',rough=.52)
lash_mat=material('MainEye painted lash mass','MainEye_LashMass.png')
brow_mat=material('MainEye painted brow','MainEye_Brow.png')
shade_mat=material('MainEye local lid shading','MainEye_LidShade.png',alpha=True)
def mesh(name,pts,faces,mat,depth='panel'):
    vs=[]
    for px,py in pts:
        x,z=xz(px,py)
        if depth=='panel':
            upper=interp(UP,px);lower=interp(LOW,px);v=(py-upper)/max(.01,lower-upper)
            bulge=.0024*math.sin(math.pi*min(1,max(0,v)))*max(0,1-((px-568)/15)**2)
            y=surface(x,z)-.0003-bulge
        elif depth=='lash': y=min(surface(x,z)-.00065,skin_y(x,z)-.00025)
        else:y=skin_y(x,z)-.00018
        vs.append((x,y,z))
    me=bpy.data.meshes.new(name);me.from_pydata(vs,[],faces);me.update()
    ob=bpy.data.objects.new(name,me);collection.objects.link(ob);me.materials.append(mat)
    uv=me.uv_layers.new(name='LocalReferenceEyeUV')
    for poly in me.polygons:
        poly.use_smooth=True
        for li in poly.loop_indices:
            px,py=pts[me.loops[li].vertex_index];uv.data[li].uv=((px-550)/37,1-(py-108)/32)
    return ob
# Non-elliptic eye surface, with irregular traced upper and lower borders.
pts=[];fs=[];NX,NY=120,20
for i in range(NX+1):
    px=553+30*i/NX;top=interp(UP,px);bottom=interp(LOW,px)
    for j in range(NY+1):pts.append((px,top+(bottom-top)*j/NY))
for i in range(NX):
    for j in range(NY):
        a=i*(NY+1)+j;fs.append((a,a+1,a+NY+2,a+NY+1))
panel=mesh('MainEye curved surface - reference opening',pts,fs,eye_mat)
def polygon(name,points,mat,depth):
    vecs=[Vector((x,y,0)) for x,y in points]
    triangles=tessellate_polygon([vecs])
    return mesh(name,points,triangles,mat,depth)
lash=polygon('MainEye upper lash shaped mass',LASH+list(reversed(UP[1:-1])),lash_mat,'lash')
solid=lash.modifiers.new('Thin lash volume','SOLIDIFY');solid.thickness=.00022
polygon('MainEye visible brow contour',[(554,113.5),(560,112),(567,110.5),(573,110.5),(575,111),(574,113),(568,112.5),(561,113.5),(554,115)],brow_mat,'skin')
# Low-opacity local strokes follow the surface. They do not carry the eye opening.
pts=[];fs=[];NX,NY=48,40
for i in range(NX+1):
    for j in range(NY+1):pts.append((550+37*i/NX,108+32*j/NY))
for i in range(NX):
    for j in range(NY):
        a=i*(NY+1)+j;fs.append((a,a+1,a+NY+2,a+NY+1))
shade=mesh('MainEye local fold and lower shade',pts,fs,shade_mat,'skin');shade['hide_for_gray']=True
scene.camera.location=(.043,-2,.856);scene.camera.rotation_euler=(Vector((.043,0,.856))-scene.camera.location).to_track_quat('-Z','Y').to_euler()
scene.camera.data.type='ORTHO';scene.camera.data.ortho_scale=.085
scene.render.resolution_x=1000;scene.render.resolution_y=800;scene.render.resolution_percentage=100
scene.cycles.samples=32;scene.cycles.use_denoising=True
scene.view_settings.exposure=0;scene.camera.data.dof.use_dof=False;scene.render.use_motion_blur=False
scene['review_status']='ONE MAIN EYE ONLY; Candidate 02 face rejected; awaiting approval; no full-face candidate'
scene['main_eye_source']='yecheng/正面.png screen-right; same local scale on x and z'
changed=[];unchanged=[]
for name,coords in before.items():
    now=bpy.data.objects[name]
    ids=[i for i,(a,v) in enumerate(zip(coords,now.data.vertices)) if a!=tuple(v.co)]
    if ids:changed.append({'object':name,'vertices':ids,'bounds':[[min(now.data.vertices[i].co[k] for i in ids),max(now.data.vertices[i].co[k] for i in ids)] for k in range(3)]})
    else:unchanged.append(name)
assert len(changed)==1 and changed[0]['object']==face.name
(OUT/'scope_record.json').write_text(json.dumps({'source':str(SOURCE),'changed_existing_meshes':changed,'unchanged_existing_meshes':unchanged,'hidden_old_main_eye_parts':oldparts,'other_eye_unchanged':True,'new_collection':collection.name,'production_assets_modified':False},ensure_ascii=False,indent=2),encoding='utf-8')
(OUT/'eye_landmarks.json').write_text(json.dumps({'upper':UP,'lower':LOW,'lash_outer':LASH,'reference_world_mapping':{'source_origin':[568,130],'world_xz':[.043,.842],'meters_per_source_pixel':S},'camera':{'location':list(scene.camera.location),'ortho_scale':.085,'resolution':[1000,800]},'status':'one-eye study only, pending review'},indent=2),encoding='utf-8')
bpy.ops.file.pack_all()
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'YeCheng_MainEye_Study.blend'))
print('MAIN_EYE_STUDY_READY',flush=True)
