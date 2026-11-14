# Avenant A12a — Pipeline de rendu après les chantiers 3b et 2b

> **Statut** : schéma de référence au 28/09/2026 (Référence H), complété le 30/09/2026 par le chantier 3a.
> **Rattachement** : annexe A12 (`Annexe_A12_Pipeline_Rendu.md`, état après le chantier 2) ; avenants C3 (chantier 3b) et C4 (chantier 2b).
> **Nature** : cet avenant **remplace** les § 2 à 7 de l'annexe A12. Le principe d'organisation de A12 § 1 (classement par fréquence d'exécution) reste en vigueur, inchangé.

---

## 1. Ce qui a changé depuis l'annexe A12

| Chantier | Bande touchée | Changement |
|---|---|---|
| **3b** (avenant C3) | triangle | rejet exact, avant le rasterizer, de tout triangle dont la boîte serrée ne contient aucun centre de pixel. 99,7 % des triangles de la ceinture ne vont plus au rasterizer |
| **2b** (avenant C4) | sommet / triangle | pour les instances `Inside`, la division par w et `ToRaster` remontent de la bande triangle (par coin) à la bande sommet (par sommet). La bande sommet a désormais **deux branches** |
| **3a** (avenant C4 § 14) | pixel | la boucle pixel ne parcourt plus que la **boîte serrée** : même `TightPixelBox` que le rejet 3b. Pixels testés : 14 372 → 10 562 par frame. Image identique |
| Instrumentation | instance × vue | mesure de la taille apparente (`LV3_LOD_STATS`), au point exact où le LOD choisira le mesh |

Rappel de la règle de fréquence (A12 § 1) :

> Un calcul s'exécute au niveau le **moins fréquent** où son résultat est encore constant. Le descendre d'un niveau multiplie son coût par le rapport des cardinalités.

---

## 2. Le schéma

Cardinalités : scène `solar_system_v1compat_belt.json`, 4 vues, 1536×900, Référence H (i7-1265U, P-core). Valeurs médianes par frame.

```
NIVEAU            CARDINALITÉ          ÉTAGE                                          DONNÉES
──────────────────────────────────────────────────────────────────────────────────────────────────────────────
par frame         ×1                   Clean_Render + Renderer::BeginFrame            → FrameBuffer, DepthBuffer effacés
                                              │ ViewData
par vue           ×4                   RenderView : SetViewport · SetMode ·
                                       SetDepthDisplayRange
                                       [LV3_LOD_STATS] MakeScreenSizeParams
                                              │ MeshClass*, model
par instance      2 580 testées        Filtre m_hideForCamera + frustum         ──Outside──► rejet (1 283)
× vue             1 297 visibles       Constantes : mvp, vis, teinte, garde nVerts
                                       [LV3_LOD_STATS] rayon apparent → tranches   [① LOD : choisir le mesh ICI]
                                              │
                          ┌─────────── vis == Inside ───────────┬─────────── vis == Intersect ───────────┐
                          │ 269 092 faces (η = 0,976)            │ 5 896 faces                              │
par SOMMET        181 061 │ ProjectPositions                     │ TransformPositions                       │
  (toutes                 │   MulRow + 1/w + ToRaster            │   MulRow                                 │
   branches)              │   ──► RasterSpaceBuffer (pixels)     │   ──► ClipSpaceBuffer (espace clip)      │
                          │   [④ AVX2 ICI]                       │                                          │
                          │          │ RasterVertex[n]           │          │ Vec4f[n]                      │
par face          274 988 │ lit 3 RasterVertex par indice        │ cv[k] = clip[indices[..]]                │
                          │ aucune division                      │ test near : allOut → rejet               │
                          │          │                           │          │ allIn           │ mixte       │
par triangle              │          │                           │          │           ClipTriangleNear   │
                          │          │                           │          │           (3 ou 4 sommets,   │
                          │          │                           │          │            éventail)         │
                          │          │                           │   EmitClipTriangle : ProjectClip × 3     │
                          │          │                           │   (par COIN, APRÈS clipping)             │
                          └──────────┴───────────┬───────────────┴──────────────────────────────────────────┘
                                                 ▼
                  EmitRasterTriangle (aval UNIQUE, LV3_FORCEINLINE)
                  ├─ backface : EdgeFunction(x, y)                      ──dos──► rejet (≈ 50 %)
                  ├─ TrisRasterized                        134 389
                  ├─ rejet 3b : TightPixelBox().Empty()  ──vide──► TrisEarlyRejected 134 003
                  │     (sauf Wireframe)
                  └─ DrawTriangle                          ≈ 386 triangles
                                                 │ RasterTriangle
                  RasterizeTriangle : setup (aire, TightPixelBox ∩ viewport, top-left)
                                                 │
par pixel         10 562 testés        3 EdgeFunction + ShadeFragment_*       ──► TestAndSet + SetPixel
                  (= boîte serrée)
                  4 562 couverts                                                   (profondeur : presque jamais lue)
```

Lecture du schéma :

- **La bifurcation se décide une fois par (instance, vue)**, par la classification du frustum, qui existait déjà. Aucun test n'est ajouté dans les bandes inférieures.
- **Les deux branches se rejoignent dans `EmitRasterTriangle`.** Tout ce qui est en dessous ignore d'où vient le triangle.
- **Le rasterizer ne reçoit plus qu'environ 386 triangles par frame.** Le coût de `Render` est désormais presque entièrement **en amont** de `DrawTriangle` : étage sommets, boucle des faces, backface et boîte serrée.

---

## 3. Fiche par étage

| Étage | Fichier | Entrée → sortie | Invariant |
|---|---|---|---|
| Clear | `main.cpp`, `Renderer.h` | → tampons effacés | un seul effacement par frame, avant la 1ʳᵉ vue |
| État de vue | `RenderSystem.cpp` | `ViewData` → état du `Renderer` | callback de fragment résolu par vue ; `depthDisplayRange` par vue |
| Filtre + frustum | `RenderSystem.cpp`, `Frustum.cpp` | AABB monde → Inside / Intersect / Outside | une caméra ne voit jamais son propre gizmo |
| Constantes d'instance | `RenderSystem.cpp` | model, VP → `mvp`, `vis` | `nVerts ≤ Capacity()` pour les **deux** tampons |
| Taille apparente (comptage) | `ViewData.h`, `RenderSystem.cpp` | AABB locale, `mvp` → rayon en pixels, tranche | sous `LV3_LOD_STATS` uniquement ; une seule formule perspective / ortho |
| **ProjectPositions** (Inside) | `VertexStage.cpp` | `Vec3f[n]` → `RasterVertex[n]` en pixels | **w > 0 pour chaque sommet**, assertion ; boucle sans branche ; `mvp` et viewport copiés en local |
| TransformPositions (Intersect) | `VertexStage.cpp` | `Vec3f[n]` → `Vec4f[n]` espace clip | aucune division : le clipping a besoin de l'espace clip |
| Assemblage Inside | `RenderSystem.cpp` | indices → 3 `RasterVertex` | `LV3_ASSERT(idx < nVerts)` ; aucune arithmétique |
| Assemblage Intersect + test near | `RenderSystem.cpp` | indices → `ClipVertex cv[3]`, `allIn` / `allOut` | le chemin Intersect teste toujours le near |
| Clipping near | `Clipper.cpp` | 3 sommets → 0, 3 ou 4 | après clipping, w > 0 garanti ; `Lerp` couvre tous les attributs ; `ClipLess` canonique |
| **ProjectClip** | `VertexStage.h` | `Vec4f` clip → `RasterVertex` | **seul endroit du moteur où l'on divise par w** ; appelé par les deux branches, donc image identique au bit près |
| **EmitRasterTriangle** | `RenderSystem.cpp` | 3 `RasterVertex` → `RasterTriangle` ou rejet | aval unique : backface (face avant = aire négative en raster), `TrisRasterized` après backface, rejet 3b sauf Wireframe |
| Setup raster | `Rasterizer.cpp` | triangle → **boîte serrée**, top-left, `invArea` | ne reçoit que des triangles dont la boîte serrée est non vide ; `TightPixelBox` est la **seule** définition des pixels candidats (3b et 3a) |
| Pixel | `Rasterizer.cpp`, `Fragment.cpp` | (x, y, bary) → profondeur + couleur | ne parcourt que la boîte serrée : `PixelsTested` = `PixelsTight` à chaque frame ; 3 `EdgeFunction` complètes par pixel (incrémentales → L13) |

---

## 4. Données : propriétaire et durée de vie

| Donnée | Type | Propriétaire | Durée de vie | Accès |
|---|---|---|---|---|
| `vertexPositions`, `indices` | `Vec3f`, `uint32_t` | `MeshClass` via `ResourceManager` | tout le programme | lecture seule |
| **`RasterSpaceBuffer`** | `RasterVertex` (16 o) | `main.cpp` | contenu valide **pendant une instance × vue** | écrit par `ProjectPositions`, lu par la boucle des faces Inside |
| `ClipSpaceBuffer` | `Vec4f` (16 o) | `main.cpp` | idem | écrit par `TransformPositions`, lu par la boucle des faces Intersect |
| `cv[4]`, `poly[4]` | `ClipVertex` | pile | une face | chemin Intersect uniquement |
| `RasterTriangle` | struct | pile | un triangle | construit par `EmitRasterTriangle` |
| `FragmentContext` | struct | `Renderer` | une vue (état) + un triangle | mis à jour en place |
| `FrameBuffer`, `DepthBuffer` | tampons | `main.cpp` | le programme ; effacés par frame | test + écriture par pixel |

Pour une roche, la boucle des faces lit 117 × 16 o = **1,8 Kio** de `RasterVertex` : tout reste en L1.

**Invariant de durée de vie (étendu aux deux tampons) :** le contenu est écrasé par l'instance suivante. Tout étage **différé** futur (tri de transparence, rendu par tuiles, file de triangles) devra **copier** ses sommets, jamais les référencer. En multithread : une paire de tampons par thread, jamais partagée.

```cpp
// ✗ Contre-exemple : conserver un RasterVertex au-delà de l'instance
const RasterVertex* saved = rasterBuf.Data() + i;   // instance A
// ... instance B : ProjectPositions réécrit le tampon
Use(*saved);                                         // lit un sommet de B : ni crash, ni assert
```

**Deux types distincts pour deux espaces.** Un `Vec4f` du `ClipSpaceBuffer` est en espace clip, un `RasterVertex` est en pixels. Ils ont la même taille, mais ne sont jamais interchangeables : c'est le compilateur qui le garantit.

---

## 5. Application de la règle de fréquence

| Calcul | Niveau correct | État |
|---|---|---|
| Choix du callback de fragment | vue (`SetMode`) | fait |
| `kPx`, pente de w (taille apparente) | vue | fait (comptage) |
| `mvp`, teinte, classification frustum | instance × vue | fait |
| Choix de la branche Inside / Intersect | instance × vue | **fait (2b)** |
| Transformation des positions | sommet | fait (chantier 2) |
| Division par w + `ToRaster`, instances Inside | sommet | **fait (2b)** |
| Division par w + `ToRaster`, instances Intersect | coin, **après** clipping | **correct tel quel** : les sommets clippés n'existent qu'au niveau triangle |
| Backface, boîte serrée | triangle | fait |
| Rejet des triangles sans pixel | triangle, **avant** le setup | **fait (3b)** |
| Bornes de la boucle pixel | triangle (boîte serrée, calculée une fois) | **fait (3a)** |
| Règle top-left, `invArea` | triangle | fait |
| Fonctions d'arête (affines) | valeur de départ par triangle, incrément par pixel | 3 évaluations complètes par pixel → **L13** |
| Choix du mesh selon la taille à l'écran | instance × vue, avant l'étage sommets | absent → **chantier 1 (LOD)** |

La ligne « Intersect » montre qu'appliquer la règle de fréquence ne signifie pas tout remonter : un calcul reste au niveau où **ses entrées existent**.

---

## 6. Points d'extension

| Repère | Chantier | Bande | Effet attendu |
|---|---|---|---|
| ① | **LOD** (suivant) | instance × vue, entre le frustum et la bifurcation | réduit toutes les bandes inférieures d'un coup ; la mesure de taille apparente est déjà à cet endroit |
| ②b | ~~projection par sommet~~ | ~~sommet~~ | **fait**, −33,5 % sur `Render` (avenant C4) |
| ③ | ~~rejet sub-pixel~~ | ~~triangle~~ | **fait**, −29 % sur `Render` (avenant C3) |
| ④ | **AVX2** | sommet, **`ProjectPositions`** | cible déplacée : `TransformPositions` ne sert plus qu'à 2,4 % des faces. Division exacte obligatoire (`vdivps`, jamais `vrcpps`) |
| ~~3a~~ | ~~boîte serrée dans la boucle pixel~~ | ~~pixel~~ | **fait**, mise en cohérence exacte, aucun gain revendiqué (avenant C4 § 14) |
| L13 | fonctions d'arête incrémentales | pixel | à reconsidérer : 10 562 pixels testés par frame sur la ceinture ; elles partiront du coin de la boîte serrée |

### 6.1 Contraintes que le LOD devra respecter à ce point d'insertion

- **L'AABB du culling doit englober tous les niveaux** (annexe A13 § 7.2). Sinon, un sommet d'un niveau grossier pourrait sortir d'une AABB classée `Inside`, et le contrat w > 0 de `ProjectPositions` ne serait plus garanti par construction.
- `FacesSubmitted`, `VertsTransformed` et `FacesInside` doivent compter les faces et sommets **du niveau choisi**, pour que la loi de coût reste lisible.
- Le niveau choisi n'est jamais stocké dans un composant : il est calculé et consommé dans la même (instance, vue).

---

## 7. Correspondance avec un GPU

| Étage GPU | LibraryV3 |
|---|---|
| Input assembler | `MeshClass` + boucle des faces |
| Vertex shader | `ProjectPositions` (Inside) / `TransformPositions` (Intersect) |
| Post-transform cache | `RasterSpaceBuffer` / `ClipSpaceBuffer` (complets par instance, pas LRU) |
| Primitive assembly | lecture de 3 sommets par indice |
| Clipping | `ClipTriangleNear` (plan near seul), chemin Intersect uniquement |
| Perspective divide + viewport transform | `ProjectClip` : par sommet (Inside) ou par coin après clipping (Intersect) |
| Culling | frustum par instance ; backface par triangle ; rejet des triangles sans pixel (3b) |
| Rasterizer setup | début de `RasterizeTriangle` |
| Pixel shader + output merger | `ShadeFragment_*` + `TestAndSet` |

**Rapprochement :** un GPU évite de clipper la plupart des triangles grâce à la *guard band*, une zone de rasterisation plus large que l'écran, et ne clippe que ceux qui traversent vraiment le plan near. La classification `Inside` de LV3 joue le même rôle à l'échelle d'une instance entière : elle **prouve** qu'aucun sommet de l'instance n'a besoin du clipping, ce qui autorise la division par sommet.

---

## 8. Budget de la frame, Référence H

| Zone | Médiane | Part de la frame |
|---|---|---|
| **Render** | **5,14 à 5,16 ms** | ≈ 700 ‰ |
| Clear | ≈ 1,09 ms | ≈ 150 ‰ |
| Present (SDL, hors moteur) | ≈ 0,92 ms | ≈ 125 ‰ |
| Tout le reste (entrées, animation, transformations, caméras, triggers, vues) | ≈ 0,12 ms | ≈ 16 ‰ |
| **FRAME** | **7,33 à 7,37 ms** | 1000 ‰ (≈ 136 fps) |

`Render` ≈ **18,7 ns × `FacesSubmitted`**. La seule variable qui reste est le nombre de faces soumises : c'est le travail du chantier 1 (LOD).

---

*Visuel associé : « Pipeline LV3 » (étages cliquables), à mettre à jour avec la bifurcation Inside / Intersect.*
