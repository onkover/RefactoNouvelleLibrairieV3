# Annexe A11 — Découplage viewport/caméra et lisibilité debug (Discussion J)

> Prérequis : L05 (`ViewData`, `BuildCameraBindings`), Annexe A5 §4 (`RenderView`, `mode` comme état de vue) et §13 (R12, R20 — `depthDisplayRange`), Carnet de route — bug 61 (Discussion E) et bug 66.

Cette annexe couvre l'évolution du bug 61 au-delà de sa correction initiale (Discussion E) : le nombre de viewports affichés n'est plus lié au nombre de caméras actives dans la scène, et bascule automatiquement dessus quand elles sont plus nombreuses que les emplacements disponibles. Elle couvre aussi le bug 66, découvert en testant cette extension, qui referme une dette explicitement annoncée depuis l'Annexe A5 (règle R12).

---

## 1. Le problème initial

Le bug 61 (Discussion E) avait posé `kMaxCamerasHard` / `EngineConfig` comme plafond configurable, mais la boucle de jeu ne pilotait encore que deux panneaux fixes (`Panel panels[2]`, un `Gameplay`, un `Debug`) — le plafond ne changeait rien d'observable. Question posée par Onky en reprenant le sujet :

> « Pourquoi limiter le nombre de caméras via une config dans `engine.json` alors qu'on pourrait en ajouter autant qu'on veut dans la scène, le nombre pris en compte serait capé par la restriction `kMaxCamerasHard` ? »

Ce qui manquait n'était donc pas un plafond, mais une **politique de correspondance** entre « combien de caméras existent dans la scène » et « combien de viewports afficher ».

---

## 2. Les règles retenues (design audité, Onky)

Design proposé par Onky, audité (4 questions de clarification, toutes tranchées en faveur de l'option recommandée), puis implémenté :

- La scène peut contenir 1 à N caméras `Gameplay` et 1 à N caméras `Debug` (catégories déjà existantes, `ECameraCategory`, inchangées).
- `viewport.maxViewport` (`engine.json`) fixe le nombre de viewports **désirés** — remplace `camera.maxCameras`, qui n'a plus de sens dès lors que le nombre de viewports et le nombre de caméras sont deux grandeurs différentes.
- **Toujours exactement un slot réservé au rendu/gaming** (slot 0), quel que soit `maxViewport` — jamais plus, jamais zéro tant qu'une caméra `Gameplay` active existe. S'il y a plusieurs caméras `Gameplay` actives, ce slot les cycle (touches existantes).
- Les slots restants (`maxViewport - 1`) sont réservés au debug, et cyclent indépendamment sur le pool `Debug` s'il y a plus de caméras debug actives que de slots.
- Recalcul **chaque frame**, en direct — pas une décision figée au chargement de la scène (les caméras peuvent devenir actives/inactives en cours de partie).
- Dégradation : si le total de viewports calculé n'est pas directement affichable (voir §3), on réduit et on avertit une fois (`Logger::warn`, edge-triggered sur changement de total, jamais à chaque frame).
- Arrêt fatal — pas une dégradation — si aucune caméra `Gameplay` active n'existe : pas de fenêtre de rendu sans caméra de jeu.
- Le style de rendu par viewport (`ERenderMode`) reste piloté au clavier, par slot — une vraie sélection par UI est explicitement différée (pas de multiplication de touches avant qu'une UI existe).

---

## 3. La contrainte qui gouverne tout : `BuildLayout` ne connaît que {1, 2, 4}

`BuildLayout` (`CameraBinding.CPP`) ne sait produire que `ELayout::Single` (1), `SplitH`/`SplitV`/`MainSide` (2) ou `Quad` (4) — **aucun layout à 3 vues n'existe**. C'est une contrainte structurelle, pas un oubli à corriger en urgence (ajouter un `ELayout::Triple` a été explicitement reporté par Onky, deux fois).

Conséquence directe sur la politique de comptage : le total de viewports voulu (`1 + nDebugSlots`) doit être **arrondi à {1, 2, 4}** avant d'être transmis à `BuildLayout`. La règle retenue arrondit **vers le bas**, jamais vers le haut :

```cpp
// Arrondit vers le bas sur {1, 2, 4} — jamais vers le haut :
// afficher un 4e slot qu'aucune caméra n'a demandé serait pire
// qu'afficher un slot de moins.
static size_t SnapToSupportedViewportCount(size_t requested)
{
    if (requested >= 4) return 4;
    if (requested >= 2) return 2;
    return 1;
}
```

Contre-exemple explicitement écarté : arrondir 3 vers 4 en réutilisant un `Debug` déjà affiché dans un autre slot. Ça violerait l'invariant « aucune caméra deux fois » déjà gardé par `BuildCameraBindings` en `_DEBUG` (bug 53-adjacent) — mieux vaut un slot de moins qu'un doublon silencieux.

Exemples de la table d'Onky, tous vérifiés :

| Caméras Gameplay actives | Caméras Debug actives | `maxViewport` | Total demandé | Arrondi | Résultat |
|---|---|---|---|---|---|
| 4 | 4 | 2 | 1 + 3 = 4, capé à 1 | 2 | 2 viewports, switch sur les deux |
| 4 | 10 | 4 | 1 + 3 = 4 | 4 | 4 viewports, switch gaming ET debug |
| 2 | 1 | 4 | 1 + 1 = 2 | 2 | 2 viewports, mode normal (pas dégradé) |
| 2 | 2 | 4 | 1 + 2 = 3 | **2** | dégradé, avertissement |
| 2 | 3 | 4 | 1 + 3 = 4 | 4 | normal |
| 1 | 0 | n'importe | 1 + 0 = 1 | 1 | 1 viewport, comportement par défaut |
| 0 | n'importe | n'importe | — | — | **arrêt fatal** |

---

## 4. Structures et code

### 4.1 `Panel` — généralisé de 2 à `kMaxCamerasHard` slots

```cpp
struct Panel
{
    Entity      camera = NULL_ENTITY;   // sélection COURANTE — état de session
    ERenderMode mode   = ERenderMode::Solid;
};

Panel panels[kMaxCamerasHard]{};
static_assert(std::size(panels) == kMaxCamerasHard);   // garde contre une re-transcription à la main
```

`ECameraCategory` a disparu du `Panel` : slot 0 est *par convention de position* le slot gaming, slots 1..N sont debug — la catégorie n'a plus besoin d'être stockée par slot, elle se déduit de l'index. Bug rencontré en généralisant cette structure à la main : `g_cycleCam[]`/`g_cycleMode[]` étaient restés dimensionnés `[2]` — exception d'instrumentation `RangeChecks` dès qu'un slot ≥ 2 était touché. D'où le `static_assert` ci-dessus, ajouté après coup précisément pour qu'une future re-transcription manuelle ne puisse plus reproduire ce bug silencieusement.

### 4.2 Anti-collision entre slots debug

Plusieurs slots debug puisent dans le **même** pool (`ECameraCategory::Debug`). Sans garde, deux slots pourraient cycler indépendamment jusqu'à afficher la même caméra deux fois :

```cpp
static bool IsCameraUsedByOtherDebugSlot(const Panel* panels, size_t nDebugSlots,
                                          size_t exceptIndex, Entity cam)
{
    for (size_t p = 1; p <= nDebugSlots; ++p)
        if (p != exceptIndex && panels[p].camera == cam)
            return true;
    return false;
}
```

`NextCamera` (`System.cpp`, inchangée) continue de filtrer par catégorie et de trier par priorité décroissante ; cette garde est appliquée par-dessus, côté appelant, avant d'accepter le résultat du cycle sur un slot debug. `BuildCameraBindings` garde en plus son propre `LV3_ASSERT` (`_DEBUG` seulement) vérifiant qu'aucune caméra n'apparaît deux fois dans les bindings finaux — double filet, pas redondance inutile : l'un est une politique de choix (silencieuse, dégradation), l'autre une invariance de sortie (bruyante, développement).

### 4.3 Touches

| Slot | Cycle caméra | Cycle mode de rendu |
|---|---|---|
| 0 (gaming) | F2 | F3 |
| 1 (debug) | F4 | F5 |
| 2 (debug) | F6 | F7 |
| 3 (debug) | F8 | F9 |

Le cycle de mode par slot existe donc déjà au clavier — la « sélection par UI » différée par Onky concerne une interface dédiée, pas l'absence totale de contrôle par viewport.

---

## 5. Bug 66 — `depthDisplayRange` partagé entre toutes les vues (R12)

### 5.1 Symptôme

Une fois plusieurs viewports simultanés opérationnels, le mode `ERenderMode::Depth` restait lisible sur `Overview_Camera` mais donnait un écran saturé (uniformément noir ou blanc) sur une caméra debug ajoutée (`Right_Camera`), même avec `near`/`far`/`infiniteFar` correctement paramétrés pour cette caméra.

### 5.2 Diagnostic

`ShadeFragment_Depth` (`Fragment.cpp`) ne dépend d'aucune identité de caméra — le calcul (`dist = 1/invW`, `t = Saturate(dist / depthDisplayRange)`, gamma) est le même pour toutes les vues. Le défaut était en amont : `renderer.SetDepthDisplayRange(EngineConfig::Get().debug.depthDisplayRange)` était appelé **une seule fois**, avant la boucle `for (i < nViews) RenderView(...)` de `main.cpp` — une valeur unique (80.0, calibrée pour la distance de vue de `Overview_Camera`) s'appliquait donc à **toutes** les vues de la frame, quel que soit leur propre profil de distance à la scène.

Ce défaut était déjà anticipé, noté et daté dans `Annexe_A5_Gizmos_Camera.md` §13.3 au moment même où `depthDisplayRange` avait été introduit :

> « `SetDepthDisplayRange` est appelée une fois, hors de la boucle de vues, sur un `Renderer` stateful — donc la valeur s'applique à toutes les vues. **Sans conséquence aujourd'hui** (une seule vue peut être en mode `Depth`), mais le paramètre est de la même famille que `mode` et **devra rejoindre `CameraBinding` le jour où deux vues en auront besoin de valeurs différentes.** » — R12

Le découplage viewport/caméra de cette même discussion est précisément ce qui a fait arriver ce jour.

### 5.3 Contre-exemple (déjà proscrit par R20, rappelé ici parce qu'il est le réflexe naturel)

```cpp
// FAUX — c'est le contresens que R20 interdit déjà (Annexe A5 §13.3) :
const float range = cam.m_infiniteFar ? kDefaultRange : cam.m_farPlane;
```

`depthDisplayRange` n'est pas une grandeur géométrique dérivable de `farPlane` — c'est un paramètre de **lisibilité**, choisi pour que le dégradé reste visible à l'œil, pas pour décrire le volume de vue. Partager la même unité (unités monde) ne fait pas des deux la même grandeur.

### 5.4 Correctif — le bon trajet, pas une simple relocalisation

Le principe qui tranche où placer une donnée : `mode` (le rendu choisi *par l'utilisateur*, au clavier) est une donnée de **session**, et traverse légitimement `Panel → CameraBinding → ViewData`. `depthDisplayRange` n'est **pas** un choix de session — c'est une propriété **d'auteur**, exactement comme `near`/`far`/`fov`, qui appartiennent déjà à `CameraComponent` et traversent directement `CameraComponent → ViewData` via `BuildViewData`, sans jamais passer par `Panel`/`CameraBinding`. La suivre dans le mauvais trajet (celui de `mode`) aurait fonctionné mais aurait été un mélange de responsabilités — R12 parlait de « rejoindre `CameraBinding` » en 2026-09 sans encore avoir cette distinction sous la main ; elle est maintenant explicite (voir `architecture-rules.md`).

**1. `LibraryV3/Scene/Components/Component.hpp`**, dans `CameraComponent`, après `m_infiniteFar` :

```cpp
// --- Debug : affichage (PAS une donnée géométrique, cf. R20) ---
float m_depthDisplayRange = -1.0f;   // -1 = non défini -> EngineConfig::debug.depthDisplayRange
```

**2. `LibraryV3/Scene/Serializer.cpp`**, après la lecture de `infiniteFar` :

```cpp
c.m_depthDisplayRange = r.Read("depthDisplayRange", -1.0f);
```

**3. `LibraryV3/Rendering/ViewData.h`**, à côté de `mode` :

```cpp
// --- Debug : affichage ---
float depthDisplayRange = 80.0f;   // plage de lisibilité pour ERenderMode::Depth — PAS une donnée géométrique (R20)
```

**4. `LibraryV3/Scene/System.cpp::BuildViewData`**, après `v.mode = b.m_mode;` :

```cpp
v.depthDisplayRange = (cam.m_depthDisplayRange > 0.0f)
    ? cam.m_depthDisplayRange
    : LV3::EngineConfig::Get().debug.depthDisplayRange;
```

**5. `LibraryV3/Scene/RenderSystem.cpp::RenderView`**, après `renderer.SetMode(view.mode);` :

```cpp
renderer.SetDepthDisplayRange(view.depthDisplayRange);
```

**6. `RefactoNouvelleLibrairieV3/main.cpp`** : l'appel global avant la boucle (`renderer.SetDepthDisplayRange(LV3::EngineConfig::Get().debug.depthDisplayRange);`, juste après `BeginFrame`) est **supprimé** — il serait de toute façon écrasé par la première vue de la boucle, même famille de piège que le bug 30 (état écrit avant l'étape qui l'efface).

`engine.json` garde `"debug": { "depthDisplayRange": 80.0 }` : c'est désormais le **défaut global**, utilisé par toute caméra qui ne précise pas la clé optionnelle `"depthDisplayRange"` dans son propre bloc `"Camera"`.

### 5.5 Validé

Onky a paramétré `depthDisplayRange` sur chaque caméra qui en avait besoin, en gardant `engine.json` comme défaut pour les autres : « Tout fonctionne » — la boucle de rendu (une valeur par vue, résolue par caméra) est confirmée saine.

---

## 6. Convention de rotation caméra — dérivée, pas devinée

Nécessaire pour calibrer correctement les nouvelles caméras debug cardinales (Top/Bottom/Left/Right) ajoutées pour éprouver la bascule anti-collision (§4.2) avec un pool de 5 caméras debug pour 3 slots.

### 6.1 Contre-exemple

Une première tentative de caméra `TopDown_Camera` avait été calibrée par analogie visuelle plutôt que par dérivation — elle ne rendait rien de visible. Le symptôme a d'abord été pris pour un bug de bascule de caméra (le switch semblait « inutile »), alors que le switch fonctionnait parfaitement : c'est la caméra elle-même qui ne regardait nulle part d'utile. **Une rotation ne se devine jamais par analogie avec une caméra sœur — elle se dérive de `Forward()`.**

### 6.2 La convention, confirmée en lisant le code

- `Vectorlib.h` : `Forward() = (0,0,-1)`, `Right() = (1,0,0)`, `Up() = (0,1,0)` — main droite, Y-up (`static_assert` déjà en place dans le moteur).
- `Transform.h` : `Transform::Forward()/Right()/Up()` = `rotation.rotate(Vec3f::Forward/Right/Up())`.
- `QuaternionLib.h` : `Quat(Angles, eulerDeg=true)` — `Angles.x = Pitch (X)`, `Angles.y = Yaw (Y)`, `Angles.z = Roll (Z)`, composé `q_yaw * q_pitch * q_roll` (ordre Y-X-Z, confirmé en comparant la formule du constructeur à un produit de quaternions manuel).
- `Serializer.cpp` : `"rotation"` JSON = degrés, passé tel quel à `Quatf(eulerDeg, true)` — aucun remappage d'axe supplémentaire.

### 6.3 Dérivation

Pitch pur θ (autour de X) appliqué à `Forward()` :

```
Rotate_X(Forward, θ) = (0, -sinθ, -cosθ)
```

- **Top** (au-dessus, doit regarder vers -Y) : `(y,z) = (-1,0)` → `pitch = -90°`
- **Bottom** (en dessous, doit regarder vers +Y) : `(y,z) = (1,0)` → `pitch = +90°`

Yaw pur θ (autour de Y) appliqué à `Forward()` :

```
Rotate_Y(Forward, θ) = (-sinθ, 0, -cosθ)
```

- **Right** (à +X, doit regarder vers -X) : `(x,z) = (-1,0)` → `yaw = +90°`
- **Left** (à -X, doit regarder vers +X) : `(x,z) = (1,0)` → `yaw = -90°`

Dans les deux familles de cas, `Up()` reste non dégénéré (vérifié) : pas de gimbal lock à ces angles précis. Ces formules confirment que `Top_View` (`rotation:[-90,0,0]`) et `Side_View` (`rotation:[0,90,0]`), déjà présents dans `solar_system.json`, étaient corrects — de bonnes références pour toute future caméra cardinale.

### 6.4 Caméras livrées

Distance 120 (cohérente avec `Overview_Camera`, référence prouvée). Priorités distinctes (4, 5, 6, 7) pour ne jamais entrer en tie avec `Overview_Camera` (priority 3) dans le tri de `CollectActiveCameras`.

```json
{ "id": "Top_Camera",    "Transform": { "translation": [0, 120, 0],  "rotation": [-90, 0, 0] }, "priority": 7 },
{ "id": "Right_Camera",  "Transform": { "translation": [120, 0, 0],  "rotation": [0, 90, 0] },  "priority": 6 },
{ "id": "Bottom_Camera", "Transform": { "translation": [0, -120, 0], "rotation": [90, 0, 0] },  "priority": 5 },
{ "id": "Left_Camera",   "Transform": { "translation": [-120, 0, 0], "rotation": [0, -90, 0] }, "priority": 4 }
```

(`category: "debug"`, `projection: "perspective"`, `active: true`, `gizmo.length: 3.0` sur chacune ; `Overview_Camera`, priority 3, sert de 5ᵉ caméra debug.) Les anciennes `TopDown_Camera`/`Bottom_View`/`Side_Camera` mal calibrées ont été remplacées plutôt que cumulées, pour ne pas mélanger caméras correctement dérivées et caméras devinées dans le même pool de switch.

---

## 7. Journal des bugs (extrait — voir Carnet de route pour l'entrée canonique)

| # | Bug | Où | Statut |
|---|---|---|---|
| **61 (évolution)** | Nombre de viewports lié au nombre de caméras plutôt que découplé avec bascule | `main.cpp`, `EngineConfig`, `CameraBinding.CPP` | corrigé — §2 à §4 |
| **66** | `SetDepthDisplayRange` appelé une fois, hors boucle de vues (R12) | `main.cpp`, `System.cpp::BuildViewData`, `RenderSystem.cpp::RenderView` | corrigé — §5 |

---

## 8. Ce qui reste ouvert

Voir Carnet de route, section « Prochaines actions » — priorité suggérée : log de diagnostic temporaire à statuer, `GizmoAssets::IsValid()` (`&&`/`\|\|`) à trancher, `config.json` → scène réellement chargée à vérifier, `ELayout::Triple` toujours absent, séparateur d'écran codé en dur pour l'ancien layout à corriger, bug 60 toujours sans discussion assignée.
