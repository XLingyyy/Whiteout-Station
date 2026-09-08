"""Generate a sculpting candidate from the supplied reference views.

Run in the isolated Artifacts/YeChengHighDetail/venv environment. The generated
surface is an intermediate to inspect and refine, not a game-ready character.
"""
from pathlib import Path
import os
import sys
import json
import time
import argparse

parser=argparse.ArgumentParser()
parser.add_argument('--head',action='store_true')
parser.add_argument('--front-only',action='store_true')
args=parser.parse_args()

ROOT=Path(__file__).resolve().parents[2]
WORK=ROOT/'Artifacts/YeChengHighDetail'
os.environ['HF_HOME']=str(WORK/'huggingface')
os.environ['U2NET_HOME']=str(WORK/'rembg')
os.environ['HF_HUB_DISABLE_XET']='1'
sys.stdout.reconfigure(encoding='utf-8')
sys.path.insert(0,str(WORK/'Hunyuan3D-2'))

import torch
from PIL import Image
from huggingface_hub import hf_hub_download
from hy3dgen.rembg import BackgroundRemover
from hy3dgen.shapegen import Hunyuan3DDiTFlowMatchingPipeline

model='tencent/Hunyuan3D-2mv'
subfolder='hunyuan3d-dit-v2-mv'
paths={}
for filename in ['config.yaml','model.fp16.safetensors']:
    print('Fetching',filename,flush=True)
    paths[filename]=hf_hub_download(model,f'{subfolder}/{filename}')

images={}
remover=BackgroundRemover()
for view,filename in [('front','正面.png'),('left','左面.png'),('back','背后.png')]:
    cached=WORK/(view+'_input.png')
    if cached.exists():
        images[view]=Image.open(cached).convert('RGBA')
    else:
        source=Image.open(ROOT/'yecheng'/filename).convert('RGB')
        images[view]=remover(source)
        images[view].save(cached)
    print('Input ready',view,flush=True)

if args.head:
    # A dedicated bust pass gives the face substantially more spatial samples.
    images={view:im.crop((int(im.width*.29),0,int(im.width*.73),int(im.height*.27)))
            for view,im in images.items() if view in ('front','back')}
    for view,im in images.items():im.save(WORK/(view+'_head_input.png'))
if args.front_only:
    images={'front':images['front']}

start=time.monotonic()
pipeline=Hunyuan3DDiTFlowMatchingPipeline.from_single_file(
    paths['model.fp16.safetensors'],paths['config.yaml'],
    device='cpu',dtype=torch.float16,use_safetensors=True,
)
# The standalone shape pipeline lacks DiffusionPipeline.components, which its
# offload implementation expects.
pipeline.components={name:getattr(pipeline,name) for name in ('conditioner','model','vae')}
pipeline.enable_model_cpu_offload()
# The pipeline's custom sampler reads .device directly; keep its latent device
# on CUDA while Accelerate moves the individual networks as needed.
pipeline.device=torch.device('cuda')
resolution=384 if args.head else 512
mesh=pipeline(image=images,num_inference_steps=50,octree_resolution=resolution,
              num_chunks=6000,generator=torch.Generator(device='cuda').manual_seed(8426),
              mc_algo='mc',output_type='trimesh')[0]
prefix='yecheng_head' if args.head else 'yecheng_multiview'
if args.front_only:prefix+='_front'
mesh.export(WORK/(prefix+'_candidate.glb'))
report={'generator':model,'seed':8426,'views':list(images),'steps':50,
        'octree_resolution':resolution,'vertices':len(mesh.vertices),'faces':len(mesh.faces),
        'elapsed_seconds':time.monotonic()-start,'bounds':mesh.bounds.tolist()}
(WORK/(prefix+'_report.json')).write_text(json.dumps(report,indent=2),encoding='utf-8')
print('YECHENG_MULTIVIEW_CANDIDATE_COMPLETE',report,flush=True)
