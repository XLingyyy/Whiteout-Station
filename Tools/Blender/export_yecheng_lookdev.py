"""Export frozen lookdev surfaces for an isolated UE comparison scene."""
from pathlib import Path
import bpy,math,json,sys,shutil
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'SourceAssets/Characters/YeChengLookdev'
version=sys.argv[sys.argv.index('--')+1]
bpy.ops.wm.open_mainfile(filepath=str(OUT/(version+'_review.blend')))
scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.samples=1
dest=OUT/'UEExport'/version;dest.mkdir(parents=True,exist_ok=True)
groups={}
for ob in list(scene.objects):
    if ob.type!='MESH' or ob.hide_render:continue
    bpy.ops.object.select_all(action='DESELECT');ob.hide_set(False);ob.select_set(True);bpy.context.view_layer.objects.active=ob
    for mod in list(ob.modifiers):bpy.ops.object.modifier_apply(modifier=mod.name)
    # Multi-material iris surfaces remain separate until the FBX merge.
    if len(ob.data.materials)>1:
        bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT');bpy.ops.mesh.separate(type='MATERIAL');bpy.ops.object.mode_set(mode='OBJECT')
for ob in list(scene.objects):
    if ob.type=='MESH' and not ob.hide_render:
        bpy.context.view_layer.objects.active=ob;bpy.ops.object.material_slot_remove_unused()
        groups.setdefault(ob.data.materials[0].name,[]).append(ob)
manifest={};surfaces=[]
for idx,(name,objects) in enumerate(groups.items()):
    bpy.ops.object.select_all(action='DESELECT')
    for ob in objects:ob.hide_set(False);ob.select_set(True)
    bpy.context.view_layer.objects.active=objects[0]
    if len(objects)>1:bpy.ops.object.join()
    ob=bpy.context.object;mat=ob.data.materials[0];mat.name='LD_'+version+'_'+str(idx)
    p=next(n for n in mat.node_tree.nodes if n.type=='BSDF_PRINCIPLED');texture=None
    if p.inputs['Base Color'].is_linked:
        uv=ob.data.uv_layers.new(name='ReviewGameUV');ob.data.uv_layers.active=uv;uv.active_render=True
        if 'YC_HD_Reference' in name:
            # The source clothing uses this same retained atlas.
            for a,b in zip(uv.data,ob.data.uv_layers[0].data):a.uv=b.uv
            texture='Clothing_BaseColor.png';shutil.copy2(ROOT/'SourceAssets/Characters/YeChengHighDetail/YC_HD_Reference_BaseColor.png',dest/texture)
            image=bpy.data.images.load(str(dest/texture))
        elif 'YC_HD_Face' in name:
            for poly in ob.data.polygons:
                mapped=[]
                for li in poly.loop_indices:
                    c=ob.data.vertices[ob.data.loops[li].vertex_index].co
                    mapped.append((li,.5+math.atan2(c.x,.015-c.y)/math.tau,max(0,min(1,(c.z-.740)/.253))))
                wrap=max(x[1] for x in mapped)-min(x[1] for x in mapped)>.5
                for li,u,v in mapped:uv.data[li].uv=(u+1 if wrap and u<.5 else u,v)
            texture='Baseline_Face_BaseColor.png';shutil.copy2(ROOT/'SourceAssets/Characters/YeChengHighDetail/YC_HD_Face_BaseColor.png',dest/texture)
            image=bpy.data.images.load(str(dest/texture))
        else:
            bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT');bpy.ops.uv.smart_project(angle_limit=math.radians(75),island_margin=.01);bpy.ops.object.mode_set(mode='OBJECT')
            image=bpy.data.images.new(mat.name+'_BaseColor',width=2048,height=2048,alpha=False)
            nodes=mat.node_tree.nodes;links=mat.node_tree.links;target=nodes.new('ShaderNodeTexImage');target.image=image;nodes.active=target
            output=next(n for n in nodes if n.type=='OUTPUT_MATERIAL');emit=nodes.new('ShaderNodeEmission')
            links.new(p.inputs['Base Color'].links[0].from_socket,emit.inputs['Color']);links.new(emit.outputs[0],output.inputs['Surface'])
            scene.render.bake.margin=16;bpy.ops.object.bake(type='EMIT')
            texture='Hair_BaseColor.png';image.filepath_raw=str(dest/texture);image.file_format='PNG';image.save()
        rough=p.inputs['Roughness'].default_value
        mat.node_tree.nodes.clear();p=mat.node_tree.nodes.new('ShaderNodeBsdfPrincipled');p.inputs['Roughness'].default_value=rough
        tex=mat.node_tree.nodes.new('ShaderNodeTexImage');tex.image=image;out=mat.node_tree.nodes.new('ShaderNodeOutputMaterial')
        mat.node_tree.links.new(tex.outputs['Color'],p.inputs['Base Color']);mat.node_tree.links.new(p.outputs[0],out.inputs['Surface'])
        for layer in list(ob.data.uv_layers):
            if layer.name!='ReviewGameUV':ob.data.uv_layers.remove(layer)
    manifest[mat.name]={'texture':texture,'color':list(p.inputs['Base Color'].default_value)[:3],'roughness':p.inputs['Roughness'].default_value}
    for v in ob.data.vertices:v.co=(v.co.x*.85,v.co.y*.85,(v.co.z+1)*.85)
    surfaces.append(ob)
bpy.ops.object.select_all(action='DESELECT')
for ob in surfaces:ob.select_set(True)
bpy.context.view_layer.objects.active=surfaces[0];bpy.ops.object.join();bpy.context.object.name='SM_YeChengLookdev_'+version
bpy.ops.export_scene.fbx(filepath=str(dest/'HeadReview.fbx'),use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',bake_anim=False,path_mode='AUTO')
(dest/'materials.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
print('LOOKDEV_STATIC_EXPORT_COMPLETE',version,flush=True)
