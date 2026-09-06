#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
gen_meshes.py — spheres LOD + rochers, pour LibraryV3.

Convention : main droite, Y up, avant = -Z. Rayon 1.0 (l'echelle vient du Transform).

Depliage equirectangulaire, avec les DEUX corrections qui manquent a une sphere
naive :

  1. COUTURE. On genere (seg+1) colonnes de sommets, pas seg. La derniere
     colonne occupe la meme POSITION que la premiere mais porte u = 1.0 au lieu
     de u = 0.0. Sans ce doublon, le triangle qui enjambe la longitude 180
     interpole u de 0.98 vers 0.02 et balaie toute la texture a l'envers : une
     bande verticale de pixels aberrants.

  2. POLES. Le pole est UN point en 3D mais un SEGMENT entier en UV : chaque
     triangle qui y aboutit a besoin de son propre u. On genere donc une rangee
     complete de sommets au pole, tous a la meme position, chacun avec son u.
     Le quad degenere correspondant ne produit qu'un seul triangle.

     Contre-exemple a ne pas faire : un seul sommet au pole avec u = 0.5. Tous
     les triangles polaires se partagent alors le meme u et la texture se plisse
     en eventail autour du pole.
"""
import math

def uv_sphere(seg, rings, radius=1.0, noise=None):
    """Grille (rings+1) x (seg+1). noise(i,j,x,y,z) -> facteur radial optionnel."""
    V, VT, VN, F = [], [], [], []
    for j in range(rings + 1):
        phi = math.pi * j / rings              # 0 = pole nord (+Y)
        sp, cp = math.sin(phi), math.cos(phi)
        for i in range(seg + 1):
            theta = 2.0 * math.pi * i / seg
            n = (sp * math.cos(theta), cp, sp * math.sin(theta))
            r = radius * (noise(i, j, n) if noise else 1.0)
            V.append((n[0] * r, n[1] * r, n[2] * r))
            VN.append(n)
            VT.append((i / seg, 1.0 - j / rings))
    def idx(i, j): return j * (seg + 1) + i + 1
    for j in range(rings):
        for i in range(seg):
            a, b = idx(i, j),     idx(i + 1, j)
            c, d = idx(i, j + 1), idx(i + 1, j + 1)
            # Aux poles le quad degenere : un seul de ses deux triangles a une
            # aire non nulle. Emettre l'autre serait un triangle d'aire zero,
            # que le rasterizer devra charger, transformer, puis rejeter sur un
            # EdgeFunction nul — du travail pur pour rien, et une division par
            # zero potentielle dans l'interpolation barycentrique.
            if j != rings - 1: F.append((a, d, c))
            if j != 0:         F.append((a, b, d))
    return V, VT, VN, F

def write_obj(path, V, VT, VN, F, header, mtl=None, usemtl=None):
    L = [f"# {header}", "# main droite, Y up, avant = -Z"]
    if mtl: L.append(f"mtllib {mtl}")
    L.append("")
    L += ["v %.6f %.6f %.6f" % p for p in V]
    L += ["vt %.6f %.6f" % t for t in VT]
    L += ["vn %.6f %.6f %.6f" % n for n in VN]
    L.append("")
    if usemtl: L.append(f"usemtl {usemtl}")
    L += ["f " + " ".join("%d/%d/%d" % (k, k, k) for k in f) for f in F]
    open(path, "w", encoding="utf-8").write("\n".join(L) + "\n")
    return len(V), len(F)

# --- LCG deterministe : meme rocher a chaque generation ----------------------
class LCG:
    def __init__(self, seed): self.s = seed & 0xFFFFFFFF
    def next(self):
        self.s = (1664525 * self.s + 1013904223) & 0xFFFFFFFF
        return self.s / 4294967296.0
    def rng(self, a, b): return a + (b - a) * self.next()

def rock(seed, seg=12, rings=8, amp=0.30):
    """Rocher : sphere basse resolution deformee par une somme d'harmoniques.
    Deterministe, et la deformation depend de la POSITION (pas de l'indice),
    donc la couture reste soudee : les colonnes 0 et seg ont la meme normale
    et recoivent donc exactement le meme deplacement."""
    r = LCG(seed)
    waves = [(r.rng(1, 4), r.rng(1, 4), r.rng(1, 4), r.rng(0, 6.28), r.rng(0.4, 1.0))
             for _ in range(4)]
    def noise(i, j, n):
        d = 0.0
        for (a, b, c, ph, w) in waves:
            d += w * math.sin(a * n[0] * 3.1 + b * n[1] * 3.1 + c * n[2] * 3.1 + ph)
        return 1.0 + amp * d / len(waves)
    return uv_sphere(seg, rings, 1.0, noise)

if __name__ == "__main__":
    for name, (s, rg) in {"sphere_lo": (16, 10), "sphere_mid": (32, 16),
                          "sphere_hi": (64, 32)}.items():
        V, VT, VN, F = uv_sphere(s, rg)
        nv, nf = write_obj(f"{name}.obj", V, VT, VN, F,
                           f"{name}.obj — sphere UV {s}x{rg}, rayon 1.0",
                           "planet.mtl", "M_Planet")
        print(f"{name:12s} {nv:5d} sommets  {nf:5d} triangles")

    for k, name in enumerate(("rock_a", "rock_b", "rock_c")):
        V, VT, VN, F = rock(seed=0xA5701 + k * 7919)
        nv, nf = write_obj(f"{name}.obj", V, VT, VN, F,
                           f"{name}.obj — asteroide procedural, rayon moyen 1.0",
                           "planet.mtl", "M_Rock")
        print(f"{name:12s} {nv:5d} sommets  {nf:5d} triangles")

    open("planet.mtl", "w", encoding="utf-8").write(
"""# planet.mtl — materiaux generiques ; la couleur reelle vient de Material dans la scene
newmtl M_Planet
Ka 0.02 0.02 0.02
Kd 0.80 0.80 0.80
Ks 0.05 0.05 0.05
Ns 8

newmtl M_Rock
Ka 0.02 0.02 0.02
Kd 0.45 0.42 0.39
Ks 0.02 0.02 0.02
Ns 4
""")
