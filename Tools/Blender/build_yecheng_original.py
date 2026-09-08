"""Author Ye Cheng entirely from an empty scene; no imported character data.

Blender --background --factory-startup --python this_file.py
Units are metres, front is -Y. The source retains separate editable surfaces.
"""
from pathlib import Path
import math
import json
import random
import bpy
import bmesh
from mathutils import Vector, Quaternion

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'SourceAssets/Characters/YeChengOriginal'
OUT.mkdir(parents=True, exist_ok=True)
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
random.seed(42)

def material(name, color, rough=.55, metal=0):
    m=bpy.data.materials.new(name);m.diffuse_color=(*color,1);m.use_nodes=True
    bs=m.node_tree.nodes.get('Principled BSDF')
    bs.inputs['Base Color'].default_value=(*color,1)
    bs.inputs['Roughness'].default_value=rough
    bs.inputs['Metallic'].default_value=metal
    bs.inputs['Specular IOR Level'].default_value=.18 if not metal else .5
    return m

skin=material('YC_Original_Skin',(.80,.57,.49),.63)
hair=material('YC_Original_Hair',(.008,.007,.012),.68)
hairlight=material('YC_Original_HairSheen',(.014,.012,.022),.72)
white=material('YC_Original_Shell',(.76,.79,.84),.74)
blue=material('YC_Original_Slate',(.085,.13,.24),.88)
black=material('YC_Original_Thermal',(.025,.029,.038),.9)
leather=material('YC_Original_Leather',(.013,.018,.026),.43)
steel=material('YC_Original_Steel',(.43,.49,.57),.3,.78)
fur=material('YC_Original_Fur',(.72,.75,.82),.95)
sclera=material('YC_Original_Sclera',(.87,.85,.85),.35)
iris=material('YC_Original_Iris',(.12,.28,.45),.35)
pupil=material('YC_Original_Pupil',(.006,.017,.03),.3)
lip=material('YC_Original_Lips',(.39,.19,.18),.6)
glint=material('YC_Original_Glint',(.98,.99,1),.22)

def mesh(name,vs,fs,mat,sub=0):
    me=bpy.data.meshes.new(name);me.from_pydata(vs,[],fs);me.update()
    bm=bmesh.new();bm.from_mesh(me)
    bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=.000001)
    bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces))
    bm.to_mesh(me);bm.free()
    ob=bpy.data.objects.new(name,me);bpy.context.collection.objects.link(ob)
    me.materials.append(mat)
    for p in me.polygons:p.use_smooth=True
    if sub:
        mod=ob.modifiers.new('Surface refinement','SUBSURF');mod.levels=sub
        bpy.context.view_layer.objects.active=ob
        bpy.ops.object.modifier_apply(modifier=mod.name)
    return ob

def loft(name,rings,mat,n=48,start=0,end=math.tau,sub=1,cap=True):
    vs=[];fs=[]
    for z,x,y,rx,ry in rings:
        for i in range(n+1):
            a=start+(end-start)*i/n
            vs.append((x+rx*math.sin(a),y-ry*math.cos(a),z))
    for j in range(len(rings)-1):
        for i in range(n):
            k=j*(n+1)+i;fs.append((k,k+1,k+n+2,k+n+1))
    if cap and start==0:
        fs.append(tuple(reversed(range(n+1))))
        fs.append(tuple((len(rings)-1)*(n+1)+i for i in range(n+1)))
    return mesh(name,vs,fs,mat,sub)

def ellipsoid(name,pos,scale,mat):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=32,ring_count=16,location=pos)
    o=bpy.context.object;o.name=name;o.scale=scale
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    o.data.materials.append(mat)
    for p in o.data.polygons:p.use_smooth=True
    return o

def tube(name,points,radius,mat):
    c=bpy.data.curves.new(name,'CURVE');c.dimensions='3D';c.bevel_depth=radius;c.bevel_resolution=2;c.resolution_u=8
    sp=c.splines.new('BEZIER');sp.bezier_points.add(len(points)-1)
    for p,co in zip(sp.bezier_points,points):p.co=co;p.handle_left_type='AUTO';p.handle_right_type='AUTO'
    o=bpy.data.objects.new(name,c);bpy.context.collection.objects.link(o)
    bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o
    bpy.ops.object.convert(target='MESH');o=bpy.context.object;o.data.materials.append(mat)
    return o

def box(name,pos,scale,mat,bevel=.003):
    bpy.ops.mesh.primitive_cube_add(size=1,location=pos);o=bpy.context.object;o.name=name;o.scale=scale
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);o.data.materials.append(mat)
    m=o.modifiers.new('Tailored edges','BEVEL');m.width=bevel;m.segments=3
    bpy.ops.object.modifier_apply(modifier=m.name)
    for p in o.data.polygons:p.use_smooth=True
    return o

def ribbon(name,centers,widths,mat,depth=.003):
    vs=[];fs=[]
    for j,(x,y,z) in enumerate(centers):
        for k in range(7):
            t=k/3-1;vs.append((x+widths[j]*t,y-depth*(1-t*t),z))
    for j in range(len(centers)-1):
        for k in range(6):
            a=j*7+k;fs.append((a,a+1,a+8,a+7))
    o=mesh(name,vs,fs,mat,1)
    m=o.modifiers.new('Surface thickness','SOLIDIFY');m.thickness=.0015
    bpy.context.view_layer.objects.active=o;bpy.ops.object.modifier_apply(modifier=m.name)
    return o

# The facial surface is an original quad lattice with a shaped jaw, cheeks,
# orbital region and nose bridge. No base mesh is loaded.
rings=[(1.438,0,-.015,.007,.016),(1.45,0,-.005,.023,.038),
 (1.47,0,.003,.045,.051),(1.495,0,.009,.065,.061),
 (1.52,0,.012,.078,.068),(1.545,0,.012,.084,.074),
 (1.57,0,.014,.087,.080),(1.60,0,.017,.088,.084),
 (1.63,0,.02,.082,.080),(1.66,0,.021,.067,.067),
 (1.68,0,.022,.042,.045),(1.687,0,.022,.008,.01)]
face=loft('Face_original_quad_surface',rings,skin,n=64,sub=2)
for v in face.data.vertices:
    x,y,z=v.co
    if y<.012:
        front=max(0,min(1,(-y+.012)/.052))
        nose=.019*math.exp(-(x/.012)**2-((z-1.512)/.025)**2)
        orbit=.004*math.exp(-((abs(x)-.039)/.020)**2-((z-1.552)/.016)**2)
        v.co.y-=front*(nose-orbit)
loft('Neck',[(1.35,0,.011,.038,.035),(1.40,0,.012,.033,.031),(1.46,0,.014,.031,.03)],skin)
for s in [-1,1]:
    ellipsoid('Ear', (s*.084,.007,1.531),(.013,.012,.026),skin)
    # Almond ocular surfaces flatten naturally toward the outer corners.
    vs=[];fs=[]
    for j in range(9):
        v=j/8
        for i in range(25):
            t=i/24;x=s*(.015+.055*t)
            arc=math.sin(math.pi*t)**.75
            z=1.548+.004*t+arc*((1-v)*.012-v*.007)
            vs.append((x,-.064-.006*arc*math.sin(math.pi*v),z))
    for j in range(8):
        for i in range(24):
            k=j*25+i;fs.append((k,k+1,k+26,k+25))
    mesh('Almond eye white',vs,fs,sclera)
    ellipsoid('Blue grey iris',(s*.041,-.071,1.551),(.0108,.0028,.009),iris)
    ellipsoid('Pupil',(s*.041,-.0735,1.552),(.0048,.001,.006),pupil)
    ellipsoid('Eye catchlight',(s*.038,-.075,1.556),(.0023,.0008,.0023),glint)
    top=[];bottom=[]
    for i in range(13):
        t=i/12;a=math.sin(math.pi*t)**.75
        top.append((s*(.015+.055*t),-.065-.002*a,1.548+.004*t+.012*a))
        bottom.append((s*(.015+.055*t),-.065-.001*a,1.548+.004*t-.007*a))
    tube('Upper eyelash',top,.0017,hair);tube('Lower eyelid',bottom,.00065,lip)
    tube('Eyebrow',[(s*.019,-.063,1.577),(s*.042,-.063,1.581),(s*.068,-.053,1.576)],.0016,hair)
tube('Mouth line',[(-.012,-.055,1.477),(0,-.058,1.478),(.012,-.055,1.477)],.00065,lip)

# Separate tapered locks, with a central part and overlapping irregular ends.
loft('Hair scalp',[(1.568,0,.024,.091,.088),(1.62,0,.023,.095,.091),(1.667,0,.025,.078,.077),(1.697,0,.025,.044,.045),(1.704,0,.025,.002,.003)],hair,start=.83,end=math.tau-.83,n=64)
for s in [-1,1]:
    for j in range(7):
        a=j/6;root=s*(.006+.010*a);tip=s*(.016+.062*a)
        ribbon('Layered parted fringe',[(root,-.025,1.699),(s*(.015+.025*a),-.07,1.667),(s*(.014+.048*a),-.083,1.622),(tip,-.077,1.569+.026*(1-a)),(tip+s*.009,-.070,1.547+.037*(1-a))],[.008,.021,.019,.009,.0003],hair if j%3 else hairlight)
    for j in range(12):
        t=j/11;x=s*(.074+.065*t);y=.005+.095*t;z=.84+.23*random.random()
        ribbon('Long layered hair',[(s*.041,.028,1.692),(s*.085,y,1.615),(s*(.098+.012*t),y,1.46),(x,y+.015,1.25),(x+s*.025,y+.021,1.04),(x+s*.006,y+.013,z)],[.006,.021,.025,.025,.017,.0003],hair if j%4 else hairlight,.005)
    for j in range(3):
        x=s*(.088+.012*j)
        ribbon('Face framing strand',[(s*.052,-.064,1.652),(s*.082,-.057,1.554),(x,-.072,1.414),(x+s*.025,-.116,1.27),(x+s*.014,-.134,1.15-j*.04)],[.007,.014,.015,.009,.0002],hair,.004)

# Original torso and limbs stay editable under the clothing.
loft('Body torso',[(.89,0,.008,.095,.064),(.97,0,.006,.105,.072),(1.06,0,.008,.087,.062),(1.15,0,.006,.098,.07),(1.25,0,.005,.127,.076),(1.31,0,.005,.137,.060),(1.36,0,.01,.048,.037)],skin)
loft('Thermal torso',[(.91,0,.003,.103,.071),(1.02,0,.003,.097,.07),(1.11,0,.003,.10,.076),(1.22,0,.003,.132,.082),(1.30,0,.006,.141,.068),(1.34,0,.01,.045,.043),(1.40,0,.01,.042,.040)],black)
loft('Trouser pelvis',[(.84,0,.008,.118,.078),(.89,0,.004,.12,.086),(.97,0,.003,.117,.086),(.999,0,.003,.11,.08)],black)
for s in [-1,1]:
    loft('Body leg',[(.12,s*.075,.006,.032,.035),(.27,s*.076,.008,.036,.04),(.43,s*.078,.002,.046,.046),(.52,s*.078,0,.045,.05),(.69,s*.079,.008,.057,.065),(.87,s*.074,.008,.068,.075),(.94,s*.062,.008,.063,.069)],skin)
    loft('Cargo trousers',[(.19,s*.075,.01,.048,.05),(.23,s*.075,.01,.05,.054),(.26,s*.075,.008,.042,.047),(.30,s*.075,.008,.05,.052),(.42,s*.078,.002,.051,.055),(.49,s*.078,0,.052,.057),(.54,s*.078,0,.055,.059),(.68,s*.08,.008,.063,.073),(.85,s*.074,.005,.072,.081),(.95,s*.062,.005,.07,.076)],black)
    for kind,radii,mat in [('Arm',[.047,.043,.035,.028],skin),('Parka sleeve',[.078,.072,.059,.045],white)]:
        vs=[];fs=[];n=32
        for j in range(17):
            t=j/16;x=.13+.43*t;z=1.305
            f=t*3;ix=min(2,int(f));r=radii[ix]*(1-(f-ix))+radii[ix+1]*(f-ix)
            for k in range(n):
                a=math.tau*k/n;wr=1+(.045 if kind=='Parka sleeve' else 0)*math.sin(t*38+a*3)
                vs.append((s*x,.006+math.cos(a)*r*wr,z+math.sin(a)*r*wr))
        for j in range(16):
            for k in range(n):
                a=j*n+k;b=j*n+(k+1)%n;fs.append((a,b,b+n,a+n))
        mesh(kind,vs,fs,mat,1)
    ellipsoid('Glove palm',(s*.599,.006,1.305),(.045,.019,.034),leather)
    for j in range(4):
        z=1.28+j*.017
        ob=tube('Gloved finger',[(s*.619,.006,z),(s*(.66-.006*abs(j-1)),.005,z),(s*(.687-.009*abs(j-1)),.004,z-.004)],.007,leather)
        ob['finger']=j
    ob=tube('Gloved thumb',[(s*.589,-.002,1.276),(s*.609,-.009,1.257),(s*.638,-.01,1.255)],.009,leather)
    ob['finger']=4
    loft('Combat boot shaft',[(.065,s*.075,.012,.05,.057),(.12,s*.075,.015,.048,.052),(.23,s*.075,.013,.052,.056)],leather)
    box('Boot vamp',(s*.075,-.038,.063),(.105,.21,.089),leather,.028)
    box('Rubber outsole',(s*.075,-.038,.024),(.112,.223,.032),black,.011)
    for j in range(7):
        z=.085+j*.019
        tube('Boot crossed lace',[(s*.075-.032,-.055,z),(s*.075+.032,-.053,z+.017)],.0017,black)
        tube('Boot crossed lace',[(s*.075+.032,-.055,z),(s*.075-.032,-.053,z+.017)],.0017,black)
    for j in range(6):
        box('Boot tread lug',(s*.075,-.132+j*.037,.013),(.119,.016,.024),black,.003)
    for i in range(18):
        a=random.uniform(-1.2,1.2);x=s*.075+math.sin(a)*.049
        ellipsoid('Boot snow crust',(x,-.141+.027*abs(a),.036+random.random()*.041),(.006+random.random()*.008,.002,.003),fur)

coat_rings=[(.43,0,.025,.232,.13),(.46,0,.023,.227,.128),(.63,0,.018,.20,.119),(.83,0,.012,.16,.107),(1.02,0,.005,.125,.09),(1.16,0,.006,.142,.097),(1.28,0,.011,.158,.09),(1.325,0,.014,.133,.063)]
coat=loft('Tailored long parka',coat_rings,white,start=.49,end=math.tau-.49,n=72,sub=2,cap=False)
for v in coat.data.vertices:
    x,y,z=v.co;a=math.atan2(x,-y)
    wave=.003*math.sin(a*19+z*5)*max(0,(1.15-z))
    v.co.x+=math.sin(a)*wave;v.co.y-=math.cos(a)*wave
lining=loft('Slate coat lining',[(z,x,y,rx-.005,ry-.005) for z,x,y,rx,ry in coat_rings],blue,start=.49,end=math.tau-.49,n=72,sub=2,cap=False)
for s in [-1,1]:
    edge=[(s*rx*math.sin(.49),y-ry*math.cos(.49)-.002,z) for z,x,y,rx,ry in coat_rings]
    tube('Front coat piping',edge,.0025,steel)
    for i in range(10):
        t=i/9;z=.48+.80*t;x=.108-.048*t;y=-.094+.013*t
        ellipsoid('Coat press stud',(s*x,y-.02,z),(.0035,.002,.0035),steel)
    ribbon('Shoulder harness',[(s*.121,-.064,1.323),(s*.116,-.10,1.24),(s*.114,-.101,1.12),(s*.102,-.091,.99),(s*.102,-.089,.82)],[.010]*5,leather)
    for z in [1.23,1.12,.99,.86]:
        box('Harness buckle',(s*.112,-.112,z),(.029,.009,.032),steel)
        box('Buckle void',(s*.112,-.118,z),(.018,.003,.021),leather,.001)
    box('Thigh medical pouch',(s*.136,-.044,.78),(.062,.045,.121),leather,.006)
    box('Medical pouch flap',(s*.136,-.07,.822),(.065,.009,.038),black)
    box('Medical emblem upright',(s*.136,-.072,.78),(.011,.002,.039),white,.001)
    box('Medical emblem crossbar',(s*.136,-.073,.78),(.035,.002,.012),white,.001)
    ribbon('Hanging utility webbing',[(s*.108,-.107,.95),(s*.137,-.116,.78),(s*.151,-.112,.60),(s*.161,-.108,.36)],[.007]*4,leather)
    box('Webbing end buckle',(s*.161,-.113,.38),(.021,.007,.031),steel)
    box('Webbing buckle inset',(s*.161,-.118,.38),(.012,.002,.021),black,.001)
    mesh('Blue hood lapel',[(s*.05,-.06,1.393),(s*.15,-.058,1.35),(s*.128,-.106,1.25),(s*.06,-.098,1.33)],[(0,1,2,3)],blue,0)
    tube('Outer cuff welt',[(s*.548,-.038,1.27),(s*.548,-.046,1.305),(s*.548,-.038,1.34)],.004,leather)
box('Sleeve medic patch',(.23,-.073,1.305),(.060,.006,.065),blue)
box('Sleeve cross upright',(.23,-.077,1.305),(.012,.002,.044),white,.001)
box('Sleeve cross bar',(.23,-.078,1.305),(.039,.002,.012),white,.001)
loft('Utility belt',[(.965,0,.003,.117,.085),(.995,0,.003,.117,.085)],leather)
box('Belt steel buckle',(0,-.086,.98),(.042,.01,.032),steel)
box('Belt aperture',(0,-.093,.98),(.031,.003,.022),black,.001)
loft('Wrapped scarf',[(1.315,0,-.01,.071,.069),(1.335,0,-.014,.089,.076),(1.361,0,-.009,.081,.068),(1.384,0,0,.063,.05),(1.404,0,.009,.047,.045)],blue)
ribbon('Scarf tail',[(-.045,-.087,1.37),(-.065,-.108,1.28),(-.055,-.109,1.17),(-.065,-.10,1.06)],[.035,.036,.033,.039],blue,.009)
for i in range(16):tube('Scarf tassel',[(-.103+i*.005,-.103,1.066),(-.107+i*.005,-.103,1.031+random.random()*.012)],.0012,blue)

# Fur is formed from tapered tufts following the collar, not a sphere chain.
collar_points=[(.164*math.sin(a),.03-.075*math.cos(a),1.343) for a in [.55+i*(math.tau-1.1)/32 for i in range(33)]]
tube('Hood padded fur base',collar_points,.028,fur)
for i in range(130):
    a=.55+(math.tau-1.1)*random.random();r=.135+random.uniform(-.015,.027)
    x=(r+.02)*math.sin(a);y=.022-.08*math.cos(a);z=1.34+random.uniform(-.028,.026)
    ribbon('Collar fur tuft',[(x,y,z),(x*1.07,y-.008,z+.007),(x*1.15,y-.01,z+.021)],[.009,.011,.0001],fur,.004)
for s in [-1,1]:
    for i in range(35):
        a=math.tau*i/35;x=s*.075+.053*math.sin(a);y=.013-.057*math.cos(a)
        ribbon('Boot fur tuft',[(x,y,.218),(x,y-.002,.232),(x+.003,y,.247+random.random()*.01)],[.006,.009,.0001],fur)
tube('Stethoscope',[(-.013,-.09,1.36),(.052,-.12,1.31),(.069,-.127,1.225),(.035,-.128,1.218),(.018,-.12,1.285)],.003,leather)
ellipsoid('Stethoscope bell',(.069,-.133,1.226),(.012,.004,.012),steel)
box('ID card frame',(.12,-.123,1.17),(.033,.005,.047),blue)
box('ID card paper',(.12,-.127,1.17),(.028,.002,.039),white)
box('ID portrait',(.12,-.129,1.176),(.009,.001,.012),blue,.001)
for z in [1.158,1.163]:box('ID printed line',(.12,-.129,z),(.019,.001,.001),blue,.0002)

# Author a fresh anatomical skeleton using the game's humanoid naming convention.
bpy.ops.object.armature_add();arm=bpy.context.object;arm.name='YeCheng_Original_Rig'
bpy.ops.object.mode_set(mode='EDIT');arm.data.edit_bones.remove(arm.data.edit_bones[0])
def bone(name,head,tail,parent=None):
    b=arm.data.edit_bones.new(name);b.head=head;b.tail=tail
    if parent:b.parent=arm.data.edit_bones[parent]
    return b
bone('Root',(0,0,0),(0,0,.12))
chain=[('Hips',.93,1.04),('Spine',1.04,1.17),('Chest',1.17,1.27),('UpperChest',1.27,1.35),('Neck',1.35,1.45),('Head',1.45,1.68)]
parent='Root'
for name,z0,z1 in chain:
    name='J_Bip_C_'+name;bone(name,(0,.01,z0),(0,.01,z1),parent);parent=name
for s,side in [(1,'L'),(-1,'R')]:
    p='J_Bip_'+side+'_'
    bone(p+'Shoulder',(s*.03,.006,1.305),(s*.15,.006,1.305),'J_Bip_C_UpperChest')
    bone(p+'UpperArm',(s*.15,.006,1.305),(s*.36,.006,1.305),p+'Shoulder')
    bone(p+'LowerArm',(s*.36,.006,1.305),(s*.56,.006,1.305),p+'UpperArm')
    bone(p+'Hand',(s*.56,.006,1.305),(s*.64,.006,1.305),p+'LowerArm')
    for j,finger in enumerate(['Little','Ring','Middle','Index','Thumb']):
        z=1.28+j*.017
        points=[(s*.619,.006,z),(s*.645,.005,z),(s*.665,.004,z-.002),(s*.685,.004,z-.004)] if j<4 else [(s*.589,-.002,1.276),(s*.609,-.009,1.257),(s*.624,-.01,1.256),(s*.638,-.01,1.255)]
        parent=p+'Hand'
        for k in range(3):
            name=p+finger+str(k+1);bone(name,points[k],points[k+1],parent);parent=name
    bone(p+'UpperLeg',(s*.075,.008,.93),(s*.078,0,.51),'J_Bip_C_Hips')
    bone(p+'LowerLeg',(s*.078,0,.51),(s*.075,.013,.12),p+'UpperLeg')
    bone(p+'Foot',(s*.075,.013,.12),(s*.075,-.10,.06),p+'LowerLeg')
    bone(p+'Toes',(s*.075,-.10,.06),(s*.075,-.14,.06),p+'Foot')
bpy.ops.object.mode_set(mode='OBJECT')

def weights(o,v):
    x,y,z=o.matrix_world@v.co;side='L' if x>0 else 'R';p='J_Bip_'+side+'_';name=o.name.lower()
    if any(t in name for t in ['hair','fringe','strand','face','eye','iris','pupil','catchlight','mouth','ear']):
        if z<1.40 and ('hair' in name or 'strand' in name):
            t=max(.15,min(1,(z-1.15)/.25));return {'J_Bip_C_Head':t,'J_Bip_C_UpperChest':1-t}
        return {'J_Bip_C_Head':1}
    if 'finger' in o:
        finger=['Little','Ring','Middle','Index','Thumb'][o['finger']]
        k=1 if abs(x)<.645 else (2 if abs(x)<.665 else 3)
        if finger=='Thumb':k=1 if abs(x)<.611 else (2 if abs(x)<.625 else 3)
        return {p+finger+str(k):1}
    if 'glove' in name or 'finger' in name or 'thumb' in name:return {p+'Hand':1}
    if 'sleeve' in name or 'cuff' in name or name.startswith('arm'):
        t=max(0,min(1,(abs(x)-.31)/.10));return {p+'UpperArm':1-t,p+'LowerArm':t}
    if 'boot' in name or 'outsole' in name:return {p+'Foot':1}
    if 'leg' in name or 'trousers' in name:
        t=max(0,min(1,(z-.46)/.10));return {p+'LowerLeg':1-t,p+'UpperLeg':t}
    if z<1.01:return {'J_Bip_C_Hips':1}
    if z>1.4:return {'J_Bip_C_Head':1}
    centers=[(1.0,'Hips'),(1.12,'Spine'),(1.24,'Chest'),(1.33,'UpperChest'),(1.40,'Neck')]
    for (za,a),(zb,b) in zip(centers,centers[1:]):
        if za<=z<=zb:
            t=(z-za)/(zb-za);return {'J_Bip_C_'+a:1-t,'J_Bip_C_'+b:t}
    return {'J_Bip_C_UpperChest':1}

meshes=[o for o in bpy.context.scene.objects if o.type=='MESH']
for o in meshes:
    for v in o.data.vertices:
        for name,w in weights(o,v).items():
            if w>0:
                g=o.vertex_groups.get(name) or o.vertex_groups.new(name=name);g.add([v.index],w,'REPLACE')
    o.parent=arm;m=o.modifiers.new('Original skin weights','ARMATURE');m.object=arm
    bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o
    bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT');bpy.ops.uv.smart_project(island_margin=.02);bpy.ops.object.mode_set(mode='OBJECT')

# Keep the complete original body in the editable source, excluding covered
# surfaces from the game export to prevent poke-through and wasted draw work.
for o in meshes:
    if o.name.startswith(('Body torso','Body leg','Arm.')) or o.name=='Arm':
        o.hide_render=True;o.hide_set(True)

bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'YeCheng_Original.blend'))
(OUT/'materials.json').write_text(json.dumps({m.name:{'color':list(m.diffuse_color)[:3],'roughness':m.node_tree.nodes.get('Principled BSDF').inputs['Roughness'].default_value,'metallic':m.node_tree.nodes.get('Principled BSDF').inputs['Metallic'].default_value} for m in bpy.data.materials if m.name.startswith('YC_Original_')},indent=2),encoding='utf-8')
report={'source':'empty scene; all meshes, materials, UVs and weights authored in this script','meshes':len(meshes),'bones':len(arm.data.bones),'triangles':sum(len(p.vertices)-2 for o in meshes if not o.hide_render for p in o.data.polygons)}
(OUT/'build_report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
bpy.ops.object.select_all(action='DESELECT')
for o in meshes:
    if not o.hide_render:o.select_set(True)
bpy.context.view_layer.objects.active=next(o for o in meshes if not o.hide_render)
bpy.ops.object.join()
bpy.context.object.name='YeCheng_Original_Surface'
arm.select_set(True)
bpy.ops.export_scene.fbx(filepath=str(OUT/'YeCheng_Original.fbx'),use_selection=True,object_types={'ARMATURE','MESH'},add_leaf_bones=False,bake_anim=False,axis_forward='-Y',axis_up='Z')
scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.samples=24
for side,angle in [('L',70),('R',-70)]:
    pb=arm.pose.bones['J_Bip_'+side+'_UpperArm'];q=pb.bone.matrix_local.to_quaternion()
    pb.rotation_mode='QUATERNION';pb.rotation_quaternion=q.inverted()@Quaternion((0,1,0),math.radians(angle))@q
scene.render.resolution_x=1000;scene.render.resolution_y=1300;scene.render.resolution_percentage=100
scene.world.color=(.25,.25,.25);scene.view_settings.view_transform='AgX'
for pos,power,size in [((0,-3,3),260,3),((2,-1,2),160,3),((0,2,3),220,2)]:
    bpy.ops.object.light_add(type='AREA',location=pos);o=bpy.context.object;o.data.energy=power;o.data.shape='DISK';o.data.size=size;o.rotation_euler=(Vector((0,0,1))-o.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add();cam=bpy.context.object;scene.camera=cam;cam.data.type='ORTHO';cam.data.ortho_scale=1.88
for view,pos,target,scale in [('front',(0,-4,.86),(0,0,.86),1.88),('face',(0,-4,1.565),(0,0,1.565),.36),('side',(-4,0,.86),(0,0,.86),1.88),('back',(0,4,.86),(0,0,.86),1.88)]:
    cam.location=pos;cam.rotation_euler=(Vector(target)-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.ortho_scale=scale
    scene.render.filepath=str(OUT/(view+'.png'));bpy.ops.render.render(write_still=True)
print('YECHENG_ORIGINAL_BUILT',report)
