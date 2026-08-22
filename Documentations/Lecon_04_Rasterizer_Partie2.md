# Leçon 04 — Le Rasterizer (Partie 2 : Clipping, Perspective, Profondeur)

> **Moteur :** LibraryV3 | **Projet :** LIB (Static Library) | **Compilateur :** Visual Studio 2026 / C++23
> **Namespace canonique :** `LV3` | **Prérequis :** L02 (main droite, vecteur-ligne, row-major) · L04 P1 (edge function, top-left rule à biais relatif) · L05 (reverse-Z, far infini, `Projection`, `Viewport`, `ViewData`, `Frustum` 3 états, `DepthBuffer`, `Renderer` à état)

---

## 0. Position dans le pipeline

```
   World ──V──▶ VIEW ──P──▶ ┌──────────────────────────────────────┐
                            │  CLIP SPACE (x, y, z, w)             │
                            │                                      │
                            │  ▶▶▶ CHANTIER A — CLIPPING NEAR ◀◀◀  │  AVANT la division
                            │  1 triangle → 0, 1 ou 2 triangles    │
                            └──────────────┬───────────────────────┘
                                           │  ÷ w
                            ┌──────────────▼───────────────────────┐
                            │  NDC   x,y ∈ [-1,1]   z ∈ [0,1]      │
                            └──────────────┬───────────────────────┘
                                           │  Viewport::ToRaster (flip Y)
                            ┌──────────────▼───────────────────────┐
                            │  SCREEN SPACE + invW                 │
                            │  Back-face culling (signe de l'aire) │
                            │  RasterizeTriangle (P1)              │
                            │                                      │
                            │  ▶▶▶ CHANTIER C — EARLY-Z GREATER ◀◀◀│
                            │  ▶▶▶ CHANTIER B — ÷ invW par pixel ◀◀│
                            └──────────────────────────────────────┘
```

**Le point d'architecture à graver :** le clipping se fait en clip space, le shading en screen space, et **`invW` est le seul passager qui traverse la frontière**. Tout le reste est soit consommé avant (les `w`), soit produit après (les pixels).

---

## 1. Ce qui était déjà juste — et pourquoi

Trois choses fonctionnaient avant cette leçon. Il faut savoir **pourquoi** avant d'y toucher.

### 1.1 L'interpolation de `z` est affine, et c'est EXACT

Avec la matrice de `Projection.cpp` (reverse-Z, main droite, `m[2][3] = -1`) :

```
w_clip = d                              (d = -z_vue = distance à l'œil)
z_clip = n·(f-d)/(f-n)
z_ndc  = z_clip/w = [n·f/(f-n)]·(1/d) − n/(f-n)
```

`z_ndc` est une fonction **affine de `1/d`**. Or `1/d = 1/w` est linéaire en espace écran. Donc **`z_ndc` est linéaire en espace écran**, et les barycentriques affines de la Partie 1 l'interpolent exactement.

En far infini (`m[2][2]=0`, `m[3][2]=n`), c'est encore plus net : `z_ndc = n/d`, purement proportionnel à `1/w`.

> **Conséquence architecturale majeure :** la profondeur est le **seul** attribut testable sans avoir calculé le dénominateur perspectif. C'est exactement ce qui rend l'early-Z possible.

### 1.2 Le test de rejet near était algébriquement bon

L'ancien `if (c[k].w <= view.nearPlane)` est équivalent à `w − z < 0` au facteur positif `f/(f−n)` près, puisque `w = d`. **Le test était correct.** Ce qui était faux, c'est ce qu'on en faisait : rejeter la face entière.

### 1.3 Ce qui était faux

| Défaut | Symptôme |
|---|---|
| Rejet de face entière près du near | Faces qui **disparaissent d'un coup** au lieu d'être coupées |
| `w <= nearPlane` en orthographique | `w = 1` partout → le test ne teste plus rien |
| `EIntersect::Inside` produit puis jeté | Travail de discrimination payé pour rien |
| Aucun varying interpolable | Chantier B inobservable |

---

## 2. Chantier A — Le clipping near

### 2.1 Pourquoi clipper est obligatoire, pas optionnel

Un sommet derrière la caméra a `w ≤ 0`. La division `x/w` produit alors :

- `w = 0` → division par zéro, `inf` ou `NaN`
- `w < 0` → le point est projeté **du mauvais côté**, symétriquement. Le triangle se retourne, s'étire, disparaît de façon erratique.

Ce n'est pas un artefact esthétique. **C'est une singularité mathématique.**

### 2.2 Pourquoi en clip space, avant la division

1. **Après la division, l'information est détruite.** Un point à `w < 0` a déjà été projeté au mauvais endroit.
2. **En clip space, l'interpolation est linéaire.** Le point d'intersection s'obtient par un simple `lerp` de paramètre `t` — sur les coordonnées homogènes **et sur tous les attributs**. C'est le seul espace où cette simplicité existe.
3. **Le volume canonique rend le test trivial** : des comparaisons, pas des intersections rayon/plan.

### 2.3 Le test canonique — forme reverse-Z

Volume canonique (NDC `x,y ∈ [-1,1]`, `z ∈ [0,1]`) :

```
-w ≤ x ≤ w        -w ≤ y ≤ w        0 ≤ z ≤ w
```

| Condition | Standard | **Reverse-Z (LV3)** |
|---|---|---|
| `z ≥ 0` | plan **near** | plan **far** |
| `w − z ≥ 0` | plan **far** | plan **near** |

```cpp
[[nodiscard]] LV3_FORCEINLINE float NearDistance(const ClipVertex& v) noexcept
{
    return v.clip.w - v.clip.z;      // reverse-Z : w - z est le plan NEAR
}
```

**Vérification algébrique :** `w − z = f(d − n)/(f − n)`, donc `≥ 0` ⟺ `d ≥ n`. La fonction est **affine dans les coordonnées de clip**, ce qui garantit un `t` d'intersection exact.

**Bonus non évident :** ce test unique rejette aussi tous les points à `w ≤ 0` (derrière la caméra), puisque `d ≤ 0 < n`. Aucun cas particulier à écrire.

**Far infini :** `w − z = d − n`. Même test, sans changement de code. ✅ *validé par le test 8*

### 2.4 Règle dictatoriale — un seul plan clippé

| Plan | Traitement | Pourquoi |
|---|---|---|
| **near** | Clipping géométrique (Sutherland-Hodgman) | Singularité mathématique — obligatoire |
| gauche/droite/haut/bas | **Scissor** : `Viewport::ClampBox` | Aucune singularité. 4 `min/max` vs allocations + triangles supplémentaires |
| **far** | Rien | Avec far infini, il n'existe pas |

C'est ce que fait le matériel : les GPU ont un **guard band** précisément pour éviter le clipping latéral.

### 2.5 Décision — trianguler AVANT de clipper

Les meshes ont `vertsPerFace ∈ {3,4}`. Deux ordres possibles :

| | Clipper le polygone, puis trianguler | **Trianguler, puis clipper** |
|---|---|---|
| Taille max de sortie | quad → 5 sommets | triangle → **4, borne fixe** |
| Convexité exigée | **oui** (un quad OBJ concave casse) | **jamais** (un triangle est convexe) |
| Chemin de code | deux tailles d'entrée | **un seul, uniforme** |
| Coût sur face traversante | 1 clipping | 2 clippings |
| Correspondance matérielle | — | ✅ le *primitive assembly* GPU triangule avant le clip |

**Décision : on triangule d'abord.** Le surcoût ne concerne que les faces qui traversent le near (< 1 %), tandis que le bénéfice — borne fixe, aucune hypothèse de convexité, chemin unique — est structurel.

### 2.6 L'algorithme — Blinn-Newell + Sutherland-Hodgman + Cyrus-Beck

**Le nom exact de la technique :** *Blinn-Newell homogeneous clipping*, dont la règle d'émission est celle de Sutherland-Hodgman, spécialisée à un plan, avec trivial accept/reject à la Cyrus-Beck.

| Brique | Apport |
|---|---|
| **Blinn-Newell** | Opérer en coordonnées homogènes 4D, avant la division → `t` exact, interpolation d'attributs linéaire |
| **Sutherland-Hodgman** | La règle d'émission ordonnée qui préserve topologie et winding |
| **Cyrus-Beck** | Le rejet/acceptation trivial en tête, qui court-circuite ≈ 95 % des triangles |

#### La règle d'émission

Pour chaque arête `i → j` :

```
                      dj ≥ 0                dj < 0
              ┌──────────────────────┬──────────────────────┐
   di ≥ 0     │  émettre src[i]      │  émettre src[i]      │
              │                      │  émettre I(i,j)      │
              ├──────────────────────┼──────────────────────┤
   di < 0     │  émettre I(i,j)      │  (rien)              │
              └──────────────────────┴──────────────────────┘
```

Traduit en deux conditions indépendantes :

```
si (di ≥ 0)                    →  émettre src[i]      « le sommet est gardé »
si (signe(di) ≠ signe(dj))     →  émettre I(i,j)      « l'arête traverse »
```

**L'ordre des deux `if` est structurel** : le sommet vient toujours avant l'intersection de son arête sortante, sinon le contour s'auto-intersecte.

#### Les quatre cas

| Sommets gardés | Polygone | Triangles émis |
|---|---|---|
| 0 | rejeté | **0** |
| 1 | triangle (1 sommet + 2 intersections) | **1** |
| 2 | **quadrilatère** (2 sommets + 2 intersections) | **2** |
| 3 | inchangé | **1** |

Le cas à 2 sommets impose que la fonction retourne **jusqu'à 4 sommets**, jamais 3.

### 2.7 Pourquoi PAS Cyrus-Beck ni Liang-Barsky ici

**Confusion à dissiper :** il y a deux « Sutherland » distincts.

| Algorithme | Nature | Réputation |
|---|---|---|
| **Cohen-Sutherland** (1967) | Découpage de **segment** par outcodes, itératif | ✅ « algo pour débutant » — c'est celui-là |
| **Sutherland-Hodgman** (1974) | Découpage de **polygone** par plans successifs | ❌ Rien à voir. Référence en pipeline 3D |

**Cyrus-Beck et Liang-Barsky sont les rivaux de Cohen-Sutherland, pas de Sutherland-Hodgman.** Ils résolvent un problème différent : ils clippent un **segment**, pas un **polygone**.

#### La démonstration

Clippe les 3 arêtes d'un triangle indépendamment avec Cyrus-Beck :

- Arête V0→V1 : gardée
- Arête V1→V2 : tronquée en **I₁**
- Arête V2→V0 : tronquée en **I₂**

Résultat : **3 segments avec un trou entre I₁ et I₂**. Le segment I₁→I₂ — l'arête *créée par le plan de coupe* — n'existe dans aucune arête d'entrée. **Aucun algorithme de clipping de segment ne peut le produire**, quelle que soit son efficacité. C'est une limite de **topologie**, pas de performance.

> **Règle 20 — Le choix de l'algorithme de clipping suit la topologie de sortie, pas la réputation.**

| Sortie requise | Algorithme | Statut LV3 |
|---|---|---|
| Polygone fermé (triangle → rasterizer) | **Sutherland-Hodgman 4D homogène** | ✅ acté |
| Segment (ray/AABB, picking) | **Liang-Barsky** (= *slab method*) | ✅ acté, L05 |
| Segment vs volume convexe non aligné | **Cyrus-Beck** | ✅ acté |
| — | Cohen-Sutherland | ❌ rejeté (itératif) |

**Ce qui est récupéré de Cyrus-Beck :** le rejet/acceptation trivial (`if all out → 0`, `if all in → copie`) et la formulation paramétrique `t = dA/(dA−dB)`, qui **est** sa formule, réécrite avec les distances pré-calculées.

### 2.8 Code canonique

#### `Rendering/ClipVertex.h`

```cpp
#pragma once
#include "../Maths/MatrixLib.h"

namespace LV3
{

// Sommet en espace de CLIP, avant division par w.
//
// ⚠️ CONTRAT DE MAINTENANCE — À LIRE AVANT D'AJOUTER UN CHAMP
// Tout attribut ajouté ici DOIT être ajouté dans Lerp() ci-dessous.
// Un attribut oublié vaut zéro UNIQUEMENT sur les triangles clippés,
// donc uniquement quand la caméra frôle l'objet. Bug fantôme garanti.
struct ClipVertex
{
    Vec4f clip;      // (x, y, z, w) — sortie de MulRow(mvp, position)
    // uv     : différé — dette MeshClass::GetFaceView
    // normal : différé — idem
};

[[nodiscard]] LV3_FORCEINLINE ClipVertex Lerp(const ClipVertex& a,
                                              const ClipVertex& b,
                                              float t) noexcept
{
    ClipVertex r;
    r.clip = a.clip + (b.clip - a.clip) * t;
    // AJOUTER ICI tout nouvel attribut de ClipVertex.
    return r;
}

[[nodiscard]] LV3_FORCEINLINE float NearDistance(const ClipVertex& v) noexcept
{
    return v.clip.w - v.clip.z;
}

} // namespace LV3
```

#### `Rendering/Clipper.h`

```cpp
#pragma once
#include "ClipVertex.h"

namespace LV3
{

// Un triangle (convexe, 3 sommets) coupé par UN plan donne au plus 4 sommets.
// Garantie mathématique : un convexe coupé par un plan gagne au plus un sommet.
inline constexpr int32_t kMaxClipVertices = 4;

[[nodiscard]] int32_t ClipTriangleNear(const ClipVertex src[3],
                                        ClipVertex dst[kMaxClipVertices]) noexcept;

} // namespace LV3
```

#### `Rendering/Clipper.cpp`

```cpp
#include "Clipper.h"

namespace LV3
{

// Ordre total DÉTERMINISTE sur les coordonnées de clip.
// NÉCESSAIRE, mesuré : sans lui, 4 trous + 2 doublons sur la couture du quad
// de Test_ClipCoverage (124 trous si EdgeFunction n'est pas antisymétrique).
// Les deux correctifs sont requis : ils traitent deux causes distinctes.
[[nodiscard]] LV3_FORCEINLINE bool ClipLess(const Vec4f& a, const Vec4f& b) noexcept
{
    if (a.x != b.x) return a.x < b.x;
    if (a.y != b.y) return a.y < b.y;
    if (a.z != b.z) return a.z < b.z;
    return a.w < b.w;
}

int32_t ClipTriangleNear(const ClipVertex src[3],
                          ClipVertex dst[kMaxClipVertices]) noexcept
{
    const float d[3] = { NearDistance(src[0]),
                         NearDistance(src[1]),
                         NearDistance(src[2]) };

    // ── Rejet trivial (idée de Cyrus-Beck) ──
    if (d[0] < 0.0f && d[1] < 0.0f && d[2] < 0.0f)
        return 0;

    // ── Acceptation triviale ──
    if (d[0] >= 0.0f && d[1] >= 0.0f && d[2] >= 0.0f)
    {
        dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2];
        return 3;
    }

    // ── Sutherland-Hodgman, un plan, parcours ORDONNÉ des arêtes ──
    int32_t count = 0;

    for (int32_t i = 0; i < 3; ++i)
    {
        const int32_t j  = (i + 1) % 3;
        const float   di = d[i];
        const float   dj = d[j];

        // 1. le sommet AVANT l'intersection de son arête sortante.
        //    Inverser ces deux blocs produit un polygone auto-intersectant.
        if (di >= 0.0f)
            dst[count++] = src[i];

        // 2. changement de signe → l'arête traverse le plan
        if ((di >= 0.0f) != (dj >= 0.0f))
        {
            // Calcul TOUJOURS dans le sens canonique lo → hi, quel que soit
            // le sens dans lequel CE triangle parcourt l'arête.
            // di - dj est STRICTEMENT positif : aucun epsilon requis,
            // aucune division par zéro possible.
            const bool    swap = !ClipLess(src[i].clip, src[j].clip);
            const int32_t lo   = swap ? j : i;
            const int32_t hi   = swap ? i : j;

            const float dl = d[lo], dh = d[hi];
            dst[count++] = Lerp(src[lo], src[hi], dl / (dl - dh));
        }
    }

    LV3_ASSERT(count == 3 || count == 4);
    return count;
}

} // namespace LV3
```

### 2.9 Intégration — `RenderSystem.cpp`

```cpp
// ── Le seul endroit du moteur où la division par w a lieu ──
static void EmitClipTriangle(Renderer& renderer, const ViewData& view,
                             const ClipVertex& a, const ClipVertex& b,
                             const ClipVertex& c, Color col)
{
    const ClipVertex* v[3] = { &a, &b, &c };

    Vec3f r[3];
    float invW[3];

    for (int k = 0; k < 3; ++k)
    {
        // Après clipping, w > 0 est GARANTI. Pas de garde nécessaire.
        invW[k] = 1.0f / v[k]->clip.w;
        r[k] = view.viewport.ToRaster({ v[k]->clip.x * invW[k],
                                        v[k]->clip.y * invW[k],
                                        v[k]->clip.z * invW[k] });
    }

    if (IsBackFacing(EdgeFunction(r[0], r[1], r[2]))) return;

    renderer.DrawTriangle(
        RasterTriangle{ { r[0].x, r[0].y }, { r[1].x, r[1].y }, { r[2].x, r[2].y },
                          r[0].z,  r[1].z,  r[2].z,
                          invW[0], invW[1], invW[2] },
        col);
}
```

```cpp
for (size_t f = 0; f < mesh->faceCount(); ++f)
{
    const uint32_t base = uint32_t(f) * vpf;

    ClipVertex cv[4];
    for (uint8_t k = 0; k < vpf; ++k)
        cv[k].clip = MulRow(mvp, mesh->vertexPositions[mesh->indices[base + k]]);

    // Chemin rapide garanti par la classification du MESH (Chantier C.2),
    // pas redécouvert face par face.
    bool allIn = true;

    if (needsNearClip)
    {
        float d[4];
        bool  allOut = true;
        for (uint8_t k = 0; k < vpf; ++k)
        {
            d[k] = NearDistance(cv[k]);
            if (d[k] >= 0.0f) allOut = false;
            else              allIn  = false;
        }
        if (allOut) continue;
    }

    const Color col = FaceColor(int(f));

    for (uint8_t t = 0; t + 2 < vpf; ++t)
    {
        const ClipVertex tri[3] = { cv[0], cv[t + 1], cv[t + 2] };

        if (allIn)
        {
            EmitClipTriangle(renderer, view, tri[0], tri[1], tri[2], col);
        }
        else
        {
            ClipVertex poly[kMaxClipVertices];
            const int32_t n = ClipTriangleNear(tri, poly);
            // éventail — winding PRÉSERVÉ par l'ordre d'émission
            for (int32_t q = 1; q + 1 < n; ++q)
                EmitClipTriangle(renderer, view, poly[0], poly[q], poly[q + 1], col);
        }
    }
}
```

**`w > 0` est garanti après clipping** — c'est la propriété la plus utile du Chantier A : plus de garde anti-division, et `invW` toujours fini et positif.

---

## 3. BUG MAJEUR — Le backface culling était inversé depuis la Leçon 05

### 3.1 La démonstration, depuis `Viewport::ToRaster`

Triangle **front-face** au sens LV3 : CCW vu de la caméra, main droite, regard −Z.

```
a = (-1,-1,-5)    b = (1,-1,-5)    c = (0,1,-5)
(b−a)×(c−a) = (2,0,0)×(1,2,0) = (0,0,4)  → normale +Z, vers la caméra ✅
```

En 800×800, avec `yr = (0.5 − yn·0.5)·800` (**le flip**) :

| | NDC | Raster |
|---|---|---|
| a | (−0.2, −0.2) | (320, **480**) |
| b | ( 0.2, −0.2) | (480, **480**) |
| c | ( 0.0,  0.2) | (400, **320**) |

```
EdgeFunction(a,b,c) = (480−320)·(320−480) − (480−480)·(400−320)
                    = 160 · (−160) = −25600      ← NÉGATIF
```

> **En LV3, une face avant a une aire signée NÉGATIVE en espace raster.** Le flip Y inverse le sens de rotation : CCW en espace vue devient CW à l'écran.

Le test `if (EdgeFunction(...) <= 0.0f) continue;` **cullait donc les faces avant et gardait les faces arrière.**

### 3.2 Pourquoi personne ne l'a vu pendant toute une leçon

`FaceColor(int(f))` est un **hash par face**. Sur un objet convexe fermé :

| | Faces avant dessinées | Faces arrière dessinées |
|---|---|---|
| Silhouette | identique | identique |
| Test de profondeur | garde la surface proche | garde la lointaine, **cohérente** |
| Couleurs | hash arbitraire | hash arbitraire |

**Un cube rendu à l'envers ressemble exactement à un cube.** Aucun indice visuel tant que les couleurs n'ont pas de sens géométrique.

Le journal L05 contient même la trace du raisonnement fautif :

> *« Commente le test `area <= 0` → si le mesh apparaît, le signe de winding est inversé »*

Le mesh apparaissait **avec** `<= 0` — donc le test a été jugé correct. Ce qui apparaissait, c'étaient les faces arrière. **Le critère « ça s'affiche » ne discrimine pas les deux hypothèses.**

Ce qui l'a rendu visible : `ShadeFragment_Depth` produit une couleur **qui a un sens géométrique**. Une distance mesurée sur la face lointaine, ça se voit.

### 3.3 Le correctif — un seul point de vérité

```cpp
// Rendering/Rasterizer.h
//
// CONVENTION DE FACE — mesurée, pas devinée. Verrouillée par TNR §Winding.
//   Front-face LV3 = CCW en espace VUE (main droite, regard -Z).
//   Viewport::ToRaster inverse Y  =>  CW en espace RASTER
//   =>  l'aire signée d'une FACE AVANT est NEGATIVE.
//
// Le >= rejette aussi l'aire nulle : triangle dégénéré, rien à dessiner.
[[nodiscard]] LV3_FORCEINLINE bool IsBackFacing(float signedArea) noexcept
{
    return signedArea >= 0.0f;
}
```

Le commentaire est la moitié du correctif : sans lui, le prochain qui lit `>= 0` le « corrigera » en croyant bien faire.

---

## 4. BUG MAJEUR — 124 trous sur les coutures du clipping

### 4.1 Le défaut

Deux triangles adjacents partagent une arête, mais la **parcourent en sens opposé** :

```
Triangle A traverse  2 → 0  :  t = d2/(d2−d0)   →   I20 = Lerp(v2, v0, t)
Triangle B traverse  0 → 2  :  t = d0/(d0−d2)   →   I02 = Lerp(v0, v2, t)
```

Mathématiquement `I20 == I02`. **En IEEE 754, presque jamais bit à bit** : deux séquences d'opérations différentes sur les mêmes données. Un écart d'un ULP suffit à ce que la couture ne soit plus une arête partagée mais deux arêtes distinctes séparées d'un fuseau infiniment mince — et la règle top-left, qui teste `w == 0.0f` exactement, ne voit plus rien à recoller.

### 4.2 La mesure

```
[COVERAGE] doubles=0  trous=124
[COVERAGE] premier (215,40)  dernier (90,165)
```

```
Δx = −125    Δy = +125    →  pente exactement −1
124 trous pour 125 pas    →  UN pixel par ligne, sans exception
```

Une **droite d'un pixel de large**, continue. `doubles = 0` : les deux triangles se rejettent mutuellement. Signature sans ambiguïté de la fissure de couture.

### 4.3 La bissection — les deux correctifs sont nécessaires

| Configuration | Résultat |
|---|---|
| Ancienne `EdgeFunction` + pas de `ClipLess` | **124 trous**, 0 doublon |
| **Nouvelle `EdgeFunction` seule** | **4 trous, 2 doublons** |
| Nouvelle `EdgeFunction` + `ClipLess` | **0 / 0** ✅ |

Deux causes distinctes :

- **`EdgeFunction` antisymétrique** supprime 97 % du défaut. Elle règle l'**amplification** : les intersections tombaient à ~4500 pixels hors viewport, où l'annulation catastrophique transforme un ULP en plusieurs pixels.
- **`ClipLess`** supprime le reste. Il traite la cause **racine** : les deux triangles produisaient physiquement deux points différents. Aucune amélioration du rasterizer ne peut réparer ça.

L'apparition de `doubles = 2` dans la configuration intermédiaire est la confirmation : selon le côté où l'écart tombe, un pixel est réclamé par personne (trou) ou par les deux (doublon). L'erreur change de signe le long de l'arête.

### 4.4 `EdgeFunction` antisymétrique

```cpp
// Le NOYAU. Tout le reste y délègue.
//
// Forme ANTISYMÉTRIQUE : le point testé p sert de référence, donc
//     E(b,a,p) == -E(a,b,p)  EXACTEMENT en IEEE 754
// (les deux produits sont identiques, seul l'ordre de la soustraction change).
//
// Bénéfice secondaire : en soustrayant p d'abord, les magnitudes restent
// locales au pixel au lieu d'être absolues. Supprime l'annulation
// catastrophique sur les sommets très éloignés du viewport, ce qui arrive
// systématiquement sur les intersections du plan near.
[[nodiscard]] LV3_FORCEINLINE constexpr float EdgeFunction(
    float ax, float ay, float bx, float by, float px, float py) noexcept
{
    const float ux = ax - px, uy = ay - py;
    const float vx = bx - px, vy = by - py;
    return ux * vy - uy * vx;
}
```

> ⚠️ **La formule vit dans le NOYAU, jamais dans les surcharges.** La cascade `(a,b,c)` → noyau et `(a,b,px,py)` → noyau existe précisément pour qu'une correction se fasse à un seul endroit. Modifier une surcharge sans le noyau crée **deux formules** pour la même quantité : le culling et la rasterisation cessent d'être d'accord, et sur un triangle presque dégénéré leurs signes peuvent diverger.

---

## 5. Chantier B — Interpolation perspective-correcte

### 5.1 Le théorème fondamental

> **Pour tout attribut `a` linéaire en espace 3D, `a/w` est linéaire en espace écran. Et `1/w` l'est aussi.**

Démonstration en une ligne : la projection perspective est une homographie ; l'inverse d'une homographie est une homographie ; en coordonnées homogènes, `1/w` est le facteur qui « défait » la division.

### 5.2 La recette

```
Au triangle SETUP (3 fois par triangle) :  a'ₖ = aₖ · invWₖ
Au PIXEL :   num = b0·a'₀ + b1·a'₁ + b2·a'₂
             den = b0·invW₀ + b1·invW₁ + b2·invW₂
             a   = num / den
```

**Une seule division par pixel**, quel que soit le nombre de varyings : `den` est calculé une fois et partagé.

### 5.3 Ce qui exige la correction, et ce qui ne l'exige pas

| Attribut | Correction ? |
|---|---|
| UV, couleurs, normales, position monde, tangentes | ✅ **oui** |
| Distance linéaire à l'œil (`w`) | ✅ oui |
| **`z_ndc`** | ❌ **non — affine** (cf. §1.1) |

C'est ce qui rend l'early-Z possible : la profondeur se teste **avant** d'avoir calculé le dénominateur.

### 5.4 Structures

```cpp
struct RasterTriangle
{
    Vec2f v0, v1, v2;          // pixels, Y déjà flippé par ToRaster
    float z0, z1, z2;          // z_ndc ∈ [0,1] reverse-Z — interpolé AFFINEMENT
    float invW0, invW1, invW2; // 1/w — dénominateur perspectif
};
```

```cpp
#ifdef _DEBUG
// Sentinelle de vérification du contexte passé via void*.
// Parade au bug n°17 (L05) : static_cast<T*>(void*) réussit TOUJOURS.
inline constexpr uint32_t kFragmentContextMagic = 0xF2A9C0DEu;
#endif

struct FragmentContext
{
#ifdef _DEBUG
    uint32_t magic = kFragmentContextMagic;   // EN PREMIER : lu quel que soit le décalage en aval
#endif
    FrameBuffer* fb = nullptr;
    DepthBuffer* db = nullptr;
    Color        color{};

    float z0    = 0.f, z1    = 0.f, z2    = 0.f;  // z_ndc — AFFINE
    float invW0 = 0.f, invW1 = 0.f, invW2 = 0.f;  // dénominateur perspectif

    float depthDisplayRange = 100.f;   // debug seulement — PAS un paramètre de lentille
};

[[nodiscard]] LV3_FORCEINLINE FragmentContext* AsFragmentContext(void* userData) noexcept
{
    auto* ctx = static_cast<FragmentContext*>(userData);
    LV3_ASSERT(ctx != nullptr);
    LV3_ASSERT(ctx->magic == kFragmentContextMagic &&
               "FragmentContext : contexte incompatible derriere le void*");
    return ctx;
}
```

> **`magic` en PREMIER membre, pas en dernier.** Une sentinelle en fin de structure n'est lue correctement que si toute la structure était déjà bien alignée — c'est-à-dire précisément dans le cas où il n'y a pas de bug. À l'offset 0, elle est la première chose lue, quel que soit le décalage en aval.

> **Défaut `invW = 0.f` délibéré.** Si le remplissage est oublié, `den = 0` → `1/den = inf` → écran visiblement faux. **Un défaut qui échoue bruyamment vaut mieux qu'un défaut plausible.** `1.0f` produirait un rendu presque correct, donc un bug silencieux.

### 5.5 `nearPlane` / `farPlane` retirés de `FragmentContext`

Ils servaient à **reconstruire** une distance depuis `z_ndc`. `invW` la donne **directement** :

| | Via `near`/`far` | Via `invW` |
|---|---|---|
| Grandeur | distance reconstruite | distance **directe** (`w = d`) |
| Paramètres requis | 2 (couplage à la lentille) | **0** |
| Far infini (`1e30f`) | ❌ **cassé** — annulation catastrophique | ✅ inchangé |
| Orthographique | ❌ formule différente | ✅ cohérent |
| Précision au loin | dégradée | **erreur uniforme** |

> **Un champ ajouté pour contourner une information manquante doit être réexaminé le jour où l'information arrive.** Sinon il survit, diverge, et devient une seconde source de vérité.

`depthDisplayRange` reste, parce qu'il exprime autre chose : **« sur quelle distance j'étale ma rampe de gris »** — une préférence d'affichage, pas une propriété de la caméra.

### 5.6 Les fragments

```cpp
void ShadeFragment_Solid(int32_t x, int32_t y,
                          const BarycentricWeights& b, void* userData)
{
    auto* ctx = AsFragmentContext(userData);

    // 1. PROFONDEUR : affine, aucune correction
    const float z = b.w0 * ctx->z0 + b.w1 * ctx->z1 + b.w2 * ctx->z2;

    // 2. EARLY-Z : rejeter AVANT tout autre calcul
    if (!ctx->db->TestAndSet(x, y, z)) return;

    // 3. Le dénominateur n'est calculé QUE si le fragment survit
    ctx->fb->SetPixel(x, y, ctx->color);
}

// DEBUG — bandes de distance. Rend visible l'erreur perspective.
void ShadeFragment_LinearDepth(int32_t x, int32_t y,
                                const BarycentricWeights& b, void* userData)
{
    auto* ctx = AsFragmentContext(userData);

    const float z = b.w0 * ctx->z0 + b.w1 * ctx->z1 + b.w2 * ctx->z2;
    if (!ctx->db->TestAndSet(x, y, z)) return;

    // ── LA division perspective par pixel ──
    const float den  = b.w0 * ctx->invW0 + b.w1 * ctx->invW1 + b.w2 * ctx->invW2;
    const float dist = 1.0f / den;                 // distance VRAIE à l'œil

    const uint8_t g = uint8_t((std::fmod(dist, 1.0f)) * 255.0f);
    ctx->fb->SetPixel(x, y, MakeColor(g, g, g));
}

// DEBUG — rampe de profondeur avec gamma
void ShadeFragment_Depth(int32_t x, int32_t y,
                          const BarycentricWeights& b, void* userData)
{
    auto* ctx = AsFragmentContext(userData);

    const float z = b.w0 * ctx->z0 + b.w1 * ctx->z1 + b.w2 * ctx->z2;
    if (!ctx->db->TestAndSet(x, y, z)) return;

    const float den  = b.w0 * ctx->invW0 + b.w1 * ctx->invW1 + b.w2 * ctx->invW2;
    const float dist = 1.0f / den;
    const float t    = Saturate(dist / ctx->depthDisplayRange);

    const uint8_t g = uint8_t(std::pow(1.0f - t, 1.0f / 2.2f) * 255.0f);
    ctx->fb->SetPixel(x, y, MakeColor(g, g, g));
}
```

**L'ordre des trois blocs est la règle, pas un détail.** `den` après `TestAndSet` : un fragment occulté ne paie ni la multiplication ni la division.

> ⚠️ **Limite de l'early-Z, à documenter dès maintenant :** il devient invalide si un fragment peut *modifier* la profondeur (parallax) ou *rejeter* le pixel (alpha test / cutout). LV3 n'a ni l'un ni l'autre. Quand ça viendra, ce sera par un flag `EDepthWrite` — et `EDepthTest`, aujourd'hui en dette, en devient le propriétaire naturel.

### 5.7 Le protocole de validation — produire le défaut à volonté

Voir des bandes droites ne prouve rien tant qu'on n'a pas vu à quoi ressemble l'erreur. Un mode fautif **temporaire** :

```cpp
// FAUX EXPRÈS : interpolation AFFINE de la distance
const float dist = b.w0 * (1.0f/ctx->invW0)
                 + b.w1 * (1.0f/ctx->invW1)
                 + b.w2 * (1.0f/ctx->invW2);
```

| Mode | Observation sur un sol incliné |
|---|---|
| Correct | Bandes **rectilignes**, parallèles, **resserrées au loin** ✅ |
| Fautif | Bandes qui **plient** sur les diagonales, espacement uniforme ✅ *défaut reproduit* |

> **Pour valider une correction, il faut pouvoir produire le défaut à volonté.** Si les deux modes donnaient la même image, c'est que la version « correcte » ne fait rien.

**Observation confirmée :** l'épaisseur des bandes varie avec l'inclinaison de la caméra — `épaisseur ≈ 1/|d(dist)/d(pixel)|`. Un sol rasant comprime les bandes, un sol vu perpendiculairement les élargit. **C'est le comportement physique attendu**, et le mode fautif ne le reproduit pas.

---

## 6. Chantier C — Profondeur et granularité

### 6.1 C.1 — Audit du Z-buffer

`DepthBuffer` était déjà conforme. L'audit consistait à le **prouver**.

| Point | Attendu | État |
|---|---|---|
| `Clear()` | `0.0f` | ✅ |
| Test dans `TestAndSet` | `z > stored` (GREATER) | ✅ |
| Indexation | `y * width + x` — **légitime** : buffer propriétaire | ✅ |
| Ordre dans le fragment | test **avant** shading | ✅ |
| `Clear()` appelé chaque frame | dans `BeginFrame` | ✅ |
| `db.Width() == fb.Width()` | invariant asserté | ✅ |

**Le mode d'échec dangereux :** clear à `1.0f` + test `<`. **Ça affiche une image.** Sur un objet unique, rien ne paraît anormal — ça ne casse qu'avec deux surfaces qui se croisent.

```cpp
void Renderer::BeginFrame(FrameBuffer& fb, DepthBuffer& db)
{
    LV3_ASSERT(db.Width()  == fb.Width()  &&
               db.Height() == fb.Height() &&
               "DepthBuffer et FrameBuffer de tailles differentes — TestAndSet ecrira hors bornes");
    m_ctx.fb = &fb;
    m_ctx.db = &db;
    db.Clear();
}
```

> **Une assertion qu'on n'a jamais vue échouer n'est pas prouvée.** Le protocole de validation a été : retirer délibérément le `Resize` du redimensionnement → l'assertion doit sauter. Sans cette étape, on teste que le code marche, pas que le filet attrape.

### 6.2 C.2 — Exploiter `EIntersect::Inside`

C'est la réponse à l'objection « le backface culling arrive trop tard ». Le bon levier n'est pas de déplacer un test par triangle : c'est de **changer de granularité**.

| Granularité | Un test rejette | Coût |
|---|---|---|
| `Classify` sur l'AABB | **tout le mesh** | 6 produits scalaires, une fois |
| `NearDistance` par face | 1 face | `vpf` soustractions × faceCount |

```cpp
const EIntersect vis = view.frustum.Classify(
                          mesh->GetMeshAABB().Transformed(modelMatrix));
if (vis == EIntersect::Outside) continue;

// Inside ⇒ l'AABB monde est ENTIÈREMENT dans les 6 plans, donc entièrement
// devant le plan near. Aucun de ses triangles ne peut le traverser :
// le clipping est structurellement inutile, pas seulement improbable.
const bool needsNearClip = (vis == EIntersect::Intersect);
```

**Pourquoi c'est exact et non heuristique :** `Frustum::Build` est appelée avec `reverseZ = true`, donc le plan `w − z ≥ 0` **est** l'un des plans du frustum. Si `Classify` retourne `Inside`, tous les sommets du mesh sont du bon côté du near **par déduction**, pas par mesure.

> ⚠️ **Condition de validité :** l'AABB doit être dans le **même espace** que les plans. `Transformed(modelMatrix)` donne une AABB monde, `Build(viewProjectionMatrix)` donne des plans monde. Cohérent. Un jour où les plans passeraient en espace objet, l'AABB devrait y rester aussi.

#### Mesure sur scène réelle (2 vues, 2 meshes)

```
[CULL] 60 frames, 2 vues/frame
   vue 0 : 120 tests | Inside 0..100% | Intersect 0..95% | Outside 0..100%
   vue 1 : 120 tests | Inside 100%    | Intersect 0%     | Outside 0%
```

- **Vue 1** (`Overview_Camera`, fixe) : `Inside 100%` en permanence → C.2 s'applique à toute sa géométrie, tout le temps.
- **Vue 0** (caméra animée) : `Intersect` monte à **95 %** → **le clipper s'exerce réellement, en continu**. Ce n'est pas un chemin de code théorique validé une fois en unitaire.

**Contrôle de non-régression :** forcer `needsNearClip = true` doit produire une image **pixel pour pixel identique**. ✅ validé.

---

## 7. Ordre canonique de la frame

```
1.  BuildViewData()                    → V, P, VP, frustum
2.  clear color + depth à 0.0f         (BeginFrame)
3.  SDL_LockTexture une fois           → FrameBuffer::Bind
4.  pour chaque mesh :
      a. Frustum::Classify(AABB monde)      → Outside : skip
      b. needsNearClip = (vis == Intersect)      ← Chantier C.2
      c. pour chaque face :
           i.   transformer les vpf sommets en CLIP SPACE
           ii.  si needsNearClip : d[k] = NearDistance ; allOut → skip
           iii. éventail en clip space :
                  - si allIn  : émission directe
                  - sinon     : ClipTriangleNear → 0..2 triangles   ← Chantier A
           iv.  pour chaque triangle émis :
                  - division ÷w → NDC, invW conservé
                  - Viewport::ToRaster (flip Y)
                  - IsBackFacing(EdgeFunction) → rejet
                  - RasterizeTriangle
                       · z_ndc affine → EARLY-Z GREATER             ← Chantier C.1
                       · dénominateur invW → varyings               ← Chantier B
5.  SDL_UnlockTexture / RenderCopy / RenderPresent
```

**Deux ordonnancements à ne pas inverser :**

- **Le culling de mesh avant le clipping.** C'est tout le gain du troisième état.
- **Le back-face culling après la division.** Le signe de l'aire en espace écran est gratuit (l'aire sert déjà aux barycentriques) et gère correctement le scale négatif et les miroirs.

---

## 8. Le back-face culling — analyse et décision révisée

### 8.1 Pourquoi PAS en espace objet

Le seul test qui rejette **avant** de payer la MVP :

```cpp
bool isBackFace = dot(N, camPosObj - v0) <= 0.0f;   // 1 produit scalaire
```

Mathématiquement exact, trois ordres de grandeur moins cher qu'une MVP. **Alors pourquoi n'est-ce pas la norme ?**

| Coût caché | Détail |
|---|---|
| **Mémoire** | Une normale de face **par triangle** : +12 o/triangle. 1,2 Mo sur 100k triangles, traversant le cache chaque frame |
| **Scale négatif** | Un miroir inverse le winding sans inverser la normale stockée → test faux, silencieusement |
| **Cache de sommets** | Voir ci-dessous — c'est l'argument décisif |
| **Duplication de vérité** | Le winding encodé à deux endroits (ordre des indices *et* normale) |

**L'argument du cache de sommets.** Sur un mesh indexé, un sommet est partagé par ~6 triangles et transformé **une seule fois**. Or sur un mesh fermé, un sommet de silhouette appartient **à la fois** à des faces avant et arrière.

| Scénario | Transformations économisées |
|---|---|
| Sphère fermée indexée | **≈ 40 %** |
| Terrain / plan / UI | **0 %** |
| Soupe de triangles | ≈ 50 % |

C'est pourquoi **tous les GPU transforment d'abord et cullent ensuite** : le vertex shader tourne sur tous les sommets, le culling se fait au primitive assembly sur l'aire signée.

### 8.2 Règle 19 — révisée et rétrogradée

`ClipOrientation` (déterminant 3×3 sur `(x,y,w)`) permettrait de culler **avant les divisions** :

```
Coût   : 14 opérations sur 100 % des triangles
Gain   : (3 div + 3 ToRaster) × 50 % ≈ 30 cycles
```

**Match nul, dans le bruit de mesure.** Et deux coûts non arithmétiques :

- Un **second signe d'orientation à calibrer** (déterminant clip et aire raster n'ont pas le même signe à cause du flip Y). Deux vérités sur le même concept.
- Le déterminant n'est valide que si les trois `w` sont positifs → uniquement sur le chemin rapide. **Deux tests de culling coexistants.**

> **Règle 19 révisée — Un seul test de backface : le signe de l'aire en espace raster.** `ClipOrientation` est un **chantier différé sous condition de profilage**.

### 8.3 Le vrai levier : la granularité

| Niveau | Granularité | Rejette d'un coup |
|---|---|---|
| Frustum culling (AABB) | objet | tout le mesh |
| **Cluster cone culling** | meshlet de 64–128 triangles | **128 triangles en 1 test** |
| Back-face culling | triangle | 1 triangle |

Le *cone culling* (Nanite, pipelines GPU-driven) précalcule un cône englobant les normales d'un groupe de 128 triangles. Un produit scalaire rejette les 128, **avant toute transformation**. **C'est ça, « culler plus tôt »** — pas déplacer un test par triangle de trois nanosecondes.

---

## 9. Règles dictatoriales — état consolidé

| # | Règle | Statut |
|---|---|---|
| 9 | Le clipping se fait en **clip space**, avant la division par `w` | ✅ |
| 10 | **Un seul plan clippé : near.** Latéraux = scissor (`ClampBox`), far inexistant | ✅ |
| 11 | Le test near s'écrit **`w − z ≥ 0`**, forme canonique. **Jamais avec un paramètre de lentille** — `w <= nearPlane` est faux en orthographique | 🔄 renforcée |
| 12 | `ClipTriangleNear` retourne **jusqu'à 4 sommets**. On **triangule avant** de clipper : borne fixe, aucune hypothèse de convexité | 🔄 révisée |
| 13 | L'interpolation au clipping est **linéaire en clip space** (exacte). À ne pas confondre avec la correction perspective | ✅ |
| 14 | Tout varying s'interpole en `a·invW` divisé par `invW` interpolé. **Sauf `z_ndc`, qui est affine** — et c'est ce qui rend l'early-Z possible | ✅ |
| 15 | La pré-division `a·invW` se fait au **triangle setup**, jamais dans la boucle pixel | ✅ |
| 16 | Z-buffer : clear **`0.0f`**, test **`GREATER`**. Les deux vérifiés ensemble | ✅ |
| 17 | **Early-Z** : le test de profondeur précède tout autre calcul du fragment, dénominateur inclus | ✅ |
| 18 | `DepthBuffer` possède sa mémoire → `y*width+x`. La règle du `pitch` ne vaut que pour `FrameBuffer` | ✅ |
| **19** | **Un seul test de backface : le signe de l'aire raster.** `ClipOrientation` = chantier différé sous condition de profilage | 🔄 rétrogradée |
| **20** | Le choix de l'algorithme de clipping suit la **topologie de sortie** : polygone → Sutherland-Hodgman 4D ; segment → Liang-Barsky / Cyrus-Beck | 🆕 |
| **21** | **Un seul type derrière le `void*`**, avec `magic` en premier membre, vérifié en Debug. Étendre `FragmentContext`, jamais en créer un second | 🆕 |
| **22** | Quand un test par élément coûte trop cher, **changer de granularité** plutôt que déplacer le test | 🆕 |
| **23** | Un point d'intersection **partagé entre deux primitives** doit être calculé dans un **ordre canonique**, indépendant du sens de parcours | 🆕 |
| **24** | La fonction d'arête s'écrit sous forme **antisymétrique**, le point testé servant de référence : `E(b,a,p) = −E(a,b,p)` exactement | 🆕 |
| **25** | Le `#ifdef` conditionne le **corps** d'une fonction, jamais sa **signature** — sinon divergence Debug/Release à l'édition de liens | 🆕 |

---

## 10. Suite de validation — tous validés ✅

| # | Test | Critère | Résultat |
|---|---|---|---|
| **1** | Caméra à moins de `near` d'une grande face | Coupée, pas supprimée. Aucun `NaN` | ✅ |
| **2** | `ClipTriangleNear` — 4 configurations + copie bit à bit du chemin rapide | 3 devant → `n==3` identique · 3 derrière → `0` · 1 devant → `3` · **2 devant → `4`** | ✅ |
| **3** | Winding préservé après clipping | Même signe que le triangle source | ✅ |
| **4** | Deux quads croisés en X, **dans les deux ordres de soumission** | `gauche_faux=0 droite_faux=0 vides=0` | ✅ |
| **5** | `ERenderMode::Depth` | **Clair PRÈS**, sombre au loin | ✅ |
| **6** | `LinearDepth` sur sol incliné + **mode fautif de comparaison** | Bandes rectilignes, resserrées au loin ; le mode fautif plie | ✅ |
| **7** | Damier sur quad synthétique | — | ⏳ reporté au chantier UV |
| **8** | `far: 30` fini vs infini, **même valeur de `far`** | Coupure présente puis **disparue** | ✅ |
| **9** | `MeasureQuadCoverage` **après clipping** | `doubles=0 trous=0` | ✅ |
| **10** | `RasterizeTriangle` — boîtes vides + **contre-exemple positif** | Rejets à 0 fragment, triangle valide > 0 | ✅ |

### Le test 4 — pourquoi les deux ordres

Le Z-buffer doit trancher, **pas l'ordre de soumission**. Un test à sens unique pourrait passer par coïncidence. Les deux ordres donnant le même résultat, c'est la définition opérationnelle d'un Z-buffer qui fonctionne.

### Le test 10 — pourquoi un contre-exemple positif

Une suite qui ne vérifie que des **rejets** serait entièrement satisfaite par `void RasterizeTriangle(...) { return; }`. Le cas « triangle valide → au moins 1 fragment » est ce qui donne du sens aux cinq cas négatifs.

### Le test 9 — le seul qui attrape les fissures

Une fissure d'un pixel ne crashe pas, ne produit pas de `NaN`, et se perd dans le bruit d'une scène animée. **Il faut compter, pas regarder.** Le critère du trou — *un pixel vide dont les quatre voisins sont pleins* — rend le test insensible aux bords : seules les fissures intérieures sont comptées.

---

## 11. Journal des bugs

| # | Bug | Détecté par | Cause |
|---|---|---|---|
| **19** | Champ ajouté à `FragmentContext`, oublié dans `DrawTriangle` | écran noir uniforme (`1/0 = inf`) | Structure remplie ailleurs qu'à sa déclaration |
| **20** | **Backface culling inversé depuis la L05** | mode `Depth` (couleur à sens géométrique) | Flip Y du viewport : front-face = aire **négative** |
| **21** | **124 trous sur les coutures du clipping** | `Test_ClipCoverage` (comptage) | Intersection calculée dans deux sens différents |
| **22** | `EdgeFunction` dédoublée (surcharge modifiée sans le noyau) | audit du dépôt | Délégation coupée → deux formules |
| **23** | `ParseCamera` : `out_activeCamera != NULL_ENTITY;` | audit du dépôt | Comparaison au lieu d'affectation (warning C4553) |
| **24** | `ShadeFragment_Barycentric` contourne `AsFragmentContext` | audit du dépôt | `static_cast` direct → parade du bug n°17 désactivée |
| **25** | FPS + Follow actifs simultanément sur la même caméra | dump des contrôleurs | Exclusion promise par un commentaire, jamais implémentée |
| **26** | Clés JSON écrites mais jamais lues (`active`/`priority` dans `CameraFPS`) | `JsonReader::WarnUnread` | Aucun avertissement sur les clés inconnues |
| **27** | `SDL_LockTexture` « Invalid call » au redimensionnement | test de resize | Texture recréée pendant qu'une frame est en cours |

### Structures à obligation invisible

Trois structures du moteur sont remplies **ailleurs** que là où elles sont déclarées. Un champ ajouté sans mettre à jour le site de remplissage ne produit **aucune erreur de compilation** :

| Structure | Site de remplissage |
|---|---|
| `ClipVertex` | `Lerp()` — un attribut oublié vaut zéro **uniquement sur les triangles clippés** |
| `RasterTriangle` | `EmitClipTriangle` |
| `FragmentContext` | `Renderer::DrawTriangle` |

**Parade :** un invariant asserté, puisque `w > 0` est garanti après clipping.

```cpp
LV3_ASSERT(ctx.invW0 > 0.0f && ctx.invW1 > 0.0f && ctx.invW2 > 0.0f
           && "invW non rempli — champ oublie dans DrawTriangle ?");
```

---

## 12. Principes méthodologiques dégagés

> **1. « Ça s'affiche » n'est jamais un critère de validation.** Il faut une sortie dont la valeur a un **sens géométrique vérifiable**. Un cube rendu à l'envers ressemble à un cube ; un damier tordu ressemble à un damier.

> **2. Pour rendre visible une erreur continue, il faut un signal discontinu.** Un dégradé cache les petites erreurs ; une grille, un damier ou des bandes les amplifient. C'est pourquoi `LinearDepth` utilise `fmod`.

> **3. Compter, pas regarder.** Deux fois de suite (top-left rule, coutures du clipping), la mesure a trouvé ce que l'œil ne pouvait pas voir. Et la **localisation** des trous a désigné la cause avant qu'on ouvre le code.

> **4. Pour valider une correction, il faut pouvoir produire le défaut à volonté.** Le mode `LinearDepth_WRONG` a prouvé que la version correcte fait quelque chose.

> **5. Mesurer avant de garder.** La bissection `ClipLess` / `EdgeFunction` a prouvé que les deux correctifs étaient nécessaires — pas un de trop, pas un de moins.

> **6. Une assertion qu'on n'a jamais vue échouer n'est pas prouvée.** Retirer délibérément le `Resize` pour vérifier que l'assertion saute.

> **7. Quand un algorithme oblige à écrire une passe de réparation derrière lui, c'est qu'il ne calculait pas ce qu'on voulait.** La retriangulation manuelle après Cyrus-Beck était la reconstruction topologique que l'algorithme avait détruite.

> **8. Comparer la topologie de la sortie AVANT de comparer les performances.** Un algorithme plus rapide qui produit la mauvaise structure coûte la différence — et plus, en bugs de recollage.

> **9. Une donnée écrite dans un fichier de configuration et jamais lue est un mensonge que rien ne compile.** Le code a le compilateur ; les données n'ont que ce qu'on construit.

> **10. Quand une information doit être répétée à deux endroits sans vérification automatique, il faut soit la dériver, soit l'asserter.** `JsonReader` fait le premier, `LV3_ASSERT(invW > 0)` le second.

---

## 13. Dettes ouvertes

| Dette | Impact |
|---|---|
| `MeshClass::GetFaceView` — UV et normales inexploitables | Bloque le test 7, le texturing, l'éclairage |
| `ClipVertex` sans `uv` / `normal` | À ajouter **avec** `Lerp()`, sous peine du bug fantôme |
| `ECullMode` / `EDepthTest` / `EBlendMode` non exploités par `Renderer` | `EDepthTest` deviendra propriétaire du flag `EDepthWrite` (alpha test) |
| `ClipOrientation` — culling avant les divisions | Chantier différé, sous condition de profilage |
| `TriggerSystem` en O(N²) | Broad-phase spatiale (grille ou quadtree) avant la narrow-phase sphère/sphère |
| Bugs 23, 24, 25 du journal | Non corrigés à la clôture de cette leçon |

---

## 14. Prochaine étape

- **Chantier UV/normales** : corriger `MeshClass::GetFaceView`, étendre `ClipVertex` **et `Lerp()`**, valider par le test 7 (damier sur quad incliné à 80°)
- **Leçon 06** : texturing, échantillonnage, filtrage — le premier vrai consommateur de l'interpolation perspective-correcte
