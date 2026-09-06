#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
gen_ship.py — genere assets/Meshes/ship_scout.obj (+ .mtl)

Convention LibraryV3 : main droite, Y up, AVANT = -Z.
Le vaisseau est donc modelise nez vers -Z, ailes selon X, dessus vers +Y.

Depliage UV : chaque piece est depliee par sa propre projection naturelle
(cylindrique pour les corps de revolution, planaire pour les ailes), dans une
bande horizontale distincte de l'atlas -> pas de recouvrement entre groupes,
et un atlas 1024x1024 suffit.

  v in [0.00, 0.34] : coque (hull)
  v in [0.36, 0.58] : ailes (wings)
  v in [0.60, 0.80] : moteurs (engine)
  v in [0.82, 1.00] : verriere (glass)
"""
import math

V, VT, VN, F = [], [], [], []          # positions, uv, normales, faces (groupees)
GROUPS = []                            # (nom_groupe, materiau, index_debut_face)

def vtx(p):  V.append(p);  return len(V)
def uv(t):   VT.append(t); return len(VT)
def nrm(n):
    l = math.sqrt(sum(c*c for c in n)) or 1.0
    VN.append((n[0]/l, n[1]/l, n[2]/l)); return len(VN)

def tri(a, b, c): F.append((a, b, c))
def group(name, mtl): GROUPS.append((name, mtl, len(F)))

def ring(z, r, n, ry=None):
    """Anneau de n sommets dans le plan XY a la cote z. Retourne les indices."""
    ry = r if ry is None else ry
    out = []
    for i in range(n):
        a = 2.0 * math.pi * i / n
        out.append((r * math.cos(a), ry * math.sin(a), z, a))
    return out

def tube(z0, r0, z1, r1, n, v0, v1, ry0=None, ry1=None, cap_start=False, cap_end=False):
    """Tronc de cone a n cotes entre z0 et z1. UV cylindrique : u = angle, v = bande."""
    A = ring(z0, r0, n, ry0)
    B = ring(z1, r1, n, ry1)
    ia = [vtx((p[0], p[1], p[2])) for p in A]
    ib = [vtx((p[0], p[1], p[2])) for p in B]
    # duplication du sommet u=0/u=1 : une couture propre, pas de triangle qui
    # traverse tout l'atlas (contre-exemple classique du depliage cylindrique)
    ta = [uv((i / n, v0)) for i in range(n + 1)]
    tb = [uv((i / n, v1)) for i in range(n + 1)]
    na = [nrm((p[0], p[1], (r0 - r1) * 0.5)) for p in A]
    nb = [nrm((p[0], p[1], (r0 - r1) * 0.5)) for p in B]
    for i in range(n):
        j = (i + 1) % n
        tri((ia[i], ta[i], na[i]), (ib[i], tb[i], nb[i]),     (ib[j], tb[i+1], nb[j]))
        tri((ia[i], ta[i], na[i]), (ib[j], tb[i+1], nb[j]),   (ia[j], ta[i+1], na[j]))
    if cap_start: fan(A, z0, -1, (v0 + v1) * 0.5)
    if cap_end:   fan(B, z1, +1, (v0 + v1) * 0.5)
    return ia, ib

def fan(R, z, sign, vband):
    """Bouchon en eventail. UV en disque autour de (0.5, vband)."""
    n = len(R)
    ic = vtx((0.0, 0.0, z)); tc = uv((0.5, vband)); nc = nrm((0.0, 0.0, float(sign)))
    idx = [(vtx((p[0], p[1], p[2])),
            uv((0.5 + 0.06 * math.cos(p[3]), vband + 0.06 * math.sin(p[3]))),
            nrm((0.0, 0.0, float(sign)))) for p in R]
    for i in range(n):
        j = (i + 1) % n
        a, b = idx[i], idx[j]
        if sign > 0: tri((ic, tc, nc), a, b)
        else:        tri((ic, tc, nc), b, a)

def slab(pts_top, thickness, n_start, u0, u1, v0, v1):
    """
    Plaque epaisse (aile / derive) definie par un polygone convexe donne dans
    l'ordre trigonometrique, extrude de +-thickness/2 selon Y.
    UV planaire : le polygone est projete sur son AABB puis remis dans [u0,u1]x[v0,v1].
    """
    xs = [p[0] for p in pts_top]; zs = [p[1] for p in pts_top]
    x0, x1 = min(xs), max(xs); z0, z1 = min(zs), max(zs)
    def puv(p):
        s = (p[0] - x0) / max(x1 - x0, 1e-6); t = (p[1] - z0) / max(z1 - z0, 1e-6)
        return uv((u0 + s * (u1 - u0), v0 + t * (v1 - v0)))
    h = thickness * 0.5
    up, dn = nrm((0, 1, 0)), nrm((0, -1, 0))
    T = [(vtx((p[0],  h, p[1])), puv(p), up) for p in pts_top]
    B = [(vtx((p[0], -h, p[1])), puv(p), dn) for p in pts_top]
    m = len(pts_top)
    for i in range(1, m - 1):                       # dessus
        tri(T[0], T[i], T[i + 1])
    for i in range(1, m - 1):                       # dessous
        tri(B[0], B[i + 1], B[i])
    for i in range(m):                              # tranche
        j = (i + 1) % m
        e = nrm((pts_top[j][1] - pts_top[i][1], 0.0, -(pts_top[j][0] - pts_top[i][0])))
        a1 = (T[i][0], T[i][1], e); a2 = (T[j][0], T[j][1], e)
        b1 = (B[i][0], B[i][1], e); b2 = (B[j][0], B[j][1], e)
        tri(a1, b1, b2); tri(a1, b2, a2)

def fin(pts_zy, thickness, u0, u1, v0, v1):
    """Derive verticale : polygone donne dans le plan (Z,Y), extrude de +-t/2 selon X."""
    zs = [p[0] for p in pts_zy]; ys = [p[1] for p in pts_zy]
    z0, z1 = min(zs), max(zs); y0, y1 = min(ys), max(ys)
    def puv(p):
        s_ = (p[0]-z0)/max(z1-z0,1e-6); t_ = (p[1]-y0)/max(y1-y0,1e-6)
        return uv((u0 + s_*(u1-u0), v0 + t_*(v1-v0)))
    h = thickness*0.5
    rt, lf = nrm((1,0,0)), nrm((-1,0,0))
    R = [(vtx(( h, p[1], p[0])), puv(p), rt) for p in pts_zy]
    L = [(vtx((-h, p[1], p[0])), puv(p), lf) for p in pts_zy]
    m = len(pts_zy)
    for i in range(1, m-1): tri(R[0], R[i+1], R[i])
    for i in range(1, m-1): tri(L[0], L[i], L[i+1])
    for i in range(m):
        j = (i+1) % m
        e = nrm((0.0, pts_zy[j][0]-pts_zy[i][0], -(pts_zy[j][1]-pts_zy[i][1])))
        tri((R[i][0],R[i][1],e), (L[i][0],L[i][1],e), (L[j][0],L[j][1],e))
        tri((R[i][0],R[i][1],e), (L[j][0],L[j][1],e), (R[j][0],R[j][1],e))

# =============================================================================
#  CONSTRUCTION — le vaisseau tient dans une boite ~ 2.0 x 0.7 x 3.6 (X,Y,Z)
# =============================================================================
N = 10                                   # cotes du fuselage

group("hull", "M_Hull")
tube(-1.80, 0.020, -1.20, 0.170, N, 0.02, 0.10)          # pointe du nez
tube(-1.20, 0.170, -0.55, 0.300, N, 0.10, 0.18)          # ogive
tube(-0.55, 0.300,  0.85, 0.320, N, 0.18, 0.28)          # fuselage
tube( 0.85, 0.320,  1.25, 0.230, N, 0.28, 0.33, cap_end=True)   # retreint arriere

group("wings", "M_Hull")
# aile babord : polygone (x, z) vu de dessus, sens trigo
slab([(-0.28, -0.10), (-1.05, 0.55), (-1.00, 1.10), (-0.28, 0.95)], 0.075, N,
     0.02, 0.48, 0.36, 0.57)
slab([( 0.28, -0.10), ( 0.28, 0.95), ( 1.00, 1.10), ( 1.05, 0.55)], 0.075, N,
     0.52, 0.98, 0.36, 0.57)
# derive dorsale : plaque VERTICALE, definie dans le plan (Z,Y), extrudee selon X
fin([(0.30, 0.18), (1.22, 0.62), (1.22, 0.10), (0.36, 0.05)], 0.085,
    0.02, 0.20, 0.36, 0.57)

group("engine", "M_Engine")
for sx in (-0.62, 0.62):
    b = len(V)
    tube(0.30, 0.150, 1.05, 0.165, N, 0.62, 0.72, cap_start=True)   # nacelle
    tube(1.05, 0.165, 1.32, 0.125, N, 0.72, 0.78)                   # tuyere
    for i in range(b, len(V)):
        x, y, z = V[i]; V[i] = (x + sx, y - 0.02, z)

group("thruster", "M_Thruster")
for sx in (-0.62, 0.62):
    b = len(V)
    fan(ring(1.325, 0.120, N), 1.325, +1, 0.90)                     # disque emissif
    for i in range(b, len(V)):
        x, y, z = V[i]; V[i] = (x + sx, y - 0.02, z)

group("glass", "M_Glass")
_b = len(V)
tube(-0.60, 0.090, -0.20, 0.190, N, 0.84, 0.92, ry0=0.05, ry1=0.11)
tube(-0.20, 0.190,  0.15, 0.120, N, 0.92, 0.99, ry0=0.11, ry1=0.07, cap_end=True)
for i in range(_b, len(V)):
    x, y, z = V[i]; V[i] = (x, y + 0.26, z)

# =============================================================================
#  ECRITURE
# =============================================================================
def fix_winding():
    """Passe finale : aligne le sens de parcours de chaque triangle sur les
    normales declarees. Les helpers (tube/fan/slab/fin) produisent des normales
    justes par construction, mais des ordres de parcours heterogenes ; sans
    cette passe, le backface culling en eliminerait un sur deux, et l'eclairage
    verrait des faces retournees. On corrige la donnee a la source plutot que
    de desactiver le culling dans le moteur."""
    fixed = 0
    for k, f in enumerate(F):
        A, B, C = [V[i[0] - 1] for i in f]
        u = [B[i] - A[i] for i in range(3)]
        v = [C[i] - A[i] for i in range(3)]
        n = (u[1]*v[2] - u[2]*v[1], u[2]*v[0] - u[0]*v[2], u[0]*v[1] - u[1]*v[0])
        if sum(x*x for x in n) < 1e-14: continue
        ref = [sum(VN[i[2] - 1][j] for i in f) / 3.0 for j in range(3)]
        if sum(n[j] * ref[j] for j in range(3)) < 0.0:
            F[k] = (f[0], f[2], f[1]); fixed += 1
    return fixed


def write_obj(path):
    L = ["# ship_scout.obj — LibraryV3 / vaisseau du joueur",
         "# main droite, Y up, AVANT = -Z. Emprise ~ 2.1 x 0.9 x 3.2 unites.",
         "mtllib ship_scout.mtl", ""]
    for p in V:  L.append("v %.5f %.5f %.5f" % p)
    for t in VT: L.append("vt %.5f %.5f" % t)
    for n in VN: L.append("vn %.5f %.5f %.5f" % n)
    L.append("")
    bounds = GROUPS + [(None, None, len(F))]
    for gi in range(len(GROUPS)):
        name, mtl, start = bounds[gi]; end = bounds[gi + 1][2]
        L.append(f"g {name}"); L.append(f"usemtl {mtl}")
        for f in F[start:end]:
            L.append("f " + " ".join("%d/%d/%d" % v for v in f))
        L.append("")
    open(path, "w", encoding="utf-8").write("\n".join(L))

def write_mtl(path):
    open(path, "w", encoding="utf-8").write("""# ship_scout.mtl
newmtl M_Hull
Ka 0.10 0.10 0.11
Kd 0.72 0.74 0.78
Ks 0.45 0.46 0.48
Ns 48
d  1.0
map_Kd assets/Textures/ship_scout_albedo_1k.png
map_Ks assets/Textures/ship_scout_spec_1k.png
map_Bump assets/Textures/ship_scout_normal_1k.png

newmtl M_Engine
Ka 0.08 0.08 0.09
Kd 0.38 0.39 0.42
Ks 0.60 0.60 0.62
Ns 72
map_Kd assets/Textures/ship_scout_albedo_1k.png
map_Bump assets/Textures/ship_scout_normal_1k.png

newmtl M_Thruster
Ka 0.00 0.00 0.00
Kd 0.10 0.35 0.85
Ks 0.00 0.00 0.00
Ke 0.30 0.70 1.00
Ns 1
map_Ke assets/Textures/ship_scout_emissive_1k.png

newmtl M_Glass
Ka 0.02 0.03 0.04
Kd 0.10 0.16 0.22
Ks 0.90 0.92 0.95
Ns 160
d  0.45
""")

print("winding corrige sur %d triangles" % fix_winding())
write_obj("ship_scout.obj")
write_mtl("ship_scout.mtl")
print("sommets %d | uv %d | normales %d | triangles %d | groupes %d"
      % (len(V), len(VT), len(VN), len(F), len(GROUPS)))
bx = [min(p[i] for p in V) for i in range(3)]
Bx = [max(p[i] for p in V) for i in range(3)]
print("AABB min (%.3f %.3f %.3f)  max (%.3f %.3f %.3f)" % (*bx, *Bx))
