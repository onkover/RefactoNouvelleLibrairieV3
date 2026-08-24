# Consolidation C1 — La chaîne des espaces

> **Moteur :** LibraryV3 | **Projet :** LIB (Static Library) | **Compilateur :** Visual Studio 2026 / C++23
> **Namespace canonique :** `LV3`
> **Nature :** consolidation — *relecture transversale, aucun code nouveau*
> **Sources :** L01 (Config/Enums) · L02 (Maths) · L03 (ECS/SparseSet/ResourceManager) · L04 P1 (Triangle setup) · L04 P2 (Clipping/Perspective/Profondeur) · L05 (Caméra/Frustum/Pipeline) · Récapitulatif 01-05
> **Statut :** entrée C1.1 — document ouvert, d'autres entrées suivront avant la Leçon 06

---

## 0. Régime de lecture

Deux séries distinctes coexistent désormais dans le cours :

| Série | Objet | Ce qu'on en fait | Fichier |
|---|---|---|---|
| `Leçon NN` | **construction** — nouveau code, nouveaux chantiers | on la **valide** : compile + test de validation | `Lecon_06_Scenegraph.md` |
| `Consolidation Cn` | **relecture transversale** — cartographie de l'existant | on la **vérifie** : aucune contradiction avec les leçons sources | `Consolidation_C1_Espaces.md` |

**Deux principes non négociables pour toute la série C :**

1. **Aucune décision d'architecture ne se prend dans une consolidation.** Si une question fait apparaître une incohérence, elle est *nommée* et *inscrite à l'ordre du jour* d'une leçon — jamais corrigée au vol.
2. **Chaque affirmation distingue le confirmé du déduit.** Ce qui provient littéralement d'une leçon, et ce qui est reconstitué par cohérence. La méthode est celle du Récapitulatif 01-05 sur le scenegraph.

---

## 1. Définition dictatoriale de « espace »

**Règle C1.1 — un espace est un contrat en cinq clauses, pas une structure de données.**

```
ESPACE = ( origine , base , main , unité , domaine de validité )
```

Deux repères qui ne diffèrent **que par le domaine** sont deux espaces distincts. C'est pourquoi NDC et Raster sont séparés dans ce document alors qu'une transformation affine triviale les relie : leurs domaines, leurs unités et le sens de leur axe Y diffèrent tous les trois.

### Rappel des conventions LV3

```
REPÈRE      main DROITE, Y en haut, avant = -Z
            Vec3::Forward() = { 0, 0, -1 }
MATRICES    vecteur-ligne, v' = v·M, stockage row-major, translation en LIGNE 3
NDC         x, y ∈ [-1, +1]   ·   z ∈ [0, 1] REVERSE-Z (near→1, far→0)
PROFONDEUR  clear 0.0f, test GREATER, far infini
```

> ⚠️ **Dette documentaire confirmée :** la Leçon 02 affirme encore « main gauche, +Z entre dans l'écran ». Cette phrase contredit E3 et E4 ci-dessous. Inscrite à l'ordre du jour — non corrigée ici.

---

## 2. La liste des espaces

| # | Espace | Dimension / nature | Repère | Domaine de validité |
|---|---|---|---|---|
| **E0** | Actif (asset) | 3D affine | celui du DCC / du fichier OBJ | inconnu — hors moteur |
| **E1** | Objet (local, model) | 3D affine | pivot du mesh | AABB locale |
| **E2** | Parent (hiérarchie) | 3D affine, **N niveaux empilés** | pivot du parent | par branche du scenegraph |
| **E3** | Monde | 3D affine, main droite | origine de scène | ℝ³ |
| **E4** | Vue (eye, camera) | 3D affine | œil à l'origine, regarde −Z | `z < 0` devant |
| **E5** | Clip | **4D projectif homogène** | canonique | `−w ≤ x,y ≤ w` · `0 ≤ z ≤ w` |
| **E6** | NDC | 3D cartésien | cube canonique | `x,y ∈ [−1,1]` · `z ∈ [0,1]` |
| **E7** | Raster (screen) | 2,5D **continu**, Y vers le bas | coin haut-gauche du **viewport** | `[vx, vx+w] × [vy, vy+h]` |
| **E8** | Mémoire (buffers) | **1D discret** | octet 0 de la texture SDL | `[0, pitch·height[` |
| *EX* | *Tangent* | *3D, base (T,B,N) par sommet* | *par sommet* | *déclaré, non implémenté* |
| *EY* | *Texture (UV)* | *2D* | *`[0,1]²`* | *déclaré, non fiable* |

### 2.1 Deux corrections de vocabulaire

**E2 n'est pas un espace — c'est une pile d'espaces.** Chaque nœud du scenegraph en définit un. Les traiter comme un seul est l'erreur qui produit les ré-parentages faux et les remontées world→local incorrectes.

**E5 n'est pas un espace de points — c'est un espace de droites.** `(x,y,z,w)` et `(2x,2y,2z,2w)` désignent le **même** point. Cette redondance est exactement ce qui rend le clipping linéaire et exact. C'est aussi exactement ce que la division par `w` détruit.

---

## 3. Comment on entre, comment on sort

| Frontière | Opérateur d'entrée | Chemin de retour | Inversible ? |
|---|---|---|---|
| E0 → E1 | `OBJLoader` + `ComputeMeshAABB()` | aucun | ❌ **destructif** |
| E1 → E2 | `m_localMatrix` (`LocalTransformSystem`, consomme `m_dirty`) | `inverse()` générique | ✅ |
| E2 → E3 | `m_worldMatrix = m_localMatrix · parent.m_worldMatrix` | `inverse()` générique | ✅ |
| E3 → E4 | `viewMatrix = tr.m_worldMatrix.inverseRigid()` | **`tr.m_worldMatrix` — gratuit** | ✅ |
| E4 → E5 | `projectionMatrix` (reverse-Z, `m[2][3] = −1`) | `inverse(P)` | ✅ |
| E5 → E5 | `ClipTriangleNear` (Blinn-Newell homogène) | aucun | ❌ topologie modifiée |
| E5 → E6 | **`÷ w`** — *pas une matrice* | uniquement si `invW` a été conservé | ❌ **destructif** |
| E6 → E7 | `Viewport::ToRaster` (affine + **flip Y**) | `ToNDC` | ✅ |
| E7 → E8 | `floor(x), floor(y)` + adressage `pitch` | aucun | ❌ **quantification** |
| E8 → E3 | `invViewProjection` (paresseuse) + lecture du `DepthBuffer` | — | ✅ **le seul retour** |

### Règle C1.2 — trois frontières sur neuf sont à sens unique

L'**import**, la **division par `w`**, la **quantification pixel**. Toutes les autres sont des matrices.

Savoir lesquelles sont irréversibles, c'est savoir ce qu'on peut encore reconstruire en fin de frame. C'est la justification structurelle de `InverseViewProjection` dans l'API caméra (L05, règle 10) : picking, reconstruction de la position monde depuis le Z-buffer, effets en espace écran. Elle se calcule **une fois par frame**, et **paresseusement**.

### Le retour E4 → E3 est gratuit — et c'est un point d'architecture

On n'inverse **pas** la `viewMatrix`. On relit `tr.m_worldMatrix`, qui est précisément ce qu'on avait inversé pour la produire.

Ce n'est possible que parce que **la caméra n'est pas un cas spécial du scenegraph** (L05, règle 1) : elle porte un `TransformComponent` ordinaire et traverse le même `WorldTransformSystem` que n'importe quelle entité. Il n'existe aucun chemin de code séparé pour elle dans la propagation hiérarchique.

> ⚠️ **Contrat de `inverseRigid()` :** valable uniquement sur une **isométrie**. Un scale sur le Transform de la caméra rend la vue fausse — silencieusement. Avec scale, seul `inverse()` générique est correct.

---

## 4. Entrées, sorties, et travail effectué par espace

| Espace | Entrée | Sortie | Travail qui n'a de sens QUE là |
|---|---|---|---|
| **E0** | fichier `.obj`, chemin + répertoire | `MeshClass` en SoA, `meshAABB` | désindexation vers le SoA, normalisation d'unité/orientation, **calcul de l'AABB** |
| **E1** | `vertexPositions`, `indices`, `vertsPerFace ∈ {3,4}` | `Vec3f` par sommet, AABB **exacte** | **rien par frame** — E1 est immobile, donc c'est le seul espace où un précalcul s'amortit |
| **E2** | `m_local` (position + quaternion + scale), `m_worldMatrix` du parent | **une seule** matrice 4×4 par entité | composition hiérarchique, propagation **descendante** depuis les racines |
| **E3** | `m_worldMatrix`, AABB transformée, 6 plans Gribb-Hartmann | `EIntersect ∈ {Inside, Intersect, Outside}`, `needsNearClip` | **culling** (frustum, distance/LOD, occlusion), physique, triggers |
| **E4** | `TransformComponent` + `CameraComponent` (lentille pure) + `Viewport` | `ViewData` : V, P, V·P, frustum, position, forward, near/far | `inverseRigid()`, `FovYFromFocal`. **Aucun mesh n'est connu ici** — d'où N vues par frame |
| **E5** | `MVP = modelMatrix · viewProjectionMatrix`, composée **par mesh** | 0, 1 ou 2 triangles ; ≤ 4 sommets intermédiaires | **clipping near** (`w − z ≥ 0`), triangulation en éventail **avant** le clip, lerp exact des varyings |
| **E6** | `Vec4f` clip post-clipping | `Vec3f` NDC **+ `invW`** | une division, trois multiplications. `z_ndc` devient affine en `1/w` |
| **E7** | NDC + `invW` + `Viewport` | `Triangle2D` + `z_ndc` + `invW`, bbox entière | back-face (signe de l'aire), edge function antisymétrique, top-left rule, barycentriques, `ClampBox`, **flip Y ici et nulle part ailleurs** |
| **E8** | `FragmentContext` (`void*` + sentinelle `magic`), `x,y` entiers | `uint32` ARGB, `float` profondeur | **early-Z** (`GREATER` vs clear `0.0f`), blending, écriture |

### Règle C1.3 — chaque opération appartient à l'espace où elle est la moins chère ET la plus universelle

- On **cull en E3** parce que c'est le seul espace commun à toutes les vues et à toutes les entités.
- On **clippe en E5** parce que c'est le seul où les plans sont des comparaisons de flottants, et le seul où l'interpolation est linéaire.
- On teste le **winding en E7** parce que l'aire y est déjà calculée pour les barycentriques : le test est **gratuit**.

Ce n'est pas une hiérarchie de coût seule. Un test moins cher dans le mauvais espace est un test faux (voir §7, contre-exemples 3 et 4).

---

## 5. Ce qui transite entre les espaces

**C'est la bonne question, et presque personne ne se la pose.** Un attribut qui ne figure pas dans la colonne « charge utile » n'existe pas de l'autre côté de la frontière : il faut le recalculer, ou il est définitivement perdu.

| Frontière | Charge utile transportée | Détruit au passage |
|---|---|---|
| E0 → E1 | positions, indices, `vertsPerFace`, submeshes, AABB locale | unités et repère du DCC |
| E1 → E2 | position ; normale via **`(M⁻¹)ᵀ`**, jamais `M` | — |
| E2 → E3 | position, AABB **regrossie**, normale | le serrage de l'AABB |
| E3 → E4 | position — **et rien d'autre ne descend** : le frustum reste en E3 | — |
| E4 → E5 | `Vec4f` clip, avec **`w` = distance à l'œil** | la métrique : plus aucune distance mesurable |
| E5 → E5 | `ClipVertex` (clip + varyings), paramètre `t` | 1 triangle → 0, 1 ou 2 |
| **E5 → E6** | NDC **+ `invW`** ← **le passager** | **`w`** |
| E6 → E7 | x,y pixels flottants, `z_ndc`, `invW` | — |
| E7 → E8 | `uint32` ARGB, `float` profondeur | la position sous-pixel |
| E8 → E3 | reconstruction monde depuis le `DepthBuffer`, picking | — |

### Règle C1.4 — `invW` est le seul passager qui franchit la frontière E5/E6

Tout le reste est soit **consommé avant** (les `w`), soit **produit après** (les pixels).

**Et une seule exception : `z_ndc`.** Avec la matrice de `Projection.cpp` :

```
w_clip = d                              (d = -z_vue = distance à l'œil)
z_ndc  = [n·f/(f-n)]·(1/d) − n/(f-n)     → affine en 1/d
far infini :  z_ndc = n/d                → purement proportionnel à 1/w
```

`1/d = 1/w` est linéaire en espace écran, donc **`z_ndc` est linéaire en espace écran** et les barycentriques affines l'interpolent exactement.

> **Conséquence architecturale :** la profondeur est le **seul** attribut testable sans avoir calculé le dénominateur perspectif. C'est exactement ce qui rend l'**early-Z** possible.

Tous les autres varyings se transportent en `a·invW`, **pré-divisés au triangle setup**, jamais dans la boucle pixel (L04 P2, règle 15).

---

## 6. Quelle fonctionnalité gère quel espace

| Espace | Propriétaire | Ce qu'il ignore délibérément |
|---|---|---|
| **E0** | `OBJLoader`, `ResourceManager`, `MeshHandle` | tout le rendu |
| **E1** | `MeshClass` | où il se trouve dans le monde |
| **E2** | `TransformComponent`, `HierarchyComponent`, `LocalTransformSystem`, `WorldTransformSystem` | la caméra, les meshes |
| **E3** | `Frustum`, `AABB3d`, `Plane`, boucle de mesh de `RenderSystem` | le rasterizer |
| **E4** | `BuildViewData`, `ViewData`, `Projection`, `MatrixLib::inverseRigid` | **tous les meshes** |
| **E5** | `Clipper`, `ClipVertex`, `RenderSystem` | les pixels |
| **E6** | `RenderSystem` (la division), `Viewport` | — |
| **E7** | `Viewport`, `Rasterizer`, `Renderer` (état : mode, cull, depth test) | le contenu du contexte de shading |
| **E8** | `FrameBuffer`, `DepthBuffer`, `Fragment`, SDL | `RenderSystem` entièrement |

### Correspondance avec les quatre couches du rendu (L05, §9)

```
RenderSystem   →  E3 (cull) puis E5 (clip)      ne connaît pas le rasterizer
Renderer       →  E7 (état : viewport, mode)     ne connaît pas le Registry
Rasterizer     →  E7 (balayage, barycentriques)  ignore le contenu du contexte
Fragment       →  E8 (test de profondeur, écriture)  ignore RenderSystem
```

**Chaque couche n'appelle que celle du dessous, avec des données explicites. Jamais d'état partagé implicite, jamais d'appel remontant.**

---

## 7. Contre-exemples — cinq erreurs d'espace, zéro crash

Aucune de ces cinq erreurs ne provoque de plantage. Toutes produisent une image qui a l'air *presque* juste. C'est exactement ce qui les rend coûteuses.

| Ce qu'on fait | L'espace violé | Le symptôme exact |
|---|---|---|
| Diviser par `w` **avant** de clipper | E5 traité comme E6 | le triangle réapparaît **en miroir** de l'autre côté de l'écran quand on recule dedans |
| Transformer une normale par `M` au lieu de `(M⁻¹)ᵀ` | E1→E3 appliqué à un **covecteur** | éclairage faux **uniquement** sur les objets à scale non uniforme |
| Tester le frustum (plans en E3) contre une AABB restée en E1 | mélange d'espaces | objets qui disparaissent **quand ils tournent** |
| Câbler `area >= 0` pour le back-face | test écrit pour E6, exécuté en E7 (Y inversé) | ce sont les faces **avant** qui sont éliminées — on voit l'intérieur du cube |
| Indexer le `FrameBuffer` en `y*width+x` | E8 confondu avec E7 | image en escalier **seulement** aux largeurs non alignées (1922 casse, 1920 non) |

**Le fil commun :** dans les cinq cas, le calcul est algébriquement correct. C'est son **espace d'exécution** qui est faux. Un test juste au mauvais endroit est un test faux.

---

## 8. Le schéma d'ensemble

```
E0 ══OBJLoader══▶ E1 ──M_local──▶ E2 ──M_world──▶ E3
ACTIF   destructif  OBJET          PARENT           MONDE ◀── Frustum::Classify
                                                      │        (on CULL ici)
                                                  viewMatrix
                                                  inverseRigid()
                                                      ▼
                                                    E4  VUE
                                                      │  projectionMatrix
                                                      ▼
                                          ┌──────────────────────┐
                                          │  E5  CLIP  (x,y,z,w) │  ClipTriangleNear
                                          │  test : w − z ≥ 0    │  (on CLIPPE ici)
                                          └──────────┬───────────┘
                                            ÷w  ║ DESTRUCTIF ║   invW ↗ passager
                                          ┌──────────▼───────────┐
                                          │  E6  NDC   z ∈ [0,1] │
                                          └──────────┬───────────┘
                                             Viewport::ToRaster (flip Y)
                                          ┌──────────▼───────────┐
                                          │  E7  RASTER  + invW  │  backface, edge fn
                                          └──────────┬───────────┘
                                          floor+pitch ║ DESTRUCTIF ║
                                          ┌──────────▼───────────┐
                                          │  E8  MÉMOIRE         │  early-Z GREATER
                                          └──────────────────────┘
                                                      ╎
                        invViewProjection ◀╌╌╌╌╌╌╌╌╌╌╌╯   le SEUL retour
```

**Support interactif associé :** `LV3_Espaces_Pipeline.html` — rail cliquable des neuf espaces, contrat complet par espace, et bascule **Aller / Retour** montrant l'inversibilité de chaque arête.

---

## 9. Règles dictatoriales issues de C1

| # | Règle |
|---|---|
| **C1.1** | Un espace est un contrat en cinq clauses (origine, base, main, unité, domaine) — pas une structure de données. Deux repères qui ne diffèrent que par le domaine sont deux espaces distincts. |
| **C1.2** | Trois frontières sur neuf sont à sens unique : import, `÷w`, quantification pixel. Toutes les autres sont des matrices. `InverseViewProjection` existe parce que la troisième est irréversible. |
| **C1.3** | Chaque opération appartient à l'espace où elle est la moins chère **et** la plus universelle. Un test juste au mauvais endroit est un test faux. |
| **C1.4** | `invW` est le seul passager de la frontière E5/E6. Seule exception au transport `a·invW` : `z_ndc`, affine — et c'est ce qui rend l'early-Z possible. |
| **C1.5** | E2 est une **pile** d'espaces, pas un espace. E5 est un espace de **droites**, pas de points. |
| **C1.6** | Le retour E4→E3 est gratuit parce que la caméra n'est pas un cas spécial du scenegraph. Toute exception à cette règle détruirait le bénéfice. |

---

## 10. Dette et angles morts confirmés par cette relecture

**Aucun de ces points n'est corrigé ici** (principe 1 de la série C). Ils sont nommés et inscrits.

| Sujet | Espace concerné | Risque | Destination |
|---|---|---|---|
| Leçon 02 affirme « main gauche, +Z entre dans l'écran » | E3 / E4 | documentation mensongère, contredit le code | correction documentaire immédiate |
| `ComputeMeshAABB()` appelée automatiquement nulle part | E0 → E3 | `meshAABB` invalide → culling élimine tout → écran noir sans erreur | Leçon 06 |
| `GetFaceView` suppose les sommets contigus dans `vertexPositions` | E1 → EY | normales et UV silencieusement fausses dès qu'on s'en servira | **bloquant pour EX / EY** |
| `HierarchyComponent` : parent explicite ? cascade à la destruction ? | E2 | ré-parentage, entités orphelines, handles périmés | **Leçon 06 — scenegraph** |
| Scale non uniforme hérité en cascade | E2 → E3 | normales via `(M⁻¹)ᵀ` en cascade, coût et exactitude | Leçon 06 |
| `Renderer` n'exploite ni `ECullMode`, ni `EDepthTest`, ni `EBlendMode` | E7 / E8 | trois enums sans usage effectif | chantier de rendu |
| Chantier B (L04 P2) — validation de l'interpolation perspective-correcte | E5 → E7 | **ouvert** | à clore avant la Leçon 06 |

### Ordre de traitement recommandé

1. Correction documentaire de la Leçon 02 (coût : dix minutes, évite une demi-journée un jour)
2. Clôture du **Chantier B** de la Leçon 04 P2
3. **Leçon 06 — le scenegraph** (E2), qui répondra aux trois angles morts hiérarchiques
4. `GetFaceView` **avant** toute ouverture de EX (tangent) ou EY (UV)

---

*Document ouvert. Entrée C1.1 close. Les entrées suivantes de la consolidation C1 seront ajoutées à ce fichier, ou scindées si les sujets divergent trop.*
