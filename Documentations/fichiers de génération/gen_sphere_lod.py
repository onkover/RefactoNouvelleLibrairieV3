#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
gen_sphere_lod.py — chaines de LOD des spheres, pour LibraryV3 (phase G, etape 1).

Complete gen_meshes.py, qu'il IMPORTE : meme uv_sphere, meme write_obj.
sphere_lo / sphere_mid / sphere_hi ne sont PAS regeneres : deja produits par gen_meshes.py.

Decisions (4 octobre 2026) :
  * fabrication "resample" : la sphere vient d'une fonction, on re-echantillonne
    la MEME surface (exact). Le QEM est reserve aux meshes sans fonction generatrice.
  * pas de droit de monter : L0 = resolution choisie par l'auteur = plafond.
    D'ou TROIS descripteurs, qui partagent les memes .obj.

MESURE. Ecart radial |r_k - r_L0| (A13 4.2), 100 000 directions aleatoires
+ directions ciblees sur les sommets ET sur les centres de faces de tous les
niveaux. Sur une surface convexe lisse, le maximum est au centre des faces
(fleche), pas aux sommets comme sur les rochers. Verifie aussi analytiquement
(distance du centre au plan de chaque triangle) : accord a 1e-5.

Cles prefixees par '_' : informatives, ignorees par JsonReader::WarnUnread.
Dependance : numpy.
"""
import json, math
import numpy as np
from gen_meshes import uv_sphere, write_obj
from gen_rock_lod import ray_radius

GRIDS  = {"hi": (64, 32), "mid": (32, 16), "lo": (16, 10),
          "12x6": (12, 6), "8x5": (8, 5), "6x4": (6, 4)}
FILES  = {"hi": "sphere_hi.obj", "mid": "sphere_mid.obj", "lo": "sphere_lo.obj",
          "12x6": "sphere_12x6.obj", "8x5": "sphere_8x5.obj", "6x4": "sphere_6x4.obj"}
CHAINS = {"sphere_hi":  ("hi", "mid", "lo", "8x5"),
          "sphere_mid": ("mid", "lo", "12x6", "8x5"),
          "sphere_lo":  ("lo", "12x6", "8x5", "6x4")}
SAFETY = 1.1
N_RAND = 100_000
SEED   = 1234

def ceil4(x): return math.ceil(x * 1e4) / 1e4        # arrondi VERS LE HAUT : jamais optimiste

def loaded_vertex_count(F):
    """Ce que rend MeshClass::vertexCount() : triplets v/vt/vn REFERENCES et distincts
    (OBJLoader deduplique ; ici v = vt = vn). Les sommets non references ne comptent pas."""
    return len({k for f in F for k in f})

if __name__ == "__main__":
    M = {k: uv_sphere(*g) for k, g in GRIDS.items()}
    for k in ("12x6", "8x5", "6x4"):
        V, VT, VN, F = M[k]; s, rg = GRIDS[k]
        write_obj(FILES[k], V, VT, VN, F,
                  f"{FILES[k]} — sphere UV {s}x{rg}, rayon 1.0 (niveau de LOD, gen_sphere_lod.py)",
                  "planet.mtl", "M_Planet")

    rng = np.random.default_rng(SEED)
    def unit(d): return d / np.linalg.norm(d, axis=1, keepdims=True)
    def jitter(P): return np.repeat(P, 3, 0) + rng.normal(scale=1e-4, size=(3 * len(P), 3))
    verts = np.array([p for k in M for p in M[k][0]])
    cents = np.array([np.asarray(M[k][0])[np.array(f) - 1].mean(0) for k in M for f in M[k][3]])
    D = unit(np.concatenate([rng.normal(size=(N_RAND, 3)), jitter(verts), jitter(cents)]))
    R = {k: ray_radius(M[k][0], M[k][3], D) for k in M}

    for name, levels in CHAINS.items():
        out = []
        for i, k in enumerate(levels):
            dev = np.abs(R[k] - R[levels[0]])
            eMax, eMoy = float(dev.max()), float(dev.mean())
            out.append({"mesh": FILES[k], "vertices": loaded_vertex_count(M[k][3]),
                        "faces": len(M[k][3]),
                        "epsilon": 0.0 if i == 0 else ceil4(eMax * SAFETY),
                        "_measure": {"max": round(eMax, 4), "mean": round(eMoy, 4),
                                     "sagVsExactSphere": round(float(np.abs(R[k] - 1).max()), 4)}})
            print(f"{name:11s} L{i} {k:5s} {out[-1]['faces']:5d} f  eps={out[-1]['epsilon']:.4f}")
        desc = {"format": "lv3.lodchain", "version": 1, "name": name, "units": "local",
                "_fabrication": "resample",
                "_errorMetric": {"method": "radial", "randomDirections": N_RAND,
                                 "vertexTargeted": True, "faceCenterTargeted": True,
                                 "safety": SAFETY},
                "levels": out}
        with open(f"{name}.lod.json", "w", encoding="utf-8", newline="\n") as fp:
            json.dump(desc, fp, indent=2, ensure_ascii=False); fp.write("\n")
