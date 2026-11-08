# Leçon 01 — Architecture des headers : Config, Enums & Settings

> **Moteur :** LibraryV3 | **Projet :** LIB (Static Library) | **Compilateur :** Visual Studio 2026 / C++23

---

## 1. Le problème de départ

Le `pch.h` (Precompiled Header) accélère la compilation en précompilant les headers stables une seule fois.

**Règle absolue :** tout ce qui est inclus dans `pch.h` ne doit **jamais changer** — sinon tout le projet recompile.

Conséquence directe : les `#define` moteur, les `enum` évolutifs et `Platform.h` (qui inclut `windows.h`) **n'ont pas leur place dans `pch.h`**.

---

## 2. Architecture complète des fichiers

### 2.1 Tableau de référence

| Fichier | Dans `pch.h` | Contenu | Règle de sélection |
|---|---|---|---|
| `Core/Compiler.h` | ✅ Oui | `FORCEINLINE`, `ALIGN_SIMD` | Zéro pollution, utilisé partout |
| `Core/Config.h` | ✅ Oui | `PI`, `EPSILON`, `VERSION`, macros debug | Vrai le jour 1, vrai le jour 1000 |
| `Core/CoreTypes.h` | ✅ Oui | `EAxis`, `ESpace`, `EFace`, `EWindingOrder`, `EEnabled` | Multi-système + jamais modifié |
| `Core/Platform.h` | ❌ Non | `#include <windows.h>`, `HWND`, `HDC` | Pollue les macros globales |
| `Core/EngineSettings.h` | ❌ Non | Bornes mémoire + valeurs par défaut | Dimensionne des structures |
| `Core/EngineConfig.h` | ❌ Non | Struct runtime chargée depuis JSON | Configurable sans recompiler |
| `Rendering/RenderTypes.h` | ❌ Non | `ERenderMode`, `ECullMode`, `EDepthTest`, `EBlendMode`, `EShadingModel` | Évolue avec les features |
| `Lighting/LightTypes.h` | ❌ Non | `ELightType`, `EShadowMode`, `EAttenuationMode` | Évolue avec les features |
| `Scene/SceneTypes.h` | ❌ Non | `ELayerMask` + opérateurs bitmask | Bitmask évolutif |
| `engine.json` | — | Features on/off, tile_size, multithread… | Configurable sans recompiler |

### 2.2 Architecture à trois niveaux de configuration

Les moteurs pros distinguent trois couches — chaque réglage appartient à exactement une couche :

```
┌─────────────────────────────────────────────────────────────┐
│  NIVEAU 1 — Compile-time dur          EngineSettings.h      │
│  Bornes mémoire, dimensions de structures                   │
│  LV3_MAX_LIGHTS, LV3_MAX_VERTICES, LV3_MAX_ENTITIES…        │
│  → Changer = recompiler (voulu — impacte les types)         │
├─────────────────────────────────────────────────────────────┤
│  NIVEAU 2 — Startup-time              engine.json           │
│  Features on/off, tile_size, multithread…                   │
│  → Changer = relancer l'exe (pas recompiler)                │
├─────────────────────────────────────────────────────────────┤
│  NIVEAU 3 — Runtime                   EngineConfig (struct) │
│  Chargée depuis engine.json, accessible partout             │
│  → Changer = effet immédiat sans recompiler ni relancer     │
└─────────────────────────────────────────────────────────────┘
```

**Règle de tri :**
```
La valeur dimensionne un tableau ou un type ?   OUI → EngineSettings.h
La valeur change pendant l'exécution du jeu ?   OUI → EngineConfig en mémoire
                                                NON → engine.json suffit
```

---

## 3. `Core/Config.h` — Constantes absolues (dans `pch.h`)

**Critère d'entrée :** vrai le jour 1, vrai le jour 1000. Si on hésite → `EngineSettings.h`.

```cpp
#pragma once
// ============================================================
//  Core/Config.h — Constantes absolues du moteur
//  Inclus via pch.h — NE JAMAIS MODIFIER après initialisation
//  Règle : vrai le jour 1, vrai le jour 1000
// ============================================================

// --- Version (identité du moteur) ---
#define LV3_VERSION_MAJOR   1
#define LV3_VERSION_MINOR   0
#define LV3_VERSION_PATCH   0

// --- Debug (calculé par le compilateur, pas manuellement) ---
#ifdef _DEBUG
    #define LV3_DEBUG       1
    #define LV3_ASSERT(x)   assert(x)
    #define LV3_DEBUG_LOG   1
#else
    #define LV3_DEBUG       0
    #define LV3_ASSERT(x)   ((void)0)
    #define LV3_DEBUG_LOG   0
#endif

// --- Mathématiques (constantes universelles) ---
#define LV3_PI          3.14159265358979323846
#define LV3_INV_PI      0.31830988618379067154
#define LV3_TWO_PI      6.28318530717958647692
#define LV3_HALF_PI     1.57079632679489661923
#define LV3_DEG2RAD     (LV3_PI / 180.0)
#define LV3_RAD2DEG     (180.0 / LV3_PI)
#define LV3_EPSILON     1e-6f
#define LV3_SQRT2       1.41421356237309504880
```

---

## 4. `Core/EngineSettings.h` — Bornes compile-time (HORS `pch.h`)

**Critère d'entrée strict :** la valeur dimensionne un tableau, un type, ou une structure.
Changer cette valeur impose de recompiler — et c'est **voulu**.

Ce qui n'y va plus : les feature flags et les réglages de performance → ils vont dans `engine.json` + `EngineConfig`.

```cpp
#pragma once
// ============================================================
//  Core/EngineSettings.h — Bornes structurelles compile-time
//  HORS pch.h — inclure manuellement là où c'est nécessaire
//  Règle : si changer la valeur oblige à redimensionner
//          un tableau ou un type → ici. Sinon → engine.json
// ============================================================

// --- Bornes mémoire (dimensionnent des structures) ---
#define LV3_MAX_LIGHTS          8       // dimensionne std::array<Light, LV3_MAX_LIGHTS>
#define LV3_MAX_MATERIALS       256     // dimensionne le pool de matériaux
#define LV3_MAX_MESH_VERTICES   65536   // uint16 safe → impacte le type des indices
#define LV3_MAX_SUBMESHES       32      // dimensionne les tableaux par mesh
#define LV3_MAX_RENDER_LAYERS   32      // dimensionne ELayerMask (uint32_t = 32 bits)
#define LV3_MAX_ENTITIES        4096    // dimensionne le Registry ECS
#define LV3_MAX_TEXTURES        512     // slots texture dans le ResourceManager
#define LV3_MAX_MESHES          256     // slots mesh dans le ResourceManager

// --- Valeurs par défaut (fallback si engine.json absent) ---
// Préfixe LV3_DEFAULT_* = signal explicite que c'est un fallback
#define LV3_DEFAULT_TILE_SIZE       64
#define LV3_DEFAULT_MAX_BOUNCES     10
#define LV3_DEFAULT_SHADOW_BIAS     0.0001f
#define LV3_DEFAULT_ALPHA_THRESH    0.5f
#define LV3_DEFAULT_MULTITHREAD     true
#define LV3_DEFAULT_SHADOWS         true
#define LV3_DEFAULT_FOG             false
#define LV3_DEFAULT_STATS           true
#define LV3_DEFAULT_RESOURCE_PATH   "assets/"
```

---

## 5. `engine.json` + `Core/EngineConfig.h` — Configuration runtime

### 5.1 `engine.json` — Startup-time (pas de recompilation)

Tout ce qui était en feature flag dans `EngineSettings.h` migre ici.
Modifier ce fichier → relancer l'exe. Pas toucher au compilateur.

```json
{
    "renderer": {
        "tile_size": 64,
        "multithread": true,
        "shadow_bias": 0.0001,
        "alpha_thresh": 0.5,
        "max_bounces": 10
    },
    "features": {
        "shadows": true,
        "fog": false,
        "wireframe": false,
        "stats": true,
        "raycast": true
    },
    "resources": {
        "path": "assets/"
    }
}
```

### 5.2 `Core/EngineConfig.h` — Struct runtime chargée au démarrage

```cpp
#pragma once
// ============================================================
//  Core/EngineConfig.h — Configuration runtime du moteur
//  Chargée depuis engine.json au démarrage
//  Accessible partout via EngineConfig::Get()
// ============================================================
#include "Core/EngineSettings.h"   // pour les LV3_DEFAULT_*

namespace LV3
{

struct RendererConfig
{
    int   tileSize    = LV3_DEFAULT_TILE_SIZE;
    int   maxBounces  = LV3_DEFAULT_MAX_BOUNCES;
    float shadowBias  = LV3_DEFAULT_SHADOW_BIAS;
    float alphaThresh = LV3_DEFAULT_ALPHA_THRESH;
    bool  multithread = LV3_DEFAULT_MULTITHREAD;
};

struct FeaturesConfig
{
    bool shadows   = LV3_DEFAULT_SHADOWS;
    bool fog       = LV3_DEFAULT_FOG;
    bool wireframe = false;
    bool stats     = LV3_DEFAULT_STATS;
    bool raycast   = true;
};

struct ResourcesConfig
{
    std::string path = LV3_DEFAULT_RESOURCE_PATH;
};

struct EngineConfig
{
    RendererConfig  renderer;
    FeaturesConfig  features;
    ResourcesConfig resources;

    // Singleton — chargé une fois au démarrage, lu partout
    static EngineConfig& Get()
    {
        static EngineConfig instance;
        return instance;
    }

    void LoadFromJson(const std::string& path); // lit engine.json
};

} // namespace LV3
```

### 5.3 Utilisation dans le code

```cpp
// Avant — compile-time, figé, nécessite une recompilation
#if LV3_FEATURE_MULTITHREAD
    RenderTilesParallel();
#endif

// Après — runtime, configurable via engine.json sans recompiler
if (EngineConfig::Get().features.multithread)
    RenderTilesParallel();
else
    RenderTilesSerial();

// Tile size — tester 32 vs 64 vs 128 sans recompiler
const int tileSize = EngineConfig::Get().renderer.tileSize;
m_tiles.reserve((screenW / tileSize) * (screenH / tileSize));
```

### 5.4 Séparation finale des responsabilités

```
EngineSettings.h (compile-time)        engine.json + EngineConfig (runtime)
────────────────────────────           ────────────────────────────────────
LV3_MAX_LIGHTS          ✅             multithread              ✅
LV3_MAX_MESH_VERTICES   ✅             shadows                  ✅
LV3_MAX_ENTITIES        ✅             fog                      ✅
LV3_MAX_RENDER_LAYERS   ✅             tile_size                ✅
LV3_DEFAULT_*           ✅ (fallback)  max_bounces              ✅
                                       resource path            ✅
LV3_FEATURE_*           ❌ supprimé    stats / wireframe        ✅
LV3_TILE_SIZE (direct)  ❌ supprimé
```

---

## 6. `Core/Platform.h` — Pourquoi HORS `pch.h`

`Platform.h` inclut `<windows.h>` qui injecte dans **tous** les `.cpp` :

```cpp
#define min(a,b)   // écrase std::min !
#define max(a,b)   // écrase std::max !
#define near       // collision avec les variables near/far de caméra
#define far        // idem
```

**La bonne architecture — scinder en deux :**

```cpp
// Core/Compiler.h — DANS le PCH
// Contient : FORCEINLINE, ALIGN_SIMD, détection compilateur
// Zéro risque, zéro pollution

// Core/Platform.h — HORS PCH
// Contient : #include <windows.h>, HWND, HDC
// Inclus uniquement dans les .cpp qui ouvrent des fenêtres
```

```cpp
// Rendering/WindowManager.cpp
#include "pch.h"
#include "Core/Platform.h"   // ici seulement — HWND et HDC nécessaires
```

---

## 6. `pch.h` — État final corrigé

```cpp
#pragma once
// STL stable
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <map>
#include <functional>
#include <algorithm>
#include <numeric>
#include <array>
#include <cstdint>
#include <cstddef>
#include <cmath>
#include <cassert>
#include <limits>
#include <type_traits>
#include <string_view>
#include <iostream>
#include <span>
#include <ranges>
#include <format>

#if __cplusplus >= 202302L
    #include <expected>
#endif

// Core moteur stable (jamais modifiés, zéro pollution)
#include "Core/Compiler.h"    // FORCEINLINE, ALIGN_SIMD
#include "Core/Config.h"      // PI, EPSILON, VERSION, ASSERT
#include "Core/CoreTypes.h"   // EAxis, ESpace, EFace, EWindingOrder, EEnabled

// NE PAS INCLURE :
// #include "Core/Platform.h"      ← windows.h pollue les macros globales
// #include "Core/EngineSettings.h" ← se modifie fréquemment
```

---

## 7. Les énumérations

### 7.1 Règle absolue : toujours `enum class`

```cpp
// ❌ INTERDIT — enum nu, pollution du scope global
enum ELightType { Directional, Point, Spot };
// Point entre en collision avec ::Point de Windows.h !

// ✅ OBLIGATOIRE — enum class, scoped et taille contrôlée
enum class ELightType : uint8_t { Directional, Point, Spot };
// Accès : ELightType::Point — zéro ambiguïté
```

**Le type sous-jacent est obligatoire** (`: uint8_t`, `: uint32_t`…) pour :
- Contrôler la taille mémoire
- Garantir la sérialisation
- Assurer l'alignement dans les structures

### 7.2 Filtre de sélection pour `CoreTypes.h`

Un enum entre dans `CoreTypes.h` si et seulement s'il passe **les deux** conditions :

| Condition | Si NON → |
|---|---|
| La liste des valeurs est close — on n'ajoutera jamais rien | Header du système concerné |
| Au moins deux systèmes distincts l'utilisent | Header du système concerné |

### 7.3 `Core/CoreTypes.h` — Vocabulaire fondamental (dans `pch.h`)

```cpp
#pragma once
// ============================================================
//  Core/CoreTypes.h — Vocabulaire fondamental du moteur
//  Inclus via pch.h — LISTE CLOSE, NE PAS MODIFIER
//  Règle : multi-système + jamais modifié
// ============================================================

namespace LV3
{

// Axe cartésien — Transform, Camera, Renderer, Physics
enum class EAxis : uint8_t { X, Y, Z };

// Espace de référence — Transform, Renderer
enum class ESpace : uint8_t { Local, World };

// Face géométrique — Rasterizer, Material
enum class EFace : uint8_t { Front, Back, Both };

// Ordre d'enroulement des triangles — OBJLoader, Rasterizer
enum class EWindingOrder : uint8_t { CW, CCW };

// État d'activation générique — tous les composants
enum class EEnabled : uint8_t { No = 0, Yes = 1 };

} // namespace LV3
```

### 7.4 `Rendering/RenderTypes.h` — Pipeline de rendu (HORS `pch.h`)

```cpp
#pragma once
// ============================================================
//  Rendering/RenderTypes.h — Enums du pipeline de rendu CPU
// ============================================================

namespace LV3
{

// Mode d'affichage global
enum class ERenderMode : uint8_t
{
    Solid, Wireframe, Depth, Normals, UV
};

// Face culling — quelles faces le rasterizer élimine
enum class ECullMode : uint8_t
{
    None, Front, Back
};

// Test de profondeur — comment le Z-buffer accepte un fragment
enum class EDepthTest : uint8_t
{
    Always,     // Toujours passe — skybox, UI
    Less,       // Plus proche — standard 3D
    LessEqual,  // Plus proche ou égal — terrain, décals
    Never       // Masque d'occlusion
};

// Mode de fusion avec le framebuffer
enum class EBlendMode : uint8_t
{
    Opaque,     // Remplace le pixel
    AlphaTest,  // Découpe binaire — feuillages
    AlphaBlend, // Transparence douce — src*a + dst*(1-a)
    Additive    // Accumulation — feu, particules
};

// Modèle d'ombrage
enum class EShadingModel : uint8_t
{
    Unlit,    // Aucun calcul lumière
    Flat,     // Une normale par face
    Gouraud,  // Interpolation par sommet
    Phong     // Interpolation par fragment
};

} // namespace LV3
```

**Flux du pipeline :**
```
Mesh → ECullMode → EDepthTest → EShadingModel → EBlendMode → ERenderMode → Framebuffer
```

### 7.5 `Lighting/LightTypes.h` — Système d'éclairage (HORS `pch.h`)

```cpp
#pragma once
// ============================================================
//  Lighting/LightTypes.h — Enums du système d'éclairage
// ============================================================

namespace LV3
{

// Type de source lumineuse
enum class ELightType : uint8_t
{
    Directional,  // Soleil — direction infinie, pas de position
    Point,        // Ampoule — position, atténuation sphérique
    Spot,         // Projecteur — position + cône (innerAngle / outerAngle)
    Ambient       // Lumière globale — aucune direction
};

// Comportement des ombres portées (par lumière)
enum class EShadowMode : uint8_t
{
    None,   // Pas d'ombre
    Hard,   // Ombre franche — shadow map basique
    Soft    // Ombre douce — PCF
};

// Loi d'atténuation de l'intensité avec la distance
enum class EAttenuationMode : uint8_t
{
    None,          // Pas d'atténuation (débogage)
    Linear,        // f = 1 - (d / range)
    Quadratic,     // f = 1 / (1 + k*d²) — physiquement correct
    InverseSquare  // f = (range / max(d,0.01))² — standard PBR
};

} // namespace LV3
```

### 7.6 `Scene/SceneTypes.h` — Bitmasks de couches (HORS `pch.h`)

```cpp
#pragma once
// ============================================================
//  Scene/SceneTypes.h — Enums et opérateurs du système scène
// ============================================================

namespace LV3
{

enum class ELayerMask : uint32_t  // uint32_t = 32 couches max
{
    None     = 0,
    Default  = 1 << 0,   // 0x01
    UI       = 1 << 1,   // 0x02
    Physics  = 1 << 2,   // 0x04
    Raycast  = 1 << 3,   // 0x08
    FX       = 1 << 4,   // 0x10
    Editor   = 1 << 5,   // 0x20
    All      = ~0u        // 0xFFFFFFFF
};

// Obligatoire — enum class bloque |, & et ~ par défaut
inline ELayerMask operator|(ELayerMask a, ELayerMask b)
{
    return static_cast<ELayerMask>(
        static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline ELayerMask operator&(ELayerMask a, ELayerMask b)
{
    return static_cast<ELayerMask>(
        static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline ELayerMask operator~(ELayerMask a)
{
    return static_cast<ELayerMask>(~static_cast<uint32_t>(a));
}

// Raccourci — true si au moins un bit commun
inline bool HasLayer(ELayerMask mask, ELayerMask layer)
{
    return (mask & layer) != ELayerMask::None;
}

} // namespace LV3
```

**Les trois opérations fondamentales :**

```cpp
// Activer des couches — opérateur |
ELayerMask mask = ELayerMask::Default | ELayerMask::Physics;
//   Default  = 0000 0001
//   Physics  = 0000 0100
//   résultat = 0000 0101

// Tester — opérateur &
if (HasLayer(mask, ELayerMask::Physics))  // true
if (HasLayer(mask, ELayerMask::UI))       // false

// Retirer — opérateur &~
mask = mask & ~ELayerMask::Physics;       // éteint le bit 2
```

> **Pourquoi `uint32_t` et pas `uint8_t` :** `uint8_t` = 8 couches max. Unity en a 32.
> Changer le type sous-jacent en cours de projet casse la sérialisation des scènes.

---

## 8. `#if` vs `#ifdef` — règle définitive

```cpp
// ❌ #ifdef — on ne peut que définir ou ne pas définir
#ifdef LV3_FEATURE_SHADOWS
    RenderShadows();
#endif
// Pour désactiver : supprimer le #define → risque de références pendantes

// ✅ #if — on force 0 sans supprimer le define
#if LV3_FEATURE_SHADOWS
    RenderShadows();
#endif
// Pour désactiver : LV3_FEATURE_SHADOWS = 0
// Le define existe toujours → aucun "symbole manquant" ailleurs
```

---

## 9. Utilisation dans le code

```cpp
// Renderer.cpp
#include "pch.h"                       // Compiler + Config + CoreTypes disponibles
#include "Core/EngineSettings.h"       // LV3_MAX_LIGHTS, LV3_DEFAULT_*
#include "Core/EngineConfig.h"         // EngineConfig::Get()
#include "Rendering/RenderTypes.h"     // ERenderMode, ECullMode…
#include "Scene/SceneTypes.h"          // ELayerMask

void Renderer::Init()
{
    // tile_size lu depuis engine.json — pas de recompilation pour changer
    const int tileSize = EngineConfig::Get().renderer.tileSize;
    m_tiles.reserve((screenW / tileSize) * (screenH / tileSize));
}

void Renderer::RenderFrame()
{
    // Features contrôlées depuis engine.json
    if (EngineConfig::Get().features.multithread)
        RenderTilesParallel();
    else
        RenderTilesSerial();

    if (EngineConfig::Get().features.shadows)
        RenderShadowPass();

    if (EngineConfig::Get().features.stats)
        m_stats.drawcalls++;
}

// LightSystem.cpp — LV3_MAX_LIGHTS reste compile-time (dimensionne un tableau)
void LightSystem::AddLight(LightComponent* light)
{
    LV3_ASSERT(m_lights.size() < LV3_MAX_LIGHTS);
    m_lights.push_back(light);
}

// SceneManager.cpp — culling par layer
void SceneManager::Cull(Camera& cam)
{
    for (auto& obj : m_objects)
        if (HasLayer(cam.cullingMask, obj.layer))
            m_visibleObjects.push_back(&obj);
}
```

---

## 10. `engine.json` — Fichier exemple complet

```json
{
    "_comment": "LibraryV3 — Configuration moteur (engine.json)",
    "_version": "1.0.0",

    "renderer": {
        "tile_size": 64,
        "max_bounces": 10,
        "shadow_bias": 0.0001,
        "alpha_thresh": 0.5,
        "multithread": true
    },

    "features": {
        "shadows": true,
        "fog": false,
        "wireframe": false,
        "stats": true,
        "raycast": true
    },

    "resources": {
        "path": "assets/"
    }
}
```

> Les clés préfixées `_` (`_comment`, `_version`) sont ignorées silencieusement par `ReadIf()` — elles servent de commentaires car JSON n'a pas de syntaxe native pour ça.

---

## 11. `Core/EngineConfig.cpp` — Chargement complet

### 11.1 Process de chargement

```
LoadFromJson("engine.json")
    │
    ├── Fichier absent   → Warning loggé + Sanitize() + return false
    ├── JSON malformé    → Error loggée  + Sanitize() + return false
    └── Fichier valide   → ReadIf() sur chaque clé (merge partiel)
                                │
                                ├── Clé présente → valeur JSON écrase le default
                                └── Clé absente  → LV3_DEFAULT_* conservé intact
                                │
                                └── Sanitize()        → valeurs plancher
                                └── LogCurrentConfig() → dump au démarrage
```

### 11.2 Code complet

```cpp
#include "pch.h"
#include "Core/EngineConfig.h"
#include "Core/Logger.h"
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace LV3
{

// -------------------------------------------------------
//  Helper local — lit une valeur si la clé existe,
//  laisse la valeur par défaut intacte sinon (merge partiel)
// -------------------------------------------------------
template<typename T>
static void ReadIf(const json& obj, const std::string& key, T& target)
{
    if (obj.contains(key) && !obj[key].is_null())
        target = obj[key].get<T>();
}

// -------------------------------------------------------
bool EngineConfig::LoadFromJson(const std::string& filepath)
{
    // --- 1. Ouverture ---
    std::ifstream file(filepath);
    if (!file.is_open())
    {
        Logger::Warning("[EngineConfig] engine.json introuvable : '"
            + filepath + "' — valeurs par défaut conservées.");
        Sanitize();
        return false;
    }

    // --- 2. Parsing ---
    json data;
    try { file >> data; }
    catch (const json::parse_error& e)
    {
        Logger::Error("[EngineConfig] JSON malformé : " + std::string(e.what())
            + " — valeurs par défaut conservées.");
        Sanitize();
        return false;
    }

    // --- 3. Vérification de version (optionnelle) ---
    if (data.contains("_version"))
    {
        std::string fileVer = data["_version"].get<std::string>();
        if (fileVer != "1.0.0")
            Logger::Warning("[EngineConfig] Version inattendue : " + fileVer);
    }

    // --- 4. Merge partiel — chaque clé est optionnelle ---
    if (data.contains("renderer") && data["renderer"].is_object())
    {
        const auto& r = data["renderer"];
        ReadIf(r, "tile_size",    renderer.tileSize);
        ReadIf(r, "max_bounces",  renderer.maxBounces);
        ReadIf(r, "shadow_bias",  renderer.shadowBias);
        ReadIf(r, "alpha_thresh", renderer.alphaThresh);
        ReadIf(r, "multithread",  renderer.multithread);
    }

    if (data.contains("features") && data["features"].is_object())
    {
        const auto& f = data["features"];
        ReadIf(f, "shadows",   features.shadows);
        ReadIf(f, "fog",       features.fog);
        ReadIf(f, "wireframe", features.wireframe);
        ReadIf(f, "stats",     features.stats);
        ReadIf(f, "raycast",   features.raycast);
    }

    if (data.contains("resources") && data["resources"].is_object())
        ReadIf(data["resources"], "path", resources.path);

    // --- 5. Sanitisation — valeurs plancher ---
    Sanitize();

    Logger::info("[EngineConfig] Chargé depuis '" + filepath + "'.");
    LogCurrentConfig();
    return true;
}

// -------------------------------------------------------
//  Sanitize — protège contre les valeurs aberrantes dans le JSON
//  Appelé même si le fichier est absent (protège les defaults)
// -------------------------------------------------------
void EngineConfig::Sanitize()
{
    if (renderer.tileSize   < 1)    renderer.tileSize   = LV3_DEFAULT_TILE_SIZE;
    if (renderer.maxBounces < 0)    renderer.maxBounces = LV3_DEFAULT_MAX_BOUNCES;
    if (renderer.shadowBias < 0.f)  renderer.shadowBias = LV3_DEFAULT_SHADOW_BIAS;
    if (renderer.alphaThresh < 0.f || renderer.alphaThresh > 1.f)
                                    renderer.alphaThresh = LV3_DEFAULT_ALPHA_THRESH;
    if (resources.path.empty())     resources.path      = LV3_DEFAULT_RESOURCE_PATH;
}

// -------------------------------------------------------
void EngineConfig::LogCurrentConfig() const
{
    Logger::info("[EngineConfig] ── Renderer ──────────────────────");
    Logger::info("  tile_size   : " + std::to_string(renderer.tileSize));
    Logger::info("  max_bounces : " + std::to_string(renderer.maxBounces));
    Logger::info("  multithread : " + std::string(renderer.multithread ? "ON" : "OFF"));
    Logger::info("[EngineConfig] ── Features ──────────────────────");
    Logger::info("  shadows     : " + std::string(features.shadows   ? "ON" : "OFF"));
    Logger::info("  fog         : " + std::string(features.fog       ? "ON" : "OFF"));
    Logger::info("  wireframe   : " + std::string(features.wireframe ? "ON" : "OFF"));
    Logger::info("  stats       : " + std::string(features.stats     ? "ON" : "OFF"));
    Logger::info("[EngineConfig] ── Resources ─────────────────────");
    Logger::info("  path        : " + resources.path);
}

} // namespace LV3
```

---

## 12. `main.cpp` — Démarrage complet

### 12.1 Ordre d'initialisation

```
1. Plateforme    SetConsoleMode()        encodage UTF-8 — toujours en premier
2. Logger        Logger::Init()          doit être prêt avant TOUT le reste
3. EngineConfig  LoadFromJson()          engine.json → merge avec LV3_DEFAULT_*
4. Resources     ResourceManager::Init() chemin assets/ vient d'EngineConfig
5. Window        Window::Init()          surface de rendu (HWND)
6. Renderer      Renderer::Init()        tile_size/multithread depuis EngineConfig
7. Scène         SceneSerializer::Load() meshes/matériaux doivent être disponibles
8. Boucle        RunMainLoop()           Update → Render → Events
9. Shutdown      ordre INVERSE           Scene → Renderer → Resources → Window → Logger
```

> **Règle d'or :** le Logger est le premier à naître et le **dernier à mourir**.
> Sans ça, les erreurs de shutdown sont silencieuses.

### 12.2 Code complet

```cpp
// main.cpp
#include "pch.h"
#include "Core/Platform.h"      // SetConsoleMode — inclus ici seulement
#include "Core/Logger.h"
#include "Core/EngineConfig.h"
#include "Core/EngineSettings.h"

using namespace LV3;

bool InitRenderer();
bool LoadScene(const std::string& scenePath);
void RunMainLoop();
void Shutdown();

// -------------------------------------------------------
int main()
{
    // ════════════════════════════════════════════════════
    //  1 — Plateforme
    //  Toute première instruction — configure l'encodage
    //  UTF-8 de la console (accents dans les logs).
    // ════════════════════════════════════════════════════
    SetConsoleMode();

    // ════════════════════════════════════════════════════
    //  2 — Logger
    //  AVANT tout autre système — les erreurs suivantes
    //  doivent pouvoir être loguées.
    // ════════════════════════════════════════════════════
    Logger::Init();
    Logger::info("══════════════════════════════════════════");
    Logger::info("  LibraryV3 v"
        + std::to_string(LV3_VERSION_MAJOR) + "."
        + std::to_string(LV3_VERSION_MINOR) + "."
        + std::to_string(LV3_VERSION_PATCH));
    Logger::info("══════════════════════════════════════════");

    // ════════════════════════════════════════════════════
    //  3 — Configuration moteur
    //  Charge engine.json, merge avec LV3_DEFAULT_*.
    //  Si absent → warning + defaults actifs → on continue.
    //  DOIT être avant ResourceManager (chemin assets/)
    //  et avant Renderer (tile_size, multithread).
    // ════════════════════════════════════════════════════
    if (!EngineConfig::Get().LoadFromJson("engine.json"))
        Logger::Warning("Démarrage avec la configuration par défaut.");

    // ════════════════════════════════════════════════════
    //  4 — ResourceManager
    //  Le chemin assets/ vient d'EngineConfig → après lui.
    //  Prépare les pools : textures, meshes, matériaux.
    // ════════════════════════════════════════════════════
    const std::string& assetsPath = EngineConfig::Get().resources.path;
    Logger::info("ResourceManager → path : " + assetsPath);
    // ResourceManager::Get().Init(assetsPath);   ← Leçon suivante

    // ════════════════════════════════════════════════════
    //  5 — Fenêtre
    //  Crée la surface de rendu (HWND).
    //  Platform.h est inclus ici et nulle part ailleurs.
    // ════════════════════════════════════════════════════
    Logger::info("Window → création...");
    // Window::Get().Init(1280, 720, "LibraryV3");  ← Leçon suivante
    Logger::info("Window → OK");

    // ════════════════════════════════════════════════════
    //  6 — Renderer
    //  Lit tile_size et multithread depuis EngineConfig.
    //  Doit être après Window (surface cible) et
    //  après ResourceManager (accès meshes/textures).
    // ════════════════════════════════════════════════════
    Logger::info("Renderer → initialisation...");
    if (!InitRenderer())
    {
        Logger::Error("Renderer → échec. Arrêt.");
        Shutdown();
        return EXIT_FAILURE;
    }

    // ════════════════════════════════════════════════════
    //  7 — Chargement de la scène
    //  SceneSerializer lit scene.json et construit le
    //  Registry ECS. Doit être après Renderer et
    //  ResourceManager (les ressources doivent exister).
    // ════════════════════════════════════════════════════
    Logger::info("Scène → chargement...");
    if (!LoadScene(assetsPath + "scenes/scene.json"))
    {
        Logger::Error("Scène → échec. Arrêt.");
        Shutdown();
        return EXIT_FAILURE;
    }

    // ════════════════════════════════════════════════════
    //  8 — Boucle principale
    //  Update (logique) → Render (visuel) → Events (input)
    //  Tourne jusqu'à fermeture de la fenêtre.
    // ════════════════════════════════════════════════════
    Logger::info("Boucle principale → démarrage.");
    RunMainLoop();

    // ════════════════════════════════════════════════════
    //  9 — Shutdown (ordre INVERSE de l'init)
    // ════════════════════════════════════════════════════
    Shutdown();
    return EXIT_SUCCESS;
}

// -------------------------------------------------------
bool InitRenderer()
{
    const auto& cfg = EngineConfig::Get().renderer;
    Logger::info("  tile_size   : " + std::to_string(cfg.tileSize));
    Logger::info("  multithread : " + std::string(cfg.multithread ? "ON" : "OFF"));
    Logger::info("  shadows     : " + std::string(
        EngineConfig::Get().features.shadows ? "ON" : "OFF"));
    // Renderer::Get().Init(cfg);  ← Leçon suivante
    return true;
}

bool LoadScene(const std::string& scenePath)
{
    Logger::info("  fichier : " + scenePath);
    // SceneSerializer::Get().Load(scenePath, registry);  ← Leçon suivante
    return true;
}

void RunMainLoop()
{
    // while (Window::Get().IsOpen())
    // {
    //     float dt = Timer::Get().Tick();
    //     SceneManager::Get().Update(dt);
    //     Renderer::Get().RenderFrame();
    //     Window::Get().PollEvents();
    // }
    Logger::info("[stub] boucle principale — Leçon suivante.");
}

void Shutdown()
{
    Logger::info("Shutdown → début (ordre inverse)...");
    // SceneManager::Get().Shutdown();
    // Renderer::Get().Shutdown();
    // ResourceManager::Get().Shutdown();
    // Window::Get().Shutdown();
    Logger::info("Shutdown → terminé.");
    Logger::Shutdown();   // Logger en dernier — toujours
}
```

---

## 13. Résumé décisionnel

```
Init :     Logger → Config → Resources → Window → Renderer → Scene
Shutdown : Scene → Renderer → Resources → Window → Config → Logger
           (ordre strictement inverse — Logger toujours dernier)
```

```
Un header change souvent ?
    OUI → hors pch.h (EngineSettings.h, RenderTypes.h…)
    NON → peut entrer dans pch.h (Config.h, CoreTypes.h, Compiler.h)

C'est un #define ?
    Constante mathématique / version         → Config.h         → pch.h
    Borne mémoire (dimensionne un tableau)   → EngineSettings.h → hors pch.h
    Valeur par défaut (fallback JSON)        → EngineSettings.h → LV3_DEFAULT_*
    Feature flag / réglage de perf           → engine.json      → EngineConfig::Get()
    Paramètre variable au runtime            → engine.json      → EngineConfig::Get()

C'est un enum ?
    Multi-système + liste close              → CoreTypes.h      → pch.h
    Vocabulaire du renderer                  → RenderTypes.h    → hors pch.h
    Vocabulaire de l'éclairage               → LightTypes.h     → hors pch.h
    Bitmask de couches                       → SceneTypes.h     → hors pch.h + opérateurs

C'est un header plateforme ?
    Macros compilateur (FORCEINLINE)         → Compiler.h       → pch.h
    API OS (windows.h, HWND)                 → Platform.h       → hors pch.h (inclus manuellement)
```
