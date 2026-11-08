# Leçon 05 — La caméra, le frustum et le pipeline de rendu

> **Moteur :** LibraryV3 | **Projet :** LIB (Static Library) | **Compilateur :** Visual Studio 2026 / C++23
> **Namespace canonique :** `LV3` | **Préfixe macro :** `LV3_`
> **Prérequis :** Leçon 02 (mathématiques), Leçon 03 (ECS), Leçon 04 P1 (rasterizer — fondamentaux)

---

## 0. État de départ — audit du legacy LibV2

Le point de départ était une classe `Camera` de 400 lignes et une classe `Frustum` de 700, héritées de LibV2. Elles fonctionnaient. Elles n'étaient pas récupérables.

### Bugs de correction (B)

| # | Constat |
|---|---|
| B1 | `Frustum::Rebuild` composait `proj * view` — ordre inversé en convention vecteur-ligne. **Tous les plans étaient faux.** |
| B2 | `ComputeFrustumCorner` calculait les 4 coins **far** depuis le centre du plan **near** → frustum plat |
| B3 | Extraction du plan near en forme Direct3D `[0,1]` alors que la matrice était OpenGL `[-1,1]` |
| B4 | `getFoV()` renvoyait le **demi**-FOV (facteur 2 manquant) |
| B5 | `SetViewMatrix` composait **deux** sources d'orientation : base lookAt **et** quaternion |
| B6 | Rotation souris multipliée par `deltaTime` → sensibilité dépendante du framerate |
| B7 | `if (moveForward) movement -= forward` → on reculait en avançant |
| B8 | `forward` signifiait trois choses différentes dans trois méthodes de la même classe |
| B9 | Aucune protection sur le vecteur UP dégénéré ; contournée par un `EPSILON` dans `setOrigin` |
| B10 | Cinq méthodes déclarées et jamais définies |
| B11 | `enum { _LEFT, _NEAR, _FAR }` : identifiants réservés (UB) + collision avec les macros `windows.h` |
| B12 | `ClassifyPointAgainstFrustum(AABB3d*)` renvoyait `BEHIND` inconditionnellement |
| B13 | `result == 0.0` : comparaison de flottants, contraire à la règle de la Leçon 02 |
| B14 | `getMatrix()` et `getViewMatrix()` renvoyaient la même matrice — alors que ce sont des inverses |
| B15 | `TransformLocalMesh` écrivait les sommets transformés **dans le mesh partagé** |
| B16 | La caméra servait de bloc-notes (`ObjectSpaceCameraPos` réécrit par mesh) |

### Constat structurel

- **Huit représentations concurrentes** d'une seule orientation dans `Camera` (`Yaw/Pitch/Roll`, `theta/phi`, `script3D[]`, `rotationAngle`, `camera_rotation`, `rotation_mat`, `forward/right/UP`, `TO`)
- `Frustum.h` incluait `Camera.h`, `Mesh.h` **et** `GfxFunctions.h` — inversion de couches complète
- Sept fonctions de classification pour une seule question
- Trois énumérations pour un seul concept (`PointVSPlane`, `FrustumInside`, `PlaneSide`)

**Principe directeur de la leçon :** une caméra n'est pas un objet, c'est un contrat entre quatre responsabilités qui s'ignorent.

---

## 1. Les onze règles dictatoriales

### Règle 1 — « La caméra » n'existe pas

Il n'y a pas d'objet caméra. Il y a **quatre choses distinctes** qui ne se connaissent presque pas :

| Objet | Contient | Produit | Unity | Unreal |
|---|---|---|---|---|
| **Transform** | position, rotation | matrice **View** | `Transform` | `FTransform` |
| **Lentille** | fov/focale, near, far | matrice **Projection** | `Camera` | `FMinimalViewInfo` |
| **Viewport** | x, y, largeur, hauteur | **NDC → raster** | `Camera.rect` | `FViewport` |
| **Frustum** | 6 plans | **culling** | `CalculateFrustumPlanes` | `FConvexVolume` |

Le composant `Camera` d'Unity **ne contient aucune position**. Si tu veux savoir où est la caméra, tu demandes à son `Transform`, comme pour n'importe quel objet de la scène.

> **Corollaire :** si la caméra n'a pas de position, la logique « déplacer la caméra » devient de la logique de *Transform*, mutualisable avec tout autre objet. On ne code pas un déplacement deux fois.

### Règle 2 — La caméra ne bouge jamais, c'est le monde qui bouge

```
View = inverse(WorldMatrix_camera)
```

Le rasterizer ne sait projeter que depuis l'origine, le long d'un axe fixe. On ne déplace donc pas l'œil : on déplace la scène en sens inverse.

**A.** La View est une transformation **rigide**. Son inverse est analytique, jamais un Gauss-Jordan :

```
View = [ Rᵀ | -Rᵀ·t ]
```

**B.** `LookAt` est un **constructeur d'orientation**, pas un mode de fonctionnement. Il fabrique un quaternion à partir de deux points. Unity l'expose comme `Transform.LookAt()` — méthode du Transform, pas de la Camera.

### Règle 3 — Le pipeline des espaces

```
        M              V              P            /w          Viewport
Local ─────► Monde ─────► Vue ─────► Clip ─────► NDC ─────────► Raster
  │            │            │          │           │              │
  │            │            │          │           │              └─ pixels, y vers le BAS
  │            │            │          │           │                 z ∈ [0,1] (Z-buffer)
  │            │            │          │           │
  │            │            │          │           └─ x,y ∈ [-1,+1]   frustum symétrique
  │            │            │          │              z   ∈ [0,1]     REVERSE-Z : near→1, far→0
  │            │            │          │
  │            │            │          └─ 4D homogène. ON CLIPPE ICI.
  │            │            │             -w ≤ x ≤ w   -w ≤ y ≤ w   0 ≤ z ≤ w
  │            │            │             w = distance à l'œil  →  w < 0 = derrière
  │            │            │
  │            │            └─ œil à l'origine, regard vers -Z (main droite)
  │            │               V = inverse RIGIDE de la matrice monde de la caméra
  │            │
  │            └─ CULLING ICI : 6 plans Gribb-Hartmann extraits de V·P
  │               AABB monde, test p-vertex / n-vertex, 3 états
  │
  └─ sommets du mesh, AABB locale, normales
```

**Les trois règles que le schéma encode :**

1. On *cull* dans le **monde**, on *clippe* dans le **clip**. Le nom de l'espace le dit.
2. La division par `w` arrive **après** le clipping, jamais avant.
3. Le flip Y n'existe qu'à la **dernière** flèche, dans `Viewport::ToRaster()`. Nulle part ailleurs.

**Pourquoi clipper en espace de clip et non dans le monde** — trois raisons, par ordre d'importance :

1. **La correction.** Un sommet derrière l'œil a `w < 0`. Diviser avant de clipper inverse le signe : le point traverse l'écran et réapparaît en miroir. C'est le bug du « triangle qui explose quand on recule dedans ».
2. **Le coût.** En clip space, les six plans sont **canoniques** : une comparaison de deux flottants. En worldspace, trois multiplications par plan et par sommet.
3. **L'universalité.** Perspective, orthographique, projection oblique, far infini : les plans canoniques ne changent jamais.

### Règle 4 — Culling et clipping sont deux métiers

| | **Culling** | **Clipping** |
|---|---|---|
| Granularité | par objet / par nœud d'arbre | par triangle |
| Espace | monde (ou objet) | clip |
| Représentation | 6 plans explicites | plans canoniques implicites |
| Question | « est-ce que je m'en occupe ? » | « où est-ce que je coupe ? » |
| Réponse | Inside / Intersect / Outside | un polygone découpé |
| Coût | négligeable | élevé |
| Erreur tolérée | **conservative** | **exacte** |

**Un culling doit toujours être conservatif.** Répondre « peut-être visible » à tort coûte un peu de rendu inutile ; répondre « invisible » à tort fait disparaître un objet.

> **Le secret industriel :** dans un rasterizer professionnel, **seul le plan near impose un vrai clipping**. Les quatre plans latéraux se traitent par **scissor** sur la bounding box du triangle, avec une **guard band** (2× à 16× l'écran) au-delà de laquelle seulement on clippe. Sur une scène normale, **plus de 99 % des triangles ne sont jamais découpés.**

### Règle 5 — Gribb-Hartmann, et le choix de l'espace d'extraction

On n'obtient pas les plans depuis les 8 coins. On les extrait de la matrice.

- **Aucun cas particulier** : perspective, ortho, far infini, reverse-Z, projection oblique — une quinzaine de lignes pour tout.
- **Les normales pointent vers l'intérieur par construction.**
- **Pas de vecteurs `forward/right/up`** à maintenir, donc pas de dégénérescence à la verticale.
- Les coins ne servent plus qu'au **debug** et aux **cascades de shadow map**.

Et l'idée que presque personne n'exploite — **la matrice source détermine l'espace des plans** :

| Matrice source | Espace des plans | Usage |
|---|---|---|
| `P` | **vue** | culling de lumières, tiled shading |
| `V · P` | **monde** | culling de scène classique |
| `M · V · P` | **objet** | culling par mesh, sans transformer son AABB |

La troisième ligne est le vrai gain : transformer une AABB dans le monde la « regrossit ». Transformer les **6 plans** dans l'espace objet et tester l'AABB locale exacte est meilleur *et* moins cher.

### Règle 6 — Le test AABB : p-vertex / n-vertex

On ne teste jamais les 8 coins. On en teste **un ou deux**, choisis par le signe de la normale.

```
pour chaque plan :
    si distance(p-vertex) < 0  →  OUTSIDE, retour immédiat
    si distance(n-vertex) < 0  →  état = INTERSECT
retour : INSIDE ou INTERSECT
```

- **p-vertex** : le coin le plus loin dans la direction de la normale. S'il est derrière, les 8 le sont.
- **n-vertex** : le coin le plus proche. S'il est devant tous les plans, l'AABB est entièrement dedans.

**Le troisième état paie tout le reste :**

- Un nœud d'arbre **Inside** ⇒ tous ses enfants sont Inside, on arrête la descente.
- Un mesh **Inside** ⇒ **aucun de ses triangles n'a besoin d'être testé ni clippé.**

> **Faux positif assumé :** une grosse AABB peut chevaucher les 6 plans sans intersecter le frustum. Les moteurs pro l'acceptent : c'est conservatif, donc correct, et le corriger coûte plus cher que le rendu inutile.

### Règle 7 — FOV ou focale : un modèle, deux vocabulaires

```
fov = 2 · atan( tailleFilm / (2 · focale) )
```

- **Vocabulaire jeu** (Unity, Godot) : FOV vertical en degrés.
- **Vocabulaire film** (Maya, Nuke, Unreal `CineCameraComponent`) : focale en mm + *filmback*.

Un moteur professionnel expose **les deux** et dérive l'un de l'autre. La *resolution gate* (`Fill` / `Overscan`) est la terminologie de Maya.

**Deux erreurs à ne pas commettre :**

1. **L'aspect ratio appartient au viewport, pas à la lentille.** Sinon l'image se déforme au redimensionnement.
2. **Le FOV est *vertical* par convention.** Le fixer horizontalement fait varier ce que le joueur voit selon son écran.

### Règle 8 — La profondeur

Après division par `w`, la profondeur n'est pas linéaire :

```
z_ndc = (f/(f-n)) · (1 - n/z_vue)
```

Avec `near = 0.1` et `far = 1000`, **la moitié de la plage NDC est consommée par les 20 premiers centimètres.**

Les trois décisions professionnelles :

1. **NDC de profondeur dans `[0, 1]`** (Direct3D, Vulkan, Metal).
2. **Reverse-Z** : `near → 1`, `far → 0`, test de profondeur `GREATER`. La non-linéarité de la projection et celle du flottant se **compensent**.
3. **Far infini**, parfaitement conditionné une fois le reverse-Z en place.

### Règle 9 — L'ordre du culling en production

1. **Distance / LOD** — le moins cher, une comparaison de distance au carré
2. **Frustum culling hiérarchique** — BVH ou octree, pas une boucle linéaire
3. **Occlusion culling** — ce qui est derrière un mur
4. **Backface culling** — par triangle
5. **Rejet des triangles dégénérés / sub-pixel**

Sur le point 4, deux emplacements possibles :

| | Espace objet | Espace raster (standard GPU) |
|---|---|---|
| Méthode | `dot(normale, camPos - point) < 0` | signe de l'aire signée |
| Coût | 1 normale + 1 produit scalaire par face | **gratuit** : l'aire sert déjà aux barycentriques |
| Prérequis | normales stockées, camPos en espace objet | rien |
| Avantage | rejette **avant** la transformation | gère le scale négatif et le miroir |

Le second est la norme parce qu'il est gratuit. Le piège serait de faire les deux.

### Règle 10 — L'API réellement exposée

```
ViewMatrix              World → Vue
ProjectionMatrix        Vue   → Clip
ViewProjection          World → Clip          (mis en cache)
InverseViewProjection   ← celle qu'on oublie toujours
GetFrustum()            6 plans, espace monde
GetFrustumCorners()     debug et cascades d'ombres
ScreenPointToRay()      picking, raycast
WorldToScreenPoint()    UI ancrée, marqueurs
CullingMask             quelles couches cette caméra voit
```

`InverseViewProjection` permet le picking, la reconstruction de la position monde depuis le Z-buffer, et tout effet en espace écran. Elle se calcule **une fois par frame**, et **paresseusement**.

### Règle 11 — Le contrôleur n'est pas la caméra

```
Unity    :  Camera (composant)  +  script contrôleur  +  Cinemachine
Unreal   :  UCameraComponent    +  APlayerCameraManager  +  APlayerController
```

Aucun moteur professionnel ne met la lecture du clavier dans la classe caméra. Le découpage correct :

```
Input        → produit une intention
Controller   → convertit l'intention en Transform      ← interchangeable
Transform    → produit la View
Camera(lens) → produit la Projection
Frustum      → dérivé de V·P
```

Chaque flèche est unidirectionnelle. Rien ne remonte.

---

## 2. Le NDC : pourquoi `[0,1]` et pas `[-1,1]`

Le mot « NDC » recouvre deux notions selon la source. Scratchapixel emploie `[0,1]` pour X et Y dans sa leçon *Pinhole Camera*, et `[-1,1]` pour X, Y **et Z** dans sa leçon *Perspective Projection Matrix* (qui reproduit OpenGL). Le legacy LibV2 contenait littéralement les deux.

### X et Y : `[-1,1]`, sans débat

Trois raisons :

1. **Le frustum est symétrique** autour de l'axe de visée → les termes de décentrement s'annulent.
2. **Le clipping devient trivial** : `-w ≤ x ≤ w`.
3. **La précision du flottant est maximale au centre de l'écran**, là où l'œil regarde.

Le `[0,1]` de Scratchapixel pour X/Y est un espace intermédiaire de commodité. Il ne disparaît pas — il vit dans `Viewport::ToRaster()` :

```cpp
xr = (xn * 0.5f + 0.5f) * width;   // le "* 0.5 + 0.5", c'est son NDC [0,1]
```

### Z : c'est là que se joue le débat

OpenGL produit `[-1,1]`, puis **répare** avec `glDepthRange` :

```
matrice de projection      →   z_clip
division par w             →   z_ndc ∈ [-1, +1]
glDepthRange (défaut 0..1) →   z_window = (z_ndc + 1) / 2   ∈ [0, 1]
```

Direct3D, Vulkan et Metal produisent `[0,1]` directement. Une opération affine en moins — **et surtout, la conversion d'OpenGL arrive trop tard.**

### Le chiffre qui tranche

Nombre de valeurs `float` distinctes dans une tranche de 0,001 :

| Intervalle | Pas du flottant | Valeurs distinctes |
|---|---|---|
| `[0 ; 0,001]` | jusqu'à 1e-45 | **≈ 1 000 000 000** |
| `[0,999 ; 1,0]` | 5,96e-8 | **≈ 16 800** |
| `[-1,0 ; -0,999]` | 5,96e-8 | **≈ 16 800** |

Facteur **60 000**.

En `[0,1]`, une extrémité de la plage tombe dans la zone au milliard de valeurs — le reverse-Z l'y place au **far**, là où la projection en `1/z` est la plus écrasée. Les deux non-linéarités se compensent.

En `[-1,1]`, les deux extrémités sont grossières. **Le reverse-Z y est impossible** : inverser ne déplace rien.

### Le gain mesuré

Incertitude sur la distance réelle représentée par un seul pas de flottant (`n = 0.1`, `f = 1000`) :

| Distance | `[0,1]` standard | `[0,1]` reverse-Z | Gain |
|---|---|---|---|
| 1 m | 0,0006 mm | 0,00007 mm | ×8 |
| 100 m | 6 mm | 0,012 mm | ×500 |
| 1000 m | **60 cm** | 0,07 mm | **×8000** |

> **La règle à retenir :** `[-1,1]` sur X et Y, parce que le frustum est symétrique. `[0,1]` sur Z, parce que le flottant ne l'est pas.
>
> OpenGL lui-même a fait marche arrière : `glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE)` (GL 4.5, 2014) existe uniquement pour permettre le `[0,1]`. Vulkan n'offre même plus le `[-1,1]`.

---

## 3. Les décisions actées

```
REPÈRE
    Main DROITE, Y en haut, l'avant est -Z
    Vec3::Forward() = { 0, 0, -1 }     ← corrigé (valait +Z)
    Vec3::Back()    = { 0, 0, +1 }
    Verrou : static_assert(Right × Forward == Up)

MATRICES
    Convention  → vecteur-ligne, v' = v·M, composition S·R·T
    Stockage    → row-major, x[row][col], translation en LIGNE 3
    Matrix44    → ALGÈBRE SEULE. Toute fonction qui connaît une
                  convention de rendu (near/far, NDC, handedness)
                  sort du type matrice.

PROFONDEUR
    NDC         → x,y ∈ [-1,+1]   z ∈ [0,1]
    Reverse-Z   → near → 1, far → 0, test GREATER, clear à 0
    Far infini  → m[2][2] = 0, m[3][2] = near   (z_ndc = n / distance)

CLIPPING
    Espace      → CLIP, avant la division par w
    Plans       → 1 seul (near). Latéraux = scissor + guard band.

CULLING
    Extraction  → Gribb-Hartmann depuis V·P
    Test AABB   → p-vertex / n-vertex, TROIS états
    Enum        → EIntersect { Outside, Intersect, Inside }  (unique)

ARCHITECTURE
    La caméra   → CameraComponent (lentille) + TransformComponent
    Le mouvement→ contrôleurs interchangeables, écrivent m_local
    Le dérivé   → ViewData, recalculé chaque frame
    Le rendu    → RenderSystem → Renderer → Rasterizer → Fragment
```

### Le critère qui tranche les emplacements

> **`Matrix44` ne connaît que l'algèbre linéaire.**
> Dès qu'une fonction doit connaître une **convention de rendu** — near/far, plage NDC, main droite, reverse-Z, flip Y — elle sort du type matrice.

| Fonction | Verdict | Raison |
|---|---|---|
| `operator*`, `transposed`, `inverse` | **reste** | algèbre pure |
| `translate`, `scale`, `rotateX/Y/Z` | **reste** | algèbre pure |
| `inverseRigid()` | **reste** | « inverse d'une isométrie » |
| `Perspective`, `Orthographic` | **sort** → `Projection.h` | connaît near/far, NDC, reverse-Z |
| `LookAt` | **sort** → `Quat::LookRotation` | connaît la main droite |

C'est ce que font GLM (`glm::perspective`, fonction libre), DirectXMath (`XMMatrixPerspectiveFovRH`, fonction libre) et Unreal (`FReversedZPerspectiveMatrix`, classe dédiée). Unity fait l'inverse — c'est l'exception.

---

## 4. Architecture des fichiers

```
LIB — LibraryV3
  Maths/
    Vectorlib.h        Forward() = -Z + verrou static_assert
    MatrixLib.h        algèbre seule + inverseRigid()
    QuaternionLib.h    + LookRotation() / LookAt()
    Transform.h        position + Quatf + scale, ToLocalMatrix() inline
    Projection.h/.cpp  fabriques pures : Perspective, Infinite, Ortho, Filmback
    geometry/
      Plane.h          SignedDistance, Normalize, EPlaneSide
      AABB3d.h         + PVertex / NVertex / Transformed
      Frustum.h/.cpp   Build (Gribb-Hartmann) + Classify (3 états)

  Rendering/
    Viewport.h         NDC ↔ raster, flip Y, ClampBox (scissor)
    ViewData.h         V, P, V·P, frustum, position, invVP paresseux
    FrameBuffer.h      vue NON propriétaire sur les pixels verrouillés par SDL
    DepthBuffer.h      possède sa mémoire, reverse-Z, TestAndSet
    Rasterizer.h/.cpp  EdgeFunction, MulRow (inline) + balayage top-left
    Fragment.h/.cpp    callbacks de shading + leurs contextes
    Renderer.h/.cpp    ÉTAT de rendu : cibles, viewport, mode
    RenderTypes.h      ERenderMode, ECullMode, EDepthTest, EBlendMode

  Core/
    InputState.h       POD rempli par l'application, jamais par le moteur
    EventNames.h       constantes d'événements

  Scene/
    Components/Component.hpp   CameraComponent, CameraFollowComponent,
                               FPSControllerComponent, TransformComponent
    System.hpp/.cpp            contrôleurs, transformations, BuildViewData
    RenderSystem.h/.cpp        RenderView : géométrie → soumission

EXE — application
  main.cpp             SDL : fenêtre, texture, verrou, boucle
  BuildInputState()    SDL → InputState
```

**Aucune de ces couches ne connaît celle du dessus.** `Projection` ignore l'existence de `Camera`. `Frustum` ignore l'existence de `Mesh`. `Renderer` ignore l'existence du `Registry`.

> **Test de placement :** *« si je remplaçais SDL par Win32 GDI, ce fichier changerait-il ? »*
> Non → il va dans la LIB. Oui → il reste dans l'EXE.

### 4.1 Les quatre couches du rendu

```
RenderSystem  →  GÉOMÉTRIE   transforme, cull, projette
                              produit des Triangle2D en espace écran
     ↓
Renderer      →  ÉTAT        détient fb / db / viewport / mode,
                              choisit le chemin de fragment
     ↓
Rasterizer    →  BALAYAGE    bounding box, règle top-left, barycentriques
     ↓ callback
Fragment      →  PIXEL       test de profondeur, écriture couleur
```

`Renderer` n'est **pas** « une fonction qui dessine » : c'est le **propriétaire de l'état de rendu**. Tant qu'il était sans état, il n'était qu'un `switch` déguisé et paraissait inutile. Une fois qu'il détient les cibles, le viewport et le mode, les enums de `RenderTypes.h` — `ECullMode`, `EDepthTest`, `EBlendMode` — trouvent naturellement leur propriétaire.

Conséquence directe : `RenderView` ne prend plus de `ERenderMode` en argument. Le mode est un **état**, pas un paramètre de dessin. Les modes de debug `Depth` et `BarycentricColors` deviennent alors disponibles sur la scène 3D sans écrire une ligne.

### 4.2 Un FrameBuffer, N Viewports

`FrameBuffer` décrit **la mémoire**. `Viewport` décrit **la région qu'on y dessine**. Ils coïncident 99 % du temps ; c'est le 1 % restant qui justifie la séparation : écran splitté, minimap, incrustation, letterboxing, faces de cubemap.

```
┌──────────────── FrameBuffer 1600×900 ─────────────────┐
│         DepthBuffer 1600×900 (partagé)                │
├───────────────────────────┬───────────────────────────┤
│ Viewport {0,0,800,900}    │ Viewport {800,0,800,900}  │
│ ViewData #1               │ ViewData #2               │
│   viewMatrix  (suivi)     │   viewMatrix  (ensemble)  │
│   projection  fov 45°     │   projection  fov 60°     │
│   frustum     6 plans     │   frustum     6 plans     │
└───────────────────────────┴───────────────────────────┘
```

```cpp
renderer.BeginFrame(fb, db);

renderer.SetMode(ERenderMode::Solid);
RenderView(registry, rm, renderer, viewLeft);

renderer.SetMode(ERenderMode::Wireframe);
RenderView(registry, rm, renderer, viewRight);

renderer.EndFrame();
```

Le split-screen ne coûte **aucune modification du rendu** : on appelle `RenderView` deux fois. C'est le bénéfice concret du découplage — l'ancien `Frustum`, qui portait la lentille *et* l'aspect ratio *et* les plans *et* la classification, aurait exigé deux instances complètes et une refonte de `Clip3DAndProject`.

> ⚠️ **Le Z-buffer partagé ne fonctionne que parce que les deux viewports sont disjoints.** Les profondeurs de deux caméras ne sont pas comparables entre elles ; elles ne se rencontrent jamais tant que les régions ne se recouvrent pas. Une incrustation exigera un `DepthBuffer::ClearRect(vp)` avant la seconde vue.

### 4.3 La chaîne d'autorité sur les dimensions

```
UTILISATEUR / OS  →  redimensionne la FENÊTRE SDL
       ↓
main.cpp          →  LE SEUL DÉCIDEUR : taille de la texture
       ↓
fb.Bind(pixels, pitch, w, h)          → le FrameBuffer DÉCRIT, ne décide pas
       ↓
Viewport::FullScreen(fb.Width(), …)   → DÉRIVÉ, jamais stocké
       ↓
BuildViewData(..., viewport)          → consomme l'aspect ratio
```

Aucune classe du moteur ne stocke durablement les dimensions — sauf `DepthBuffer`, qui possède sa mémoire. Le redimensionnement de fenêtre coûte cinq lignes dans `main.cpp` : recréer la texture, appeler `db.Resize()`. Tout le reste suit, puisque tout est recalculé chaque frame.

C'était précisément le défaut du legacy : `Frustum` calculait `deviceAspectRatio` dans son **constructeur** et ne le remettait jamais à jour.

---

## 5. Code de référence

### 5.1 Projection perspective — reverse-Z, main droite

```cpp
// Maths/Projection.cpp  —  namespace LV3::Projection
//
// Convention : main DROITE (vue vers -Z), vecteur-ligne (v' = v·M),
//              NDC x,y ∈ [-1,+1] ; z ∈ [0,1] REVERSE-Z (near→1, far→0)

Matrix44f PerspectiveOffCenter(float l, float r, float b, float t,
                               float n, float f) noexcept
{
    Matrix44f m = Zeroed();                 // /!\ PAS l'identité : m[3][3] doit valoir 0

    m[0][0] =  (2.0f * n) / (r - l);
    m[1][1] =  (2.0f * n) / (t - b);
    m[2][0] =  (r + l) / (r - l);           // décentrement horizontal
    m[2][1] =  (t + b) / (t - b);           // décentrement vertical
    m[2][2] =  n / (f - n);                 // REVERSE-Z
    m[2][3] = -1.0f;                        // recopie -z dans w (main droite)
    m[3][2] =  (n * f) / (f - n);           // REVERSE-Z
    m[3][3] =  0.0f;                        // matrice projective
    return m;
}

Matrix44f Perspective(float fovYRad, float aspect, float n, float f) noexcept
{
    const float t = std::tan(fovYRad * 0.5f) * n;
    const float r = t * aspect;
    return PerspectiveOffCenter(-r, r, -t, t, n, f);   // cas symétrique
}

Matrix44f PerspectiveInfinite(float fovYRad, float aspect, float n) noexcept
{
    const float th = std::tan(fovYRad * 0.5f);
    Matrix44f m = Zeroed();
    m[0][0] =  1.0f / (aspect * th);
    m[1][1] =  1.0f / th;
    m[2][2] =  0.0f;                        // limite de n/(f-n) quand f → +∞
    m[2][3] = -1.0f;
    m[3][2] =  n;                           // z_ndc = n / distance
    return m;
}
```

**Différence avec la version OpenGL du legacy — deux coefficients :**

| | OpenGL `[-1,1]` | LV3 `[0,1]` reverse-Z |
|---|---|---|
| `m[2][2]` | `-(f+n)/(f-n)` | `n/(f-n)` |
| `m[3][2]` | `-2fn/(f-n)` | `n·f/(f-n)` |
| `m[2][3]` | `-1` | `-1` (inchangé) |

### 5.2 Inverse rigide

```cpp
// Maths/MatrixLib.h
//   M   = [ R  | 0 ]        M⁻¹ = [  Rᵀ    | 0 ]
//         [ t  | 1 ]              [ -t·Rᵀ  | 1 ]
Matrix44 inverseRigid() const noexcept
{
    Matrix44 r;
    r.x[0][0]=x[0][0]; r.x[0][1]=x[1][0]; r.x[0][2]=x[2][0];   // transposée 3x3
    r.x[1][0]=x[0][1]; r.x[1][1]=x[1][1]; r.x[1][2]=x[2][1];
    r.x[2][0]=x[0][2]; r.x[2][1]=x[1][2]; r.x[2][2]=x[2][2];

    r.x[3][0] = -(x[3][0]*r.x[0][0] + x[3][1]*r.x[1][0] + x[3][2]*r.x[2][0]);
    r.x[3][1] = -(x[3][0]*r.x[0][1] + x[3][1]*r.x[1][1] + x[3][2]*r.x[2][1]);
    r.x[3][2] = -(x[3][0]*r.x[0][2] + x[3][1]*r.x[1][2] + x[3][2]*r.x[2][2]);

    r.x[0][3] = r.x[1][3] = r.x[2][3] = T(0); r.x[3][3] = T(1);
    return r;
}
```

⚠️ **Contrat :** valable uniquement sur une **isométrie**. Avec un scale, seul `inverse()` générique est correct. La TNR §6.3 verrouille cette limite en assertant que `inverseRigid` **échoue** en présence de scale.

### 5.3 Extraction Gribb-Hartmann

```cpp
// Maths/geometry/Frustum.cpp
//
// Vecteur-ligne : clip.j = Σ_i v[i]·M[i][j] + M[3][j]
// La COLONNE j de M porte le composant j du vecteur clip :
//     normale = ( M[0][j], M[1][j], M[2][j] )      d = M[3][j]
//
// Volume canonique, NDC z ∈ [0,1] :
//     -w ≤ x ≤ w      -w ≤ y ≤ w      0 ≤ z ≤ w
//
// Les deux conditions en profondeur sont (z ≥ 0) et (w - z ≥ 0).
//     Standard   :  z ≥ 0  est le plan NEAR
//     Reverse-Z  :  z ≥ 0  est le plan FAR
// → on échange simplement les étiquettes.

void Frustum::Build(const Matrix44f& M, bool reverseZ, bool infiniteFar) noexcept
{
    auto col = [&M](int j) { return Plane(Vec3f(M[0][j], M[1][j], M[2][j]), M[3][j]); };
    auto add = [](const Plane& a, const Plane& b) { return Plane(a.normal + b.normal, a.d + b.d); };
    auto sub = [](const Plane& a, const Plane& b) { return Plane(a.normal - b.normal, a.d - b.d); };

    const Plane cx = col(0), cy = col(1), cz = col(2), cw = col(3);

    m_planes[Left]   = add(cw, cx);      //  x + w ≥ 0
    m_planes[Right]  = sub(cw, cx);      //  w - x ≥ 0
    m_planes[Bottom] = add(cw, cy);      //  y + w ≥ 0
    m_planes[Top]    = sub(cw, cy);      //  w - y ≥ 0

    if (reverseZ) { m_planes[Near] = sub(cw, cz); m_planes[Far] = cz;          }
    else          { m_planes[Near] = cz;          m_planes[Far] = sub(cw, cz); }

    // Far infini : la colonne z donne une normale nulle. Plan dégénéré, non testé.
    // (Far est le DERNIER de l'enum : c'est ce qui rend cette troncature possible.)
    m_count = infiniteFar ? int(Far) : int(PlaneCount);

    for (int i = 0; i < m_count; ++i) m_planes[i].Normalize();
}
```

⚠️ **`viewProj = view * projection`**, pas l'inverse. En convention vecteur-ligne, `v' = v·V·P`.

### 5.4 Classification AABB — trois états

```cpp
EIntersect Frustum::Classify(const AABB3d& box) const noexcept
{
    EIntersect result = EIntersect::Inside;

    for (int i = 0; i < m_count; ++i)
    {
        const Plane& p = m_planes[i];

        if (p.SignedDistance(box.PVertex(p.normal)) < 0.0f)
            return EIntersect::Outside;                  // sortie immédiate

        if (p.SignedDistance(box.NVertex(p.normal)) < 0.0f)
            result = EIntersect::Intersect;
    }
    return result;
}
```

### 5.5 La fonction d'arête — un noyau, deux gabarits

```cpp
// Rendering/Rasterizer.h — DEFINIES dans le header (inline)
//
//  Elle ne travaille QUE sur x et y : le z n'intervient jamais.
//  Trois services pour le prix d'un :
//    * sur trois sommets   -> deux fois l'aire signée (backface, normalisation)
//    * signe sur un pixel  -> le pixel est-il du bon côté de l'arête
//    * valeur / aire       -> coordonnée barycentrique

[[nodiscard]] LV3_FORCEINLINE constexpr float EdgeFunction(
    float ax, float ay, float bx, float by, float px, float py) noexcept
{
    return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
}

template<typename V>
[[nodiscard]] LV3_FORCEINLINE float EdgeFunction(const V& a, const V& b,
                                                 float px, float py) noexcept
{ return EdgeFunction(a.x, a.y, b.x, b.y, px, py); }

template<typename V>
[[nodiscard]] LV3_FORCEINLINE float EdgeFunction(const V& a, const V& b,
                                                 const V& c) noexcept
{ return EdgeFunction(a.x, a.y, b.x, b.y, c.x, c.y); }
```

Trois fonctions couvrent `Vec2f`, `Vec3f` et tout type doté d'un `.x` et d'un `.y`. Empiler des surcharges concrètes menait à cinq variantes et à un `C2665` à chaque nouveau site d'appel.

### 5.6 `BuildViewData` — le point de convergence

```cpp
ViewData BuildViewData(const TransformComponent& tr, const CameraComponent& cam,
                       const Viewport& vp) noexcept
{
    ViewData v;
    v.viewport = vp;  v.reverseZ = true;                       // ÉTAPE 0 — contexte

    const Matrix44f& world = tr.m_worldMatrix;                 // ÉTAPE 1 — View
    v.viewMatrix = world.inverseRigid();                       //   inverse RIGIDE
    v.position   = {  world[3][0],  world[3][1],  world[3][2] };
    v.forward    = { -world[2][0], -world[2][1], -world[2][2] };  // main droite : -Z

    const float fovY = (cam.m_lensModel == ELensModel::Filmback)   // ÉTAPE 2 — Projection
        ? Projection::FovYFromFocal(cam.m_focalLengthMm, cam.m_filmHeightMm)
        : cam.m_fovYDeg * TO_RADIAN;

    v.projectionMatrix = cam.m_infiniteFar
        ? Projection::PerspectiveInfinite(fovY, vp.Aspect(), cam.m_nearPlane)
        : Projection::Perspective(fovY, vp.Aspect(), cam.m_nearPlane, cam.m_farPlane);

    v.viewProjectionMatrix = v.viewMatrix * v.projectionMatrix;   // ÉTAPE 3 — V·P

    v.frustum.Build(v.viewProjectionMatrix, true, cam.m_infiniteFar);  // ÉTAPE 4 — 6 plans

    v.nearPlane = cam.m_nearPlane;
    v.farPlane  = cam.m_infiniteFar ? 1e30f : cam.m_farPlane;
    return v;                                    // invViewProjection : PARESSEUX
}
```

**Il n'y a pas d'étape 5 ici.** La matrice `Model · View · Projection` se compose **par mesh**, dans `RenderView`. `BuildViewData` ne connaît aucun mesh : c'est ce qui permet d'en construire plusieurs par frame.

### 5.7 `RenderView` — la boucle de rendu

```cpp
void RenderView(Registry& registry, ResourceManager& rm,
                Renderer& renderer, const ViewData& view)
{
    renderer.SetViewport(view.viewport);                       // l'état de la VUE

    for (auto&& [entity, meshComp, transform] : registry.ViewGroup<MeshComponent, TransformComponent>())
    {
        const MeshClass* mesh = rm.GetMesh(meshComp.m_meshHandle);
        if (!mesh || mesh->faceCount() == 0) continue;

        const Matrix44f& modelMatrix = transform.m_worldMatrix;          // 5. Model

        if (view.frustum.Classify(mesh->GetMeshAABB().Transformed(modelMatrix))
            == EIntersect::Outside) continue;                            // 6+7. Culling

        const Matrix44f mvp = modelMatrix * view.viewProjectionMatrix;   // 9+10. MVP
        const uint8_t   vpf = mesh->vertsPerFace;                        // 3 ou 4

        for (size_t f = 0; f < mesh->faceCount(); ++f)
        {
            const uint32_t base = uint32_t(f) * vpf;

            // Local -> CLIP. SoA INDEXÉ : indices[base + k], PAS indices[base] + k
            Vec4f c[4];
            for (uint8_t k = 0; k < vpf; ++k)
                c[k] = MulRow(mvp, mesh->vertexPositions[mesh->indices[base + k]]);

            // Rejet du near. Diviser par un w négatif ramènerait le point EN MIROIR.
            bool behind = false;
            for (uint8_t k = 0; k < vpf; ++k)
                if (c[k].w <= view.nearPlane) { behind = true; break; }
            if (behind) continue;

            // /w -> NDC -> RASTER (flip Y dans ToRaster)
            Vec3f r[4];
            for (uint8_t k = 0; k < vpf; ++k)
            {
                const float inv = 1.0f / c[k].w;
                r[k] = view.viewport.ToRaster({ c[k].x*inv, c[k].y*inv, c[k].z*inv });
            }

            // 8. Backface culling : GRATUIT, l'aire sert aussi aux barycentriques
            if (EdgeFunction(r[0], r[1], r[2]) <= 0.0f) continue;

            // 11. SOUMISSION. RenderView ne connaît ni le rasterizer ni les fragments.
            const Color col = FaceColor(int(f));
            for (uint8_t t = 0; t + 2 < vpf; ++t)          // triangulation en éventail
                renderer.DrawTriangle(
                    Triangle2D{ { r[0].x, r[0].y }, { r[t+1].x, r[t+1].y },
                                { r[t+2].x, r[t+2].y },
                                  r[0].z, r[t+1].z, r[t+2].z },
                    col);
        }
    }
}
```

---

## 6. L'ordre des systèmes — non négociable

```
1. InputSystem              →  InputState (rempli par l'application)
2. AnimationSystem          ┐
   FPSControllerSystem      ├─ écrivent m_local, lèvent m_dirty
   CameraFollowSystem       ┘  (m_isEnabled arbitre entre les deux caméras)
3. LocalTransformSystem     →  m_local  →  m_localMatrix   (consomme m_dirty)
4. WorldTransformSystem     →  propagation descendante depuis les racines
5. FindActiveCamera + BuildViewData        (une fois PAR VUE)
6. RenderView                              (une fois PAR VUE)
```

> **Le bug classique :** exécuter l'étape 5 avant l'étape 4. La caméra lit alors la matrice monde de la frame *précédente* — une frame de retard, quasi invisible en debug, très perceptible à la souris.

**Chaque contrôleur ne fait qu'une chose : écrire dans `m_local`.** C'est ce qui rend FPS, suivi, orbite et cinématique interchangeables sans toucher une ligne du rendu.

---

## 7. Les bugs attrapés pendant l'implémentation

Journal empirique — chacun de ces bugs aurait survécu à une relecture.

| # | Bug | Détecté par |
|---|---|---|
| 1 | `proj * view` au lieu de `view * proj` | audit du code |
| 2 | `Vec3::Forward()` valait `+Z` en main droite | TNR §5.0 |
| 3 | Garde anti-division à `EPSILON_FLOAT = 1e-3` alors que la normale du plan far vaut `1e-4` → **plan jamais normalisé** | TNR §7.1 |
| 4 | `16 / 9` en **division entière** → aspect = 1.0 | TNR §1 |
| 5 | Double conversion degrés→radians dans `ParseTransform` → inclinaison **57× trop petite** | trace du pôle |
| 6 | `nlohmann::json` itère par ordre **alphabétique** → `Transform` parsé après `Mesh` → rayon d'orbite nul | log de chargement |
| 7 | Rayon d'orbite calculé avec `position.length()` — **contaminé par Y** | invariant `\|xz\| == R` |
| 8 | Struct `Transform` fantôme dans `Component.hpp` masquant le vrai — avec `scale = (0,0,0)` par défaut | `#pragma message(__FILE__)` |
| 9 | `Entity{}` vaut `0`, une entité **valide** — `NULL_ENTITY` est `0xFFFFFFFF` | relecture de `Entity.hpp` |
| 10 | Rotation composée `spin * initial` → **précession parasite** de l'axe | trace du pôle |
| 11 | `"texte" + entier` = arithmétique de pointeur, pas concaténation | compilation |
| 12 | Chaînes littérales en ANSI faute de `/utf-8` | affichage console |
| 13 | `RGB` est une **macro de `wingdi.h`** → `C4430` incompréhensible | renommage en `MakeColor` |
| 14 | `LV3_FORCEINLINE` **déclaré** dans le `.h`, **défini** dans le `.cpp` → 3 × `LNK2019` | édition de liens |
| 15 | Surcharges d'`EdgeFunction` empilées jusqu'au `C2665` | remplacées par 2 gabarits |
| 16 | `.cpp` absents du `.vcxproj` : compilés par IntelliSense, jamais par le compilateur | `LNK2019` en série |
| 17 | Contextes de fragment divergents castés depuis un `void*` → lecture décalée de 12 octets | `z0..z2` aberrants, mode `Depth` seul touché |
| 18 | Biais top-left à `-1.0f` : constante **absolue** appliquée à une grandeur **relative** → trous sur les petits triangles | l'œil : bruit de fond en 3D, rien en 2D |

### Ce que ce journal enseigne

- **Le bug n°3 classifiait correctement** mais mesurait faux d'un facteur 10 000. Il n'aurait cassé qu'au moment des cascades d'ombres, des mois plus tard.
- **Le bug n°5 a été diagnostiqué par un rapport constant.** Trois objets indépendants divisés par 55,69 ; `sin(23.5°·k) / sin(23.5°·k²) = 55.68`. Un facteur uniforme sur des objets indépendants n'est jamais un hasard : c'est une constante de conversion.
- **Le bug n°8 se trouve avec `#pragma message(__FILE__)`.** Quand un symbole « n'existe pas » alors qu'on vient de l'écrire, il faut faire dire au compilateur *quel fichier il lit*, pas deviner.
- **Le bug n°14 donne une règle générale :**

> `inline` / `LV3_FORCEINLINE` ⇒ **le corps va dans le `.h`.**
> Une fonction `inline` n'émet aucun symbole externe : sa définition doit être visible dans chaque unité de compilation qui l'appelle. Une fonction d'une ligne appelée en boucle interne → header. Une fonction qui contient elle-même des boucles → `.cpp`.

- **Le bug n°17 est le prix du `void*`.** `static_cast<T*>(void*)` réussit **toujours**, quel que soit `T` : le compilateur ne peut rien vérifier. Trois contextes différents (`SolidContext`, `DepthContext`, `UnlitContext`) pour un seul point d'envoi, et la mémoire est réinterprétée en silence.

```
Ce que Renderer ENVOIE          Ce que ShadeFragment_Depth LISAIT
FragmentContext                 DepthContext
┌──────────────────┐  offset    ┌──────────────────┐
│ FrameBuffer*  fb │   0..7     │ FrameBuffer*  fb │  ✅
├──────────────────┤   8..11    ├──────────────────┤
│ DepthBuffer*  db │            │ float         z0 │  ❌ moitié basse du POINTEUR
│                  │  12..15    │ float         z1 │  ❌ moitié haute
├──────────────────┤  16..19    ├──────────────────┤
│ Color      color │            │ float         z2 │  ❌ lit color
├──────────────────┤  20..31    └──────────────────┘
│ float   z0,z1,z2 │  ← jamais lus
└──────────────────┘
```

> **Parade :** un **seul** type de contexte pour tous les callbacks. Si un jour plusieurs sont nécessaires, un champ `magic` vérifié par assertion en Debug.
>
> `BarycentricColors` « fonctionnait » par accident : il n'utilise que `fb`, à l'offset 0 dans les deux structures.

- **Le bug n°18 donne la règle la plus transposable de toute la leçon :**

> **Une constante absolue appliquée à une grandeur relative est une bombe à retardement.**
> `EdgeFunction = |arête| × distance`. Exiger `w ≥ 1` revient à exiger `distance ≥ 1/|arête|` : 0,0025 px sur un triangle de 400 px, **0,33 px** sur un triangle de 3 px. Le `-1` venait de la virgule fixe, où l'unité vaut 1/256 de pixel. En flottant, il n'a aucun sens.
>
> Symptôme diagnostique : **un échec qui disparaît quand la taille augmente** ne peut désigner qu'une constante absolue sur une grandeur relative.

---

## 8. La TNR — ce qu'elle verrouille

Deux fichiers de test, ~180 assertions.

### `TestMatrixLib` — l'algèbre

| § | Verrouille | Si ça casse |
|---|---|---|
| 1 | La convention : ligne 3, row-major, point vs direction | quelqu'un est passé en colonne-majeur |
| 2 | `A*B` = « A puis B » | tous les produits `V·P`, `enfant·parent` s'inversent |
| 3 | Sens des rotations, main droite | objets miroir, backface culling inversé |
| 4 | L'ordre S·R·T du chaînage | un scale non uniforme déforme après rotation |
| 5 | `inverse()` et son contrat sur les singulières | crash ou NaN silencieux |
| 6 | `inverseRigid` **et sa limite** | quelqu'un l'appelle sur une matrice scalée |
| 7 | Quaternion et matrice parlent la même langue | rotations divergentes selon le chemin |
| 8 | `LookRotation` | caméra qui regarde à l'envers |

### `TestCameraMath` — la caméra

Projection (coefficients, reverse-Z, monotonie, bords, `w < 0` derrière l'œil), far infini, orthographique, inverse rigide, `LookRotation` (dont le cas dégénéré), `PVertex`/`NVertex`, construction du frustum (plans normalisés, near/far), classification AABB (les trois états), viewport (flip Y).

### `TestRasterizer` — la couverture

Le test historique vérifiait qu'aucun pixel n'était dessiné **deux fois**. Il ne pouvait pas voir le bug n°18 : un pixel dessiné **zéro fois** passait tous les contrôles. Il faut les deux moitiés de l'invariant.

> **Deux triangles adjacents couvrent leur quad EXACTEMENT une fois — ni trou, ni doublon — QUELLE QUE SOIT LEUR TAILLE.**

```cpp
for (float size : { 3.0f, 5.0f, 8.0f, 15.0f, 40.0f, 200.0f })
{
    int holes = 0, doubles = 0;
    MeasureQuadCoverage(size, holes, doubles);   // carré pivoté de 0,3 rad,
    assert(holes == 0 && doubles == 0);          // découpé sur sa diagonale
}
```

Ce qu'aurait affiché l'ancien biais :

```
  cote    3.0 px :  2 trou(s),  0 doublon(s)   <<< FAIL
  cote    5.0 px :  3 trou(s),  0 doublon(s)   <<< FAIL
  cote    8.0 px :  1 trou(s),  0 doublon(s)   <<< FAIL
  cote   15.0 px :  0 trou(s),  0 doublon(s)   OK
  cote  200.0 px :  0 trou(s),  0 doublon(s)   OK
```

Trois choix de conception, chacun pour une raison :

| Choix | Raison |
|---|---|
| **Carré pivoté** de 0,3 rad | convexe par construction, aucune arête alignée sur la grille — un quad axial testerait un cas dégénéré favorable |
| **Marge proportionnelle** (`0.5 × size` en unités de fonction d'arête) | un demi-pixel réel à toutes les échelles ; une marge fixe serait elle-même un biais dépendant de la taille |
| **Boucle sur les tailles** | l'invariant n'est pas « ça marche » mais « ça marche **à toutes les échelles** » |

> **Un test qui ne balaie pas son paramètre critique ne teste qu'un point de l'espace.** Le test de non-recouvrement passait — il utilisait de gros triangles.

### Les invariants d'exécution

```cpp
void CheckSceneInvariants(Registry& registry)   // appelée chaque frame en _DEBUG
{
    // 1. Le quaternion reste unitaire      → détecte l'accumulation sur soi-même
    // 2. |xz(local)| == m_orbitRadius      → détecte la dérive d'orbite
    // 3. m_orbitRadius > 0 si vitesse ≠ 0  → détecte le chargement muet
    // 4. scale != 0                        → détecte la géométrie invisible
}
```

> **Un invariant vaut mieux qu'une valeur attendue.** Une valeur, il faut la calculer à la main ; un invariant se vérifie tout seul, à chaque frame, gratuitement. Trois des seize bugs ci-dessus ont été trouvés ainsi.

### La méthode de débogage numérique

1. **N'affiche jamais que la position monde** — c'est la fin d'une chaîne à quatre maillons. Affiche `local`, `world`, `|xz|`, `rayon`, `angle`, `pôle`.
2. **Instrumente le chargement**, pas seulement l'exécution. Un récapitulatif des meshes chargés aurait montré `orbitRadius = 0` avant même la première frame.
3. **Vérifie des invariants**, pas des valeurs.
4. **`dt = 0` doit reproduire le JSON.** Si un objet bouge à `dt = 0`, le bug est dans les matrices ; sinon il est dans l'animation. L'espace de recherche est divisé en deux d'une seule exécution.
5. **Ne cherche pas la parité au bit près avec l'ancienne version.** Elle contenait quatre bugs. La référence, ce sont les invariants.

### Isoler un rendu qui n'affiche rien

Trois coupes, trois sous-systèmes :

1. Commente le test `area <= 0` → si le mesh apparaît, le signe de winding est inversé (piège du flip Y).
2. Commente `if (Classify == Outside) continue;` → si le mesh apparaît, le frustum ou l'AABB est faux.
3. Affiche `r[0]` d'un seul triangle → s'il sort de l'écran, le problème est dans les matrices, pas dans le rasterizer.

### Et si le fond transparaît à travers la géométrie

**La forme des trous est le diagnostic.** Mets une couleur de fond criarde et regarde de près :

| Ce que tu vois | Cause |
|---|---|
| Pixels **épars, comme du bruit** | seuil de couverture dépendant de l'échelle (bug n°18) |
| **Fentes nettes** le long des arêtes partagées | même cause, stade avancé |
| **Triangles entiers** manquants | culling, clipping ou géométrie — pas le remplissage |
| Trous **qui clignotent** d'une frame à l'autre | z-fighting sur géométrie coplanaire |

Trois coupes pour confirmer :

1. Biais du rasterizer à `0` → les trous disparaissent ⇒ c'était le seuil.
2. Test de profondeur commenté → les trous disparaissent ⇒ z-fighting.
3. Mode `Wireframe` → si le fil de fer laisse déjà des vides, le problème est dans le mesh ou la triangulation des quads.

---

## 9. Chantiers — état

| # | Chantier | Livrable | État |
|---|---|---|---|
| 1a | `MatrixLib` | `inverseRigid()`, fabriques retirées | ✅ |
| 1b | `Projection` | reverse-Z, far infini, ortho, filmback | ✅ |
| 2 | `geometry/Plane` | `SignedDistance`, `Normalize` | ✅ |
| 3 | `geometry/AABB3d` | `PVertex`/`NVertex`, `Transformed` | ✅ |
| 4 | `geometry/Frustum` | Gribb-Hartmann, `Classify` 3 états | ✅ |
| 5 | `CameraComponent` | lentille pure + contrôleurs séparés | ✅ |
| 5bis | `TransformComponent` | embarque `LV3::Transform` | ✅ |
| 6 | `Viewport` | flip Y, `ClampBox`, `FullScreen` | ✅ |
| 7 | `ViewData` | matrices + frustum + invVP paresseux | ✅ |
| 8 | `BuildViewData` / `FindActiveCamera` | dans `Scene/System.cpp` | ✅ |
| 9 | `RenderSystem` + `Renderer` | 4 couches, écran splitté validé | ✅ |

### Dette technique identifiée

| Sujet | Risque |
|---|---|
| `MeshClass::GetFaceView` suppose les sommets **contigus** dans `vertexPositions` | normales et UV fausses dès qu'on s'en servira |
| `ComputeMeshAABB()` n'est appelée nulle part automatiquement | si l'`OBJLoader` l'oublie : `meshAABB` invalide, culling incohérent, écran noir |
| `Renderer` n'exploite ni `ECullMode`, ni `EDepthTest`, ni `EBlendMode` | trois enums sans usage effectif |
| Matériaux : `submeshes` ignorés, couleur par hash de face | pas de rendu réaliste possible |
| Leçon 02 affirme encore « main gauche, +Z entre dans l'écran » | documentation mensongère |

### Chantiers différés

- ❌ coins du frustum / debug draw
- ❌ culling en espace objet (extraction depuis `M·V·P`)
- ❌ guard band
- ❌ BVH / octree — la boucle linéaire suffit
- ❌ occlusion culling
- ❌ `enum class Entity` (sécurité de type) — touche toute la signature du Registry
- ❌ refonte du `TriggerSystem` (détection N²)

---

## 10. Résumé décisionnel

```
LA CAMÉRA N'EXISTE PAS
    Transform (position/rotation) + Lentille (fov/near/far)
    + Viewport (pixels) + Frustum (6 plans)
    Aucun des quatre ne connaît les trois autres.

LA VIEW EST UN INVERSE RIGIDE
    View = [ Rᵀ | -Rᵀt ]     jamais un Gauss-Jordan

ON CULL DANS LE MONDE, ON CLIPPE DANS LE CLIP
    Culling  : 6 plans, AABB, 3 états, conservatif
    Clipping : plans canoniques, avant /w, plan near SEUL

LA PROFONDEUR
    NDC z ∈ [0,1], reverse-Z, far infini
    ×8000 de précision à 1 km, pour deux coefficients de matrice

LE CONTRÔLEUR N'EST PAS LA CAMÉRA
    Input → Controller → Transform → View
    Changer de caméra = activer un composant

LE RENDU EN QUATRE COUCHES
    RenderSystem (géométrie) → Renderer (état)
    → Rasterizer (balayage) → Fragment (pixel)
    Le mode d'affichage est un ÉTAT, pas un argument.

MATRIX44 NE CONNAÎT QUE L'ALGÈBRE
    Toute fonction qui connaît une convention de rendu en sort

INLINE ⇒ CORPS DANS LE HEADER
    Une fonction inline n'émet aucun symbole externe.

UN FICHIER APPARTIENT À LA COUCHE DE SON ARGUMENT LE PLUS HAUT

JAMAIS DE CONSTANTE ABSOLUE SUR UNE GRANDEUR RELATIVE
    Un échec qui disparaît quand la taille augmente en dénonce une.

UN SEUL TYPE DE CONTEXTE DERRIÈRE UN void*
    Le compilateur ne vérifie rien ; c'est à la conception de le faire.

RÈGLE D'OR
    Un invariant vaut mieux qu'une valeur attendue.
    Un test qui ne balaie pas son paramètre critique ne teste qu'un point.
    Un test qui échoue tôt vaut mieux qu'un bug qui se tait.
```

---

*Prochaine étape — Leçon 04 Partie 2 : clipping near en espace de clip, interpolation perspective-correcte (le `z` s'interpole linéairement, les UV et les couleurs exigent le `1/w`), et exploitation du troisième état du culling.*
