#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
gen_solar_system.py — generateur de scenegraph "Systeme Solaire" pour LibraryV3.

PRINCIPE DIRECTEUR (regle S1) :
    Ce fichier est la SOURCE DE VERITE. Il ne contient que des grandeurs
    PHYSIQUES REELLES (km, jours, heures, degres). Aucune valeur "deja mise a
    l'echelle a la main" n'y figure.
    Le passage aux unites moteur est fait par UN SEUL endroit : le ScaleModel.
    Changer de modele d'echelle = changer 4 constantes, pas 200 nombres.

Sorties :
    solar_system_v2.json      -> schema cible (composants Body/Orbit/Spin/Material/Ring)
    solar_system_v1compat.json-> schema actuel (Transform/Mesh/Light/Trigger) : tourne TOUT DE SUITE
    solar_system_data.md      -> la table des donnees physiques, pour la doc

Usage : python gen_solar_system.py
"""

import json, math, os

# =============================================================================
# 1. CONSTANTES ASTRONOMIQUES
# =============================================================================
AU_KM = 149_597_870.7          # unite astronomique en km
R_EARTH_KM = 6_371.0           # rayon terrestre moyen (reference des rayons)

# =============================================================================
# 2. LE MODELE D'ECHELLE  (regle S2)
# =============================================================================
# Probleme a resoudre, en une ligne : dans le systeme solaire reel, le rapport
# entre la plus petite dimension utile (rayon de Deimos, 6 km) et la plus grande
# (demi-grand axe de Neptune, 4.5e9 km) vaut 7.5e8. Aucun ecran n'affiche cela.
#
# On applique donc deux lois de puissance, une par famille de grandeurs :
#
#   distance planetaire :  D = D_REF * (a / AU) ^ P_DIST
#   rayon              :  R = R_REF * (r / R_EARTH) ^ P_RAD
#   distance satellite :  d = R_parent_display * (a / R_parent_km) ^ P_SAT
#
# Une loi de puissance (et non un log) parce qu'elle est monotone, sans
# singularite en 0, exactement inversible, et qu'elle preserve l'ORDRE et les
# rapports relatifs : Jupiter reste la plus grosse, Neptune reste la plus loin.
SCALE = {
    "name": "compressed",
    "distanceRefAU": 1.0, "distanceRefUnits": 60.0, "distanceExponent": 0.50,
    "radiusRefKm": R_EARTH_KM, "radiusRefUnits": 1.0, "radiusExponent": 0.40,
    "satelliteExponent": 0.45,
    "minRadiusUnits": 0.04,
    "daysPerSecond": 1.0,       # 1 seconde reelle = 1 jour simule
}

# Horloge de simulation (regle S10) : facteur multiplicatif, comme le zoom camera.
SIMULATION = {
    "daysPerSecond":  1.0,      # valeur de depart
    "timeScaleStep":  1.5,      # ratio par cran  ( [ et ] )
    "timeScaleSprint": 4.0,     # multiplicateur Shift, coherent avec sprintMultiplier
    "timeScaleMin":   0.001,
    "timeScaleMax":   1000.0,
    "epochDays":      0.0,      # date simulee initiale
    "_note": "Regle S11 : angles CALCULES depuis epochDays, jamais integres frame a frame."
}

def disp_radius(r_km: float) -> float:
    v = SCALE["radiusRefUnits"] * (r_km / SCALE["radiusRefKm"]) ** SCALE["radiusExponent"]
    return round(max(v, SCALE["minRadiusUnits"]), 4)

def disp_orbit_planet(a_km: float) -> float:
    return round(SCALE["distanceRefUnits"] * (a_km / AU_KM) ** SCALE["distanceExponent"], 3)

def disp_orbit_satellite(a_km: float, parent_r_km: float, parent_r_disp: float,
                         self_r_disp: float) -> float:
    ratio = (a_km / parent_r_km) ** SCALE["satelliteExponent"]
    # garde-fou : jamais sous la surface du parent
    floor = 1.6 + self_r_disp / max(parent_r_disp, 1e-6)
    return round(parent_r_disp * max(ratio, floor), 4)

# =============================================================================
# 3. LA TABLE DES DONNEES PHYSIQUES
# =============================================================================
# radius_km  : rayon moyen
# a_km       : demi-grand axe (autour du parent)
# ecc        : excentricite
# incl_deg   : inclinaison du plan orbital (planetes : / ecliptique ;
#              satellites : / plan equatorial du parent)
# period_d   : periode orbitale, en jours (signe negatif = retrograde)
# rot_h      : periode de rotation propre, en heures (negatif = retrograde)
# tilt_deg   : obliquite de l'axe de rotation
# color      : couleur d'albedo moyenne, sRGB [0..1] (secours avant le mapping)
# emissive   : intensite d'emission propre (0 pour tout sauf le Soleil)
BODIES = [
 # key           name         cls      parent    radius_km   a_km          ecc     incl    period_d    rot_h     tilt    color
 dict(key="Sun",       name="Soleil",   cls="star",   parent=None,   radius_km=696_340.0, a_km=0.0,          ecc=0.0,    incl=0.0,    period_d=0.0,       rot_h=609.12,   tilt=7.25,   color=(1.00,0.94,0.80), emissive=1.0),

 dict(key="Mercury",   name="Mercure",  cls="planet", parent="Sun",  radius_km=2_439.7,   a_km=57_909_050.0, ecc=0.2056, incl=7.005,  period_d=87.969,    rot_h=1407.6,   tilt=0.034,  color=(0.55,0.52,0.49)),
 dict(key="Venus",     name="Venus",    cls="planet", parent="Sun",  radius_km=6_051.8,   a_km=108_208_000.0,ecc=0.0068, incl=3.395,  period_d=224.701,   rot_h=-5832.5,  tilt=177.36, color=(0.91,0.79,0.53)),
 dict(key="Earth",     name="Terre",    cls="planet", parent="Sun",  radius_km=6_371.0,   a_km=149_598_023.0,ecc=0.0167, incl=0.000,  period_d=365.256,   rot_h=23.9345,  tilt=23.44,  color=(0.24,0.42,0.70)),
 dict(key="Moon",      name="Lune",     cls="moon",   parent="Earth",radius_km=1_737.4,   a_km=384_400.0,    ecc=0.0549, incl=5.145,  period_d=27.3217,   rot_h=655.72,   tilt=6.68,   color=(0.62,0.61,0.58)),
 dict(key="Mars",      name="Mars",     cls="planet", parent="Sun",  radius_km=3_389.5,   a_km=227_939_366.0,ecc=0.0934, incl=1.850,  period_d=686.980,   rot_h=24.6229,  tilt=25.19,  color=(0.76,0.42,0.28)),
 dict(key="Phobos",    name="Phobos",   cls="moon",   parent="Mars", radius_km=11.267,    a_km=9_376.0,      ecc=0.0151, incl=1.093,  period_d=0.31891,   rot_h=7.654,    tilt=0.0,    color=(0.42,0.38,0.35)),
 dict(key="Deimos",    name="Deimos",   cls="moon",   parent="Mars", radius_km=6.2,       a_km=23_463.2,     ecc=0.00033,incl=0.93,   period_d=1.26244,   rot_h=30.30,    tilt=0.0,    color=(0.46,0.42,0.38)),

 dict(key="Ceres",     name="Ceres",    cls="dwarf",  parent="Sun",  radius_km=469.73,    a_km=413_690_250.0,ecc=0.0785, incl=10.594, period_d=1_681.63,  rot_h=9.074,    tilt=4.0,    color=(0.55,0.54,0.52)),

 dict(key="Jupiter",   name="Jupiter",  cls="planet", parent="Sun",  radius_km=69_911.0,  a_km=778_479_000.0,ecc=0.0489, incl=1.303,  period_d=4_332.589, rot_h=9.9250,   tilt=3.13,   color=(0.79,0.68,0.55)),
 dict(key="Io",        name="Io",       cls="moon",   parent="Jupiter", radius_km=1_821.6,a_km=421_700.0,    ecc=0.0041, incl=0.050,  period_d=1.769138,  rot_h=42.459,   tilt=0.0,    color=(0.90,0.82,0.42)),
 dict(key="Europa",    name="Europe",   cls="moon",   parent="Jupiter", radius_km=1_560.8,a_km=671_034.0,    ecc=0.0094, incl=0.470,  period_d=3.551181,  rot_h=85.228,   tilt=0.1,    color=(0.86,0.83,0.77)),
 dict(key="Ganymede",  name="Ganymede", cls="moon",   parent="Jupiter", radius_km=2_634.1,a_km=1_070_412.0,  ecc=0.0013, incl=0.204,  period_d=7.154553,  rot_h=171.709,  tilt=0.165,  color=(0.62,0.58,0.53)),
 dict(key="Callisto",  name="Callisto", cls="moon",   parent="Jupiter", radius_km=2_410.3,a_km=1_882_709.0,  ecc=0.0074, incl=0.205,  period_d=16.689018, rot_h=400.536,  tilt=0.0,    color=(0.45,0.42,0.39)),

 dict(key="Saturn",    name="Saturne",  cls="planet", parent="Sun",  radius_km=58_232.0,  a_km=1_433_530_000.0,ecc=0.0565,incl=2.485, period_d=10_759.22, rot_h=10.656,   tilt=26.73,  color=(0.87,0.79,0.60)),
 dict(key="Mimas",     name="Mimas",    cls="moon",   parent="Saturn", radius_km=198.2,   a_km=185_539.0,    ecc=0.0196, incl=1.574,  period_d=0.942,     rot_h=22.608,   tilt=0.0,    color=(0.88,0.88,0.86)),
 dict(key="Enceladus", name="Encelade", cls="moon",   parent="Saturn", radius_km=252.1,   a_km=237_948.0,    ecc=0.0047, incl=0.009,  period_d=1.370218,  rot_h=32.885,   tilt=0.0,    color=(0.96,0.96,0.95)),
 dict(key="Tethys",    name="Thetys",   cls="moon",   parent="Saturn", radius_km=531.1,   a_km=294_619.0,    ecc=0.0001, incl=1.12,   period_d=1.887802,  rot_h=45.307,   tilt=0.0,    color=(0.92,0.91,0.89)),
 dict(key="Dione",     name="Dione",    cls="moon",   parent="Saturn", radius_km=561.4,   a_km=377_396.0,    ecc=0.0022, incl=0.019,  period_d=2.736915,  rot_h=65.686,   tilt=0.0,    color=(0.89,0.88,0.86)),
 dict(key="Rhea",      name="Rhea",     cls="moon",   parent="Saturn", radius_km=763.8,   a_km=527_108.0,    ecc=0.0013, incl=0.345,  period_d=4.518212,  rot_h=108.437,  tilt=0.0,    color=(0.87,0.86,0.84)),
 dict(key="Titan",     name="Titan",    cls="moon",   parent="Saturn", radius_km=2_574.7, a_km=1_221_870.0,  ecc=0.0288, incl=0.348,  period_d=15.945,    rot_h=382.68,   tilt=0.0,    color=(0.85,0.60,0.25)),
 dict(key="Iapetus",   name="Japet",    cls="moon",   parent="Saturn", radius_km=734.5,   a_km=3_560_820.0,  ecc=0.0286, incl=15.47,  period_d=79.3215,   rot_h=1903.72,  tilt=0.0,    color=(0.50,0.47,0.44)),

 dict(key="Uranus",    name="Uranus",   cls="planet", parent="Sun",  radius_km=25_362.0,  a_km=2_872_460_000.0,ecc=0.0457,incl=0.773, period_d=30_688.5,  rot_h=-17.24,   tilt=97.77,  color=(0.56,0.78,0.82)),
 dict(key="Miranda",   name="Miranda",  cls="moon",   parent="Uranus", radius_km=235.8,   a_km=129_390.0,    ecc=0.0013, incl=4.232,  period_d=1.413479,  rot_h=33.923,   tilt=0.0,    color=(0.72,0.72,0.71)),
 dict(key="Ariel",     name="Ariel",    cls="moon",   parent="Uranus", radius_km=578.9,   a_km=190_900.0,    ecc=0.0012, incl=0.260,  period_d=2.520,     rot_h=60.489,   tilt=0.0,    color=(0.79,0.79,0.78)),
 dict(key="Umbriel",   name="Umbriel",  cls="moon",   parent="Uranus", radius_km=584.7,   a_km=266_000.0,    ecc=0.0039, incl=0.128,  period_d=4.144,     rot_h=99.460,   tilt=0.0,    color=(0.52,0.52,0.51)),
 dict(key="Titania",   name="Titania",  cls="moon",   parent="Uranus", radius_km=788.4,   a_km=435_910.0,    ecc=0.0011, incl=0.340,  period_d=8.706,     rot_h=208.941,  tilt=0.0,    color=(0.66,0.65,0.63)),
 dict(key="Oberon",    name="Oberon",   cls="moon",   parent="Uranus", radius_km=761.4,   a_km=583_520.0,    ecc=0.0014, incl=0.058,  period_d=13.463,    rot_h=323.118,  tilt=0.0,    color=(0.61,0.60,0.58)),

 dict(key="Neptune",   name="Neptune",  cls="planet", parent="Sun",  radius_km=24_622.0,  a_km=4_495_060_000.0,ecc=0.0113,incl=1.770, period_d=60_195.0,  rot_h=16.11,    tilt=28.32,  color=(0.26,0.40,0.72)),
 dict(key="Triton",    name="Triton",   cls="moon",   parent="Neptune",radius_km=1_353.4, a_km=354_759.0,    ecc=0.000016,incl=156.885,period_d=-5.876854, rot_h=-141.044, tilt=0.0,    color=(0.82,0.80,0.77)),

 dict(key="Pluto",     name="Pluton",   cls="dwarf",  parent="Sun",  radius_km=1_188.3,   a_km=5_906_380_000.0,ecc=0.2488,incl=17.16, period_d=90_560.0,  rot_h=-153.293, tilt=122.53, color=(0.72,0.62,0.52)),
 # --- Ceinture principale : les deux plus gros asteroides ---------------------
 dict(key="Vesta",     name="Vesta",    cls="dwarf",  parent="Sun",  radius_km=262.7,     a_km=353_318_000.0,  ecc=0.0887, incl=7.14,   period_d=1_325.75,  rot_h=5.342,    tilt=29.0,   color=(0.62,0.58,0.52)),
 dict(key="Pallas",    name="Pallas",   cls="dwarf",  parent="Sun",  radius_km=255.5,     a_km=414_700_000.0,  ecc=0.2299, incl=34.83,  period_d=1_686.0,   rot_h=7.813,    tilt=84.0,   color=(0.55,0.55,0.54)),

 # --- Objets transneptuniens -------------------------------------------------
 dict(key="Haumea",    name="Haumea",   cls="dwarf",  parent="Sun",  radius_km=779.6,     a_km=6_452_000_000.0,ecc=0.1912, incl=28.21,  period_d=103_774.0, rot_h=3.9155,   tilt=126.0,  color=(0.90,0.90,0.88), axes=(1.347,0.689,1.077)),
 dict(key="Hiiaka",    name="Hi'iaka",  cls="moon",   parent="Haumea", radius_km=160.0,   a_km=49_880.0,       ecc=0.0513, incl=126.36, period_d=49.12,     rot_h=235.0,    tilt=0.0,    color=(0.88,0.88,0.87)),
 dict(key="Namaka",    name="Namaka",   cls="moon",   parent="Haumea", radius_km=85.0,    a_km=25_657.0,       ecc=0.249,  incl=113.0,  period_d=18.2783,   rot_h=100.0,    tilt=0.0,    color=(0.85,0.85,0.84)),
 dict(key="Makemake",  name="Makemake", cls="dwarf",  parent="Sun",  radius_km=715.0,     a_km=6_796_000_000.0,ecc=0.1591, incl=28.98,  period_d=113_183.0, rot_h=22.8266,  tilt=0.0,    color=(0.75,0.55,0.45)),
 dict(key="Eris",      name="Eris",     cls="dwarf",  parent="Sun",  radius_km=1_163.0,   a_km=1_015_390_000_0.0,ecc=0.4362,incl=44.04, period_d=203_830.0, rot_h=378.864,  tilt=78.0,   color=(0.94,0.94,0.92)),
 dict(key="Dysnomia",  name="Dysnomia", cls="moon",   parent="Eris", radius_km=307.5,     a_km=37_273.0,       ecc=0.0062, incl=0.0,    period_d=15.786,    rot_h=378.864,  tilt=0.0,    color=(0.30,0.29,0.28)),
 dict(key="Charon",    name="Charon",   cls="moon",   parent="Pluto",radius_km=606.0,     a_km=19_591.0,     ecc=0.0002, incl=0.080,  period_d=6.3872,    rot_h=153.293,  tilt=0.0,    color=(0.60,0.58,0.56)),
]

BY_KEY = {b["key"]: b for b in BODIES}

# --- Anneaux : (corps, interieur_km, exterieur_km, couleur, opacite) ---------
RINGS = [
    dict(key="Jupiter", inner_km=122_500.0, outer_km=129_000.0, color=(0.55,0.50,0.45), opacity=0.10),
    dict(key="Saturn",  inner_km= 74_500.0, outer_km=140_220.0, color=(0.82,0.77,0.66), opacity=0.85),
    dict(key="Uranus",  inner_km= 38_000.0, outer_km= 51_150.0, color=(0.45,0.50,0.52), opacity=0.25),
    dict(key="Neptune", inner_km= 40_900.0, outer_km= 62_930.0, color=(0.30,0.36,0.50), opacity=0.15),
    dict(key="Haumea",  inner_km=  2_252.0, outer_km=  2_322.0, color=(0.70,0.70,0.68), opacity=0.35),
]

# --- Ceintures : (nom, a_min_AU, a_max_AU, epaisseur relative) ---------------
BELTS = [
    dict(key="AsteroidBelt", name="Ceinture principale", inner_au=2.06, outer_au=3.27, thickness=0.30),
    dict(key="KuiperBelt",   name="Ceinture de Kuiper",  inner_au=30.0, outer_au=50.0, thickness=0.20),
]

# =============================================================================
# 4. LES ZONES DE DANGER (regle S7)
# =============================================================================
# GAMEPLAY, pas physique : exprimees en rayons du corps porteur, valeurs
# choisies pour etre jouables. La colonne "reel" documente la grandeur physique
# de reference dont elle s'inspire (rayon de Hill, limite de Roche, magnetosphere).
HAZARDS = {
 "Sun":     [("CORONA",     2.00, "Couronne solaire — degats extremes"),
             ("HEAT",       4.50, "Zone de chaleur — degats continus (reste sous l'orbite de Mercure)"),
             ("GRAVITY",    6.00, "Puits gravitationnel du Soleil")],
 "Mercury": [("SURFACE",    1.05, "Pas d'atmosphere — impact direct"),
             ("GRAVITY",    3.20, "Puits gravitationnel (Hill reel ~ 90 R)")],
 "Venus":   [("ATMOSPHERE", 1.20, "Atmosphere corrosive 92 bar, 464 C"),
             ("GRAVITY",    4.50, "Puits gravitationnel (Hill reel ~ 168 R)")],
 "Earth":   [("ATMOSPHERE", 1.12, "Rentree atmospherique"),
             ("GRAVITY",    5.00, "Puits gravitationnel (Hill reel ~ 235 R)"),
             ("SAFE_ZONE",  9.00, "Zone de reparation / ravitaillement")],
 "Moon":    [("GRAVITY",    3.00, "Puits gravitationnel lunaire")],
 "Mars":    [("ATMOSPHERE", 1.10, "Atmosphere tenue + tempetes de poussiere"),
             ("GRAVITY",    4.20, "Puits gravitationnel (Hill reel ~ 320 R)")],
 "Ceres":   [("GRAVITY",    3.00, "Puits gravitationnel faible")],
 "Jupiter": [("ATMOSPHERE", 1.15, "Ecrasement — 1000 bar a -100 km"),
             ("RADIATION",  4.00, "Ceinture de radiations / tore de Io : letal"),
             ("GRAVITY",   10.00, "Puits gravitationnel majeur (Hill reel ~ 740 R)")],
 "Io":      [("VOLCANIC",   2.00, "Volcanisme actif — ejectas")],
 "Europa":  [("GRAVITY",    2.50, "Puits gravitationnel")],
 "Saturn":  [("ATMOSPHERE", 1.15, "Ecrasement atmospherique"),
             ("GRAVITY",    8.00, "Puits gravitationnel (Hill reel ~ 1100 R)")],
 "Titan":   [("ATMOSPHERE", 1.40, "Atmosphere epaisse d'azote/methane")],
 "Uranus":  [("ATMOSPHERE", 1.15, "Ecrasement atmospherique"),
             ("GRAVITY",    6.50, "Puits gravitationnel")],
 "Neptune": [("ATMOSPHERE", 1.15, "Vents a 2100 km/h"),
             ("GRAVITY",    6.50, "Puits gravitationnel")],
 "Pluto":   [("GRAVITY",    3.00, "Puits gravitationnel faible")],
 "Vesta":   [("GRAVITY",    2.50, "Puits gravitationnel — ancrage minier")],
 "Haumea":  [("SPIN",       2.20, "Rotation en 3h55 : debris ejectes a l'equateur"),
             ("GRAVITY",    3.00, "Puits gravitationnel")],
 "Eris":    [("GRAVITY",    3.00, "Puits gravitationnel — corps le plus lointain")],
}

# =============================================================================
# 5. TEXTURES
# =============================================================================
TEX_DIR = "assets/Textures"

def textures_for(b):
    """Jeu de maps recommande par corps. Convention : equirectangulaire 2:1."""
    k = b["key"].lower()
    t = {"albedoMap": f"{TEX_DIR}/{k}_albedo_2k.jpg"}
    if b["cls"] == "star":
        return {"emissiveMap": f"{TEX_DIR}/sun_emissive_2k.jpg",
                "albedoMap":   f"{TEX_DIR}/sun_albedo_2k.jpg"}
    # relief : tous les corps solides
    if b["cls"] in ("planet", "moon", "dwarf") and b["key"] not in (
            "Jupiter", "Saturn", "Uranus", "Neptune"):
        t["normalMap"] = f"{TEX_DIR}/{k}_normal_2k.png"
        t["heightMap"] = f"{TEX_DIR}/{k}_height_2k.png"
    if b["key"] == "Earth":
        t["specularMap"] = f"{TEX_DIR}/earth_specular_2k.png"   # masque oceans
        t["emissiveMap"] = f"{TEX_DIR}/earth_night_2k.jpg"      # lumieres des villes
        t["cloudMap"]    = f"{TEX_DIR}/earth_clouds_2k.png"     # couche separee, alpha
    if b["key"] in ("Venus",):
        t["cloudMap"] = f"{TEX_DIR}/venus_clouds_2k.jpg"
    if b["key"] in ("Titan",):
        t["cloudMap"] = f"{TEX_DIR}/titan_haze_2k.png"
    return t

# =============================================================================
# 6. GEOMETRIE / LOD
# =============================================================================
def mesh_for(b, r_disp):
    """Le LOD est choisi sur le rayon AFFICHE, pas sur le rayon reel."""
    if r_disp >= 2.0:  return "assets/Meshes/sphere_hi.obj"
    if r_disp >= 0.5:  return "assets/Meshes/sphere_mid.obj"
    return "assets/Meshes/sphere_lo.obj"

# =============================================================================
# 7. CONSTRUCTION DES NOEUDS
# =============================================================================
def compute_display():
    """Passe 1 : calcule rayon et distance affiches de chaque corps."""
    d = {}
    for b in BODIES:
        d[b["key"]] = {"r": disp_radius(b["radius_km"])}
    for b in BODIES:
        if b["parent"] is None:
            d[b["key"]]["a"] = 0.0
        elif b["parent"] == "Sun":
            d[b["key"]]["a"] = disp_orbit_planet(b["a_km"])
        else:
            p = BY_KEY[b["parent"]]
            d[b["key"]]["a"] = disp_orbit_satellite(
                b["a_km"], p["radius_km"], d[p["key"]]["r"], d[b["key"]]["r"])
    return d

DISP = compute_display()

def anchor_id(k):  return f"{k}-Anchor"
def body_id(k):    return k

def deg_per_day(period_d):
    return 0.0 if period_d == 0.0 else 360.0 / period_d

def deg_per_hour(rot_h):
    return 0.0 if rot_h == 0.0 else 360.0 / rot_h


def build_v2_nodes():
    nodes = []

    # -- Racine du systeme : porte le referentiel, rien d'autre --------------
    nodes.append({
        "id": "Solar-System",
        "type": "Anchor",
        "components": {"Transform": {"translation": [0,0,0], "rotation": [0,0,0], "scale": [1,1,1]}}
    })

    for idx, b in enumerate(BODIES):
        k, r, a = b["key"], DISP[b["key"]]["r"], DISP[b["key"]]["a"]
        parent_anchor = "Solar-System" if b["parent"] is None else anchor_id(b["parent"])
        ax = b.get("axes", (1.0, 1.0, 1.0))   # corps triaxial (Haumea) : echelle non uniforme

        # ---- Noeud ANCRE : porte la POSITION orbitale, jamais d'orientation
        anchor = {
            "id": anchor_id(k),
            "parent": parent_anchor,
            "type": "Anchor",
            "components": {
                "Transform": {"translation": [round(a,4),0,0], "rotation": [0,0,0], "scale": [1,1,1]}
            }
        }
        if b["parent"] is not None:
            anchor["components"]["Orbit"] = {
                "semiMajorAxisKm":  b["a_km"],
                "displayRadius":    a,
                "eccentricity":     b["ecc"],
                "inclinationDeg":   b["incl"],
                "periodDays":       b["period_d"],
                "meanAnomalyDeg":   round((idx * 137.507) % 360.0, 2),  # angle d'or : phases bien reparties, deterministes
                "angularSpeedDegPerDay": round(deg_per_day(b["period_d"]), 8),
            }
        nodes.append(anchor)

        # ---- Noeud CORPS : porte l'orientation propre + le rendu -----------
        body = {
            "id": body_id(k),
            "parent": anchor_id(k),
            "type": {"star":"Etoile","planet":"Planete","moon":"Lune","dwarf":"PlaneteNaine"}[b["cls"]],
            "components": {
                "Transform": {"translation":[0,0,0], "rotation":[0,0,0],
                              "scale":[round(r*ax[0],4), round(r*ax[1],4), round(r*ax[2],4)]},
                "Body": {
                    "class":     b["cls"],
                    "radiusKm":  b["radius_km"],
                    "displayRadius": r,
                    "axes": [round(x,4) for x in ax],
                },
                "Spin": {
                    "axialTiltDeg":        b["tilt"],
                    "rotationPeriodHours": b["rot_h"],
                    "angularSpeedDegPerHour": round(deg_per_hour(b["rot_h"]), 8),
                    "initialAngleDeg":     0.0,
                    "inheritParentRotation": False
                },
                "Mesh": {"model": mesh_for(b, r)},
                "Material": dict(
                    baseColor=[round(c,3) for c in b["color"]],
                    emissive=b.get("emissive", 0.0),
                    **textures_for(b)
                )
            }
        }
        if b["cls"] == "star":
            body["components"]["Light"] = {
                "type": "POINT_LIGHT",
                "color": [round(c,3) for c in b["color"]],
                "intensity": 4.0,
                "rangeUnits": 600.0
            }
        nodes.append(body)

        # ---- Anneaux -------------------------------------------------------
        for rg in RINGS:
            if rg["key"] != k: continue
            nodes.append({
                "id": f"{k}-Rings",
                "parent": anchor_id(k),
                "type": "Ring",
                "components": {
                    "Transform": {"translation":[0,0,0], "rotation":[0,0,0], "scale":[1,1,1]},
                    "Spin": {"axialTiltDeg": b["tilt"], "rotationPeriodHours": 0.0,
                             "inheritParentRotation": False},
                    "Ring": {
                        "innerRadiusKm": rg["inner_km"], "outerRadiusKm": rg["outer_km"],
                        "innerRadius": round(r * rg["inner_km"]/b["radius_km"], 4),
                        "outerRadius": round(r * rg["outer_km"]/b["radius_km"], 4),
                        "segments": 96
                    },
                    "Material": {
                        "baseColor": [round(c,3) for c in rg["color"]],
                        "opacity":   rg["opacity"],
                        "albedoMap": f"{TEX_DIR}/{k.lower()}_ring_albedo.png",
                        "alphaMap":  f"{TEX_DIR}/{k.lower()}_ring_alpha.png",
                        "uvMode":    "radial",
                        "doubleSided": True
                    }
                }
            })

        # ---- Zones de danger ----------------------------------------------
        for (evt, mult, note) in HAZARDS.get(k, []):
            nodes.append({
                "id": f"{k}-{evt.title().replace('_','')}-Zone",
                "parent": anchor_id(k),
                "_note": note,
                "type": "Trigger",
                "components": {
                    "Transform": {"translation":[0,0,0], "rotation":[0,0,0], "scale":[1,1,1]},
                    "Trigger": {
                        "role": "zone",
                        "shape": "sphere",
                        "radiusBodyRadii": mult,
                        "radius": round(r * mult, 4),
                        "onEnterEvent": f"ENTER_{k.upper()}_{evt}",
                        "onStayEvent":  f"STAY_{k.upper()}_{evt}",
                        "onExitEvent":  f"EXIT_{k.upper()}_{evt}"
                    }
                }
            })

    # -- Ceintures : triggers en coquille ------------------------------------
    for belt in BELTS:
        ri = disp_orbit_planet(belt["inner_au"] * AU_KM)
        ro = disp_orbit_planet(belt["outer_au"] * AU_KM)
        nodes.append({
            "id": f"{belt['key']}-Zone",
            "parent": "Solar-System",
            "_note": f"{belt['name']} — trigger en coquille (shape 'shell')",
            "type": "Trigger",
            "components": {
                "Transform": {"translation":[0,0,0], "rotation":[0,0,0], "scale":[1,1,1]},
                "Trigger": {
                    "role": "zone",
                    "shape": "shell",
                    "innerRadius": ri, "outerRadius": ro,
                    "halfThickness": round((ro-ri)*belt["thickness"], 3),
                    "radius": ro,
                    "onEnterEvent": f"ENTER_{belt['key'].upper()}",
                    "onStayEvent":  f"STAY_{belt['key'].upper()}",
                    "onExitEvent":  f"EXIT_{belt['key'].upper()}"
                }
            }
        })

    return nodes


# =============================================================================
# 7bis. LA CEINTURE D'ASTEROIDES
# =============================================================================
BELT_CFG = dict(
    count=600, seed=1337,
    inner_au=2.06, outer_au=3.27,
    ecc_max=0.20, incl_max_deg=12.0,
    radius_min_km=8.0, radius_max_km=120.0,
    rot_min_h=2.0, rot_max_h=20.0,
    meshes=["assets/Meshes/rock_a.obj", "assets/Meshes/rock_b.obj", "assets/Meshes/rock_c.obj"],
    # Lacunes de Kirkwood : resonances orbitales avec Jupiter (3:1, 5:2, 7:3, 2:1).
    # Jupiter y a vide la ceinture. Purement decoratif ici, mais visible de dessus.
    kirkwood=[(2.502, 0.035), (2.825, 0.025), (2.958, 0.022), (3.279, 0.030)],
)

class _LCG:
    """Generateur congruentiel lineaire. Deterministe et portable : la meme
    ceinture a chaque generation, condition pour qu'un test de charge soit
    reproductible. random.random() de Python ne l'aurait pas garanti d'une
    version a l'autre."""
    def __init__(self, seed): self.s = seed & 0xFFFFFFFF
    def next(self):
        self.s = (1664525 * self.s + 1013904223) & 0xFFFFFFFF
        return self.s / 4294967296.0
    def rng(self, a, b): return a + (b - a) * self.next()

def build_belt_nodes():
    """Les 600 asteroides, cuits en noeuds. Le v2 n'en a pas besoin (il porte un
    composant Belt que le moteur developpe au chargement) ; le v1 si."""
    cfg = BELT_CFG; r = _LCG(cfg["seed"]); nodes = []; i = 0; guard = 0
    while i < cfg["count"] and guard < cfg["count"] * 20:
        guard += 1
        a_au = r.rng(cfg["inner_au"], cfg["outer_au"])
        if any(abs(a_au - g) < w for (g, w) in cfg["kirkwood"]):
            continue                                    # rejet : lacune de Kirkwood
        a_km   = a_au * AU_KM
        rad_km = cfg["radius_min_km"] * (cfg["radius_max_km"] / cfg["radius_min_km"]) ** (r.next() ** 2.2)
        per_d  = 365.256 * a_au ** 1.5                  # 3e loi de Kepler
        rot_h  = r.rng(cfg["rot_min_h"], cfg["rot_max_h"])
        rd     = disp_radius(rad_km)
        tint   = r.rng(-0.06, 0.06)
        col    = [round(min(1.0, max(0.0, c + tint)), 3) for c in (0.46, 0.42, 0.38)]
        key    = "Asteroid_%03d" % i
        nodes.append({
            "id": key + "-Anchor", "parent": "Solar-System", "type": "Anchor",
            "components": {
                "Transform": {"translation": [round(disp_orbit_planet(a_km), 4), 0, 0],
                              "rotation": [0, 0, 0], "scale": [1, 1, 1]},
                "Orbit": {"semiMajorAxisKm": round(a_km, 1),
                          "displayRadius": round(disp_orbit_planet(a_km), 4),
                          "eccentricity": round(r.rng(0.0, cfg["ecc_max"]), 4),
                          "inclinationDeg": round(r.rng(-cfg["incl_max_deg"], cfg["incl_max_deg"]), 3),
                          "periodDays": round(per_d, 3),
                          "meanAnomalyDeg": round(r.rng(0.0, 360.0), 2),
                          "angularSpeedDegPerDay": round(360.0 / per_d, 8)}}})
        nodes.append({
            "id": key, "parent": key + "-Anchor", "type": "Asteroide",
            "components": {
                "Transform": {"translation": [0, 0, 0],
                              "rotation": [0, 0, 0],
                              "scale": [rd, round(rd * r.rng(0.7, 1.0), 4),
                                        round(rd * r.rng(0.7, 1.0), 4)]},
                "Body": {"class": "asteroid", "radiusKm": round(rad_km, 2),
                         "displayRadius": rd, "axes": [1.0, 1.0, 1.0]},
                "Spin": {"axialTiltDeg": round(r.rng(0.0, 180.0), 2),
                         "rotationPeriodHours": round(rot_h, 3),
                         "angularSpeedDegPerHour": round(360.0 / rot_h, 6),
                         "initialAngleDeg": round(r.rng(0.0, 360.0), 2),
                         "inheritParentRotation": False},
                "Mesh": {"model": cfg["meshes"][int(r.next() * 3) % 3]},
                "Material": {"baseColor": col, "emissive": 0.0,
                             "albedoMap": f"{TEX_DIR}/rock_albedo_1k.jpg",
                             "normalMap": f"{TEX_DIR}/rock_normal_1k.png"}}})
        i += 1
    return nodes

def build_belt_component():
    """Version v2 : douze lignes au lieu de 1200 noeuds."""
    cfg = BELT_CFG
    return {
        "id": "AsteroidBelt", "parent": "Solar-System", "type": "Belt",
        "_note": "Developpe proceduralement au chargement. Meme graine = meme ceinture.",
        "components": {
            "Transform": {"translation": [0,0,0], "rotation": [0,0,0], "scale": [1,1,1]},
            "Belt": {
                "count": cfg["count"], "seed": cfg["seed"],
                "innerAU": cfg["inner_au"], "outerAU": cfg["outer_au"],
                "eccentricityMax": cfg["ecc_max"], "inclinationMaxDeg": cfg["incl_max_deg"],
                "radiusMinKm": cfg["radius_min_km"], "radiusMaxKm": cfg["radius_max_km"],
                "rotationPeriodMinHours": cfg["rot_min_h"], "rotationPeriodMaxHours": cfg["rot_max_h"],
                "meshes": cfg["meshes"],
                "kirkwoodGaps": [[g, w] for (g, w) in cfg["kirkwood"]],
                "baseColor": [0.46, 0.42, 0.38]
            }
        }
    }

# =============================================================================
# 8. CAMERAS ET JOUEUR (communs aux deux schemas)
# =============================================================================
def build_gameplay_nodes():
    d_earth = DISP["Earth"]["a"]
    return [
    {
      "id": "Player_Ship",
      "_note": "Pas de parent : le controleur ecrit une position MONDE.",
      "components": {
        "Transform": {"translation": [round(d_earth + 4.0,3), 0.0, 0.0],
                      "rotation": [0,0,0], "scale": [0.35,0.35,0.35]},
        "Mesh":  {"model": "assets/Meshes/ship_scout.obj"},
        "Material": {
            "baseColor":[0.72,0.74,0.78],
            "albedoMap":   f"{TEX_DIR}/ship_scout_albedo_1k.png",
            "normalMap":   f"{TEX_DIR}/ship_scout_normal_1k.png",
            "specularMap": f"{TEX_DIR}/ship_scout_spec_1k.png",
            "emissiveMap": f"{TEX_DIR}/ship_scout_emissive_1k.png"
        },
        "Trigger": {"role":"probe", "shape":"sphere", "radius": 0.5},
        "Health":  {"maxHealth": 100},
        "PlayerControl": {"speed": 12.0}
      }
    },
    {
      "id": "FPS_Camera",
      "_note": "Pas de parent : un controleur ecrit en local.",
      "components": {
        "Transform": {"translation": [round(d_earth,3), 3.0, 12.0], "rotation": [0,0,0], "scale":[1,1,1]},
        "Camera": {"projection":"perspective","lens":"fov","fov":45.0,"near":0.05,
                   "far":2000.0,"infiniteFar":True,"active":True,"priority":10,
                   "category":"game","gizmo":{"length":3.0}},
        "CameraFPS": {"enabled":True,"moveSpeed":25.0,"mouseSensitivity":0.15,
                      "lockVertical":False,"pitchLimit":89.0,"sprintMultiplier":6.0}
      }
    },
    {
      "id": "Follow_Camera",
      "_note": "Pas de parent : CameraFollowSystem produit une position MONDE.",
      "components": {
        "Transform": {"translation":[0,0,0],"rotation":[0,0,0],"scale":[1,1,1]},
        "Camera": {"projection":"perspective","lens":"fov","fov":50.0,"near":0.05,
                   "far":2000.0,"infiniteFar":True,"active":True,"priority":5,
                   "category":"game","gizmo":{"length":3.0}},
        "CameraFollow": {"enabled":True,"target":"Player_Ship","offset":[0.0,1.5,-6.0],
                         "smoothSpeed":5.0,"lookAtHeight":0.0}
      }
    },
    {
      "id": "Overview_Camera",
      "_note": "Vue d'ensemble du systeme.",
      "components": {
        "Transform": {"translation":[0,0,0],"rotation":[0,0,0],"scale":[1,1,1]},
        "Camera": {"projection":"orthographic","orthoHeight":1200.0,"near":1.0,
                   "far":5000.0,"infiniteFar":True,"active":True,"priority":0,
                   "category":"debug","gizmo":{"length":3.0}}
      }
    },
    {
      "id": "Top_View",
      "components": {
        "Transform": {"translation":[0,950,0],"rotation":[-90,0,0],"scale":[1,1,1]},
        "Camera": {"projection":"perspective","lens":"fov","fov":60.0,"near":1.0,
                   "far":5000.0,"infiniteFar":True,"active":True,"priority":4,
                   "category":"debug","gizmo":{"length":3.0}}
      }
    },
    {
      "id": "Side_View",
      "components": {
        "Transform": {"translation":[950,0,0],"rotation":[0,90,0],"scale":[1,1,1]},
        "Camera": {"projection":"perspective","lens":"fov","fov":60.0,"near":1.0,
                   "far":5000.0,"infiniteFar":True,"active":True,"priority":3,
                   "category":"debug","gizmo":{"length":3.0}}
      }
    }]


# =============================================================================
# 9. RETRO-PROJECTION VERS LE SCHEMA ACTUEL (v1 compat)
# =============================================================================
def to_v1(nodes_v2):
    """
    Retro-projection vers ce que Serializer.cpp sait lire AUJOURD'HUI :
    Transform / Mesh(model, texture, orbitalSpeed, rotationSpeed) / Light /
    Camera / CameraFPS / CameraFollow / Trigger / Health / PlayerControl.

    Trois aplatissements sont necessaires — et chacun est exactement la dette
    que le schema v2 supprime :

      1. Le couple (X-Anchor, X) est FUSIONNE en un seul noeud X. Position
         orbitale et orientation propre cohabitent alors dans le meme Transform
         -> l'axe de rotation precesse avec la revolution (section 3).
      2. Comme le noeud porte desormais une ECHELLE (le rayon affiche), tout
         enfant herite de cette echelle. Il faut donc PRE-DIVISER la translation
         et l'echelle de chaque enfant par le rayon affiche du parent. Des
         nombres justes, mais illisibles : personne ne peut relire "9.177" et y
         reconnaitre l'orbite terrestre.
      3. Les triggers en coquille redeviennent des spheres pleines de rayon
         exterieur.

    Perdus : excentricite, inclinaison, anneaux, maps multiples, LOD par groupe.
    """
    byid = {n["id"]: n for n in nodes_v2}
    out, consumed = [], set()

    def parent_scale(pid):
        """Echelle heritee du noeud parent une fois l'ancre fusionnee."""
        if not pid: return 1.0
        base = pid[:-7] if pid.endswith("-Anchor") else pid
        b = byid.get(base)
        if not b: return 1.0
        return b.get("components", {}).get("Body", {}).get("displayRadius", 1.0)

    def remap(pid):
        return pid[:-7] if pid and pid.endswith("-Anchor") else pid

    for n in nodes_v2:
        nid = n["id"]
        if nid in consumed or nid.endswith("-Anchor"):
            continue
        c = dict(n.get("components", {}))
        anchor = byid.get(f"{nid}-Anchor")
        raw_parent = anchor.get("parent") if anchor is not None else n.get("parent")
        parent = remap(raw_parent)
        k = parent_scale(raw_parent)          # facteur de compensation
        inv = 1.0 / k if k else 1.0

        nc = {}
        tr = dict(c.get("Transform", {}))
        if anchor is not None:
            at = anchor["components"]["Transform"]
            tr["translation"] = [round(v * inv, 5) for v in at["translation"]]
            consumed.add(anchor["id"])
        elif "translation" in tr and parent:
            tr["translation"] = [round(v * inv, 5) for v in tr["translation"]]
        if "scale" in tr and parent:
            tr["scale"] = [round(v * inv, 5) for v in tr["scale"]]
        spin = c.get("Spin")
        if spin and spin.get("axialTiltDeg", 0.0):
            tr["rotation"] = [round(spin["axialTiltDeg"], 3), 0.0, 0.0]
        if tr: nc["Transform"] = tr

        orb = anchor["components"].get("Orbit") if anchor is not None else None
        if "Mesh" in c:
            m = {"model": c["Mesh"]["model"]}
            mat = c.get("Material", {})
            if "albedoMap" in mat: m["texture"] = os.path.basename(mat["albedoMap"])
            if orb:  m["orbitalSpeed"]  = round(orb["angularSpeedDegPerDay"] * SCALE["daysPerSecond"], 6)
            if spin: m["rotationSpeed"] = round(spin["angularSpeedDegPerHour"] * 24.0
                                                * SCALE["daysPerSecond"], 6)
            nc["Mesh"] = m

        for key in ("Light", "Camera", "CameraFPS", "CameraFollow", "Health", "PlayerControl"):
            if key in c: nc[key] = c[key]

        if "Trigger" in c:
            t = c["Trigger"]
            r = t["outerRadius"] if t.get("shape") == "shell" else t["radius"]
            nc["Trigger"] = {"role": t.get("role", "zone"), "radius": r,
                             "onEnterEvent": t.get("onEnterEvent", ""),
                             "onStayEvent":  t.get("onStayEvent", ""),
                             "onExitEvent":  t.get("onExitEvent", "")}
            # Regle S9 : le rayon d'un trigger est en unites MONDE et n'herite
            # jamais de l'echelle. Aucune compensation a poser ici.

        if not nc: continue
        node = {"id": nid}
        if parent: node["parent"] = parent
        if "type"  in n: node["type"]  = n["type"]
        if "_note" in n: node["_note"] = n["_note"]
        node["components"] = nc
        out.append(node)
    return out


# =============================================================================
# 10. ECRITURE
# =============================================================================
def main():
    nodes_v2 = build_v2_nodes() + [build_belt_component()] + build_gameplay_nodes()
    belt     = build_belt_nodes()

    scene_v2 = {
        "sceneName": "Systeme Solaire",
        "schemaVersion": 2,
        "units":  {"length": "km", "time": "day", "angle": "deg"},
        "scaleModel": SCALE,
        "simulation": SIMULATION,
        "nodes": nodes_v2
    }
    with open("solar_system_v2.json", "w", encoding="utf-8") as f:
        json.dump(scene_v2, f, indent=2, ensure_ascii=False)

    v1_core = to_v1([n for n in nodes_v2 if n["id"] != "AsteroidBelt"])
    with open("solar_system_v1compat.json", "w", encoding="utf-8") as f:
        json.dump({"sceneName": "Systeme Solaire", "nodes": v1_core},
                  f, indent=2, ensure_ascii=False)
    with open("solar_system_v1compat_belt.json", "w", encoding="utf-8") as f:
        json.dump({"sceneName": "Systeme Solaire + ceinture",
                   "nodes": v1_core + to_v1(belt)}, f, indent=2, ensure_ascii=False)

    # ---- table de reference ------------------------------------------------
    lines = ["# Donnees physiques et valeurs d'affichage\n",
             f"Modele d'echelle `{SCALE['name']}` : "
             f"distance = {SCALE['distanceRefUnits']}x(a/UA)^{SCALE['distanceExponent']} ; "
             f"rayon = {SCALE['radiusRefUnits']}x(r/Rterre)^{SCALE['radiusExponent']} ; "
             f"satellites = Rparent x (a/Rparent_km)^{SCALE['satelliteExponent']}\n",
             "| Corps | Parent | Rayon (km) | R affiche | a (km) | a affiche | e | i (deg) "
             "| P orb (j) | P rot (h) | Obliquite | Couleur |",
             "|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|"]
    for b in BODIES:
        d = DISP[b["key"]]
        col = "#%02X%02X%02X" % tuple(int(round(c * 255)) for c in b["color"])
        lines.append(f"| {b['name']} | {b['parent'] or '-'} | {b['radius_km']:,.1f} | {d['r']:.3f} "
                     f"| {b['a_km']:,.0f} | {d['a']:.3f} | {b['ecc']:.4f} | {b['incl']:.3f} "
                     f"| {b['period_d']:,.3f} | {b['rot_h']:,.2f} | {b['tilt']:.2f} | `{col}` |")
    open("solar_system_data.md", "w", encoding="utf-8").write("\n".join(lines) + "\n")

    ntri = {"assets/Meshes/sphere_lo.obj": 288, "assets/Meshes/sphere_mid.obj": 960,
            "assets/Meshes/sphere_hi.obj": 3968, "assets/Meshes/rock_a.obj": 168,
            "assets/Meshes/rock_b.obj": 168, "assets/Meshes/rock_c.obj": 168,
            "assets/Meshes/ship_scout.obj": 296}
    def tris(ns): return sum(ntri.get(n["components"].get("Mesh", {}).get("model"), 0) for n in ns)
    print(f"v2            : {len(nodes_v2):4d} noeuds")
    print(f"v1 (sans belt): {len(v1_core):4d} noeuds, {tris(v1_core):,} triangles")
    print(f"v1 (avec belt): {len(v1_core) + len(to_v1(belt)):4d} noeuds, "
          f"{tris(v1_core) + tris(to_v1(belt)):,} triangles")
    print(f"asteroides    : {len(belt)//2}")
    print("R affiche : Soleil %.3f | Jupiter %.3f | Terre %.3f | Deimos %.3f"
          % (DISP["Sun"]["r"], DISP["Jupiter"]["r"], DISP["Earth"]["r"], DISP["Deimos"]["r"]))
    print("Orbites   : Mercure %.1f | Terre %.1f | Jupiter %.1f | Neptune %.1f | Pluton %.1f | Eris %.1f"
          % (DISP["Mercury"]["a"], DISP["Earth"]["a"], DISP["Jupiter"]["a"],
             DISP["Neptune"]["a"], DISP["Pluto"]["a"], DISP["Eris"]["a"]))

if __name__ == "__main__":
    main()
