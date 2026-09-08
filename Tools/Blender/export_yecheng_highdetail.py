"""Bake the refined character, author skin weights and export its game skeleton."""
from pathlib import Path
import bpy,math,json
from mathutils import Vector,Quaternion
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'SourceAssets/Characters/YeChengHighDetail'
bpy.ops.wm.open_mainfile(filepath=str(OUT/'YeCheng_HighDetail_work.blend'))
scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.samples=1
meshes=[o for o in scene.objects if o.type=='MESH']

def clamp(x):return max(0.,min(1.,x))
def arm_amount(x,z):
 if z<.06:return 0
 return clamp((abs(x)-(.19+max(0,.40-z)*.28))/.065)
def weights(ob,co):
 x,y,z=co;side='L' if x>0 else 'R';p='J_Bip_'+side+'_';name=ob.name.lower()
 if any(w in name for w in ['hair','lock','fringe','crown']):
  h=clamp((z-.48)/.28);return {'J_Bip_C_Head':h,'J_Bip_C_UpperChest':1-h}
 if 'face' in name:return {'J_Bip_C_Head':1}
 if name=='neck':return {'J_Bip_C_Neck':1}
 if 'glove' in name:return {p+'Hand':1}
 if abs(x)>.30 and .06<z<.18:return {p+'LowerArm':1}
 if z<.18:
  if z>-.74 and (abs(x)>.18 or y>.09):
   a=clamp((.16-z)/.4)*.68;return {'J_Bip_C_Hips':1-a,p+'UpperLeg':a}
  if z<-.83:return {p+'Foot':1}
  if z<-.72:
   t=clamp((-z-.72)/.11);return {p+'LowerLeg':1-t,p+'Foot':t}
  t=clamp((z+.50)/.13);return {p+'LowerLeg':1-t,p+'UpperLeg':t}
 centers=[(.14,'Hips'),(.30,'Spine'),(.47,'Chest'),(.60,'UpperChest'),(.72,'Neck')]
 result={'J_Bip_C_UpperChest':1.}
 for (a,na),(b,nb) in zip(centers,centers[1:]):
  if a<=z<=b:
   t=(z-a)/(b-a);result={'J_Bip_C_'+na:1-t,'J_Bip_C_'+nb:t};break
 a=arm_amount(x,z)
 if a:
  t=clamp((.39-z)/.13)
  result={k:v*(1-a) for k,v in result.items()}
  result[p+'UpperArm']=a*(1-t);result[p+'LowerArm']=a*t
 return result

for ob in meshes:
 groups={}
 for v in ob.data.vertices:
  for name,w in weights(ob,v.co).items():
   if w>0:
    g=groups.get(name)
    if g is None:g=ob.vertex_groups.new(name=name);groups[name]=g
    g.add([v.index],w,'REPLACE')

# Merge only common material surfaces; keep existing non-overlapping clothing UVs.
material_groups={}
for ob in meshes:material_groups.setdefault(ob.data.materials[0].name,[]).append(ob)
baked=[];manifest={}
for name,objects in material_groups.items():
 bpy.ops.object.select_all(action='DESELECT')
 for ob in objects:ob.select_set(True)
 bpy.context.view_layer.objects.active=objects[0]
 if len(objects)>1:bpy.ops.object.join()
 ob=bpy.context.object;ob.name=name+'_Surface';mat=ob.data.materials[0]
 bs=next(n for n in mat.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
 color=list(bs.inputs['Base Color'].default_value)
 rough=bs.inputs['Roughness'].default_value
 texture=None
 if name in ['YC_HD_Reference','YC_HD_Face','YC_HD_Hair']:
  if name!='YC_HD_Reference':
   uv=ob.data.uv_layers.new(name='GameUV');ob.data.uv_layers.active=uv
   if name=='YC_HD_Face':
    for poly in ob.data.polygons:
     mapped=[]
     for index in poly.loop_indices:
      co=ob.data.vertices[ob.data.loops[index].vertex_index].co
      mapped.append([index,.5+math.atan2(co.x,.015-co.y)/math.tau,clamp((co.z-.740)/.253)])
     wrap=max(v[1] for v in mapped)-min(v[1] for v in mapped)>.5
     for index,u,v in mapped:uv.data[index].uv=(u+1 if wrap and u<.5 else u,v)
   else:
    bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT');bpy.ops.uv.smart_project(angle_limit=math.radians(75),island_margin=.012);bpy.ops.object.mode_set(mode='OBJECT')
  else:ob.data.uv_layers.active_index=0
  ob.data.uv_layers.active.active_render=True
  size=4096 if name=='YC_HD_Reference' else 2048
  im=bpy.data.images.new(name+'_BaseColor',width=size,height=size,alpha=False)
  nodes=mat.node_tree.nodes;links=mat.node_tree.links
  target=nodes.new('ShaderNodeTexImage');target.image=im;nodes.active=target
  output=next(n for n in nodes if n.type=='OUTPUT_MATERIAL');emit=nodes.new('ShaderNodeEmission')
  source=bs.inputs['Base Color'].links[0].from_socket
  links.new(source,emit.inputs['Color']);links.new(emit.outputs[0],output.inputs['Surface'])
  scene.render.bake.use_selected_to_active=False;scene.render.bake.margin=24
  print('Baking',name,flush=True);bpy.ops.object.bake(type='EMIT')
  texture=im.name+'.png';im.filepath_raw=str(OUT/texture);im.file_format='PNG';im.save();im.pack()
  nodes.clear();output=nodes.new('ShaderNodeOutputMaterial');bs=nodes.new('ShaderNodeBsdfPrincipled');tex=nodes.new('ShaderNodeTexImage');tex.image=im
  uvnode=nodes.new('ShaderNodeUVMap');uvnode.uv_map=ob.data.uv_layers.active.name;links.new(uvnode.outputs[0],tex.inputs['Vector']);links.new(tex.outputs['Color'],bs.inputs['Base Color']);links.new(bs.outputs[0],output.inputs['Surface']);bs.inputs['Roughness'].default_value=rough;bs.inputs['Specular IOR Level'].default_value=.18
  # FBX exports the active game UV first; source projection UVs remain in work.blend.
  active_name=ob.data.uv_layers.active.name
  for uv in list(ob.data.uv_layers):
   if uv.name!=active_name:ob.data.uv_layers.remove(uv)
  ob.data.uv_layers.active.name='UVMap';uvnode.uv_map='UVMap'
 manifest[name]={'color':color[:3],'roughness':rough,'metallic':0,'texture':texture,
                 'emissive_strength':{'YC_HD_Face':.35,'YC_HD_Skin':.35,'YC_HD_Reference':.12,'YC_HD_Hair':.06,'YC_HD_Fur':.08,'YC_HD_Gloves':.02}[name]}
 baked.append(ob)

def to_tpose(co,side,amount=1):
 s=1 if side=='L' else -1;pivot=Vector((s*.205,-.02,.575))
 q=Quaternion((0,1,0),math.radians(-s*63))
 return co.lerp(pivot+q@(co-pivot),amount)
def meters(co):return Vector((co[0]*.85,co[1]*.85,(co[2]+1)*.85))
for ob in baked:
 for v in ob.data.vertices:
  side='L' if v.co.x>0 else 'R';prefix='J_Bip_'+side+'_'
  amount=sum(g.weight for g in v.groups if ob.vertex_groups[g.group].name in [prefix+'UpperArm',prefix+'LowerArm',prefix+'Hand'])
  v.co=meters(to_tpose(v.co,side,amount))

bpy.ops.object.armature_add();arm=bpy.context.object;arm.name='YeCheng_HighDetail_Rig'
bpy.ops.object.mode_set(mode='EDIT');arm.data.edit_bones.remove(arm.data.edit_bones[0])
def bone(name,h,t,parent=None):
 b=arm.data.edit_bones.new(name);b.head=meters(h);b.tail=meters(t)
 if parent:b.parent=arm.data.edit_bones[parent]
bone('Root',(0,0,-1),(0,0,-.85))
parent='Root'
for name,z0,z1 in [('Hips',.14,.30),('Spine',.30,.47),('Chest',.47,.60),('UpperChest',.60,.70),('Neck',.70,.78),('Head',.78,.98)]:
 name='J_Bip_C_'+name;bone(name,(0,0,z0),(0,0,z1),parent);parent=name
for s,side in [(1,'L'),(-1,'R')]:
 p='J_Bip_'+side+'_'
 shoulder=Vector((s*.205,-.02,.575));elbow=to_tpose(Vector((s*.320,-.075,.35)),side);wrist=to_tpose(Vector((s*.414,-.112,.09)),side);hand=to_tpose(Vector((s*.439,-.122,.015)),side)
 bone(p+'Shoulder',(s*.04,0,.60),shoulder,'J_Bip_C_UpperChest');bone(p+'UpperArm',shoulder,elbow,p+'Shoulder');bone(p+'LowerArm',elbow,wrist,p+'UpperArm');bone(p+'Hand',wrist,hand,p+'LowerArm')
 for i,(finger,length) in enumerate(zip(['Index','Middle','Ring','Little'],[.057,.070,.064,.048])):
  x=s*(.417+i*.016);z=.024-i*.004;points=[(x,-.124,z),(x+s*.009,-.128,z-length*.45),(x+s*.013,-.121,z-length*.80),(x+s*.011,-.110,z-length)]
  parent=p+'Hand'
  for k in range(3):name=p+finger+str(k+1);bone(name,to_tpose(Vector(points[k]),side),to_tpose(Vector(points[k+1]),side),parent);parent=name
 points=[(s*.407,-.116,.068),(s*.387,-.128,.041),(s*.382,-.126,.015),(s*.387,-.118,0)];parent=p+'Hand'
 for k in range(3):name=p+'Thumb'+str(k+1);bone(name,to_tpose(Vector(points[k]),side),to_tpose(Vector(points[k+1]),side),parent);parent=name
 bone(p+'UpperLeg',(s*.082,0,.14),(s*.086,0,-.44),'J_Bip_C_Hips');bone(p+'LowerLeg',(s*.086,0,-.44),(s*.091,.01,-.84),p+'UpperLeg');bone(p+'Foot',(s*.091,.01,-.84),(s*.091,-.12,-.94),p+'LowerLeg');bone(p+'Toes',(s*.091,-.12,-.94),(s*.091,-.20,-.95),p+'Foot')
bpy.ops.object.mode_set(mode='OBJECT')
for ob in baked:
 ob.parent=arm;mod=ob.modifiers.new('Character skin','ARMATURE');mod.object=arm
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'YeCheng_HighDetail.blend'))
bpy.ops.object.select_all(action='DESELECT')
for ob in baked:ob.select_set(True)
bpy.context.view_layer.objects.active=baked[0];bpy.ops.object.join();ob=bpy.context.object;ob.name='YeCheng_HighDetail_Surface';arm.select_set(True)
bpy.ops.export_scene.fbx(filepath=str(OUT/'YeCheng_HighDetail.fbx'),use_selection=True,object_types={'ARMATURE','MESH'},add_leaf_bones=False,bake_anim=False,axis_forward='-Y',axis_up='Z',path_mode='COPY',embed_textures=False)
(OUT/'materials.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
report={'bones':len(arm.data.bones),'vertices':len(ob.data.vertices),'triangles':sum(len(f.vertices)-2 for f in ob.data.polygons),'materials':list(manifest),'height_m':max(v.co.z for v in ob.data.vertices)-min(v.co.z for v in ob.data.vertices)}
(OUT/'build_report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print('YECHENG_HIGHDETAIL_EXPORTED',report,flush=True)
