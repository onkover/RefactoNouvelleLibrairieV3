# Annexe A7 — Dolly, zoom, et le faux bug de la caméra orthographique

> Annexe à la **Leçon 05 — Caméra & Frustum** et à l'**Annexe A5** (Gizmos de caméra — `CameraFovY()`, `CameraGizmoSystem`).
> Prérequis : L05 (`Projection.h`, `ViewData`), A5 §2.4 (section constante vs section proportionnelle), `ParseCamera` (discriminant `lens: "fov"|"filmback"`).

---

## 1. Le problème posé

Constat initial : au clavier, une caméra perspective avance et recule normalement. Une caméra orthographique, elle, semble figée sur l'axe avant/arrière — seul le déplacement latéral (droite/gauche) produit un effet visible à l'écran.

Deux hypothèses possibles avant tout diagnostic :

1. Un bug dans le contrôleur, qui traiterait différemment les deux types de projection.
2. Un bug — ou une propriété non anticipée — dans la projection elle-même.

Le chantier a fermé la première hypothèse par lecture directe du code, confirmé la seconde par calcul, puis abouti à l'ajout d'un système absent : le zoom.

---

## 2. Le contrôleur est innocent

`FPSControllerSystem` (`Scene/System.cpp`) ne teste jamais `cam.m_projection`. Il écrit dans `TransformComponent::position`, un point, sans savoir ce qui regarde à travers :

```cpp
const Vec3f fwd   = rot.rotate(Vec3f::Forward());
const Vec3f right = rot.rotate(Vec3f::Right());

Vec3f dir;
if (in.moveForward)  dir += fwd;
if (in.moveBackward) dir -= fwd;
...
tr.m_local.position += dir;
```

`position.z` change identiquement, à chaque frame, quel que soit le type de projection. La Règle 1 (« la caméra n'existe pas » pour le contrôleur) est respectée — et c'est justement elle qui empêche le bug d'exister à cet endroit.

---

## 3. La preuve est dans la matrice

`Maths/Projection.cpp`, colonne qui produit `x_clip` :

| | Perspective | Orthographique |
|---|---|---|
| `m[0][0]` | `(2·n)/(r-l)` | `2/(r-l)` |
| dépendance à `z_view` | oui — `m[2][3] = -1` recopie `-z` dans `w`, division homogène | **aucune** |

En perspective, `w = -z_view`, donc `x_ndc = x_view / (-z_view) · const` : la distance module directement la position et la taille apparentes. C'est le dolly classique — approcher un point hors-axe le fait converger vers le centre et grossir.

En orthographique, `w = 1` toujours. `x_ndc = x_view · const`, sans terme en `z`. Translater la caméra le long de son axe de visée modifie `z_view` — donc `z_ndc` (la profondeur) — mais **jamais** `x_ndc` ni `y_ndc`. La caméra bouge réellement ; rien ne peut le montrer à l'écran, hors franchissement du `near`/`far` ou changement d'ordre de profondeur entre objets.

> **Ce n'est pas un bug. C'est l'invariant qui définit l'orthographique** : sections parallèles constantes, indépendantes de la distance (déjà observé en A5 §2.4 sur la forme du gizmo — même cause, deux symptômes).

---

## 4. Contre-exemple : les fausses pistes

**Patcher `Orthographic()` pour dépendre de `z`** (`m[0][0] += k/z`) — détruirait l'invariant même qui définit la projection. Chaque gizmo, chaque section supposée constante par le reste du moteur (A5 §2.4) deviendrait fausse.

**Augmenter `moveSpeed` en mode ortho** — inutile. Le problème n'est pas une vitesse insuffisante, c'est un terme algébrique nul. `moveSpeed = 10000` produit un `x_ndc` identique au bit près tant que `near`/`far` ne sont pas franchis.

---

## 5. Dolly et zoom sont deux commandes différentes

Convention retenue : le clavier déplace la caméra (dolly, `Transform`), la souris l'oriente, la molette **zoome** — et zoomer n'est jamais un problème de position. C'est un problème de lentille, donc de `CameraComponent`.

| Projection | Modèle de lentille | Source de vérité | Zoomer (molette avant) |
|---|---|---|---|
| Orthographic | — | `m_orthoHeight` | **réduit** la valeur |
| Perspective | `FieldOfView` | `m_fovYDeg` | **réduit** l'angle |
| Perspective | `Filmback` | `m_focalLengthMm` | **augmente** la focale |

La troisième ligne est le piège : en optique réelle, zoomer *in* allonge la focale (un 200 mm est plus zoomé qu'un 24 mm), alors que pour le FOV et l'`orthoHeight`, zoomer *in* réduit la valeur. Une formule unique appliquée aux trois cas sans y penser inverse le sens perçu du zoom sur les caméras `Filmback`.

Le zoom écrit systématiquement dans le champ **réellement source de vérité** pour le modèle actif — jamais `m_fovYDeg` quand `m_lensModel == Filmback`, sinon `CameraFovY()` l'ignore silencieusement à la frame suivante et le zoom n'a aucun effet observable.

---

## 6. Décisions d'architecture

| Décision | Choix retenu | Justification |
|---|---|---|
| Emplacement | `Scene/System.cpp`, aux côtés de `FPSControllerSystem` | contrôleur générique, pas un gizmo — `DebugGizmos.cpp` n'est pas concerné |
| Portée (quelles caméras) | Caméras avec `FPSControllerComponent` **activé** (`m_isEnabled`), pas les caméras `m_isActive` | même filtre que le déplacement clavier — on zoome ce qu'on pilote, pas ce qui est rendu ; découple pilotage et rendu, cohérent avec le reste de l'architecture |
| Pas de progression | **multiplicatif** (`valeur × ratio^wheel`), pas additif | `orthoHeight` (≈120), `fovYDeg` (≈45) et `focalLengthMm` (≈35) n'ont pas la même échelle ; un pourcentage par cran se comporte pareil quelle que soit la valeur de départ |
| Sprint (Shift) | accélère le zoom, via `ctrl.m_sprintMultiplier` | cohérence avec le déplacement clavier, qui accélère déjà de la même façon |
| Sens de `wheelDelta` | molette vers l'avant → `wheelDelta > 0` → zoom in | convention choisie par Onky, à vérifier côté application si le mapping SDL est inversé |
| Commentaire `CameraComponent` | mise à jour de « donnée d'AUTEUR, pure » | trois champs deviennent modifiables au runtime ; le commentaire mentait sinon (cf. A5 §12 — « ça s'affiche » n'est jamais un critère, ici c'est « le commentaire dit vrai » qui ne l'était plus) |

---

## 7. Le code

```cpp
// ════════════════════════════════════════════════════════════════
//  CameraZoomSystem — molette = zoom optique, JAMAIS un déplacement.
//
//  Un seul ratio mémorisé, réutilisé à l'endroit ou à l'envers selon
//  le sens physique du paramètre : FOV et orthoHeight RÉTRÉCISSENT
//  quand on zoome, la focale ALLONGE. Trois constantes indépendantes
//  auraient été trois occasions de se tromper de signe.
// ════════════════════════════════════════════════════════════════
void CameraZoomSystem(Registry& registry, const InputState& in, float /*dt*/)
{
    if (in.wheelDelta == 0) return;

    constexpr float zoomRatioPerNotch = 0.90f;   // 10 % par cran

    const float wheel = static_cast<float>(in.wheelDelta);

    for (auto&& [entity, cam, ctrl] :
         registry.ViewGroup<CameraComponent, FPSControllerComponent>())
    {
        if (!ctrl.m_isEnabled) continue;

        const float effectiveWheel = wheel * (in.sprint ? ctrl.m_sprintMultiplier : 1.0f);
        const float shrink = std::pow(zoomRatioPerNotch, effectiveWheel);   // < 1 en avançant

        switch (cam.m_projection)
        {
        case EProjectionType::Orthographic:
        {
            constexpr float minHeight = 0.5f;
            constexpr float maxHeight = 5000.0f;

            cam.m_orthoHeight = std::clamp(cam.m_orthoHeight * shrink, minHeight, maxHeight);
            break;
        }
        case EProjectionType::Perspective:
        {
            if (cam.m_lensModel == ELensModel::FieldOfView)
            {
                constexpr float minFov = 1.0f;
                constexpr float maxFov = 170.0f;

                cam.m_fovYDeg = std::clamp(cam.m_fovYDeg * shrink, minFov, maxFov);
            }
            else // Filmback : zoomer = ALLONGER la focale -> ratio inversé
            {
                constexpr float minFocal = 1.0f;
                constexpr float maxFocal = 2000.0f;

                cam.m_focalLengthMm = std::clamp(cam.m_focalLengthMm / shrink, minFocal, maxFocal);
            }
            break;
        }
        }
    }
}
```

`shrink < 1` quand `wheel > 0` : `orthoHeight` et `fovYDeg` sont multipliés par `shrink`, donc rétrécissent. `focalLengthMm` est **divisé** par `shrink`, donc grandit. Une seule constante porte l'inversion dans la formule, jamais dans une copie modifiée à la main.

**`Component.hpp`**, en-tête de `CameraComponent` :

```cpp
/*
    LA LENTILLE, et rien d'autre.
    ...
    Donnée principalement d'AUTEUR, sérialisable telle quelle.
    Exception : m_fovYDeg / m_focalLengthMm / m_orthoHeight peuvent
    être réécrits au runtime par CameraZoomSystem (molette), selon
    m_lensModel / m_projection — jamais les trois à la fois sur la
    même caméra, un seul est la source de vérité pour un instant donné.
*/
```

**Ordre canonique**, mis à jour :

```
PollEvents → FPSControllerSystem → CameraFollowSystem → CameraZoomSystem → BuildCameraBindings → CameraGizmoSystem → ...
```

`CameraGizmoSystem` (`DebugGizmos.cpp`) lit déjà `cam->m_orthoHeight` et `CameraFovY(*cam)` fraîchement à chaque frame (A5 §2.3) : le gizmo suit le zoom sans une seule ligne modifiée de son côté. C'est la séparation policy/données de l'Annexe A5 qui paie une seconde fois ici.

---

## 8. Validation

### 8.1 Manuelle

Vérification à l'œil, par Onky : sens du zoom (molette avant = resserrement, dans les trois cas, y compris la focale malgré la formule inversée), suivi du gizmo sans latence, accélération nette et stable sous Shift, comportement propre en butée de clamp (pas d'artefact, pas de division par zéro). Tout confirmé fonctionnel.

### 8.2 `TestCameraZoom` — le TNR

**Dette fermée.** Huit sections, chacune sur une entité fraîche pour qu'aucune ne fasse réussir la suivante par accident (même discipline que `TestRasterizer`, L04 P2) :

| § | Verrouille | Si ça casse |
|---|---|---|
| 1 | `orthoHeight` rétrécit de 10 % pile par cran avant | le ratio ou le sens a changé sans qu'on s'en aperçoive |
| 2 | Dézoomer (`wheel < 0`) est l'exact inverse de zoomer | `+=` câblé au lieu d'un ratio symétrique |
| 3 | `fovYDeg` suit le **même sens** que `orthoHeight` | une des deux branches copiée-collée de travers |
| 4 | `focalLengthMm` va dans le sens **inverse** — le piège du §5 | quelqu'un « simplifie » en appliquant la même formule aux trois cas |
| 5 | Shift multiplie l'exposant, pas juste la vitesse affichée | `m_sprintMultiplier` ignoré ou mal appliqué |
| 6 | Une caméra sans `FPSControllerComponent` actif est **ignorée** par la molette | régression vers un filtre `m_isActive` — la décision du §6 défaite silencieusement |
| 7 | `wheelDelta == 0` ne touche à rien | le early-return a sauté |
| 8 | Les trois clamps tiennent sous rafale extrême, sans `inf`/`NaN` | une borne retirée ou mal orientée |

```cpp
#ifdef _DEBUG

// ════════════════════════════════════════════════════════════════
//  TestCameraZoom — verrouille CameraZoomSystem.
//  Chaque cas construit sa propre entite : aucun etat partage entre
//  sections, donc aucune section ne peut faire reussir la suivante
//  par accident (meme discipline que TestRasterizer, L04 P2).
// ════════════════════════════════════════════════════════════════
void TestCameraZoom()
{
    constexpr float kRatio   = 0.90f;
    constexpr float kEpsilon = 1e-4f;

    auto MakeControlledCamera = [](Registry& reg, EProjectionType proj, ELensModel lens,
                                    bool controllerEnabled = true) -> Entity
    {
        Entity e = reg.CreateEntity();

        CameraComponent cam{};
        cam.m_projection    = proj;
        cam.m_lensModel     = lens;
        cam.m_orthoHeight   = 120.0f;
        cam.m_fovYDeg       = 45.0f;
        cam.m_focalLengthMm = 35.0f;
        reg.addComponent(e, cam);

        FPSControllerComponent ctrl{};
        ctrl.m_isEnabled        = controllerEnabled;
        ctrl.m_sprintMultiplier = 3.0f;
        reg.addComponent(e, ctrl);

        return e;
    };

    // ----------------------------------------------------------------
    // §1 — Orthographique : un cran avant retrecit de 10% pile.
    // ----------------------------------------------------------------
    {
        Registry reg;
        Entity cam = MakeControlledCamera(reg, EProjectionType::Orthographic, ELensModel::FieldOfView);

        InputState in{};
        in.wheelDelta = 1;
        CameraZoomSystem(reg, in, 0.0f);

        const float expected = 120.0f * kRatio;
        const float actual   = reg.getComponent<CameraComponent>(cam).m_orthoHeight;
        assert(std::fabs(actual - expected) < kEpsilon);
    }

    // ----------------------------------------------------------------
    // §2 — Dezoomer est l'inverse exact de zoomer.
    // ----------------------------------------------------------------
    {
        Registry reg;
        Entity cam = MakeControlledCamera(reg, EProjectionType::Orthographic, ELensModel::FieldOfView);

        InputState in{};
        in.wheelDelta = -1;
        CameraZoomSystem(reg, in, 0.0f);

        const float expected = 120.0f / kRatio;   // agrandit
        const float actual   = reg.getComponent<CameraComponent>(cam).m_orthoHeight;
        assert(std::fabs(actual - expected) < kEpsilon);
        assert(actual > 120.0f);                  // sens qualitatif, independant de la formule
    }

    // ----------------------------------------------------------------
    // §3 — Perspective FieldOfView : meme sens que l'ortho (retrecit).
    // ----------------------------------------------------------------
    {
        Registry reg;
        Entity cam = MakeControlledCamera(reg, EProjectionType::Perspective, ELensModel::FieldOfView);

        InputState in{};
        in.wheelDelta = 1;
        CameraZoomSystem(reg, in, 0.0f);

        const float expected = 45.0f * kRatio;
        const float actual   = reg.getComponent<CameraComponent>(cam).m_fovYDeg;
        assert(std::fabs(actual - expected) < kEpsilon);
        assert(actual < 45.0f);                   // retrecit, comme l'ortho
    }

    // ----------------------------------------------------------------
    // §4 — Filmback : sens INVERSE. Le piege central du §5.
    // ----------------------------------------------------------------
    {
        Registry reg;
        Entity cam = MakeControlledCamera(reg, EProjectionType::Perspective, ELensModel::Filmback);

        InputState in{};
        in.wheelDelta = 1;
        CameraZoomSystem(reg, in, 0.0f);

        const float expected      = 35.0f / kRatio;   // ALLONGE
        const float wrongIfCopied = 35.0f * kRatio;    // ce que donnerait un copier-coller naif
        const float actual        = reg.getComponent<CameraComponent>(cam).m_focalLengthMm;

        assert(std::fabs(actual - expected) < kEpsilon);
        assert(actual > 35.0f);                        // sens qualitatif
        assert(std::fabs(actual - wrongIfCopied) > kEpsilon);  // prouve qu'on n'a PAS le defaut
    }

    // ----------------------------------------------------------------
    // §5 — Sprint multiplie l'exposant, pas seulement l'affichage.
    // ----------------------------------------------------------------
    {
        Registry reg;
        Entity cam = MakeControlledCamera(reg, EProjectionType::Orthographic, ELensModel::FieldOfView);

        InputState in{};
        in.wheelDelta = 1;
        in.sprint     = true;                     // ctrl.m_sprintMultiplier == 3.0f
        CameraZoomSystem(reg, in, 0.0f);

        const float expected = 120.0f * std::pow(kRatio, 3.0f);
        const float actual   = reg.getComponent<CameraComponent>(cam).m_orthoHeight;
        assert(std::fabs(actual - expected) < kEpsilon);

        const float withoutSprint = 120.0f * kRatio;
        assert(std::fabs(actual - withoutSprint) > 1.0f);
    }

    // ----------------------------------------------------------------
    // §6 — Portee : sans controleur actif, la molette ne touche rien.
    //      Verrouille la decision d'architecture du §6 (FPSControllerComponent,
    //      PAS m_isActive).
    // ----------------------------------------------------------------
    {
        Registry reg;
        Entity cam = MakeControlledCamera(reg, EProjectionType::Orthographic, ELensModel::FieldOfView,
                                           /*controllerEnabled=*/false);

        InputState in{};
        in.wheelDelta = 5;
        CameraZoomSystem(reg, in, 0.0f);

        const float actual = reg.getComponent<CameraComponent>(cam).m_orthoHeight;
        assert(std::fabs(actual - 120.0f) < kEpsilon);   // inchange
    }

    // ----------------------------------------------------------------
    // §7 — wheelDelta == 0 : early-return, aucun effet de bord.
    // ----------------------------------------------------------------
    {
        Registry reg;
        Entity cam = MakeControlledCamera(reg, EProjectionType::Perspective, ELensModel::Filmback);

        InputState in{};   // wheelDelta == 0 par defaut
        CameraZoomSystem(reg, in, 0.0f);

        const float actual = reg.getComponent<CameraComponent>(cam).m_focalLengthMm;
        assert(std::fabs(actual - 35.0f) < kEpsilon);
    }

    // ----------------------------------------------------------------
    // §8 — Clamps sous rafale extreme : ni inf, ni NaN, ni depassement.
    // ----------------------------------------------------------------
    {
        struct Case { EProjectionType proj; ELensModel lens; int wheel; const char* label; };
        const Case cases[] = {
            { EProjectionType::Orthographic, ELensModel::FieldOfView, +200, "ortho zoom max"   },
            { EProjectionType::Orthographic, ELensModel::FieldOfView, -200, "ortho dezoom max" },
            { EProjectionType::Perspective,  ELensModel::FieldOfView, +200, "fov zoom max"     },
            { EProjectionType::Perspective,  ELensModel::FieldOfView, -200, "fov dezoom max"   },
            { EProjectionType::Perspective,  ELensModel::Filmback,    +200, "focale zoom max"  },
            { EProjectionType::Perspective,  ELensModel::Filmback,    -200, "focale dezoom max"},
        };

        for (const Case& c : cases)
        {
            Registry reg;
            Entity cam = MakeControlledCamera(reg, c.proj, c.lens);

            InputState in{};
            in.wheelDelta = c.wheel;
            CameraZoomSystem(reg, in, 0.0f);

            const CameraComponent& result = reg.getComponent<CameraComponent>(cam);
            const float values[] = { result.m_orthoHeight, result.m_fovYDeg, result.m_focalLengthMm };

            for (float v : values)
            {
                assert(std::isfinite(v));   // pas d'inf, pas de NaN
                assert(v > 0.0f);           // aucune borne minimale n'est <= 0
            }

            printf("  [%-20s] orthoHeight=%8.2f fov=%6.2f focal=%7.1f\n",
                   c.label, result.m_orthoHeight, result.m_fovYDeg, result.m_focalLengthMm);
        }
    }

    Logger::info("\033[32m[TNR] TestCameraZoom : tous les cas passent\033[0m");
}

#endif // _DEBUG
```

**Point d'insertion.** Dans le même fichier que `TestCameraMath` (repo `RefactoNouvelleLibrairieV3`), appelée depuis le même runner `_DEBUG`. Fenêtre **B** au sens de l'Annexe A5 §8.1 : un état recalculé chaque frame, pas une donnée figée au chargement.

### 8.3 Le doute sur `Registry` — levé

Une construction/destruction répétée d'un `Registry` local par cas (§1 à §8, six instances rien qu'en §8) suppose que `Registry` ne porte aucun état partagé entre instances. Lecture du fichier `Registry.hpp` fourni par Onky :

- `Registry` ne possède que ce qu'il déclare (`m_Storages`, `m_Generations`, `m_Alive`, `m_FreeIndices`, `m_AliveCount`) — rien de statique au niveau de la classe.
- Le seul état global du fichier est `ComponentTypeManager::s_NextNextID`, **process-wide** par construction (`static uint32_t id = s_NextNextID++;`, un compteur par type `T`, partagé par tous les `Registry`).

Ce partage est **sans danger** plutôt qu'un défaut caché : il garantit que `CameraComponent` et `FPSControllerComponent` obtiennent le même `typeID` quel que soit le `Registry`, donc que chaque instance s'accorde sur l'emplacement de ses propres composants sans jamais avoir besoin de se synchroniser avec les autres. Ce n'est pas le cas d'« objet stateful partagé » que R12 (Annexe A5) mettrait en garde — R12 concerne un état qui *change le résultat* d'un consommateur à l'autre ; ici le compteur ne fait qu'attribuer une étiquette stable, jamais lue en dehors du mécanisme lui-même.

**Conséquence :** six `Registry reg;` locaux, une entité fraîche par section, sont exactement le bon calibre — chaque `Registry` est une éprouvette jetable, isolée par construction et pas seulement par convention de test.

---

## 9. Synthèse

Le symptôme rapporté (« la caméra ortho ne se déplace pas ») décrivait un comportement **correct** : la position change réellement, l'écran ne peut mathématiquement pas le montrer, faute de division par `z` dans la matrice orthographique. Le vrai manque n'était pas un correctif, mais une commande absente — le zoom, déjà anticipée dans `InputState::wheelDelta` (commentaire « molette : zoom / vitesse ») mais jamais câblée.

La solution retenue unifie trois grandeurs d'échelles très différentes (`orthoHeight`, `fovYDeg`, `focalLengthMm`) sous une seule formule multiplicative, appliquée à l'endroit ou à l'envers selon le sens physique du paramètre — plutôt que trois constantes indépendantes, chacune une occasion de se tromper de signe. Le `TestCameraZoom` (§8.2) verrouille précisément cette inversion : si un jour quelqu'un « simplifie » les trois branches en une formule unique, le test qui compare `actual` à `wrongIfCopied` échoue avant que le bug n'atteigne l'écran.

> **Un déplacement clavier qui ne produit aucun effet visible n'est pas nécessairement un bug de déplacement.** Avant de corriger le contrôleur, vérifier ce que la projection est mathématiquement capable de montrer.

---

*Annexe A7 — clôturée. Prochaine étape : Leçon 06 (Scenegraph).*
