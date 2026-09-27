# Annexe A12 — Pipeline de rendu, classé par fréquence d'exécution

> **Statut** : schéma de référence après le chantier 2 (27/09/2026).
> **Rattachement** : Avenant C2 — Chantier 2 ; remplace toute description antérieure du parcours de `RenderView`.
> **Visuel associé** : artifact « Pipeline LV3 » (étages cliquables).

---

## 1. Principe d'organisation

Le pipeline n'est pas décrit par ordre d'appel, mais par **niveau de fréquence** : chaque étage appartient à la bande dont il partage le nombre d'exécutions.

> **Règle de fréquence** — un calcul s'exécute au niveau le **moins fréquent** où son résultat est encore constant. Le descendre d'un niveau multiplie son coût par le rapport des cardinalités.

Le chantier 2 n'a rien ajouté au calcul : il a créé la bande **sommet**, qui n'existait pas, et y a fait remonter `MulRow`, qui vivait dans la bande **face** (÷ 4,56).

---

## 2. Le schéma

Cardinalités : scène `solar_system_v1compat_belt.json`, 4 vues, 1536×900, Référence F (i7-1265U, P-core).

```
NIVEAU            CARDINALITÉ        ÉTAGE                                        DONNÉES
─────────────────────────────────────────────────────────────────────────────────────────────────────────
par frame         ×1                 Clean_Render + Renderer::BeginFrame          → FrameBuffer, DepthBuffer (effacés)
                                            │ ViewData
par vue           ×4                 RenderView : SetViewport · SetMode ·
                                     SetDepthDisplayRange
                                            │ MeshClass*, model
par instance      2 580 testées      Filtre (m_hideForCamera) + frustum     ──Outside──► rejet (1 283)
× vue             1 297 visibles     Constantes : mvp = model × VP,               [① LOD : choisir le mesh ici]
                                     needsNearClip, teinte, garde nVerts
                                            │ mvp, n
par SOMMET        181 061            TransformPositions                     ◄── MeshClass.vertexPositions (lecture)
  (nouveau)                            dst[i] = MulRow(m, src[i])           ──► ClipSpaceBuffer[n]  (écrit)
                                     [④ AVX2 ici]  [②b division + ToRaster ici, instances Inside]
                                            │ Vec4f[n]
par face          274 988            Assemblage : cv[k] = clip[indices[..]] ◄── ClipSpaceBuffer (lit 3 / face)
                                     rejet near allOut (si Intersect)       ◄── MeshClass.indices
                                        │ Intersect              │ Inside (allIn)
par triangle      134 389 émis       ClipTriangleNear ──► EmitClipTriangle ──► Setup raster
                                     (0/3/4 sommets,      1/w, ToRaster ×3,    aire, flip, bbox ∩ vp,
                                      éventail)           backface             top-left ×3
                                                                               [③ rejet sub-pixel ici]
                                            │ RasterTriangle
par pixel         non compté         3 EdgeFunction + ShadeFragment_*       ──► TestAndSet + SetPixel
                                                                                (profondeur 5,5 Mo, hors L2)
```

---

## 3. Fiche par étage

| Étage | Fichier | Entrée → sortie | Invariant |
|---|---|---|---|
| Clear | `main.cpp`, `Renderer.h` | → tampons effacés | un seul effacement par frame, avant la 1ʳᵉ vue |
| État de vue | `RenderSystem.cpp` | `ViewData` → état du `Renderer` | callback de fragment résolu par vue ; `depthDisplayRange` par vue (R12) |
| Filtre + frustum | `RenderSystem.cpp`, `Frustum.cpp` | AABB monde → Inside / Intersect / Outside | une caméra ne voit jamais son propre gizmo |
| Constantes d'instance | `RenderSystem.cpp` | model, VP → `mvp`, `needsNearClip` | Inside ⇒ aucun triangle ne traverse le near ; `nVerts ≤ Capacity()` |
| **TransformPositions** | `Rendering/VertexStage.cpp` | `Vec3f[n]` → `Vec4f[n]` espace clip | boucle plate sans branche ; `mvp` copié en local (aliasing) |
| Assemblage | `RenderSystem.cpp` | indices + tampon → `ClipVertex cv[3]` | `LV3_ASSERT(vi < nVerts)` ; aucune multiplication |
| Clipping near | `Clipper.cpp` | 3 sommets → 0/3/4 | après clipping, w > 0 garanti ; `Lerp` couvre tous les attributs |
| EmitClipTriangle | `RenderSystem.cpp` | clip → `RasterTriangle` | seul endroit où l'on divise par w ; face avant = aire négative |
| Setup raster | `Rasterizer.cpp` | triangle → bbox, top-left, `invArea` | prix fixe par triangle, indépendant de sa surface |
| Pixel | `Rasterizer.cpp`, `Fragment.cpp` | (x, y, bary) → profondeur + couleur | 3 `EdgeFunction` complètes par pixel (incrémentales → L13) |

---

## 4. Données : propriétaire et durée de vie

| Donnée | Propriétaire | Durée de vie | Accès par le pipeline |
|---|---|---|---|
| `vertexPositions`, `indices` | `MeshClass` via `ResourceManager` | tout le programme | lecture seule |
| `ClipSpaceBuffer` | `main.cpp` (appelant de `RenderView`) | contenu valide **pendant une instance × vue** | écrit par l'étage sommets, lu par l'assemblage |
| `cv[4]`, `poly[4]`, `RasterTriangle` | pile | une face / un triangle | local |
| `FragmentContext` | `Renderer` | une vue (état) + un triangle (sommets) | mis à jour en place |
| `FrameBuffer`, `DepthBuffer` | `main.cpp` | le programme ; effacés par frame | test + écriture par pixel |

**Invariant de durée de vie** : le contenu du `ClipSpaceBuffer` est écrasé par l'instance suivante.

```cpp
// ❌ Contre-exemple : conserver un pointeur au-delà de l'instance
const Vec4f* saved = clipBuf.Data() + vi;   // instance A
// ... instance B : TransformPositions réécrit le tampon
Use(*saved);                                 // lit un sommet de B — ni crash, ni assert
```

Tout futur étage **différé** (tri de transparence, rendu par tuiles, file de triangles) devra **copier** ses sommets, jamais les référencer. En multithread : un `ClipSpaceBuffer` par thread, jamais partagé.

---

## 5. Application de la règle de fréquence

| Calcul | Niveau correct | État |
|---|---|---|
| Choix du callback de fragment | vue (`SetMode`) | fait |
| `mvp`, teinte, `needsNearClip` | instance × vue | fait |
| Transformation des positions | sommet | **fait (chantier 2)** |
| Division par w + `ToRaster`, instances `Inside` | sommet | encore par coin → **étape 2b** |
| Règle top-left, `invArea` | triangle | fait |
| Fonctions d'arête (affines) | valeur de départ par triangle, incrément par pixel | 3 évaluations complètes par pixel → **L13** |
| Choix du mesh selon la taille à l'écran | instance × vue, avant l'étage sommets | absent → **chantier 1 (LOD)** |

Condition de 2b : la division n'est permise par sommet **que** pour une instance `Inside` (tous les sommets devant le near, w > 0). Une instance `Intersect` garde le chemin actuel : clipping en espace homogène, division après.

---

## 6. Points d'extension

| Repère | Chantier | Bande | Effet attendu |
|---|---|---|---|
| ① | LOD | instance × vue | réduit **toutes** les bandes inférieures d'un coup |
| ②b | division + `ToRaster` par sommet | sommet | retire environ 3 ms isolées de la bande triangle (gain réel moindre : recouvrement) |
| ③ | rejet sub-pixel | triangle, avant le setup | évite le prix fixe du setup pour les triangles qui ne couvrent aucun pixel |
| ④ | AVX2 | sommet (`TransformPositions`) | ≤ 0,6 ms isolée : dernier de la liste |

Le compteur de **pixels couverts** (distribution 0 / 1 / 2–4 / > 4 par triangle) remplira la dernière case vide de la colonne « cardinalité » et départagera ②b et ③.

---

## 7. Correspondance avec un GPU

| Étage GPU | LibraryV3 |
|---|---|
| Input assembler | `MeshClass` + boucle des faces |
| Vertex shader | `TransformPositions` |
| Post-transform cache | `ClipSpaceBuffer` (complet par instance, pas LRU) |
| Primitive assembly | `cv[k] = clip[indices[…]]` |
| Clipping | `ClipTriangleNear` (plan near seul) |
| Perspective divide + viewport | `EmitClipTriangle` |
| Culling | frustum par instance, backface par triangle |
| Rasterizer setup | début de `RasterizeTriangle` |
| Pixel shader + output merger | `ShadeFragment_*` + `TestAndSet` |
