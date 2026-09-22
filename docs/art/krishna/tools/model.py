# The Krishna as one game-ready static mesh, built in Blender (background) from geometry.py.
# Run: blender -b -P model.py -- <fbx_out_path> <render_dir>
#
# Output: SM_Krishna -- a single mesh, four material slots, pivot at the centre of the rig's 8 m cabin,
# +X forward, +Z up, starboard at UE +Y once the FBX is imported (Blender -Y here; the FBX axis
# convention flips it). Units: metres in Blender, centimetres after import, as apply_unit_scale writes.
import bpy, bmesh, sys, os, math
from mathutils import Vector, Matrix

here = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, here)
import geometry as G

args = sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
FBX_PATH = args[0] if len(args) > 0 else os.path.join(here, 'SM_Krishna.fbx')
RENDER_DIR = args[1] if len(args) > 1 else here
os.makedirs(os.path.dirname(FBX_PATH), exist_ok=True); os.makedirs(RENDER_DIR, exist_ok=True)

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.unit_settings.system = 'METRIC'; scene.unit_settings.scale_length = 1.0

# ------------------------------------------------------------------ materials
def material(name, rgb, emission=None, metallic=0.4, roughness=0.55):
    m = bpy.data.materials.new(name); m.use_nodes = True
    bsdf = m.node_tree.nodes.get('Principled BSDF')
    bsdf.inputs['Base Color'].default_value = (*rgb, 1.0)
    bsdf.inputs['Metallic'].default_value = metallic
    bsdf.inputs['Roughness'].default_value = roughness
    if emission:
        bsdf.inputs['Emission Color'].default_value = (*emission, 1.0)
        bsdf.inputs['Emission Strength'].default_value = 4.0
    m.diffuse_color = (*rgb, 1.0)
    return m

MATS = {
    'hull_light': material('M_Krishna_HullLight', (0.82, 0.84, 0.87)),
    'hull_dark':  material('M_Krishna_HullDark',  (0.42, 0.45, 0.50)),
    'glass':      material('M_Krishna_Glass',     (0.22, 0.30, 0.42), metallic=0.1, roughness=0.15),
    'emissive':   material('M_Krishna_Emissive',  (0.30, 0.85, 0.90), emission=(0.30, 0.85, 0.90), metallic=0.0),
}
SLOT = {k: i for i, k in enumerate(MATS)}

# ------------------------------------------------------------------ helpers
def to_world(x, y, z):
    """Ship coords (x aft from nose, y starboard, z up) -> Blender (X forward, Y left, Z up)."""
    return Vector((G.L/2 - x, -y, z))

PARTS = []   # (object, material key)

def mesh_object(name, verts, faces, mat='hull_light'):
    me = bpy.data.meshes.new(name)
    me.from_pydata([to_world(*v) for v in verts], [], faces)
    me.update()
    ob = bpy.data.objects.new(name, me)
    scene.collection.objects.link(ob)
    ob.data.materials.append(MATS[mat])
    PARTS.append((ob, mat))
    return ob

def primitive(op, name, mat='hull_light', **kw):
    op(**kw)
    ob = bpy.context.object; ob.name = name
    ob.data.materials.append(MATS[mat])
    PARTS.append((ob, mat))
    return ob

def bevel(ob, width, segments=3, angle_deg=30):
    m = ob.modifiers.new('Bevel', 'BEVEL'); m.width = width; m.segments = segments
    m.limit_method = 'ANGLE'; m.angle_limit = math.radians(angle_deg); m.harden_normals = False
    return ob

def cut(ob, cutter):
    """Boolean difference; the cutter is consumed (never exported)."""
    m = ob.modifiers.new('Cut', 'BOOLEAN'); m.operation = 'DIFFERENCE'; m.object = cutter; m.solver = 'EXACT'
    cutter.hide_render = True; cutter.hide_viewport = True
    CUTTERS.append(cutter)
    return ob
CUTTERS = []

def box(name, x0, x1, y0, y1, z0, z1, mat='hull_light'):
    v = [(x0,y0,z0),(x1,y0,z0),(x1,y1,z0),(x0,y1,z0),(x0,y0,z1),(x1,y0,z1),(x1,y1,z1),(x0,y1,z1)]
    f = [(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)]
    return mesh_object(name, v, f, mat)

def cutter_box(name, x0, x1, y0, y1, z0, z1):
    me = bpy.data.meshes.new(name)
    v = [(x0,y0,z0),(x1,y0,z0),(x1,y1,z0),(x0,y1,z0),(x0,y0,z1),(x1,y0,z1),(x1,y1,z1),(x0,y1,z1)]
    f = [(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)]
    me.from_pydata([to_world(*p) for p in v], [], f); me.update()
    ob = bpy.data.objects.new(name, me); scene.collection.objects.link(ob)
    return ob

def prism(name, plan, z0, z1, mat='hull_light'):
    n = len(plan)
    v = [(x,y,z0) for x,y in plan] + [(x,y,z1) for x,y in plan]
    f = [tuple(reversed(range(n))), tuple(range(n,2*n))] + [(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
    return mesh_object(name, v, f, mat)

def slab(name, mid_pts, offsets, mat='hull_light'):
    n = len(mid_pts)
    v = [tuple(p[i] - o[i] for i in range(3)) for p, o in zip(mid_pts, offsets)] + \
        [tuple(p[i] + o[i] for i in range(3)) for p, o in zip(mid_pts, offsets)]
    f = [tuple(reversed(range(n))), tuple(range(n,2*n))] + [(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
    return mesh_object(name, v, f, mat)

def spike(name, p0, p1, r0=0.25, r1=0.04, mat='hull_dark'):
    a, b = to_world(*p0), to_world(*p1); d = b - a
    ob = primitive(bpy.ops.mesh.primitive_cone_add, name, mat, vertices=16, radius1=r0, radius2=r1, depth=d.length, location=(a+b)/2)
    ob.rotation_euler = d.to_track_quat('Z','Y').to_euler()
    return ob

def cylinder_x(name, x0, x1, y, z, r, mat='hull_light', vertices=48):
    a, b = to_world(x0, y, z), to_world(x1, y, z)
    ob = primitive(bpy.ops.mesh.primitive_cylinder_add, name, mat, vertices=vertices, radius=r, depth=abs(x1-x0), location=(a+b)/2)
    ob.rotation_euler = (0, math.radians(90), 0)
    return ob

# ------------------------------------------------------------------ hull: faceted loft, chisel nose
stations = [0, 1.5, 3, 5, 8, 11, 14, 17, 20, 23, 26, 29, 32, 35, 38, 41, 43, G.HULL_END]
verts, faces, rings = [], [], []
fr = G.CHINE_WIDTH_FRACTION
for x in stations:
    w = G.interp(G.hull_plan, x); t = G.interp(G.top_line, x); b = G.interp(G.bot_line, x); zc = G.interp(G.chine, x)
    rings.append(list(range(len(verts), len(verts)+6)))
    verts += [(x, -w, zc), (x, -fr*w, t), (x, fr*w, t), (x, w, zc), (x, fr*w, b), (x, -fr*w, b)]
faces.append(tuple(rings[0]))
for r0, r1 in zip(rings, rings[1:]):
    for i in range(6):
        faces.append((r0[i], r1[i], r1[(i+1)%6], r0[(i+1)%6]))
faces.append(tuple(reversed(rings[-1])))
hull = mesh_object('Hull', verts, faces)
bevel(hull, 0.18, 2)

# the vestibule: a through-socket from the starboard skin to the cabin's +Y wall (see geometry.VESTIBULE)
c = G.CAB
VEST = G.VESTIBULE
cut(hull, cutter_box('VestibuleCutter', VEST['x0'], VEST['x1'], VEST['y0'], VEST['y1'], VEST['z0'], VEST['z1']))

# ------------------------------------------------------------------ spine, cockpit block, window strip
sp = G.SPINE
sv, sf = [], []
for i, x in enumerate([sp['x0'], sp['x0']+3, (sp['x0']+sp['x1'])/2, sp['x1']]):
    t = G.interp(G.top_line, x) - 0.2
    yh = sp['yh'] * (0.4 if i == 0 else 1.0)
    sv += [(x, -yh, t), (x, yh, t), (x, yh*0.45, t + sp['h']), (x, -yh*0.45, t + sp['h'])]
for k in range(3):
    b0 = 4*k
    for i in range(4):
        sf.append((b0+i, b0+4+i, b0+4+(i+1)%4, b0+(i+1)%4))
sf += [(0,1,2,3), (12,15,14,13)]
bevel(mesh_object('Spine', sv, sf, 'hull_dark'), 0.08, 2)
ck = G.COCKPIT
bevel(box('CockpitBlock', ck['x0'], ck['x1'], -ck['yh'], ck['yh'], G.DECK-0.3, G.DECK+ck['h']), 0.25, 3)
box('WindowStrip', ck['x0']-0.06, ck['x0']+0.35, -ck['yh']*0.8, ck['yh']*0.8, G.DECK+0.3, G.DECK+0.7, 'emissive')

# ------------------------------------------------------------------ dome: glass skirt, cap, crown, visor arc
skirt = primitive(bpy.ops.mesh.primitive_cylinder_add, 'DomeSkirt', 'glass', vertices=96, radius=G.DOME_R, depth=G.SKIRT,
                  location=to_world(G.DOME_X, 0, G.DECK + G.SKIRT/2))
# the skirt's top disc would sit on the same rim the cap's equator merges into: the disc goes, and the
# open cap takes its place.
bm = bmesh.new(); bm.from_mesh(skirt.data)
top = [f for f in bm.faces if f.calc_center_median().z > G.SKIRT/2 - 0.01]   # local coords: the top disc only
print(f'STATS skirt_top_faces_removed={len(top)}')
bmesh.ops.delete(bm, geom=top, context='FACES_ONLY')
bm.to_mesh(skirt.data); bm.free(); skirt.data.update()
dome = primitive(bpy.ops.mesh.primitive_uv_sphere_add, 'DomeCap', 'hull_light', segments=96, ring_count=48, radius=1.0,
                 location=to_world(G.DOME_X, 0, G.DECK + G.SKIRT))
dome.scale = (G.DOME_R, G.DOME_R, G.CAP)
# drop the lower hemisphere: it would sit inside the hull and double the dome's triangles for nothing.
# The equator stays open on purpose: its 96 vertices coincide with the skirt's top rim, remove_doubles
# merges them after the parts join, and cap + skirt become one closed shell. A fill here would make three
# faces meet on every equator edge.
bm = bmesh.new(); bm.from_mesh(dome.data)
bmesh.ops.delete(bm, geom=[v for v in bm.verts if v.co.z < -0.02], context='VERTS')
bm.to_mesh(dome.data); bm.free(); dome.data.update()
for p in dome.data.polygons: p.use_smooth = True
primitive(bpy.ops.mesh.primitive_cylinder_add, 'DomeCrown', 'hull_dark', vertices=64, radius=G.DOME_TOP['r'], depth=G.DOME_TOP['h']*2,
          location=to_world(G.DOME_X, 0, G.DECK+G.SKIRT+G.CAP-0.35))
# forward-facing emissive arc on the cap, as on the render
arc = primitive(bpy.ops.mesh.primitive_torus_add, 'DomeVisor', 'emissive', major_segments=96, minor_segments=8,
                major_radius=G.DOME_R*0.72, minor_radius=0.12, location=to_world(G.DOME_X, 0, G.DECK+G.SKIRT+G.CAP*0.62))
arc.scale = (1.0, 1.0, 0.45)
cut(arc, cutter_box('VisorCutter', G.DOME_X-1.0, G.DOME_X+G.DOME_R+1, -G.DOME_R-1, G.DOME_R+1, 0, 20))   # keep the forward half only

# ------------------------------------------------------------------ turret pods with muzzle slits, lower fairings
for sgn in (1,-1):
    tt = G.TUR_TOP
    deck_here = G.interp(G.top_line, (tt['x0']+tt['x1'])/2) - 0.3
    pod = bevel(box('TurretPod%+d'%sgn, tt['x0'], tt['x1'], sgn*tt['y0'], sgn*tt['y1'], deck_here-0.4, deck_here+tt['h']), 0.9, 6)
    ym = sgn*(tt['y0']+tt['y1'])/2
    cut(pod, cutter_box('SlitCutter%+d'%sgn, tt['x0']+2.0, tt['x0']+8.0, ym-0.22, ym+0.22, deck_here+tt['h']-0.35, deck_here+tt['h']+1))
    fa = G.FAIRING
    belly = G.interp(G.bot_line, (fa['x0']+fa['x1'])/2) + 0.5
    bevel(box('Fairing%+d'%sgn, fa['x0'], fa['x1'], sgn*fa['y0'], sgn*fa['y1'], belly-fa['h'], belly), 0.7, 5)

# ------------------------------------------------------------------ nacelles: bevelled prisms, recessed round intakes
for sgn in (1,-1):
    nu = G.NAC_UP
    nac = bevel(prism('NacelleUp%+d'%sgn, [(x, sgn*y) for x,y in G.splayed_plan(nu)], nu['z0'], nu['z1']), 0.45, 4)
    ym = sgn*(nu['yi0']+nu['yo0'])/2; zm = (nu['z0']+nu['z1'])/2; r = (nu['z1']-nu['z0'])/2 - 0.5
    cyl_cut = primitive(bpy.ops.mesh.primitive_cylinder_add, 'IntakeCutter%+d'%sgn, 'hull_dark', vertices=48, radius=r, depth=1.2,
                        location=to_world(nu['x0']+0.1, ym, zm))
    cyl_cut.rotation_euler = (0, math.radians(90), 0)
    PARTS.remove((cyl_cut, 'hull_dark')); cut(nac, cyl_cut)
    cylinder_x('IntakeDisc%+d'%sgn, nu['x0']+0.55, nu['x0']+0.75, ym, zm, r-0.05, 'glass')
    cylinder_x('IntakeRing%+d'%sgn, nu['x0']+0.5, nu['x0']+0.7, ym, zm, r+0.02, 'hull_dark')

# ------------------------------------------------------------------ wings, fins, spikes
for sgn in (1,-1):
    mid = [(x, sgn*y, G.WING['zc']) for x,y in G.WING['plan']]
    offs = [(0.0, 0.0, G.wing_half_thickness(y)) for x,y in G.WING['plan']]
    bevel(slab('WingLo%+d'%sgn, mid, offs, 'hull_dark'), 0.12, 2)
for nm, fin in (('FinFwd', G.FIN_FWD), ('FinAft', G.FIN_AFT)):
    mid = [(x, 0.0, z) for x,z in fin['poly']]
    offs = [(0.0, G.fin_half_thickness(fin, z), 0.0) for x,z in fin['poly']]
    bevel(slab(nm, mid, offs, 'hull_dark'), 0.06, 2)
for sgn, tag in ((1,'S'), (-1,'P')):
    for nm, s in (('Trail', G.TRAIL), ('NoseSpike', G.NOSE_SPIKE)):
        spike(nm+tag, (s[0][0], sgn*s[0][1], s[0][2]), (s[1][0], sgn*s[1][1], s[1][2]))

# ------------------------------------------------------------------ transom: three main nozzles
for y in (-3.2, 0.0, 3.2):
    cylinder_x('Nozzle%+.0f'%y, G.HULL_END-0.5, G.HULL_END+0.9, y, -0.2, 1.05, 'hull_dark')
    cylinder_x('NozzleBore%+.0f'%y, G.HULL_END+0.6, G.HULL_END+0.95, y, -0.2, 0.8, 'glass')

# ------------------------------------------------------------------ merge everything into one mesh
depsgraph = bpy.context.evaluated_depsgraph_get()
merged = bmesh.new()
tri_count = 0
for ob, mat in PARTS:
    ev = ob.evaluated_get(depsgraph)
    me = bpy.data.meshes.new_from_object(ev, preserve_all_data_layers=False, depsgraph=depsgraph)
    me.transform(ob.matrix_world)
    for p in me.polygons: p.material_index = SLOT[mat]
    merged.from_mesh(me)
    bpy.data.meshes.remove(me)

final_me = bpy.data.meshes.new('SM_Krishna')
merged.to_mesh(final_me); merged.free()
for m in MATS.values(): final_me.materials.append(m)
final = bpy.data.objects.new('SM_Krishna', final_me)
scene.collection.objects.link(final)
for ob, _ in PARTS: bpy.data.objects.remove(ob, do_unlink=True)
for ob in CUTTERS: bpy.data.objects.remove(ob, do_unlink=True)

# open the vestibule into the cabin: the boolean left its inner end as a wall at the cube's inner face,
# which would stand in the rig's doorway. Everything within the socket's footprint at that plane goes.
bm = bmesh.new(); bm.from_mesh(final_me)
bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-4)
end_plane = to_world(0, VEST['y0'], 0).y
doorway = [f for f in bm.faces
           if abs(f.calc_center_median().y - end_plane) < 0.02
           and to_world(VEST['x1'],0,0).x - 0.01 <= f.calc_center_median().x <= to_world(VEST['x0'],0,0).x + 0.01
           and VEST['z0'] - 0.01 <= f.calc_center_median().z <= VEST['z1'] + 0.01]
print(f'STATS vestibule_end_faces_removed={len(doorway)}')
bmesh.ops.delete(bm, geom=doorway, context='FACES')

# n-gons (cylinder caps, boolean leftovers) cannot carry tangents through FBX: triangulate them, quads stay
bmesh.ops.triangulate(bm, faces=[f for f in bm.faces if len(f.verts) > 4], quad_method='BEAUTY', ngon_method='BEAUTY')

# shading: smooth faces, sharp edges above 35 degrees -- the hard-surface look without extra geometry
for f in bm.faces: f.smooth = True
for e in bm.edges:
    if len(e.link_faces) == 2:
        e.smooth = e.calc_face_angle(0.0) < math.radians(35)
    else:
        e.smooth = False
bm.to_mesh(final_me); bm.free()

# pivot: the centre of the rig's cabin, so the mesh drops onto ASFTestShip's origin
pivot = to_world(c['cx'], 0.0, (c['z0']+c['z1'])/2)
final_me.transform(Matrix.Translation(-pivot))
final.location = (0, 0, 0)

# UVs: automatic projection, good enough for the grey stage; the lightmap UV is Unreal's job
bpy.context.view_layer.objects.active = final
final.select_set(True)
bpy.ops.object.mode_set(mode='EDIT')
bpy.ops.mesh.select_all(action='SELECT')
bpy.ops.uv.smart_project(angle_limit=math.radians(66), island_margin=0.002)
bpy.ops.object.mode_set(mode='OBJECT')

# ------------------------------------------------------------------ stats
final_me.calc_loop_triangles()
bb = [final.matrix_world @ Vector(v) for v in final.bound_box]
mins = Vector((min(v.x for v in bb), min(v.y for v in bb), min(v.z for v in bb)))
maxs = Vector((max(v.x for v in bb), max(v.y for v in bb), max(v.z for v in bb)))
bm = bmesh.new(); bm.from_mesh(final_me)
non_manifold = sum(1 for e in bm.edges if not e.is_manifold)
bm.free()
print(f'STATS verts={len(final_me.vertices)} faces={len(final_me.polygons)} tris={len(final_me.loop_triangles)} '
      f'non_manifold_edges={non_manifold} slots={len(final_me.materials)}')
print(f'STATS bbox_min=({mins.x:.2f},{mins.y:.2f},{mins.z:.2f}) bbox_max=({maxs.x:.2f},{maxs.y:.2f},{maxs.z:.2f}) '
      f'size=({maxs.x-mins.x:.2f},{maxs.y-mins.y:.2f},{maxs.z-mins.z:.2f}) m')
print(f'STATS pivot_to_nose={maxs.x:.2f} m  pivot_to_belly={-mins.z:.2f} m')

# ------------------------------------------------------------------ export FBX
bpy.ops.object.select_all(action='DESELECT'); final.select_set(True)
bpy.ops.export_scene.fbx(filepath=FBX_PATH, use_selection=True, object_types={'MESH'},
                         apply_unit_scale=True, apply_scale_options='FBX_SCALE_NONE', global_scale=1.0,
                         axis_forward='-Z', axis_up='Y', use_mesh_modifiers=True, mesh_smooth_type='EDGE',
                         use_tspace=True, add_leaf_bones=False, bake_space_transform=False, path_mode='AUTO')
print('EXPORTED', FBX_PATH, os.path.getsize(FBX_PATH), 'bytes')

# ------------------------------------------------------------------ check renders (Workbench, material colours)
def add_camera(name, loc, target, lens=40, ortho=None):
    cam = bpy.data.cameras.new(name); ob = bpy.data.objects.new(name, cam)
    scene.collection.objects.link(ob); ob.location = loc
    ob.rotation_euler = (Vector(target) - Vector(loc)).to_track_quat('-Z','Y').to_euler()
    cam.lens = lens; cam.clip_end = 1000
    if ortho: cam.type = 'ORTHO'; cam.ortho_scale = ortho
    return ob
centre = Vector((5.0, 0.0, 0.0))
cams = [
    ('model_hero',  add_camera('CamHero',  centre + Vector((38, 74, 22)), centre, 40)),
    ('model_rear',  add_camera('CamRear',  centre + Vector((-62, -50, 30)), centre, 35)),
    ('model_stbd',  add_camera('CamStbd',  centre + Vector((20, -70, 8)), centre, 40)),   # hatch side
    ('model_below', add_camera('CamBelow', centre + Vector((45, 40, -45)), centre, 40)),
    ('model_top',   add_camera('CamTop',   centre + Vector((0, 0, 120)), centre, 40, ortho=70)),
    # the hatch socket, close: pivot is the cabin centre, the socket sits on the starboard (-Y) skin below mid-height
    ('model_hatch', add_camera('CamHatch', Vector((6, -26, -1)), Vector((0, -7, -2.5)), 50)),
]
scene.render.engine = 'BLENDER_WORKBENCH'
sh = scene.display.shading
sh.light = 'STUDIO'; sh.color_type = 'MATERIAL'
sh.show_cavity = True; sh.cavity_type = 'BOTH'; sh.cavity_ridge_factor = 1.2; sh.cavity_valley_factor = 1.2
sh.show_object_outline = True; sh.show_shadows = False
scene.display.render_aa = '8'
scene.render.resolution_x = 1600; scene.render.resolution_y = 1000
world = bpy.data.worlds.new('World'); scene.world = world; world.color = (0.93, 0.93, 0.92)
scene.view_settings.view_transform = 'Standard'
for nm, cam in cams:
    scene.camera = cam
    scene.render.filepath = os.path.join(RENDER_DIR, f'krishna_{nm}.png')
    bpy.ops.render.render(write_still=True)
    print('rendered', nm)

bpy.ops.wm.save_as_mainfile(filepath=os.path.join(RENDER_DIR, 'krishna_model.blend'))
print('saved blend')
