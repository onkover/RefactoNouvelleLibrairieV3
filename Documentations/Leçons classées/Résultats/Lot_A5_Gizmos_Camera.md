# Lot A5 — Les gizmos de caméra : décisions prises

*Fichier de résultats. Chantier mené pendant la Leçon 06, rattaché comme **Annexe A5** à la Leçon 05 (Caméra & Frustum). Artefact interactif : « Lot A5 — Les gizmos de caméra, pas à pas ». Document détaillé : `Annexe_A5_Gizmos_Camera.md`. Points ouverts A5-1 à A5-9 : `Plan_Phase_G_LibraryV3.md`.*

---

## Statut

**Chantier CLOS.** Les deux objectifs fonctionnels sont atteints et vérifiés numériquement :

1. Toutes les caméras de la scène sont affichées comme des volumes filaires dans la vue d'observation.
2. La caméra active se distingue des inactives (teinte ambre contre gris froid), et aucune caméra ne rend son propre gizmo.

Validé par quatre tests, dont `Test_GizmoMatchesFrustum` qui prouve que le gizmo **est** le frustum de sa caméra (coins du mesh projetés sur `ndcX = ndcY = ±1`, à 1e-4 près, sur les trois caméras).

---

## 1. Décisions actées

| # | Décision | Alternative écartée | Raison du choix |
|---|----------|---------------------|-----------------|
| **A5-D1** | Le gizmo est **dérivé** des paramètres de la caméra, jamais modélisé | Un `camera_body.obj` décoratif accroché en `MeshComponent` | Un asset figé ne connaît ni le `fov`, ni l'aspect, ni la projection : il ment dès qu'un paramètre change, et le mensonge est silencieux |
| **A5-D2** | **Un mesh canonique par type de projection**, partagé par toutes les caméras ; la forme sort de la matrice de scale | Un mesh généré par caméra, régénéré au changement de fov | Propriété de la perspective : tronquer une pyramide à n'importe quelle distance donne une figure semblable. Un seul `MeshHandle` en mémoire, quel que soit le nombre de caméras |
| **A5-D3** | Le gizmo est une **entité enfant** de la caméra ; le scale ne touche jamais le nœud caméra | Un scale posé sur le `TransformComponent` de la caméra | `View = world.inverseRigid()` suppose R orthonormale : un scale casserait l'inverse **sans assertion ni NaN** |
| **A5-D4** | `DebugVisualComponent` **générique** (couleur + `m_hideForCamera`) : la couche rendu ignore le mot « caméra » | Un `if (Has<CameraGizmoComponent>)` dans `RenderView` | Réutilisable tel quel pour les AABB du BVH (L11), les normales, les axes. Pas de dépendance rendu → scène |
| **A5-D5** | `CameraBinding` + `BuildLayout` / `BuildCameraBindings` : point de décision unique de la configuration d'affichage | Un `float aspect` global passé au système | L'aspect appartient au couple (caméra, viewport). Un nombre unique n'était correct que par hasard, les deux viewports ayant la même taille |
| **A5-D6** | Ordonnancement **sans retard de frame** : bindings (statiques) avant les transforms, `ViewData` (dérivées) après | Consommer les `ViewData` dans le système, au prix d'une frame | Judder sur la Follow amortie, culling sur frustum périmé, erreur cumulée en hiérarchie profonde, flash au démarrage |
| **A5-D7** | Échec de chargement des assets : **booléen `IsValid()` + journal**, pas `std::expected` | Propager la cause de l'échec | Toutes les causes mènent à la même action (pas de gizmos). Propager une erreur que personne ne traite différemment est du bruit dans les signatures |
| **A5-D8** | `JsonReader::Child()` : consomme la clé conteneur sur le parent, ouvre un lecteur sur le sous-objet | `r.Read(compJson.value("length", 3.0f))` | La seconde forme passe un `float` au lecteur, qui ne voit jamais le nom de la clé : le TNR est désarmé et `WarnUnread` produit un faux positif |
| **A5-D9** | `active` par défaut à **`false`** : l'activité se déclare | Défaut `true` | Une caméra qui ne le dit pas ne doit pas se déclarer candidate à la vue active |
| **A5-D10** | `ViewData.farPlane` : **`hasFarPlane` explicite**, pas de sentinelle | `1e30f`, puis `std::numeric_limits<float>::infinity()` | `1e30f` est validée par `isfinite()` mais déborde au premier carré ; `infinity()` produit `NaN` sur `inf − inf` et `0 × inf`, et une normalisation par `inf` rend **0 partout sans symptôme** |
| **A5-D11** | `faceCount()` dérive de `indices.size() / vertsPerFace` | `faceNormals.size()` | Un attribut optionnel absent rendait `faceCount() == 0` sur un mesh valide, rejeté silencieusement par `RenderView` |
| **A5-D12** | **Pas de volet SIMD** sur ce lot | Vectoriser par principe | 3 à 5 entités par frame : la transposition SoA coûterait plus que le calcul économisé. Règle du moteur — on ne vectorise que ce que le profileur désigne |
| **A5-D13** | Marqueur « up » ajouté aux deux OBJ (2 triangles, double face) | S'en passer | Les deux meshes sont symétriques en Y : un roll de 180° produit un gizmo **pixel pour pixel identique**. Le test T4 ne le voit pas non plus |
| **A5-D14** | `ERenderMode` porté par `CameraBinding`, appliqué par `RenderView` | Un paramètre de `RenderView` | Le mode est un attribut de la vue, même origine et même véhicule que le viewport. Un paramètre séparé recréerait un chemin parallèle |

---

## 2. Ce qui a été livré

### Assets

| Fichier | Contenu | Vérification |
|---|---|---|
| `assets/meshes/camera_gizmo.obj` | Pyramide canonique, apex à l'origine, quad far en `z = −1` | 8 sommets, 6 normales, **8 faces**, `vertsPerFace = 3` |
| `assets/meshes/camera_gizmo_box.obj` | Boîte canonique, face avant `z = 0`, arrière `z = −1` | 11 sommets, 6 normales, **14 faces**, `vertsPerFace = 3` |

Convention commune : main droite, Y up, regard vers `−Z`, winding CCW vu de l'extérieur, triangles uniquement (`vertsPerFace` est unique pour tout le mesh).

### Code — côté LIB

| Élément | Nature | Emplacement |
|---|---|---|
| `CameraGizmoComponent` | nouveau composant | `Scene/Components/Component.hpp` |
| `DebugVisualComponent` | nouveau composant | `Scene/Components/Component.hpp` |
| `CameraBinding`, `ViewSlot`, `ELayout` | nouvelles structures | `Scene/CameraBinding.hpp` |
| `BuildLayout`, `BuildCameraBindings` | nouvelles fonctions | `Scene/CameraBinding.cpp` |
| `GizmoAssets`, `LoadGizmoAssets`, `SpawnCameraGizmos`, `CameraGizmoSystem` | nouveau fichier dédié | `Scene/DebugGizmos.{hpp,cpp}` |
| `LinkChildToParent` | extraite de `SceneSerializer` en API publique | `Scene/Hierarchy.hpp` |
| `CameraFovY()` | source unique de l'angle vertical | header de `CameraComponent` |
| `JsonReader::Child()` | descente dans un sous-objet, clé conteneur consommée | `Core/JsonReader.h` |
| `ViewData` | `sourceCamera`, `mode`, `hasFarPlane` ajoutés ; préfixe `m_` retiré | `Render/ViewData.h` |
| `BuildViewData` | signature `(const Registry&, const CameraBinding&)` | `Render/ViewData.cpp` |
| `RenderView` | `SetMode`, filtre de vue, teinte | `Render/RenderView.cpp` |
| `ParseCamera` | refondu : discriminant unique, garde-fous parlants, branches complètes | `Scene/SceneSerializer.cpp` |
| `faceCount()` | dérive de `indices.size() / vertsPerFace` | `Render/MeshClass.h` |
| `Viewport::Resize` | **supprimée** (orpheline) | `Render/Viewport.h` |

### Tests

| Test | Fenêtre | Ce qu'il prouve |
|---|---|---|
| `Test_GizmoAssetLoads` | A (chargement) | Les deux meshes sont présents et ont la bonne topologie |
| `Test_GizmoCountMatchesCameras` | A (chargement) | Autant de gizmos que de caméras qui en demandent ; aucun orphelin |
| `Test_CameraWorldMatrixIsRigid` | B (chaque frame) | Aucun scale ne s'est glissé sur une matrice monde de caméra |
| `Test_GizmoMatchesFrustum` | B (chaque frame) | Le gizmo **est** le frustum de sa caméra (6 bugs couverts) |

---

## 3. Journal des bugs

| # | Bug | Détecté par | Cause | Correction |
|---|-----|-------------|-------|------------|
| **28** | `tan(fov)` au lieu de `tan(fov/2)` | `ndcY = 2,414213 = 1/tan(22,5°) = 1+√2` | demi-angle omis | `* 0.5f` dans `CameraGizmoSystem` |
| **29** | Aspect ratio jamais appliqué au gizmo | `ndcX/ndcY = 0,8 = 1/1,25` | pas de source unique pour l'aspect | `* aspect` sur la composante X |
| **30** | Scale du gizmo absent de `m_worldMatrix` | `clipW = 1` au lieu de 3 | écriture de la source **après** sa cuisson par `LocalTransformSystem` | `CameraGizmoSystem` repassé avant `LocalTransformSystem` |
| **31** | Aspect unique pour N viewports | layout `MainSide` (1152×800 et 384×800) | l'aspect appartient au couple (caméra, viewport) | `CameraBinding` |

### La méthode de diagnostic — à réutiliser

Deux identités **exactes** séparent les causes. Elles sont désormais intégrées à l'outil de diagnostic de l'artefact :

```
ndcY        =  tanHalf_gizmo / tanHalf_proj      <- ne depend QUE du fov
ndcX / ndcY =  aspect_gizmo  / aspect_proj       <- ne depend QUE de l'aspect
clip.w      =  L en perspective, 1 en orthographique
```

| Observation | Conclusion |
|---|---|
| `ndcY = 1`, `ndcX ≠ 1` | fov correct ; seul l'aspect est faux, et `ndcX` en donne le rapport exact |
| `ndcY ≠ 1`, `ndcX/ndcY = 1` | aspect correct ; seul le fov est faux |
| `ndcY ≠ 1`, `ndcX/ndcY ≠ 1` | les deux |
| `ndcX = ndcY = 0` | scale nul |
| `clip.w = 1` en perspective | le scale n'atteint pas la matrice monde — matrice à l'identité |

Le bug 28 s'est identifié par la constante `2,414213 = 1 + √2 = 1/tan(22,5°)`, qui confirmait à la fois l'omission du demi-angle **et** que les caméras étaient à `fov = 45°`.

### Deux incidents de déploiement

Deux fois dans la même session, le `.obj` du répertoire d'exécution n'était pas celui édité : d'abord une boîte qui s'affichait en pyramide, puis un `faceCount()` obsolète. Remède structurel noté en roadmap (A5-5).

---

## 4. Pistes écartées — ne pas les re-explorer

- **Un rasterizer de lignes dédié au frustum.** Inutile : la vue d'observation tourne déjà en `ERenderMode::Wireframe` depuis la L05. Le frustum s'affiche en fil de fer gratuitement.
- **Générer la géométrie du gizmo par caméra.** Écarté par A5-D2 : un seul mesh canonique suffit, la forme sortant de la matrice.
- **Faire porter `length` par le far plane.** Un `far = 1000` produirait un gizmo qui écrase la scène ; et les caméras sont en `infiniteFar`, donc le far n'existe pas.
- **Dériver `depthDisplayRange` de `farPlane`.** Contresens : une valeur d'affichage debug est choisie pour la lisibilité (bandes du `fmod`), pas pour la justesse du volume. Avec `far = 2000`, une seule bande sur toute la scène. Voir règle R20 et point A5-1.
- **Supporter l'orthographique en déformant la pyramide.** La section ortho est constante, celle du frustum croît avec `z` : deux natures, pas deux variantes. Un second mesh était obligatoire.
- **`std::expected` pour l'échec de chargement des gizmos.** Voir A5-D7.

---

## 5. Règles dictatoriales produites

Vingt règles, numérotées R1 à R20, détaillées dans `Annexe_A5_Gizmos_Camera.md` §10 et dans l'artefact §10. Les six qui ont le plus de portée hors de ce lot :

> **R1 — Source et dérivé.** Une source est toujours écrite **avant** l'étape qui la cuit. Écrire après produit un dérivé jamais recalculé ; écrire dans le dérivé produit un écrasement. Aucune des deux ne génère d'erreur.

> **R3 — Données de paire.** Une donnée qui dépend d'une paire ne peut pas être stockée sur l'un des deux membres. Le moteur fonctionne tant qu'il n'y a qu'un observateur, puis casse silencieusement au second.

> **R9 — Sentinelles numériques.** Une sentinelle dans un champ numérique est un booléen déguisé qui se propage dans l'arithmétique. `infinity()` est le moindre mal, mais `inf − inf` et `0 × inf` donnent `NaN`, et une normalisation par `inf` rend 0 partout sans symptôme.

> **R14 — Grandeurs structurelles.** Une grandeur structurelle se dérive de la donnée structurelle, jamais d'un attribut optionnel. Sinon l'absence de l'attribut se traduit par une structure vide plutôt que par une absence d'attribut.

> **R16 — Tests possiblement vides.** Tout test dont la boucle peut ne rien parcourir doit compter ce qu'il a vérifié et asserter que ce compte n'est pas nul.

> **R20 — Valeurs d'affichage debug.** Une valeur d'affichage debug ne se dérive pas d'une valeur géométrique. Les lier revient à faire dépendre l'outil de mesure de ce qu'il mesure.

### Principes méthodologiques ajoutés

> **11.** Quand deux positions opposées dans l'ordonnancement échouent toutes les deux, la cause n'est pas l'ordre : c'est la donnée écrite, ou l'étape qui la consomme.

> **12.** Une dépendance circulaire entre systèmes est presque toujours le symptôme d'une donnée trop grosse. Séparer ce qui est statique de ce qui est dérivé, et le cycle disparaît.

---

## 6. Dettes fermées

| Dette | État |
|---|---|
| **Bug 26** — `active` et `priority` écrites dans le JSON, jamais lues | **Fermé** par le `ParseCamera` refondu |
| **Dette D1** — association (caméra, viewport) inexistante comme donnée | **Fermée** par `CameraBinding` |
| **Dette D3** — `SpawnCameraGizmos` logée dans `SceneSerializer` par simple accès à `LinkChildToParent` | **Fermée** : `LinkChildToParent` extraite en API publique, gestion gizmo dans `Scene/DebugGizmos.*` |
| Nommage `m_` incohérent dans `ViewData` | **Fermé** : préfixe retiré sur les structures de passage, conservé sur les composants |
| `Viewport::Resize` orpheline | **Fermée** : supprimée (R17) |
| Sentinelle `farPlane = 1e30f` | **Fermée** : `hasFarPlane` + `farPlane` |
| `faceCount()` dérivé de `faceNormals.size()` | **Fermée** : dérive de `indices.size() / vertsPerFace` |

---

## 7. Dette D2 — reformulée, reportée en leçon

**Formulation initiale :** « `gizmo.length` en unités monde devrait donner une taille constante à l'écran ».

**Reformulation.** Une taille écran-constante exige `L = distance(observateur, gizmo) · k`. Mais l'**observateur** est la caméra qui *regarde* le gizmo, pas celle qu'il décrit — et un même gizmo peut être vu par plusieurs viewports simultanément, à des distances différentes.

`L` devient donc un attribut de la **paire (gizmo, vue observatrice)**. Il ne peut plus vivre dans `m_local.scale`, puisqu'une entité n'a qu'un transform. C'est le cas général de la règle R3, un cran au-dessus du bug 31.

**Trois issues, toutes structurantes :**

1. Une entité gizmo par vue observatrice — N×M entités plus le filtrage. Lourd.
2. Un scale appliqué **au moment du rendu**, hors du transform — introduit une notion de « transform par vue » que `RenderView` n'a pas.
3. Renoncer, et garder `L` en unités monde ajusté à la main.

La deuxième est la porte d'entrée du **billboarding**, des **LOD** et du **sprite scaling**. Ce n'est pas une correction, **c'est une leçon entière**.

> **Note.** Le bug du gizmo démesuré (session du 12-13/09, `__gizmo(Overview_Camera)` à `scale(256, 600, 5)`) en est la première manifestation pratique : `gizmoLength` n'a aucune prise sur la section en ortho, qui ne dépend que d'`orthoHeight`. Le correctif d'échelle ortho a été appliqué et validé ; la dette de fond reste entière.

---

## 8. Ce que ce lot débloque

- **Un split à quatre, une minimap, un picture-in-picture** : une ligne dans `BuildLayout`, rien ailleurs. Le gizmo, le rendu et les tests suivent, puisque tous les trois itèrent sur `nViews`.
- **Les AABB du BVH (L11), les normales, les axes de repère** : même `DebugVisualComponent`, même filtre de vue, même teinte.
- **Un mode `Depth` sur une seule vue** : `CameraBinding.mode`.
- **La dette D2 clairement posée** comme leçon future, avec ses trois issues évaluées.

---

## 9. L'enseignement central

Le gizmo « fonctionnait bien et à priori bien orienté » dès le premier lancement, **avec quatre bugs dedans**. Il était 2,4 fois trop ouvert et au mauvais ratio. Rien ne clochait visuellement, parce qu'aucun repère externe ne dit à l'œil quelle *devrait* être l'ouverture d'un frustum.

C'est la démonstration la plus nette du **principe méthodologique n°1** produite depuis la Leçon 04 :

> **« Ça s'affiche » n'est jamais un critère de validation.**
> Un cube rendu à l'envers ressemble à un cube. Un frustum au mauvais fov ressemble à un frustum.
