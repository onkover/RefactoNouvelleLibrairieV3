#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
gen_level_fps.py — scene V3 de LibraryV3 : un niveau de type FPS (banc de rendu, chantier D).

Sorties (dans le dossier --out, par defaut le dossier courant) :
    level_fps.obj        le niveau : 6 pieces, 6 portails, accessoires
    level_fps.mtl        7 materiaux, sans texture (aucun souci de chargement)
    bench_V_V3.json      la scene : 4 cameras (vue de jeu + 3 vues debug) et le niveau
    level_fps.meta.json  pieces, portails, cameras, compte de faces (pour les sondes hors moteur)

Principes (voir le Lot V3 pas a pas) :
  * Les coques des pieces sont enroulees VERS L'INTERIEUR (normales inversees) : on les voit
    de l'interieur, de l'exterieur elles sont de dos. Les accessoires (colonnes, caisses,
    pilastres) sont enroules VERS L'EXTERIEUR, comme tout solide.
  * Les lignes de grille d'une piece sont COMMUNES a toutes ses surfaces (sol, plafond, murs)
    et incluent les bords des portails : aucun T-junction, donc aucune fissure.
  * La taille des cellules decroit avec la distance voulue : 4 m (piece A, tout pres) a 0,4 m
    (salle D, tout au fond) -> grands triangles de pres, triangles de 1 a 4 pixels au fond.
  * Sol de la salle B : EVENTAIL depuis un coin -> triangles tres longs et minces.
  * Deterministe : aucune source aleatoire hors du LCG ci-dessous.
Ordre de dessin = ordre du fichier : pieces de la plus proche a la plus lointaine
(--order far-near l'inverse : pire cas de surdessin).
"""
import argparse, json, math, os, sys

# ---------------------------------------------------------------- maillage
class Mesh:
    def __init__(self):
        self.v, self.vi = [], {}
        self.t, self.ti = [], {}
        self.groups = []            # (nom_groupe, [(mat, [(v,t),(v,t),(v,t)]), ...])
        self.cur = None
    def group(self, name):
        self.cur = (name, []); self.groups.append(self.cur)
    def _v(self, p):
        k = (round(p[0], 4), round(p[1], 4), round(p[2], 4))
        if k not in self.vi: self.vi[k] = len(self.v) + 1; self.v.append(k)
        return self.vi[k]
    def _t(self, uv):
        k = (round(uv[0], 4), round(uv[1], 4))
        if k not in self.ti: self.ti[k] = len(self.t) + 1; self.t.append(k)
        return self.ti[k]
    def tri(self, mat, p0, p1, p2, n, uv):
        """Triangle enroule pour que sa normale (p1-p0)x(p2-p0) regarde dans le sens de n."""
        a = [p1[i] - p0[i] for i in range(3)]; b = [p2[i] - p0[i] for i in range(3)]
        c = (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])
        if c[0]*n[0] + c[1]*n[1] + c[2]*n[2] < 0: p1, p2 = p2, p1
        self.cur[1].append((mat, [(self._v(p), self._t(uv(p))) for p in (p0, p1, p2)]))
    def count(self): return sum(len(g[1]) for g in self.groups)

def lcg(seed):
    s = seed & 0xFFFFFFFF
    while True:
        s = (1664525 * s + 1013904223) & 0xFFFFFFFF
        yield s / 4294967296.0

def reg(lo, hi, step):
    n = round((hi - lo) / step)
    assert abs(lo + n * step - hi) < 1e-6, f"dimension {hi-lo} non multiple de {step}"
    return [lo + i * step for i in range(n + 1)]

def merge(*ls):
    out = sorted({round(x, 4) for l in ls for x in l}); return out

def inside(h, a0, a1, b0, b1):
    return any(h[0] - 1e-6 <= a0 and a1 <= h[1] + 1e-6 and h[2] - 1e-6 <= b0 and b1 <= h[3] + 1e-6 for h in h_list(h))
def h_list(h): return h if isinstance(h, list) else [h]

# ---------------------------------------------------------------- une surface plane
AX = {0: (1, 2), 1: (0, 2), 2: (0, 1)}      # axes dans le plan pour une surface perpendiculaire a l'axe k
def plane(M, mat, k, coord, nsign, la, lb, holes=(), uvscale=2.0):
    """Grille irreguliere (la x lb) dans le plan axe_k = coord. holes : rectangles (a0,a1,b0,b1) laisses vides."""
    ia, ib = AX[k]
    n = [0, 0, 0]; n[k] = nsign
    def P(a, b):
        p = [0, 0, 0]; p[k] = coord; p[ia] = a; p[ib] = b; return tuple(p)
    uv = lambda p: (p[ia] / uvscale, p[ib] / uvscale)
    for i in range(len(la) - 1):
        for j in range(len(lb) - 1):
            a0, a1, b0, b1 = la[i], la[i+1], lb[j], lb[j+1]
            if any(h[0]-1e-6 <= a0 and a1 <= h[1]+1e-6 and h[2]-1e-6 <= b0 and b1 <= h[3]+1e-6 for h in holes):
                continue
            p00, p10, p11, p01 = P(a0, b0), P(a1, b0), P(a1, b1), P(a0, b1)
            if (i + j) & 1: M.tri(mat, p00, p10, p11, n, uv); M.tri(mat, p00, p11, p01, n, uv)
            else:           M.tri(mat, p00, p10, p01, n, uv); M.tri(mat, p10, p11, p01, n, uv)

# ---------------------------------------------------------------- une piece (coque vers l'interieur)
SHELL = +1      # +1 : coque vers l'INTERIEUR (normales inversees) ; -1 : contre-exemple, coque vers l'exterieur
def room(M, r):
    x0, x1, y0, y1, z0, z1, c = r['x0'], r['x1'], r['y0'], r['y1'], r['z0'], r['z1'], r['cell']
    op = r.get('open', {})                     # 'N','S' : (x0,x1,y0,y1) ; 'E','W' : (z0,z1,y0,y1)
    lx = merge(reg(x0, x1, c), *[[o[0], o[1]] for w in 'NS' for o in op.get(w, [])])
    lz = merge(reg(z0, z1, c), *[[o[0], o[1]] for w in 'EW' for o in op.get(w, [])])
    ly = merge(reg(y0, y1, c) if (y1-y0) % c < 1e-6 or abs(round((y1-y0)/c)*c-(y1-y0)) < 1e-6 else [y0, y1],
               *[[o[2], o[3]] for w in 'NSEW' for o in op.get(w, [])])
    M.group(r['name'])
    mf, mc, mw = r['mats']
    # sol : grille, ou eventail depuis le coin (x0,z0)
    if r.get('fan'):
        per = [(x, z0) for x in lx] + [(x1, z) for z in lz[1:]] + [(x, z1) for x in reversed(lx[:-1])] + [(x0, z) for z in reversed(lz[1:-1])]
        ctr = (x0, z0); n = (0, SHELL, 0)
        uv = lambda p: (p[0] / 2.0, p[2] / 2.0)
        for a, b in zip(per[1:], per[2:]):                      # per[0] = coin = centre
            if (a[1] == z0 and b[1] == z0) or (a[0] == x0 and b[0] == x0): continue
            M.tri(mf, (ctr[0], y0, ctr[1]), (a[0], y0, a[1]), (b[0], y0, b[1]), n, uv)
    else:
        plane(M, mf, 1, y0, +1*SHELL, lx, lz)
    plane(M, mc, 1, y1, -1*SHELL, lx, lz)
    # surface perpendiculaire a Z : axes du plan = (x, y) -> la = lx, lb = ly ; trou (x0,x1,y0,y1)
    plane(M, mw, 2, z0, +1*SHELL, lx, ly, [o for o in op.get('N', [])])     # mur lointain, normale +z
    plane(M, mw, 2, z1, -1*SHELL, lx, ly, [o for o in op.get('S', [])])     # mur proche,   normale -z
    # surface perpendiculaire a X : axes du plan = (y, z) -> la = ly, lb = lz ; trou (z0,z1,y0,y1) -> (y0,y1,z0,z1)
    plane(M, mw, 0, x0, +1*SHELL, ly, lz, [(o[2], o[3], o[0], o[1]) for o in op.get('W', [])])   # mur ouest, normale +x
    plane(M, mw, 0, x1, -1*SHELL, ly, lz, [(o[2], o[3], o[0], o[1]) for o in op.get('E', [])])   # mur est,   normale -x

# ---------------------------------------------------------------- accessoires (enroules vers l'exterieur)
def box(M, mat, cx, cy, cz, sx, sy, sz, sub=2, bottom=False, top=True):
    # bottom=False : pas de face de dessous (le solide repose sur le sol : invisible, coplanaire avec lui)
    # top=False    : pas de face de dessus (colle au plafond)
    x0, x1, y0, y1, z0, z1 = cx-sx/2, cx+sx/2, cy-sy/2, cy+sy/2, cz-sz/2, cz+sz/2
    gx = [x0 + (x1-x0)*i/sub for i in range(sub+1)]; gy = [y0 + (y1-y0)*i/sub for i in range(sub+1)]
    gz = [z0 + (z1-z0)*i/sub for i in range(sub+1)]
    if top: plane(M, mat, 1, y1, +1, gx, gz, uvscale=1.0)
    if bottom: plane(M, mat, 1, y0, -1, gx, gz, uvscale=1.0)
    plane(M, mat, 2, z1, +1, gx, gy, uvscale=1.0); plane(M, mat, 2, z0, -1, gx, gy, uvscale=1.0)
    plane(M, mat, 0, x1, +1, gy, gz, uvscale=1.0); plane(M, mat, 0, x0, -1, gy, gz, uvscale=1.0)

def pillar(M, mat, cx, cz, rad, sides, y0, y1, step):
    rows = round((y1 - y0) / step)
    for s in range(sides):
        a0, a1 = 2*math.pi*s/sides, 2*math.pi*(s+1)/sides
        am = 0.5*(a0+a1); n = (math.cos(am), 0, math.sin(am))
        for r_ in range(rows):
            ya, yb = y0 + r_*step, y0 + (r_+1)*step
            p00 = (cx+rad*math.cos(a0), ya, cz+rad*math.sin(a0)); p10 = (cx+rad*math.cos(a1), ya, cz+rad*math.sin(a1))
            p11 = (cx+rad*math.cos(a1), yb, cz+rad*math.sin(a1)); p01 = (cx+rad*math.cos(a0), yb, cz+rad*math.sin(a0))
            uv = lambda p, s=s: (s / sides * 4.0, p[1])
            M.tri(mat, p00, p10, p11, n, uv); M.tri(mat, p00, p11, p01, n, uv)

# ---------------------------------------------------------------- le niveau
ROOMS = [
 dict(name='A_spawn', x0=-4, x1=4, y0=0, y1=4, z0=-4, z1=4, cell=4.0, mats=('M_Floor','M_Ceil','M_Wall_A'),
      open=dict(N=[(-2,2,0,3)])),
 dict(name='B_hall', x0=-6, x1=6, y0=0, y1=5, z0=-20, z1=-4, cell=1.0, mats=('M_Floor','M_Ceil','M_Wall_B'), fan=True,
      open=dict(S=[(-2,2,0,3)], N=[(-3.5,3.5,0,3.5)], E=[(-12,-9,0,2.6)], W=[(-14,-10,1,2.5)])),
 dict(name='C_gallery', x0=-5, x1=5, y0=0, y1=4, z0=-50, z1=-20, cell=0.5, mats=('M_Floor','M_Ceil','M_Wall_C'),
      open=dict(S=[(-3.5,3.5,0,3.5)], N=[(-4.5,4.5,0,3.5)])),
 dict(name='D_far', x0=-12, x1=12, y0=0, y1=8, z0=-90, z1=-50, cell=0.4, mats=('M_Floor','M_Ceil','M_Wall_D'),
      open=dict(S=[(-4.5,4.5,0,3.5)])),
 dict(name='E_side', x0=6, x1=14, y0=0, y1=4, z0=-16, z1=-6, cell=0.5, mats=('M_Floor','M_Ceil','M_Wall_E'),
      open=dict(W=[(-12,-9,0,2.6)])),
 dict(name='F_annex', x0=-14, x1=-6, y0=0, y1=4, z0=-18, z1=-8, cell=1.0, mats=('M_Floor','M_Ceil','M_Wall_F'),
      open=dict(E=[(-14,-10,1,2.5)])),
]
CAMERAS = [   # nom, categorie, priorite, position, lacet (degres)
 ('Bench_Cam_Game', 'gameplay', 10, (0.0, 1.7,   3.0),   0.0),
 ('Bench_Cam_Dbg1', 'debug',     3, (3.0, 1.7,   2.5),   0.0),
 ('Bench_Cam_Dbg2', 'debug',     2, (0.0, 1.7, -24.0),   0.0),
 ('Bench_Cam_Dbg3', 'debug',     1, (0.0, 1.7, -55.0), 180.0),
]
MATS = {  # nom : (Kd, Ns)
 'M_Floor': ((0.36,0.34,0.31), 6), 'M_Ceil': ((0.78,0.78,0.80), 2),
 'M_Wall_A': ((0.80,0.45,0.40), 8), 'M_Wall_B': ((0.45,0.65,0.80), 8), 'M_Wall_C': ((0.55,0.75,0.50), 8),
 'M_Wall_D': ((0.80,0.72,0.40), 8), 'M_Wall_E': ((0.65,0.50,0.80), 8), 'M_Wall_F': ((0.50,0.78,0.72), 8),
 'M_Pillar': ((0.62,0.62,0.66), 24), 'M_Crate': ((0.58,0.40,0.22), 4),
}

def build(order='near-far'):
    M = Mesh(); R = lcg(20261029)
    by_name = {r['name']: r for r in ROOMS}
    seq = ROOMS if order == 'near-far' else list(reversed(ROOMS))
    for r in seq:
        room(M, r)
        n = r['name']
        if n == 'A_spawn':
            for (x, z, s) in ((-2.6,-1.0,1.2), (2.4,-2.4,1.0), (-1.2,1.0,0.8)):
                box(M, 'M_Crate', x, s/2, z, s, s, s, 2)
        elif n == 'B_hall':
            for z in (-6.5, -9.5, -12.5, -15.5, -18.5):
                for x in (-3.8, 3.8): pillar(M, 'M_Pillar', x, z, 0.35, 16, 0, 5, 0.5)
            for (x, z) in ((-1.5,-7.0),(1.0,-8.0),(-0.5,-14.0),(1.8,-16.0),(-2.2,-17.5),(0.5,-11.0)):
                box(M, 'M_Crate', x, 0.5, z, 1.0, 1.0, 1.0, 2)
        elif n == 'C_gallery':
            for s in (-1, 1):
                for z in range(-47, -21, 3):                     # pilastres le long des murs lateraux
                    box(M, 'M_Pillar', s*4.85, 2.0, z, 0.3, 4.0, 0.6, 4, bottom=False, top=False)
        elif n == 'D_far':
            for z in range(-54, -88, -3):
                for x in (-8, 8): pillar(M, 'M_Pillar', x, z, 0.4, 12, 0, 8, 0.5)
            for _ in range(30):
                x = -10 + 20*next(R); z = -52 - 36*next(R); s = 0.4 + 0.5*next(R)
                box(M, 'M_Crate', x, s/2, z, s, s, s, 1)
        elif n == 'E_side':
            for (x, z) in ((8,-8),(10.5,-12),(12.5,-9),(9,-14.5)):
                box(M, 'M_Crate', x, 0.5, z, 1.0, 1.0, 1.0, 2)
    return M

def write_obj(M, path, mtlname):
    with open(path, 'w', newline='\n') as f:
        f.write(f"# {os.path.basename(path)} — niveau FPS de la scene V3 (gen_level_fps.py)\n")
        f.write(f"# {M.count()} triangles, {len(M.v)} positions, {len(M.t)} UV. Coques enroulees vers l'interieur.\n")
        f.write(f"mtllib {mtlname}\n")
        for p in M.v: f.write("v %.4f %.4f %.4f\n" % p)
        for t in M.t: f.write("vt %.4f %.4f\n" % t)
        for name, tris in M.groups:
            f.write(f"g {name}\n"); last = None
            for mat, vt in tris:
                if mat != last: f.write(f"usemtl {mat}\n"); last = mat
                f.write("f %d/%d %d/%d %d/%d\n" % (vt[0][0], vt[0][1], vt[1][0], vt[1][1], vt[2][0], vt[2][1]))

def write_mtl(path):
    with open(path, 'w', newline='\n') as f:
        f.write("# level_fps.mtl — materiaux du niveau V3. Aucune texture : le banc mesure la geometrie.\n")
        for name, (kd, ns) in MATS.items():
            f.write(f"newmtl {name}\nKa 0.02 0.02 0.02\nKd {kd[0]:.2f} {kd[1]:.2f} {kd[2]:.2f}\nKs 0.05 0.05 0.05\nNs {ns}\n\n")

def cam_node(name, cat, prio, pos, yaw):
    return {"id": name, "parent": None, "_type": "Camera", "components": {
        "Transform": {"translation": list(pos), "rotation": [0, yaw, 0], "scale": [1.0, 1.0, 1.0]},
        "Camera": {"projection": "perspective", "lens": "fov", "fov": 60.0, "near": 0.1, "infiniteFar": True,
                   "active": True, "priority": prio, "category": cat, "gizmo": {"length": 3.0},
                   "depthDisplayRange": 150, "lodTolerancePx": None}}}

def write_scene(path, objpaths, title):
    nodes = [cam_node(*c) for c in CAMERAS]
    for nid, objpath in objpaths:
        nodes.append({"id": nid, "parent": None, "_type": "Bench", "components": {
            "Transform": {"translation": [0, 0, 0], "rotation": [0, 0, 0], "scale": [1.0, 1.0, 1.0]},
            "Mesh": {"model": objpath, "orbitalSpeed": 0.0, "rotationSpeed": 0.0}}})
    doc = {"sceneName": title, "nodes": nodes}
    # tableaux de nombres sur une ligne (format des scenes du depot)
    txt = json.dumps(doc, indent=2, ensure_ascii=False)
    import re
    txt = re.sub(r'\[\s+([^\[\]{}]*?)\s+\]', lambda m: '[ ' + ' '.join(m.group(1).split()) + ' ]', txt)
    open(path, 'w', encoding='utf-8', newline='\n').write(txt + "\n")

if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('--out', default='.'); ap.add_argument('--order', default='near-far', choices=['near-far', 'far-near'])
    ap.add_argument('--name', default='level_fps'); ap.add_argument('--shell', default='inward', choices=['inward', 'outward'])
    a = ap.parse_args(); os.makedirs(a.out, exist_ok=True)
    if a.shell == 'outward': SHELL = -1
    M = build(a.order)
    write_obj(M, os.path.join(a.out, a.name + '.obj'), 'level_fps.mtl')
    write_mtl(os.path.join(a.out, 'level_fps.mtl'))
    write_scene(os.path.join(a.out, 'bench_V_V3.json'), [("Level_FPS", f'assets/Meshes/{a.name}.obj')],
                "Banc V - V3 : niveau FPS, 1 mesh, 6 pieces, 6 portails (mix de tailles)")
    # variante : UNE PIECE = UN MESH (meme geometrie, memes cameras) -> mesure du culling par objet
    pieces = []
    for g in M.groups:
        sub = Mesh(); sub.group(g[0]); sub.v, sub.vi, sub.t, sub.ti = [], {}, [], {}
        for mat, vt in g[1]:
            sub.cur[1].append((mat, [(sub._v(M.v[v-1]), sub._t(M.t[t-1])) for v, t in vt]))
        fn = f"{a.name}_{g[0][0]}.obj"; write_obj(sub, os.path.join(a.out, fn), 'level_fps.mtl')
        pieces.append(("Room_" + g[0], f"assets/Meshes/{fn}"))
    write_scene(os.path.join(a.out, 'bench_V_V3_rooms.json'), pieces,
                "Banc V - V3 rooms : le meme niveau, une piece = un mesh (culling par objet)")
    meta = {"triangles": M.count(), "positions": len(M.v), "uvs": len(M.t), "order": a.order,
            "rooms": ROOMS, "cameras": [dict(name=c[0], category=c[1], priority=c[2], pos=c[3], yaw=c[4]) for c in CAMERAS],
            "per_group": {g[0]: len(g[1]) for g in M.groups}}
    json.dump(meta, open(os.path.join(a.out, a.name + '.meta.json'), 'w'), indent=1)
    print(M.count(), 'triangles', len(M.v), 'positions', len(M.t), 'uv'); print(meta["per_group"])
