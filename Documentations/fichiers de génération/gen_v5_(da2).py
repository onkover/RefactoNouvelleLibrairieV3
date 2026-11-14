# gen_v5_(da2).py — scène V5 « grands triangles » (lot D-a2, hiérarchie d'Abrash). Fichiers générés fournis tels quels.
# Un mur unique face aux caméras à 2 m, plus grand que la vue (6 m x 3,6 m ; la vue couvre 3,94 m x 2,31 m),
# tourné de 7° dans son plan pour qu'aucune arête ne tombe sur la grille des pixels.
#   v5_wall_cNN : grille de cellules carrées de NN px à l'écran (2 triangles par cellule) ; boîte d'un triangle ~ NN px.
#   v5_fan      : éventail de 360 lames de 1° autour du centre du mur (rayon 2,5 m) : boîtes immenses,
#                 triangles très minces (couverture / boîte de quelques %). Le meilleur cas de la hiérarchie.
# Taille d'une cellule à l'écran : F * s / D, F = 389,71 px (vue quad 768 x 450, fov vertical 60°).
import json, math, os
OUT = os.path.dirname(os.path.abspath(__file__))
F = 225.0 / math.tan(math.radians(30.0))          # 389,71 px
D = 2.0                                            # distance caméra -> mur (m)
TH = math.radians(7.0)
W, H = 6.0, 3.6                                    # mur (m)
CLASSES = {"c24": 24.0, "c48": 48.0, "c96": 96.0, "c192": 192.0}   # nom -> taille visée d'une cellule (px)
FAN_K, FAN_R = 360, 2.5
CAMS = [("Bench_Cam_Game", 0.0, 0.0, 10, "gameplay"), ("Bench_Cam_Dbg1", 0.0031, 0.0047, 3, "debug"),
        ("Bench_Cam_Dbg2", -0.0059, 0.0023, 2, "debug"), ("Bench_Cam_Dbg3", 0.0072, -0.0066, 1, "debug")]

def rot(u, v):
    cs, sn = math.cos(TH), math.sin(TH)
    return u * cs - v * sn, u * sn + v * cs

def write_mtl(name, label):
    with open(os.path.join(OUT, "Meshes", f"v5_{name}.mtl"), "w", newline="\n") as f:
        f.write(f"# v5_{name}.mtl — materiau du mur V5 {label}. Aucune texture : le banc mesure la geometrie.\n")
        f.write(f"newmtl M_V5_{name}\nKa 0.02 0.02 0.02\nKd 0.48 0.56 0.62\nKs 0.05 0.05 0.05\nNs 4\n")

def write_wall(name, px):
    s_target = px * D / F                           # côté visé d'une cellule (m)
    nx, ny = max(1, round(W / s_target)), max(1, round(H / s_target))
    sx, sy = W / nx, H / ny                         # cellule réelle (quasi carrée)
    path = os.path.join(OUT, "Meshes", f"v5_wall_{name}.obj")
    with open(path, "w", newline="\n") as f:
        f.write(f"# v5_wall_{name}.obj — mur de la scene V5 (gen_v5_(da2).py, lot D-a2)\n")
        f.write(f"# grille {nx} x {ny}, {2*nx*ny} triangles, {(nx+1)*(ny+1)} positions ; mur {W} x {H} m tourne de 7 deg dans le plan z = 0\n")
        f.write(f"# a {D} m : cellule {F*sx/D:.2f} x {F*sy/D:.2f} px. Normale +Z (face a la camera)\n")
        f.write(f"mtllib v5_wall_{name}.mtl\n")
        for j in range(ny + 1):
            for i in range(nx + 1):
                x, y = rot(-W / 2 + i * sx, -H / 2 + j * sy)
                f.write(f"v {x:.6f} {y:.6f} 0.000000\n")
        for j in range(ny + 1):
            for i in range(nx + 1):
                f.write(f"vt {i/nx:.6f} {j/ny:.6f}\n")
        f.write(f"usemtl M_V5_wall_{name}\n")
        idx = lambda i, j: j * (nx + 1) + i + 1
        for j in range(ny):
            for i in range(nx):
                a, b, c, d = idx(i, j), idx(i+1, j), idx(i+1, j+1), idx(i, j+1)
                f.write(f"f {a}/{a} {b}/{b} {c}/{c}\n")      # CCW vu de +Z (face avant LV3)
                f.write(f"f {a}/{a} {c}/{c} {d}/{d}\n")
    write_mtl(f"wall_{name}", name)
    return 2 * nx * ny

def write_fan():
    path = os.path.join(OUT, "Meshes", "v5_fan.obj")
    with open(path, "w", newline="\n") as f:
        f.write("# v5_fan.obj — eventail de la scene V5 (gen_v5_(da2).py, lot D-a2)\n")
        f.write(f"# {FAN_K} lames de {360/FAN_K:g} deg, rayon {FAN_R} m, centre au milieu du mur, plan z = 0, tourne de 7 deg. Normale +Z\n")
        f.write("mtllib v5_fan.mtl\n")
        f.write("v 0.000000 0.000000 0.000000\n")
        for k in range(FAN_K):
            a = 2 * math.pi * k / FAN_K + TH
            f.write(f"v {FAN_R*math.cos(a):.6f} {FAN_R*math.sin(a):.6f} 0.000000\n")
        f.write("vt 0.5 0.5\n")
        for k in range(FAN_K):
            a = 2 * math.pi * k / FAN_K + TH
            f.write(f"vt {0.5+0.5*math.cos(a):.6f} {0.5+0.5*math.sin(a):.6f}\n")
        f.write("usemtl M_V5_fan\n")
        for k in range(FAN_K):
            b, c = 2 + k, 2 + (k + 1) % FAN_K
            f.write(f"f 1/1 {b}/{b} {c}/{c}\n")              # CCW vu de +Z
    write_mtl("fan", "fan")
    return FAN_K

def camera_node(cid, x, y, prio, cat):
    return {"id": cid, "parent": None, "_type": "Camera", "components": {
        "Transform": {"translation": [x, y, D], "rotation": [0, 0.0, 0], "scale": [1.0, 1.0, 1.0]},
        "Camera": {"projection": "perspective", "lens": "fov", "fov": 60.0, "near": 0.1, "infiniteFar": True,
                   "active": True, "priority": prio, "category": cat, "gizmo": {"length": 0.0},
                   "depthDisplayRange": 150, "lodTolerancePx": None}}}

def write_scene(tag, model, label):
    nodes = [camera_node(*c) for c in CAMS]
    nodes.append({"id": f"Wall_{tag}", "parent": None, "_type": "Bench", "components": {
        "Transform": {"translation": [0.0, 0.0, 0.0], "rotation": [0, 0, 0], "scale": [1.0, 1.0, 1.0]},
        "Mesh": {"model": f"assets/Meshes/{model}", "orbitalSpeed": 0.0, "rotationSpeed": 0.0}}})
    scene = {"sceneName": f"Banc V - V5 {tag} : {label} (grands triangles, lot D-a2)", "nodes": nodes}
    os.makedirs(os.path.join(OUT, "GraphScene"), exist_ok=True)
    with open(os.path.join(OUT, "GraphScene", f"bench_V_V5_{tag}.json"), "w", newline="\n", encoding="utf-8") as f:
        json.dump(scene, f, indent=1, ensure_ascii=False)

if __name__ == "__main__":
    os.makedirs(os.path.join(OUT, "Meshes"), exist_ok=True)
    for name, px in CLASSES.items():
        n = write_wall(name, px)
        write_scene(name, f"v5_wall_{name}.obj", f"mur de cellules de {px:g} px, {n} triangles")
    n = write_fan()
    write_scene("fan", "v5_fan.obj", f"eventail de {n} lames de 1 deg")
