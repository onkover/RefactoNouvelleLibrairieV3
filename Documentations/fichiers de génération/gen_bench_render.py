#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
gen_bench_render.py — scenes synthetiques de la campagne R (phase G, etape 4).

But : chiffrer le COUT FIXE de Render en isolant UN facteur par scene (regle M4).

    Render ~ k  +  a * (meshes testes)  +  b * (meshes dessines)  +  c * (faces soumises)  [+ pixels]

    S0        : 4 cameras, aucun mesh                       -> k  (vues + gizmos)
    S1_N      : N rochers DERRIERE les cameras              -> a  (mesh teste puis rejete)
    S2_N      : N rochers (168 faces) visibles, sous-pixel  -> b + 168 c
    S3_N      : N cubes   ( 12 faces) visibles, sous-pixel  -> b +  12 c
    N = 300 et 1200 : la PENTE entre les deux donne le cout unitaire.

Les 4 cameras sont IDENTIQUES (meme pose, meme objectif) : chaque vue voit
exactement les memes objets, le nombre de vues ne fait que multiplier.
Rien ne bouge (vitesses nulles, aucun controleur) : les compteurs sont
constants d'une frame a l'autre, seul le temps varie.

Geometrie : cameras a l'origine, regard -Z, fov 60.
Objets sur une grille a D = 200 unites, dans un cone de +/-10 degres
(visible dans les 4 viewports, y compris le 400x800 le plus etroit).
Taille projetee : diametre 0.1 a 200 unites -> ~0.35 px dans une vue de
800 px de haut : SOUS-PIXEL, la rasterisation est quasi nulle par construction.

Format : toute cle lue est ecrite (regle Onky du 7 octobre 2026), D1/D2.
Campagne V (etape 4b, vectorisation) : deux scenes ou la FACE domine, PAS le pixel
ni le point (rayon apparent au-dessus du seuil pointRadiusPx dans les 4 vues) :
    V1 : 32 sphere_hi (3968 faces) a ~7 px de rayon (~0,9 px dans les vues titre)
         -> ~0,5 M faces/frame, presque aucun pixel : le cout PAR FACE domine.
    V2 : 1 sphere_hi en gros plan (rayon ~170 px) -> garde-fou : le cout PIXEL domine,
         la vectorisation ne doit pas le degrader.
Le .obj est charge directement (pas de .lod.json) : les 3968 faces sont toujours soumises.

Usage : python gen_bench_render.py   (ecrit bench_R_*.json, bench_V_*.json, bench_R.bat, bench_V.bat)
"""
import json, math

D       = 200.0                    # distance du plan des objets
HALF    = math.tan(math.radians(10.0)) * D
ROCK    = ("assets/Meshes/rock_a.obj", 0.05)    # rayon 1    -> diametre 0.1
CUBE    = ("assets/Meshes/cube.obj",   0.004)   # cote 25    -> cote 0.1

def T(pos=(0, 0, 0), scale=1.0):
    return {"translation": list(pos), "rotation": [0, 0, 0], "scale": [scale] * 3}

def camera(cid, category, priority):
    return {"id": cid, "parent": None, "_type": "Camera",
            "components": {
                "Transform": T(),
                "Camera": {"projection": "perspective", "lens": "fov", "fov": 60.0,
                           "near": 0.1, "infiniteFar": True,
                           "active": True, "priority": priority, "category": category,
                           "gizmo": {"length": 3.0},
                           "depthDisplayRange": 150, "lodTolerancePx": None}}}

CAMERAS = [camera("Bench_Cam_Game", "gameplay", 10),
           camera("Bench_Cam_Dbg1", "debug", 3),
           camera("Bench_Cam_Dbg2", "debug", 2),
           camera("Bench_Cam_Dbg3", "debug", 1)]

def grid(n, z):
    """n positions sur une grille ~4:3 dans le carre +/-HALF, au plan z."""
    cols = math.ceil(math.sqrt(n * 4 / 3)); rows = math.ceil(n / cols)
    out = []
    for i in range(n):
        cx, cy = i % cols, i // cols
        x = -HALF + (cx + 0.5) * (2 * HALF / cols)
        y = -HALF + (cy + 0.5) * (2 * HALF / rows)
        out.append((round(x, 4), round(y, 4), z))
    return out

def objects(prefix, mesh, n, z):
    model, s = mesh
    return [{"id": f"{prefix}_{i:04d}", "parent": None, "_type": "Bench",
             "components": {"Transform": T(p, s),
                            "Mesh": {"model": model, "orbitalSpeed": 0.0, "rotationSpeed": 0.0}}}
            for i, p in enumerate(grid(n, z))]

# --- ecriture lisible (meme format que gen_solar_system.py) ------------------
def _is_scalar(v): return not isinstance(v, (dict, list))
def dump(v, ind=0):
    sp = "  " * ind
    if isinstance(v, list):
        if all(_is_scalar(x) for x in v):
            return "[ " + ", ".join(json.dumps(x, ensure_ascii=False) for x in v) + " ]" if v else "[]"
        return "[\n" + ",\n".join(sp + "  " + dump(x, ind + 1) for x in v) + "\n" + sp + "]"
    if isinstance(v, dict):
        if not v: return "{}"
        if len(v) <= 2 and all(_is_scalar(x) for x in v.values()):
            return "{ " + ", ".join(json.dumps(k, ensure_ascii=False) + ": " + json.dumps(x, ensure_ascii=False)
                                    for k, x in v.items()) + " }"
        return "{\n" + ",\n".join(sp + "  " + json.dumps(k, ensure_ascii=False) + ": " + dump(x, ind + 1)
                                   for k, x in v.items()) + "\n" + sp + "}"
    return json.dumps(v, ensure_ascii=False)

SCENES = {"S0":      ("Banc R - S0 : cameras seules",                     []),
          "S1_300":  ("Banc R - S1 : 300 rochers derriere",              objects("Rock", ROCK,  300, +D)),
          "S1_1200": ("Banc R - S1 : 1200 rochers derriere",             objects("Rock", ROCK, 1200, +D)),
          "S2_300":  ("Banc R - S2 : 300 rochers sous-pixel (168 f)",    objects("Rock", ROCK,  300, -D)),
          "S2_1200": ("Banc R - S2 : 1200 rochers sous-pixel (168 f)",   objects("Rock", ROCK, 1200, -D)),
          "S3_300":  ("Banc R - S3 : 300 cubes sous-pixel (12 f)",       objects("Cube", CUBE,  300, -D)),
          "S3_1200": ("Banc R - S3 : 1200 cubes sous-pixel (12 f)",      objects("Cube", CUBE, 1200, -D))}

SPHERE = ("assets/Meshes/sphere_hi.obj", 2.0)
VSCENES = {"V1": ("Banc V - V1 : 32 spheres hi, cout par face", objects("Sphere", SPHERE, 32, -D)),
           "V2": ("Banc V - V2 : 1 sphere hi en gros plan, cout pixel",
                  [{"id": "Sphere_Big", "parent": None, "_type": "Bench",
                    "components": {"Transform": T((0, 0, -10.0), 2.5),
                                   "Mesh": {"model": "assets/Meshes/sphere_hi.obj",
                                            "orbitalSpeed": 0.0, "rotationSpeed": 0.0}}}])}

def write_bat(name, keys, prefix, header):
    order = list(keys) + list(reversed(keys))
    lines = ["@echo off", f"rem {header}",
             "rem A lancer depuis le dossier de l'exe (x64\\Release).",
             "set EXE=RefactoNouvelleLibrairieV3.exe",
             f"if not exist Mesures\\{prefix} mkdir Mesures\\{prefix}",
             f'start "" /wait /affinity 4 "%EXE%" --scene=bench_{prefix}_{keys[0]}.json --bench=300 --csv=Mesures/{prefix}/jetable.csv']
    seen = {}
    for key in order:
        seen[key] = seen.get(key, 0) + 1
        lines.append(f'start "" /wait /affinity 4 "%EXE%" --scene=bench_{prefix}_{key}.json --bench=300 '
                     f'--csv=Mesures/{prefix}/{prefix}_{key}_{"ab"[seen[key]-1]}.csv')
    open(name, "w", encoding="utf-8", newline="\r\n").write("\r\n".join(lines) + "\r\n")
    print(name, ":", len(order) + 1, "runs")

if __name__ == "__main__":
    for key, (name, objs) in SCENES.items():
        with open(f"bench_R_{key}.json", "w", encoding="utf-8", newline="\n") as f:
            f.write(dump({"sceneName": name, "nodes": CAMERAS + objs}) + "\n")
        print(f"bench_R_{key}.json : {len(objs)} objets")

    write_bat("bench_R.bat", list(SCENES), "R",
              "Campagne R (phase G, etape 4) : cout fixe de Render. Release, P-core (affinity 4).")

    for key, (name, objs) in VSCENES.items():
        with open(f"bench_V_{key}.json", "w", encoding="utf-8", newline="\n") as f:
            f.write(dump({"sceneName": name, "nodes": CAMERAS + objs}) + "\n")
        print(f"bench_V_{key}.json : {len(objs)} objets")
    write_bat("bench_V.bat", list(VSCENES), "V",
              "Campagne V (phase G, etape 4b) : cout par face avant/apres vectorisation. Release, P-core.")
