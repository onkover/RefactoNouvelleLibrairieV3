# Annexe A5 — Les gizmos de caméra

> Annexe à la **Leçon 05 — Caméra & Frustum**.
> Rattachée aux espaces **E4 (Vue)** et **E5 (Clip)** de la Consolidation C1.
> Prérequis : L03 (ECS, ResourceManager), L04 P2 (rasterizer, clipping), L05 (`ViewData`, `Frustum`).

---

## 1. Le problème posé

Depuis la Leçon 04, l'écran est scindé en deux : la vue de gauche rend la caméra FPS ou Follow, celle de droite affiche la scène entière. Mais **les caméras elles-mêmes n'y figurent pas**. Impossible de voir où elles sont, où elles regardent, ni quel volume elles couvrent.

Deux objectifs :

1. Afficher toutes les caméras de la scène comme des objets visualisables.
2. Distinguer visuellement la caméra dont l'image est rendue à gauche.

Le chantier a produit quatre bugs, deux dettes corrigées, une dette reformulée, et une structure — `CameraBinding` — qui est devenue le point de décision unique de toute la configuration d'affichage du moteur.

---

## 2. Le principe fondateur : dériver, ne pas modéliser

### 2.1 Le contre-exemple

La première intuition consiste à ajouter un composant `Mesh` pointant vers un `camera.obj` décoratif :

```json
"components": {
  "Camera": { "fov": 45.0 },
  "Mesh":   { "source": "assets/meshes/camera_body.obj" }
}
```

**Ce qu'il ne faut pas faire.** Le mesh est un asset figé : il ne connaît ni le `fov`, ni l'aspect, ni le type de projection. Changez le `fov` à 90° — le gizmo continue d'afficher la même pyramide. L'objet de debug ment sur ce qu'il est censé décrire.

> **Un objet de debug qui ment est pire que pas d'objet de debug : il transforme un outil de diagnostic en source d'erreur.**

Le risque concret : vous finiriez par déboguer le **culling** en croyant le gizmo, alors que c'est le gizmo qui est faux.

### 2.2 La règle

Le gizmo doit être **dérivé** des paramètres de la caméra, jamais stocké à côté. Et il doit être **validé numériquement** contre ces paramètres — pas à l'œil.

### 2.3 L'astuce du mesh canonique

Générer un mesh par caméra serait une réponse naïve. La bonne réponse exploite une propriété de la projection perspective : **tronquer une pyramide à n'importe quelle distance produit une figure semblable**. Les angles sont conservés.

Un seul mesh canonique suffit donc pour toutes les caméras perspectives du moteur :

- apex à l'origine (position de l'œil),
- quad lointain en `z = −1`, demi-extensions `(1, 1)`.

La forme réelle sort d'une matrice de scale :

```
Sx = length · tan(fovY/2) · aspect
Sy = length · tan(fovY/2)
Sz = length
```

`length` est une **longueur d'affichage** (3 à 5 unités), pas le `farPlane` : un `far = 1000` produirait un gizmo qui écrase la scène entière.

### 2.4 Le cas orthographique

Le volume orthographique n'est pas une pyramide déformée : c'est une **boîte**. Sa section transversale est constante, alors que celle du frustum perspectif croît linéairement avec `z`. Ce n'est pas une variation de forme, c'est une différence de nature — et elle se lit dans les formules :

| | Perspective | Orthographique |
|---|---|---|
| `Sx` | `L · tanHalf · aspect` | `(orthoHeight/2) · aspect` |
| `Sy` | `L · tanHalf` | `orthoHeight/2` |
| `Sz` | `L` | `L` |

Côté ortho, **`L` n'apparaît plus dans `Sx` ni `Sy`**. Allonger le gizmo ne l'élargit pas. Il faut donc un **second mesh canonique** : une boîte, face avant en `z = 0`, face arrière en `z = −1`, demi-extensions `(1, 1)` — même convention que la pyramide, pour que le système reste symétrique.

---

## 3. Les deux assets

### 3.1 `assets/meshes/camera_gizmo.obj` — perspective

```
o CameraGizmo

# ---- sommets du frustum ----
v  0.0  0.0  0.0     # 1 : apex
v -1.0 -1.0 -1.0     # 2 : far bas-gauche
v  1.0 -1.0 -1.0     # 3 : far bas-droit
v  1.0  1.0 -1.0     # 4 : far haut-droit
v -1.0  1.0 -1.0     # 5 : far haut-gauche
# ---- sommets du marqueur "up" ----
v -0.35  1.05 -1.0   # 6
v  0.35  1.05 -1.0   # 7
v  0.00  1.60 -1.0   # 8

vn  0.000000 -0.707107  0.707107   # 1 : face basse
vn  0.707107  0.000000  0.707107   # 2 : face droite
vn  0.000000  0.707107  0.707107   # 3 : face haute
vn -0.707107  0.000000  0.707107   # 4 : face gauche
vn  0.000000  0.000000 -1.000000   # 5 : quad far + marqueur recto
vn  0.000000  0.000000  1.000000   # 6 : marqueur verso

f 1//1 2//1 3//1
f 1//2 3//2 4//2
f 1//3 4//3 5//3
f 1//4 5//4 2//4
f 5//5 4//5 3//5
f 5//5 3//5 2//5
f 6//5 7//5 8//5
f 8//6 7//6 6//6
```

`vertsPerFace = 3` · `faceCount() = 8` · 8 sommets · 6 normales

### 3.2 `assets/meshes/camera_gizmo_box.obj` — orthographique

Une boîte de 12 triangles (8 sommets, 6 normales axiales) plus le même marqueur « up » (2 triangles, réutilisant les normales `−Z` et `+Z` déjà déclarées pour les faces avant et arrière).

`vertsPerFace = 3` · `faceCount() = 14` · 11 sommets · 6 normales

### 3.3 Le marqueur « up »

Les deux meshes sont symétriques en Y. Une caméra tournée de 180° autour de son axe de visée (un **roll**) produit un gizmo **pixel pour pixel identique**. Le petit triangle asymétrique posé sur l'arête supérieure lève l'ambiguïté à l'œil.

Il est doublé à winding inversé pour rester visible malgré le backface culling.

### 3.4 Winding et convention

CCW vu de l'extérieur, conforme à la convention OBJ standard. Si le solide apparaît retourné : `OBJLoadOptions::flipWindingOrder = true`.

Rappel du **bug 20** de la L04 P2 : le flip Y du viewport inverse le winding en espace raster, donc front-face = aire **négative**.

---

## 4. L'architecture ECS

### 4.1 Les composants

```cpp
// Le gizmo sait quelle camera il decrit.
struct CameraGizmoComponent
{
    Entity m_owner  = NULL_ENTITY;
    float  m_length = 3.0f;          // longueur d'affichage, PAS le farPlane
};

// GENERIQUE : la couche rendu ignore ce qu'est une camera.
struct DebugVisualComponent
{
    Color  m_color;
    Entity m_hideForCamera = NULL_ENTITY;   // masque si la vue vient de cette camera
};
```

**Règle de couche.** `RenderView` n'apprend jamais le mot « caméra ». Il ne connaît que `DebugVisualComponent`. C'est ce qui permettra de réutiliser exactement le même mécanisme pour afficher les AABB d'un futur BVH.

### 4.2 Le porteur des assets

```cpp
struct GizmoAssets
{
    MeshHandle m_perspective;    // camera_gizmo.obj      (pyramide,  8 faces)
    MeshHandle m_orthographic;   // camera_gizmo_box.obj  (boite,    14 faces)

    [[nodiscard]] bool IsValid() const noexcept
    { return m_perspective.IsValid() && m_orthographic.IsValid(); }

    [[nodiscard]] MeshHandle For(EProjectionType p) const noexcept
    {
        LV3_ASSERT(IsValid());
        return (p == EProjectionType::Orthographic) ? m_orthographic : m_perspective;
    }
};
```

`For()` est le **point de décision unique**. Chaque site qui choisit un mesh l'appelle ; aucun ne réécrit le `if`.

L'échec de chargement ne remonte pas par `std::expected` :

> **`std::expected` se justifie quand l'appelant peut agir différemment selon la cause de l'échec.** Si toutes les causes mènent à la même action, un état booléen suffit et la raison appartient au journal, pas au type de retour.

Fichier introuvable, OBJ malformé, mesh vide : même conséquence — pas de gizmos. Mais le mode dégradé doit être **bruyant** :

```cpp
if (gizmoAssets.IsValid())
    SpawnCameraGizmos(registry, gizmoAssets);
else
    Logger::warn("[Gizmo] assets absents : aucun gizmo de camera ne sera affiche");
```

### 4.3 Le spawn

```cpp
void SpawnCameraGizmos(Registry& registry, const GizmoAssets& assets)
{
    LV3_ASSERT(assets.IsValid());

    // 1. COLLECTER d'abord : creer des entites pendant l'iteration
    //    d'un ViewGroup invalide les tableaux denses du SparseSet.
    std::vector<Entity> cameras;
    for (auto&& [e, cam] : registry.ViewGroup<CameraComponent>())
        if (cam.m_gizmoLength > 0.0f) cameras.push_back(e);

    // 2. Creer ensuite.
    for (Entity camEntity : cameras)
    {
        const CameraComponent& cam = registry.getComponent<CameraComponent>(camEntity);

        Entity g = registry.CreateEntity();
        registry.addComponent(g, NameComponent{
            "__gizmo(" + EntityLabel(registry, camEntity) + ")" });
        registry.addComponent(g, TransformComponent{});
        registry.addComponent(g, MeshComponent{ assets.For(cam.m_projection) });
        registry.addComponent(g, CameraGizmoComponent{ camEntity, cam.m_gizmoLength });
        registry.addComponent(g, DebugVisualComponent{ Color{}, camEntity });
        LinkChildToParent(registry, g, camEntity);
    }
}
```

Trois points structurants :

**Collecter avant de créer.** Créer des entités pendant l'itération d'un `ViewGroup` invalide les tableaux denses du `SparseSet`. C'est le pattern *command buffer* de la L03, appliqué au spawn.

**Le spawn intervient après `ParseHierarchy`, jamais dans `ParseCamera`.** `LinkChildToParent` crée un `HierarchyComponent` sur le parent. Si `ParseCamera` liait un gizmo immédiatement, la caméra recevrait un `HierarchyComponent{parent vide, [gizmo], isRoot=true}` — puis `ParseHierarchy`, qui tourne après tous les nœuds, l'écraserait pour y mettre son vrai parent, **emportant la liste `m_children`**. Gizmo orphelin, aucun message.

**L'entité est créée dans un état déjà cohérent.** `assets.For(cam.m_projection)` au spawn *et* dans le système :

> **Une entité est créée dans un état déjà cohérent. Un système entretient un invariant, il ne le fabrique pas.** Sinon la validité de l'état initial devient une propriété de l'ordonnancement, donc invisible et fragile.

### 4.4 Le gizmo est un nœud *enfant* — et c'est obligatoire

Le scale ne doit **jamais** être posé sur le nœud de la caméra elle-même. `View = world.inverseRigid()` est un inverse **rigide**, qui suppose R orthonormale. Un scale sur la matrice monde de la caméra casserait `inverseRigid()` silencieusement et tordrait toute la projection.

```
CameraNode  (T × R pur, aucun scale)
 └── GizmoNode  (scale calcule + MeshComponent)
```

Bénéfice secondaire : la propagation de transform existante fait tout le travail, et le gizmo traverse le rendu comme n'importe quel mesh. Aucun cas particulier dans la boucle de traversal.

### 4.5 Le système

```cpp
void CameraGizmoSystem(Registry& registry, Entity activeCamera,
                       const CameraBinding* bindings, size_t count,
                       const GizmoAssets& assets)
{
    LV3_ASSERT(count == 0 || bindings[0].camera == activeCamera);

    for (auto&& [e, giz, tr, mc, dbg] :
         registry.ViewGroup<CameraGizmoComponent, TransformComponent,
                            MeshComponent, DebugVisualComponent>())
    {
        const CameraComponent* cam = registry.TryGet<CameraComponent>(giz.m_owner);
        if (!cam) continue;

        // L'aspect vient du viewport ou CETTE camera est rendue. Pas d'ailleurs.
        const CameraBinding* b = nullptr;
        for (size_t i = 0; i < count; ++i)
            if (bindings[i].camera == giz.m_owner) { b = &bindings[i]; break; }

        const float aspect = b ? b->viewport.Aspect() : 0.0f;

        // ── a) FORME : ecriture dans la SOURCE m_local ────────────────
        if (aspect > 0.0f)
        {
            const float L = giz.m_length;
            Vec3f wanted;

            if (cam->m_projection == EProjectionType::Orthographic)
            {
                const float halfH = cam->m_orthoHeight * 0.5f;
                wanted = { halfH * aspect, halfH, L };
            }
            else
            {
                const float tanHalf = std::tan(CameraFovY(*cam) * 0.5f);
                wanted = { L * tanHalf * aspect, L * tanHalf, L };
            }

            if (std::fabs(wanted.x - tr.m_local.scale.x) > 1e-6f ||
                std::fabs(wanted.y - tr.m_local.scale.y) > 1e-6f ||
                std::fabs(wanted.z - tr.m_local.scale.z) > 1e-6f)
            {
                tr.m_local.scale = wanted;
                tr.m_dirty       = true;      // le contrat avec LocalTransformSystem
            }
        }

        // ── b) MESH : suit le type de projection, meme a l'execution ──
        const MeshHandle want = assets.For(cam->m_projection);
        if (mc.m_meshHandle.id != want.id) mc.m_meshHandle = want;

        // ── c) ETAT : hors du bloc conditionnel ───────────────────────
        dbg.m_color = (giz.m_owner == activeCamera)
                    ? Color{ 255, 216,  26 }    // ACTIVE   : ambre
                    : Color{ 110, 112, 128 };   // inactive : gris froid
    }
}
```

Le `if (aspect > 0.0f)` plutôt qu'un `continue` : une caméra non rendue voit quand même son mesh et sa couleur mis à jour. Seule la forme est gelée. Un `continue` figerait aussi la couleur — la troisième caméra ne changerait plus de teinte en devenant active.

Le seuil `1e-6f` est **absolu** et c'est délibéré : les composantes valent quelques unités, jamais `1e-9` ni `1e9`. Un seuil relatif serait plus rigoureux en général, mais inutile ici.

**`m_dirty` est un contrat.** `TransformComponent` porte un drapeau qui évite de recuire les matrices pour rien.

> **Tout système qui écrit dans la source et ne lève pas le drapeau introduit un bug *différé* : correct à la première frame, faux à toutes les suivantes.** C'est la pire catégorie, parce que le test de démarrage passe au vert.

### 4.6 Les deux lignes dans `RenderView`

**Filtre de vue**, tout en haut du corps de la boucle d'entités, *avant* `rm.GetMesh()` — c'est un rejet d'entité, autant éviter le hachage de la map, la classification du frustum et la construction de la MVP :

```cpp
const DebugVisualComponent* dbg = registry.TryGet<DebugVisualComponent>(entity);
if (dbg && dbg->m_hideForCamera == view.sourceCamera) continue;
```

Sans ce `continue`, la vue de gauche rendrait l'intérieur de son propre frustum : géométrie tranchée par le near plane, plein écran.

**Teinte**, hoistée hors de la boucle des faces — `dbg` est invariant par entité :

```cpp
const bool  hasTint = (dbg != nullptr);
const Color tint    = hasTint ? dbg->m_color : Color{};

for (size_t f = 0; f < mesh->faceCount(); ++f)
{
    ...
    const Color col = hasTint ? tint : FaceColor(int(f));
}
```

---

## 5. `CameraBinding` — l'association réifiée

### 5.1 Le problème

`CameraGizmoSystem` recevait initialement **un seul** `float aspect` pour **toutes** les caméras. Or l'aspect n'est pas une propriété de la caméra : c'est celle du viewport où elle est rendue.

Le bug était invisible parce que les deux viewports faisaient 768×800 chacun — même aspect. Le nombre unique se trouvait correct pour les deux, **par hasard, pas par construction**.

> **Une donnée qui dépend d'une paire ne peut pas être stockée sur l'un des deux membres.** Tenter de l'y loger produit un moteur qui fonctionne tant qu'il n'y a qu'un observateur — et casse silencieusement au second.

### 5.2 Le faux cycle

La première tentative faisait consommer les `ViewData` au système. Mais `ViewData` contient les matrices, produites par `WorldTransformSystem` — d'où un cycle apparent, résolu par une **frame de retard**.

C'était une dette inacceptable : judder sur la Follow amortie, culling qui rejette la géométrie entrant dans le champ (objets qui *apparaissent* au bord, un frame trop tard), erreur cumulée sur les hiérarchies profondes, flash d'une frame au démarrage.

Le cycle n'en était pas un :

```
CameraGizmoSystem a besoin de  ->  l'ASPECT du viewport   (statique)
BuildViewData a besoin de      ->  la MATRICE MONDE       (derivee)
```

> **Principe méthodologique n°12.** Une dépendance circulaire entre systèmes est presque toujours le symptôme d'une donnée trop grosse. Sépare ce qui est statique de ce qui est dérivé, et le cycle disparaît.

### 5.3 La structure

```cpp
// L'association (camera, viewport, mode). AUCUNE matrice : construite
// avant les transforms, donc disponible pour tout consommateur.
struct CameraBinding
{
    Entity      camera = NULL_ENTITY;
    Viewport    viewport;
    ERenderMode mode   = ERenderMode::Solid;
};
```

### 5.4 Géométrie et politique, séparées

`BuildLayout` découpe un rectangle. Il ne connaît aucune entité :

```cpp
enum class ELayout : uint8_t { Single, SplitH, SplitV, Quad, MainSide };

size_t BuildLayout(ELayout layout, int w, int h, Viewport* out, size_t capacity)
{
    LV3_ASSERT(out);

    if (w < 8 || h < 8)
    {
        Logger::warn("[Layout] fenetre trop petite : " + std::to_string(w)
                   + "x" + std::to_string(h) + ", vue unique forcee");
        out[0] = { 0, 0, (w > 0 ? w : 1), (h > 0 ? h : 1) };
        return 1;
    }

    const int hw = w / 2, hh = h / 2;

    switch (layout)
    {
    case ELayout::Single:
        out[0] = { 0, 0, w, h };
        return 1;

    case ELayout::SplitH:                       // cote a cote
        out[0] = { 0,  0, hw,     h };
        out[1] = { hw, 0, w - hw, h };
        return 2;

    case ELayout::SplitV:                       // l'un au-dessus de l'autre
        out[0] = { 0, 0,  w, hh     };
        out[1] = { 0, hh, w, h - hh };
        return 2;

    case ELayout::MainSide:                     // 75% principal, 25% lateral
    {
        const int mainW = (w * 3) / 4;
        out[0] = { 0,     0, mainW,     h };
        out[1] = { mainW, 0, w - mainW, h };
        return 2;
    }

    case ELayout::Quad:
        out[0] = { 0,  0,  hw,     hh     };
        out[1] = { hw, 0,  w - hw, hh     };
        out[2] = { 0,  hh, hw,     h - hh };
        out[3] = { hw, hh, w - hw, h - hh };
        return 4;
    }
    return 0;   // enum hors domaine
}
```

Le `w - hw` plutôt qu'un second `hw` : sur une largeur impaire, le pixel orphelin est attribué plutôt que perdu.

**Pas de `default:`** — le jour où `ELayout::PictureInPicture` est ajouté, MSVC signale le `switch` incomplet (C4062).

`BuildCameraBindings` associe caméras et modes aux cases produites :

```cpp
struct ViewSlot { Entity camera; ERenderMode mode; };

size_t BuildCameraBindings(ELayout layout, const ViewSlot* slots, size_t slotCount,
                           int w, int h, CameraBinding* out, size_t capacity)
{
    Viewport vps[8];
    const size_t n = BuildLayout(layout, w, h, vps, std::size(vps));

    LV3_ASSERT(n == slotCount && n <= capacity);

    for (size_t i = 0; i < n; ++i)
    {
        LV3_ASSERT(slots[i].camera != NULL_ENTITY);
        out[i] = { slots[i].camera, vps[i], slots[i].mode };
    }

    // Aucune camera deux fois : son gizmo porte m_hideForCamera et serait
    // masque dans TOUTES les vues ou elle apparait, donc invisible partout.
#ifdef _DEBUG
    for (size_t i = 0; i < n; ++i)
        for (size_t j = i + 1; j < n; ++j)
            LV3_ASSERT(out[i].camera != out[j].camera);
#endif

    return n;
}
```

> **Une postcondition s'asserte dans la fonction qui produit le résultat, pas dans un test qui le consulte plus tard.** La proximité au site de la faute est la moitié de la valeur d'une assertion.

### 5.5 Le paramètre `Registry` supprimé

`BuildCameraBindings` ne lit aucun composant : c'est de la géométrie pure. Le `Registry` que portait sa première version était un paramètre mort.

> **La signature d'une fonction est une déclaration de ce dont elle dépend.** Un paramètre inutilisé est un mensonge sur ce contrat — il interdit de raisonner sur l'ordonnancement et masque la pureté réelle de la fonction.

Même remarque pour `fb` : la fonction a besoin de **deux entiers**, pas d'un `FrameBuffer` dont l'état n'existe qu'entre `Bind()` et `Unbind()`.

### 5.6 Le mode de rendu appartient au binding

Passer `ERenderMode` en paramètre à `RenderView` recréerait un chemin parallèle aux viewports — deux sources, une divergence. Le mode est un attribut de la **vue**, même origine et même véhicule que le viewport :

```cpp
renderer.SetViewport(view.viewport);
renderer.SetMode(view.mode);        // l'etat de la VUE, pas du renderer
```

> **Un objet stateful partagé entre plusieurs consommateurs exige que chacun pose son état complet en entrée.** Ce qui n'est pas explicitement affecté est hérité — et l'héritage silencieux d'état est indiagnosticable.

`Renderer` étant stateful et vivant hors de la boucle, une assertion de fin de `RenderView` verrouille le contrat :

```cpp
LV3_ASSERT(renderer.GetMode() == view.mode);
```

### 5.7 L'ordonnancement final

```
1. PollEvents                    -> met a jour WinW / WinH
   const int frameW = WinW, frameH = WinH;   // FIGEES pour la frame

2. FPSControllerSystem / CameraFollowSystem   -> ecrivent m_local
3. BuildCameraBindings           -> (camera, viewport, mode) : AUCUNE matrice
4. CameraGizmoSystem             -> lit l'aspect, ecrit m_local.scale
5. LocalTransformSystem          -> cuit m_local -> m_localMatrix
6. WorldTransformSystem          -> propage m_worldMatrix
7. TriggerSystem
8. BuildViewData x nViews        -> lit les matrices de CETTE frame
9. Tests (_DEBUG)
10. SDL_LockTexture / fb.Bind / RenderView x nViews / Unbind
```

Zéro retard de frame. Toute la logique est **hors du verrou SDL** : si une assertion saute, aucune texture n'est verrouillée.

`frameW` / `frameH` figées : un redimensionnement survenu en cours de frame prend effet à la suivante, plutôt que de produire des bindings d'une taille et un `fb.Bind()` d'une autre.

---

## 6. Le JSON

### 6.1 `JsonReader::Child` — descendre dans un sous-objet

`"gizmo"` et `"length"` n'appartiennent pas au même nœud. Un `JsonReader` est attaché à **un** nœud ; il en faut donc deux, et un moyen de descendre en marquant la clé au passage :

```cpp
// Descente dans un sous-objet. NON const : elle consomme une cle.
[[nodiscard]] JsonReader Child(const char* key)
{
    m_seen.insert(key);            // "gizmo" est lue ICI, en tant que CONTENEUR

    // Objet vide de repli : duree de vie statique, donc jamais pendant.
    static const json s_empty = json::object();

    const auto it = m_j.find(key);
    const json& sub = (it != m_j.end() && it->is_object()) ? *it : s_empty;

    return JsonReader(sub, m_comp + "." + key, m_owner);
}
```

Quatre décisions :

- **`m_seen.insert(key)`** — sans lui, le `WarnUnread()` du parent signalerait « clé ignorée `gizmo` » alors qu'on vient de la lire. `Child()` est un `Read()` dont la valeur est un nœud au lieu d'un scalaire.
- **`static const json s_empty`** — `m_j` est une **référence**. Si la clé manque, il faut lier cette référence à quelque chose qui survit au `return`. Une locale non-statique donnerait une référence pendante.
- **`find()` plutôt que `operator[]`** — sur un `json` non-const, `operator[]` **insère** un `null` quand la clé manque.
- **`m_comp + "." + key`** — le préfixe devient `Camera.gizmo`, donc l'avertissement dira exactement où chercher.

**Le contre-exemple :**

```cpp
c.m_gizmoLength = r.Read(compJson.value("length", 3.0f));   // FAUX
```

`compJson.value(...)` fait la recherche lui-même et retourne un `float`. `JsonReader` reçoit `3.0f` — un nombre nu. Il n'a jamais vu la chaîne `"length"` et son `WarnUnread()` la déclarera comme clé ignorée.

> **Une clé JSON n'est jamais lue par `contains()` + `operator[]` + `.value()`. Elle est lue uniquement par `Read()`, à qui l'on passe le nom de la clé, jamais sa valeur.** Le nom est la seule chose que le lecteur peut enregistrer.

### 6.2 `ParseCamera` refondu

```cpp
void SceneSerializer::ParseCamera(const void* pJsonNode, ParseContext& ctx,
                                  Entity entity, Entity& out_activeCamera)
{
    const json& j = *static_cast<const json*>(pJsonNode);
    if (!j.is_object()) return;

    const std::string owner = EntityLabel(ctx.registry, entity);
    JsonReader r(j, "Camera", owner);

    CameraComponent c;

    // ── 1. PROJECTION : discriminant de premier niveau ─────────────
    c.m_projection = r.ReadProjectionType("projection");

    // ── 2. PLANS ──────────────────────────────────────────────────
    c.m_nearPlane   = r.Read("near",  0.1f);
    c.m_infiniteFar = r.Read("infiniteFar", false);
    c.m_farPlane    = r.Read("far", 1000.0f);

    if (c.m_infiniteFar && r.Has("far"))
        Logger::warn("[Camera] " + owner + " : 'far' est ignore (infiniteFar=true)");

    // ── 3. LENTILLE : chaque branche assigne TOUS ses champs ──────
    if (c.m_projection == EProjectionType::Orthographic)
    {
        c.m_lensModel   = ELensModel::FieldOfView;      // sans objet, mais DEFINI
        c.m_orthoHeight = r.Read("orthoHeight", 10.0f);

        if (c.m_infiniteFar)
        {
            Logger::warn("[Camera] " + owner + " : 'infiniteFar' sans effet en ortho");
            c.m_infiniteFar = false;
        }
    }
    else
    {
        // UN SEUL discriminant, explicite.
        const std::string lens = r.Read("lens", std::string("fov"));
        c.m_lensModel = (lens == "filmback") ? ELensModel::Filmback
                                             : ELensModel::FieldOfView;
        if (lens != "fov" && lens != "filmback")
            Logger::warn("[Camera] " + owner + " : 'lens' inconnu '" + lens + "' -> fov");

        if (c.m_lensModel == ELensModel::Filmback)
        {
            c.m_focalLengthMm = r.Read("focalLength", 35.0f);
            c.m_filmHeightMm  = r.Read("filmHeight",  24.0f);
            c.m_gateFit = (r.Read("gateFit", std::string("fill")) == "overscan")
                        ? EGateFit::Overscan : EGateFit::Fill;
        }
        else
        {
            c.m_fovYDeg = std::clamp(r.Read("fov", 45.0f), 1.0f, 179.0f);
        }
    }

    // ── 4. GIZMO ──────────────────────────────────────────────────
    if (r.Has("gizmo"))
    {
        JsonReader rg = r.Child("gizmo");
        c.m_gizmoLength = std::max(0.0f, rg.Read("length", 2.0f));
        rg.WarnUnread();                            // TNR du sous-objet
    }
    else
        c.m_gizmoLength = 0.0f;                     // 0 = pas de gizmo

    // ── 5. SELECTION ──────────────────────────────────────────────
    c.m_isActive = r.Read("active",  false);   // defaut FALSE : l'activite se declare
    c.m_priority = r.Read("priority", 0);

    // ── 6. GARDE-FOUS QUI PARLENT ─────────────────────────────────
    if (c.m_nearPlane <= 0.0f)
    {
        Logger::warn("[Camera] " + owner + " : near <= 0, force a 0.1");
        c.m_nearPlane = 0.1f;
    }
    if (!c.m_infiniteFar && c.m_farPlane <= c.m_nearPlane)
    {
        Logger::warn("[Camera] " + owner + " : far <= near, force a near*1000");
        c.m_farPlane = c.m_nearPlane * 1000.0f;
    }

    ctx.registry.addComponent(entity, c);
    r.WarnUnread();                                 // DERNIERE ligne du parse

    if (c.m_isActive)
    {
        const CameraComponent* cur = (out_activeCamera != NULL_ENTITY)
            ? ctx.registry.TryGet<CameraComponent>(out_activeCamera) : nullptr;
        if (!cur || c.m_priority >= cur->m_priority)
            out_activeCamera = entity;
    }
}
```

Défauts corrigés par rapport à la version accumulée :

| # | Défaut | Correction |
|---|---|---|
| 1 | `contains("focalLength")` **+** `lensModel` : deux discriminants concurrents | une clé `"lens"` unique |
| 2 | `focalLength` lu deux fois, `infiniteFar` lu deux fois | une lecture par clé |
| 3 | `orthoHeight` lu inconditionnellement — coche la clé même en perspective | déplacé dans la branche ortho |
| 4 | `clamp(m_fovYDeg)` en fin de fonction — s'applique aussi aux Filmback où le champ n'est pas lu | déplacé dans la branche `fov` |
| 5 | Garde-fous muets | garde-fous qui journalisent |
| 6 | `active` par défaut à `true` | défaut `false` : l'activité se déclare |
| 7 | `m_lensModel` non écrit sur tous les chemins | chaque branche assigne tous ses champs |

**Le mensonge silencieux le plus vicieux** était `"far": 30.0` cohabitant avec `"infiniteFar": true` sur les trois caméras : `far` lu, clampé, garde-fouté, puis **jeté** par `BuildViewData` qui appelle `PerspectiveInfinite`. Aucun avertissement.

> **Corollaire du principe n°9.** Une donnée lue et jamais consommée est un mensonge que le TNR valide. Le lecteur prouve qu'on a regardé la clé, jamais qu'on en a fait quelque chose.

### 6.3 `CameraFovY` — la source unique de l'angle

`BuildViewData` et `CameraGizmoSystem` calculaient tous deux le FOV vertical. Deux chemins vers la même grandeur, donc deux vérités possibles.

```cpp
[[nodiscard]] inline float CameraFovY(const CameraComponent& cam) noexcept
{
    LV3_ASSERT(cam.m_lensModel == ELensModel::Filmback ||
               (cam.m_fovYDeg >= 1.0f && cam.m_fovYDeg <= 179.0f));
    return (cam.m_lensModel == ELensModel::Filmback)
        ? Projection::FovYFromFocal(cam.m_focalLengthMm, cam.m_filmHeightMm)
        : cam.m_fovYDeg * TO_RADIAN;
}
```

### 6.4 `ReadProjection` doit parler

Une valeur non reconnue qui retombe silencieusement en perspective, c'est le garde-fou muet à l'échelle du type de projection :

```cpp
[[nodiscard]] EProjectionType ReadProjection(const json& j, const char* key)
{
    const std::string s = j.value(key, std::string("perspective"));
    if (s == "orthographic" || s == "ortho") return EProjectionType::Orthographic;
    if (s == "perspective"  || s == "persp") return EProjectionType::Perspective;

    Logger::warn("[Camera] projection inconnue '" + s + "' -> perspective");
    return EProjectionType::Perspective;
}
```

---

## 7. Catalogue JSON des caméras

Deux axes **indépendants**, à ne jamais confondre :

- `Camera.active` / `Camera.priority` — sélection de la lentille qui rend la vue.
- `CameraFPS.enabled` / `CameraFollow.enabled` — autorisation d'écrire dans le transform.

Une caméra peut être `active: false` avec un contrôleur `enabled: true` : elle bouge sans être regardée. L'inverse est également valide.

| # | Projection | Lentille | Contrôleur | Parent | Gizmo | Statut |
|---|---|---|---|---|---|---|
| 1 | perspective | fov | FPS | interdit | oui | valide |
| 2 | perspective | fov | Follow | interdit | oui | valide |
| 3 | perspective | filmback | aucun | permis | oui | valide |
| 4 | orthographic | — | aucun | permis | oui | valide |
| 5 | orthographic | — | FPS | interdit | oui | valide |
| 6 | orthographic | — | Follow | interdit | oui | valide |
| 7 | perspective | fov | aucun | permis | **non** | observateur |
| 8 | — | — | FPS **+** Follow | — | — | **interdit** |

### 7.1 Perspective + FPS

```json
{
  "id": "FPS_Camera",
  "_note": "PAS de parent : le controleur ecrit en local",
  "components": {
    "Transform": {
      "translation": [ 0.0, 5.0, 40.0 ],
      "rotation": [ 0.0, 0.0, 0.0 ],
      "scale": [ 1.0, 1.0, 1.0 ]
    },
    "Camera": {
      "projection": "perspective",
      "lens": "fov",
      "fov": 45.0,
      "near": 0.1,
      "infiniteFar": true,
      "active": true,
      "priority": 10,
      "gizmo": { "length": 3.0 }
    },
    "CameraFPS": {
      "enabled": true,
      "moveSpeed": 15.0,
      "mouseSensitivity": 0.15,
      "lockVertical": false,
      "pitchLimit": 89.0
    }
  }
}
```

Pas de `far` : `infiniteFar` le rend inopérant, et le parse avertit s'il traîne.

### 7.2 Perspective + Follow

```json
"Camera": {
  "projection": "perspective",
  "lens": "fov",
  "fov": 45.0,
  "near": 0.1,
  "far": 2000.0,
  "infiniteFar": false,
  "active": true,
  "priority": 5,
  "gizmo": { "length": 3.0 }
},
"CameraFollow": {
  "enabled": true,
  "target": "Cube1",
  "offset": [ 0.0, 5.0, -35.0 ],
  "smoothSpeed": 5.0,
  "lookAtHeight": 0.0
}
```

`priority: 5` contre 10 : les deux sont `active`, la FPS gagne.

### 7.3 Perspective sténopé, statique

```json
"Camera": {
  "projection": "perspective",
  "lens": "filmback",
  "focalLength": 50.0,
  "filmHeight": 24.0,
  "gateFit": "fill",
  "near": 0.1,
  "far": 500.0,
  "infiniteFar": false,
  "active": false,
  "priority": 0,
  "gizmo": { "length": 4.0 }
}
```

Pas de `fov` : `CameraFovY()` le dérive de la focale. L'écrire déclencherait `WarnUnread`.

### 7.4 Orthographique, vue de dessus

```json
"Transform": {
  "translation": [ 0.0, 150.0, 0.0 ],
  "rotation": [ -90.0, 0.0, 0.0 ],
  "scale": [ 1.0, 1.0, 1.0 ]
},
"Camera": {
  "projection": "orthographic",
  "orthoHeight": 120.0,
  "near": 1.0,
  "far": 400.0,
  "active": false,
  "priority": 0,
  "gizmo": { "length": 30.0 }
}
```

`far` obligatoire et **réellement consommé** ici. `length: 30` et non 3 : une boîte de 120 unités de haut sur 3 de profondeur est une dalle illisible.

### 7.5 Orthographique + Follow (isométrique)

```json
"Transform": { "rotation": [ -35.264, 45.0, 0.0 ] },
"Camera": {
  "projection": "orthographic",
  "orthoHeight": 30.0,
  "near": 1.0,
  "far": 300.0,
  "active": false,
  "priority": 0,
  "gizmo": { "length": 10.0 }
},
"CameraFollow": {
  "enabled": true,
  "target": "Cube1",
  "offset": [ 60.0, 60.0, -60.0 ],
  "smoothSpeed": 5.0,
  "lookAtHeight": 1.0
}
```

`−35.264°` est `atan(1/√2)` — l'angle isométrique exact.

### 7.6 L'observateur, sans gizmo

```json
"Camera": {
  "projection": "perspective",
  "lens": "fov",
  "fov": 60.0,
  "near": 1.0,
  "infiniteFar": true,
  "active": false,
  "priority": 0
}
```

L'absence de bloc `gizmo` met `m_gizmoLength` à 0 : aucune entité créée. Économie d'un cas dégénéré — le gizmo de l'observateur ne serait visible dans aucune vue.

### 7.7 Les trois interdits

**Deux contrôleurs sur la même entité** — `CheckControllerExclusivity` doit sauter (bug 25).

**Un parent sous un contrôleur** — le contrôleur écrit un transform absolu ; `WorldTransformSystem` y composerait la matrice du parent. Dérive silencieuse.

**Clés hors branche** — `fov` ou `infiniteFar` sur une ortho : `WarnUnread` doit parler pour chacune.

---

## 8. Les tests

### 8.1 Trois fenêtres, pas une

Ce qui distingue les fenêtres n'est pas la chronologie mais la **durée de vie de la donnée**.

| Fenêtre | Ce qui est vrai | Tests |
|---|---|---|
| **A — après chargement** | Assets et structure de scène. Immuables ensuite. | 1, 2 |
| **B — fin de frame, avant le rendu** | États dérivés : `m_local`, `m_worldMatrix`, `ViewData`. Reconstruits chaque frame. | 3, 4 |
| **C — dans le système** | Préconditions locales au point d'écriture. | assertions inline |

> **Un test s'appelle à l'instant où son invariant devient vrai, et il se répète tant que quelque chose peut le rendre faux. Pas là où c'est commode.**

Un `MeshHandle` chargé une fois reste valide : le tester une fois suffit. Une `m_worldMatrix` est réécrite 60 fois par seconde : la tester une fois ne prouve rien sur les 59 autres.

### 8.2 Test 1 — l'asset

```cpp
void Test_GizmoAssetLoads(ResourceManager& rm, const GizmoAssets& a)
{
    const MeshClass* p = rm.GetMesh(a.m_perspective);
    const MeshClass* o = rm.GetMesh(a.m_orthographic);
    LV3_ASSERT(p && o);
    LV3_ASSERT(p->vertsPerFace == 3 && p->faceCount() == kGizmoFacesPersp);   // 8
    LV3_ASSERT(o->vertsPerFace == 3 && o->faceCount() == kGizmoFacesOrtho);   // 14
}
```

### 8.3 Test 2 — le comptage

```cpp
void Test_GizmoCountMatchesCameras(Registry& registry)
{
    size_t expected = 0;
    for (auto&& [e, cam] : registry.ViewGroup<CameraComponent>())
        if (cam.m_gizmoLength > 0.0f) ++expected;

    size_t actual = 0;
    for (auto&& [e, giz] : registry.ViewGroup<CameraGizmoComponent>())
    {
        ++actual;
        LV3_ASSERT(registry.hasComponent<CameraComponent>(giz.m_owner));  // pas d'orphelin
        LV3_ASSERT(registry.hasComponent<DebugVisualComponent>(e));
    }
    LV3_ASSERT(actual == expected);
}
```

L'assertion sur `giz.m_owner` attraperait la régression du piège `ParseHierarchy` : si le spawn repassait avant, le lien casserait.

### 8.4 Test 3 — l'invariant rigide

Le filet de sécurité du §4.4. À appeler **chaque frame**, dans l'esprit de `CheckControllerExclusivity` :

```cpp
void Test_CameraWorldMatrixIsRigid(Registry& registry)
{
    constexpr float kEps = 1e-4f;
    for (auto&& [e, cam, tr] : registry.ViewGroup<CameraComponent, TransformComponent>())
    {
        const Matrix44f& w = tr.m_worldMatrix;
        const Vec3f X{ w[0][0], w[0][1], w[0][2] };
        const Vec3f Y{ w[1][0], w[1][1], w[1][2] };
        const Vec3f Z{ w[2][0], w[2][1], w[2][2] };

        LV3_ASSERT(std::fabs(V3Len(X) - 1.0f) < kEps);   // normes unitaires
        LV3_ASSERT(std::fabs(V3Len(Y) - 1.0f) < kEps);
        LV3_ASSERT(std::fabs(V3Len(Z) - 1.0f) < kEps);

        LV3_ASSERT(std::fabs(V3Dot(X, Y)) < kEps);       // orthogonalite :
        LV3_ASSERT(std::fabs(V3Dot(X, Z)) < kEps);       // sinon inverseRigid()
        LV3_ASSERT(std::fabs(V3Dot(Y, Z)) < kEps);       // ment sans planter
    }
}
```

### 8.5 Test 4 — le seul qui prouve la sémantique

Les trois précédents vérifient de la plomberie. Celui-ci vérifie que **le gizmo est réellement le frustum de sa caméra** : les 4 coins lointains du mesh, passés en monde puis projetés par le `viewProjection` de sa caméra, doivent atterrir exactement sur les coins du NDC.

```cpp
void Test_GizmoMatchesFrustum(Registry& registry, ResourceManager& rm,
                              const ViewData* views, size_t count,
                              const GizmoAssets& assets)
{
    constexpr float kEps = 1e-4f;
    static const Vec3f kCorners[4] = { {-1,-1,-1}, {1,-1,-1}, {1,1,-1}, {-1,1,-1} };

    size_t checked = 0;

    for (auto&& [e, giz, trGiz, mcGiz] :
         registry.ViewGroup<CameraGizmoComponent, TransformComponent, MeshComponent>())
    {
        const auto& cam     = registry.getComponent<CameraComponent>(giz.m_owner);
        const bool  isOrtho = (cam.m_projection == EProjectionType::Orthographic);

        // ── NIVEAU ENTITE ────────────────────────────────────────────
        // 1. Le handle designe le bon asset.
        LV3_ASSERT(mcGiz.m_meshHandle.id == assets.For(cam.m_projection).id);

        // 2. L'asset contient bien la geometrie attendue.
        //    Un handle correct ne prouve RIEN sur le contenu du fichier.
        const MeshClass* mg = rm.GetMesh(mcGiz.m_meshHandle);
        LV3_ASSERT(mg && mg->vertsPerFace == 3);
        LV3_ASSERT(mg->faceCount() == (isOrtho ? kGizmoFacesOrtho : kGizmoFacesPersp));

        const float expectedW = isOrtho ? 1.0f : giz.m_length;

        const ViewData* vd = nullptr;
        for (size_t i = 0; i < count; ++i)
            if (views[i].sourceCamera == giz.m_owner) { vd = &views[i]; break; }

        if (!vd) continue;   // camera non rendue cette frame : rien a comparer

        // ── NIVEAU COIN ──────────────────────────────────────────────
        for (const Vec3f& c : kCorners)
        {
            // local -> MONDE. Matrice AFFINE : w vaut 1 par construction.
            const Vec4f w4 = MulRow(trGiz.m_worldMatrix, c);
            LV3_ASSERT(std::fabs(w4.w - 1.0f) < kEps);

            const Vec3f world{ w4.x, w4.y, w4.z };   // legitime PARCE QUE w == 1
            const Vec4f clip = MulRow(vd->viewProjectionMatrix, world);

            LV3_ASSERT(std::fabs(clip.w - expectedW) < kEps);   // L, ou 1 en ortho
            LV3_ASSERT(std::fabs(std::fabs(clip.x / clip.w) - 1.0f) < kEps);
            LV3_ASSERT(std::fabs(std::fabs(clip.y / clip.w) - 1.0f) < kEps);
        }
        ++checked;
    }

    LV3_ASSERT(checked > 0);   // un test qui n'a rien verifie n'est pas vert
}
```

Ce test attrape **six bugs distincts** : mauvais fov, mauvais aspect, signe de Z inversé, scale posé sur le nœud caméra, ordre de composition erroné, asset ne correspondant pas à la projection. **Aucun ne se voit à l'écran.**

Trois détails de conception :

**`ndcZ` n'est pas testé.** Le quad du gizmo est à `m_gizmoLength`, pas au far plane — c'est voulu.

**`clip.w` est comparé à une valeur attendue, pas à `> 0`.** La matrice orthographique ne divise pas : son `w` reste à 1 quelle que soit la distance. Là où `clipW = L` prouve la perspective, `clipW = 1` prouve l'ortho. La version molle `> 0` avait laissé passer le bug 30.

**`LV3_ASSERT(checked > 0)` n'est pas décoratif.** La boucle commence par un `continue` conditionnel : si le rapprochement par `sourceCamera` échouait, le corps ne s'exécuterait jamais et le test serait **vert sans avoir rien testé**.

> **Tout test dont la boucle peut être vide doit compter ce qu'il a vérifié et asserter que ce compte n'est pas nul.** Un test vert qui n'a rien exécuté est pire qu'un test absent : il consomme la confiance sans rien garantir.

### 8.6 Le test de non-régression de la dette 1

Avec deux viewports identiques (768×800), le bug de l'aspect unique était **strictement invisible**. Le protocole :

1. **Produire le défaut.** Remettre `const float aspect = 1536.0f / 800.0f;` dans le système, layout `MainSide` (1152×800 et 384×800, aspects 1,44 et 0,48). Le test doit **sauter**, avec `ndcX` faux d'un facteur 1,33 sur une vue et 4,0 sur l'autre.
2. **Corriger.** Rétablir `b->viewport.Aspect()`. `ndcX = ndcY = ±1.000000` sur les deux.

C'est le **principe méthodologique n°4** : pour valider une correction, il faut pouvoir produire le défaut à volonté. Un test qui n'a jamais échoué ne prouve rien.

### 8.7 Ce que le test ne couvre pas

Le **roll**. Les quatre coins occupent les mêmes positions après une rotation de 180° autour de l'axe de visée, seulement permutés, et `fabs()` écrase les signes. Le marqueur « up » couvre le cas à l'œil.

Tester le roll exigerait de comparer les positions **signées**, donc de connaître l'ordre canonique des sommets dans le mesh chargé — hypothèse jamais vérifiée sur `OBJLoader`. Dette mineure.

---

## 9. Journal des bugs

| # | Bug | Détecté par | Cause | Statut |
|---|---|---|---|---|
| **28** | `tan(fov)` au lieu de `tan(fov/2)` | `Test_GizmoMatchesFrustum` : `ndcY = 2.414213 = 1/tan(22.5°)` | demi-angle omis | corrigé |
| **29** | Aspect ratio jamais appliqué au gizmo | même test : `ndcX/ndcY = 0.8 = 1/1.25` | pas de source unique pour l'aspect | corrigé |
| **30** | Scale du gizmo absent de `m_worldMatrix` | même test : `clipW = 1` au lieu de `3` | `CameraGizmoSystem` écrivait la source **après** sa cuisson par `LocalTransformSystem` | corrigé |
| **31** | Aspect unique pour N viewports | layout `MainSide` (viewports asymétriques) | l'aspect appartient au couple (caméra, viewport), pas à un `float` global | corrigé |

### 9.1 Ce que le diagnostic a enseigné

Les nombres ne disaient pas « c'est faux », ils donnaient **le facteur d'erreur exact**. Deux identités permettent de séparer les causes :

```
ndcY        =  tanHalf_gizmo / tanHalf_proj      <- ne depend QUE du fov
ndcX / ndcY =  aspect_gizmo  / aspect_proj       <- ne depend QUE de l'aspect
```

Table de décision :

| Observation | Conclusion |
|---|---|
| `ndcY = 1`, `ndcX ≠ 1` | fov correct, seul l'aspect est faux — `ndcX` en donne le rapport |
| `ndcY ≠ 1`, `ndcX/ndcY = 1` | aspect correct, seul le fov est faux |
| `ndcY ≠ 1`, `ndcX/ndcY ≠ 1` | les deux |
| `ndcX = ndcY = 0` | scale nul |
| `clipW = 1` en perspective | le scale n'atteint pas la matrice monde |

Le bug 28 s'est identifié par la constante `2.414213 = 1 + √2 = 1/tan(22.5°)`, qui confirmait à la fois l'omission du demi-angle **et** que les caméras étaient à `fov = 45°`.

### 9.2 Les bugs 30 et 31 sont symétriques

Le bug 30 s'est manifesté sous deux formes opposées selon la position du système :

```
AVANT LocalTransform  ->  scale ecrit, puis ECRASE       ->  scale = (1,1,1)
APRES LocalTransform  ->  scale ecrit, mais JAMAIS CUIT  ->  monde = identite
```

> **Principe méthodologique n°11.** Quand deux positions opposées dans l'ordonnancement échouent toutes les deux, la cause n'est pas l'ordre : c'est la donnée écrite — ou l'étape qui la consomme.

**Diagnostic final :** `m_local` **est** la source (`Transform` : position, `Quatf`, scale) ; `m_localMatrix` et `m_worldMatrix` sont les dérivés. Écrire `m_local.scale` était correct. Le défaut était l'**ordre** : une source doit être écrite **avant** l'étape qui la cuit.

### 9.3 Deux incidents de déploiement

Deux fois dans la même session, le fichier `.obj` du répertoire d'exécution n'était pas celui édité dans le projet — d'abord une boîte qui s'affichait en pyramide, puis un `faceCount()` obsolète.

**Remède structurel :** propriété *Copier si plus récent* sur les assets, ou étape post-build synchronisant `assets/`. Plus un log au chargement, pour que l'incident soit détectable au démarrage plutôt que trois systèmes plus loin.

---

## 10. Règles dictatoriales dégagées

> **R1 — Source et dérivé.** Une source est toujours écrite **avant** l'étape qui la cuit. Écrire après produit un dérivé jamais recalculé ; écrire dans le dérivé lui-même produit un écrasement. Aucune des deux ne génère d'erreur.

> **R2 — Objets de debug.** Un gizmo dérivé de paramètres doit être validé **numériquement** contre ces paramètres. Un objet de debug qui ment transforme l'outil de diagnostic en source d'erreur.

> **R3 — Données de paire.** Une donnée qui dépend d'une paire ne peut pas être stockée sur l'un des deux membres. Le moteur fonctionne tant qu'il n'y a qu'un observateur, puis casse silencieusement.

> **R4 — Signatures honnêtes.** La signature d'une fonction déclare ce dont elle dépend. Un paramètre inutilisé est un mensonge sur ce contrat. Passer le conteneur plutôt que la valeur crée une dépendance à un cycle de vie inutile.

> **R5 — Lecture des clés JSON.** Une clé n'est jamais lue par `contains()` + `operator[]` + `.value()`, mais uniquement par `Read(nom)`. Le nom est la seule chose que le lecteur peut enregistrer.

> **R6 — Discriminants.** Un choix de branche a un seul discriminant, explicite et nommé. La **présence** d'une clé n'est jamais un discriminant : elle rend le format illisible et interdit d'exprimer le défaut.

> **R7 — Garde-fous.** Un garde-fou corrige **et** parle. Corriger sans dire transforme une erreur d'auteur en comportement inexplicable.

> **R8 — `switch` sur `enum class`.** Pas de `default`. Le `default` échange une erreur de compilation contre un bug silencieux.

> **R9 — Sentinelles numériques.** Une sentinelle dans un champ numérique est un booléen déguisé qui **se propage dans l'arithmétique**. `1e30f` est le pire cas : représentable, donc validée par `isfinite()`, mais elle déborde vers `inf` au premier carré. `infinity()` est le moindre mal — comparaisons correctes, détectable par `isinf()` — mais `inf − inf` et `0 × inf` produisent `NaN`, et une normalisation `(z − near)/(far − near)` avec `far = inf` rend **0 partout, sans NaN et sans symptôme**. Seul le booléen explicite force le consommateur à traiter le cas.

> **R10 — État initial.** Une entité est créée dans un état déjà cohérent. Un système entretient un invariant, il ne le fabrique pas.

> **R11 — Drapeau `dirty`.** C'est un contrat entre l'écrivain et le cuiseur. Écrire dans la source sans lever le drapeau introduit un bug **différé** : correct à la première frame, faux ensuite.

> **R12 — Objets stateful partagés.** Chaque consommateur pose son état complet en entrée. Ce qui n'est pas affecté est hérité, et l'héritage silencieux d'état est indiagnosticable.

> **R13 — Postconditions.** Une postcondition s'asserte dans la fonction qui produit le résultat, pas dans un test qui le consulte plus tard.

> **R14 — Grandeurs structurelles.** Une grandeur structurelle se dérive de la donnée structurelle, jamais d'un attribut optionnel. Sinon l'absence de l'attribut se traduit par une structure vide plutôt que par une absence d'attribut. (`faceCount()` = `indices.size() / vertsPerFace`, pas `faceNormals.size()`.)

> **R15 — Identifiants et contenus.** Une assertion sur un identifiant ne prouve rien sur la donnée qu'il désigne. Vérifier le handle, c'est vérifier l'étiquette du bocal ; il faut aussi ouvrir le bocal.

> **R16 — Tests possiblement vides.** Tout test dont la boucle peut ne rien parcourir doit compter ce qu'il a vérifié et asserter que ce compte n'est pas nul.

> **R17 — Méthodes orphelines.** Une méthode publique sans appelant est une promesse d'API que personne ne tient. Elle survit aux refontes, diverge, et finit par être appelée par quelqu'un qui la croit maintenue.

> **R18 — Données non consommées.** Une donnée non consommée aujourd'hui se vérifie quand même. Son inexactitude ne produit aucun symptôme jusqu'au jour où un consommateur la lit — et le bug est alors attribué au consommateur.

> **R19 — Événements et ressources.** Un gestionnaire d'événement ne modifie jamais une ressource en cours d'utilisation. Il lève un drapeau ; le point stable de la boucle applique le changement.

> **R20 — Valeurs d'affichage debug.** Une valeur d'affichage debug ne se dérive pas d'une valeur géométrique. Elle est choisie pour la lisibilité de la sortie, pas pour la justesse du volume — les lier revient à faire dépendre l'outil de mesure de ce qu'il mesure. Deux grandeurs partageant une unité ne sont pas la même grandeur. (`depthDisplayRange` ≠ `farPlane`.)

### Principes méthodologiques ajoutés

> **11.** Quand deux positions opposées dans l'ordonnancement échouent toutes les deux, la cause n'est pas l'ordre : c'est la donnée écrite, ou l'étape qui la consomme.

> **12.** Une dépendance circulaire entre systèmes est presque toujours le symptôme d'une donnée trop grosse. Séparer ce qui est statique de ce qui est dérivé, et le cycle disparaît.

---

## 11. Dettes

### 11.1 Corrigées dans cette annexe

| Dette | Résolution |
|---|---|
| **D1 — Association (caméra, viewport)** inexistante comme donnée | `CameraBinding` + `BuildLayout` / `BuildCameraBindings`. Point de décision unique pour toute la configuration d'affichage. |
| **D3 — `SpawnCameraGizmos` logée dans `SceneSerializer`** par simple accès à `LinkChildToParent` | `LinkChildToParent` extraite en API publique ; toute la gestion gizmo dans `Scene/DebugGizmos.{hpp,cpp}`. |

### 11.2 Reformulée et reportée

**D2 — Transformations dépendantes de la vue.** Formulation initiale : « `gizmo.length` en unités monde devrait donner une taille constante à l'écran ».

Reformulation : une taille écran-constante exige `L = distance(observateur, gizmo) · k`. Mais l'**observateur** est la caméra qui *regarde* le gizmo, pas celle qu'il décrit — et un même gizmo peut être vu par plusieurs viewports simultanément, à des distances différentes.

`L` devient donc un attribut de la **paire (gizmo, vue observatrice)**. Il ne peut plus vivre dans `m_local.scale`, puisqu'une entité n'a qu'un transform. C'est le cas général de la règle R3.

Trois issues, toutes structurantes :

1. Une entité gizmo par vue observatrice — N×M entités. Lourd.
2. Un scale appliqué **au moment du rendu**, hors du transform — introduit la notion de « transform par vue ».
3. Renoncer, et garder `L` en unités monde.

La deuxième est la porte d'entrée du **billboarding**, des **LOD** et du **sprite scaling**. Ce n'est pas une correction : **c'est une leçon entière.**

### 11.3 Autres dettes touchées

| Dette | État |
|---|---|
| **Bug 26** — clés JSON écrites jamais lues (`active`, `priority`) | **fermé** par le `ParseCamera` refondu |
| **Bug 27** — `SDL_LockTexture` « Invalid call » au resize | forme du remède identifiée (drapeau différé + dimensions figées), non implémentée |
| `faceCount()` dérivé de `faceNormals.size()` | **fermé** — dérive de `indices.size() / vertsPerFace` (voir §13.1) |
| `ViewData::farPlane = 1e30f` (sentinelle numérique) | **fermé** — `hasFarPlane` + `farPlane` (voir §13.2) |
| `depthDisplayRange` en dur dans la boucle | à porter dans `engine.json` / `EngineConfig` (voir §13.3) |
| `depthDisplayRange` posé hors de la boucle de vues | fuite d'état R12 ; rejoindra `CameraBinding` quand deux vues en auront des valeurs différentes |
| `ClipVertex` sans `uv` / `normal` | **bloque le Phong** sur la vue principale |
| `MeshClass::GetFaceView` suppose les sommets contigus | **bloque le Phong** |
| Roll non testé par le test 4 | dette mineure ; suppose de connaître l'ordre canonique des sommets après `OBJLoader` |
| Nommage `m_` incohérent dans `ViewData` | **fermé** — préfixe supprimé sur les structures de passage, conservé sur les composants |

---

## 12. Synthèse

Ce que le chantier a produit, au-delà des gizmos :

- **2 assets canoniques**, un seul mesh partagé par toutes les caméras de même type — la forme réelle sort entièrement de la matrice de scale.
- **2 composants génériques** (`CameraGizmoComponent`, `DebugVisualComponent`) qui laissent `RenderView` ignorer ce qu'est une caméra.
- **1 structure** — `CameraBinding` — devenue le point de décision unique de la configuration d'affichage. Un split à quatre, une minimap, un picture-in-picture : une ligne dans `BuildLayout`, rien ailleurs.
- **1 ordonnancement sans retard de frame**, obtenu en dissolvant un faux cycle.
- **4 tests**, dont deux ont réellement trouvé quelque chose.

Et l'enseignement central : le gizmo « fonctionnait bien et à priori bien orienté » dès le premier lancement, **avec quatre bugs dedans**. Il était 2,4 fois trop ouvert et au mauvais ratio. Rien ne clochait visuellement, parce qu'aucun repère externe ne dit à l'œil quelle *devrait* être l'ouverture d'un frustum.

C'est la démonstration la plus nette du **principe méthodologique n°1** produite depuis la Leçon 04 :

> **« Ça s'affiche » n'est jamais un critère de validation.**
> Un cube rendu à l'envers ressemble à un cube. Un frustum au mauvais fov ressemble à un frustum.

---

## 13. Correctifs collatéraux

Deux défauts sans rapport direct avec les gizmos, mis au jour par les règles R14 et R9 et corrigés dans la foulée.

### 13.1 `MeshClass::faceCount()` — topologie contre attribut

**Avant :**

```cpp
[[nodiscard]] size_t faceCount() const noexcept { return faceNormals.size(); }
```

`faceNormals` est un attribut **optionnel** : absent si l'OBJ ne fournit aucune normale et que `generateNormalsIfMissing` est désactivée. Un mesh parfaitement valide rendait alors `faceCount() == 0`, et le garde-fou de `RenderView`

```cpp
if (!mesh || mesh->faceCount() == 0) continue;
```

le rejetait **silencieusement**.

**Après :**

```cpp
[[nodiscard]] size_t faceCount() const noexcept
{
    if (indices.empty()) return 0;   // mesh vide : 0 face, quel que soit vertsPerFace
    LV3_ASSERT(vertsPerFace > 0);
    LV3_ASSERT(indices.size() % vertsPerFace == 0);
    return indices.size() / vertsPerFace;
}
```

La sortie anticipée sur `indices.empty()` est nécessaire : un `MeshClass` par défaut peut avoir `vertsPerFace == 0`, et le premier `LV3_ASSERT` transformerait alors un skip gracieux en crash. Un mesh sans indices a zéro face indépendamment de `vertsPerFace` — l'assertion n'a pas lieu de s'y appliquer.

**Le risque de régression, à traiter.** Avant, `faceCount()` **était** `faceNormals.size()` : toute boucle de la forme

```cpp
for (size_t f = 0; f < mesh->faceCount(); ++f)
    ... mesh->faceNormals[f] ...
```

restait dans les bornes **par construction**. Ce n'est plus vrai. Si `faceNormals` est plus court que `faceCount()`, on sort du tableau — en Release, sans message.

Deux actions :

1. Recherche globale de `faceNormals[`, `faceSmoothingGroups[` et de tout tableau indexé par face.
2. Asserter l'invariant après chargement — c'est précisément ce que le changement rend **vérifiable**, alors qu'il était tautologique avant :

```cpp
// Un tableau par face est soit absent, soit complet. Jamais partiel.
LV3_ASSERT(faceNormals.empty()         || faceNormals.size()         == faceCount());
LV3_ASSERT(faceSmoothingGroups.empty() || faceSmoothingGroups.size() == faceCount());
```

Bénéfice réel : la topologie est désormais séparée de ses attributs, donc une incohérence entre les deux devient **détectable**. Elle ne l'était pas quand l'un définissait l'autre.

### 13.2 `ViewData::farPlane` — la sentinelle remplacée

**Avant :**

```cpp
v.farPlane = cam.m_infiniteFar ? 1e30f : cam.m_farPlane;
```

`1e30f` est représentable en float, donc validée par `isfinite()`, mais tout calcul qui la met au carré ou la multiplie déborde vers `inf`. Un `NaN` dans le depth buffer contamine ensuite toutes les comparaisons `GREATER`, qui rendent `false` quel que soit l'opérande.

**Étape intermédiaire écartée :** `std::numeric_limits<float>::infinity()`. Meilleur — comparaisons correctes, détectable par `isinf()` — mais `inf − inf` et `0 × inf` produisent `NaN`, et une normalisation `(z − near)/(far − near)` avec `far = inf` rendrait **0 partout**, sans `NaN`, donc sans qu'aucune assertion se déclenche. Le bug serait cherché dans le shader.

**Après :**

```cpp
// ViewData.h
// hasFarPlane == false : le volume de vue est infini (PerspectiveInfinite).
// Tout calcul necessitant un far borne doit alors substituer une valeur
// d'affichage explicite -- jamais lire farPlane.
bool  hasFarPlane = true;
float farPlane    = 1000.0f;
```

```cpp
// BuildViewData, etape 0
v.nearPlane   = cam.m_nearPlane;
v.hasFarPlane = !cam.m_infiniteFar;
v.farPlane    = cam.m_farPlane;      // affecte INCONDITIONNELLEMENT
```

`farPlane` reçoit la valeur d'auteur même en `infiniteFar` : c'est `hasFarPlane` qui dit si elle a un sens. Un champ laissé non affecté serait une seconde forme de sentinelle.

**Le moment était bien choisi.** Aucun site ne lisait `farPlane` — pas même `SetDepthDisplayRange`. Zéro adaptation, zéro régression possible. Avec cinq consommateurs, le même changement serait devenu un chantier.

**Ce que le booléen débloque.** Tout futur consommateur devra traiter les deux cas explicitement, là où une sentinelle l'aurait laissé calculer sans le savoir :

```cpp
if (view.hasFarPlane) { /* volume borne : le calcul a un sens */ }
else                  { /* volume infini : substituer, ou ne rien faire */ }
```

Le booléen ne se contente pas d'éviter un bug : il rend **exprimable une branche qui ne l'était pas**.

### 13.3 Le contre-exemple : `depthDisplayRange` ne se dérive PAS de `farPlane`

Le réflexe naturel, une fois `hasFarPlane` en place, serait d'écrire :

```cpp
const float range = view.hasFarPlane ? view.farPlane : kDebugDepthRange;   // FAUX
```

**C'est un contresens.** `depthDisplayRange` n'est pas une grandeur géométrique : c'est un paramètre de **lisibilité**. Le mode `LinearDepth` de la L04 P2 applique un `fmod` pour produire des bandes, et c'est la discontinuité qui rend visible une erreur continue (principe méthodologique n°2). Avec `farPlane = 2000`, on obtiendrait **une seule bande** sur toute la scène — aucune information.

La valeur 80 n'a pas été choisie parce qu'elle décrit le volume de vue, mais parce qu'à cette échelle les bandes sont comptables à l'œil.

> **Une valeur d'affichage debug ne se dérive pas d'une valeur géométrique.** Elle est choisie pour la lisibilité de la sortie, pas pour la justesse du volume. Les lier revient à faire dépendre l'outil de mesure de ce qu'il mesure.

Deux grandeurs qui partagent une unité — ici les unités monde — ne sont pas pour autant la même grandeur.

**Ce qui doit réellement changer :**

1. **Le `80` en dur sort du code.** Règle de la L01 : les valeurs ajustables vivent dans `engine.json` / `EngineConfig`, pour ne pas recompiler à chaque essai.

```json
"debug": { "depthDisplayRange": 80.0 }
```

2. **La fuite d'état, à surveiller.** `SetDepthDisplayRange` est appelée une fois, hors de la boucle de vues, sur un `Renderer` stateful — donc la valeur s'applique à toutes les vues. C'est la règle **R12**. Sans conséquence aujourd'hui (une seule vue peut être en mode `Depth`), mais le paramètre est de la même famille que `mode` et devra rejoindre `CameraBinding` le jour où deux vues en auront besoin de valeurs différentes.

Le critère de réification reste le même que pour `std::expected` : **on réifie une association quand deux consommateurs peuvent en avoir des valeurs différentes.** Ce n'est pas encore le cas.

3. **L'ordre d'appel.** `SetDepthDisplayRange` écrit dans `m_ctx`. Si `BeginFrame` réinitialise `m_ctx`, un appel placé avant serait effacé — et le mode Depth afficherait une plage par défaut sans le moindre signal. Même motif que le bug 30 : un état écrit avant l'étape qui le réinitialise.

---

*Annexe A5 — clôturée. Prochaine étape : Leçon 06 (Scenegraph), qui reprendra le contrat de `HierarchyComponent`, la cascade de `DestroyEntity` et la composition du scale non uniforme.*
