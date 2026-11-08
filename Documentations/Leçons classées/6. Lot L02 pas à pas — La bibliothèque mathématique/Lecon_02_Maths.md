# Leçon 02 — La bibliothèque mathématique

> **Moteur :** LibraryV3 | **Projet :** LIB (Static Library) | **Compilateur :** Visual Studio 2026 / C++23
> **Convention géométrique :** row-major, vecteur-ligne, S·R·T, right-handed, NDC z ∈ [-1, 1]

---

## 1. Pourquoi la lib maths est le contrat géométrique du moteur

Une lib maths n'est pas un sac de fonctions utiles : c'est le **contrat** que tout le moteur doit respecter. Si deux fichiers divergent sur *une seule* convention (sens des axes, rangement mémoire, ordre de multiplication), la caméra, les normales, les ombres et le culling deviendront subtilement faux. **On verrouille les conventions avant d'écrire une ligne.**

La liste des fonctionnalités est dictée par le pipeline géométrique :

```
 Local ──M_model──▶ Monde ──M_view──▶ Vue ──M_proj──▶ Clip ──÷w──▶ NDC ──viewport──▶ Écran
(sommets)         (scène)          (caméra)        (x,y,z,w)    [-1,1]            (pixels)
```

De ce flux découlent deux couches :

- **Algèbre linéaire** : Vec2/3/4, Mat4 (TRS + caméra), quaternions, Transform.
- **Géométrie & rasterizer** : Plane, AABB, Frustum, Ray, barycentriques, edge functions, clipping, viewport, interpolation perspective-correct.

---

## 2. Fiche de convention — verrouillée

| Décision | Choix LibraryV3 |
|---|---|
| Rangement mémoire | **row-major** `m[row][col]` |
| Orientation vecteur | **vecteur-ligne** `v' = v · M` |
| Composition | **S · R · T** (chaînée depuis l'identité) |
| Translation | **ligne 3** (`m[3][0..2]`) |
| Main (handedness) | **right-handed** (la vue regarde vers −Z) |
| Z du NDC | **[-1, 1]** |
| Angles internes | **radians** ; degrés uniquement aux API publiques |
| Namespace | **`LV3`** ; macros préfixées `LV3_` |
| Quaternion → matrice | forme **vecteur-ligne** (`QuaternionLib::ToMatrix44`) |

> **Vérification de cohérence right-handed :** la rotation `rotateX(+90°)` envoie `+Y → +Z` (règle de la main droite autour de +X), et `crossProduct` donne `X × Y = +Z`. Rotations et produit vectoriel sont donc droitiers et cohérents.

---

## 3. Architecture des fichiers `Maths/` et décision

Le dépôt contenait **deux familles concurrentes** de types, incluses simultanément par `Ressources/MathTypes.h` — source de collisions de typedefs (`Vec3f`, `Matrix44f`… définis deux fois).

**Décision :**

- **Cœur runtime** = famille **templatée** (`Vectorlib.h`, `MatrixLib.h`, `QuaternionLib.h`), car elle est déjà nativement row-major / S·R·T, et c'est le choix de convention retenu.
- **Couche géométrie conservée** = les fichiers modernes qui n'ont pas d'équivalent : `AABB3d.h`, `Plane.h`, `Frustum.h/.cpp`, `Transform.h/.cpp`.
- **À retirer de `MathTypes.h`** : l'une des deux familles de vecteurs/matrices, pour supprimer la collision de noms.
- **Renommage effectué** : `LV2_*` → `LV3_*` dans `Compiler.h` et propagé à toute la solution.

---

## 4. Les vecteurs — `Vectorlib.h`

Famille templatée `Vec2<T>`, `Vec3<T>`, `Vec4<T>` dans `namespace LV3`. Le `Vec3` est le cœur 3D (points, vecteurs, normales).

**Principes clés**

- **Point vs Vecteur** : un point se translate (w=1), une direction non (w=0). Cette distinction se joue côté matrice (`multVecMatrix` vs `multDirMatrix`).
- **`crossProduct` right-handed** : `X × Y = +Z`.
- Noms hérités conservés (`dotProduct`, `crossProduct`, `norm`, `Normalized`, `operator[]`) car `MatrixLib` et `QuaternionLib` en dépendent.

**Standardisation appliquée**

- Scoping `namespace LV3` (fin de la pollution globale et des collisions).
- Fabriques `Zero/One/Up/Down/Left/Right/Forward/Back` **corrigées** (l'ancienne syntaxe `Vec3<T>return{…}` ne compilait pas).
- `noexcept` partout ; `constexpr` partout où c'est licite (sauf `length`/`normalize`/`angleTo` à cause de `std::sqrt`/`std::acos`).
- Ajouts géométriques : `reflect`, `projectOnto`, `distanceTo`, `angleTo`, `Lerp` ; et `dotProduct`/`length`/`norm` sur `Vec4` (absents avant), plus `Vec4(Vec3, w)`.
- `getAngle(const Vec3&)` (qui renvoyait `max(dot,0)`, ce qui n'était pas un angle) remplacé par `angleTo()` ; `std::min/max` qualifiés.

---

## 5. Les matrices — `MatrixLib.h`

Matrice 4×4 templatée `Matrix44<T>`, **row-major**, **vecteur-ligne**, **S·R·T**, translation en **ligne 3**.

### 5.1 Une 4×4 affine = deux choses rangées ensemble

```
┌─────────────┬──────┐
│  bloc 3×3   │  0   │   ← partie linéaire : rotation / scale / cisaillement
│ (lin.)      │  0   │
│             │  0   │
├─────────────┼──────┤
│ translation │  1   │   ← ligne 3 : déplacement
└─────────────┴──────┘
```

On reste **en 4×4 partout**. La « 3×3 » n'est pas un autre type : c'est le coin supérieur gauche, utilisé automatiquement selon le cas :

- **Point** (w=1) → `multVecMatrix` : bloc 3×3 **+ translation**, puis division homogène par `w`.
- **Direction / Normale** (w=0) → `multDirMatrix` : bloc 3×3 **seul**, translation ignorée.

### 5.2 Composition S·R·T

```cpp
Matrix44f M;                          // identité
M.scale(s).rotateX(a).translate(t);   // = S · R · T (lecture gauche→droite)
```

Les builders d'instance post-multiplient (`*this = *this * X`). Les rotations sont right-handed (`rotateX : +Y→+Z`, etc.).

### 5.3 Matrices caméra (row-major, RH)

```cpp
Matrix44f view = Matrix44f::LookAt(eye, target, up);          // vue regarde vers -Z
Matrix44f proj = Matrix44f::Perspective(fovRad, aspect, n, f);// z NDC ∈ [-1,1]
Matrix44f orth = Matrix44f::Orthographic(l, r, b, t, n, f);
```

> **Piège de l'identité :** le constructeur par défaut initialise à l'identité. Pour `LookAt`/`Orthographic`, c'est parfait. Mais une matrice de **projection** a `m[3][3] = 0` (pas 1) : `Perspective` remet donc explicitement la matrice à **zéro** avant de la remplir. Un `1` parasite en `[3][3]` fausserait la division homogène.

> **Où vit la latéralité :** dans `LookAt` (ordre `f×up` puis `r×f`) et dans `Perspective` (`m[2][3] = -1` qui recopie `-z` dans `w`). C'est ce qui ancre le right-handed de bout en bout.

Pipeline : `M_finale = M_model · M_view · M_proj`, puis `v_clip = v_monde · M_finale`, et la division par `w` donne le NDC.

### 5.4 Normales sous scale non-uniforme — sans quitter la 4×4

Avec un scale non-uniforme, transformer une normale comme la géométrie la fait pencher → éclairage faux. Correction générale, **100 % en 4×4** :

```cpp
Matrix44f normalMat = model.inverse().transposed();  // inverse-transposée
Vec3f nWorld;
normalMat.multDirMatrix(nLocal, nWorld);              // 3×3 seul (w=0)
nWorld.normalize();
```

> Tant qu'on n'a **que** rotations + scale uniforme, `model.multDirMatrix(...)` direct suffit. L'`inverse().transposed()` est l'assurance « correcte en toutes circonstances ». Une `Mat3` comme type séparé n'est qu'une **optimisation tardive** (9 floats vs 16) — non nécessaire aujourd'hui.

### 5.5 Standardisation appliquée

- Scoping `namespace LV3`, `noexcept`, `data()`, `Identity()`.
- Code mort retiré : `<immintrin.h>` + SIMD/OpenMP en commentaire, `inverseV2` commentée, `main` de test.
- `multVecMatrix` : garde anti division par zéro (`w == 0`).
- `getAngle<S>()` → `getEulerAngles()` généralisé `Vec3<T>` (utilisait `Vec3f` en dur). **À noter : adapter les éventuels sites d'appel.**
- Constructeur 16 arguments conservé (requis par `QuaternionLib::ToMatrix44`).
- Multiplication `__restrict` conservée (bonne optimisation).

---

## 6. Les quaternions — `QuaternionLib.h`

Quaternion templaté `Quat<T>` dans `namespace LV3`, stocké `r` (scalaire) + `v` (Vec3). Cœur de rotation, sans gimbal lock.

**Principes conservés**

- **`ToMatrix44()`** produit la forme **vecteur-ligne** (`2(xy+wz)` en `[0][1]`) — c'est la transposée de la matrice colonne-vecteur standard, donc cohérente row-major. C'est la seule conversion à utiliser.
- Produit de Hamilton, `Dot`, `Slerp` (repli LERP au seuil `0.9995` + correction anti-podale), ordre d'Euler **Y-X-Z** (Yaw-Pitch-Roll).
- Dépend des noms hérités du vecteur (`dotProduct`/`crossProduct`/`Normalized`), assurés par `Vectorlib.h`.

**Bugs corrigés**

- Constructeur Euler : `Vec3f` codé en dur dans le template (cassait `Quat<double>`) → `Vec3<T>`.
- `Matrix44<T>` utilisé sans inclure `MatrixLib.h` → include ajouté.
- `constexpr` retiré des méthodes à trigo/`sqrt` (Euler, `SetAxisAngle`, `normalize`, `Slerp`) ; conservé sur l'arithmétique pure (`ToMatrix44`, `conjugate`, `inverse`, `rotate`, opérateurs).

**Nettoyage & standardisation**

- `ToMatrix44_OLD()` supprimée (forme colonne-vecteur, incompatible). Commentaire géant condensé, `static` parasite sur `Slerp` retiré. `namespace LV3`, `noexcept`, typedefs `using`.
- Constructeur Euler passé `explicit` (supprime la conversion implicite `Vec3 → Quat`).

**Ajouts (empruntés à `Quat.h`)**

- `conjugate()` et `inverse()` — absents auparavant ; requis pour composer/annuler des rotations.
- `rotate(const Vec3<T>&)` — rotation directe d'un vecteur via `p + 2r(v×p) + 2(v×(v×p))`, right-handed, sans passer par une matrice.
- `Identity()`.

> **`Quat.h` (jeu moderne, `namespace LibV2`)** : redondant, son `ToMatrix()` est colonne-majeur (`m[col][row]`). **À retirer du build.**

---

## 7. La couche géométrie — conservée du jeu moderne

`AABB3d`, `Plane`, `Frustum` (extraction Gribb-Hartmann), `Transform` (TRS + hiérarchie parent) : aucun équivalent dans la famille héritée, **on les garde**.

> **`Transform::ToLocalMatrix` à adapter** à la convention row-major S·R·T :
> ```cpp
> return Scale(scale) * rotation.ToMatrix44() * Translation(position); // S · R · T
> ```

---

## 8. Récapitulatif & reste à faire

```
Type canonique        → famille templatée LV3 (Vectorlib / MatrixLib / QuaternionLib)
Couche géométrie       → fichiers modernes conservés (AABB, Plane, Frustum, Transform)
Convention             → row-major, vecteur-ligne, S·R·T, RH, z NDC [-1,1]
Point   (w=1)          → multVecMatrix (3×3 + translation + ÷w)
Direction/Normale (w=0)→ multDirMatrix (3×3 seul)
Normale + scale n.u.   → model.inverse().transposed() puis multDirMatrix
```

**À faire côté projet :**
1. Retirer une des deux familles de `Ressources/MathTypes.h` (supprime la collision `Vec3f`/`Matrix44f`).
2. Intégrer les trois fichiers standardisés (`Vectorlib.h`, `MatrixLib.h`, `QuaternionLib.h`) — déjà en `namespace LV3`, code mort retiré, `ToMatrix44_OLD()` supprimée.
3. Retirer du build les fichiers modernes redondants (`Quat.h`, `Vec*f.h`, `Matrix44f.*`) : `namespace LibV2`, colonne-majeur, incompatibles avec le cœur retenu.
4. Adapter `Transform::ToLocalMatrix` en S·R·T row-major.
5. Mettre à jour les sites d'appel de l'ancien `getAngle()` → `getEulerAngles()`.

---

## 9. Sur l'horizon (Leçon 03 probable)

Géométrie du rasterizer : coordonnées barycentriques, edge functions, clipping near-plane, transformation viewport, interpolation perspective-correct — la couche qui distingue une lib *de moteur de rendu logiciel* d'une lib maths générique.
