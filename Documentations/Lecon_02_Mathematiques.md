# Leçon 02 — La librairie mathématique du moteur

> **Moteur :** LibraryV3 | **Projet :** LIB (Static Library) | **Compilateur :** Visual Studio 2026 / C++23
> **Namespace canonique :** `LV3` | **Préfixe macro :** `LV3_`

---

## 0. État de départ (audit du legacy LibV2)

La lib mathématique héritée (`Vec3f`, `Vec4f`, `Matrix44f`, `Quat`) présentait :

| Constat | Nature |
|---|---|
| Stockage **colonne-majeur** `m[col][row]` | ✅ À conserver |
| Convention **vecteurs-colonnes** `v' = M·v` | ✅ À conserver |
| `LookAt` en **main droite** (`f = -forward`) | ⚠️ À réécrire (voir Décision A) |
| `Perspective` mappant la profondeur dans **`[-1, 1]`** (OpenGL) | ⚠️ À réécrire (voir Décision C) |
| `Quat<T>` templaté mais **hard-code `Vec3f` (float)** | ❌ Incohérence à corriger |
| **Trois namespaces** cohabitent : `LibV2`, `LibV3`, `LV3` | ❌ À unifier (voir §6) |
| Aucun `Transform` ni couche géométrie | ❌ Manquant (fait « lib » et non « moteur ») |

**Principe directeur de la leçon :** en 3D, une convention non décidée est un bug qui dort. On tranche tout, une fois.

---

## 1. Pourquoi une lib maison plutôt que GLM

En production : on prend GLM, DirectXMath ou Eigen, point. Libs blindées, vectorisées SIMD, éprouvées.

Mais **LibraryV3 est un moteur d'apprentissage CPU**, ce qui inverse la décision :

- **Comprendre, pas consommer** — impossible d'auditer un rasterizer sans maîtriser *exactement* la multiplication matrice-vecteur.
- **GLM a la forme du GPU** — son API mime GLSL. Un *software renderer* CPU veut ses propres ergonomies (contrôle du NDC, de la profondeur, du balayage).
- **Contrôle total du layout mémoire** — indispensable pour le SIMD et le rendu par tuiles (`LV3_TILE_SIZE`) : alignement, AoS vs SoA.
- **Zéro dépendance, débogabilité totale** — point d'arrêt possible dans chaque opération.

> ⚠️ **Piège :** « maison ≠ plus rapide ». Au début, la lib sera *plus lente* que GLM. La valeur est **pédagogique et architecturale**, jamais la performance.

---

## 2. Inventaire fonctionnel

Périmètre minimal d'un moteur type Unity :

| Couche | Contenu | Rôle |
|---|---|---|
| **Scalaires** | `lerp`, `clamp`, `saturate`, `radians`/`degrees`, `sign`, `approxEqual`, `smoothstep` | Briques de base, comparaisons robustes |
| **Vecteurs** | `Vec2`, `Vec3`, `Vec4` (+ `dot`, `cross`, `normalize`, `length`, `reflect`) | Positions, normales, UV, couleurs |
| **Matrices** | `Mat3` (rotation/normales), `Mat4` (transformations homogènes) | Cœur du pipeline MVP |
| **Quaternions** | `Quat` (+ `slerp`, `fromAxisAngle`, `toMatrix`) | Rotations sans gimbal lock |
| **Transform** | composite **T·R·S** | Abstraction « GameObject » |
| **Géométrie** | `Ray`, `Plane`, `AABB`, `Sphere`, `Frustum` | Culling, picking, raycast |
| **Interpolations** | `lerp`, `slerp`, Bézier | Animation, caméra |

Le legacy couvre Vec/Mat/Quat. **Manquants : `Transform` + couche géométrie.**

---

## 3. Les trois décisions dictatoriales

Ce sont les choix qu'on ne peut **jamais** changer plus tard sans tout casser.

### Décision A — Repère : main gauche, Y haut, Z avant

**Comme Unity.** X à droite, Y en haut, **+Z entre dans l'écran**.

- Intuitif : « plus loin = Z plus grand ».
- Aligne le modèle mental sur la référence Unity.

> ⚠️ **Conséquence :** le legacy `LookAt` (main droite, `f = -forward`) **doit être réécrit** en main gauche. Un repère mal aligné avec le rasterizer donne des objets retournés et un *backface culling* inversé.

### Décision B — Stockage et convention matricielle

Deux notions distinctes à ne pas confondre :

| Aspect | Choix | Statut vs legacy |
|---|---|---|
| **Convention** | vecteurs-colonnes, `v' = M·v`, composition **droite → gauche** `P·V·M·v` | ✅ Inchangé |
| **Stockage** | colonne-majeur, `m[col][row]` | ✅ Inchangé |

> Sur ce point : **continuité totale avec LibV2.** La cohérence se paie en ne touchant pas à ce qui marche. Bonus : facilite un futur upload GPU.

### Décision C — Profondeur NDC et précision

- **NDC profondeur dans `[0, 1]`** (style Direct3D), pas `[-1, 1]` (OpenGL legacy).
  → Meilleure répartition de la précision du *depth buffer* près de la caméra.
  → *Reverse-Z* (proche mappé sur 1) : optimisation ultérieure.
- **`float` partout.** Le `double` reste réservé à de rares accumulations (physique longue). Jamais dans `Vec3` : doublerait la bande passante mémoire pour rien.

> ⚠️ **Conséquence :** le legacy `Perspective` (NDC `[-1,1]`) **doit être réécrit**.

---

## 4. Sujets ajoutés au programme

Au-delà des questions posées, un cours sérieux doit couvrir :

1. **Le `Transform` T·R·S** — l'abstraction reine (position + rotation quaternion + scale, matrices locale et monde mises en cache). Le plus gros manque du legacy.
2. **Gimbal lock et quaternions** — *pourquoi* stocker les rotations en quaternion plutôt qu'en angles d'Euler.
3. **Transformation des normales** — piège classique : une normale ne se transforme **pas** comme une position. Sous scale non-uniforme → **transposée de l'inverse** de la matrice. C'est un `Mat3`.
4. **Robustesse numérique** — jamais de `==` sur des floats. `approxEqual` avec `LV3_EPSILON`, garde-fou anti division-par-zéro dans `normalize`.
5. **Alignement SIMD** — concevoir `Vec4` et `Mat4` `alignas(16)` *dès maintenant*, même sans SIMD, pour éviter un réalignement futur.
6. **Taxonomie des espaces** — Local → Monde → Vue → Clip → NDC → Écran. Savoir dans quel espace vit chaque donnée est la compétence n°1 en 3D.
7. **Ne pas optimiser trop tôt** — le mythe du *fast inverse square root* : inutile aujourd'hui. `std::sqrt` est précis et vectorisé par le compilateur.

---

## 5. Code de référence — `Vec3` canonique LV3

Pur C++23, header-only, `constexpr`, zéro dépendance plateforme.
Différences avec `Vec3f` legacy : namespace `LV3`, garde-fou dans `normalized`, repère canonique explicite.

```cpp
#pragma once
// ============================================================
//  Maths/Vec3.hpp — Vecteur 3D du moteur LV3
//  Pur C++23 : header-only, constexpr, zéro dépendance plateforme
// ============================================================
#include <cmath>

namespace LV3
{
    struct Vec3
    {
        float x = 0.0f, y = 0.0f, z = 0.0f;

        constexpr Vec3() noexcept = default;
        constexpr Vec3(float x_, float y_, float z_) noexcept : x(x_), y(y_), z(z_) {}
        explicit constexpr Vec3(float s) noexcept : x(s), y(s), z(s) {}

        // --- Arithmétique composante par composante ---
        constexpr Vec3 operator+(const Vec3& v) const noexcept { return { x + v.x, y + v.y, z + v.z }; }
        constexpr Vec3 operator-(const Vec3& v) const noexcept { return { x - v.x, y - v.y, z - v.z }; }
        constexpr Vec3 operator*(float s)       const noexcept { return { x * s, y * s, z * s }; }
        constexpr Vec3 operator-()              const noexcept { return { -x, -y, -z }; }
        constexpr Vec3& operator+=(const Vec3& v) noexcept { x += v.x; y += v.y; z += v.z; return *this; }

        // --- Produits ---
        constexpr float dot(const Vec3& v) const noexcept { return x * v.x + y * v.y + z * v.z; }
        constexpr Vec3  cross(const Vec3& v) const noexcept
        {
            return { y * v.z - z * v.y,
                     z * v.x - x * v.z,
                     x * v.y - y * v.x };
        }

        // --- Longueur (sqrt n'est pas constexpr : length() reste runtime) ---
        constexpr float lengthSq() const noexcept { return dot(*this); }
        float           length()   const noexcept { return std::sqrt(lengthSq()); }

        Vec3 normalized() const noexcept
        {
            const float l2 = lengthSq();
            if (l2 < LV3_EPSILON) return { 0.0f, 0.0f, 0.0f };   // garde-fou : pas de division par ~0
            return *this * (1.0f / std::sqrt(l2));
        }
    };

    // --- Repère canonique LV3 : main gauche, Y haut, Z avant ---
    inline constexpr Vec3 VEC3_RIGHT   { 1.0f, 0.0f, 0.0f };
    inline constexpr Vec3 VEC3_UP      { 0.0f, 1.0f, 0.0f };
    inline constexpr Vec3 VEC3_FORWARD { 0.0f, 0.0f, 1.0f };   // +Z entre dans l'écran
}
```

---

## 6. Architecture des fichiers `Maths/`

```
Maths/
  MathTypes.h     ← umbrella : inclut tout + alias (Vec3, Mat4…) — inclus par OBJLoader.h
  Scalar.hpp      ← lerp, clamp, saturate, radians, approxEqual…
  Vec2.hpp  Vec3.hpp  Vec4.hpp
  Mat3.hpp  Mat4.hpp
  Quat.hpp
  Transform.hpp   ← composite T·R·S (Unity-like)
  Geometry/
    Ray.hpp  Plane.hpp  AABB.hpp  Sphere.hpp  Frustum.hpp
```

### PCH ou pas ?

Application directe de la Leçon 1 :

- **Pendant le développement de la lib** → **hors PCH.** On édite ces fichiers en permanence ; les mettre dans le PCH = recompilation totale à chaque virgule.
- **Une fois `Vec3`/`Mat4` stabilisés et figés** → candidats au PCH (stables, utilisés partout). **Pas avant.**

---

## 7. Migration depuis le legacy — checklist

| Action | Détail |
|---|---|
| ☐ Unifier le namespace | `LibV2` + `LibV3` → **`LV3`** (aligne avec `LV3_` et `CoreTypes.h`) |
| ☐ Réécrire `LookAt` | Passage **main droite → main gauche** |
| ☐ Réécrire `Perspective` | NDC **`[-1,1]` → `[0,1]`** |
| ☐ Nettoyer `Quat` | Supprimer le mélange `T` / `Vec3f` hard-codé |
| ☐ Ajouter garde-fous | `approxEqual`, protection division-par-zéro dans `normalize` |
| ☐ Créer `Transform.hpp` | Composite T·R·S manquant |
| ☐ Créer `Maths/Geometry/` | `Ray`, `Plane`, `AABB`, `Sphere`, `Frustum` |
| ☐ Aligner `Vec4`/`Mat4` | `alignas(16)` dès la conception |

---

## 8. Résumé décisionnel

```
REPÈRE
    Main gauche, Y haut, Z avant (+Z entre dans l'écran)   → comme Unity
    Conséquence : réécrire LookAt

MATRICES
    Convention  → vecteurs-colonnes, v' = M·v, compo P·V·M  → inchangé
    Stockage    → colonne-majeur, m[col][row]               → inchangé

PROFONDEUR
    NDC         → [0, 1] (Direct3D)                         → réécrire Perspective
    Reverse-Z   → plus tard (optimisation de précision)

PRÉCISION
    float partout ; double réservé aux accumulations rares

NAMESPACE
    LV3 unique (fin de LibV2 / LibV3)

PCH
    Maths hors PCH pendant le dev ; candidats au PCH une fois figés

RÈGLE D'OR
    N'optimise pas trop tôt : code clair d'abord, profilage ensuite
```

---

*Prochaine étape — Leçon 2.1 : les vecteurs en profondeur (dot, cross, projection, réflexion), avec intuitions géométriques visuelles.*
