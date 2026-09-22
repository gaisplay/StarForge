# Blender (background) blockout of the Krishna from geometry.py, rendered as a grey sketch.
# Run: blender -b -P blockout.py -- <out_dir>
import bpy, sys, os
from mathutils import Vector

here = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, here)
import geometry as G

out_dir = sys.argv[sys.argv.index('--')+1] if '--' in sys.argv else here

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.unit_settings.system = 'METRIC'

GREY = (0.80, 0.82, 0.85, 1.0)
DARK = (0.58, 0.61, 0.65, 1.0)
BLUE = (0.30, 0.38, 0.50, 1.0)
CYAN = (0.35, 0.85, 0.90, 1.0)
RED  = (0.90, 0.15, 0.15, 1.0)

def to_world(x, y, z):
    """Ship coords (x aft from nose, y starboard, z up) -> Blender (X forward, Y left, Z up), nose at +X."""
    return Vector((G.L/2 - x, -y, z))

def mesh_object(name, verts, faces, color=GREY, smooth=False):
    me = bpy.data.meshes.new(name)
    me.from_pydata([to_world(*v) for v in verts], [], faces)
    me.update()
    ob = bpy.data.objects.new(name, me)
    scene.collection.objects.link(ob)
    ob.color = color
    if smooth:
        for p in me.polygons: p.use_smooth = True
    return ob

def bevel(ob, width, segments=3):
    m = ob.modifiers.new('Bevel', 'BEVEL'); m.width = width; m.segments = segments; m.limit_method = 'ANGLE'
    return ob

def box(name, x0, x1, y0, y1, z0, z1, color=GREY):
    v = [(x0,y0,z0),(x1,y0,z0),(x1,y1,z0),(x0,y1,z0),(x0,y0,z1),(x1,y0,z1),(x1,y1,z1),(x0,y1,z1)]
    f = [(0,1,2,3),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)]
    return mesh_object(name, v, f, color)

def prism(name, plan, z0, z1, color=GREY):
    n = len(plan)
    v = [(x,y,z0) for x,y in plan] + [(x,y,z1) for x,y in plan]
    f = [tuple(range(n)), tuple(range(n,2*n))] + [(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
    return mesh_object(name, v, f, color)

def slab(name, mid_pts, offsets, color=GREY):
    """A plate: mid-surface polygon (3D points) thickened by a per-vertex offset vector on both sides."""
    n = len(mid_pts)
    v = [tuple(p[i] - o[i] for i in range(3)) for p, o in zip(mid_pts, offsets)] + \
        [tuple(p[i] + o[i] for i in range(3)) for p, o in zip(mid_pts, offsets)]
    f = [tuple(range(n)), tuple(range(n,2*n))] + [(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
    return mesh_object(name, v, f, color)

# ---- hull: lofted flattened hexagons along x, chisel nose ----
stations = [0, 1.5, 3, 5, 8, 11, 14, 17, 20, 23, 26, 29, 32, 35, 38, 41, 43, G.HULL_END]
verts, faces, rings = [], [], []
f = G.CHINE_WIDTH_FRACTION
for x in stations:
    w = G.interp(G.hull_plan, x); t = G.interp(G.top_line, x); b = G.interp(G.bot_line, x); zc = G.interp(G.chine, x)
    rings.append(list(range(len(verts), len(verts)+6)))
    verts += [(x, -w, zc), (x, -f*w, t), (x, f*w, t), (x, w, zc), (x, f*w, b), (x, -f*w, b)]
faces.append(tuple(reversed(rings[0])))
for r0, r1 in zip(rings, rings[1:]):
    for i in range(6):
        faces.append((r0[i], r0[(i+1)%6], r1[(i+1)%6], r1[i]))
faces.append(tuple(rings[-1]))
mesh_object('Hull', verts, faces)

# ---- spine and cockpit block on the deck ----
sp = G.SPINE
spine_v, spine_f = [], []
for i, x in enumerate([sp['x0'], sp['x0']+3, (sp['x0']+sp['x1'])/2, sp['x1']]):
    t = G.interp(G.top_line, x) - 0.2
    yh = sp['yh'] * (0.4 if i == 0 else 1.0)
    spine_v += [(x, -yh, t), (x, yh, t), (x, yh*0.45, t + sp['h']), (x, -yh*0.45, t + sp['h'])]
for k in range(3):
    b = 4*k
    for i in range(4):
        spine_f.append((b+i, b+(i+1)%4, b+4+(i+1)%4, b+4+i))
spine_f += [(0,1,2,3), (12,15,14,13)]
mesh_object('Spine', spine_v, spine_f, DARK)
ck = G.COCKPIT
bevel(box('CockpitBlock', ck['x0'], ck['x1'], -ck['yh'], ck['yh'], G.DECK-0.3, G.DECK+ck['h']), 0.3)
box('WindowStrip', ck['x0']-0.05, ck['x0']+0.4, -ck['yh']*0.8, ck['yh']*0.8, G.DECK+0.3, G.DECK+0.7, CYAN)

# ---- dome: skirt + ellipsoid cap + crown disc ----
bpy.ops.mesh.primitive_cylinder_add(vertices=64, radius=G.DOME_R, depth=G.SKIRT, location=to_world(G.DOME_X, 0, G.DECK + G.SKIRT/2))
bpy.context.object.name = 'DomeSkirt'; bpy.context.object.color = DARK
bpy.ops.mesh.primitive_uv_sphere_add(segments=64, ring_count=32, radius=1.0, location=to_world(G.DOME_X, 0, G.DECK + G.SKIRT))
dome = bpy.context.object; dome.name = 'DomeCap'; dome.scale = (G.DOME_R, G.DOME_R, G.CAP); dome.color = GREY
bpy.ops.object.shade_smooth()
bpy.ops.mesh.primitive_cylinder_add(vertices=48, radius=G.DOME_TOP['r'], depth=G.DOME_TOP['h']*2, location=to_world(G.DOME_X, 0, G.DECK+G.SKIRT+G.CAP-0.35))
bpy.context.object.name = 'DomeCrown'; bpy.context.object.color = GREY

# ---- turret pods and lower fairings ----
for sgn in (1,-1):
    tt = G.TUR_TOP
    deck_here = G.interp(G.top_line, (tt['x0']+tt['x1'])/2) - 0.3
    bevel(box('TurretPod%+d'%sgn, tt['x0'], tt['x1'], sgn*tt['y0'], sgn*tt['y1'], deck_here-0.4, deck_here+tt['h']), 0.9, 5)
    fa = G.FAIRING
    belly = G.interp(G.bot_line, (fa['x0']+fa['x1'])/2) + 0.5
    bevel(box('Fairing%+d'%sgn, fa['x0'], fa['x1'], sgn*fa['y0'], sgn*fa['y1'], belly-fa['h'], belly), 0.7, 4)

# ---- upper nacelles with a round blue front ----
for sgn in (1,-1):
    nu = G.NAC_UP
    ob = prism('NacelleUp%+d'%sgn, [(x, sgn*y) for x,y in G.splayed_plan(nu)], nu['z0'], nu['z1'], GREY)
    bevel(ob, 0.5)
    box('NacelleFront%+d'%sgn, nu['x0']-0.3, nu['x0']+0.6, sgn*(nu['yi0']+0.5), sgn*(nu['yo0']-0.5), nu['z0']+0.6, nu['z1']-0.6, BLUE)

# ---- lower wings: tapered plates with a swept tip ----
for sgn in (1,-1):
    mid = [(x, sgn*y, G.WING['zc']) for x,y in G.WING['plan']]
    offs = [(0.0, 0.0, G.wing_half_thickness(y)) for x,y in G.WING['plan']]
    bevel(slab('WingLo%+d'%sgn, mid, offs, DARK), 0.15, 2)

# ---- fins: flat blades on the centreline ----
for nm, fin in (('FinFwd', G.FIN_FWD), ('FinAft', G.FIN_AFT)):
    mid = [(x, 0.0, z) for x,z in fin['poly']]
    offs = [(0.0, G.fin_half_thickness(fin, z), 0.0) for x,z in fin['poly']]
    bevel(slab(nm, mid, offs, DARK), 0.08, 2)

# ---- spikes ----
def spike(name, p0, p1, r0=0.25, r1=0.04):
    a, b = to_world(*p0), to_world(*p1)
    d = b - a
    bpy.ops.mesh.primitive_cone_add(vertices=12, radius1=r0, radius2=r1, depth=d.length, location=(a+b)/2)
    ob = bpy.context.object; ob.name = name; ob.color = DARK
    ob.rotation_euler = d.to_track_quat('Z','Y').to_euler()
for sgn, tag in ((1,'S'), (-1,'P')):
    for nm, s in (('Trail', G.TRAIL), ('NoseSpike', G.NOSE_SPIKE)):
        spike(nm+tag, (s[0][0], sgn*s[0][1], s[0][2]), (s[1][0], sgn*s[1][1], s[1][2]))

# ---- the rig's cabin, as a red wire cube ----
c = G.CAB
cab = box('Cabin8m', c['cx']-c['half'], c['cx']+c['half'], -c['half'], c['half'], c['z0'], c['z1'], RED)
cab.modifiers.new('Wire','WIREFRAME').thickness = 0.18

# ---- cameras ----
def add_camera(name, loc, target, lens=35):
    cam = bpy.data.cameras.new(name); ob = bpy.data.objects.new(name, cam)
    scene.collection.objects.link(ob)
    ob.location = loc
    ob.rotation_euler = (Vector(target) - Vector(loc)).to_track_quat('-Z','Y').to_euler()
    cam.lens = lens; cam.clip_end = 1000
    return ob

centre = to_world(25, 0, 1.0)
cam_hero = add_camera('CamHero', centre + Vector((38, 74, 22)), centre, lens=40)     # the official render's angle
cam_rear = add_camera('CamRear', centre + Vector((-62, -50, 30)), centre, lens=35)

# ---- render: Workbench sketch look ----
scene.render.engine = 'BLENDER_WORKBENCH'
sh = scene.display.shading
sh.light = 'STUDIO'; sh.color_type = 'OBJECT'
sh.show_cavity = True; sh.cavity_type = 'BOTH'; sh.cavity_ridge_factor = 1.5; sh.cavity_valley_factor = 1.5
sh.show_object_outline = True; sh.show_shadows = False
scene.display.render_aa = '8'
scene.render.resolution_x = 1600; scene.render.resolution_y = 1000
world = bpy.data.worlds.new('World'); scene.world = world; world.color = (0.93, 0.93, 0.92)
scene.view_settings.view_transform = 'Standard'

for cam, nm in ((cam_hero, 'krishna_blockout_hero.png'), (cam_rear, 'krishna_blockout_rear.png')):
    scene.camera = cam
    scene.render.filepath = os.path.join(out_dir, nm)
    bpy.ops.render.render(write_still=True)
    print('rendered', nm)

sh.show_xray = True; sh.xray_alpha = 0.35; sh.show_cavity = False
scene.camera = cam_hero
scene.render.filepath = os.path.join(out_dir, 'krishna_blockout_xray.png')
bpy.ops.render.render(write_still=True)
print('rendered xray')
sh.show_xray = False; sh.show_cavity = True

bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out_dir, 'krishna_blockout.blend'))
print('saved blend')
