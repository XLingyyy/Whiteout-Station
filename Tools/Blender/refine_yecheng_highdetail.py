"""Refine the generated clothing study with an authored face and layered hair."""
import bpy,bmesh,math,json
import numpy as np
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[2]
W=ROOT/'Artifacts/YeChengHighDetail'
OUT=ROOT/'SourceAssets/Characters/YeChengHighDetail'
OUT.mkdir(parents=True,exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=str(OUT/'YeCheng_ClothingSculpt.glb'))
body=next(o for o in bpy.context.scene.objects if o.type=='MESH');body.name='YeCheng tailored clothing sculpt'
bpy.context.view_layer.objects.active=body
bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
allco=np.array([v.co[:] for v in body.data.vertices]);lo=allco.min(0);hi=allco.max(0)
bm=bmesh.new();bm.from_mesh(body.data)
remove=[]
for v in bm.verts:
 x,y,z=v.co
 if z>.735 or (y>.11 and abs(x)<.25 and z>.12) or (abs(x)>.375 and -.2<z<.085):remove.append(v)
bmesh.ops.delete(bm,geom=remove,context='VERTS');bm.to_mesh(body.data);bm.free()
dec=body.modifiers.new('Game surface reduction','DECIMATE');dec.ratio=min(1,140000/len(body.data.polygons));bpy.ops.object.modifier_apply(modifier=dec.name)

def surface(name,vs,fs):
 me=bpy.data.meshes.new(name);me.from_pydata(vs,[],fs);me.update()
 ob=bpy.data.objects.new(name,me);bpy.context.collection.objects.link(ob)
 bm=bmesh.new();bm.from_mesh(me);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=.00001);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(me);bm.free()
 for p in me.polygons:p.use_smooth=True
 return ob

def loft(name,rings,n=96):
 vs=[];fs=[]
 for z,rx,ry,cy in rings:
  for i in range(n):
   a=math.tau*i/n;x=rx*math.sin(a);y=cy-ry*math.cos(a)
   if name=='Face quad surface' and math.cos(a)>0:
    front=math.cos(a)**4
    nose=.022*math.exp(-(x/.017)**2-((z-.812)/.020)**2)
    bridge=.007*math.exp(-(x/.012)**2-((z-.839)/.032)**2)
    cheeks=.004*sum(math.exp(-((x-s*.047)/.025)**2-((z-.814)/.023)**2) for s in [-1,1])
    sockets=.003*sum(math.exp(-((x-s*.043)/.021)**2-((z-.846)/.010)**2) for s in [-1,1])
    y-=front*(nose+bridge+cheeks-sockets)
   vs.append((x,y,z))
 for j in range(len(rings)-1):
  for i in range(n):a=j*n+i;b=j*n+(i+1)%n;fs.append((a,b,b+n,a+n))
 fs.extend([tuple(reversed(range(n))),tuple((len(rings)-1)*n+i for i in range(n))])
 return surface(name,vs,fs)

face=loft('Face quad surface',[(.741,.007,.022,-.045),(.749,.023,.039,-.023),(.761,.041,.054,-.008),(.777,.058,.066,.002),(.794,.071,.076,.005),(.813,.080,.085,.008),(.834,.083,.091,.010),(.855,.084,.095,.012),(.878,.087,.098,.015),(.904,.09,.101,.017),(.930,.086,.098,.018),(.954,.073,.085,.020),(.972,.054,.065,.020),(.986,.029,.038,.020),(.992,.001,.003,.020)])
sub=face.modifiers.new('Facial quad subdivision','SUBSURF');sub.levels=2
bpy.context.view_layer.objects.active=face;bpy.ops.object.modifier_apply(modifier=sub.name)
neck=loft('Neck',[(.69,.036,.035,.005),(.72,.034,.034,.005),(.77,.034,.035,.005)],48)

def strand(name,points,widths,depth=.008):
 vs=[];fs=[]
 # Catmull-Rom interpolation produces tapering curved locks with a crowned section.
 pts=[Vector(p) for p in points]
 for j in range((len(pts)-1)*8+1):
  q=j/8;i=min(int(q),len(pts)-2);t=q-i
  a=pts[max(0,i-1)];b=pts[i];c=pts[i+1];d=pts[min(len(pts)-1,i+2)]
  p=.5*((2*b)+(-a+c)*t+(2*a-5*b+4*c-d)*t*t+(-a+3*b-3*c+d)*t*t*t)
  width=widths[i]*(1-t)+widths[i+1]*t
  for k in range(9):
   u=k/4-1;vs.append(tuple(p+Vector((u*width,-depth*(1-u*u),0))))
 rows=len(vs)//9
 for j in range(rows-1):
  for k in range(8):a=j*9+k;fs.append((a,a+1,a+10,a+9))
 ob=surface(name,vs,fs)
 sol=ob.modifiers.new('Hair lock thickness','SOLIDIFY');sol.thickness=.002
 bpy.context.view_layer.objects.active=ob;bpy.ops.object.modifier_apply(modifier=sol.name)
 return ob

# A smooth crown sits behind independent locks. Length and width vary per lock.
cap=loft('Hair crown',[(.864,.109,.106,.034),(.90,.116,.118,.031),(.939,.11,.118,.03),(.97,.090,.10,.03),(.99,.062,.074,.03),(1.002,.022,.03,.03),(1.005,.001,.002,.03)])
sub=cap.modifiers.new('Smooth crown','SUBSURF');sub.levels=2;bpy.context.view_layer.objects.active=cap;bpy.ops.object.modifier_apply(modifier=sub.name)
for i in range(22):
 t=(i+.5)/22;ang=math.pi*(.09+.82*t);x=.118*math.cos(ang);y=.028+.108*math.sin(ang)
 end=.07+.09*(.5+.5*math.sin(i*2.4))
 strand('Back hair lock %02d'%i,[(x*.3,.068,.995),(x,y,.88),(x*1.55,y+.045,.61),(x*1.85+.01*math.sin(i),y+.03,.31),(x*1.70,y+.015,end)], [.013,.022,.020,.015,.0008],-.006)
for side in [-1,1]:
 for i in range(4):
  x=side*(.015+i*.021)
  end=([.856,.836,.859,.877] if side<0 else [.88,.869,.858,.882])[i]
  strand('Parted fringe %s %s'%(side,i),[(-.010,.012,1.003),(x*.60-.009,-.071,.966),(x,-.110,.916),(x+side*.014,-.121,end)],[.006,.020,.018,.0003],.008)
 for i in range(5):
  x=side*(.093+i*.01)
  strand('Temple lock %s %s'%(side,i),[(side*.075,-.028,.97),(x,-.091,.84),(x+side*.009,-.09,.68),(x+side*(.027+i*.009),-.09+i*.012,.43-i*.035)],[.014,.018,.014,.0005],.006)

# Five separate tapered fingers, with bent distal segments and a thumb web.
def glove_piece(name,points,radii):
 vs=[];fs=[];n=12
 for i,p in enumerate(points):
  tangent=Vector(points[min(i+1,len(points)-1)])-Vector(points[max(0,i-1)])
  tangent.normalize();u=tangent.cross(Vector((0,1,0))).normalized();v=tangent.cross(u).normalized()
  for j in range(n):a=j*math.tau/n;vs.append(tuple(Vector(p)+radii[i]*(math.cos(a)*u+.7*math.sin(a)*v)))
 for i in range(len(points)-1):
  for j in range(n):a=i*n+j;b=i*n+(j+1)%n;fs.append((a,b,b+n,a+n))
 fs.extend([tuple(reversed(range(n))),tuple((len(points)-1)*n+j for j in range(n))])
 return surface(name,vs,fs)
hands=[]
for s in [-1,1]:
 hands.append(glove_piece('Glove palm %s'%s,[(s*.409,-.01,.10),(s*.419,-.016,.065),(s*.435,-.02,.027),(s*.441,-.017,.012)],[.023,.028,.027,.020]))
 for i,length in enumerate([.057,.070,.064,.048]):
  x=s*(.417+i*.016);z=.024-i*.004
  hands.append(glove_piece('Glove finger %s %s'%(s,i),[(x,-.024,z),(x+s*.009,-.028,z-length*.45),(x+s*.013,-.021,z-length*.80),(x+s*.011,-.010,z-length),(x+s*.011,-.009,z-length-.002)],[.010,.009,.008,.006,.002]))
 hands.append(glove_piece('Glove thumb %s'%s,[(s*.407,-.016,.068),(s*.387,-.028,.041),(s*.382,-.026,.015),(s*.387,-.018,.0)],[.013,.012,.009,.003]))

# Preserve the supplied artwork's detail through aligned projection UVs.
mat=bpy.data.materials.new('YC_HD_Reference');mat.use_nodes=True
n=mat.node_tree.nodes;l=mat.node_tree.links;n.clear();out=n.new('ShaderNodeOutputMaterial');p=n.new('ShaderNodeBsdfPrincipled');p.inputs['Roughness'].default_value=.85;p.inputs['Specular IOR Level'].default_value=.08;l.new(p.outputs[0],out.inputs['Surface'])
texs=[];bounds={}
for view in ['front','back']:
 im=bpy.data.images.load(str(W/(view+'_input.png')));im.pack()
 arr=np.empty(im.size[0]*im.size[1]*4,dtype=np.float32);im.pixels.foreach_get(arr);ys,xs=np.where(arr.reshape(im.size[1],im.size[0],4)[:,:,3]>.5)
 bounds[view]=(xs.min()/im.size[0],xs.max()/im.size[0],ys.min()/im.size[1],ys.max()/im.size[1])
 tex=n.new('ShaderNodeTexImage');tex.image=im;tex.extension='EXTEND';uv=n.new('ShaderNodeUVMap');uv.uv_map=view;l.new(uv.outputs[0],tex.inputs[0]);texs.append(tex)
geo=n.new('ShaderNodeNewGeometry');sep=n.new('ShaderNodeSeparateXYZ');l.new(geo.outputs['Normal'],sep.inputs[0]);r=n.new('ShaderNodeMapRange');r.inputs['From Min'].default_value=-.4;r.inputs['From Max'].default_value=.4;l.new(sep.outputs['Y'],r.inputs['Value']);mix=n.new('ShaderNodeMixRGB');l.new(r.outputs[0],mix.inputs[0]);l.new(texs[0].outputs['Color'],mix.inputs[1]);l.new(texs[1].outputs['Color'],mix.inputs[2]);l.new(mix.outputs[0],p.inputs['Base Color']);l.new(mix.outputs[0],p.inputs['Emission Color']);p.inputs['Emission Strength'].default_value=.15
glove=bpy.data.materials.new('YC_HD_Gloves');glove.diffuse_color=(.014,.019,.027,1);glove.use_nodes=True;gb=glove.node_tree.nodes.get('Principled BSDF');gb.inputs['Base Color'].default_value=glove.diffuse_color;gb.inputs['Roughness'].default_value=.58
hair=bpy.data.materials.new('YC_HD_Hair');hair.use_nodes=True
hn=hair.node_tree.nodes;hl=hair.node_tree.links;hb=hn.get('Principled BSDF')
hb.inputs['Roughness'].default_value=.56;hb.inputs['Specular IOR Level'].default_value=.24
noise=hn.new('ShaderNodeTexNoise');noise.inputs['Scale'].default_value=3;noise.inputs['Detail'].default_value=0
tc=hn.new('ShaderNodeTexCoord');mapping=hn.new('ShaderNodeVectorMath');mapping.operation='MULTIPLY';mapping.inputs[1].default_value=(2,2,.1);hl.new(tc.outputs['Generated'],mapping.inputs[0]);hl.new(mapping.outputs[0],noise.inputs['Vector'])
ramp=hn.new('ShaderNodeValToRGB');ramp.color_ramp.elements[0].position=.15;ramp.color_ramp.elements[0].color=(.010,.012,.021,1);ramp.color_ramp.elements[1].position=.85;ramp.color_ramp.elements[1].color=(.065,.062,.087,1);hl.new(noise.outputs['Fac'],ramp.inputs[0]);hl.new(ramp.outputs['Color'],hb.inputs['Base Color'])
face_mat=mat.copy();face_mat.name='YC_HD_Face'
fn=face_mat.node_tree.nodes;fl=face_mat.node_tree.links;ft=next(node for node in fn if node.type=='TEX_IMAGE' and node.image.name.startswith('front_input'));fb=next(node for node in fn if node.type=='BSDF_PRINCIPLED')
ft.image=bpy.data.images.load(str(OUT/'FacePaint.png'));ft.image.pack()
faceuv=fn.new('ShaderNodeUVMap');faceuv.uv_map='facepaint';fl.new(faceuv.outputs[0],ft.inputs[0])
fl.new(ft.outputs['Color'],fb.inputs['Base Color']);fl.new(ft.outputs['Color'],fb.inputs['Emission Color'])
fg=fn.new('ShaderNodeNewGeometry');fs=fn.new('ShaderNodeSeparateXYZ');fl.new(fg.outputs['Normal'],fs.inputs[0]);fr=fn.new('ShaderNodeMapRange');fr.inputs['From Min'].default_value=-.65;fr.inputs['From Max'].default_value=-.25;fl.new(fs.outputs['Y'],fr.inputs['Value'])
fm=fn.new('ShaderNodeMixRGB');fm.inputs[2].default_value=(.72,.52,.46,1);fl.new(fr.outputs[0],fm.inputs[0]);fl.new(ft.outputs['Color'],fm.inputs[1]);fl.new(fm.outputs[0],fb.inputs['Base Color']);fl.new(fm.outputs[0],fb.inputs['Emission Color'])
skin=bpy.data.materials.new('YC_HD_Skin');skin.use_nodes=True;sb=skin.node_tree.nodes.get('Principled BSDF');sb.inputs['Base Color'].default_value=(.72,.52,.46,1);sb.inputs['Roughness'].default_value=.8
for ob in [o for o in bpy.context.scene.objects if o.type=='MESH']:
 chosen=glove if ob in hands else face_mat if ob==face else skin if ob==neck else hair if ob!=body else mat
 ob.data.materials.clear();ob.data.materials.append(chosen)
 for f in ob.data.polygons:f.use_smooth=True
 co=np.array([v.co[:] for v in ob.data.vertices]);indices=np.empty(len(ob.data.loops),dtype=np.int32);ob.data.loops.foreach_get('vertex_index',indices)
 for view,(x0,x1,y0,y1) in bounds.items():
  uv=ob.data.uv_layers.new(name=view);u=(co[:,0]-lo[0])/(hi[0]-lo[0]);v=(co[:,2]-lo[2])/(hi[2]-lo[2])
  if view=='back':u=1-u
  uv.data.foreach_set('uv',np.stack([x0+u*(x1-x0),y0+v*(y1-y0)],axis=1)[indices].astype(np.float32).ravel())
 if ob in [face,neck]:
  uv=ob.data.uv_layers.new(name='facepaint');u=.5+co[:,0]*3.31
  y=np.interp(co[:,2],[.69,.741,.78,.812,.844,.877,.94,.992],[1240,1074,913,778,605,480,220,15])
  uv.data.foreach_set('uv',np.stack([u,1-y/1278],axis=1)[indices].astype(np.float32).ravel())
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'YeCheng_HighDetail_work.blend'))
scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.samples=24;scene.view_settings.view_transform='Standard';scene.world=bpy.data.worlds.new('Studio');scene.world.use_nodes=True;scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.20,.20,.20,1)
for pos,power,size in [((1,-3,3),100,4),((-2,-1,1),50,3)]:
 d=bpy.data.lights.new('Softbox','AREA');o=bpy.data.objects.new('Softbox',d);scene.collection.objects.link(o);o.location=pos;d.energy=power;d.size=size;o.rotation_euler=(Vector((0,0,0))-o.location).to_track_quat('-Z','Y').to_euler()
cam=bpy.data.cameras.new('Camera');ob=bpy.data.objects.new('Camera',cam);scene.collection.objects.link(ob);scene.camera=ob;cam.type='ORTHO';scene.render.resolution_x=1000;scene.render.resolution_y=1300;scene.render.resolution_percentage=100
for name,angle,z,scale in [('front',0,0,2.16),('quarter',35,0,2.16),('face',0,.858,.35),('face_side',65,.858,.35),('back',180,0,2.16)]:
 a=math.radians(angle);target=Vector((0,0,z));ob.location=target+Vector((math.sin(a)*4,-math.cos(a)*4,0));ob.rotation_euler=(target-ob.location).to_track_quat('-Z','Y').to_euler();cam.ortho_scale=scale;scene.render.filepath=str(OUT/(name+'.png'));bpy.ops.render.render(write_still=True)
print('YECHENG_REFINED_STUDY_COMPLETE',flush=True)
