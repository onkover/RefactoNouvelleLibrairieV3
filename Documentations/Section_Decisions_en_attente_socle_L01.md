## Décisions en attente — socle L01 (relevé du 07/11/2026)

**Aucune décision à ce stade : tout ce qui suit reste à acter.**
Origine : artefact « Lot L01 pas à pas — Config, Enums & PCH », section 5 « Écarts entre la leçon et le code d'aujourd'hui » (leçon 01 et points de la leçon 02 qui la touchent, comparés à l'état du dépôt LIB lu en novembre 2026).
La colonne « Nature relevée » décrit ce que l'écart *est* ; ce n'est pas un verdict. La colonne « À décider » dit ce que l'acter ou le refuser engage.

| # | Sujet | Nature relevée | À décider |
|---|-------|----------------|-----------|
| E1 | `config.h` : constantes en `constexpr` dans `LV3` (`M_PI`, `M_PI_2`, `TO_RADIAN`, `INV_PI`…) au lieu de `#define LV3_PI…` | Évolution + dette mineure | Entériner `constexpr` comme forme des constantes du moteur. Renommer ou non `M_PI`, `M_E`… : ces noms sont ceux des macros de `<cmath>` ; un `_USE_MATH_DEFINES` casserait des lignes correctes. |
| E2 | `config.h` : deux familles distinctes, tolérances (`EPSILON`, `EPSILON_FLOAT`) et gardes anti-division (`NEAR_ZERO`, `NEAR_ZERO_SQ`), au lieu d'un seul `LV3_EPSILON` | Évolution (née du bug n°3, leçon 05) | Entériner la séparation tolérance / garde comme règle. |
| E3 | `config.h` : `LV3_ASSERT` appelle `LV3::AssertFailed` (et non `assert`), `sizeof` quand éteint (annexe A10 §8.3) | Évolution | Entériner. Les règles R45 à R47 en découlent. |
| E4 | `config.h` : une douzaine de commutateurs de build vivent dans le PCH (asserts, journaux, mesure, ablations, variantes) | Évolution (née du bug 0.3, ODR) | Préciser D1 : le critère n'est pas seulement « stable » mais « recompilé par le bon nombre de fichiers ». Un commutateur qui change le code de tous les fichiers peut vivre dans le PCH. |
| E5 | `config.h` : `#define LV3_PROFILE 1` alors que le commentaire annonce « Défaut 0 » | Incohérence | Remettre le défaut à 0 (la campagne de mesure passe par `LV3.Common.props`, consolidation C2), ou corriger le commentaire. |
| E6 | `pch.h` : `#error` sur `__cpp_lib_expected` au lieu d'un include conditionné par `__cplusplus` | Évolution | Entériner : une exigence de version se vérifie par sa macro de fonctionnalité et échoue tôt. |
| E7 | `pch.h` : `Maths/MathTypes.h` est entré dans le PCH (la leçon 02 le voulait hors PCH tant qu'il évolue) | À acter | Déclarer `MathTypes.h` figé (toute retouche assume alors un build complet), ou le sortir du PCH pendant un chantier qui le fait évoluer. |
| E8 | `pch.h` : `Maths/Transform.h` et `Core/InputState.h` dans le PCH | Dette | Les sortir du PCH et les inclure là où ils servent, ou les acter dans le PCH. `InputState` grandira avec l'interface ; chaque touche ajoutée reconstruit aujourd'hui tout le moteur. |
| E9 | `Compiler.h` : `alignas(32)` / `alignas(64)`, `LV3_ALIGN_CACHE`, `LV3_NOINLINE` | Évolution | Entériner : une macro n'entre dans `Compiler.h` que si le standard n'a pas d'équivalent. |
| E10 | `Platform.h` : `NOMINMAX`, `NOGDI`, `VC_EXTRALEAN` avant `<windows.h>` | Évolution (née du bug n°13, RGB) | Entériner, en gardant l'isolement de `Platform.h` : `near`, `far`, `NEAR`, `FAR` restent définis. |
| E11 | `pch.h` : commentaire « LibraryV2 », `<limits>` inclus deux fois, casse incohérente des chemins (`"core/Compiler.h"` / `"Core/CoreTypes.h"`) | Dette mineure | Nettoyage. La casse casserait la compilation sur un système de fichiers sensible à la casse. |
| E12 | EXE : `#include <pch.h>` (chevrons) dans `RefactoNouvelleLibrairieV3.cpp`, d'après un extrait du dossier projet | À vérifier d'abord (l'extrait peut être antérieur au dépôt actuel) | Si confirmé : passer à `#include "pch.h"`. |
| E13 | Extension des headers : la leçon 02 écrit `.hpp` (`Scalar.hpp`, `Vec2.hpp`, `Mat4.hpp`…), le dépôt utilise `.h` (`Vec2f.h`, `Vec3f.h`, `AABB3d.h`, `MathTypes.h`) ; seul `json.hpp` est en `.hpp` | À acter | `.h` partout (état du dépôt, D9 proposée) ou `.hpp` pour le C++ pur (renommer les headers et leurs includes ; chaque oubli sort en C1083). Si `.h` est acté, corriger la liste de fichiers de la leçon 02. |

**Décisions de lot proposées, non actées** (artefact L01, section 4) :
- **D8** : guillemets pour le dépôt, chevrons pour la STL, le SDK et les dépendances externes ; `"pch.h"` en première ligne de chaque `.cpp`. Se tranche avec **E12**.
- **D9** : une seule extension, `.h`. Se tranche avec **E13**.

**Ordre de lecture suggéré (sans engagement)** : E12 (vérification, sans coût), puis E5 (incohérence), puis E7 et E8 (ce qui entre dans le PCH), puis E13 et E1 (conventions de nommage), le reste étant des évolutions déjà en place à entériner.
