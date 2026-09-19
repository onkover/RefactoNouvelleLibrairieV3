# Annexe A9 — L'échelle du gizmo orthographique, et l'anatomie d'un faux diagnostic

> Prérequis : A5 (gizmos de caméra, `CameraBinding`, `Test_GizmoMatchesFrustum`), A7 (zoom, clamps), A8 (drapeau `dirty`), L05 (`Projection.h`, `ViewData`).
>
> Chantier ouvert sur un bug intitulé « le gizmo de `FPS_Camera` devient démesuré ». Ce titre était faux. C'est le sujet principal de cette annexe.

---

## 1. Le symptôme, et ce qu'on en avait conclu à tort

Dans le viewport de rendu (`FPS_Camera`, perspective), un objet démesuré apparaît dès que la scène contient `Overview_Camera` en orthographique avec `orthoHeight: 1200`. Il disparaît quand `Overview_Camera` repasse en perspective.

La conclusion posée en fin de session précédente : le gizmo de `FPS_Camera` subit une influence d'`Overview_Camera`. Comme rien dans `CameraGizmoSystem` ne permet ce couplage, la piste retenue devenait « état mutable partagé côté `ResourceManager`/`MeshClass` ».

**Cette conclusion était fausse de bout en bout.** Elle reposait sur une identification visuelle jamais vérifiée, et elle a orienté toute une session vers une chasse à une fuite de données inexistante.

---

## 2. La réfutation — deux lignes déjà écrites

Au spawn (A5 §4.3) :

```cpp
registry.addComponent(g, DebugVisualComponent{ Color{}, camEntity });
//                                                     ^^^^^^^^^ m_hideForCamera
```

Dans `RenderView` (A5 §4.6) :

```cpp
if (dbg && dbg->m_hideForCamera == view.m_sourceCamera) continue;
```

Le gizmo de `FPS_Camera` est **inconditionnellement rejeté** de la vue de `FPS_Camera`. Il ne peut pas y être démesuré : il n'y est jamais dessiné. Le seul gizmo que ce viewport peut afficher est celui d'`Overview_Camera`.

Le raisonnement est purement structurel, il n'a demandé aucune exécution. La mesure l'a ensuite confirmé :

```
__gizmo(FPS_Camera)      -> scale(1.590580, 1.242641, 3.000000)
__gizmo(Overview_Camera) -> scale(256.000000, 600.000000, 5.000000)
```

Le gizmo de la FPS est sain **pendant** que l'autre est à 600. Aucune fuite, aucun couplage, aucun état partagé.

> **Principe méthodologique n°13.** Avant de chercher la cause d'un comportement, vérifier l'identité de l'objet qui le manifeste. Une identification visuelle n'est pas une mesure. Tant que l'entité fautive n'est pas nommée par le programme lui-même, toute hypothèse sur la cause porte potentiellement sur le mauvais objet.

### 2.1 Relecture des constats

| Constat initial | Lecture correcte |
|---|---|
| « En perspective, le gizmo n'apparaît pas dans le viewport de rendu » | Ce n'est pas un bug, c'est A5 §4.6. Une caméra ne voit jamais son propre gizmo — sinon elle rendrait l'intérieur de son propre frustum, tranché par le near plane, plein écran. |
| « L'ortho impacte le gizmo de la FPS » | Confusion d'identité. L'objet est `__gizmo(Overview_Camera)`, une dalle de 512 × 1200 × 5 unités posée à l'emplacement d'`Overview_Camera`. |
| « 2 perspectives = aucun problème » | Prédit par la formule : deux gizmos de ~2,4 unités, invisibles à l'échelle du système solaire. Sans rapport avec « les deux font le même calcul ». |
| « Même souci avec d'autres scenegraphs » | Normal : la taille ne dépend que du couple (caméra, viewport). Le scenegraph n'entre pas dans `wanted`. |
| « Traits dans tous les sens » | Cette même dalle vue sous un angle rasant. Le diagnostic géométrique était bon, il était attribué à la mauvaise entité. |

### 2.2 Protocole d'identification, réutilisable

1. **Mesure directe** — journaliser nom + scale calculée, par entité, dans le système qui l'écrit.
2. **Suppression ciblée** — `gizmoLength: 0` sur une seule caméra (`SpawnCameraGizmos` filtre sur `m_gizmoLength > 0.0f`). L'objet disparaît ou non : l'identité est tranchée.
3. **Variation continue** — `orthoHeight` 1200 → 12. L'objet doit rétrécir d'un facteur 100, pas disparaître.

---

## 3. La vraie cause — section constante contre section proportionnelle

La formule d'origine, correcte au sens strict :

```cpp
// perspective
wanted = { L * tanHalf * aspect, L * tanHalf, L };
// orthographique
const float halfH = cam->m_orthoHeight * 0.5f;
wanted = { halfH * aspect, halfH, L };
```

Les deux branches décrivent fidèlement le frustum. Mais elles n'ont pas la même **nature d'échelle** — c'est la distinction de l'A5 §2.4, dont la conséquence n'avait jamais été tirée :

| | Demi-section | Dépend de `L` ? | Bornée ? |
|---|---|---|---|
| Perspective | `L · tan(fovY/2)` | **oui**, proportionnelle | par `L`, que tu choisis |
| Orthographique | `orthoHeight / 2` | **non**, constante | par `orthoHeight`, grandeur de scène |

Deux conséquences, toutes deux invisibles jusqu'ici :

**a) En ortho, `gizmoLength` n'a aucune prise sur la section.** Il n'agit que sur la profondeur. Le commentaire du composant mentait :

```cpp
float m_length = 3.0f;   // longueur d'affichage, PAS le farPlane   <-- faux en ortho
```

Vérifié à l'exécution : `gizmoLength: 1` sur `Overview_Camera` laisse le gizmo tout aussi gigantesque.

**b) La taille du gizmo ortho est dictée par la scène, pas par toi.** `orthoHeight` est plafonné à 5000 par le clamp de l'A7 : le gizmo peut donc atteindre 5000 unités d'arête, en toute légalité.

> **Règle R13.** Un gizmo de debug remplit deux fonctions contradictoires : *dire la vérité géométrique* (ceci est le volume vu par cette caméra) et *rester lisible* (il y a une caméra ici). La première a une taille imposée par la scène, la seconde une taille imposée par l'écran de l'observateur. Les séparer devient obligatoire dès qu'il existe deux observateurs.

> **Règle R15.** Un gizmo porte ce qui est *spatial* : position, orientation, forme, appartenance. Il ne porte pas ce qui est *scalaire*. Encoder une grandeur non bornée dans une géométrie produit un objet qui devient illisible précisément dans les cas où on aurait besoin de le lire.

**Contre-exemple : le code d'origine lui-même.** Il encodait `orthoHeight = 1200` dans une arête de 1200 unités. À l'instant où la valeur devenait intéressante — une caméra qui cadre très large —, le gizmo devenait une dalle masquant la scène. L'encodage géométrique d'un scalaire non borné s'auto-détruit à mesure que le scalaire grandit.

---

## 4. Les trois sémantiques possibles

Tant que celle-ci n'est pas tranchée, aucun code n'est « correct ».

| | Sémantique | Taille imposée par | Prix |
|---|---|---|---|
| **S1** | Le gizmo **est** le frustum, échelle 1 | la scène (`orthoHeight`) | illisible dès que la caméra cadre large |
| **S2** | Le gizmo est une **maquette** du frustum, échelle `s` | toi (`gizmoLength`) | perte de la lecture du volume absolu |
| **S3** | Le gizmo est une **icône** à taille constante à l'écran | l'écran de l'observateur | exige une donnée par couple (gizmo, observateur) — dette D2 |

Décision : **S2 maintenant, S3 en leçon complète.** S2 n'est pas une impasse — S3 est S2 avec un `s` recalculé par observateur.

### 4.1 Pourquoi le correctif écarté en session précédente l'était à raison

Une tentative antérieure bornait la section ortho par `L`, écrasant le ratio x/y. Le gizmo cessait alors de décrire la *forme* de son frustum, et `Test_GizmoMatchesFrustum` sautait — légitimement.

S2 est différent : le ratio est **exactement préservé** (`(L·aspect)/L = aspect`), seule l'échelle globale change, et elle change d'une quantité connue et calculable. Une maquette n'est pas un mensonge tant qu'on connaît son échelle.

---

## 5. L'implémentation

### 5.1 Source unique de la demi-section

`DebugGizmos.hpp` :

```cpp
// Demi-section AFFICHEE du gizmo, en unites locales.
// SOURCE UNIQUE : consommee par CameraGizmoSystem ET par Test_GizmoMatchesFrustum.
// Passer en S3 = modifier cette seule fonction.
[[nodiscard]] inline Vec2f GizmoHalfSection(const CameraComponent& cam, float L) noexcept
{
    if (cam.m_projection == EProjectionType::Orthographic)
        return { L, L };                       // maquette : L pilote la section
    const float tanHalf = std::tan(CameraFovY(cam) * 0.5f);
    return { L * tanHalf, L * tanHalf };       // echelle 1 exacte
}

// Demi-hauteur REELLE du frustum a la distance d — la grandeur par laquelle la
// matrice de projection divise. Ortho : constante. Perspective : proportionnelle.
[[nodiscard]] inline float FrustumHalfHeightAt(const CameraComponent& cam, float d) noexcept
{
    return (cam.m_projection == EProjectionType::Orthographic)
         ? cam.m_orthoHeight * 0.5f
         : d * std::tan(CameraFovY(cam) * 0.5f);
}
```

### 5.2 Le système

```cpp
const Vec2f hs = GizmoHalfSection(*cam, L);
wanted = { hs.x * aspect, hs.y, L };
```

Le commentaire du composant, rectifié :

```cpp
struct CameraGizmoComponent
{
    Entity m_owner  = NULL_ENTITY;
    // Taille d'affichage dans les DEUX modes :
    //   perspective : demi-section = L·tan(fovY/2)  — maquette a l'echelle 1
    //   ortho       : demi-section = L              — maquette a l'echelle L/(orthoHeight/2)
    // Ce n'est jamais le farPlane.
    float  m_length = 3.0f;
};
```

### 5.3 Le test

L'échelle attendue en NDC n'est pas une demi-section : c'est un **rapport**.

Dérivation, perspective. Le coin local `(1,1,-1)` après `m_local.scale` vaut en espace caméra `(hs.x·aspect, hs.y, -L)`. La projection divise par la demi-section réelle à `z` :

```
clip.x / clip.w = (hs.x·aspect) / (tanHalf·aspect · L) = hs.x / (L·tanHalf)
```

L'**aspect s'annule** — d'où un `s` unique valable pour `x` et pour `y`, et le maintien de `ndcX == ndcY` comme signature d'un aspect correct. En ortho, `clip.w = 1` et la division se fait par `orthoHeight/2` : même forme. Dans les deux cas :

```
s = demi-section AFFICHEE / demi-section REELLE du frustum a la distance L
```

```cpp
const Vec2f hs = GizmoHalfSection(cam, giz.m_length);
const float s  = hs.y / FrustumHalfHeightAt(cam, giz.m_length);
// NOTE : s est scalaire parce que hs.x == hs.y dans les deux modes actuels.
// Une demi-section non uniforme exigerait s.x et s.y separes.

LV3_ASSERT(std::fabs(clip.w - expectedW) < eps);
LV3_ASSERT(std::fabs(std::fabs(clip.x / clip.w) - s) < eps);
LV3_ASSERT(std::fabs(std::fabs(clip.y / clip.w) - s) < eps);
```

### 5.4 L'epsilon — correction indépendante de S1/S2

L'epsilon était indexé sur `camPos`, reconstruit depuis `trGiz.m_worldMatrix[3]`. Or la magnitude qui gouverne l'annulation catastrophique est celle du **coin évalué**, `world = camPos ± encombrement du gizmo`. Avec un encombrement de 600 unités, c'est l'encombrement qui domine — et il était ignoré.

```cpp
constexpr float kUlpMargin = 8.0f;
const float eps = kUlpMargin * std::numeric_limits<float>::epsilon()
                * std::max(1.0f, world.length());   // et non camPos.length()
```

Conservateur (il majore l'erreur réelle), mais sans perte de pouvoir de détection : les bugs visés sont d'ordre 1, pas 1e-6. Avec S2 appliqué, l'encombrement retombe à `L` et le problème s'évapore de lui-même — la correction devient une ceinture, pas un pansement.

---

## 6. Lire l'écart, pas seulement le rouge

Pendant le chantier, un assert a sauté avec `|clip.x / clip.w| = 1` contre `s = 0.005`, dans un état mixte (système revenu en S1, test resté en S2). L'écart valait `0,995`.

| Ordre de grandeur de l'écart | Nature | Traitement |
|---|---|---|
| `1e-7 … 1e-4`, résultat ≈ 1,0001 | précision `float32` | sujet d'epsilon |
| `0,1 … 10`, résultat = 1,33 ou 0,005 | désaccord sémantique | sujet de formule |

Aucun epsilon ne rattrape un écart d'ordre 1 — et c'est heureux : un epsilon qui l'absorberait absorberait aussi tous les vrais bugs. **L'invariance de l'écart** portait le reste de l'information : `|clip.x/clip.w| = 1` quel que soit `orthoHeight` signe une identité (en S1, le coin *est* le bord du NDC par construction), donc le terme variable était forcément l'autre.

Ce rouge a fourni gratuitement la validation qui manquait : il prouve que le test inspecte réellement l'échelle du gizmo ortho, avec l'écart exactement égal à `1 − s`.

---

## 7. La duplication, deux fois en une heure

Le chantier a produit deux occurrences du même défaut, sur deux sujets sans rapport.

**a) `s` écrit deux fois** — une copie dans `CameraGizmoSystem`, une dans le test. Rien n'obligeait les deux à rester d'accord ; elles ont divergé au premier retour arrière.

**b) `CheckControllerExclusivity` définie deux fois** — une dans `System.hpp` (LIB), une dans l'EXE de TNR. Découverte **par accident**, via un `#include` qui a rendu les deux visibles simultanément. Elle pouvait vivre indéfiniment. Et elle divergeait déjà : la version EXE appelait `getComponent<NameComponent>` sans garde, alors que `NameComponent` est optionnel — le diagnostic plantait donc avant d'avoir affiché ce qu'il avait trouvé.

> **Règle R16.** Une règle ou une formule partagée entre deux sites doit exister en un seul exemplaire, exposé comme fonction. Deux copies ne divergent pas *si* on modifie l'une sans l'autre : elles divergent *quand*. Une règle dupliquée entre la librairie et son harnais de test transforme le test en tautologie — il vérifie sa copie, pas le moteur.

Décision : `CheckControllerExclusivity` reste dans la LIB (l'invariant appartient à qui le porte : ce sont ses composants et ses systèmes qui l'enfreignent), déclarée en header, définie en `.cpp`, avec la garde sur `NameComponent` et sans codes ANSI au point d'appel — **la sévérité est portée par le niveau du log, jamais par la chaîne.** La copie EXE est supprimée, seul l'appel subsiste.

> **Règle R17.** Un test peut partager avec le système la *définition* d'une grandeur, jamais la *chaîne de traitement* qui la transporte. Partager la définition supprime un doublon ; partager la chaîne supprime le test.

C'est ce qui rend `GizmoHalfSection` légitime : le test ne rejoue pas le calcul du système. Il forme un **rapport entre deux grandeurs d'origines différentes** — numérateur venu du gizmo, dénominateur venu de la définition du frustum, celle qu'implémente `viewProjectionMatrix` par un tout autre chemin — puis vérifie que ce rapport survit à la chaîne complète `m_local.scale → LocalTransformSystem → WorldTransformSystem → viewProjection`. C'est cette chaîne, et elle seule, que le test prouve.

---

## 8. Validation

> **Rectifié (Discussion E, session du 18/09/2026).** Cette section décrivait encore `LV3_ASSERT(n == 2)` comme le garde-fou de vacuité du test. **C'était la documentation qui était périmée, pas le code** : le code réel (`main.cpp`) a depuis évolué vers un invariant strictement plus général, et c'est lui qui fait foi. `n == 2` n'aurait jamais pu survivre tel quel : il code en dur le nombre de caméras de la scène de test de ce chantier (`FPS_Camera` + `Overview_Camera`) et casse le jour où une troisième caméra apparaît dans une scène — sur `solar_system_v1compat_belt.json` par exemple.

| Palier | Attendu | Obtenu |
|---|---|---|
| `Test_GizmoMatchesFrustum` retourne | `checked > 0` s'il existe au moins un gizmo dans les vues rendues | conforme |
| `s`, `orthoHeight = 12`, `L = 3` | `0.5` | **0.5** |
| `s`, `orthoHeight = 1200`, `L = 3` | `0.005` | **0.00499999989** |
| `s`, `orthoHeight = 5000`, `L = 3` | `0.0012` | **0.00120000006** |
| `s`, perspective | `1` exactement | conforme |
| Asserts | aucun | aucun |

Le point décisif : `s` **varie** avec `orthoHeight` alors qu'il était figé à 1 en S1. Le test suit désormais l'échelle réelle du gizmo. La faiblesse introduite en remplaçant `LV3_ASSERT(checked > 0)` par un simple `Logger::info` (§5 plus haut, `TestAffichageGizmoCamera.cpp::Test_GizmoMatchesFrustum`) est compensée au point d'appel — mais par une **garde de vacuité généralisée**, pas par un compte figé à 2 :

```cpp
// main.cpp — après le rendu des vues de la frame
const size_t nGizChecked = Test_GizmoMatchesFrustum(registry, rm, views, nViews, GizAssets);
// Garde de vacuité DÉPLACÉ, pas supprimé : si les assets sont valides et
// qu'au moins une caméra a déclaré un gizmo, alors 0 vérification = câblage cassé.
if (GizAssets.IsValid())
{
    size_t declared = 0;
    for (auto&& [e, cam] : registry.ViewGroup<CameraComponent>())
        if (cam.m_gizmoLength > 0.0f) ++declared;
    LV3_ASSERT(declared == 0 || nGizChecked > 0);
}
```

`declared == 0 || nGizChecked > 0` dit exactement ce que `n == 2` essayait de dire sur cette seule scène de test, mais pour **n'importe quelle scène** : « si des caméras ont demandé un gizmo, le test en a réellement vérifié au moins un — sinon le câblage (spawn, filtre de vue, source des `ViewData`) est cassé en silence ». Un test vert qui n'a rien exécuté reste pire qu'un test absent (§ci-dessus, note sur `checked`) ; cette formulation généralise ce principe au lieu de le figer sur un effectif de caméras particulier.

À l'écran : la dalle a disparu du viewport de rendu. Le gizmo d'`Overview_Camera` mesure 2,56 × 6 × 3 unités au lieu de 512 × 1200 × 5.

---

## 9. Ce que S2 coûte, et où l'information part

**Perdu :** le gizmo ortho ne réagit plus visuellement au zoom (bénéfice annoncé de l'A7, conservé en perspective), et il ne dit plus le volume cadré.

**Conservé, et sans substitut ailleurs :** position, direction de visée, roulis (marqueur « up » asymétrique, A5 §3.3), nature de la projection (boîte contre pyramide), ratio d'aspect exact, état actif/inactif (par la teinte, A5 §4.5 — voir aussi §11.2 plus bas). Cinq informations spatiales sur six — et une information spatiale se lit toujours mieux dans une forme que dans un nombre.

**Prévu, PAS ENCORE FAIT :** le scalaire manquant (le sixième) est destiné à un futur overlay de debug par viewport — mais cet overlay **n'existe pas dans le code aujourd'hui**. Cette section décrivait jusqu'ici cette destination au passé (« Déplacé »), comme si le déplacement avait déjà eu lieu. **Rectifié (Discussion E, session du 18/09/2026)** : ce n'est qu'une cible de conception, formalisée ici pour ne pas la perdre, explicitement rattachée à un numéro de dette daté plutôt qu'à un vague `TODO`. Le carnet de route (Phase 3) le confirme : cet item **bloque la Leçon 07** — c'est là, et seulement là, qu'il sera implémenté.

Maquette visée pour cet overlay, à construire en L07 :

```
Overview_Camera | ortho | h=1200.0 | aspect 0.427
FPS_Camera      | persp | fov 45.0 | aspect 1.280
```

Gain triple attendu : lisible à tout zoom, exact au dixième, et valable pour les caméras **hors champ** — dont le gizmo, par définition, n'apprenait rien. Attention au bug A8 en l'écrivant, le jour venu : l'affichage de debug lit, il n'écrit jamais, et ne touche jamais `m_dirty`.

---

## 10. Journal des bugs

| # | Bug | Détecté par | Cause | Statut |
|---|---|---|---|---|
| **32** | Gizmo démesuré attribué à `FPS_Camera` | relecture du filtre `m_hideForCamera` — aucune exécution | identification visuelle jamais vérifiée ; une caméra ne peut pas afficher son propre gizmo | corrigé (diagnostic) |
| **33** | Section du gizmo ortho non bornée ; `gizmoLength` sans effet sur elle | `gizmoLength: 1` laisse le gizmo gigantesque | section constante contre proportionnelle (A5 §2.4), conséquence jamais tirée | corrigé (S2) |
| **34** | Epsilon du test indexé sur `camPos` au lieu de `world` | assert au dézoom, indépendant de la position | la magnitude de l'annulation est celle du coin, pas celle du nœud | corrigé |
| **35** | `CheckControllerExclusivity` dupliquée LIB/EXE, divergente | découverte accidentelle via `#include` | pas de source unique ; la copie EXE plantait sur `NameComponent` absent | corrigé |

### 10.1 Ce que le diagnostic a enseigné

Le bug 32 n'a coûté aucune exécution à réfuter : deux lignes de code déjà écrites suffisaient. Il avait pourtant consommé une session entière et produit une piste de recherche entièrement fictive (« état mutable partagé côté `ResourceManager` »).

> **La question « quel objet fait ça ? » précède toujours « pourquoi le fait-il ? ». Inverser les deux produit des hypothèses cohérentes, argumentées, et portant sur le mauvais objet.**

Le bug 35 illustre le point complémentaire : un défaut que ni test ni revue ne pouvaient révéler, parce qu'aucun des deux ne regarde *deux fichiers à la fois*. Seule une contrainte structurelle — une source unique — le rend impossible.

---

## 11. Dettes et suites

| | Sujet | État |
|---|---|---|
| **D2** | S3 : icône de taille constante à l'écran | ouverte — voir §11.1 |
| | Overlay de debug portant `orthoHeight` / `fov` / `aspect` par viewport | à faire, §9 — bloque L07 |
| | Politique de visibilité : ne dessiner le frustum que de la caméra active | proposée, **tranchée en Discussion E : NON retenue pour l'instant** — voir §11.2 |
| | Sonde de triangle quasi dégénéré dans `RasterizeTriangle` | à retirer (disculpée, et la géométrie extrême a disparu) |
| | `Overview_Camera` : `gizmoLength` resté à 5 (résidu d'un test d'élimination) | à remettre à 3 |
| | `ScaledEps` / `AssertNear` mutualisés pour `TestCameraMath` | non implémentée |

### 11.1 S3 — ce qu'il exigera

Le facteur d'échelle dépend de la distance entre le gizmo et l'observateur : donnée du **couple**, exactement comme l'aspect dans le bug 31.

> Une donnée qui dépend d'une paire ne peut pas être stockée sur l'un des deux membres.

Conséquence structurelle : une entité gizmo par couple (caméra décrite, caméra observatrice), chacune avec son `TransformComponent` et un champ `m_forView`. Avec N caméras, N·(N−1) entités — 2 dans la scène actuelle.

Le faux cycle se résout comme pour `CameraBinding` : la distance exige les matrices monde, produites par `WorldTransformSystem`. Mais le gizmo est une **feuille** de la hiérarchie, sans enfant à propager — une seconde passe placée après peut recomposer sa seule matrice monde sans frame de retard.

```
LocalTransformSystem -> WorldTransformSystem -> GizmoScreenScaleSystem -> BuildViewData
```

> **Règle R14 (proposée).** Une transformation dépendante de la vue appartient à une feuille de la hiérarchie et se recompose après la passe de propagation, jamais avant. La calculer avant introduit une frame de retard ou un cycle.

Grâce à R16, le passage à S3 se fait en modifiant `GizmoHalfSection` — le système suit, `s` suit, le test reste vert sans qu'une ligne y soit touchée.

### 11.2 La politique de visibilité — répond à une autre question

Aujourd'hui, tous les gizmos sont dessinés dans tous les viewports sauf le leur. À 5 caméras : 4 gizmos superposés par viewport. L'option consiste à ne dessiner le frustum que de la caméra active, en réutilisant la condition qui pilote déjà la teinte :

```cpp
struct DebugVisualComponent
{
    Color  m_color;
    Entity m_hideForCamera = NULL_ENTITY;
    bool   m_visible       = true;      // ajout
};
```

```cpp
const bool isActive = (giz.m_owner == activeCamera);
dbg.m_color   = isActive ? Color{255,216,26} : Color{110,112,128};
dbg.m_visible = isActive;
```

```cpp
if (dbg && (!dbg->m_visible || dbg->m_hideForCamera == view.m_sourceCamera)) continue;
```

C'est le comportement de Unity — le frustum n'apparaît que sur la caméra sélectionnée. **Mais ce n'est pas un correctif de taille** : rendre `Overview_Camera` active ramènerait sa dalle telle quelle en S1. À traiter séparément.

> **Rectifié (Discussion E, bug 56, session du 18/09/2026).** Cette proposition avait fini à moitié câblée dans le code : `dbg.m_visible = isActive` était bien écrit par `CameraGizmoSystem`, mais le filtre `RenderView` censé le lire était en pratique désactivé — la couleur grise (caméra inactive) calculée juste à côté n'était donc plus jamais observable si ce filtre avait été réactivé sans qu'on y prenne garde, en contradiction frontale avec A5 §4.5. Discussion E a tranché : **on ne retient pas cette politique maintenant.** L'écriture morte de `m_visible` a été retirée de `CameraGizmoSystem`, tous les gizmos redeviennent visibles avec la seule teinte pour les distinguer (comportement A5, validé et testé). Le champ `DebugVisualComponent::m_visible` reste déclaré (défaut `true`, donc inerte tant que rien ne l'écrit) : si cette politique est un jour retranchée pour de bon, l'infrastructure ci-dessus est prête à être reconnectée en une fois, proprement, plutôt que redécouverte bug par bug.
