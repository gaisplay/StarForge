# Krishna ASX-08 three-view blueprint, 48.9 m. All geometry comes from geometry.py, in metres.
# Run: python threeview.py [out_dir]   -> writes krishna_threeview.svg into out_dir (default: cwd)
import math, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from geometry import *

OUT_DIR = sys.argv[1] if len(sys.argv) > 1 else '.'
S = 12.0  # px per metre

out = []
def emit(s): out.append(s)
def P(pts): return ' '.join(f'{x:.1f},{y:.1f}' for x,y in pts)

class View:
    def __init__(self, ox, oy, fx, fy):
        self.ox, self.oy, self.fx, self.fy = ox, oy, fx, fy
    def p(self, a, b):
        return (self.ox + a*S*self.fx, self.oy + b*S*self.fy)
    def poly(self, pts, cls='hull', closed=True):
        tag = 'polygon' if closed else 'polyline'
        emit(f'<{tag} class="{cls}" points="{P([self.p(a,b) for a,b in pts])}"/>')
    def line(self, a, b, cls='hull'):
        (x1,y1),(x2,y2) = self.p(*a), self.p(*b)
        emit(f'<line class="{cls}" x1="{x1:.1f}" y1="{y1:.1f}" x2="{x2:.1f}" y2="{y2:.1f}"/>')
    def circle(self, c, r, cls='hull'):
        x,y = self.p(*c); emit(f'<circle class="{cls}" cx="{x:.1f}" cy="{y:.1f}" r="{r*S:.1f}"/>')
    def rect(self, a0, b0, a1, b1, cls='hull', rx=0):
        (x0,y0),(x1,y1) = self.p(a0,b0), self.p(a1,b1)
        x,y = min(x0,x1), min(y0,y1); w,h = abs(x1-x0), abs(y1-y0)
        emit(f'<rect class="{cls}" x="{x:.1f}" y="{y:.1f}" width="{w:.1f}" height="{h:.1f}" rx="{rx*S:.1f}"/>')
    def cap(self, a0, a1, b, rz, cls='hull2'):
        (x0,y0),(x1,_) = self.p(a0,b), self.p(a1,b)
        emit(f'<path class="{cls}" d="M{x0:.1f},{y0:.1f} A{abs(x1-x0)/2:.1f},{rz*S:.1f} 0 0 1 {x1:.1f},{y0:.1f} Z"/>')
    def text(self, a, b, s, cls='lbl', anchor='middle', dx=0, dy=0):
        x,y = self.p(a,b); emit(f'<text class="{cls}" x="{x+dx:.1f}" y="{y+dy:.1f}" text-anchor="{anchor}">{s}</text>')
    def dim_h(self, a0, a1, b, label):
        (x0,y),(x1,_) = self.p(a0,b), self.p(a1,b)
        emit(f'<line class="dim" x1="{x0:.1f}" y1="{y:.1f}" x2="{x1:.1f}" y2="{y:.1f}" marker-start="url(#ar)" marker-end="url(#ar)"/>')
        emit(f'<text class="dimt" x="{(x0+x1)/2:.1f}" y="{y-4:.1f}" text-anchor="middle">{label}</text>')
    def dim_v(self, a, b0, b1, label, side='right'):
        (x,y0),(_,y1) = self.p(a,b0), self.p(a,b1)
        emit(f'<line class="dim" x1="{x:.1f}" y1="{y0:.1f}" x2="{x:.1f}" y2="{y1:.1f}" marker-start="url(#ar)" marker-end="url(#ar)"/>')
        ym = (y0+y1)/2
        if side == 'right':
            emit(f'<text class="dimt" x="{x+5:.1f}" y="{ym+4:.1f}" text-anchor="start">{label}</text>')
        else:
            emit(f'<text class="dimt" transform="rotate(-90 {x-5:.1f} {ym:.1f})" x="{x-5:.1f}" y="{ym:.1f}" text-anchor="middle">{label}</text>')
    def grid(self, a0, a1, b0, b1, step=5):
        a = math.floor(a0/step)*step
        while a <= a1:
            self.line((a,b0),(a,b1),'grid'); a += step
        b = math.floor(b0/step)*step
        while b <= b1:
            self.line((a0,b),(a1,b),'grid'); b += step

def mirror(pts): return [(a,-b) for a,b in pts]
def sg(pts, sgn): return [(a, sgn*b) for a,b in pts]

W, H = 1560, 1320
emit(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}" font-family="Segoe UI, Arial, sans-serif">')
emit('''<defs><marker id="ar" viewBox="0 0 10 10" refX="5" refY="5" markerWidth="6" markerHeight="6" orient="auto-start-reverse"><path d="M0,2 L6,5 L0,8 z" fill="#1d4ed8"/></marker></defs>
<style>
 .bg{fill:#fbfbf8} .grid{stroke:#dde3ea;stroke-width:0.6} .axis{stroke:#9aa5b1;stroke-width:0.8;stroke-dasharray:6 4}
 .hull{fill:#eef1f4;stroke:#1f2937;stroke-width:1.6;stroke-linejoin:round} .hull2{fill:#e1e6eb;stroke:#1f2937;stroke-width:1.4}
 .hull3{fill:#d3d9df;stroke:#1f2937;stroke-width:1.2} .blue{fill:#5b6f8a;stroke:#1f2937;stroke-width:1} .cyan{fill:#67d8e0;stroke:#1f2937;stroke-width:0.8}
 .hid{fill:none;stroke:#1f2937;stroke-width:1;stroke-dasharray:5 3} .spike{stroke:#1f2937;stroke-width:2.2;stroke-linecap:round}
 .thin{fill:none;stroke:#4b5563;stroke-width:0.9} .cab{fill:rgba(220,38,38,0.08);stroke:#dc2626;stroke-width:1.6;stroke-dasharray:7 4}
 .cabl{stroke:#dc2626;stroke-width:1.2;stroke-dasharray:3 3} .thr{stroke:#dc2626;stroke-width:1.4} .hatch{fill:#dc2626;stroke:none}
 .dim{stroke:#1d4ed8;stroke-width:1} .dimt{fill:#1d4ed8;font-size:12px;font-weight:600} .lbl{fill:#374151;font-size:12px}
 .title{fill:#111827;font-size:20px;font-weight:700} .sub{fill:#4b5563;font-size:12.5px} .vt{fill:#111827;font-size:14px;font-weight:700}
 .note{fill:#991b1b;font-size:11.5px;font-weight:600}
</style>''')
emit(f'<rect class="bg" width="{W}" height="{H}"/>')
emit('<text class="title" x="40" y="42">ASX-08 «Кришна» — чертёж в трёх проекциях, третий проход по официальному арту</text>')
emit('<text class="sub" x="40" y="64">Полная длина 48.9 м — официальный сайт аниме-адаптации saikyosoubi.com: «小型艦 クリシュナ 全長 48.9ｍ». Шипы-антенны в 48.9 не входят (принятое допущение). Купол 12.5 м — решение владельца от 2026-09-22.</text>')
emit('<text class="sub" x="40" y="82">Сетка 5 м. Красным — кабина текущего тестового корабля (куб 8 м), её хатч 2×2.5 м в правом борту и четыре угла двигателей физики на ±2 м.</text>')

c = CAB
# ===== TOP VIEW (nose left, starboard up) =====
T = View(150, 360, 1, -1)
T.grid(-2, 64, -19, 19)
T.line((-2,0),(64,0),'axis')
emit('<text class="vt" x="40" y="120">ВИД СВЕРХУ</text>')
T.text(0, 18.2, 'правый борт (+Y), сторона хатча', 'lbl', 'start')
for sgn in (1,-1):                                           # things under the hull first
    T.poly(sg(WING['plan'], sgn), 'hull3')
    T.rect(FAIRING['x0'], sgn*FAIRING['y0'], FAIRING['x1'], sgn*FAIRING['y1'], 'hull3', rx=1.4)
full = hull_plan + [(HULL_END,0)] + list(reversed(mirror(hull_plan)))[:-1]
T.poly(full, 'hull')
T.line((0,0),(HULL_END,0),'thin')
for sgn in (1,-1):
    T.rect(FAIRING['x0'], sgn*FAIRING['y0'], FAIRING['x1'], sgn*FAIRING['y1'], 'hid', rx=1.4)   # hidden part of the fairings
    T.poly(sg(splayed_plan(NAC_UP), sgn), 'hull2')
    T.rect(NAC_UP['x0']-0.3, sgn*(NAC_UP['yi0']+0.5), NAC_UP['x0']+0.6, sgn*(NAC_UP['yo0']-0.5), 'blue')
    T.rect(TUR_TOP['x0'], sgn*TUR_TOP['y0'], TUR_TOP['x1'], sgn*TUR_TOP['y1'], 'hull2', rx=1.7)
T.poly([(SPINE['x0'],-0.6),(SPINE['x0']+3,-SPINE['yh']),(SPINE['x1'],-SPINE['yh']),(SPINE['x1'],SPINE['yh']),(SPINE['x0']+3,SPINE['yh']),(SPINE['x0'],0.6)], 'hull2')
T.rect(COCKPIT['x0'], -COCKPIT['yh'], COCKPIT['x1'], COCKPIT['yh'], 'hull2', rx=0.3)
T.rect(COCKPIT['x0'], -COCKPIT['yh']*0.8, COCKPIT['x0']+0.4, COCKPIT['yh']*0.8, 'cyan')
T.circle((DOME_X,0), DOME_R, 'hull2')
T.circle((DOME_X,0), DOME_TOP['r'], 'thin')
for fin in (FIN_FWD, FIN_AFT):                                # blades seen edge-on
    xs = [p[0] for p in fin['poly']]; t = fin['t_root']/2
    T.poly([(min(xs), -t), (max(xs), -fin['t_tip']/2), (max(xs), fin['t_tip']/2), (min(xs), t)], 'hull2')
for sgn in (1,-1):
    for s in (TRAIL, NOSE_SPIKE):
        T.line((s[0][0], sgn*s[0][1]), (s[1][0], sgn*s[1][1]), 'spike')
T.rect(c['cx']-c['half'], -c['half'], c['cx']+c['half'], c['half'], 'cab')
for x,y in thruster_corners():
    T.line((x-0.5,y),(x+0.5,y),'thr'); T.line((x,y-0.5),(x,y+0.5),'thr')
T.rect(c['cx']-HATCH['w']/2, c['half']-0.15, c['cx']+HATCH['w']/2, c['half']+0.15, 'hatch')
T.text(c['cx']+c['half']+0.4, c['half']+1.6, 'хатч, 4 м до обшивки', 'note', 'start')
T.dim_h(0, L, -17.0, '48.9 м — нос до кормы нижних крыльев')
T.dim_h(0, HULL_END, -14.8, '45.5 м корпус')
T.dim_v(-4.5, -8.0, 8.0, 'корпус 16 м', 'left')
T.dim_v(-9.0, -WING['y_tip'], WING['y_tip'], f'размах {SPAN:.1f} м', 'left')
T.dim_h(DOME_X-DOME_R, DOME_X+DOME_R, 15.0, 'купол 12.5 м')
T.dim_h(TUR_TOP['x0'], TUR_TOP['x1'], 8.0, 'турельные капли 15 м')
T.dim_h(FAIRING['x0'], FAIRING['x1'], -10.2, 'нижние обтекатели 22 м')
T.text(19.5, 4.6, 'турель (верх)', 'lbl'); T.text(13, 0.0, 'хребет', 'lbl', dy=4); T.text(24.2, -0.2, 'окно', 'lbl', dy=4)
T.text(38, 9.3, 'верхняя гондола', 'lbl'); T.text(40, 12.9, 'нижнее крыло', 'lbl'); T.text(18, -7.7, 'обтекатель (низ)', 'lbl')
T.text(57, 13.6, 'шипы', 'lbl', 'start'); T.text(50, 1.2, 'кили-лезвия (с ребра)', 'lbl', 'start')

# ===== SIDE VIEW (port profile, nose left) =====
Sv = View(150, 935, 1, -1)
Sv.grid(-2, 64, -12, 17)
Sv.line((-2,0),(64,0),'axis')
emit('<text class="vt" x="40" y="700">ВИД СБОКУ (левый борт)</text>')
Sv.poly(top_line + list(reversed(bot_line)), 'hull')
Sv.poly(chine, 'thin', closed=False)
# spine and cockpit block ride on the deck line
Sv.poly([(SPINE['x0'], interp(top_line,SPINE['x0'])),(SPINE['x0']+3, interp(top_line,SPINE['x0']+3)+SPINE['h']),(SPINE['x1'], interp(top_line,SPINE['x1'])+SPINE['h']),(SPINE['x1'], interp(top_line,SPINE['x1']))], 'hull2')
Sv.rect(COCKPIT['x0'], DECK-0.2, COCKPIT['x1'], DECK+COCKPIT['h'], 'hull2')
Sv.rect(COCKPIT['x0'], DECK+0.3, COCKPIT['x0']+0.4, DECK+0.7, 'cyan')
Sv.rect(DOME_X-DOME_R, DECK, DOME_X+DOME_R, DECK+SKIRT, 'hull2')
Sv.cap(DOME_X-DOME_R, DOME_X+DOME_R, DECK+SKIRT, CAP)
Sv.rect(DOME_X-DOME_R+0.3, DECK+0.35, DOME_X+DOME_R-0.3, DECK+SKIRT-0.35, 'thin')
Sv.rect(DOME_X-DOME_TOP['r'], DECK+SKIRT+CAP-0.35, DOME_X+DOME_TOP['r'], DECK+SKIRT+CAP+0.05, 'hull2')
# near-side (port) attachments drawn over the hull
tt = TUR_TOP; d0 = interp(top_line, tt['x0']); d1 = interp(top_line, tt['x1'])
Sv.poly([(tt['x0'], d0-0.2),(tt['x0']+2.5, d0+tt['h']-0.3),(tt['x1']-3, d1+tt['h']-0.2),(tt['x1'], d1-0.1)], 'hull2')
fa = FAIRING; b0 = interp(bot_line, fa['x0'])+0.5; b1 = interp(bot_line, fa['x1'])+0.5
Sv.poly([(fa['x0'], b0),(fa['x0']+2.5, b0-fa['h']),(fa['x1']-3, b1-fa['h']),(fa['x1'], b1)], 'hull2')
wz, wt, wtip = WING['zc'], WING['t_root']/2, WING['t_tip']/2
Sv.poly([(WING['plan'][0][0], wz+wt), (L, wz+wtip), (L, wz-wtip), (WING['plan'][4][0], wz-wt)], 'hull3')
nu = NAC_UP
Sv.poly([(nu['x0']+1.2, nu['z1']),(nu['x1'], nu['z1']),(nu['x1'], nu['z0']),(nu['x0']+1.2, nu['z0']),(nu['x0'], nu['z0']+1.2),(nu['x0'], nu['z1']-1.2)], 'hull2')
Sv.rect(nu['x0']-0.3, nu['z0']+0.6, nu['x0']+0.6, nu['z1']-0.6, 'blue')
for fin in (FIN_FWD, FIN_AFT):
    Sv.poly(fin['poly'], 'hull2')
for s in (TRAIL, NOSE_SPIKE):
    Sv.line((s[0][0], s[0][2]), (s[1][0], s[1][2]), 'spike')
Sv.rect(c['cx']-c['half'], c['z0'], c['cx']+c['half'], c['z1'], 'cab')
Sv.line((c['cx']-c['half'], c['z0']+3.0), (c['cx']+c['half'], c['z0']+3.0), 'cabl')
Sv.rect(c['cx']-HATCH['w']/2, c['z0']+0.1, c['cx']+HATCH['w']/2, c['z0']+0.1+HATCH['h'], 'hid')
Sv.text(c['cx'], c['z0']+1.1, 'хатч', 'note')
Sv.text(c['cx']-c['half']-0.5, c['z1']+2.6, 'потолок кабины +2.0 м над палубой,', 'note', 'end')
Sv.text(c['cx']-c['half']-0.5, c['z1']+1.6, 'до купола в углу 0.33 м', 'note', 'end')
Sv.dim_v(-4.5, -3.3, DECK, f'корпус {HEIGHT_BODY:.1f} м', 'left')
Sv.dim_v(-9.0, -3.3, DECK+SKIRT+CAP, f'с куполом {HEIGHT_DOME:.1f} м', 'left')
Sv.dim_v(64.5, TRAIL[1][2], FIN_FWD['poly'][3][1], 'с килями и шипами 25 м')
Sv.dim_h(DOME_X-DOME_R, DOME_X+DOME_R, DECK+SKIRT+CAP+1.0, 'купол 12.5 м')
Sv.dim_v(DOME_X+DOME_R+0.8, DECK, DECK+SKIRT, 'бортик 1.2 м')
Sv.dim_h(0, L, -8.0, '48.9 м')
Sv.dim_h(NAC_UP['x0'], NAC_UP['x1'], nu['z1']+0.8, 'гондола 14 м')
Sv.text(19, 5.2, 'турель', 'lbl'); Sv.text(38, 2.2, 'верхняя гондола', 'lbl'); Sv.text(40, -1.9, 'нижнее крыло', 'lbl')
Sv.text(19, -5.9, 'обтекатель', 'lbl'); Sv.text(13, 3.6, 'хребет', 'lbl'); Sv.text(24.2, DECK+COCKPIT['h']+0.9, 'окно', 'lbl')

# ===== FRONT VIEW (from the nose; starboard on the viewer's left) =====
F = View(1300, 935, -1, -1)
F.grid(-15, 15, -12, 17)
F.line((-15,0),(15,0),'axis'); F.line((0,-12),(0,17),'axis')
emit('<text class="vt" x="1130" y="700">ВИД СПЕРЕДИ</text>')
F.text(14.5, 16.2, 'правый борт', 'lbl', 'start')
for sgn in (1,-1):
    F.poly([(sgn*WING['y_root'], WING['zc']+WING['t_root']/2), (sgn*WING['y_tip'], WING['zc']+WING['t_tip']/2),
            (sgn*WING['y_tip'], WING['zc']-WING['t_tip']/2), (sgn*WING['y_root'], WING['zc']-WING['t_root']/2)], 'hull3')
    F.rect(sgn*NAC_UP['yi0'], NAC_UP['z0'], sgn*NAC_UP['yo1'], NAC_UP['z1'], 'hull2', rx=0.5)
    F.rect(sgn*(NAC_UP['yi0']+0.5), NAC_UP['z0']+0.6, sgn*(NAC_UP['yo0']-0.5), NAC_UP['z1']-0.6, 'blue', rx=1.2)
F.rect(-DOME_R, DECK, DOME_R, DECK+SKIRT, 'hull2')
F.cap(-DOME_R, DOME_R, DECK+SKIRT, CAP)
F.rect(-DOME_R+0.3, DECK+0.35, DOME_R-0.3, DECK+SKIRT-0.35, 'thin')
F.rect(-DOME_TOP['r'], DECK+SKIRT+CAP-0.35, DOME_TOP['r'], DECK+SKIRT+CAP+0.05, 'hull2')
F.rect(-COCKPIT['yh'], DECK-0.2, COCKPIT['yh'], DECK+COCKPIT['h'], 'hull2')
F.rect(-COCKPIT['yh']*0.8, DECK+0.3, COCKPIT['yh']*0.8, DECK+0.7, 'cyan')
F.poly(section_hex(), 'hull')
F.poly([(-SPINE['yh'], 3.0),(SPINE['yh'], 3.0),(SPINE['yh']*0.45, 3.0+SPINE['h']),(-SPINE['yh']*0.45, 3.0+SPINE['h'])], 'hull2')
for sgn in (1,-1):
    F.rect(sgn*TUR_TOP['y0'], 2.7, sgn*TUR_TOP['y1'], 2.7+TUR_TOP['h'], 'hull2', rx=0.8)
    F.rect(sgn*FAIRING['y0'], -2.8, sgn*FAIRING['y1'], -2.8-FAIRING['h'], 'hull2', rx=0.7)
for fin in (FIN_AFT, FIN_FWD):
    zs = [p[1] for p in fin['poly']]; t = fin['t_root']/2; tt = fin['t_tip']/2
    F.poly([(-t, min(zs)), (t, min(zs)), (tt, max(zs)), (-tt, max(zs))], 'hull2')
for sgn in (1,-1):
    for s in (TRAIL, NOSE_SPIKE):
        F.line((sgn*s[0][1], s[0][2]), (sgn*s[1][1], s[1][2]), 'spike')
F.rect(-c['half'], c['z0'], c['half'], c['z1'], 'cab')
F.rect(c['half']-0.15, c['z0']+0.1, c['half']+0.15, c['z0']+0.1+HATCH['h'], 'hatch')
F.text(c['half']+0.6, c['z0']+1.1, 'хатч', 'note', 'end')
F.dim_h(-WING['y_tip'], WING['y_tip'], -9.3, f'размах {SPAN:.1f} м')
F.dim_h(-8.0, 8.0, -7.0, 'корпус 16 м')
F.dim_v(-16.5, -3.3, DECK+SKIRT+CAP, f'{HEIGHT_DOME:.1f} м')
F.dim_v(11.0, c['z0'], c['z1'], 'кабина 8 м', 'left')

ny = 1185
notes = [
 'Открытые вопросы к владельцу (после решения по куполу):',
 '1. Хатч. Стыковочный порт куба стоит на его стенке +Y (4 м от оси), а обшивка на этой станции — на 8 м. Между ними 4 м корпуса: нужен тамбур-тоннель, либо порт переезжает на обшивку и стыковка требует правки.',
 '2. Длина. 48.9 м принята как нос — корма нижних крыльев; шипы-антенны (до 60 м по X, 15 м вверх) в неё не входят. Если 全長 у дизайнера включал шипы, корпус ужимается примерно до 38 м, и кабина не помещается вовсе.',
 '3. Двигатели физики (±2 м, четыре угла на плоскости Z=0) — точки приложения силы, не сопла. Видимые сопла на модели — синие круги гондол и транец; их положение на физику не влияет.',
 '4. Что такое купол. У новеллы кокпит впереди, жилой отсек за ним; на арте перед куполом стоит блок с окном-полосой. Купол читается как надстройка над жилым отсеком, окно — как кокпит. На компоновку интерьера это влияет, на модель корпуса — нет.',
]
for i,s in enumerate(notes):
    emit(f'<text class="{ "vt" if i==0 else "sub"}" x="40" y="{ny + i*22}">{s}</text>')
emit('</svg>')
path = os.path.join(OUT_DIR, 'krishna_threeview.svg')
open(path, 'w', encoding='utf-8').write('\n'.join(out))
print('ok', len(out), 'elements ->', path)
