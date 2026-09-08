"""Render the bound character in idle, profile and a walking pose."""
import bpy,math
from pathlib import Path
from mathutils import Vector,Quaternion
O=Path(__file__).resolve().parents[2]/'SourceAssets/Characters/YeChengHighDetail'
bpy.ops.wm.open_mainfile(filepath=str(O/'YeCheng_HighDetail.blend'))
scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.samples=16;scene.view_settings.view_transform='Standard'
scene.world=bpy.data.worlds.new('Verification');scene.world.use_nodes=True;scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.5,.5,.5,1)
arm=next(o for o in scene.objects if o.type=='ARMATURE')
def rot(name,axis,deg):
 p=arm.pose.bones[name];q=p.bone.matrix_local.to_quaternion();p.rotation_mode='QUATERNION';p.rotation_quaternion=q.inverted()@Quaternion(axis,math.radians(deg))@q
for side,d in [('L',70),('R',-70)]:rot('J_Bip_'+side+'_UpperArm',(0,1,0),d)
d=bpy.data.lights.new('Softbox','AREA');o=bpy.data.objects.new('Softbox',d);scene.collection.objects.link(o);o.location=(1,-3,3);d.energy=80;d.size=4;o.rotation_euler=(Vector((0,0,1))-o.location).to_track_quat('-Z','Y').to_euler()
c=bpy.data.cameras.new('Camera');cam=bpy.data.objects.new('Camera',c);scene.collection.objects.link(cam);scene.camera=cam;c.type='ORTHO'
scene.render.resolution_x=1000;scene.render.resolution_y=1300;scene.render.resolution_percentage=100
for name,angle,z,scale in [('bound_idle',15,.85,1.9),('bound_face',0,1.57,.31),('bound_side',90,.85,1.9),('bound_walk',25,.85,1.9)]:
 if name=='bound_walk':rot('J_Bip_L_UpperLeg',(1,0,0),22);rot('J_Bip_R_UpperLeg',(1,0,0),-18);rot('J_Bip_R_LowerLeg',(1,0,0),24)
 a=math.radians(angle);target=Vector((0,0,z));cam.location=target+Vector((4*math.sin(a),-4*math.cos(a),0));cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler();c.ortho_scale=scale;scene.render.filepath=str(O/(name+'.png'));bpy.ops.render.render(write_still=True)
print('BINDING_PREVIEW_COMPLETE')
