# Annexe A6 — JsonReader générique & EngineConfig::LoadFromJson

> Annexe à la **Leçon 01** (Config, Enums, PCH — frontière compile-time/runtime, `engine.json`) et à l'**Annexe A5** (Gizmos de caméra — origine de `JsonReader` et de la règle R5).
> Prérequis : L01 (`Core/EngineSettings.h`, `Core/EngineConfig.h`), Annexe A5 §6 (`JsonReader`), Annexe A5 §13.3 (`depthDisplayRange`).

---

## 1. Le problème de départ

Deux constats, posés séparément, qui se sont révélés être la même faute d'architecture vue sous deux angles :

1. `engine.json` existait, avec un contenu à jour (`renderer`, `features`, `resources`, `debug`), mais `EngineConfig::LoadFromJson` n'était que déclarée — jamais implémentée, jamais appelée. Le fichier était défini, jamais employé.
2. `JsonReader` — le lecteur à clé tracée qui applique la règle R5 (Annexe A5 §6) — n'existait que comme classe privée à l'intérieur de `Scene/Serializer.cpp`. Un seul système (le scenegraph) pouvait en bénéficier ; tout autre lecteur de JSON du moteur (à commencer par `EngineConfig`) était condamné à réinventer soit le mécanisme, soit ses failles (lecture directe par `contains()` + `operator[]`, aucune détection de clé ignorée).

Le chantier a consisté à fermer les deux dettes ensemble : extraire `JsonReader` en composant partagé, puis l'utiliser pour donner enfin un corps à `LoadFromJson`.

---

## 2. Extraction de `JsonReader` — mécanisme générique vs politique de domaine

### 2.1 Le test de placement

Adapté du test déjà utilisé pour `SDL` (Leçon 05 §4) et pour `engine.json` (§4 de cette annexe) :

> **« Si je remplaçais `SceneSerializer` par un autre système consommant du JSON (config moteur, matériaux, bindings d'input...), ce fichier changerait-il ? »**
> Non → il n'a rien à faire dans `Scene/Serializer.cpp`.

`JsonReader` ne dépendait que de `nlohmann::json` et d'un `Logger`. Une seule fuite le liait à Scene : la méthode `ReadProjectionType()`, qui connaissait `EProjectionType` — un type Camera, mono-consommateur.

### 2.2 La règle du partage

- **Mécanisme générique** — `Read()`, `ReadVector()`, `Child()`, `Has()`, `WarnUnread()`. Devient commun.
- **Politique de domaine** — tout mapping `string → type métier` (`EProjectionType`, potentiellement `ELightType` demain). Reste dans le fichier du domaine, sous forme de fonction libre prenant un `JsonReader&`.

`ReadVector()` fait exception à la règle du "générique = scalaires seulement" : `Vec3f` est un type **Core** (`Maths/Vectorlib.h`), utilisé par au moins deux systèmes distincts (Transform, Camera, Light...) — exactement le critère de la Leçon 01 §7.2 pour `CoreTypes.h` ("liste close + multi-système"). Il reste dans le lecteur générique.

### 2.3 `Core/JsonReader.h` — extraction fidèle

Header-only, **hors `pch.h`** (dépend de `json.hpp`, un header lourd — même raisonnement que `Platform.h`), namespace `LV3` (le namespace courant, pas `LibV3` où vivait l'original) :

```cpp
#pragma once
// ============================================================
// Core/JsonReader.h — HORS pch.h
// Lecteur JSON a cle tracee (R5) — generique.
// Vec3f est une exception assumee : type Core (Maths), multi-systeme,
// meme critere que CoreTypes.h (Lecon 01, §7.2). Tout type de DOMAINE
// (EProjectionType, ELightType...) reste hors de ce fichier.
// ============================================================

#include <string>
#include <string_view>
#include <set>
#include "Core/Logger.h"
#include "Maths/Vectorlib.h"      // Vec3f, ReadVec3
#include "../Ressources/json.hpp"

namespace LV3
{
    class JsonReader
    {
    public:
        JsonReader(const nlohmann::json& j, std::string comp, std::string owner) noexcept
            : m_j(j), m_comp(std::move(comp)), m_owner(std::move(owner)) {}

        template<typename T>
        [[nodiscard]] T Read(const char* key, T def)
        {
            m_seen.insert(key);
            return m_j.value(key, def);
        }

        [[nodiscard]] Vec3f ReadVector(const char* key, const Vec3f& def)
        {
            m_seen.insert(key);
            return ReadVec3(m_j, key, def);   // deja dans LV3
        }

        // Descente dans un sous-objet. NON const : elle consomme une cle.
        [[nodiscard]] JsonReader Child(const char* key)
        {
            m_seen.insert(key);
            static const nlohmann::json s_empty = nlohmann::json::object();
            const auto it = m_j.find(key);
            const nlohmann::json& sub = (it != m_j.end() && it->is_object()) ? *it : s_empty;
            return JsonReader(sub, m_comp + "." + key, m_owner);
        }

        [[nodiscard]] bool Has(std::string_view key) const { return m_j.contains(key); }

        // A appeler en DERNIER : toute cle jamais passee par Read()/Child() est inconnue.
        void WarnUnread() const
        {
            for (auto& [key, _] : m_j.items())
            {
                if (key.starts_with('_')) continue;   // "_comment", "_version"... : assume
                if (!m_seen.contains(key))
                    Logger::warn("\033[31m[" + m_comp + "] cle ignoree '" + key + "' sur " + m_owner + "\033[0m");
            }
        }

    private:
        const nlohmann::json&              m_j;
        std::string                        m_comp, m_owner;
        std::set<std::string, std::less<>> m_seen;
    };

} // namespace LV3
```

Détail délibéré : `nlohmann::json` en toutes lettres, jamais d'alias `using json = ...` au niveau du header — un alias posé dans un header fuite dans tout fichier qui l'inclut. L'alias local (`nlo_json` ou `json`) reste une affaire de `.cpp`.

### 2.4 `ReadProjectionType` — devenu fonction libre côté Scene

```cpp
// Scene/Serializer.cpp — seul endroit qui connait EProjectionType
[[nodiscard]] EProjectionType ReadProjectionType(LV3::JsonReader& r, const char* key)
{
    const std::string s = r.Read(key, std::string("perspective"));
    if (s == "orthographic" || s == "ortho") return EProjectionType::Orthographic;
    if (s == "perspective"  || s == "persp") return EProjectionType::Perspective;

    Logger::warn("[Camera] projection inconnue '" + s + "' -> perspective");
    return EProjectionType::Perspective;
}
```

Appel : `c.m_projection = ReadProjectionType(r, "projection");` — absorbe au passage l'ancienne fonction libre `ReadProjection(const json&, key)` de l'Annexe A5 §6.4, sans exposer le `json` brut de `JsonReader`.

---

## 3. `EngineConfig::LoadFromJson` — donner un corps à la déclaration

### 3.1 Le décalage struct / fichier

`engine.json` contenait des clés qu'`EngineConfig.h` ne modélisait pas encore : `renderer.rasterizer`, `renderer.raycaster`, `renderer.max_bounces`, `renderer.alpha_thresh`, `features.raycast`, et surtout `debug.depthDisplayRange` — exactement la correction prescrite en **Annexe A5 §13.3** ("le 80 en dur sort du code... `"debug": { "depthDisplayRange": 80.0 }`"), restée jusqu'ici sans consommateur.

### 3.2 `Core/EngineConfig.h` — mis à niveau

```cpp
#pragma once
#include <string>
#include "Core/EngineSettings.h"

namespace LV3
{
    struct RendererConfig
    {
        bool  rasterizer  = true;
        bool  raycaster   = false;
        int   tileSize    = LV3_DEFAULT_TILE_SIZE;
        int   maxBounces  = LV3_DEFAULT_MAX_BOUNCES;
        float shadowBias  = LV3_DEFAULT_SHADOW_BIAS;
        float alphaThresh = LV3_DEFAULT_ALPHA_TEST_THRESH;
        bool  multithread = static_cast<bool>(LV3_DEFAULT_FEATURE_MULTITHREAD);
    };

    struct FeaturesConfig
    {
        bool shadows   = static_cast<bool>(LV3_DEFAULT_FEATURE_SHADOWS);
        bool fog       = static_cast<bool>(LV3_DEFAULT_FEATURE_FOG);
        bool wireframe = static_cast<bool>(LV3_DEFAULT_FEATURE_WIREFRAME);
        bool stats     = static_cast<bool>(LV3_DEFAULT_FEATURE_STATS);
        bool raycast   = static_cast<bool>(LV3_DEFAULT_FEATURE_RAYCAST);
    };

    struct ResourcesConfig { std::string path = LV3_DEFAULT_RESOURCE_PATH; };

    // cf. Annexe A5 §13.3 — parametre de LISIBILITE, jamais derive d'une
    // grandeur geometrique (far plane, etc.)
    struct DebugConfig { float depthDisplayRange = 80.0f; };

    struct EngineConfig
    {
        RendererConfig  renderer;
        FeaturesConfig  features;
        ResourcesConfig resources;
        DebugConfig     debug;

        static EngineConfig& Get() { static EngineConfig instance; return instance; }

        // Retourne false si le fichier est introuvable ou malforme :
        // *this garde alors ses defauts LV3_DEFAULT_*, jamais un etat partiel.
        bool LoadFromJson(const std::string& path);
    };
}
```

Changement cassant assumé : `resourcePath` (à plat) devient `resources.path` (imbriqué) pour épouser la forme du JSON — sinon `Child("resources")` n'a nulle part où écrire.

### 3.3 `Core/EngineConfig.cpp`

```cpp
#include "pch.h"
#include "Core/EngineConfig.h"
#include "Core/Logger.h"
#include "Core/JsonReader.h"
#include "../Ressources/json.hpp"
#include <fstream>

namespace LV3
{
    bool EngineConfig::LoadFromJson(const std::string& path)
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            Logger::error("EngineConfig::LoadFromJson — fichier introuvable : " + path);
            return false;
        }

        nlohmann::json root;
        try { file >> root; }
        catch (const nlohmann::json::parse_error& e)
        {
            Logger::error(std::string("EngineConfig::LoadFromJson — JSON malforme : ") + e.what());
            return false;
        }

        try
        {
            JsonReader r(root, "engine", path);

            {
                JsonReader rr = r.Child("renderer");
                renderer.rasterizer  = rr.Read("rasterizer",   true);
                renderer.raycaster   = rr.Read("raycaster",    false);
                renderer.tileSize    = rr.Read("tile_size",    LV3_DEFAULT_TILE_SIZE);
                renderer.maxBounces  = rr.Read("max_bounces",  LV3_DEFAULT_MAX_BOUNCES);
                renderer.shadowBias  = rr.Read("shadow_bias",  LV3_DEFAULT_SHADOW_BIAS);
                renderer.alphaThresh = rr.Read("alpha_thresh", LV3_DEFAULT_ALPHA_TEST_THRESH);
                renderer.multithread = rr.Read("multithread",  static_cast<bool>(LV3_DEFAULT_FEATURE_MULTITHREAD));

                if (renderer.rasterizer && renderer.raycaster)
                    Logger::warn("[EngineConfig] rasterizer ET raycaster actifs — aucun arbitrage defini");
                if (renderer.tileSize <= 0)
                {
                    Logger::warn("[EngineConfig] renderer.tile_size <= 0, force a " + std::to_string(LV3_DEFAULT_TILE_SIZE));
                    renderer.tileSize = LV3_DEFAULT_TILE_SIZE;
                }
                rr.WarnUnread();
            }
            {
                JsonReader rf = r.Child("features");
                features.shadows   = rf.Read("shadows",   static_cast<bool>(LV3_DEFAULT_FEATURE_SHADOWS));
                features.fog       = rf.Read("fog",       static_cast<bool>(LV3_DEFAULT_FEATURE_FOG));
                features.wireframe = rf.Read("wireframe", static_cast<bool>(LV3_DEFAULT_FEATURE_WIREFRAME));
                features.stats     = rf.Read("stats",     static_cast<bool>(LV3_DEFAULT_FEATURE_STATS));
                features.raycast   = rf.Read("raycast",   static_cast<bool>(LV3_DEFAULT_FEATURE_RAYCAST));

                if (features.raycast && !renderer.raycaster)
                    Logger::warn("[EngineConfig] features.raycast=true mais renderer.raycaster=false — sans effet");
                rf.WarnUnread();
            }
            {
                JsonReader rs = r.Child("resources");
                resources.path = rs.Read("path", std::string(LV3_DEFAULT_RESOURCE_PATH));
                rs.WarnUnread();
            }
            if (r.Has("debug"))
            {
                JsonReader rd = r.Child("debug");
                debug.depthDisplayRange = rd.Read("depthDisplayRange", 80.0f);
                rd.WarnUnread();
            }
            r.WarnUnread();
        }
        catch (const nlohmann::json::exception& e)
        {
            Logger::error(std::string("EngineConfig::LoadFromJson — type invalide dans engine.json : ") + e.what());
            return false;
        }

        Logger::log("EngineConfig::LoadFromJson — configuration chargee depuis " + path);
        return true;
    }
}
```

Le `catch (nlohmann::json::exception&)` protège contre ce que `Read()` ne peut pas garder muet : `m_j.value(key, def)` lève un `type_error` si une clé existe avec un type incompatible (`"tile_size": "abc"`). Sans ce filet, une valeur mal typée ferait planter le chargement au lieu de journaliser et retomber sur les défauts — contraire à R7.

Point d'intégration, à poser avant `ResourceManager`/`Renderer::Init` dans `main.cpp` :

```cpp
if (!LV3::EngineConfig::Get().LoadFromJson("engine.json"))
    Logger::warn("EngineConfig — defauts LV3_DEFAULT_* utilises");
```

---

## 4. Où vit `engine.json` — LIB ou EXE

### 4.1 Le test de placement

> **« Si un autre jeu liait cette même `LibraryV3.lib`, `engine.json` changerait-il de contenu ? »**
> Oui → il ne va pas dans la LIB.

`tile_size`, `shadow_bias`, `resources.path`... sont des réglages d'usage, pas des constantes moteur. Une `StaticLibrary` n'a par nature qu'un seul exemplaire compilé ; si le fichier de config vivait à côté d'elle, tous les jeux qui la consomment partageraient la même config — l'inverse de la raison d'être d'`engine.json` (varier sans recompiler la LIB).

### 4.2 Ce que le `.vcxproj` réel montrait

```xml
<!-- LibraryV3.vcxproj — ConfigurationType: StaticLibrary -->
<ItemGroup>
  <None Include="engine.json" />
</ItemGroup>
```

Sans `<Filter>` (contrairement à tout le reste du projet) et sans `CopyToOutputDirectory` — une `StaticLibrary` ne produit pas de dossier d'exécution, personne ne copie ce fichier nulle part au build. Inerte, au même titre que `LoadFromJson()` l'était avant implémentation : une donnée déclarée, jamais consommée (R18 de l'Annexe A5).

### 4.3 Le remède

Même geste que l'Annexe A5 §9.3 pour les `.obj` de gizmo ("Copier si plus récent") :

1. Retirer `engine.json` de `LibraryV3.vcxproj` / `.filters`.
2. L'ajouter dans le `.vcxproj` de l'EXE, en `Content`, `CopyToOutputDirectory = PreserveNewest`.
3. Vérifier que **Debugging → Working Directory** du projet EXE résout le chemin relatif de `LoadFromJson("engine.json")` au même endroit que la copie — sinon le fichier bien placé au build reste introuvable au lancement F5.

---

## 5. Migration des `Parse*` de `Scene/Serializer.cpp`

### 5.1 Le canal `void*` — ne change pas

```cpp
const nlo_json& compJson = *static_cast<const nlo_json*>(pJsonNode);
```

Pare-feu de compilation : `Serializer.hpp` ne doit pas exposer `nlohmann::json` dans ses signatures publiques (sinon tout fichier qui inclut ce header pour appeler `LoadSceneGraph` recompile `json.hpp` avec lui). Le canal opaque reste identique — même mécanisme, même risque, que `void* userData` dans `FragmentCallback` (Leçon 04 P1) : correct par convention, jamais par le type. La migration vers `JsonReader` se fait **après** ce cast, jamais à la place.

### 5.2 `ParseTransform` — le patron

```cpp
void SceneSerializer::ParseTransform(const void* pJsonNode, ParseContext& ctx, Entity entity)
{
    const json& compJson = *static_cast<const json*>(pJsonNode);
    if (!compJson.is_object()) return;

    const std::string owner = EntityLabel(ctx.registry, entity);
    LV3::JsonReader r(compJson, "Transform", owner);

    TransformComponent t;
    t.m_local.position = r.ReadVector("translation", Vec3f::Zero());
    t.m_local.scale    = r.ReadVector("scale",       Vec3f::One());

    const Vec3f eulerDeg = r.ReadVector("rotation", Vec3f::Zero());
    t.m_local.rotation = Quatf(eulerDeg, true);   // le JSON stocke des degres, Quat(v,true) convertit

    t.m_initialRotation = t.m_local.rotation;
    t.m_dirty = true;

    ctx.registry.addComponent(entity, std::move(t));
    r.WarnUnread();
}
```

Recette rejouée sur chaque `Parse*` : owner → `JsonReader r(...)` → `r.Read()`/`r.ReadVector()`/`r.Child()` remplacent `contains()`+`operator[]` → `r.WarnUnread()` en toute dernière ligne, après l'ajout du composant.

### 5.3 `ParseNode` — pourquoi il ne bouge PAS

`ParseNode` route entre blocs de composants ; il ne lit aucune valeur de configuration avec un défaut. R5 protège contre une clé mal orthographiée retombant silencieusement sur un défaut — un problème qui n'existe pas pour du routage structurel. Son `else { Logger::warn("Composant inconnu...") }` **est** déjà l'équivalent de `WarnUnread()`, à son propre étage. Règle de tri pour la suite : une lecture a un défaut sensé si la clé est absente → `JsonReader`. C'est un choix structurel (quel bloc existe, quelle branche prendre) → contrôle explicite qui parle, pas de `JsonReader`.

### 5.4 `ParseLight` — deux bugs trouvés pendant la migration

- `Logger::warn(... + compJson.contains("type"))` — `std::string + bool` n'a pas d'`operator+` : **ne compile pas**. Préexistant dans l'ancien code, corrigé en réutilisant `owner`.
- `r.Read("type", "Ambient")` — `"Ambient"` est un `const char*`, `T` se déduit donc en `const char*`, pas `std::string`. Fonctionne ici par copie immédiate dans `typeStr`, mais diverge de la convention déjà posée (`r.Read("lens", std::string("fov"))` dans `ParseCamera`) et expose à un pointeur pendant en cas de réutilisation future du motif sans copie immédiate. Corrigé : `r.Read("type", std::string("Ambient"))`.
- `r.ReadVector("color", Vec3f(155))` — défaut à confirmer contre l'initialiseur réel de `LightComponent::m_color` ; `155` n'a de sens que si l'échelle de couleur est `[0,255]`, ce qui serait incohérent avec un calcul d'éclairage normalisé. Recommandation : `Vec3f::One()`, cohérent avec la convention `Zero()`/`One()` déjà utilisée dans `ParseTransform`. **Non tranché — à confirmer contre le header du composant.**

### 5.5 `ParseTrigger` — donnée lue puis jetée, et avertissement manquant

```cpp
const bool isColliding = r.Read("isColliding", false);   // lu...
...
ctx.registry.emplaceComponent<TriggerComponent>(
    entity, radius, ...,
    false,                    // ...mais c'est le litteral qui part au constructeur
    std::set<Entity>{}
);
```

`isColliding` était lu puis totalement ignoré — un bug invisible à `WarnUnread()`, puisque la clé, si présente, **est** passée par `Read()` (donc "vue"). C'est un trou différent de celui que R5 couvre : une clé lue puis jetée au site de construction, pas une clé jamais lue. Confirmé par l'auteur du moteur : `is_colliding` est un état **dérivé**, recalculé chaque frame par `TriggerSystem`, jamais autorisé depuis le JSON — exactement comme `overlapping_entities`, hardcodé à `std::set<Entity>{}` sans même tenter une lecture. Correction : suppression pure de la lecture, remplacée par un commentaire explicite au site de construction (R10 — *« un système entretient un invariant, il ne le fabrique pas »*).

Second défaut, indépendant : `r.WarnUnread()` était absent en fin de fonction — oubli mécanique qui rouvrait, pour ce composant seul, exactement le trou que la migration cherchait à fermer partout ailleurs.

### 5.6 `ParsePlayerControl` — migration propre

Aucun défaut trouvé. Seul point resté ouvert : confirmer que le défaut `0.5f` posé dans `r.Read("speed", 0.5f)` correspond à l'initialiseur en classe de `PlayerControlComponent::m_speed`.

### 5.7 `std::move` — un réflexe, pas une évaluation au cas par cas

`addComponent` prend son paramètre **par valeur** (à la différence d'`emplaceComponent`, qui transmet des arguments de constructeur bruts via références universelles — Leçon 03 §4.5). Passer une lvalue force la copie du paramètre ; `std::move` la fait passer par le constructeur de déplacement à la place.

Pour un composant POD pur (`CameraComponent` : que des scalaires et des enums), déplacement et copie produisent le même code machine — gain nul, coût nul. Pour un composant qui possède de la mémoire (`TriggerComponent` et son `std::set<Entity>`, ses `std::string` d'événements), la différence devient réelle : un échange de pointeurs contre une copie profonde.

`ParseCamera` (code plus ancien, antérieur à l'adoption de ce réflexe) reste sans `std::move(c)` — à corriger par cohérence, pas parce que c'est un défaut coûteux aujourd'hui. La valeur du réflexe est justement de ne pas avoir à réévaluer, composant par composant, si ça "vaut le coup".

---

## 6. Règles dictatoriales dégagées (suite de l'Annexe A5)

> **R20 — Mécanisme générique vs politique de domaine.** Un utilitaire partagé (lecteur, sérialiseur) ne connaît que des types Core — multi-système, liste close. Toute conversion vers un type de domaine reste une fonction libre du domaine, jamais une méthode de l'utilitaire.

> **R21 — Portée de R5.** La règle "une clé n'est jamais lue que par `Read()`" protège les valeurs à défaut significatif. Le routage structurel (existence d'un bloc, choix de branche entre composants) est hors de son périmètre — un garde-fou explicite qui parle y joue le même rôle, à son propre étage.

> **R22 — Une clé lue n'est pas une clé consommée.** `WarnUnread()` détecte une clé jamais lue, jamais une valeur lue puis jetée au site de construction. Ce second trou n'est fermé que par relecture humaine du code qui consomme la valeur.

> **R23 — Emplacement projet ≠ emplacement code.** Un fichier de configuration runtime appartient au projet exécutable (EXE) qui le charge, jamais à la bibliothèque statique (LIB) qui définit son schéma — une LIB n'a pas d'espace d'exécution propre, et la partager entre plusieurs jeux annule l'intérêt même d'avoir sorti la valeur du compile-time.

> **R24 — `std::move` en réflexe.** Céder une lvalue locale qu'on ne réutilise plus est un réflexe systématique, pas une décision cas par cas sur le "poids" du composant. Gratuit sur un POD, réel sur un type qui possède de la mémoire — et personne ne se souvient d'avoir "fait exprès" de l'omettre le jour où le composant change de nature.

---

## 7. Journal des bugs

| # | Bug | Détecté par | Cause | Statut |
|---|---|---|---|---|
| 32 | `EngineConfig::LoadFromJson` déclarée, jamais implémentée ni appelée | audit initial de la conversation | fonction laissée en chantier | **fermé** — §3 |
| 33 | `engine.json` inerte dans `LibraryV3.vcxproj` (`None`, sans filtre, sans copie) | lecture du `.vcxproj` réel | fichier de config placé dans la LIB au lieu de l'EXE | **fermé** — §4 |
| 34 | `Logger::warn(... + compJson.contains("type"))` dans `ParseLight` | relecture pendant la migration | `std::string + bool` sans `operator+` — ne compile pas | **fermé** — §5.4 |
| 35 | `r.Read("type", "Ambient")` déduit `T = const char*` | relecture pendant la migration | défaut passé comme littéral au lieu de `std::string(...)` | **fermé** — §5.4 |
| 36 | `Vec3f(155)` comme défaut de `LightComponent::m_color` | relecture pendant la migration | échelle de couleur non confirmée contre le composant | **ouvert** — à valider |
| 37 | `isColliding` lu dans `ParseTrigger` puis jamais utilisé (littéral `false` au constructeur) | relecture pendant la migration | lecture d'un champ dérivé, jamais destiné à être autorisé depuis le JSON | **fermé** — §5.5, lecture supprimée |
| 38 | `r.WarnUnread()` absent de `ParseTrigger` | relecture pendant la migration | oubli mécanique lors de la migration | **fermé** — §5.5 |
| 39 | `ParseCamera` sans `std::move(c)` | question de l'auteur du moteur | code antérieur au réflexe R24 | **ouvert** — correctif trivial, non appliqué |

---

## 8. Dettes

### 8.1 Fermées dans cette annexe

| Dette | Résolution |
|---|---|
| `JsonReader` privé à `Scene/Serializer.cpp` | extrait en `Core/JsonReader.h`, namespace `LV3`, réutilisable par tout consommateur de JSON |
| `depthDisplayRange` en dur (Annexe A5 §13.3) | lu depuis `engine.json` via `EngineConfig::debug.depthDisplayRange` |
| `engine.json` non consommé | `LoadFromJson` implémentée, appelée avant `ResourceManager`/`Renderer::Init` |

### 8.2 Ouvertes, hors périmètre de cette annexe

| Dette | État |
|---|---|
| **Gestion des événements du Trigger** — `onEnterEvent`/`onStayEvent`/`onExitEvent` en `std::string` sur un composant chaud, violation de la règle POD de la Leçon 03 (§4.7 : *« un composant chaud est un POD... jamais ne les possède »*). Piste posée : `enum class EEventID`, résolution string→enum une seule fois au chargement (même patron que `ReadProjectionType`), `IsKnownEvent` absorbée dans la résolution. | **reportée à une annexe dédiée**, en attente du contenu réel de `Core/EventNames.h` et de la déclaration de `TriggerComponent` |
| `Vec3f(155)` comme défaut de couleur dans `ParseLight` | à confirmer contre `LightComponent::m_color` |
| `PlayerControlComponent::m_speed` — défaut `0.5f` posé dans le parseur, à recouper avec l'initialiseur en classe | à vérifier |
| `ParseCamera` sans `std::move(c)` | correctif trivial, non appliqué — cohérence avec R24 |
| `ParseMesh`, `ParseHealth` — encore en lecture directe (`compJson.contains()` + `operator[]`) | migration vers `JsonReader` non entamée, même recette que §5.2 |

---

## 9. Synthèse

Ce chantier a fermé une dette qui se présentait comme deux questions distinctes — *« pourquoi `engine.json` n'est-il pas lu ? »* et *« comment partager `JsonReader` ? »* — et qui n'en formaient qu'une : un mécanisme de lecture correct, enfermé dans le seul système qui en avait l'usage, ne pouvait pas servir à la config moteur ; une config moteur sans lecteur partagé ne pouvait pas être lue proprement. L'extraction a forcé une séparation nette entre mécanisme générique et politique de domaine (R20), révélé au passage que le fichier de config vivait dans le mauvais projet Visual Studio (R23), et la migration systématique des `Parse*` a débusqué trois défauts que ni la compilation ni les tests existants n'auraient signalés — dont un bug de compilation resté invisible tant que personne ne relisait cette branche, et une donnée lue puis jetée que le mécanisme même de traçabilité (R5/`WarnUnread`) ne pouvait pas détecter (R22). La leçon transversale : un mécanisme qui protège contre une classe d'erreurs ne protège que contre celle-là — la relecture humaine reste nécessaire pour tout le reste.

*Annexe A6 — close pour sa partie JsonReader/EngineConfig. La gestion des événements du Trigger fera l'objet d'une annexe séparée.*
