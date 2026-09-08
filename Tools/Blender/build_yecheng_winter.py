"""Build the reference winter outfit on Ye Cheng's existing facial/spring rig.

Run with Blender 5.2 and the V10 editable source loaded. Source remains untouched.
"""
from pathlib import Path
import math
import random
import json
import bpy
import bmesh
from mathutils import Vector
from mathutils.kdtree import KDTree

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'SourceAssets/Characters/YeChengWinter'
OUT.mkdir(parents=True, exist_ok=True)
arm = bpy.data.objects['Armature']
body = bpy.data.objects['Body']
original_bones = [b.name for b in arm.data.bones]
random.seed(18)

# Transfer the established body skinning to the new garment surfaces.
kd = KDTree(len(body.data.vertices))
weights = []
for v in body.data.vertices:
    kd.insert(body.matrix_world @ v.co, v.index)
    weights.append([(body.vertex_groups[g.group].name, g.weight) for g in v.groups])
kd.balance()

def mat(name, color, metal=0, rough=.7):
    m = bpy.data.materials.new(name)
    m.diffuse_color = (*color, 1)
    m.use_nodes = True
    bs = m.node_tree.nodes.get('Principled BSDF')
    bs.inputs['Base Color'].default_value = (*color, 1)
    bs.inputs['Metallic'].default_value = metal
    bs.inputs['Roughness'].default_value = rough
    return m

white = mat('YC_SnowShell', (.73,.76,.82))
blue = mat('YC_SlateBlueFabric', (.075,.115,.23))
black = mat('YC_CharcoalCloth', (.018,.023,.033))
leather = mat('YC_BlackLeather', (.012,.016,.022), 0,.42)
silver = mat('YC_BrushedSteel', (.34,.39,.46), .8,.3)
fur = mat('YC_IceFur', (.66,.70,.78))
badge = mat('YC_BadgeWhite', (.9,.93,.97))

def bind(o, bone=None):
    o.parent = arm
    mod = o.modifiers.new('Character skin', 'ARMATURE'); mod.object = arm
    if bone:
        o.vertex_groups.new(name=bone).add(list(range(len(o.data.vertices))),1,'REPLACE')
    else:
        for v in o.data.vertices:
            co = o.matrix_world @ v.co
            near = kd.find_n(co, 3)
            accum = {}
            for _, idx, dist in near:
                for name,w in weights[idx]: accum[name] = accum.get(name,0)+w/max(dist,.001)**2
            total = sum(accum.values())
            for name,w in accum.items():
                g = o.vertex_groups.get(name) or o.vertex_groups.new(name=name)
                g.add([v.index],w/total,'REPLACE')
    for p in o.data.polygons: p.use_smooth = True
    return o

def mesh(name, verts, faces, material, bone=None, thickness=0):
    me=bpy.data.meshes.new(name); me.from_pydata(verts,[],faces); me.update()
    o=bpy.data.objects.new(name,me); bpy.context.collection.objects.link(o)
    o.data.materials.append(material)
    if thickness:
        mod=o.modifiers.new('Fabric thickness','SOLIDIFY');mod.thickness=thickness
        bpy.context.view_layer.objects.active=o
        bpy.ops.object.modifier_apply(modifier=mod.name)
    return bind(o,bone)

def loft(name, rings, material, bone=None, start=0, end=2*math.pi, steps=40, thick=0):
    # Rings: height, center X/Y, X/Y radii; front faces negative Y.
    vs=[]
    for j,(z,x,y,rx,ry) in enumerate(rings):
        for i in range(steps+1):
            a=start+(end-start)*i/steps
            wrinkle=1+.004*math.sin(i*2.7+j*1.9)
            vs.append((x+rx*math.sin(a)*wrinkle,y-ry*math.cos(a)*wrinkle,z))
    fs=[]
    for j in range(len(rings)-1):
        for i in range(steps):
            k=j*(steps+1)+i;fs.append((k,k+1,k+steps+2,k+steps+1))
    return mesh(name,vs,fs,material,bone,thick)

def box(name, pos, scale, material, bone='J_Bip_C_Chest', bevel=.003):
    bpy.ops.mesh.primitive_cube_add(size=1,location=pos)
    o=bpy.context.object;o.name=name;o.scale=scale
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    o.data.materials.append(material)
    if bevel:
        m=o.modifiers.new('Rounded edges','BEVEL');m.width=bevel;m.segments=2
        bpy.ops.object.modifier_apply(modifier=m.name)
    return bind(o,bone)

def tube(name, points, radius, material, bone='J_Bip_C_Chest'):
    c=bpy.data.curves.new(name,'CURVE');c.dimensions='3D';c.bevel_depth=radius;c.bevel_resolution=2
    s=c.splines.new('BEZIER');s.bezier_points.add(len(points)-1)
    for p,co in zip(s.bezier_points,points):p.co=co;p.handle_left_type='AUTO';p.handle_right_type='AUTO'
    o=bpy.data.objects.new(name,c);bpy.context.collection.objects.link(o)
    bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o
    bpy.ops.object.convert(target='MESH');o=bpy.context.object;o.data.materials.append(material)
    return bind(o,bone)

# Keep the original neck and articulated hands. Remove the street outfit and
# hidden body surfaces so the long coat cannot reveal a skirt during movement.
body.shape_key_clear()
bm=bmesh.new();bm.from_mesh(body.data)
delete=[]
for f in bm.faces:
    center=body.matrix_world @ f.calc_center_median()
    keep=f.material_index==0 and (center.z>1.24 or abs(center.x)>.535)
    if not keep:delete.append(f)
bmesh.ops.delete(bm,geom=delete,context='FACES')
bm.to_mesh(body.data);bm.free()
body.data.materials.append(leather)
for p in body.data.polygons:
    if abs((body.matrix_world@p.center).x)>.53:p.material_index=len(body.data.materials)-1
bpy.ops.object.select_all(action='DESELECT')
body.select_set(True)
bpy.context.view_layer.objects.active=body
bpy.ops.object.material_slot_remove_unused()

loft('Thermal jacket',[(.83,0,0,.107,.075),(.91,0,0,.10,.076),(1.02,0,0,.12,.082),(1.12,0,0,.132,.092),(1.21,0,.008,.112,.07),(1.25,0,.008,.047,.047)],black)
loft('High collar',[(1.22,0,.006,.049,.048),(1.275,0,.006,.047,.045)],black,'J_Bip_C_Neck',thick=.004)
coat=loft('Open winter parka',[(.40,0,.018,.235,.13),(.46,0,.018,.222,.125),(.60,0,.012,.19,.116),(.76,0,.005,.151,.1),(.89,0,0,.127,.09),(1.02,0,0,.139,.101),(1.14,0,.008,.151,.103),(1.225,0,.015,.125,.078)],white,start=.40,end=2*math.pi-.40,steps=56,thick=.005)
coat.data.materials.append(blue)
# Inner shell uses the solidify generated material offset below through a
# separate inset surface, keeping the blue lining visible at the front opening.
loft('Blue coat lining',[(.401,0,.018,.226,.121),(.46,0,.018,.213,.116),(.60,0,.012,.181,.107),(.76,0,.005,.142,.091),(.89,0,0,.118,.081),(1.02,0,0,.130,.092),(1.14,0,.008,.142,.094),(1.224,0,.015,.116,.069)],blue,start=.40,end=2*math.pi-.40,steps=56)

for sign,side in [(1,'L'),(-1,'R')]:
    # T-pose sleeve loft follows the original shoulder/elbow/wrist positions.
    vs=[];fs=[];n=32
    for j,(x,z,r) in enumerate([(.10,1.208,.081),(.16,1.208,.079),(.25,1.205,.070),(.32,1.203,.064),(.40,1.202,.067),(.49,1.202,.054),(.535,1.202,.047)]):
        for i in range(n):
            a=i*2*math.pi/n;rr=r*(1+.035*math.sin(i*3+j*2))
            vs.append((sign*x,.005+rr*math.cos(a),z+rr*math.sin(a)))
    for j in range(6):
        for i in range(n):
            k=j*n+i;fs.append((k,j*n+(i+1)%n,(j+1)*n+(i+1)%n,k+n))
    mesh('Parka sleeve '+side,vs,fs,white,thickness=.004)
    box('Wrist tab '+side,(sign*.513,-.048,1.205),(.040,.012,.043),leather,'J_Bip_'+side+'_LowerArm')
    box('Wrist stud '+side,(sign*.513,-.057,1.205),(.008,.005,.008),silver,'J_Bip_'+side+'_LowerArm')
    loft('Cargo trousers '+side,[(.16,sign*.039,.024,.040,.04),(.22,sign*.041,.016,.044,.046),(.32,sign*.044,.008,.048,.052),(.47,sign*.05,.004,.058,.059),(.52,sign*.052,0,.061,.061),(.64,sign*.06,0,.064,.073),(.77,sign*.068,0,.067,.083),(.86,sign*.065,0,.066,.077)],black)
    loft('Boot shaft '+side,[(.075,sign*.039,.023,.045,.053),(.14,sign*.04,.025,.043,.047),(.25,sign*.042,.02,.048,.05)],leather)
    box('Boot toe '+side,(sign*.041,-.024,.062),(.091,.186,.087),leather,'J_Bip_'+side+'_Foot',.025)
    box('Tread sole '+side,(sign*.041,-.025,.019),(.098,.195,.029),black,'J_Bip_'+side+'_Foot',.008)
    for j in range(7):
        z=.095+j*.02
        tube('Boot lace '+side+str(j),[(sign*.04-.03,-.028,z),(sign*.04+.03,-.033,z+.014)],.0025,silver,'J_Bip_'+side+'_Foot')
    for j in range(5):
        box('Boot tread '+side+str(j),(sign*.041,-.098+j*.037,.008),(.103,.015,.014),black,'J_Bip_'+side+'_Foot',.002)
    box('Belt pouch '+side,(sign*.135,-.044,.76),(.058,.042,.102),leather,'J_Bip_C_Hips')
    box('Pouch flap '+side,(sign*.135,-.069,.792),(.061,.009,.035),black,'J_Bip_C_Hips')
    if side=='L':
        box('Pouch cross vertical',(sign*.135,-.077,.76),(.01,.003,.034),badge,'J_Bip_C_Hips',.0005)
        box('Pouch cross horizontal',(sign*.135,-.078,.76),(.03,.003,.011),badge,'J_Bip_C_Hips',.0005)
    # Straps are separated from the coat to keep their readable raised profile.
    tube('Harness '+side,[(sign*.11,-.084,1.21),(sign*.104,-.108,1.10),(sign*.10,-.099,.96),(sign*.105,-.091,.88)],.011,leather)
    for z in [1.14,.96]:
        box('Harness buckle '+side+str(z),(sign*.107,-.119,z),(.026,.009,.032),silver)
        box('Harness buckle inset '+side+str(z),(sign*.107,-.126,z),(.017,.005,.020),black)
    tube('Coat opening piping '+side,[(sign*.091,-.105,.41),(sign*.077,-.097,.60),(sign*.059,-.077,.89),(sign*.059,-.087,1.10),(sign*.049,-.059,1.22)],.0035,blue)
    for z in [.46,.62,.80,1.02,1.17]:
        x=.04+.052*(1.22-z)/.82
        box('Coat snap '+side+str(z),(sign*x,-.105 if z<.7 else -.095,z),(.01,.005,.012),silver)
    for j in range(20):
        a=j*2*math.pi/20
        bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1,radius=1,location=(sign*.042+.049*math.sin(a),.02-.052*math.cos(a),.249))
        o=bpy.context.object;o.name='Boot fur '+side;o.scale=(.016,.014,.015);bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);o.data.materials.append(fur);bind(o,'J_Bip_'+side+'_LowerLeg')

loft('Utility belt',[(.867,0,0,.129,.095),(.896,0,0,.129,.095)],leather,'J_Bip_C_Hips',thick=.004)
box('Belt buckle',(0,-.099,.882),(.042,.012,.033),silver,'J_Bip_C_Hips')
box('Belt buckle inset',(0,-.107,.882),(.029,.004,.021),black,'J_Bip_C_Hips')
loft('Scarf folds',[(1.205,0,-.004,.079,.075),(1.222,0,-.006,.092,.08),(1.248,0,-.005,.079,.069),(1.274,0,.003,.065,.056)],blue,'J_Bip_C_Neck',thick=.006)
mesh('Scarf hanging end',[(-.083,-.104,1.235),(-.012,-.112,1.225),(-.024,-.121,1.11),(-.099,-.117,1.10),(-.035,-.115,.94),(-.112,-.111,.93)],[(0,1,2,3),(3,2,4,5)],blue,'J_Bip_C_Chest',.006)
for i in range(13):
    x=-.111+i*.006
    tube('Scarf fringe '+str(i),[(x,-.111,.942),(x-.004,-.112,.909+random.random()*.009)],.0018,blue)
for i in range(66):
    a=.62+(2*math.pi-1.24)*i/65
    pos=(.165*math.sin(a),.018-.077*math.cos(a),1.267+.012*math.cos(a))
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1,radius=1,location=pos)
    o=bpy.context.object;o.name='Hood fur tuft';o.scale=(.027,.025,.026+random.random()*.016)
    o.rotation_euler=(random.random(),random.random(),random.random())
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);o.data.materials.append(fur);bind(o,'J_Bip_C_UpperChest')
tube('Stethoscope tubing',[(.045,-.084,1.247),(.083,-.123,1.17),(.065,-.134,1.081),(.026,-.132,1.083),(.012,-.121,1.16)],.004,black)
tube('Stethoscope steel',[(.012,-.121,1.16),(.012,-.13,1.11),(.032,-.137,1.095)],.0028,silver)
box('Medical ID clip',(.111,-.13,1.082),(.012,.008,.019),silver)
box('Medical ID blue frame',(.113,-.132,1.048),(.037,.008,.053),blue)
box('Medical ID paper',(.113,-.138,1.048),(.031,.003,.045),badge)
box('Medical ID portrait',(.113,-.141,1.054),(.011,.002,.015),blue)
for z in [1.036,1.031]:box('ID print',(.113,-.141,z),(.021,.002,.0015),blue,bevel=0)
box('Medic sleeve patch',(.20,-.080,1.21),(.056,.005,.060),blue,'J_Bip_L_UpperArm')
box('Medic cross vertical',(.20,-.084,1.21),(.012,.003,.043),badge,'J_Bip_L_UpperArm',.001)
box('Medic cross horizontal',(.20,-.085,1.21),(.038,.003,.012),badge,'J_Bip_L_UpperArm',.001)

# Lengthen the rear hair below the shoulder while retaining every shape delta
# and the rig's spring chains. The skull and front bangs stay in place.
hair=bpy.data.objects['Hair001']
for v in hair.data.vertices:
    co=hair.matrix_world@v.co
    if co.z<1.25:
        dz=-.20*((1.25-co.z)/(.29))
        v.co.z+=dz
        if hair.data.shape_keys:
            for key in hair.data.shape_keys.key_blocks:key.data[v.index].co.z+=dz

# New geometry has no UV texture dependency; the existing face/hair maps are
# packed in the deliverable. Keep the established armature and facial keys.
assert [b.name for b in arm.data.bones]==original_bones
assert len(bpy.data.objects['Face'].data.shape_keys.key_blocks)==57
for o in list(bpy.context.scene.objects):
    if o.type in {'CAMERA','LIGHT'}:bpy.data.objects.remove(o,do_unlink=True)
bpy.ops.file.pack_all()
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'YeCheng_Winter.blend'))
bpy.ops.export_scene.vrm(filepath=str(OUT/'YeCheng_Winter.vrm'))

scene=bpy.context.scene
scene.render.engine='CYCLES';scene.cycles.samples=24
scene.render.resolution_x=900;scene.render.resolution_y=1100;scene.render.resolution_percentage=100
scene.world.color=(.14,.14,.14)
scene.view_settings.view_transform='Standard'
for loc,power,size in [((0,-3,4),180,4),((3,-1,2),110,3),((0,2,3),160,3)]:
    bpy.ops.object.light_add(type='AREA',location=loc);o=bpy.context.object;o.data.energy=power;o.data.shape='DISK';o.data.size=size;o.rotation_euler=(Vector((0,0,.9))-o.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(0,-4,.8));camera=bpy.context.object;scene.camera=camera;camera.data.type='ORTHO';camera.data.ortho_scale=1.75
for view,location in [('front',(0,-4,.8)),('back',(0,4,.8)),('side',(-4,0,.8))]:
    camera.location=location;camera.rotation_euler=(Vector((0,0,.8))-camera.location).to_track_quat('-Z','Y').to_euler()
    scene.render.filepath=str(OUT/(view+'.png'));bpy.ops.render.render(write_still=True)
report={'bones':len(original_bones),'face_shape_keys':57,'meshes':sum(o.type=='MESH' for o in scene.objects),'triangles':sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in scene.objects if o.type=='MESH')}
(OUT/'build_report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print('YECHENG_WINTER_COMPLETE',report)
