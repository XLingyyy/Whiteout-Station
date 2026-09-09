"""Same close-up camera/exposure/lights for baseline, rejected C02 and the one-eye study."""
from pathlib import Path
import bpy,sys,json
from mathutils import Vector

ROOT=Path(__file__).resolve().parents[2]
LOOK=ROOT/'SourceAssets/Characters/YeChengLookdev'
OUT=LOOK/'MainEyeStudy'
args=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else ['study','basecolor']
version=args[0];modes=args[1:] or ['gray','basecolor','final']
source=OUT/'YeCheng_MainEye_Study.blend' if version=='study' else LOOK/(version+'_review.blend')
bpy.ops.wm.open_mainfile(filepath=str(source));scene=bpy.context.scene
scene.camera.location=(.043,-2,.856);scene.camera.rotation_euler=(Vector((.043,0,.856))-scene.camera.location).to_track_quat('-Z','Y').to_euler()
scene.camera.data.type='ORTHO';scene.camera.data.ortho_scale=.085
scene.render.resolution_x=1000;scene.render.resolution_y=800;scene.render.resolution_percentage=100
scene.cycles.samples=32;scene.cycles.use_denoising=True
scene.view_settings.exposure=0;scene.view_settings.gamma=1;scene.view_settings.view_transform='Standard';scene.view_settings.look='None'
scene.camera.data.dof.use_dof=False;scene.render.use_motion_blur=False
originals={o.name:list(o.data.materials) for o in scene.objects if o.type=='MESH'}
slots={o.name:[p.material_index for p in o.data.polygons] for o in scene.objects if o.type=='MESH'}
visibility={o.name:o.hide_render for o in scene.objects}
variants={}
for mats in originals.values():
    for mat in mats:
        if mat.name in variants:continue
        variants[mat.name]={}
        for mode in modes:
            m=mat.copy();m.name=mat.name+'_MainEyeReview_'+mode
            n=m.node_tree.nodes;l=m.node_tree.links;bs=next((q for q in n if q.type=='BSDF_PRINCIPLED'),None)
            output=next(q for q in n if q.type=='OUTPUT_MATERIAL')
            if mode=='gray':
                n.clear();bs=n.new('ShaderNodeBsdfPrincipled');output=n.new('ShaderNodeOutputMaterial')
                bs.inputs['Base Color'].default_value=(.38,.38,.38,1);bs.inputs['Roughness'].default_value=.7;l.new(bs.outputs[0],output.inputs['Surface'])
            elif mode=='basecolor':
                em=n.new('ShaderNodeEmission');em.inputs['Strength'].default_value=1
                if bs.inputs['Base Color'].is_linked:l.new(bs.inputs['Base Color'].links[0].from_socket,em.inputs['Color'])
                else:em.inputs['Color'].default_value=bs.inputs['Base Color'].default_value
                if bs.inputs['Alpha'].is_linked:
                    mix=n.new('ShaderNodeMixShader');transparent=n.new('ShaderNodeBsdfTransparent')
                    l.new(bs.inputs['Alpha'].links[0].from_socket,mix.inputs[0]);l.new(transparent.outputs[0],mix.inputs[1]);l.new(em.outputs[0],mix.inputs[2]);l.new(mix.outputs[0],output.inputs['Surface'])
                else:l.new(em.outputs[0],output.inputs['Surface'])
            else:bs.inputs['Emission Strength'].default_value=0
            variants[mat.name][mode]=m
dest=OUT/'Renders'/version;dest.mkdir(parents=True,exist_ok=True)
occluders=[o for o in scene.objects if o.name.startswith('Parted fringe 1 ')]
for mode in modes:
    for name,mats in originals.items():
        o=bpy.data.objects[name];o.data.materials.clear()
        for mat in mats:o.data.materials.append(variants[mat.name][mode])
        for p,i in zip(o.data.polygons,slots[name]):p.material_index=i
    for name,hidden in visibility.items():bpy.data.objects[name].hide_render=hidden
    for o in scene.objects:
        if o.get('hide_for_gray'):o.hide_render=mode=='gray'
    for occlusion in (['with_hair','clear'] if version=='study' else ['with_hair']):
        for o in occluders:o.hide_render=visibility[o.name] if occlusion=='with_hair' else True
        scene.render.filepath=str(dest/(mode+'_'+occlusion+'.png'));bpy.ops.render.render(write_still=True)
for name,hidden in visibility.items():bpy.data.objects[name].hide_render=hidden
if version=='study':
    # Save only after restoring hair, original materials and all original visibility states.
    for name,mats in originals.items():
        ob=bpy.data.objects[name];ob.data.materials.clear()
        for mat in mats:ob.data.materials.append(mat)
        for p,i in zip(ob.data.polygons,slots[name]):p.material_index=i
    scene.render.filepath='';scene.render.use_file_extension=True
    bpy.ops.wm.save_as_mainfile(filepath=str(source))
    (OUT/'render_conditions.json').write_text(json.dumps({'camera_location':list(scene.camera.location),'rotation':list(scene.camera.rotation_euler),'projection':'ORTHO','scale':.085,'resolution':[1000,800],'exposure':0,'view_transform':'Standard','motion_blur':False,'depth_of_field':False,'lights':[{'name':o.name,'position':list(o.location),'energy':o.data.energy,'size':o.data.size} for o in scene.objects if o.type=='LIGHT'],'temporarily_hidden_for_clear':[o.name for o in occluders],'hair_restored_in_saved_blend':all(o.hide_render==visibility[o.name] for o in occluders),'gray_note':'local painted fold/shadow overlay hidden; eye, lash and brow geometry remain gray; iris paint is absent'},indent=2),encoding='utf-8')
print('MAIN_EYE_RENDER_COMPLETE',version,flush=True)
