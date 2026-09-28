# Avenant C2 — Chantier 2 : pré-transformation des sommets

> **Statut** : clos le 27/09/2026.
> **Rattachement** : consolidation C2 (`Consolidation_C2_Campagne_Mesure.md`), § 8.2, chantier 2.
> **Portée** : LibraryV3 (LIB) + RefactoNouvelleLibrairieV3 (EXE), `Release`, x64.
> **Machine** : i7-1265U (portable, Alder Lake hybride 2 P-cores + 8 E-cores, AVX2), secteur, profil « performances élevées ».
> **Nature** : ce document fige le code du chantier, les chiffres, les erreurs de méthode rencontrées et les règles qui en sortent. Il **amende** C2 sur plusieurs points (§ 7).

---

## 1. Le défaut corrigé

Dans `RenderView`, `MulRow` était appelé **par coin de face** :

```cpp
for (uint8_t k = 0; k < vpf; ++k)
    cv[k].clip = MulRow(mvp, mesh->vertexPositions[mesh->indices[base + k]]);
```

Un sommet partagé par N faces était transformé N fois par vue, avec un résultat identique à chaque fois. Rapport coins ÷ sommets uniques, après la soudure `(pos, uv, nrm)` de l'OBJLoader :

| Mesh | Sommets | Coins | Coins ÷ sommets |
|---|---|---|---|
| rock_a/b/c | 115 | 504 | 4,38 |
| sphere_lo | 185 | 864 | 4,67 |
| sphere_mid | 559 | 2 880 | 5,15 |
| sphere_hi | 2 143 | 11 904 | 5,55 |
| ship_scout | 358 | 888 | 2,48 |

**Règle candidate** : un calcul qui ne dépend que du sommet se fait une fois par sommet, jamais une fois par coin.

**Précision sur C2** : C2 écrivait « par (mesh, vue) ». C'est en réalité **par (instance, vue)**, puisque `mvp = modelMatrix × VP` dépend de l'entité. Les 600 roches partagent 3 meshes mais ont 600 matrices modèle différentes.

---

## 2. Le code livré

### 2.1 L'étage sommets — `Rendering/VertexStage.h/.cpp` (nouveaux)

- **`ClipSpaceBuffer`** : tampon `Vec4f` possédé par l'**appelant** (comme `fb`/`db`). Il est alloué une fois, dimensionné par la borne dure `kMaxMeshVerticesHard` (65 536 × 16 o = 1 Mio). Il n'est pas copiable. Seuls les `vertexCount()` premiers éléments sont touchés : 1,8 Kio pour une roche, qui restent en L1.
- **`TransformPositions(mvp, src, count, dst)`** : une passe séquentielle sans branche. C'est le **point unique** où vivra la version AVX2 (chantier 4). La matrice est **copiée dans une variable locale** : lue par référence, chaque écriture `float` dans `dst` pourrait, pour le compilateur, modifier un coefficient de `mvp` (aliasing de type `float`), ce qui imposerait de recharger les 16 coefficients à chaque sommet.

### 2.2 Le plafond de sommets, vérifié une seule fois

- `Core/EngineSettings.h` : `kMaxMeshVerticesHard = LV3_MAX_MESH_VERTICES`. La macro existait mais n'était appliquée nulle part (même situation que `LV3_MAX_ENTITIES` avant le bug 54).
- `ResourceManager::RegisterMesh` : c'est la **porte d'entrée unique** de tout mesh, OBJ comme procédural. Un mesh au-delà du plafond reçoit un `Logger::error` et un `MeshHandle::Invalid()`.
- Imprécision connue : `LoadMeshChecked` classe ce refus en `ParseFailed`. Le log donne la vraie cause. À affiner en phase F.

### 2.3 `RenderView`

- Nouvelle signature : `RenderView(Registry&, ResourceManager&, Renderer&, const ViewData&, ClipSpaceBuffer&)`. `RenderSystem.h` n'inclut toujours rien (déclaration anticipée).
- Garde par mesh : `LV3_ASSERT` plus un `continue` actif en `Release`. C'est un filet mémoire, sur le même patron que `DestroyEntity` (bug 53).
- Chaque face **lit** trois `Vec4f` par indice, avec `LV3_ASSERT(vi < nVerts)`, et ne fait plus aucune multiplication.
- Côté EXE : `ClipSpaceBuffer clipBuf;` est déclaré à côté de `Renderer renderer;` et passé à chaque `RenderView`.

### 2.4 Pourquoi un `Vec4f` et pas un `Vec3f`

- **En espace objet**, w vaut toujours 1 : il est implicite, d'où l'entrée en `Vec3f`.
- **En espace clip**, w vaut `-z_vue` en perspective. C'est la distance à la caméra, et elle varie à chaque sommet. Elle est lue par `NearDistance`, `Lerp`, `ClipLess`, `invW` et `Fragment`.
- **Diviser avant de stocker est interdit** : pour un sommet derrière la caméra, w < 0, et la division renverse x et y. C'est la raison d'être du clipping en espace homogène.
- **Remarque** : en perspective infinie, c'est z_clip qui est constant (= n), pas w. En orthographique, c'est w (= 1). Un format à 3 floats dépendant de la projection a été écarté : il faudrait un pipeline par type de projection.
- **Côté mémoire** : 16 octets, c'est exactement 4 éléments par ligne de cache et un seul chargement aligné. Un `Vec3f` chevauche une ligne une fois sur quatre.

### 2.5 Instrumentation

- Nouveau compteur **`VertsTransformed`**, ajouté **en fin** d'`EProfCounter` pour que les colonnes des CSV versionnés ne se décalent pas. Le `static_assert` sur les noms a protégé l'ajout.
- En-tête CSV enrichi : **`cpu=`** (chaîne de marque CPUID), **`affinity=`** (`GetProcessAffinityMask`) et **`ablation=`**.
- Ablation par commutateur **`LV3_ABLATION_RASTER`** (`Core/config.h`, défaut 0) : elle remplace le bloc commenté à la main et se déclare elle-même dans le CSV.

---

## 3. Chronologie des mesures — et les deux fausses pistes

| Séance | Constat | Diagnostic |
|---|---|---|
| 1 — B2/D2 seuls | Géométrie −38 %, mais Render seulement −7,7 % contre la référence C2 | Comparaison entre deux jours, et probablement entre deux machines : non recevable (M4) |
| 2 — B1/D1/B2/D2 | B1 ≈ D1 (5,90 contre 5,96 ms) | **B1 avait tourné avec l'ablation active** ; les compteurs ne pouvaient pas le détecter (`TrisRasterized` est compté avant `DrawTriangle`). D'où `ablation=` dans l'en-tête |
| 2 bis — 6 runs alternés | Tout est 1,7 fois plus lent, **y compris** Xform et Trigger | Attribué d'abord à un bridage thermique. **Faux** : voir séance 4 |
| 3 — A/P/Aa/Pa, en-tête `cpu=` | CPU = **i7-1265U hybride** | Nouvelle hypothèse : le thread tournait sur un E-core |
| 4 — épinglage P (`affinity=4`) contre E (`affinity=100`) | E-core : T = 143 µs, Render = 18,1 ms, identique à la séance 2 bis | **Hypothèse E-core confirmée** |

**Leçon principale** : chaque fausse piste a été détectée par une métadonnée ou un témoin, jamais par la mémoire de l'opérateur. Et chaque fois qu'il manquait une métadonnée, une conclusion fausse a survécu une séance de plus.

---

## 4. Chiffres de clôture

Scène `solar_system_v1compat_belt.json`, 4 vues, 1536×900, 300 frames dont 60 de chauffe, médianes.

### 4.1 Travail (déterministe)

| | Avant | Après |
|---|---|---|
| Appels `MulRow` par frame | 824 964 (= 3 × `FacesSubmitted`) | **181 061** (÷ 4,56) |
| `FacesSubmitted` / `TrisRasterized` | 274 988 / 134 389 | **identiques** |

Les faces soumises et les triangles rasterisés sont identiques au chiffre près : **on dessine exactement les mêmes triangles**. Cela confirme aussi `vertsPerFace = 3`, point laissé ouvert au § 7.2 de C2.

### 4.2 Temps, par type de cœur

| | P-core | E-core |
|---|---|---|
| Géométrie isolée (ablation), après ÷ avant | **0,604** (−39,6 %) ; 0,608 lors d'une autre séance | non mesurée |
| Render complet, après ÷ avant | **0,827** (−17,3 %, −2,15 ms), 1 paire | **0,895** (−10,4 %), 3 paires à ±0,4 % |
| Part du gain isolé qui atteint la frame | **84 %** | **≈ 53 %** |

Coût unitaire mesuré : **3,5 ns par `MulRow` évité**, sur P-core, en isolé. Cela corrige C2 : les « 7,2 ns par sommet ≈ un `MulRow` » de C2 se décomposaient en environ 3,5 ns de `MulRow` et environ 3,7 ns d'autres traitements par coin (division, `ToRaster`, backface, `FaceColor`, copies).

### 4.3 Le même binaire, sur P-core et sur E-core

| | P (`affinity=4`) | E (`affinity=100`) | E ÷ P |
|---|---|---|---|
| Témoin T | 93,6 µs | 143,3 µs | ×1,53 |
| Render | 10,19 ms | 18,14 ms | **×1,78** |
| Clear | 968 µs | 820 µs | ×0,85 |
| Present | 813 µs | 1 187 µs | ×1,46 |

- Render se dégrade plus que T sur E-core : la rasterisation est dominée par des attentes, et une fenêtre out-of-order plus petite (~256 micro-opérations contre ~512) en masque moins.
- `Clear` va **plus vite** sur E-core. Il est limité par le débit mémoire, pas par la puissance de calcul du cœur, et ne peut donc pas servir de témoin de calcul.

---

## 5. Le mécanisme : le recouvrement dans un seul thread

Pourquoi le gain dans la frame est-il inférieur au gain isolé, alors que le programme n'a qu'un thread ?

Un cœur moderne est **superscalaire et out-of-order**. Il décode en avance (fenêtre ROB), exécute toute instruction dont les opérandes sont prêts, sur plusieurs unités à la fois, puis valide les résultats dans l'ordre du programme. Pendant que `RasterizeTriangle` attend le depth buffer (5,5 Mo, trop gros pour L2), les `MulRow` de la face suivante, qui ne dépendent pas de cette attente, s'exécutent déjà sur des unités flottantes autrement inactives.

Une partie du travail supprimé était donc **déjà cachée** dans les attentes de la rasterisation. L'ablation, qui retire la rasterisation, retire aussi ces attentes : elle expose le coût complet de la géométrie, et **surestime sa contribution à la frame**.

La proportion masquée dépend de la taille de la fenêtre out-of-order, donc **du cœur** : environ 16 % sur P-core, environ 47 % sur E-core.

Démonstration reproductible (un thread, même nombre d'additions) : une somme à 4 accumulateurs indépendants est 3 à 4 fois plus rapide qu'une somme à une seule chaîne de dépendance (compilée en `/O2`, sans `/fp:fast`).

Statut : le mécanisme est **compatible avec toutes les mesures**, mais n'est pas démontré par un compteur matériel. Une preuve directe demanderait Intel VTune, analyse *Microarchitecture Exploration*, catégorie *Memory Bound* de `RasterizeTriangle`.

---

## 6. Référence F — nouveau point de départ

La machine de C2 n'est pas identifiable avec certitude : aucun champ `cpu=` à l'époque, et probablement un autre ordinateur. **Les chiffres absolus de C2 ne se comparent pas à ceux du portable.** Le diagnostic structurel de C2 reste valable : limité par les primitives et non par le remplissage, `Render ∝ FacesSubmitted`, A3 abandonnée. Il repose sur des rapports mesurés sur une même machine.

| Référence F | Valeur |
|---|---|
| Machine | i7-1265U, **P-core épinglé** (`affinity=4`), secteur, performances élevées |
| Code | chantier 2 intégré |
| FRAME | 12,14 ms (≈ 82 fps) |
| Render | **10,19 ms** (838 ‰) |
| Témoin calcul T | 93,6 µs |
| Témoin mémoire Clear | 968 µs |
| Loi de coût | ≈ **37 ns par face soumise** (contre environ 42 dans C2, sur l'autre machine) |
| `WorldXform1` | 23,9 µs, soit 2 ‰ : la décision d'abandonner A3 tient |

La valeur P-core du gain (−17,3 %) repose sur une paire unique. Les runs « avant » de la prochaine campagne ABBA, qui mesureront ce code épinglé sur P-core, la confirmeront au passage.

---

## 7. Règles de mesure — mises à jour de la série M

- **M2 complétée.** L'en-tête porte aussi `cpu=`, `affinity=` et `ablation=`. Chaque champ ajouté ici a été motivé par une erreur réelle.
- **M3 complétée.** Deux témoins par campagne :
  - un témoin **calcul**, T = Animation + LocalXform1 + WorldXform1 + Trigger ;
  - un témoin **mémoire**, `Clear`.
  
  Une paire n'est recevable que si T varie de moins de 3 % entre ses deux runs. `Clear` seul n'a pas détecté un ralentissement de 67 %.
- **M7 amendée.** Une ablation mesure le coût d'un étage **isolé**, pas sa contribution à la frame. On ne déduit jamais le coût d'un étage voisin par soustraction (complet − ablation) : les étages se recouvrent.
- **M8 (nouvelle).** Deux versions se comparent **dans une même séance, en alternance ABBA** (qui annule une dérive linéaire, contrairement à ABAB), après un run jetable de mise en régime. Le verdict porte sur le **rapport au sein de chaque paire**, jamais sur des valeurs absolues venant de deux séances. Précision obtenue : ±0,4 % entre paires, contre un plancher de 15 % entre séances.
- **M9 (nouvelle).** Sur un CPU hybride, un banc monothread est **épinglé** sur un type de cœur déclaré (`start /affinity 4 …`). La référence se prend sur P-core. Sans épinglage, Windows choisit le type de cœur, et cette variable non contrôlée viole M4.

Critères de recevabilité d'un run : p95 inférieur à 1,15 × la médiane de `Render` (sinon le run est bimodal), rapport complet ÷ ablation cohérent (sinon le binaire est mal étiqueté), et `cpu=`/`affinity=` identiques au sein de la séance.

---

## 8. Conséquences sur le plan d'optimisation (C2 § 8.2)

| # | Chantier | État / révision |
|---|---|---|
| 2 | Pré-transformation des sommets | **CLOS** — −39,6 % sur la géométrie, −17,3 % sur Render (P-core) |
| 2b | *(nouveau)* division par w + `ToRaster` une fois par sommet, pour les meshes `Inside` | environ 3 ms isolées par coin restent dans la géométrie ; gain réel à mesurer (recouvrement) |
| 3 | Rejet précoce des triangles sub-pixel | **remonte** : la rasterisation représente désormais environ 60 à 65 % de `Render` |
| 4 | AVX2 sur `TransformPositions` | **rétrogradé** : environ 0,6 ms isolée au plus, et moins dans la frame |
| 1 | LOD | inchangé ; des assets LOD existent déjà pour les sphères, pas pour les roches |

**Étape suivante décidée** : brancher le compteur de **pixels couverts** (point 3 du § 9 de C2), au moins la distribution 0 / 1 / 2 à 4 / plus de 4 pixels par triangle. Il départagera 2b et 3 sur une mesure plutôt que sur une intuition.

---

## 9. Hygiène à faire

- Retirer les copies de travail `_avant` : `git worktree remove ..\LibraryV3_avant` et `..\Refacto_avant`.
- Remettre `LV3Profile` à `false` dans `LV3.Common.props`. C'était l'étape 4 du § 10 de C2, restée non faite depuis.
- Ajouter `Rendering/VertexStage.h/.cpp` au `.vcxproj.filters` (R29) si ce n'est pas encore fait.
- Versionner les CSV de la Référence F sous `Mesures/`.

---

*Visuel associé : « Étage sommets LV3 » (artifact interactif : par coin contre par sommet, compteurs `MulRow`, schéma mémoire du `ClipSpaceBuffer`).*
