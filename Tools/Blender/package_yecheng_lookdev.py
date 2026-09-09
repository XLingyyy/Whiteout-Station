"""Package the selected second candidate without changing its visible geometry."""
from pathlib import Path
import bpy,math,json
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'SourceAssets/Characters/YeChengLookdev'
bpy.ops.wm.open_mainfile(filepath=str(OUT/'candidate_02_review.blend'))
face=bpy.data.objects['LD face with eyelid and mouth loops']
uv=face.data.uv_layers.new(name='Face continuous UV')
for p in face.data.polygons:
    coords=[]
    for index in p.loop_indices:
        v=face.data.vertices[face.data.loops[index].vertex_index].co
        coords.append((index,.5+math.atan2(v.x,.015-v.y)/math.tau,(v.z-.741)/.251))
    seam=max(v[1] for v in coords)-min(v[1] for v in coords)>.5
    for index,u,v in coords:uv.data[index].uv=(u+1 if seam and u<.5 else u,v)
for ob in bpy.context.scene.objects:
    if ob.name.startswith('LD ') and ('eye panel' in ob.name or ' iris' in ob.name):
        uv=ob.data.uv_layers.new(name='Eye local UV');xs=[v.co.x for v in ob.data.vertices];zs=[v.co.z for v in ob.data.vertices]
        for index,loop in enumerate(ob.data.loops):
            v=ob.data.vertices[loop.vertex_index].co;uv.data[index].uv=((v.x-min(xs))/(max(xs)-min(xs)),(v.z-min(zs))/(max(zs)-min(zs)))
for image in bpy.data.images:
    if image.source=='FILE' and image.has_data and not image.packed_file:image.pack()
bpy.ops.object.select_all(action='DESELECT');face.select_set(True);bpy.context.view_layer.objects.active=face
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'YeCheng_HeadLookdev.blend'))
(OUT/'selection.json').write_text(json.dumps({'selected':'candidate_02','rejected':'candidate_03','reason':'candidate_03 iris-panel intersections produce radial gaps','local_iterations':3,'image_edits':'none; comparison sheets only place original renders side by side','production_replacement':False,'added_UVs_only_after_selection':True},indent=2),encoding='utf-8')
print('LOOKDEV_PACKAGE_COMPLETE',flush=True)
