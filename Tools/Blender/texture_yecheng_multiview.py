"""Texture the inspected high-detail candidate using the supplied front view."""
from pathlib import Path
import os,sys,time,json
ROOT=Path(__file__).resolve().parents[2]
WORK=ROOT/'Artifacts/YeChengHighDetail'
os.environ['HF_HOME']=str(WORK/'huggingface')
os.environ['HF_HUB_DISABLE_XET']='1'
sys.stdout.reconfigure(encoding='utf-8')
sys.path.insert(0,str(WORK/'Hunyuan3D-2'))
import torch,trimesh,pymeshlab
from PIL import Image
from hy3dgen.texgen.pipelines import Hunyuan3DPaintPipeline,Hunyuan3DTexGenConfig

model_root=Path((WORK/'paint_model_path.txt').read_text(encoding='utf-8'))
config=Hunyuan3DTexGenConfig(str(model_root/'hunyuan3d-delight-v2-0'),
    str(model_root/'hunyuan3d-paint-v2-0-turbo'),'hunyuan3d-paint-v2-0-turbo')
# Load networks in RAM before attaching offload hooks on this 8 GB GPU.
config.device='cpu'
pipeline=Hunyuan3DPaintPipeline(config)
pipeline.enable_model_cpu_offload()
for model in pipeline.models.values():
    model.device='cuda'
config.device='cuda'
mesh=trimesh.load(WORK/'yecheng_multiview_candidate.glb',force='mesh')
ms=pymeshlab.MeshSet()
ms.add_mesh(pymeshlab.Mesh(mesh.vertices,mesh.faces))
ms.meshing_decimation_quadric_edge_collapse(targetfacenum=120000,preservenormal=True,preserveboundary=True)
m=ms.current_mesh()
mesh=trimesh.Trimesh(m.vertex_matrix(),m.face_matrix(),process=False)
start=time.monotonic()
mesh=pipeline(mesh,Image.open(WORK/'front_input.png').convert('RGBA'))
mesh.export(WORK/'yecheng_textured_candidate.glb')
(WORK/'texture_report.json').write_text(json.dumps({'faces':len(mesh.faces),
    'elapsed_seconds':time.monotonic()-start},indent=2),encoding='utf-8')
print('YECHENG_TEXTURE_CANDIDATE_COMPLETE',flush=True)
