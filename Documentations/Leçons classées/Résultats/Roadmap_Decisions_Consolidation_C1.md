### Décisions en attente — Consolidation C1 : la chaîne des espaces (relevé du 08/11/2026)

**Aucune décision à ce stade : tout ce qui suit reste à acter.**
Origine : artefact « Lot Consolidation C1 — La chaîne des espaces, pas à pas ».
**Attention, les sujets sont à vérifier sur le dépôt avant toute modification.**

#### C1.1 — Écarts entre la consolidation d'origine et le dépôt

| Réf | Écart | Raison, apport, statut |
|-----|-------|------------------------|
| C1-0 | Collision de nom : « Consolidation C1/C2 » et « chantier C1/C2 » (phase G) | La section « Dette relevée en C1 » (z NaN) appartient au *chantier* C1, pas à la consolidation. Proposition : renommer la série des consolidations (ex. `K1`, `K2`) et ne plus réutiliser une lettre de chantier. Statut : décision à prendre. |
| C1-1 | Sur le chemin Inside, E5 (clip) et E6 (NDC) ne sont plus matérialisés | Chantier 2b : `ProjectPositions(8)` fait E1 → E7 en un passage ; `RasterVertex` est le type de E7. Apport : 504 → 117 divisions par roche, Render −33 %. Statut : évolution ; `Consolidation_C1_Espaces.md` (§3, §5, §8) décrit encore la frontière « par coin » — à amender. |
| C1-2 | Deux nouveaux espaces : E7·g (grille 1/256 px) et E7·fx (entiers X = x·256) | `LV3_RASTER_FIXED`, lot D-a1 (Giesen). Apport : couverture exacte, un sommet = un accrochage. Statut : évolution ; la carte d'origine (« E7 : 2,5D continu ») est fausse sous FIXED = 1 — à ajouter à la liste des espaces. |
| C1-3 | Frontière E7 → E8 : `floor(x)` dans la carte, centres `x + ½` dans le dépôt (`TightPixelBox`) | La carte d'origine décrivait « dans quel pixel tombe un point », pas « quels centres un triangle couvre ». Statut : correction documentaire. |
| C1-4 | Règle C1.3 (« le winding se teste en E7 ») et écart L4-4 (« culling en espace objet rejeté ») contredits par B5 | B5 a introduit un pré-filtre E1 **conservatif** (s = L·E < −τ), rentable seulement en AVX2 ; E7 garde le dernier mot. Statut : C1.3 amendée dans l'artefact ; **réécrire L4-4** : « rejeté en remplacement, adopté en pré-filtre SIMD (B5) ». |
| C1-5 | Clause « main » : `det(model) < 0` inverse la main de E1 | Dette « Miroir » déjà relevée par B5, rattachée ici à la carte des espaces. Prédiction : propagation à toute la descendance par E2 (L06). Statut : dette, à traiter avant ou pendant la L06, sur les deux tests (écran B4 et objet B5) à la fois. |

#### C1.2 — Dettes de la consolidation d'origine : état au 08/11

| Réf | Dette | Constat au 08/11 | Statut proposé |
|-----|-------|------------------|----------------|
| C1-6 | `ComputeMeshAABB()` appelée nulle part automatiquement | `RegisterMesh` est devenue la porte unique (SoA, plans B5, plafond de sommets) mais n'appelle pas `ComputeMeshAABB()`. Appel dans `OBJLoader` : non vérifié. | dette ; l'endroit naturel existe désormais |
| C1-7 | `GetFaceView` suppose les sommets contigus | Inchangé (`fv.verts = &vertexPositions[indices[base]]`). Bloque EX (tangent), EY (UV) et L4-2. | dette bloquante pour le texturing |
| C1-8 | Leçon 02 : « main gauche, +Z entre dans l'écran » | Traité par les lots L02 / L02-C ? À vérifier dans « socle L02 ». | à vérifier |
| C1-9 | `ECullMode`, `EDepthTest`, `EBlendMode` non exploités | Déjà porté en L4-5. | doublon : renvoyer à L4-5 |
| C1-10 | Chantier B (L04 P2) : interpolation perspective-correcte | Toujours ouvert, rattaché à la L09. Prérequis : C1-7. | reste ouvert |

#### C1.3 — Points nouveaux relevés par la relecture

| Réf | Sujet | Détail |
|-----|-------|--------|
| C1-11 | Le chemin Intersect n'a pas de version AVX2 | `TransformPositions` scalaire, projection par coin. Négligeable en scène astronomique (≈ 2 % des faces), dominant en scène FPS (lot V3 : un niveau d'un seul mesh est toujours Intersect). À relier au plan FPS / culling. |
| C1-12 | Lot D-a1d (accrochage par nombre magique) | Livré le 06/11, « à appliquer » ; le dépôt montre encore `_mm_round_ss` dans `SnapToSubpixel` et `_mm256_round_ps` dans `ProjectPositions8`. À vérifier. |
| C1-13 | `inverseRigid()` sans filet | Aucune assertion d'orthonormalité avant l'appel ; devient probable en L06 (caméra enfant d'une entité à échelle : vue fausse d'un facteur 4 sans erreur). Proposition : `LV3_ASSERT(IsRigid(m))` en Debug. |
| C1-14 | Taille apparente du LOD : une mesure qui saute de E1 à E7 | Calculée depuis l'AABB locale et la `mvp`, sans passer par les sommets. Rien à décider : à documenter dans la carte comme « mesure E1 → E7 par instance ». |

#### Règles issues de la relecture (proposées, non actées)

- **C1.3 amendée** : chaque opération appartient à l'espace où elle est la moins chère et la plus universelle ; un test peut avoir un pré-filtre conservatif dans un espace plus en amont, à condition que l'espace d'origine garde le dernier mot.
- **C1.4 confirmée** : `invW` est le seul passager de la frontière clip → NDC ; le type `RasterVertex` la grave dans le code.
- **Règles grepables** : `/ c.w` n'apparaît que dans `ProjectClip` (+ `_mm256_div_ps(one, cw)`) ; le flip Y que dans `Viewport::ToRaster` (+ sa copie AVX2 gardée par la TNR B3) ; `SnapToSubpixel` que dans `ProjectClip` ; 0 `vfmadd` dans l'asm de `VertexStage` et `RenderSystem`.
