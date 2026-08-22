# Récapitulatif — Leçons 01 à 05

> **Moteur :** LibraryV3 | **Projet :** LIB (Static Library) | **Compilateur :** Visual Studio 2026 / C++23
> **Namespace canonique :** `LV3` | **Document de synthèse**
> **Couvre :** Leçon 01 (Config/Enums) · Leçon 02 (Mathématiques) · Leçon 03 (ECS/SparseSet/ResourceManager) · Leçon 04 Parties 1 et 2 (Rasterizer) · Leçon 05 (Caméra/Frustum)

---

## 1. Fonctionnement global du moteur

LibraryV3 est un moteur de rendu 3D dont **toute la pipeline graphique s'exécute au CPU**, sans API graphique (pas d'OpenGL/DirectX/Vulkan) — SDL2 ne sert qu'à ouvrir une fenêtre et afficher un buffer de pixels calculé à la main. Le projet est une bibliothèque statique (LIB) compilée sous Visual Studio 2026 en C++23, pensée pour reproduire fidèlement l'architecture d'un moteur professionnel (Unity comme référence constante) plutôt que d'optimiser la performance dès le premier jour : chaque étage du pipeline GPU est simulé et compris avant d'être codé.

Le moteur repose sur trois piliers construits dans l'ordre des leçons. D'abord une fondation d'infrastructure (Leçon 1) qui tranche où vivent les constantes, les enums et les réglages du moteur, et qui protège le temps de compilation via une séparation stricte de ce qui entre dans le precompiled header. Ensuite une couche mathématique maison (Leçon 2) plutôt qu'une lib tierce comme GLM, justifiée par l'objectif pédagogique de contrôler exactement chaque multiplication matricielle et par le contrôle du layout mémoire. Cette couche a connu une révision de convention majeure entre la Leçon 2 et la Leçon 5 (voir §2.3). Par-dessus, une architecture de données ECS à base de SparseSet (Leçon 3), qui remplace l'héritage classique de `GameObject` par une composition entités/composants/systèmes optimisée pour la localité de cache CPU, accompagnée d'un ResourceManager qui gère meshes, matériaux et textures par handles plutôt que par possession directe.

Le cœur du rendu proprement dit se construit en deux temps. La Leçon 5 établit ce qu'est « une caméra » (en réalité quatre objets indépendants : Transform, Lentille, Viewport, Frustum) et le pipeline complet des espaces (Local → Monde → Vue → Clip → NDC → Écran), avec un frustum culling à 3 états extrait directement des matrices (Gribb-Hartmann) plutôt que des 8 coins. La Leçon 4 (parties 1 et 2) construit le rasterizer lui-même : triangle setup par edge function (Pineda), règle top-left pour éviter trous/doublons entre triangles adjacents, puis clipping near-plane en espace de clip (Sutherland-Hodgman homogène), interpolation perspective-correcte des attributs via `1/w`, et Z-buffer en reverse-Z avec early-Z.

Le fil rouge méthodologique de tout le cours : chaque décision de convention (repère, stockage matriciel, espace NDC) est **dictatoriale** — tranchée une fois, jamais réouverte à la légère — et chaque correction de bug s'accompagne d'un test de non-régression qui verrouille l'invariant plutôt que la valeur observée.

### 1.1 Validation du postulat CPU / Data-Oriented Design

Le postulat d'un moteur pensé pour le CPU (localité de cache, Data-Oriented Design) n'est formulé nulle part comme un manifeste isolé, mais il court en filigrane depuis la Leçon 1 et se justifie explicitement à chaque décision structurante.

**Ce qui confirme le postulat :**

| Élément | Preuve | Leçon |
|---|---|---|
| Rejet de l'héritage `GameObject` | « le massacre du cache CPU » identifié comme maladie n°2 de l'OOP naïve : cache miss + appel virtuel par objet | L03 §1.1 |
| SparseSet à 3 tableaux | Tableau dense contigu conçu pour le prefetcher matériel ; comparaison chiffrée L1/L2/L3/RAM (~200 cycles un miss) | L03 §2.3 |
| « Un composant chaud est un POD, ou il en paie le prix » | Règle explicite — un composant avec `std::string`/`std::set` n'est contigu qu'en façade | L03 §2.3 |
| Composants référencent, ne possèdent jamais | `MeshComponent` : `shared_ptr` → `MeshHandle`, redevient `is_trivially_copyable_v`, le swap-and-pop passe d'un décrément atomique à un `memcpy` | L03 §7.2 |
| `Emplace` plutôt que `Add` | Construction directe en place, zéro copie/déplacement intermédiaire pour les composants avec indirection | L03 §4.5 |
| Sparse paginé (B7) | Pages allouées à la demande — le « creux » du sparse ne coûte que ce qu'il indexe réellement | L03 §2.7 |
| Layout mémoire maison plutôt que GLM | Justifié explicitement par « contrôle total du layout mémoire, indispensable pour le SIMD et le rendu par tuiles : alignement, AoS vs SoA » | L02 §1 |
| `alignas(16)` anticipé sur `Vec4`/`Mat4` | Dès la conception, même sans SIMD actif, pour éviter un réalignement futur | L02 §4 |
| Pré-division au triangle setup, jamais par pixel | Amortir le coût sur 3 sommets plutôt que sur N pixels | L04 P2, règle 15 |
| Changer de granularité plutôt que déplacer un test | Chantier C.2 : classification AABB une fois par mesh plutôt qu'un test near par face | L04 P2 §6.2, règle 22 |
| Itérateur proxy `ComponentView` | Interroge directement les pointeurs typés capturés plutôt que de repasser par le Registry générique à chaque entité | L03 Partie V |

**Écarts assumés et documentés (pas des dérives silencieuses) :**

- `FragmentCallback` est un pointeur de fonction C brut, pas un template — ça empêche l'inlining dans la boucle pixel. Choix pédagogique temporaire explicitement signalé : *« Reporté volontairement pour ne pas mélanger apprentissage du mécanisme et complexité template »* (L04 P1 §6).
- `LV3_TILE_SIZE` et `LV3_FEATURE_MULTITHREAD` sont définis dans `EngineSettings.h` depuis la Leçon 1, mais **rien dans les Leçons 4 et 5 n'implémente encore le rendu par tuiles** : `RasterizeTriangle` balaie une bounding box classique, sans découpage en tuiles ni multithread. Le postulat est déclaré mais pas encore réalisé sur cet axe.

**Dette non résolue à surveiller :**

- `MeshClass::GetFaceView` suppose les sommets **contigus** dans `vertexPositions` (dette notée en L05 §9). Le layout mémoire réel du mesh (AoS classique vs SoA par attribut) n'a pas encore été tranché par une décision dictatoriale, contrairement au repère ou au NDC.
- Le seul point de rupture volontaire de type-safety du moteur est le `void*` de `FragmentContext` — accepté et encadré (règle 21 : un seul type de contexte, sentinelle `magic`), mais à garder sous surveillance si le nombre de contextes de shading augmente.

**Verdict :** le moteur reste fidèle au postulat CPU/DOD. Les deux seules zones où la théorie précède la pratique (tiling, template du callback) sont documentées comme telles dans les leçons elles-mêmes, cohérent avec la méthode du cours (« mesurer avant de garder », « comparer la topologie avant les performances »).

---

## 2. Architecture et schéma fonctionnel

### 2.1 Pipeline de rendu (une frame)

```
InputSystem → Contrôleurs (FPS/Follow/Animation, écrivent m_local)
    → LocalTransformSystem (m_local → m_localMatrix)
    → WorldTransformSystem (propagation hiérarchique)
    → FindActiveCamera + BuildViewData (View, Projection, V·P, Frustum)
    → RenderView (par mesh) :
         Frustum::Classify(AABB monde)  →  Outside : skip
         needsNearClip = (Intersect)
         pour chaque face :
             Local → Monde → Vue → CLIP SPACE (MVP)
             si needsNearClip : ClipTriangleNear (Sutherland-Hodgman 4D)
             ÷w → NDC → Viewport::ToRaster (flip Y)
             IsBackFacing(EdgeFunction) → rejet
             RasterizeTriangle (edge function, top-left rule)
                 → early-Z (z_ndc affine, GREATER, reverse-Z)
                 → interpolation perspective-correcte (invW) des varyings
                 → Fragment (écriture FrameBuffer)
    → SDL_UnlockTexture / RenderCopy / RenderPresent
```

### 2.2 Les quatre couches du rendu — et comment elles se parlent

| Couche | Rôle | Ignore |
|---|---|---|
| **RenderSystem** | Géométrie : transforme, cull, projette → Triangle2D écran | ne connaît pas le rasterizer |
| **Renderer** | État : détient FrameBuffer/DepthBuffer/Viewport/Mode | ne connaît pas le Registry |
| **Rasterizer** | Balayage : bounding box, edge function, top-left, barycentriques | ignore le contenu du contexte de shading |
| **Fragment** | Pixel : test de profondeur, écriture couleur | ignore RenderSystem entièrement |

Chaque couche appelle uniquement la couche du dessous, avec des données explicites — jamais d'état partagé implicite, jamais d'appel remontant.

```
RenderSystem                    Renderer                       Rasterizer                    Fragment
─────────────                   ────────                       ──────────                     ───────
Registry + ResourceManager      possède : FrameBuffer*,         reçoit : v0,v1,v2 (écran),     reçoit : (x,y,bary,
+ ViewData (V, P, VP, Frustum)  DepthBuffer*, Viewport,         width, height,                 void* userData)
       │                        mode courant (ÉTAT, pas         FragmentCallback,
       │ pour chaque mesh :     un paramètre)                   void* userData
       │  - Frustum::Classify          │                               │                              │
       │  - clip near si besoin        │                               │                              │
       │  - ÷w → NDC → raster          │                               │                              │
       │  - backface culling           │                               │                              │
       │                               │                               │                              │
       └──► renderer.DrawTriangle(     │                               │                              │
              Triangle2D{v0,v1,v2,     │                               │                              │
              z0,z1,z2,invW0..2},      │                               │                              │
              color)                  │                               │                              │
                                       │                               │                              │
                          choisit le fragment shading             │                              │
                          selon le mode courant, construit         │                              │
                          le contexte (Unlit/Depth/                │                              │
                          FragmentContext + magic),                │                              │
                          appelle ─────────────────────────────►  │                              │
                                       RasterizeTriangle(v0,v1,v2,        │                              │
                                       w,h, ShadeFragment_X, &ctx)        │                              │
                                                                     bounding box, edge function,  │
                                                                     biais top-left, pour           │
                                                                     chaque pixel couvert :         │
                                                                     calcule bary, appelle ────►   │
                                                                     onFragment(x,y,bary,ctx)              │
                                                                                                     AsFragmentContext(void*)
                                                                                                     (assert magic)
                                                                                                     early-Z (TestAndSet)
                                                                                                     si rejeté → return
                                                                                                     sinon : num/den (invW),
                                                                                                     SetPixel(FrameBuffer)
```

**Ce qui traverse chaque frontière — et ce qui n'y traverse pas :**

| Frontière | Ce qui passe | Ce qui NE passe PAS |
|---|---|---|
| RenderSystem → Renderer | positions écran, `z` (affine), `invW` par sommet, une `Color` | aucune connaissance du Registry, aucun `ERenderMode` en paramètre (déjà posé via `SetMode`) |
| Renderer → Rasterizer | 3 sommets écran, dimensions, un pointeur de fonction, un `void*` opaque | Renderer ne connaît pas le contenu du contexte qu'il construit — seul `Fragment` sait le décoder |
| Rasterizer → Fragment | `(x, y, bary, void* userData)`, à chaque pixel couvert | Rasterizer ignore ce qu'un fragment fait de ces données, et l'existence du FrameBuffer |
| Fragment → (rien ne remonte) | écrit directement dans `FrameBuffer`/`DepthBuffer` partagés, bindés une fois par `Renderer::BeginFrame` | aucune valeur de retour ne circule vers Rasterizer, Renderer ou RenderSystem |

**Le seul point de couplage volontairement « faible » du système est le `void*`.** C'est pour ça que la règle 21 (un seul type de contexte derrière le `void*`, `magic` en premier membre, vérifié en Debug) existe : c'est la seule frontière où le compilateur ne peut rien garantir, donc la seule qui reçoit une garde manuelle explicite. Le bug n°17 (contextes divergents castés depuis `void*`, offsets décalés) est la preuve empirique que cette frontière est le point de fragilité réel de l'architecture.

**Ce que ce découplage achète concrètement :** le mode d'affichage devient un état de `Renderer`, pas un paramètre de `RenderView`. C'est ce qui permet le split-screen de la Leçon 5 (§4.2) — deux `SetMode` + deux `RenderView` sur le même `FrameBuffer`/`DepthBuffer` — sans que `RenderSystem` sache qu'il existe plusieurs modes de rendu.

### 2.3 Architecture des fichiers (LIB / EXE)

```
LIB — LibraryV3
  Core/            Compiler.h, Config.h, CoreTypes.h (pch) · Platform.h, EngineSettings.h (hors pch)
  Maths/           Vec2/3/4, Mat3/4, Quat, Transform, Projection.h/.cpp, geometry/{Plane,AABB3d,Frustum}
  ECS/             Registry (entités versionnées), SparseSet<T> (paginé), ComponentView (itérateur proxy)
  Resources/       ResourceManager (handles Mesh/Material/Texture, chemins canoniques)
  Rendering/       Viewport, ViewData, FrameBuffer, DepthBuffer, ClipVertex, Clipper,
                   Rasterizer.h/.cpp, Fragment.h/.cpp, Renderer.h/.cpp, RenderTypes.h
  Scene/           Components (Camera, Transform, FPSController, CameraFollow, Hierarchy…),
                   System.hpp/.cpp, RenderSystem.h/.cpp, Serializer (JSON)

EXE — application
  main.cpp (SDL : fenêtre, texture STREAMING ARGB8888, boucle), BuildInputState()
```

**Principe de placement** : un fichier appartient à la LIB si son comportement ne changerait pas en remplaçant SDL par Win32 GDI ; sinon il reste dans l'EXE. `Matrix44` ne connaît que l'algèbre pure — toute fonction qui connaît une convention de rendu (near/far, NDC, main droite, reverse-Z, flip Y) en sort vers un fichier dédié (`Projection.h`, `Quat::LookRotation`).

> ⚠️ **Point de continuité non résolu.** La **Leçon 2** avait tranché : repère **main gauche**, Y haut, **+Z avant**, stockage **colonne-majeur**, convention **vecteurs-colonnes** (`v' = M·v`). La **Leçon 5** a retranché différemment : repère **main droite**, Y haut, **-Z avant**, stockage **row-major**, convention **vecteur-ligne** (`v' = v·M`). Ce revirement est documenté comme dette technique non résolue dans la Leçon 5 elle-même (§9) : *« Leçon 02 affirme encore "main gauche, +Z entre dans l'écran" — documentation mensongère »*. `Lecon_02_Mathematiques.md` n'a pas encore été corrigée pour refléter la convention réellement en vigueur depuis la Leçon 5 — voir §5.3 « Prochaines étapes ».

### 2.4 Architecture et schéma fonctionnel du Scenegraph

⚠️ **Avertissement de méthode.** Le scenegraph n'a pas encore reçu de leçon dédiée. Ce qui suit est reconstitué par recoupement entre la Leçon 3 (Partie VI, hygiène systémique) et la Leçon 5 (§6, ordre des systèmes) — pas extrait d'une leçon qui l'a traité comme sujet central. Le confirmé, le déduit et le non documenté sont volontairement séparés ci-dessous ; c'est précisément ce qui motive la Leçon 06 (voir §5.1).

#### Ce qui est confirmé

**Composants impliqués :**
- `TransformComponent` : embarque `LV3::Transform` (position + quaternion + scale — L02 §4.1), plus `m_local`, `m_localMatrix` (cache), `m_worldMatrix` (cache), et un flag `m_dirty`.
- `HierarchyComponent` : porte au minimum un `children` (`std::vector<Entity>`) — confirmé par le bug F6 corrigé (copie profonde de ce vecteur à chaque nœud, à chaque frame, dans une récursion → corrigé en passant par `const auto&`).

**Systèmes et ordre d'exécution (Leçon 5, §6 — « non négociable ») :**

```
1. InputSystem                                →  InputState
2. Contrôleurs (FPS / Follow / Animation)      →  écrivent m_local, lèvent m_dirty
3. LocalTransformSystem                        →  m_local → m_localMatrix (consomme m_dirty)
4. WorldTransformSystem                        →  propagation DESCENDANTE depuis les racines
5. FindActiveCamera + BuildViewData            →  une fois PAR VUE
6. RenderView                                  →  une fois PAR VUE
```

**Bug classique documenté sur cet ordre :** exécuter l'étape 5 avant que l'étape 4 soit terminée fait lire à la caméra la matrice monde de la **frame précédente** — un décalage d'une frame, presque invisible en statique, très perceptible dès qu'on bouge la souris.

**Point d'architecture confirmé et important :** la caméra **n'est pas un cas spécial** du scenegraph. Elle porte un `CameraComponent` (lentille pure : fov/near/far) + un `TransformComponent` ordinaire, et traverse exactement le même `WorldTransformSystem` que n'importe quelle entité (L05, Règle 1 : *« si tu veux savoir où est la caméra, tu demandes à son Transform, comme pour n'importe quel objet de la scène »*). Il n'existe pas de chemin de code séparé pour la caméra dans la propagation hiérarchique.

**Le bug F6 corrigé (Leçon 3, Partie VI) est le seul aperçu qu'on ait du fonctionnement interne de `WorldTransformSystem` :**

```cpp
// AVANT — copie profonde à chaque nœud, à chaque frame
HierarchyComponent children = registry.getComponent<...>(entity);

// APRÈS — référence, zéro allocation heap
const auto& children = registry.getComponent<HierarchyComponent>(entity).children;
```

C'est un bug directement lié au postulat DOD : une récursion qui copie un `std::vector<Entity>` à chaque nœud visité alloue sur le tas à chaque frame, sur une opération qui devrait être une simple lecture.

**Autre point vérifié (Leçon 3, Partie VI) :** la matrice racine transmise à `WorldTransformSystem` n'est, dans le code réel, **jamais modifiée en place** — seulement multipliée pour produire `m_worldMatrix`. Le risque redouté à l'audit initial (rotation cumulative frame après frame) ne s'est pas matérialisé. Vigilance notée : ce paramètre *devrait* être `const Matrix44f&` pour l'interdire à la compilation plutôt que par discipline — recommandation encore ouverte.

#### Ce qui est déduit (par cohérence, pas cité littéralement)

- La composition monde = local · parent, très probablement dans le même sens que celui observé pour la MVP (`modelMatrix * view.viewProjectionMatrix`, L05 §5.7) — cohérent avec la convention vecteur-ligne actée en Leçon 5. **Non confirmé par un extrait de code de `WorldTransformSystem` lui-même.**
- La « propagation descendante depuis les racines » implique une notion de racine(s) de scène — un ou plusieurs `Entity` sans parent, ou un `Entity` conventionnel de scène-racine. **Le mécanisme de détection des racines n'est décrit dans aucune leçon.**

#### Ce qui n'est pas documenté (angles morts — agenda de la Leçon 06)

| Question ouverte | Pourquoi ça compte |
|---|---|
| `HierarchyComponent` a-t-il un champ `parent` explicite, ou seulement des listes de `children` descendues depuis les racines ? | Détermine si un ré-parentage ou une remontée (world→local) sont possibles en O(1) ou nécessitent un scan |
| Que devient un enfant quand son parent est détruit (`DestroyEntity`) ? | La Leçon 3 documente `DestroyEntity` pour les component storages, mais rien sur la cascade hiérarchique — risque d'entités orphelines avec un handle de parent périmé |
| Le `Serializer` (JSON) sait-il lire/écrire les relations parent/enfant ? | Les extraits vus (`ParseMesh`, `ParseCamera`, `ParseTransform`) ne couvrent pas la hiérarchie |
| Y a-t-il un TRS local vs monde distinct pour le scale (composition non-uniforme en cascade) ? | La Leçon 2 signale déjà le piège des normales sous scale non-uniforme (transposée de l'inverse) — un scale hérité en cascade complique encore ce calcul |
| Multi-threading de `WorldTransformSystem` | Le postulat DOD/parallélisation (L03 §1.3) promet du multithread sur des systèmes touchant des composants disjoints — une récursion parent→enfant est par nature séquentielle sur une branche, à documenter si le sujet est repris |

---

## 3. Toutes les règles dictatoriales, par leçon

### Leçon 01 — Config, Enums, PCH

| Règle | Résumé |
|---|---|
| PCH figé | Rien dans `pch.h` ne doit jamais changer — sinon recompilation totale |
| Frontière compile-time | `Config.h`/`CoreTypes.h` (stable) → PCH ; `EngineSettings.h`/`RenderTypes.h`/etc. (évolutif) → hors PCH |
| `Platform.h` hors PCH | `windows.h` pollue les macros globales (`min`, `max`, `near`, `far`) |
| `enum class` obligatoire | Jamais d'enum nu ; type sous-jacent explicite (`: uint8_t`) toujours précisé |
| Filtre `CoreTypes.h` | Un enum y entre si liste close **et** multi-système ; sinon header du système concerné |
| `#if` plutôt que `#ifdef` | Pour les feature flags — désactiver sans supprimer le define |

### Leçon 02 — Mathématiques

| Décision | Résumé |
|---|---|
| A — Repère | Main gauche, Y haut, +Z avant *(révisé en Leçon 5 — voir §2.3)* |
| B — Matrices | Vecteurs-colonnes, colonne-majeur *(révisé en Leçon 5)* |
| C — Profondeur | NDC `[0,1]` (Direct3D-like), `float` partout, `double` réservé aux rares accumulations |
| Lib maison vs GLM | Justifiée par l'objectif pédagogique et le contrôle du layout mémoire (SIMD, tiling) |
| PCH des maths | Hors PCH pendant le développement ; candidat au PCH une fois figé |

### Leçon 03 — ECS / SparseSet / ResourceManager (13 règles, Partie X)

| # | Règle |
|---|---|
| 1 | Une `Entity` est un ticket daté ; tout accès tableau passe par `EntityIndex(e)` |
| 2 | `IsAlive` = comparaison de génération, jamais de recherche dans la free-list |
| 3 | `DestroyEntity` : notifier les storages **avant** d'incrémenter la génération |
| 4 | Sparse indexé par `EntityIndex` ; le miroir stocke le handle complet |
| 5 | Un itérateur qui fabrique sa valeur la retourne **par valeur** (itérateur proxy) |
| 6 | Ne jamais repayer une résolution déjà capturée (pointeurs typés du `ComponentView`) |
| 7 | Un composant chaud est un POD : il référence les ressources par handle, jamais ne les possède |
| 8 | Un chemin de ressource est canonique avant d'être une clé de cache |
| 9 | Une map à sens unique s'accompagne de son miroir dès que l'inverse est nécessaire |
| 10 | Un typeID runtime ne se sérialise jamais |
| 11 | `Emplace` plutôt que `Add` pour un objet créé une seule fois avec indirection interne |
| 12 | Le RAII est déterministe — jamais s'appuyer sur le nettoyage de l'OS |
| 13 | Tout invariant binaire se verrouille par `static_assert` |

*(+ B7 : sparse paginé par pages de 4096 pour borner le coût mémoire du tableau creux)*

### Leçon 04 Partie 1 — Rasterizer, Triangle Setup (8 règles, numérotation continuée en Partie 2)

| # | Règle |
|---|---|
| 1 | Rasterizer intégralement basé edge function (Pineda) — aucun scanline |
| 2 | Test de signe et cull mode calibrés ensemble, avec test unitaire explicite |
| 3 | Top-left rule obligatoire (biais `0`/`-1` par arête) |
| 4 | Test au **centre** du pixel (`+0.5f`), jamais au coin |
| 5 | `SDL_LockTexture` une fois par frame, jamais par pixel |
| 6 | Toute indexation dans un buffer SDL passe par le `pitch`, jamais par `width` recalculé |
| 7 | Format de pixel fixé explicitement (`ARGB8888`) |
| 8 | Toute fonction SDL retournant pointeur/code d'erreur est vérifiée immédiatement |

### Leçon 04 Partie 2 — Clipping, Perspective, Profondeur (règles 9 à 25, suite de la Partie 1)

| # | Règle |
|---|---|
| 9 | Le clipping se fait en clip space, avant la division par `w` |
| 10 | Un seul plan clippé : near. Latéraux = scissor, far inexistant |
| 11 | Test near sous forme canonique `w − z ≥ 0`, jamais avec un paramètre de lentille |
| 12 | `ClipTriangleNear` retourne jusqu'à 4 sommets ; on triangule avant de clipper |
| 13 | Interpolation au clipping = linéaire en clip space (exacte) |
| 14 | Varying = `a·invW` divisé par `invW` interpolé, **sauf** `z_ndc` (affine) |
| 15 | La pré-division `a·invW` se fait au triangle setup, jamais dans la boucle pixel |
| 16 | Z-buffer : clear `0.0f`, test `GREATER` — vérifiés ensemble |
| 17 | Early-Z : le test de profondeur précède tout autre calcul du fragment |
| 18 | `DepthBuffer` possède sa mémoire → `y*width+x` (règle du pitch réservée à `FrameBuffer`) |
| 19 | Un seul test de backface : signe de l'aire raster (`ClipOrientation` différé) |
| 20 | Le choix de l'algorithme de clipping suit la topologie de sortie (polygone → Sutherland-Hodgman ; segment → Liang-Barsky/Cyrus-Beck) |
| 21 | Un seul type derrière le `void*`, `magic` en premier membre, vérifié en Debug |
| 22 | Quand un test par élément coûte trop cher, changer de granularité plutôt que déplacer le test |
| 23 | Un point d'intersection partagé entre deux primitives se calcule dans un ordre canonique |
| 24 | La fonction d'arête s'écrit sous forme antisymétrique (`E(b,a,p) = −E(a,b,p)` exact) |
| 25 | Le `#ifdef` conditionne le corps d'une fonction, jamais sa signature |

### Leçon 05 — Caméra, Frustum, Pipeline (11 règles)

| # | Règle |
|---|---|
| 1 | « La caméra » n'existe pas : Transform + Lentille + Viewport + Frustum, indépendants |
| 2 | La caméra ne bouge jamais — `View = inverse(WorldMatrix)`, inverse rigide analytique |
| 3 | Pipeline des espaces : cull dans le monde, clippe dans le clip, flip Y seulement au dernier maillon |
| 4 | Culling et clipping sont deux métiers distincts (conservatif vs exact) |
| 5 | Extraction Gribb-Hartmann depuis la matrice, jamais depuis les 8 coins |
| 6 | Test AABB par p-vertex/n-vertex (1 ou 2 coins), jamais les 8 |
| 7 | FOV vertical par convention ; aspect ratio appartient au viewport, pas à la lentille |
| 8 | Profondeur : NDC `[0,1]`, reverse-Z, far infini |
| 9 | Ordre du culling en production : distance/LOD → frustum → occlusion → backface → dégénérés |
| 10 | API caméra : View/Projection/VP/InverseVP/Frustum/Ray/CullingMask exposés explicitement |
| 11 | Le contrôleur n'est pas la caméra : Input → Controller → Transform → View, flèches unidirectionnelles |

---

## 4. Tous les bugs, correction et leçon

### Leçon 03 — Audit ECS/ResourceManager

| Bug | Cause | Correction |
|---|---|---|
| `m_Sparse[entity]` brut au lieu de `EntityIndex(entity)` | Paramètre `EnsureSparseFits` mal nommé (`entity` au lieu d'`idx`) | Renommage explicite du paramètre |
| Mesh chargé mais jamais assigné (`Serializer::ParseMesh`) | `MeshHandle` calculé et validé, jamais écrit dans le composant | Migration vers référencement par handle (F4) |
| `UnloadMesh` en O(N) | Pas de map inverse id→chemin | Ajout du miroir `m_meshIdToPath` (F5) |
| Chemins de ressources incohérents (casse/séparateurs) | Pas de canonicalisation | `CanonicalKey()` via `fs::weakly_canonical` (F5) |
| Erreurs de chargement non typées | `MeshHandle::Invalid()` binaire seulement | `std::expected<MeshHandle, EMeshLoadError>` (F5) |
| Code mort `if (rawFaces.empty())` dans `OBJLoader::Load` | Toujours faux, jamais atteint | Noté, non corrigé (impact nul) |
| Divers B1-B8/C1-C6/D1-D3 (assert manquants, `INVALID_INDEX` non constexpr, faute de frappe `ConstainsEntity`, etc.) | — | Tous corrigés, voir tableau de clôture de la leçon |

*Restent ouverts : D5 (thread-safety ResourceManager), D6 (nommage dossier `Ressources`) — non structurants.*

### Leçon 05 — Legacy Camera/Frustum (bugs de correction B1-B16)

Parmi les plus significatifs : `Frustum::Rebuild` composait `proj * view` (ordre inversé, tous les plans faux) ; coins far calculés depuis le centre du near (frustum plat) ; `getFoV()` renvoyait le demi-FOV ; mélange de deux sources d'orientation (lookAt + quaternion) ; rotation souris dépendante du framerate ; inversion avant/arrière du déplacement ; UP dégénéré non protégé ; `getMatrix()`/`getViewMatrix()` confondues alors qu'inverses l'une de l'autre ; mesh partagé écrasé par la transformation locale. Tous corrigés par la réécriture complète en Leçon 5.

### Leçon 05 — Bugs d'implémentation (journal numéroté, continué en Leçon 04 P2)

| # | Bug | Détecté par | Correction |
|---|---|---|---|
| 1 | `proj * view` au lieu de `view * proj` | audit du code | ordre corrigé |
| 2 | `Vec3::Forward()` valait +Z en main droite | TNR | corrigé à -Z |
| 3 | Garde anti-division `EPSILON_FLOAT` trop grande pour la normale du plan far | TNR | seuil ajusté |
| 4 | `16/9` en division entière → aspect = 1.0 | TNR | cast en float |
| 5 | Double conversion degrés→radians | trace du pôle | conversion unique |
| 6 | JSON itéré par ordre alphabétique → rayon d'orbite nul | log de chargement | ordre de parsing revu |
| 7 | Rayon d'orbite contaminé par Y | invariant `\|xz\|==R` | calcul restreint à xz |
| 8 | Struct `Transform` fantôme masquant le vrai, `scale=(0,0,0)` | `#pragma message(__FILE__)` | struct dupliquée supprimée |
| 9 | `Entity{}` valait 0 (entité valide) au lieu de `NULL_ENTITY` | relecture | `NULL_ENTITY = 0xFFFFFFFF` |
| 10 | Rotation composée `spin*initial` → précession parasite | trace du pôle | ordre de composition revu |
| 11 | `"texte" + entier` = arithmétique de pointeur | compilation | concaténation correcte |
| 12 | Chaînes ANSI faute de `/utf-8` | affichage console | flag compilateur ajouté |
| 13 | `RGB` = macro `wingdi.h` → `C4430` | renommage | `MakeColor` |
| 14 | `LV3_FORCEINLINE` déclarée .h / définie .cpp → `LNK2019` | édition de liens | corps déplacé dans le .h |
| 15 | Surcharges `EdgeFunction` empilées → `C2665` | compilation | remplacées par 2 gabarits |
| 16 | `.cpp` absents du `.vcxproj` | `LNK2019` en série | fichiers ajoutés au projet |
| 17 | `FragmentContext` divergents castés depuis `void*` | mode Depth aberrant | sentinelle `magic` + `AsFragmentContext` |
| 18 | Biais top-left `-1.0f` absolu sur grandeur relative → trous sur petits triangles | bruit visuel en 3D | biais relatif à l'aire |

### Leçon 04 Partie 2 — Journal des bugs (suite, 19-27)

| # | Bug | Détecté par | Correction |
|---|---|---|---|
| 19 | Champ ajouté à `FragmentContext`, oublié dans `DrawTriangle` | écran noir (`1/0=inf`) | remplissage complété |
| **20** | **Backface culling inversé depuis la Leçon 5** | mode Depth (couleur à sens géométrique) | `IsBackFacing` : aire ≥ 0 rejetée (flip Y inverse le winding) |
| **21** | **124 trous sur les coutures du clipping** | `Test_ClipCoverage` (comptage) | `EdgeFunction` antisymétrique + `ClipLess` (ordre canonique) |
| 22 | `EdgeFunction` dédoublée (surcharge modifiée sans le noyau) | audit du dépôt | délégation unique restaurée |
| 23 | `ParseCamera` : comparaison au lieu d'affectation (`!=` au lieu de `=`) | audit du dépôt | corrigé le 22/08 |
| 24 | `ShadeFragment_Barycentric` contournait `AsFragmentContext` | audit du dépôt | déjà corrigé, vérifié 22/08 |
| 25 | FPS + Follow actifs simultanément sur la même caméra | dump des contrôleurs | invariant `CheckControllerExclusivity` ajouté (22/08) |
| 26 | Clés JSON écrites mais jamais lues (`active`/`priority`) | `JsonReader::WarnUnread` | ⏳ ouvert |
| 27 | `SDL_LockTexture` "Invalid call" au redimensionnement | test de resize | ⏳ ouvert |

---

## 5. Prochaines étapes

### 5.1 Leçon 06 — Le Scenegraph (à venir)

Motivée directement par les angles morts identifiés en §2.4. Agenda proposé :

1. Fixer le contrat de `HierarchyComponent` : champ `parent` explicite ou déduction par listes de `children` seules ; API de ré-parentage.
2. Définir la cascade de `DestroyEntity` sur la hiérarchie : que deviennent les enfants d'une entité détruite (destruction en cascade ? orphelinage explicite ?).
3. Étendre le `Serializer` JSON pour lire/écrire les relations parent/enfant.
4. Trancher la composition du scale en cascade (scale non-uniforme hérité + transformation des normales, cf. piège déjà signalé en Leçon 2).
5. Documenter (ou différer explicitement, comme `ClipOrientation`) la parallélisation de `WorldTransformSystem`, cohérente avec le postulat DOD de la Leçon 3.

### 5.2 Bugs ouverts

| # | Bug | Leçon d'origine |
|---|---|---|
| 26 | Clés JSON écrites mais jamais lues (`active`/`priority` de `CameraFPS`) | L04 P2 |
| 27 | `SDL_LockTexture` « Invalid call » au redimensionnement (texture recréée pendant une frame en cours) | L04 P2 |

### 5.3 Dettes techniques ouvertes

**ECS / Ressources**
- D5 — Absence de thread-safety du `ResourceManager` (à traiter si le moteur devient multi-thread). *(L03)*
- D6 — Nommage français du dossier `Ressources` — cosmétique. *(L03)*
- `TriggerSystem` en O(N²) — broad-phase spatiale (grille ou quadtree) à ajouter avant la narrow-phase sphère/sphère. *(L03, rappelé L04 P2 et L05)*
- `OBJLoader::ParseFile` — granularité d'erreur insuffisante (`bool` qui fusionne « fichier vide » et « aucune face valide »). *(L03)*

**Rendering**
- `ClipVertex` sans `uv`/`normal` — à ajouter **avec** `Lerp()`, sous peine de bug fantôme sur les triangles clippés. *(L04 P2)*
- `MeshClass::GetFaceView` suppose les sommets contigus — bloque le texturing et l'éclairage tant que non résolu. *(L04 P2, L05)*
- `ECullMode` / `EDepthTest` / `EBlendMode` déclarés mais non exploités par `Renderer` — trois enums sans usage effectif. *(L04 P2, L05)*
- `ClipOrientation` (culling avant division) — chantier différé sous condition de profilage, match nul mesuré face au coût actuel. *(L04 P2, règle 19)*
- Matériaux : `submeshes` ignorés, couleur actuelle par hash de face — pas de rendu réaliste possible en l'état. *(L05)*
- `FragmentCallback` non templaté — empêche l'inlining, report pédagogique assumé (voir §1.1). *(L04 P1)*
- `ComputeMeshAABB()` n'est appelée nulle part automatiquement — risque de `meshAABB` invalide si l'`OBJLoader` l'oublie. *(L05)*

**Performance / DOD**
- Rendu par tuiles et multithread (`LV3_TILE_SIZE`, `LV3_FEATURE_MULTITHREAD`) déclarés en Leçon 1, non implémentés à ce stade (voir §1.1).
- Layout mémoire du mesh (AoS vs SoA) non tranché par une décision dictatoriale.

**Documentation**
- `Lecon_02_Mathematiques.md` décrit encore le repère main gauche / +Z avant / colonne-majeur, obsolète depuis la Leçon 5 (main droite / -Z avant / row-major) — à corriger (voir §2.3).

---

*Document de synthèse — les prochaines leçons (06 : Scenegraph, puis texturing/échantillonnage annoncé en clôture de la Leçon 4 Partie 2) viendront combler les dettes listées en §5.*
