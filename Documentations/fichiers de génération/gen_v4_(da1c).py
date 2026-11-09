# gen_v4.py — scène V4 « petits triangles » (lot D-a1c). Fichiers générés fournis tels quels.
# Un mur de 6 x 3 dalles d'un mètre, face à la caméra à 4 m. Chaque dalle est une grille de N x N
# cellules (2 triangles par cellule), tournée de 7° dans son plan pour qu'aucune arête ne tombe
# sur la grille des pixels. Taille d'une cellule à l'écran : f * (1/N) / 4, f = 389,71 px.
import json, math, os
OUT = os.path.dirname(os.path.abspath(__file__))
F = 225.0 / math.tan(math.radians(30.0))          # 389,71 px (vue quad 768 x 450, fov vertical 60°)
D = 4.0                                            # distance caméra -> mur (m)
TH = math.radians(7.0)
CLASSES = {"c3": 32, "c5": 20, "c7": 14, "c12": 8}  # nom -> N (cellules par côté)
PITCH = 1.15                                       # pas entre dalles (dalle tournée : 1,115 m d'emprise)
COLS, ROWS = 6, 3

def write_mesh(name, N):
    cs, sn = math.cos(TH), math.sin(TH)
    cell_px = F / N / D
    path = os.path.join(OUT, "Meshes", f"v4_wall_{name}.obj")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", newline="\n") as f:
        f.write(f"# v4_wall_{name}.obj — dalle de la scene V4 (gen_v4.py, lot D-a1c)\n")
        f.write(f"# grille {N} x {N}, {2*N*N} triangles, {(N+1)**2} positions ; cote 1 m, tournee de 7 deg dans le plan z = 0\n")
        f.write(f"# a 4 m : cellule {cell_px:.2f} px, etendue d'un triangle {cell_px*(cs+sn):.2f} px. Normale +Z (face a la camera)\n")
        f.write(f"mtllib v4_wall_{name}.mtl\n")
        for j in range(N + 1):
            for i in range(N + 1):
                u, v = i / N - 0.5, j / N - 0.5
                f.write(f"v {u*cs - v*sn:.6f} {u*sn + v*cs:.6f} 0.000000\n")
        for j in range(N + 1):
            for i in range(N + 1):
                f.write(f"vt {i/N:.6f} {j/N:.6f}\n")
        f.write(f"usemtl M_V4_{name}\n")
        idx = lambda i, j: j * (N + 1) + i + 1
        for j in range(N):
            for i in range(N):
                a, b, c, d = idx(i, j), idx(i+1, j), idx(i+1, j+1), idx(i, j+1)
                # CCW vu de +Z (face avant LV3 = CCW en espace vue)
                f.write(f"f {a}/{a} {b}/{b} {c}/{c}\n")
                f.write(f"f {a}/{a} {c}/{c} {d}/{d}\n")
    with open(os.path.join(OUT, "Meshes", f"v4_wall_{name}.mtl"), "w", newline="\n") as f:
        f.write(f"# v4_wall_{name}.mtl — materiau de la dalle V4 {name}. Aucune texture : le banc mesure la geometrie.\n")
        f.write(f"newmtl M_V4_{name}\nKa 0.02 0.02 0.02\nKd 0.62 0.55 0.42\nKs 0.05 0.05 0.05\nNs 4\n")
    return cell_px

def camera(cid, pos, prio, cat):
    return {"id": cid, "parent": None, "_type": "Camera", "components": {
        "Transform": {"translation": pos, "rotation": [0, 0.0, 0], "scale": [1.0, 1.0, 1.0]},
        "Camera": {"projection": "perspective", "lens": "fov", "fov": 60.0, "near": 0.1, "infiniteFar": True,
                   "active": True, "priority": prio, "category": cat, "gizmo": {"length": 0.0},
                   "depthDisplayRange": 150, "lodTolerancePx": None}}}

# Quatre caméras à 4 m, décalées de 0,3 à 0,7 px : les mêmes triangles tombent sur d'autres
# positions sous-pixel (règle top-left, boîtes de largeurs voisines), sans changer la taille.
CAMS = [("Bench_Cam_Game", [0.0, 0.0, D], 10, "gameplay"),
        ("Bench_Cam_Dbg1", [0.0031, 0.0047, D], 3, "debug"),
        ("Bench_Cam_Dbg2", [-0.0059, 0.0023, D], 2, "debug"),
        ("Bench_Cam_Dbg3", [0.0072, -0.0066, D], 1, "debug")]

def scene(title, pick):
    nodes = [camera(*c) for c in CAMS]
    for r in range(ROWS):
        for c in range(COLS):
            name = pick(r, c)
            nodes.append({"id": f"Wall_{r}_{c}_{name}", "parent": None, "_type": "Bench", "components": {
                "Transform": {"translation": [round((c - (COLS - 1) / 2) * PITCH, 4), round((r - (ROWS - 1) / 2) * PITCH, 4), 0.0],
                              "rotation": [0, 0, 0], "scale": [1.0, 1.0, 1.0]},
                "Mesh": {"model": f"assets/Meshes/v4_wall_{name}.obj", "orbitalSpeed": 0.0, "rotationSpeed": 0.0}}})
    return {"sceneName": title, "nodes": nodes}

info = {k: write_mesh(k, n) for k, n in CLASSES.items()}
os.makedirs(os.path.join(OUT, "GraphScene"), exist_ok=True)
order = list(CLASSES)
for k in CLASSES:
    s = scene(f"Banc V - V4 {k} : mur de 18 dalles, cellules de {info[k]:.2f} px (petits triangles, lot D-a1c)", lambda r, c, k=k: k)
    json.dump(s, open(os.path.join(OUT, "GraphScene", f"bench_V_V4_{k}.json"), "w", newline="\n"), indent=1, ensure_ascii=False)
s = scene("Banc V - V4 mix : les quatre classes melangees, (ligne + colonne) mod 4 (lot D-a1c)", lambda r, c: order[(r + c) % 4])
json.dump(s, open(os.path.join(OUT, "GraphScene", "bench_V_V4_mix.json"), "w", newline="\n"), indent=1, ensure_ascii=False)
for k, v in info.items(): print(k, CLASSES[k], f"{v:.3f} px", f"etendue {v*(math.cos(TH)+math.sin(TH)):.3f} px")
