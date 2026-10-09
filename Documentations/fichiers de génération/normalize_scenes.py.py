"""
normalize_v1.py — Format de scene V1 canonique (phase G, pas 3.3d).

Regle (Onky, 2026-10-07) : toute cle est ECRITE. Soit la valeur exacte, soit
null (delegation ecrite). Une cle absente est un souci au chargement.

Ce script reecrit une scene V1 en :
  * ecrivant explicitement chaque cle lue par Serializer.cpp, avec la valeur
    que le moteur prenait DEJA par defaut (comportement inchange) ;
  * supprimant les cles qu'aucune branche ne lit (far si infiniteFar, gateFit
    sous lentille fov...) ;
  * appliquant les decisions D1..D6 (parent null, _type, plus de texture,
    "Point", _range, currentHealth, triggers explicites) ;
  * signalant (rapport) tout ce qu'il ne sait pas trancher seul.
"""
import json, sys, copy, os

REPORT = []
def note(scene, where, msg): REPORT.append(f"{scene} | {where} | {msg}")

LIGHT_TYPES = {"POINT_LIGHT": "Point", "DIRECTIONAL_LIGHT": "Directional",
               "SPOT_LIGHT": "Spot", "AMBIENT_LIGHT": "Ambient",
               "POINT": "Point", "DIRECTIONAL": "Directional", "SPOT": "Spot", "AMBIENT": "Ambient"}

def fill(d, key, value):
    """Ecrit la cle si absente, avec la valeur que le moteur prenait par defaut."""
    if key not in d:
        d[key] = copy.deepcopy(value)
        return True
    return False

def only(d, keys, scene, where):
    """Retire les cles non lues par la branche courante (sauf '_xxx')."""
    for k in list(d.keys()):
        if k.startswith("_") or k in keys: continue
        note(scene, where, f"cle non lue '{k}' = {json.dumps(d[k])} -> retiree")
        del d[k]

def norm_transform(t, s, w):
    fill(t, "translation", [0, 0, 0]); fill(t, "rotation", [0, 0, 0]); fill(t, "scale", [1, 1, 1])
    only(t, {"translation", "rotation", "scale"}, s, w)

def norm_mesh(m, s, w):
    if "model" not in m: note(s, w, "Mesh sans 'model' (cle requise) -> NON corrigeable")
    m.pop("texture", None)                                   # D3
    fill(m, "orbitalSpeed", 0.0); fill(m, "rotationSpeed", 0.0)
    only(m, {"model", "orbitalSpeed", "rotationSpeed"}, s, w)

def norm_light(l, s, w):
    t = l.get("type", "Ambient")
    if t in LIGHT_TYPES:
        note(s, w, f"Light type '{t}' -> '{LIGHT_TYPES[t]}' (D4 ; etait lu comme Ambient)")
        t = LIGHT_TYPES[t]
    l["type"] = t
    if "rangeUnits" in l: l["_range"] = l.pop("rangeUnits")  # D4
    fill(l, "color", [1, 1, 1]); fill(l, "intensity", 1.0)
    only(l, {"type", "color", "intensity"}, s, w)

def norm_camera(c, s, w):
    fill(c, "projection", "perspective")
    ortho = c["projection"] in ("orthographic", "ortho")
    fill(c, "near", 0.1); fill(c, "infiniteFar", False)
    keys = {"projection", "near", "infiniteFar", "depthDisplayRange", "lodTolerancePx",
            "category", "active", "priority", "gizmo"}
    if c["infiniteFar"]:
        if "far" in c: c.pop("far")                          # 3.3c : non lu
    else:
        fill(c, "far", 1000.0); keys.add("far")
    fill(c, "depthDisplayRange", None); fill(c, "lodTolerancePx", None)
    if ortho:
        fill(c, "orthoHeight", 10.0); keys.add("orthoHeight")
    else:
        fill(c, "lens", "fov"); keys.add("lens")
        if c["lens"] == "filmback":
            fill(c, "focalLength", 35.0); fill(c, "filmHeight", 24.0); fill(c, "gateFit", "fill")
            keys |= {"focalLength", "filmHeight", "gateFit"}
        else:
            fill(c, "fov", 45.0); keys.add("fov")
    if c.get("category") == "game":
        note(s, w, "category 'game' -> 'gameplay' (etait deja lu comme gameplay)")
        c["category"] = "gameplay"
    fill(c, "category", "gameplay"); fill(c, "active", False); fill(c, "priority", 0)
    if "gizmo" not in c: c["gizmo"] = None                  # absent = pas de gizmo -> null ecrit
    elif isinstance(c["gizmo"], dict):
        fill(c["gizmo"], "length", 2.0)                       # ancien defaut dans un bloc present
    for k in list(c.keys()):                                  # cles visiblement desactivees
        if k not in keys and not k.startswith("_") and k.endswith("AA"):
            note(s, w, f"cle '{k}' -> '_{k}' (desactivee par l'auteur ?) A CONFIRMER")
            c["_" + k] = c.pop(k)
    only(c, keys, s, w)

def norm_fps(c, s, w):
    for k, v in (("enabled", True), ("moveSpeed", 5.0), ("mouseSensitivity", 0.15),
                 ("lockVertical", True), ("pitchLimit", 89.0), ("sprintMultiplier", 3.0)):
        fill(c, k, v)
    only(c, {"enabled", "moveSpeed", "mouseSensitivity", "lockVertical", "pitchLimit", "sprintMultiplier"}, s, w)

def norm_follow(c, s, w, cam):
    if "lodTolerancePx" in c:                                 # cle mal placee
        v = c.pop("lodTolerancePx")
        note(s, w, f"'lodTolerancePx' ({json.dumps(v)}) deplacee de CameraFollow vers Camera")
        if cam is not None and cam.get("lodTolerancePx") is None: cam["lodTolerancePx"] = v
    for k, v in (("enabled", True), ("offset", [0.0, 2.0, -6.0]), ("smoothSpeed", 5.0),
                 ("lookAtHeight", 0.0), ("followRotation", True)):
        fill(c, k, v)
    if "target" not in c: note(s, w, "CameraFollow sans 'target' -> NON corrigeable")
    only(c, {"enabled", "offset", "smoothSpeed", "lookAtHeight", "followRotation", "target"}, s, w)

def norm_trigger(t, s, w):
    fill(t, "radius", 1.0); fill(t, "role", "zone")           # D5
    for k in ("onEnterEvent", "onStayEvent", "onExitEvent"): fill(t, k, "")
    only(t, {"radius", "role", "onEnterEvent", "onStayEvent", "onExitEvent"}, s, w)

def norm_health(h, s, w):
    if "m_currentHealth" in h: h["currentHealth"] = h.pop("m_currentHealth")   # D6
    fill(h, "maxHealth", 100)
    fill(h, "currentHealth", h["maxHealth"])
    only(h, {"maxHealth", "currentHealth"}, s, w)

def norm_player(p, s, w):
    fill(p, "speed", 1.0); only(p, {"speed"}, s, w)

def normalize(scene_name, d):
    fill(d, "sceneName", "<sans nom>")
    for k in list(d.keys()):
        if k not in ("sceneName", "nodes") and not k.startswith("_"):
            note(scene_name, "fichier", f"cle de fichier non lue '{k}' -> A CONFIRMER (laissee)")
    for n in d["nodes"]:
        w = n.get("id", "?")
        if "parent" not in n: n["parent"] = None              # D1
        if "type" in n: n["_type"] = n.pop("type")            # D2
        if "Notes" in n: n["_notes"] = n.pop("Notes")
        for k in list(n.keys()):
            if k not in ("id", "parent", "components") and not k.startswith("_"):
                note(scene_name, w, f"cle de noeud non lue '{k}' -> A CONFIRMER (laissee)")
        comps = n.setdefault("components", {})
        cam = comps.get("Camera")
        for cn, c in comps.items():
            ww = f"{w}.{cn}"
            if   cn == "Transform":     norm_transform(c, scene_name, ww)
            elif cn == "Mesh":          norm_mesh(c, scene_name, ww)
            elif cn == "Light":         norm_light(c, scene_name, ww)
            elif cn == "Camera":        norm_camera(c, scene_name, ww)
            elif cn == "CameraFPS":     norm_fps(c, scene_name, ww)
            elif cn == "CameraFollow":  norm_follow(c, scene_name, ww, cam)
            elif cn == "Trigger":       norm_trigger(c, scene_name, ww)
            elif cn == "Health":        norm_health(c, scene_name, ww)
            elif cn == "PlayerControl": norm_player(c, scene_name, ww)
            else: note(scene_name, ww, "composant inconnu -> A CONFIRMER (laisse)")
        # ordre canonique des cles de noeud
        ordered = {"id": n["id"], "parent": n["parent"]}
        for k, v in n.items():
            if k not in ordered and k != "components": ordered[k] = v
        ordered["components"] = n["components"]
        n.clear(); n.update(ordered)
    return d

# --- ecriture lisible : tableaux de scalaires et petits objets sur une ligne ---
def is_scalar(v): return not isinstance(v, (dict, list))
def dump(v, ind=0):
    sp = "  " * ind
    if isinstance(v, list):
        if all(is_scalar(x) for x in v):
            return "[ " + ", ".join(json.dumps(x, ensure_ascii=False) for x in v) + " ]" if v else "[]"
        return "[\n" + ",\n".join(sp + "  " + dump(x, ind + 1) for x in v) + "\n" + sp + "]"
    if isinstance(v, dict):
        if not v: return "{}"
        if len(v) <= 2 and all(is_scalar(x) for x in v.values()):
            return "{ " + ", ".join(json.dumps(k, ensure_ascii=False) + ": " + json.dumps(x, ensure_ascii=False) for k, x in v.items()) + " }"
        return "{\n" + ",\n".join(sp + "  " + json.dumps(k, ensure_ascii=False) + ": " + dump(x, ind + 1) for k, x in v.items()) + "\n" + sp + "}"
    return json.dumps(v, ensure_ascii=False)

if __name__ == "__main__":
    src, dst = sys.argv[1], sys.argv[2]
    os.makedirs(dst, exist_ok=True)
    for f in sys.argv[3:]:
        d = json.load(open(os.path.join(src, f), encoding="utf-8"))
        normalize(f, d)
        open(os.path.join(dst, f), "w", encoding="utf-8", newline="\n").write(dump(d) + "\n")
    print("\n".join(REPORT))
