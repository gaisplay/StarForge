# Shared massing of the Krishna, in metres. Ship coords: x aft from the nose tip, y to starboard, z up.
# z = 0 is the hull mid-plane at the dome station (the rig's thruster plane once the cabin is placed).
# Third pass, fitted to the official anime render (reference/official-anime-krishna-1587x982.png):
# the two tall fins are flat blades and the lower wings are tapered plates with a swept tip.
L = 48.9           # nose tip to the tip of the lower wings (assumption: antenna spikes not counted)
HULL_END = 45.5    # the hull body's stern; the wings run on to L

# plan half-outline: a broad flat chisel, not a needle
hull_plan = [(0,1.0),(3,2.2),(8,4.0),(14,5.6),(20,6.8),(26,7.6),(32,8.0),(38,8.0),(43,7.4),(HULL_END,6.4)]
top_line  = [(0,0.35),(6,1.2),(12,1.9),(18,2.5),(24,2.9),(30,3.0),(38,3.0),(43,2.9),(HULL_END,2.6)]
bot_line  = [(0,-0.35),(6,-1.1),(12,-1.9),(18,-2.5),(24,-3.0),(26,-3.3),(38,-3.3),(43,-2.9),(HULL_END,-2.5)]
chine     = [(0,0.0),(12,0.0),(30,-0.15),(HULL_END,0.0)]
CHINE_WIDTH_FRACTION = 0.80   # top/bottom faces are this fraction of the chine half-width

SPINE   = dict(x0=3, x1=23, yh=1.6, h=0.8)        # raised ridge along the top centreline
COCKPIT = dict(x0=22, x1=26.5, yh=2.6, h=1.1)     # block ahead of the dome carrying the window strip

DOME_X, DOME_R, SKIRT, CAP, DECK = 30.5, 6.25, 1.2, 3.3, 3.0   # 12.5 m dome, owner's decision 2026-09-22
DOME_TOP = dict(r=3.2, h=0.3)                     # the small flat disc on the dome's crown

TUR_TOP = dict(x0=12, x1=27, y0=3.0, y1=6.4, h=1.7)    # upper turret pods: teardrops on the shoulders
FAIRING = dict(x0=8, x1=28.5, y0=3.4, y1=6.6, h=1.6)   # long lower fairings; end ahead of the hatch station

# Upper nacelles start at 33, not 31: the hatch socket on the starboard skin spans stations 29.5-31.5 and a
# docking tunnel entering it needs the skin clear there (owner's decision 2026-09-22: vestibule, port on the cube).
NAC_UP  = dict(x0=33, x1=45.5, yi0=7.6, yi1=7.2, yo0=11.2, yo1=12.0, z0=0.2, z1=4.2)  # upper nacelles, blue round front

# Lower wings: a tapered plate per side. Plan polygon (x, y) for the starboard wing, root on the hull skin,
# swept outer edge, sharp aft tip at L. Thickness tapers from the root to the tip.
# The root starts at 33, aft of the hatch socket (29.5-31.5), for the same reason as the nacelles.
WING = dict(plan=[(33.0, 7.4), (40.0, 12.4), (L, 13.6), (46.5, 9.4), (44.0, 7.6)],
            zc=-1.9, t_root=1.2, t_tip=0.25, y_root=7.4, y_tip=13.6)

# Fins: flat blades on the centreline, polygon in (x, z), thickness in y tapering to the tip.
FIN_FWD = dict(poly=[(24.5, 3.9), (27.8, 3.9), (40.4, 14.5), (39.6, 15.2)], t_root=0.7, t_tip=0.1)
FIN_AFT = dict(poly=[(37.5, 3.6), (42.5, 3.6), (57.5, 11.0), (56.5, 11.8)], t_root=0.7, t_tip=0.1)

TRAIL      = ((46.0, 11.6, -2.3), (58.0, 13.2, -10.0))   # spike trailing down from each wing
NOSE_SPIKE = ((8.5, 5.0, -3.6), (1.5, 5.4, -5.6))        # spike ahead of each lower fairing

CAB = dict(cx=30.5, half=4.0, z0=-3.0, z1=5.0)   # the rig's 8 m cabin, floor at the belly skin
HATCH = dict(w=2.0, h=2.5)
# The vestibule: a hatch-sized through-socket from the starboard skin to the cabin's +Y wall, with a 0.3 m frame.
# Its inner end lands at the wall's inner face (3.9 m), where the rig's own jambs and lintel take over.
VESTIBULE = dict(x0=CAB['cx']-HATCH['w']/2-0.3, x1=CAB['cx']+HATCH['w']/2+0.3, y0=3.9, y1=9.5,
                 z0=CAB['z0']+0.1-0.3, z1=CAB['z0']+0.1+HATCH['h']+0.3)

def thruster_corners():
    return [(CAB['cx']-2, -2), (CAB['cx']+2, -2), (CAB['cx']-2, 2), (CAB['cx']+2, 2)]

def splayed_plan(d):
    return [(d['x0'], d['yi0']), (d['x0'], d['yo0']), (d['x1'], d['yo1']), (d['x1'], d['yi1'])]

def section_hex(w=8.0, top=3.0, bot=-3.3, zc=-0.15):
    f = CHINE_WIDTH_FRACTION
    return [(-w,zc),(-f*w,top),(f*w,top),(w,zc),(f*w,bot),(-f*w,bot)]

def interp(pts, x):
    for (x0,y0),(x1,y1) in zip(pts, pts[1:]):
        if x0 <= x <= x1:
            t = 0 if x1 == x0 else (x-x0)/(x1-x0)
            return y0 + t*(y1-y0)
    return pts[-1][1]

def wing_half_thickness(y):
    """Half-thickness of a lower wing at spanwise position |y|, linear from root to tip."""
    w = WING
    t = (abs(y) - w['y_root']) / (w['y_tip'] - w['y_root'])
    t = min(1.0, max(0.0, t))
    return 0.5 * (w['t_root'] + t * (w['t_tip'] - w['t_root']))

def fin_half_thickness(fin, z):
    """Half-thickness of a fin at height z, linear from its base to its tip."""
    zs = [p[1] for p in fin['poly']]
    t = (z - min(zs)) / (max(zs) - min(zs))
    return 0.5 * (fin['t_root'] + t * (fin['t_tip'] - fin['t_root']))

SPAN = 2 * WING['y_tip']                       # 27.2 m
HEIGHT_BODY = DECK - bot_line[6][1]            # 6.3 m
HEIGHT_DOME = DECK + SKIRT + CAP - bot_line[6][1]   # 10.8 m
