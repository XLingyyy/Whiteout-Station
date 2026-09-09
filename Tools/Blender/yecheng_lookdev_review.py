"""Fixed-condition review renders. Opens existing assets; never rebuilds the body."""
from pathlib import Path
import bpy, math, sys, json
from mathutils import Vector

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'SourceAssets/Characters/YeChengLookdev'
args=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else ['baseline']
version=args[0]
source=ROOT/'SourceAssets/Characters/YeChengHighDetail/YeCheng_HighDetail_work.blend' if version=='baseline' else OUT/(version+'.blend')
bpy.ops.wm.open_mainfile(filepath=str(source))
scene=bpy.context.scene
for ob in list(scene.objects):
    if ob.type in {'LIGHT','CAMERA'}: bpy.data.objects.remove(ob,do_unlink=True)
scene.render.engine='CYCLES';scene.cycles.samples=16
scene.cycles.use_denoising=True
scene.view_settings.view_transform='Standard';scene.view_settings.look='None'
scene.view_settings.exposure=0;scene.view_settings.gamma=1
scene.render.use_motion_blur=False
scene.render.resolution_x=800;scene.render.resolution_y=900;scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG';scene.render.film_transparent=False
scene.world=bpy.data.worlds.new('Lookdev fixed neutral world');scene.world.use_nodes=True
scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.18,.18,.18,1)
scene.world.node_tree.nodes['Background'].inputs[1].default_value=.6
lights=[((-.8,-1.5,1.6),19.5,1.2),((.9,-.7,1.0),7.5,1.2),((0,.8,1.3),10.5,1.)]
for pos,power,size in lights:
    d=bpy.data.lights.new('Fixed softbox','AREA');o=bpy.data.objects.new(d.name,d);scene.collection.objects.link(o)
    o.location=pos;d.energy=power;d.size=size;o.rotation_euler=(Vector((0,0,.8))-o.location).to_track_quat('-Z','Y').to_euler()
cam=bpy.data.cameras.new('Fixed review camera');co=bpy.data.objects.new(cam.name,cam);scene.collection.objects.link(co);scene.camera=co
cam.type='ORTHO';cam.dof.use_dof=False
views=[('front',0,.85,.38),('threequarter_left',65,.85,.38),('threequarter_right',-65,.85,.38),('profile',90,.85,.38),('shoulders',0,.69,.86)]
views=[(n,45 if a==65 else -45 if a==-65 else a,z,s) for n,a,z,s in views]
materials={}; originals={}; polygon_slots={}
for ob in [o for o in scene.objects if o.type=='MESH']:
    originals[ob.name]=list(ob.data.materials)
    polygon_slots[ob.name]=[p.material_index for p in ob.data.polygons]
    for mat in ob.data.materials:
        if mat.name in materials:continue
        modes={}
        for mode in ['final','basecolor','gray']:
            m=mat.copy();m.name=mat.name+'_'+mode;m.use_nodes=True
            nodes=m.node_tree.nodes;links=m.node_tree.links
            bs=next((n for n in nodes if n.type=='BSDF_PRINCIPLED'),None)
            output=next(n for n in nodes if n.type=='OUTPUT_MATERIAL')
            if mode=='gray':
                nodes.clear();bs=nodes.new('ShaderNodeBsdfPrincipled');output=nodes.new('ShaderNodeOutputMaterial')
                bs.inputs['Base Color'].default_value=(.38,.38,.38,1);bs.inputs['Roughness'].default_value=.7
                links.new(bs.outputs[0],output.inputs['Surface'])
            elif mode=='basecolor':
                em=nodes.new('ShaderNodeEmission')
                if bs.inputs['Base Color'].is_linked:links.new(bs.inputs['Base Color'].links[0].from_socket,em.inputs['Color'])
                else:em.inputs['Color'].default_value=bs.inputs['Base Color'].default_value
                em.inputs['Strength'].default_value=1;links.new(em.outputs[0],output.inputs['Surface'])
            else:
                bs.inputs['Emission Strength'].default_value=0
            modes[mode]=m
        materials[mat.name]=modes

dest=OUT/'Renders'/version;dest.mkdir(parents=True,exist_ok=True)
for mode in ['gray','basecolor','final']:
    for obname,mats in originals.items():
        ob=bpy.data.objects[obname];ob.data.materials.clear()
        for mat in mats:ob.data.materials.append(materials[mat.name][mode])
        for p,slot in zip(ob.data.polygons,polygon_slots[obname]):p.material_index=slot
    for name,angle,z,scale in views:
        a=math.radians(angle);target=Vector((0,0,z));co.location=target+Vector((2*math.sin(a),-2*math.cos(a),0))
        co.rotation_euler=(target-co.location).to_track_quat('-Z','Y').to_euler();cam.ortho_scale=scale
        scene.render.filepath=str(dest/(mode+'_'+name+'.png'));bpy.ops.render.render(write_still=True)
for obname,mats in originals.items():
    ob=bpy.data.objects[obname];ob.data.materials.clear()
    for mat in mats:ob.data.materials.append(materials[mat.name]['final'])
    for p,slot in zip(ob.data.polygons,polygon_slots[obname]):p.material_index=slot
# Save a review scene with the exact camera and lights used for its images.
co.location=(0,-2,.85);co.rotation_euler=(Vector((0,0,.85))-co.location).to_track_quat('-Z','Y').to_euler();cam.ortho_scale=.38
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/(version+'_review.blend')))
(OUT/'review_conditions.json').write_text(json.dumps({'views':views,'resolution':[800,900],'exposure':0,'gamma':1,'view_transform':'Standard','depth_of_field':False,'motion_blur':False,'final_emission':0,'lights':lights,'world_color':[.18]*3,'world_strength':.6},indent=2),encoding='utf-8')
print('LOOKDEV_REVIEW_COMPLETE',version,flush=True)
