"""Paint only the isolated main-eye texture layers from traced reference landmarks."""
from pathlib import Path
import json
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'SourceAssets/Characters/YeChengLookdev/MainEyeStudy'
TEX=OUT/'Textures';TEX.mkdir(parents=True,exist_ok=True)
W,H=740,640
BOX=(550.,108.,587.,140.)
x=np.linspace(BOX[0],BOX[2],W)[None,:];y=np.linspace(BOX[1],BOX[3],H)[:,None]
def save(name,a): Image.fromarray(np.uint8(np.clip(a,0,255))).save(TEX/name)
def polygon_mask(points):
    im=Image.new('L',(W,H));d=ImageDraw.Draw(im)
    d.polygon([((a-BOX[0])/(BOX[2]-BOX[0])*(W-1),(b-BOX[1])/(BOX[3]-BOX[1])*(H-1)) for a,b in points],fill=255)
    return np.asarray(im.filter(ImageFilter.GaussianBlur(.65)),dtype=float)/255

def soften_closed(points):
    result=[]
    for i in range(len(points)):
        a,b,c,d=[np.array(points[k%len(points)],dtype=float) for k in [i-1,i,i+1,i+2]]
        for t in np.linspace(0,1,12,endpoint=False):
            result.append(tuple(.5*((2*b)+(-a+c)*t+(2*a-5*b+4*c-d)*t*t+(-a+3*b-3*c+d)*t*t*t)))
    return result

# Visible iris edges follow the reference trace. The upper closure lies behind the lid.
iris_outline=[(560,126.4),(559.5,129),(560,132),(562,135),(567,136),(572,135.5),(575,133.5),(576,129),(575.5,125.3),(575,118),(561,118)]
imask=polygon_mask(soften_closed(iris_outline))
sclera=np.zeros((H,W,3))+np.array([228,225,224])
upper_shadow=np.exp(-((y-125.3)/2.5)**2)*.16
sclera*=1-upper_shadow[:,:,None]
t=np.clip((y-124)/12,0,1)
iris=np.broadcast_to((1-t[:,:,None])*np.array([64,75,112])+t[:,:,None]*np.array([146,176,219]),(H,W,3)).copy()
# A broad, low-contrast lower crescent, not invented fine radial ornament.
glow=np.exp(-((x-568)/6.2)**2-((y-133.8)/2.2)**2)
iris+=glow[:,:,None]*np.array([8,13,17])
pupil=np.exp(-(((x-568)/2.05)**2+((y-128.7)/2.7)**2)**2)
iris=iris*(1-pupil[:,:,None]*.90)+np.array([34,46,78])*pupil[:,:,None]*.90
outer=np.asarray(Image.fromarray(np.uint8(imask*255)).filter(ImageFilter.MinFilter(13)),dtype=float)/255
rim=np.clip(imask-outer,0,1)*.55
iris=iris*(1-rim[:,:,None])+np.array([66,78,109])*rim[:,:,None]
highlight=polygon_mask([(565.1,125.6),(566.7,125.1),(567.2,126),(566.5,127.2),(565.2,127.1)])
iris=iris*(1-highlight[:,:,None]*.87)+np.array([225,233,243])*highlight[:,:,None]*.87
color=sclera*(1-imask[:,:,None])+iris*imask[:,:,None]
save('MainEye_IrisSclera.png',color)
# Warm top edge and darker eye-facing edge preserve the painted lash mass.
u=np.clip((y-121)/7,0,1)
lash=np.broadcast_to((1-u[:,:,None])*np.array([71,57,62])+u[:,:,None]*np.array([27,27,37]),(H,W,3)).copy()
save('MainEye_LashMass.png',lash)
brow=np.broadcast_to(np.array([73,61,67])+(np.clip((y-110)/6,0,1)*19)[:,:,None],(H,W,3)).copy()
save('MainEye_Brow.png',brow)
xx=np.broadcast_to(x,(H,W));yy=np.broadcast_to(y,(H,W))
def distance(points):
    field=np.full((H,W),1e6)
    for a,b in zip(points,points[1:]):
        dx=b[0]-a[0];dy=b[1]-a[1]
        q=np.clip(((xx-a[0])*dx+(yy-a[1])*dy)/(dx*dx+dy*dy),0,1)
        field=np.minimum(field,np.hypot(xx-a[0]-q*dx,yy-a[1]-q*dy))
    return field
fold=[(554.8,124.5),(558,121.8),(563,120.6),(569,120),(574.5,120.4),(578,122)]
lower=[(555.5,133.2),(560,136),(566,137),(572.5,137),(577.5,135.8),(581,131.8)]
alpha=np.exp(-(distance(fold)/.38)**2)*.38+np.exp(-(distance(lower)/.65)**2)*.24
shade=np.zeros((H,W,4));shade[:,:,:3]=[123,83,86];shade[:,:,3]=np.clip(alpha,0,.5)*255
save('MainEye_LidShade.png',shade)
(OUT/'texture_recipe.json').write_text(json.dumps({'bounds_in_reference_pixels':BOX,'size':[W,H],'reference':'yecheng/正面.png, screen-right eye','paint_method':'local authored gradients, traced iris boundary, lash and brow layer colors, fold/lower-lid strokes; no whole-face projection','visible_iris_outline':iris_outline,'uncertain_hidden_iris_closure':'not a design claim; occluded by upper eyelid'},indent=2),encoding='utf-8')
print('MAIN_EYE_LOCAL_TEXTURES_READY')
