# Audit pré-Leçon 06 — Le Scenegraph

> **Moteur :** LibraryV3 (`onkover/LibraryV3`, commit `8b4498f`) | **EXE :** `onkover/RefactoNouvelleLibrairieV3`
> **Date :** 01/09/2026 | **Périmètre :** dossier `Scene/` du moteur, boucle de jeu de l'EXE, scènes JSON
> **Méthode :** audit mené sur les sources GitHub clonées le jour même — pas sur les notes des leçons.

---

## 0. Objectif et fil rouge

La Leçon 06 traite le scenegraph comme sujet central (annoncé au §5.1 du Récapitulatif 01-05).
**Fil rouge de la leçon : remettre `solar_system.json` en service.** La scène système solaire est
l'objectif du projet ; `SceneBasicTest.json` n'est qu'un pis-aller. Le §4 de cet audit établit le
diagnostic précis de sa panne : **aucune des deux causes n'est dans le graphe lui-même**.

---

## 1. Rapprochement notes ↔ code actuel

### 1.1 Fermé depuis les leçons/annexes (vérifié dans le code)

| Point | Preuve dans le code |
|---|---|
| F1 — Entity ticket daté (24 bits index / 8 bits génération) | `Entity.hpp` : `MakeEntity`, `NULL_ENTITY = 0xFFFFFFFF`, static_asserts |
| F3 — itérateur `ComponentView` sans passage par le Registry | `Registry.hpp` : tuple de storages typés, fold `&&` |
| F4/F5 — mesh par handle, `std::expected`, `LoadMeshChecked` | `ParseMesh`, `ResourceManager` |
| B7 — sparse paginé | `PagedSparseArray` (pages 4096, allocation paresseuse) |
| Bug 26 — `active`/`priority` jamais lus | `ParseCamera` les lit et arbitre par priorité |
| Bug 27 — `SDL_LockTexture` au resize | `resizePending` : recréation différée après l'unlock (à valider par test) |
| Bug 39 — `ParseCamera` sans `std::move` | move ajouté… et un défaut neuf introduit (bug 43) |
| R23 — `engine.json` dans l'EXE | le fichier vit dans le repo consommateur |
| Migration `JsonReader` + `WarnUnread` | tous les parsers, `ParseTrigger` et `PlayerControl` inclus |

### 1.2 Questions ouvertes du Récap §2.4, tranchées par lecture

- `HierarchyComponent` a un `m_parent` explicite **plus** un `m_isRoot` → double source de vérité (constat A1).
- Le Serializer gère la hiérarchie : deux passes, `ParseHierarchy` (avec les bugs 40/41).
- Cascade de `DestroyEntity` sur la hiérarchie : **toujours absente** (constat A2).
- Scale en cascade : implicite via les matrices, sans décision documentée — à trancher en leçon.
- Parallélisation de `WorldTransformSystem` : néant ; la récursion actuelle l'interdit (constat A3).

### 1.3 Toujours ouvert, hors périmètre Leçon 06

TriggerSystem O(N²) sans broad-phase ; D5 thread-safety ResourceManager ; enums
`ECullMode`/`EDepthTest`/`EBlendMode` sans usage ; `GetFaceView` et la contiguïté ; matériaux
par submesh ignorés ; texturing (leçon suivante annoncée).

---

## 2. Journal des bugs (suite — n° 40 à 51)

| # | Bug | Où | Gravité | Statut |
|---|---|---|---|---|
| 40 | `linkChildToParent` **écrase** le `HierarchyComponent` existant de l'enfant → si l'enfant était déjà parent d'autres nœuds, sa liste `m_children` est détruite. Latent : ne se déclenche que si le JSON déclare un enfant avant son parent. Contrat d'ordre implicite, jamais vérifié. | `Hierarchy.cpp:25` (via `SparseSet::Add` écrase-si-présent) | 🔴 perte de données silencieuse | ✅ **fermé** — séance 1, validé par TNR `solar_system_shuffled.json` (13 nœuds, arbre identique) |
| 41 | `ctx.entityMap[parentId]` : `operator[]` **insère** `Entity(0)` pour un parent inconnu → un `"parent"` avec faute de frappe parente silencieusement au **premier nœud de la scène**. Aucun contrôle de doublon d'`id`, ni de cycle. | `Serializer.cpp:572-573` | 🔴 corruption silencieuse | ✅ **fermé** — séance 1, validé par saboteur `"parent": "Erath"` (échec propre, parent et enfant nommés) |
| 42 | `ParseMesh` calcule `fullPath = ResolvePath(baseDir, modelPath)` puis charge… `modelPath`. La résolution de chemin est **court-circuitée** : le chargement dépend du répertoire courant. | `Serializer.cpp:189-195` | 🔴 cause n°2 de la panne solar_system | ✅ **fermé** — séance 1 (chemins `assets/Meshes/` corrigés dans les scènes) |
| 43 | `ParseCamera` lit `c.m_isActive`/`c.m_priority` **après** `std::move(c)`. Bénin tant que `CameraComponent` est trivialement copiable (static_assert), fragile le jour où il ne l'est plus. | `Serializer.cpp:351-359` | 🟠 hygiène de contrat | P2 |
| 44 | `deltaTime = 0.5f` **fixe** dans la boucle SDL temps réel : tout le travail d'indépendance au framerate (lissage `1-exp(-k·dt)`, vitesses en unités/s) tourne sur un temps imaginaire. | `main.cpp:306` | 🟠 EXE | **P1** |
| 45 | `PlayerInputSystem` ne lit aucun input : `position.x += speed·dt` inconditionnel — le vaisseau dérive vers +X à chaque frame. Appelé avant `BuildInputState()`. | `main.cpp:325`, `System.cpp:696` | 🟠 EXE | **P1** |
| 46 | `main.cpp` code en dur `FindCameraByName("FPS_Camera")` / `"Top_Camera"` et ignore l'`activeCamera` élue par priorité par le Serializer. Deux vérités pour la caméra active ; crash assuré sur toute scène sans `Top_Camera`. | `main.cpp:298-299` → `LV3_ASSERT` dans `BuildCameraBindings` | 🔴 cause n°1 de la panne solar_system | **P1** |
| 47 | `SceneGraph.cpp/.hpp` : fichier mort (retiré du `.vcxproj`), ne compilerait pas (`pWorld` inexistant), garde d'include cassé (`#endif` dans le namespace), **seconde définition** de `linkChildToParent` (ODR latent si ré-ajouté au build) — et `main.cpp` l'inclut toujours. Blocs de code mort massifs dans `Serializer.cpp`, `Hierarchy.hpp`, `DebugGizmos.cpp`. | `Scene/SceneGraph.*`, `main.cpp:47` | 🔴 hygiène bloquante | ✅ **fermé** — séance 1 (purge effectuée) |
| 48 | `JsonReader::ReadVector` fait `m_j[key]` sur un json **const** : clé absente = `JSON_ASSERT`/UB (variante nlohmann de R28 — `[]` const *exige*, `[]` non-const *insère*, aucun n'est un lookup). Jamais déclenché avant car toutes les scènes écrivaient des `Transform` complets : premier Transform partiel légitime (`Sun_Danger_Zone`, scale seul) → crash. Détecté en séance 1 par le TNR. Correctif : `find()` + défaut, aligné sur l'idiome `value()` de `Read()` ; audit des autres `m_j[...]` du lecteur (`Child()` inclus). | `Core/JsonReader.h` | 🔴 crash sur donnée valide | ✅ **fermé** — séance 1, validé (`Sun_Danger_Zone`, scale seul, charge avec défauts) |
| 49 | `JsonReader::Read` : clé présente avec un **type incompatible** (`"fov": "45"`) → `value()` lève `type_error.302`, et `LoadSceneGraph` n'a aucun catch autour du parsing des nœuds (son `try` ne couvre que `file >> sceneData`) → `terminate()`. Le filet posé par l'A6 dans `EngineConfig::LoadFromJson` n'avait jamais été étendu au Serializer. Correctif : try/catch **dans `Read()`** (défaut + warning nommant la clé, conforme R7) — pas de catch global qui avorterait la scène entière. En annexe : garde `is_object()` manquante dans `ForEachChild` (asymétrie avec `ForEachElement`), et `Child()` muet sur une clé présente mais ni objet ni tableau. | `Core/JsonReader.h`, `Serializer.cpp` | 🔴 crash sur faute d'auteur | ✅ **fermé** — séance 1, validé par saboteur `"radius": "grand"` (warning nommant la clé, défaut 1.0, scène complète) |
| 50 | `Test_GizmoMatchesFrustum` : `LV3_ASSERT(checked > 0)` confond « test sans objet » (scène sans gizmo, ou aucun propriétaire rendu cette frame — cas que le corps du test tolère déjà via `if (!vd) continue`) et « câblage cassé ». Un test a trois verdicts : succès, échec, **sans objet**. Correctif : le test retourne `checked`, le garde de vacuité déménage dans `main.cpp` (qui sait combien de gizmos la scène a déclarés) — déplacé, pas supprimé. Symptôme A2 relevé au passage : `getComponent<CameraComponent>(giz.m_owner)` assert-era sur un gizmo orphelin de caméra détruite. | `Test/TestAffichageGizmoCamera.cpp`, `main.cpp` | 🟠 faux positif de TNR | ✅ **fermé** — séance 1 (garde de vacuité déplacé dans main.cpp) |
| 51 | **Résurrection du bug 37 (A6 §5.5)** : `ParseTrigger` relit `isColliding` depuis le JSON (champ d'état dérivé, jamais autorisé — R10), et la valeur lue n'est même pas utilisée (`emplaceComponent` passe le littéral `false`) : clé lue qui ne devrait pas l'être + valeur lue puis jetée (R22). Correction A6 probablement jamais commitée — leçon : une correction validée en conversation n'existe qu'une fois dans git. En annexe : la branche « clé absente » de `Read()`/`ReadVector()` logue en `warn` → ~30 faux cris sur une scène valide, le vrai signal (type invalide) noyé ; rétrograder en `info` (hiérarchie : absent+défaut = info, type invalide = warn, structure fausse = error). | `Serializer.cpp` (`ParseTrigger`), `Core/JsonReader.h` | 🟠 régression + bruit de log | **P0** (correctifs de 2 min, prescrits en séance 1) |

---

## 3. Constats d'architecture (le programme de la Leçon 06)

### A1 — `m_isRoot` : deux sources de vérité

"Être racine" est **définissable** : `m_parent == NULL_ENTITY`. Stocker en plus un booléen crée un
deuxième témoin du même fait — et le bug 40 produit déjà des états où les deux divergent
(entité marquée racine *et* dotée d'un parent → propagée deux fois, ou pas du tout).
Même famille que le bug n°8 de la L05 (le `Transform` sosie). Unity n'a pas de flag isRoot :
`transform.parent == null` **est** la définition.
**Décision cible :** supprimer le champ, fonction libre `IsRoot(h)` ; convention à trancher pour les
entités à `TransformComponent` sans `HierarchyComponent` (aujourd'hui : jamais propagées,
`m_worldMatrix` reste l'identité, silencieusement).

### A2 — Le graphe n'a qu'un verbe : construire

Pas de `SetParent`, pas de `Detach`, pas de cascade sur `DestroyEntity` : le parent d'un mort garde
un handle périmé dans `m_children` (sous-arbre gelé, silencieusement écarté par le `TryGet`), les
enfants gardent un `m_parent` périmé. Un scenegraph sans ré-parentage ni destruction n'est pas un
graphe, c'est une photo.
**Décision cible :** API de mutation unique (`SetParent` / `Detach` / `DestroyHierarchy`), politique
de destruction explicite (cascade à la Unity, ou ré-attachement au grand-parent), et le Serializer
comme `SpawnCameraGizmos` passent par elle. La cascade appartient au module Scene, **pas** au
Registry (R20 : mécanisme générique vs politique de domaine).

### A3 — Propagation récursive : l'antithèse du postulat DOD

État des lieux : `LocalTransformSystem` itère dense et linéaire (✅ le modèle à suivre) ;
`WorldTransformSystem` récurse entité par entité avec, **par nœud**, deux `TryGet` (sauts
sparse→dense) et le parcours d'un `std::vector<Entity>` alloué séparément — accès aléatoires en
chaîne, aucune vectorisation ni parallélisation possible. `m_dirty` est à moitié implémenté : il
épargne `m_localMatrix` mais toutes les matrices **monde** sont recalculées chaque frame.
`TransformComponent` pèse ~190 octets (3 lignes de cache) en mélangeant chaud
(`m_localMatrix`/`m_worldMatrix`) et froid (`m_initialRotation`, donnée d'animation).
**Décision cible :** ordre de traversée **aplati** (tri topologique, parents avant enfants),
propagation en une boucle plate sur des indices denses — `world[i] = local[i] * world[parent[i]]`
avec `parent[i] < i` garanti — dirty par sous-arbre, split chaud/froid du composant.
Contre-exemple à éviter : remplacer la récursion par une pile explicite — le problème n'était pas
la pile d'appels, c'était la localité.

---

## 4. Diagnostic : pourquoi `solar_system.json` ne fonctionne plus

Deux causes, indépendantes, **aucune dans le graphe** :

1. **Bug 46** — la scène ne contient pas de nœud `Top_Camera` (elle a `Overview_Camera`).
   `FindCameraByName` retourne `NULL_ENTITY` → `LV3_ASSERT(slots[i].m_camera != NULL_ENTITY)`
   dans `BuildCameraBindings` tue le programme au premier rendu (Debug) ; en Release,
   `BuildViewData` ferait un `getComponent` sur `NULL_ENTITY`.
2. **Bug 42 + chemins** — les `"model"` de la scène pointent sur `assets/sphere 10 faces.obj`
   (sans sous-dossier `Meshes/`), et `ParseMesh` ignore de toute façon le `ResolvePath` qu'il
   calcule. Les mesh ne se chargent pas → `ParseMesh` sort en warning sans poser de
   `MeshComponent` → planètes absentes.

S'y ajoutent, non bloquants : la clé `"texture"` jamais lue (warning `WarnUnread` attendu tant que
le texturing n'existe pas), et la clé de nœud `"type"` ignorée sans contrôle.

**Remise en service = bug 46 (une seule vérité pour les caméras actives, pilotée par la scène) +
bug 42 + correction des chemins du JSON.** C'est le critère de sortie de la Leçon 06.

---

## 5. Priorisation

| Prio | Action | Réfs | Effort |
|---|---|---|---|
| **P0** | `linkChildToParent` sans écrasement ; `find` + validations Serializer (parent inconnu, id dupliqué, cycle) ; test JSON mélangé | 40, 41 | 1 séance |
| **P0** | Suppression de `SceneGraph.*`, du code mort, de l'include fantôme | 47 | 30 min |
| **P0** | `ParseMesh` charge `fullPath` ; chemins de `solar_system.json` corrigés | 42 | 30 min |
| **P1** | Contrat `HierarchyComponent` : mort de `m_isRoot`, convention "sans Hierarchy = racine" | A1 | 1 séance |
| **P1** | API de mutation du graphe + cascade `DestroyEntity` | A2 | 1-2 séances |
| **P1** | `dt` réel (`SDL_GetPerformanceCounter`, clampé) ; sort de `PlayerInputSystem` ; caméras des viewports pilotées par la scène, plus par des noms en dur | 44, 45, 46 | 1 séance |
| **P2** | Fix lecture-après-move ; sémantique `addComponent` vs `addOrReplace` explicite | 43 | 30 min |
| **P2** | Traversée aplatie + dirty par sous-arbre + split chaud/froid (le climax DOD de la leçon) | A3 | 2-3 séances |
| **P3** | Hygiène : `RenderSystem` console legacy homonyme de `RenderView`, `CameraBinding.CPP`, styles d'include, `std::cout` vs Logger, clé JSON `"m_currentHealth"`, `maxFrames`/doubles appels dans `main.cpp` | — | fil de l'eau |

**Logique de l'ordre : d'abord un graphe correct, ensuite un graphe rapide** — la règle top-left
avant le SIMD, une seconde fois.

---

## 6. Règles dictatoriales dégagées (suite de l'Annexe A6)

> **R25 — Une variable cédée est morte.** Plus aucune lecture après `std::move(x)`, sans exception
> mentale "sauf si POD" : la validité ne doit jamais dépendre d'un `static_assert` situé dans un
> autre fichier. Lire avant, céder après.

> **R26 — Une donnée d'entrée ne porte pas d'invariant que le code peut garantir.** "Les parents
> doivent être déclarés avant les enfants dans le JSON" n'est pas une règle de format, c'est un bug
> du chargeur. Le chargeur est invariant par permutation des nœuds, ou il est faux.

> **R27 — Un fait dérivable ne se stocke pas.** `isRoot ≡ (parent == NULL_ENTITY)` : le stocker en
> plus, c'est créer un deuxième témoin qui finira par mentir. Toute donnée dupliquée est une
> divergence en sursis (cf. bug n°8, L05).

> **R28 — `operator[]` d'une map n'est jamais un lookup.** Il insère. Toute recherche dont
> l'absence est une erreur s'écrit `find()` + diagnostic qui parle. `.at()` est réservé aux cas où
> l'absence est un bug interne, pas une donnée utilisateur mal formée.

> **R29 — Un fichier est dans le build, ou il n'est pas dans le repo.** Git est la mémoire du
> projet ; un `.cpp` orphelin du `.vcxproj` est une version parallèle qui attend de tromper
> quelqu'un (contraposée du bug n°16).

---

*Séance 1 (P0) : CLOSE le 02/09/2026 — bugs 40-42, 47-50 fermés et validés (TNR fichier mélangé : 13 nœuds, arbre identique à l'original ; saboteurs 'Erath' et 'radius mal typé' aux verdicts attendus). Le constat A1 (mort de `m_isRoot`, `IsRoot()` en deux surcharges libres, seconde passe des solitaires dans `WorldTransformSystem`) a été traité en avance de phase. Restent en P1 : API de mutation du graphe (A2), dt réel (44), PlayerInputSystem (45), caméras pilotées par la scène (46).*

*Prochaine étape initiale : Leçon 06, séance 1 — P0. Test de non-régression n°1 : `solar_system_shuffled.json`
(mêmes nœuds, ordre inversé enfants-avant-parents) doit produire une hiérarchie identique à
l'original. Critère de sortie de la leçon : `solar_system.json` tourne.*
