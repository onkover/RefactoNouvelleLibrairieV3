#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
gen_rock_lod.py — chaines de LOD des rochers, pour LibraryV3 (phase G, chantier 1).

Complete gen_meshes.py, qu'il IMPORTE : meme uv_sphere, meme rock(), memes graines.
L0 n'est PAS regenere : c'est rock_x.obj, deja produit par gen_meshes.py.

Pour chaque rocher :
  1. genere L1..L3 par RE-ECHANTILLONNAGE de la meme surface (annexe A13bis 3.1) :
     la deformation de rock() depend de la NORMALE, pas de l'indice du sommet,
     donc une grille plus grossiere retombe exactement sur la meme surface ;
  2. mesure eps_k par ecart radial (A13 4.2), directions aleatoires + directions
     CIBLEES sur les sommets des deux maillages (voir MESURE ci-dessous) ;
  3. ecrit rock_x.lod.json (descripteur de chaine, format lv3.lodchain v1).
     Les cles prefixees par "_" sont de la documentation : JsonReader ne les
     signale pas (convention de WarnUnread), le moteur ne les lit pas.

MESURE. 400 directions aleatoires (A13) SOUS-ESTIMENT eps : l'ecart maximal entre
deux surfaces lineaires par morceaux se trouve pres des SOMMETS, qu'un tirage
aleatoire rate. Contre-exemple mesure sur rock_a L1 : 0,235 avec 400 directions,
0,344 avec directions ciblees (+46 %). Le coefficient de securite x1,1 d'A13 ne
l'aurait pas couvert. On vise donc les sommets, et on verifie la convergence.

Dependance : numpy.
"""
import json, math
import numpy as np
from gen_meshes import rock, write_obj

ROCKS   = (("rock_a", 0xA5701), ("rock_b", 0xA5701 + 7919), ("rock_c", 0xA5701 + 2 * 7919))
LEVELS  = ((12, 8), (8, 5), (4, 3), (4, 2))   # (seg, rings) : L0 = celui de gen_meshes.py
SAFETY  = 1.1                                   # declare dans le descripteur (A13 4.3)
N_RAND  = 100_000

def ray_radius(V, F, D):
    """Distance du centre a la surface, pour chaque direction de D (Moller-Trumbore).
    V : sommets (liste de tuples), F : faces en indices OBJ (base 1), D : (n,3) unitaires."""
    V = np.asarray(V); T = np.array([[V[k - 1] for k in f] for f in F])
    a, e1, e2 = T[:, 0], T[:, 1] - T[:, 0], T[:, 2] - T[:, 0]
    best = np.full(len(D), np.inf)
    for i in range(len(F)):
        p = np.cross(D, e2[i]); det = p @ e1[i]
        ok = np.abs(det) > 1e-12; inv = np.where(ok, 1.0 / np.where(ok, det, 1.0), 0.0)
        s = -a[i]; u = (p @ s) * inv
        q = np.cross(s, e1[i]); v = (D @ q) * inv; t = (e2[i] @ q) * inv
        hit = ok & (u >= 0) & (u <= 1) & (v >= 0) & (u + v <= 1) & (t > 0)
        best = np.where(hit, np.minimum(best, t), best)
    assert np.all(np.isfinite(best)), "rayon sans intersection : mesh non etoile, ou trou"
    return best

def directions(meshes, seed):
    rng = np.random.default_rng(seed)
    d = rng.normal(size=(N_RAND, 3))
    v = np.array([p for m in meshes for p in m[0]])                     # sommets de TOUS les niveaux
    v = np.repeat(v, 3, axis=0) + rng.normal(scale=1e-4, size=(3 * len(v), 3))  # jamais pile sur une arete
    d = np.concatenate([d, v])
    return d / np.linalg.norm(d, axis=1, keepdims=True)

def ceil4(x): return math.ceil(x * 1e4) / 1e4        # arrondi VERS LE HAUT : jamais optimiste

if __name__ == "__main__":
    for name, seed in ROCKS:
        meshes = [rock(seed, s, rg) for s, rg in LEVELS]
        D  = directions(meshes, seed)
        r0 = ray_radius(meshes[0][0], meshes[0][3], D)
        levels = []
        for k, ((V, VT, VN, F), (s, rg)) in enumerate(zip(meshes, LEVELS)):
            path = f"{name}.obj" if k == 0 else f"{name}_L{k}.obj"
            if k > 0:
                write_obj(path, V, VT, VN, F,
                          f"{path} — niveau L{k} de {name}.obj : sphere {s}x{rg}, meme surface, meme graine",
                          "planet.mtl", "M_Rock")
            dev  = np.abs(ray_radius(V, F, D) - r0)
            eMax = float(dev.max()); eMoy = float(dev.mean())
            # Sommets COMPTES COMME LE MOTEUR : seuls les triplets v/vt/vn REFERENCES par une face.
            # uv_sphere ecrit (seg+1)(rings+1) sommets, mais un sommet de chaque pole n'est
            # utilise par aucune face : OBJLoader ne le cree pas (117 ecrits, 115 charges pour L0).
            used = len({i for f in F for i in f})
            levels.append({"mesh": path, "vertices": used, "faces": len(F),
                           "epsilon": ceil4(eMax * SAFETY),
                           "_measure": {"max": round(eMax, 4), "mean": round(eMoy, 4)}})
            print(f"{name:7s} L{k}  {used:3d} v  {len(F):3d} f  eps_max={eMax:.4f}  eps_moy={eMoy:.4f}")
        desc = {"format": "lv3.lodchain", "version": 1, "name": name, "units": "local",
                "_errorMetric": {"method": "radial", "randomDirections": N_RAND,
                                "vertexTargeted": True, "safety": SAFETY},
                "levels": levels}
        with open(f"{name}.lod.json", "w", encoding="utf-8", newline="\n") as fp:
            json.dump(desc, fp, indent=2, ensure_ascii=False); fp.write("\n")
