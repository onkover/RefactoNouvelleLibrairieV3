#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
gen_textures.py — textures procedurales pour LibraryV3.

Deux familles, deux intentions differentes :

  1. LES TEXTURES DU VAISSEAU. Fabriquees pour coller EXACTEMENT aux bandes UV
     de ship_scout.obj (verifiees disjointes) :
         hull     v 0.023 - 0.332
         wings    v 0.361 - 0.572
         engine   v 0.621 - 0.781
         thruster v 0.801 - 0.842
         glass    v 0.861 - 0.990
     Chaque bande recoit un traitement different, ce qui rend une erreur de
     depliage immediatement visible : si la verriere apparait sur une aile, le
     bug saute aux yeux au lieu de se cacher dans un degrade.

  2. LA MIRE UV. C'est un OUTIL DE MESURE, pas une decoration. Elle sert a
     valider le depliage des spheres AVANT de poser la moindre vraie texture.
     Contre-exemple a ne pas faire : debugger un depliage avec une photo de
     planete. Sur une image organique, une couture d'un pixel, un damier
     inverse ou un pole en eventail sont invisibles. Sur une mire, ils
     hurlent.

Convention OBJ : vt v = 0 en BAS de l'image. Ligne image = (1 - v) * H.
"""
import math
from PIL import Image, ImageDraw, ImageFont

# --- bandes UV du vaisseau, mesurees sur ship_scout.obj ----------------------
BANDS = [
    ("hull",     0.023, 0.332, (168, 174, 186)),
    ("wings",    0.361, 0.572, (150, 156, 168)),
    ("engine",   0.621, 0.781, ( 92,  96, 104)),
    ("thruster", 0.801, 0.842, ( 20,  26,  38)),
    ("glass",    0.861, 0.990, ( 22,  34,  48)),
]

def row(v, H):  return int(round((1.0 - v) * (H - 1)))

def band_box(name, W, H):
    for n, v0, v1, col in BANDS:
        if n == name: return (0, row(v1, H), W - 1, row(v0, H)), col
    raise KeyError(name)

def hatch(d, box, step, color, width=1):
    """Rayures a 45 deg, bornees a la boite : sert de trame de panneaux."""
    x0, y0, x1, y1 = box
    for k in range(-(y1 - y0), (x1 - x0) + 1, step):
        d.line([(x0 + k, y0), (x0 + k + (y1 - y0), y1)], fill=color, width=width)

def panels(d, box, nx, ny, color):
    """Lignes de tolerie. Elles suivent la grille UV, donc elles revelent aussi
    une distorsion : un panneau qui s'etire sur le modele signale un ilot mal
    proportionne."""
    x0, y0, x1, y1 = box
    for i in range(1, nx):
        x = x0 + (x1 - x0) * i // nx
        d.line([(x, y0), (x, y1)], fill=color, width=1)
    for j in range(1, ny):
        y = y0 + (y1 - y0) * j // ny
        d.line([(x0, y), (x1, y)], fill=color, width=1)

# =============================================================================
#  1. ALBEDO
# =============================================================================
def ship_albedo(size=1024):
    img = Image.new("RGB", (size, size), (14, 15, 18))
    d = ImageDraw.Draw(img)
    for name, v0, v1, col in BANDS:
        box = (0, row(v1, size), size - 1, row(v0, size))
        d.rectangle(box, fill=col)
        dark = tuple(max(0, c - 26) for c in col)
        light = tuple(min(255, c + 22) for c in col)
        if name == "hull":
            panels(d, box, 12, 5, dark)
            hatch(d, box, 96, light)
            # bandes d'identification : deux liserets colores le long du fuselage
            y = box[1] + (box[3] - box[1]) // 3
            d.rectangle((0, y, size - 1, y + 8), fill=(196, 84, 40))
            d.rectangle((0, y + 12, size - 1, y + 15), fill=(232, 200, 120))
        elif name == "wings":
            panels(d, box, 8, 4, dark)
            # marques d'aile : un triangle par demi-atlas (babord / tribord)
            for cx in (int(size * 0.41), int(size * 0.79)):
                cy = (box[1] + box[3]) // 2
                d.polygon([(cx, cy - 26), (cx - 22, cy + 20), (cx + 22, cy + 20)],
                          fill=(196, 84, 40))
        elif name == "engine":
            panels(d, box, 16, 3, dark)
            hatch(d, box, 40, (58, 62, 70))
        elif name == "thruster":
            cx, cy = size // 2, (box[1] + box[3]) // 2
            for r, c in ((26, (18, 62, 150)), (17, (60, 150, 235)), (9, (210, 240, 255))):
                d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=c)
        elif name == "glass":
            for i in range(box[1], box[3] + 1):     # degrade vertical
                t = (i - box[1]) / max(1, box[3] - box[1])
                d.line([(0, i), (size - 1, i)],
                       fill=(int(18 + 34 * t), int(30 + 44 * t), int(46 + 56 * t)))
            for x in range(0, size, size // 10):    # montants de verriere
                d.line([(x, box[1]), (x, box[3])], fill=(120, 132, 146), width=2)
    return img

# =============================================================================
#  2. NORMALE — tangent space, +Z vers l'observateur => (128,128,255) = plat
# =============================================================================
def ship_normal(size=1024):
    img = Image.new("RGB", (size, size), (128, 128, 255))
    d = ImageDraw.Draw(img)
    # Une rainure se code par deux lignes de pente OPPOSEE : un flanc qui
    # descend, un flanc qui remonte. Une seule ligne produirait une marche,
    # pas un creux, et l'eclairage serait faux d'un cote sur deux.
    for name, v0, v1, _ in BANDS:
        if name in ("thruster", "glass"): continue
        box = (0, row(v1, size), size - 1, row(v0, size))
        nx = {"hull": 12, "wings": 8, "engine": 16}[name]
        ny = {"hull": 5, "wings": 4, "engine": 3}[name]
        for i in range(1, nx):
            x = box[0] + (box[2] - box[0]) * i // nx
            d.line([(x - 1, box[1]), (x - 1, box[3])], fill=(96, 128, 240), width=1)
            d.line([(x + 1, box[1]), (x + 1, box[3])], fill=(160, 128, 240), width=1)
        for j in range(1, ny):
            y = box[1] + (box[3] - box[1]) * j // ny
            d.line([(box[0], y - 1), (box[2], y - 1)], fill=(128, 96, 240), width=1)
            d.line([(box[0], y + 1), (box[2], y + 1)], fill=(128, 160, 240), width=1)
    return img

# =============================================================================
#  3. SPECULAIRE / 4. EMISSIF
# =============================================================================
def ship_specular(size=1024):
    img = Image.new("L", (size, size), 0)
    d = ImageDraw.Draw(img)
    for name, v0, v1, _ in BANDS:
        box = (0, row(v1, size), size - 1, row(v0, size))
        d.rectangle(box, fill={"hull": 140, "wings": 120, "engine": 190,
                               "thruster": 10, "glass": 250}[name])
    return img.convert("RGB")

def ship_emissive(size=1024):
    img = Image.new("RGB", (size, size), (0, 0, 0))
    d = ImageDraw.Draw(img)
    _, y1, _, y0 = (0, row(0.842, size), 0, row(0.801, size))
    cx, cy = size // 2, (y1 + y0) // 2
    for r, c in ((30, (10, 40, 110)), (20, (40, 130, 230)), (11, (220, 245, 255))):
        d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=c)
    # liseret de position sur le fuselage
    yb = row(0.20, size)
    d.rectangle((0, yb, size - 1, yb + 3), fill=(160, 40, 30))
    return img

# =============================================================================
#  5. LA MIRE UV — outil de validation du depliage des spheres
# =============================================================================
def uv_grid(W=2048, H=1024):
    """Equirectangulaire 2:1. Cinq informations superposees :
         - damier 32x16 : revele l'echelle et l'orientation
         - degrade horizontal rouge->vert : revele le sens de u
         - degrade vertical  bleu         : revele le sens de v
         - barre magenta a u=0 et u=1     : la COUTURE doit etre invisible
         - disques jaunes aux poles       : le pole ne doit pas s'eventailler
    """
    img = Image.new("RGB", (W, H))
    px = img.load()
    cw, ch = W // 32, H // 16
    for y in range(H):
        v = 1.0 - y / (H - 1)
        for x in range(W):
            u = x / (W - 1)
            light = ((x // cw) + (y // ch)) % 2 == 0
            r = int(210 * u) + (30 if light else 0)
            g = int(210 * (1.0 - u)) + (30 if light else 0)
            b = int(200 * v) + (30 if light else 0)
            px[x, y] = (min(r, 255), min(g, 255), min(b, 255))
    d = ImageDraw.Draw(img)
    # couture : deux demi-barres qui doivent se recoller en une seule sur le modele
    d.rectangle((0, 0, 3, H - 1), fill=(255, 0, 255))
    d.rectangle((W - 4, 0, W - 1, H - 1), fill=(255, 0, 255))
    # meridien de reference u = 0.5 et equateur v = 0.5
    d.line([(W // 2, 0), (W // 2, H - 1)], fill=(0, 0, 0), width=3)
    d.line([(0, H // 2), (W - 1, H // 2)], fill=(0, 0, 0), width=3)
    # poles
    d.rectangle((0, 0, W - 1, 10), fill=(255, 230, 40))
    d.rectangle((0, H - 11, W - 1, H - 1), fill=(255, 140, 40))
    # graduations tous les 30 deg de longitude
    try:    font = ImageFont.load_default(size=26)
    except TypeError: font = ImageFont.load_default()
    for k in range(0, 13):
        x = int(k * (W - 1) / 12)
        d.line([(x, H // 2 - 18), (x, H // 2 + 18)], fill=(0, 0, 0), width=2)
        d.text((min(x + 6, W - 60), H // 2 + 22), f"{k*30}", fill=(0, 0, 0), font=font)
    return img

def rock_albedo(size=1024, seed=7):
    """Albedo generique des asteroides : bruit fractal en niveaux de gris."""
    import random
    rnd = random.Random(seed)
    img = Image.new("RGB", (size, size), (110, 102, 94))
    d = ImageDraw.Draw(img)
    for _ in range(2600):                        # crateres
        r = int(4 + 46 * rnd.random() ** 3)
        x, y = rnd.randrange(size), rnd.randrange(size)
        t = rnd.uniform(-0.22, 0.16)
        c = tuple(max(0, min(255, int(v * (1 + t)))) for v in (110, 102, 94))
        d.ellipse((x - r, y - r, x + r, y + r), fill=c)
    return img

if __name__ == "__main__":
    out = [
        ("ship_scout_albedo_1k.png",   ship_albedo()),
        ("ship_scout_normal_1k.png",   ship_normal()),
        ("ship_scout_spec_1k.png",     ship_specular()),
        ("ship_scout_emissive_1k.png", ship_emissive()),
        ("uv_test_2k.png",             uv_grid()),
        ("rock_albedo_1k.jpg",         rock_albedo()),
    ]
    for name, im in out:
        im.save(name, quality=92)
        print(f"{name:30s} {im.size[0]}x{im.size[1]}  {im.mode}")
