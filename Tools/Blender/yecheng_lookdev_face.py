"""One local face replacement in a copy of the existing editable character."""
from pathlib import Path
import bpy,bmesh,math,json
from mathutils import Vector

ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'SourceAssets/Characters/YeChengLookdev'
bpy.ops.wm.open_mainfile(filepath=str(ROOT/'SourceAssets/Characters/YeChengHighDetail/YeCheng_HighDetail_work.blend'))
old=bpy.data.objects['Face quad surface'];old.hide_render=True;old.hide_viewport=True
old.name='REFERENCE old face - hidden'

def mat(name,color,rough=.7):
    m=bpy.data.materials.new(name);m.use_nodes=True;p=m.node_tree.nodes.get('Principled BSDF')
    p.inputs['Base Color'].default_value=(*color,1);p.inputs['Roughness'].default_value=rough;p.inputs['Specular IOR Level'].default_value=.22
    return m

skin=mat('LD skin',(.73,.56,.51));white=mat('LD sclera',(.78,.80,.80),.45)
lash=mat('LD lashes',(.018,.014,.019));lip=mat('LD lip',(.58,.36,.34));mouthmat=mat('LD mouth interior',(.055,.018,.024))
iris=mat('LD iris bluegray',(.075,.20,.34),.43);pupil=mat('LD pupil',(.012,.025,.054),.45)
limbal=mat('LD iris rim',(.035,.065,.11),.45);loweriris=mat('LD iris lower',(.14,.32,.48),.43)

rings=[(.741,.007,.022,-.045),(.749,.023,.039,-.023),(.761,.041,.054,-.008),(.777,.058,.066,.002),(.794,.071,.076,.005),(.813,.080,.085,.008),(.834,.083,.091,.010),(.855,.084,.095,.012),(.878,.087,.098,.015),(.904,.09,.101,.017),(.930,.086,.098,.018),(.954,.073,.085,.020),(.972,.054,.065,.020),(.986,.029,.038,.020),(.992,.001,.003,.020)]
def dims(z):
    for a,b in zip(rings,rings[1:]):
        if a[0]<=z<=b[0]:
            t=(z-a[0])/(b[0]-a[0]);return tuple(a[k]*(1-t)+b[k]*t for k in [1,2,3])
    return rings[-1][1:]
def front(x,z):
    rx,ry,cy=dims(z);f=math.sqrt(max(.0,1-(x/rx)**2))
    nose=.015*math.exp(-(x/.014)**2-((z-.815)/.010)**2)
    bridge=.009*math.exp(-(x/.013)**2-((z-.836)/.025)**2)
    muzzle=.015*math.exp(-(x/.029)**2-((z-.785)/.022)**2)
    cheeks=.004*sum(math.exp(-((x-s*.047)/.026)**2-((z-.813)/.027)**2) for s in [-1,1])
    return cy-ry*f-f**3*(nose+bridge+muzzle+cheeks)
def point(i,z):
    a=(i-64)*math.tau/128;rx,ry,cy=dims(z);x=rx*math.sin(a)
    return (x,front(x,z) if math.cos(a)>=0 else cy-ry*math.cos(a),z)
def mesh(name,vs,fs,material):
    me=bpy.data.meshes.new(name);me.from_pydata(vs,[],fs);me.update()
    ob=bpy.data.objects.new(name,me);bpy.context.collection.objects.link(ob);me.materials.append(material)
    bm=bmesh.new();bm.from_mesh(me);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(me);bm.free()
    for f in me.polygons:f.use_smooth=True
    return ob

# Rectangular openings in a latitude grid are bridged into concentric eye/lip loops.
zs=sorted(set([round(.741+i*(.992-.741)/84,6) for i in range(85)]+[.820,.866,.771,.804]))
eye_a,eye_b=zs.index(.820),zs.index(.866);mouth_a,mouth_b=zs.index(.771),zs.index(.804)
holes=[(68,84,eye_a,eye_b,'L'),(44,60,eye_a,eye_b,'R'),(55,73,mouth_a,mouth_b,'mouth')]
vs=[point(i,z) for z in zs for i in range(128)];fs=[]
for j in range(len(zs)-1):
    for i in range(128):
        if any(i0<=i<i1 and j0<=j<j1 for i0,i1,j0,j1,_ in holes):continue
        fs.append((j*128+i,j*128+(i+1)%128,(j+1)*128+(i+1)%128,(j+1)*128+i))
fs.extend([tuple(reversed(range(128))),tuple((len(zs)-1)*128+i for i in range(128))])
loop_info={}
for i0,i1,j0,j1,feature in holes:
    boundary=[j0*128+i for i in range(i0,i1)]+[j*128+i1 for j in range(j0,j1)]+[j1*128+i for i in range(i1,i0,-1)]+[j*128+i0 for j in range(j1,j0,-1)]
    inner=[];angles=[]
    for index in boundary:
        j,i=divmod(index,128);u=2*(i-i0)/(i1-i0)-1;v=2*(j-j0)/(j1-j0)-1;a=math.atan2(v,u);angles.append(a)
        if feature=='mouth':
            x=.020*math.cos(a);z=.786+.0010*math.sin(a);y=front(x,z)-.0007
        else:
            side=1 if feature=='L' else -1;x=side*.043+.024*math.cos(a)
            z=.842+(.0095 if math.sin(a)>0 else .0065)*math.sin(a)+side*.04*(x-side*.043)
            y=front(x,z)-.002
        inner.append(Vector((x,y,z)))
    prev=boundary
    for t in [.33,.67,1.]:
        current=[]
        for k,index in enumerate(boundary):
            co=Vector(vs[index]).lerp(inner[k],t);co.y=front(co.x,co.z)-.002*t
            if feature=='mouth':co.y-=.0025*math.sin(math.pi*t)
            current.append(len(vs));vs.append(tuple(co))
        for k in range(len(current)):
            q=(k+1)%len(current);fs.append((prev[k],prev[q],current[q],current[k]))
        prev=current
    loop_info[feature]={'indices':prev,'angles':angles,'points':[tuple(p) for p in inner]}
face=mesh('LD face with eyelid and mouth loops',vs,fs,skin)
# Remove unused latitude vertices from the opening interiors, preserving loop groups.
for name,info in loop_info.items():
    g=face.vertex_groups.new(name=name+' rim');g.add(info['indices'],1,'REPLACE')
bm=bmesh.new();bm.from_mesh(face.data);bmesh.ops.delete(bm,geom=[v for v in bm.verts if not v.link_faces],context='VERTS');bm.to_mesh(face.data);bm.free()
sub=face.modifiers.new('Editable facial surface','SUBSURF');sub.levels=1;sub.render_levels=1

def ribbon(name,points,width,material):
    vs=[];fs=[]
    for k,p in enumerate(points):
        tangent=Vector(points[min(k+1,len(points)-1)])-Vector(points[max(0,k-1)])
        normal=Vector((0,-1,0));across=tangent.cross(normal).normalized()
        w=width*(.20+.80*math.sin(math.pi*k/(len(points)-1))**.5)
        vs.extend([tuple(Vector(p)-across*w),tuple(Vector(p)+across*w)])
    for k in range(len(points)-1):fs.append((2*k,2*k+1,2*k+3,2*k+2))
    return mesh(name,vs,fs,material)

for side,name in [(1,'L'),(-1,'R')]:
    cx=side*.043
    def eye(a,r=1):
        x=cx+r*.024*math.cos(a);z=.842+r*(.0095 if math.sin(a)>0 else .0065)*math.sin(a)+side*.04*(x-cx)
        return (x,front(x,z)-.002-.006*(1-r*r),z)
    ev=[eye(0,0)];ef=[];n=64
    for r in [.25,.5,.75,1.]:
        ev.extend(eye(k*math.tau/n,r) for k in range(n))
    for k in range(n):ef.append((0,1+k,1+(k+1)%n))
    for j in range(3):
        for k in range(n):a=1+j*n+k;b=1+j*n+(k+1)%n;ef.append((a,b,b+n,a+n))
    mesh('LD '+name+' curved eye panel',ev,ef,white)
    # Iris sits on the same curved eye panel, independently editable from the face.
    iv=[(cx,front(cx,.843)-.0085,.843)];ifs=[];indices=[]
    for r in [.0037,.0073,.0082]:
        for k in range(n):
            a=k*math.tau/n;x=cx+r*math.cos(a);z=.843+r*math.sin(a)
            q=((x-cx)/.024)**2+((z-.842)/(.0095 if z>.842 else .0065))**2
            iv.append((x,front(x,z)-.002-.006*(1-min(1,q))-.0005,z))
    for k in range(n):ifs.append((0,1+k,1+(k+1)%n));indices.append(1)
    for j in range(2):
        for k in range(n):a=1+j*n+k;b=1+j*n+(k+1)%n;ifs.append((a,b,b+n,a+n));indices.append(2 if j==1 else 3 if math.sin(k*math.tau/n)<-.2 else 0)
    ob=mesh('LD '+name+' iris',iv,ifs,iris)
    for m in [pupil,limbal,loweriris]:ob.data.materials.append(m)
    for f,mi in zip(ob.data.polygons,indices):f.material_index=mi
    pts=[]
    for k in range(33):
        a=math.pi*k/32;x,y,z=eye(a);pts.append((x,y-.0008,z))
    ribbon('LD '+name+' upper eyelash',pts,.0012,lash)
    pts=[]
    for k in range(33):
        a=math.pi+math.pi*k/32;x,y,z=eye(a);pts.append((x,y-.0005,z))
    ribbon('LD '+name+' lower eyelid edge',pts,.00035,lip)
    pts=[]
    for k in range(25):
        t=k/24;x=side*(.017+.050*t);z=.869+.004*math.sin(math.pi*t)-.003*t
        pts.append((x,front(x,z)-.001,z))
    ribbon('LD '+name+' eyebrow',pts,.0011,lash)

info=loop_info['mouth'];c=(0,front(0,.786)+.003,.786)
mesh('LD mouth cavity',[c]+[(x,y+.003,z) for x,y,z in info['points']],[(0,k+1,(k+1)%len(info['points'])+1) for k in range(len(info['points']))],mouthmat)
for upper in [True,False]:
    pts=[]
    for k in range(41):
        x=-.020+.04*k/40;t=x/.020;z=.786+(1 if upper else -1)*.001*math.sqrt(max(0,1-t*t))
        pts.append((x,front(x,z)-.0022,z))
    ribbon('LD '+('upper' if upper else 'lower')+' lip border',pts,.00045,lip)

# Candidate skin follows the source design; it has no whole-face image projection.
for ob in bpy.context.scene.objects:
    if ob.type=='MESH' and not ob.hide_render:
        for m in ob.data.materials:
            if m.use_nodes:
                for n in m.node_tree.nodes:
                    if n.type=='BSDF_PRINCIPLED':n.inputs['Emission Strength'].default_value=0
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'candidate_01.blend'))
(OUT/'face_structure.json').write_text(json.dumps({'source':'existing HighDetail work.blend copy','changed':['face surface only','eyelid annular loops','curved eye panels and separate irises','mouth opening and lip loops','nose-lip profile'],'unchanged':['hair geometry','body','neck','collar'],'whole_face_image_on_candidate':False,'animation_added':False},indent=2),encoding='utf-8')
print('LOOKDEV_LOCAL_FACE_READY',flush=True)
