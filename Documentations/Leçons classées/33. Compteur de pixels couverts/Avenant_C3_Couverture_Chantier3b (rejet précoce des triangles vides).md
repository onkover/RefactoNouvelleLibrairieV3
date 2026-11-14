# Avenant C3 — Compteur de couverture et chantier 3b : rejet précoce des triangles vides

> **Statut** : clos le 28/09/2026.
> **Rattachement** : consolidation C2 (`Consolidation_C2_Campagne_Mesure.md`) § 9 point 3, et avenant C2 (`Avenant_C2_Chantier2_PreTransformation.md`) § 8.
> **Portée** : LibraryV3 (LIB) + RefactoNouvelleLibrairieV3 (EXE), `Release` et `RelWithAsserts`, x64.
> **Machine** : i7-1265U (portable, Alder Lake hybride), secteur, performances élevées. Chronométrages épinglés sur P-core (`affinity=4`).
> **Nature** : ce document fige l'instrument de couverture, la règle de décision, les chiffres, le code du chantier 3b, les erreurs de méthode rencontrées et les règles qui en sortent. Il **amende** C2 § 7.1 et l'avenant C2 § 5.

---

## 1. La question posée

L'avenant C2 laissait deux candidats pour l'étape suivante :

- **2b** : faire la division par w et le `ToRaster` une fois par sommet plutôt qu'une fois par coin ;
- **3** : rejeter tôt les triangles sub-pixel.

Pour choisir, il fallait savoir **combien de pixels couvre un triangle de la ceinture**. Aucun compteur ne le disait (C2 § 9, point 3).

---

## 2. Une découverte faite à la relecture, avant toute mesure

```cpp
int minX = int(std::floor(std::min({ p0.x, p1.x, p2.x })));
int maxX = int(std::ceil (std::max({ p0.x, p1.x, p2.x }))) + 1;
```

Un pixel `x` est couvert si son **centre** `x + ½` est dans le triangle. Les seuls candidats possibles vont donc de `ceil(min − ½)` à `floor(max − ½)` inclus : c'est la **boîte serrée**. La boîte actuelle, dite **lâche**, est plus large d'une à trois colonnes et d'une à trois lignes. Par construction, elle fait **au moins 2×2**, même pour un triangle qui ne couvre rien.

Simulation préalable (20 000 triangles aléatoires, même algorithme, même règle top-left, inclusion de la boîte serrée vérifiée par assertion) :

| Demi-étendue des sommets | Tests par triangle (boîte lâche) | Boîte serrée | Pixels couverts | Triangles à 0 px |
|---|---|---|---|---|
| 0,5 px | 6,2 | 0,25 | 0,08 | 92 % |
| 1 px | 9,0 | 0,99 | 0,31 | 71 % |
| 2 px | 16,0 | 3,97 | 1,22 | 35 % |
| 4 px | 36,2 | 16,1 | 4,97 | 12 % |

Le chantier 3 se scindait alors en deux :

- **3a** : resserrer la boîte dans `RasterizeTriangle` ;
- **3b** : ne pas envoyer du tout au rasterizer un triangle qui ne peut rien couvrir.

---

## 3. L'instrument

### 3.1 Un commutateur dédié : `LV3_RASTER_STATS`

C'est le septième commutateur de `Core/config.h`, défaut `0`. Deux `#error` interdisent les combinaisons absurdes :

- `LV3_RASTER_STATS` sans `LV3_PROFILE` : les compteurs n'iraient nulle part ;
- `LV3_RASTER_STATS` avec `LV3_ABLATION_RASTER` : on compterait des zéros.

**Pourquoi pas sous `LV3_PROFILE` ?** Ce compteur vit dans la boucle la plus chaude du moteur. Placé sous `LV3_PROFILE`, il aurait alourdi tous les chronométrages futurs, et la Référence F serait devenue incomparable.

### 3.2 Huit compteurs, ajoutés en fin de `EProfCounter`

| Compteur | Sens |
|---|---|
| `TrisCov0`, `TrisCov1`, `TrisCov2to4`, `TrisCov5plus` | distribution des pixels couverts par triangle |
| `TrisTightEmpty` | boîte serrée vide : triangle rejetable en O(1) |
| `PixelsTested` | tours de la boucle pixel actuelle (boîte lâche) |
| `PixelsTight` | aire de la boîte serrée |
| `PixelsCovered` | pixels passant les trois fonctions d'arête, **avant** le test de profondeur |

Ils sont ajoutés en fin de liste, pour que les colonnes des CSV versionnés ne bougent pas. Le `static_assert` sur les noms a protégé l'ajout.

### 3.3 Implantation dans `RasterizeTriangle`

- Le compteur de pixels est une variable **locale**, donc un registre. Il n'y a **qu'un seul** enregistrement par triangle, au travers de `RecordRasterStats`.
- **Chaque sortie** de la fonction enregistre le triangle : aire nulle, boîte vide après rognage, fin normale. Sans cela, l'invariant Σ tranches = `TrisRasterized` casse.
- La boîte serrée est **mesurée, jamais utilisée** : la boucle reste identique au caractère près.
- Dans la boucle, `LV3_ASSERT(x ∈ boîte serrée)` sur chaque pixel couvert. C'est la preuve, sur données réelles, que la boîte serrée contient tout pixel couvert.
- La macro désactivée est `((void)0)` et non `((void)sizeof(x))` : ses arguments n'existent pas quand le commutateur vaut 0.
- Le champ `rasterstats=` de l'en-tête CSV est écrit par `Profiler.cpp`, donc par la **LIB**, celle qui compte (§ 9, M11).

### 3.4 Contre-exemple

```cpp
// ✗ NE PAS FAIRE
if (inside) {
    LV3_PROF_COUNT(EProfCounter::PixelsCovered, 1);   // appel hors ligne + branche PAR PIXEL
    onFragment(x, y, bary, userData);
}
```

Ce code a trois défauts :

- environ un million d'appels par frame dans la boucle chaude, donc une mesure perturbée ;
- sous `LV3_PROFILE`, il contaminerait **tous** les runs de chronométrage ;
- placé dans `ShadeFragment_*`, il compterait les pixels qui passent le test de profondeur. C'est une question d'**overdraw**, pas de couverture.

---

## 4. La règle de décision, et son application

### 4.1 Écrite avant la mesure

Grandeurs mesurées :

- ρ = `PixelsTested / TrisRasterized` ;
- τ = `PixelsTight / PixelsTested` ;
- f₀ = `TrisCov0 / TrisRasterized`.

Règle :

1. si τ ≤ 0,5 **et** ρ ≥ 6 → chantier **3a** ;
2. sinon, si f₀ ≥ 0,4 → chantier **3b** ;
3. sinon → chantier **2b**.

**Prédiction** (triangles supposés d'environ 1 px de demi-étendue) : ρ ≈ 9, τ ≈ 0,1, f₀ ≈ 70 %, donc règle 1.

### 4.2 Les chiffres (runs de comptage, temps non recevables)

| Run | ρ | τ | f₀ | Boîte serrée vide |
|---|---|---|---|---|
| **Ceinture, 4 vues, 1536×900** | **4,35** | **0,018** | **99,84 %** | 99,71 % |
| Ceinture, 4 vues, 768×450 | 4,08 | 0,005 | 99,91 % | 99,81 % |
| v1compat, 2 vues, 768×450 | 155 | 0,87 | 20 % | 6 % |

Sur la ceinture en 1536×900 :

- 134 389 triangles envoyés au rasterizer couvrent **4 562 pixels** au total ;
- 134 174 de ces triangles (99,8 %) ne couvrent **aucun** pixel ;
- un astéroïde fait environ 3,5 pixels à l'écran et il est dessiné avec une centaine de triangles.

**Témoins :**

- `FacesSubmitted`, `TrisRasterized` et `VertsTransformed` sont identiques à la Référence F ;
- `PixelsCovered` passe de 4 562 à 1 177 entre 1536×900 et 768×450, un rapport de 3,88 pour un rapport attendu de 4 ;
- trois runs `Release` et un run `RelWithAsserts` donnent des compteurs identiques au chiffre près.

### 4.3 Application

- **Règle 1 : elle échoue.** τ passe très largement, mais ρ = 4,35 < 6.
- **Règle 2 : f₀ = 0,998, on ouvre le chantier 3b.**

**Erreur de calibrage reconnue.** Le seuil ρ ≥ 6 supposait des triangles d'environ 1 px. Les triangles réels sont environ trois fois plus petits. La boîte lâche tombe alors à son plancher de 4, et ρ ne peut plus atteindre 6. On n'a pas réécrit la règle après coup. Elle a désigné le bon chantier : pour un triangle qui ne couvre rien, la meilleure boucle, lâche ou serrée, est celle qu'on n'exécute pas.

---

## 5. Ce que les compteurs réinterprètent

### 5.1 L'expérience C de C2 est expliquée (amende C2 § 7.1)

C2 concluait : « le coût de la rasterisation n'est pas dans les pixels, il est dans la préparation de chaque triangle ». En réalité :

- entre 1536×900 et 768×450, `PixelsTested` ne baisse que de **6 %** (584 545 → 548 346) alors que l'écran compte 4 fois moins de pixels ;
- le coût était celui du **plancher de la boîte lâche** : environ 4 tests par micro-triangle, quelle que soit la résolution.

C'est ce qui rendait `Render` insensible à la résolution.

### 5.2 L'avenant C2 § 5 est réfuté sur la cause, pas sur le mécanisme

L'avenant attribuait les attentes de la rasterisation au depth buffer de 5,5 Mo. Or il n'y a que **4 562 fragments par frame, toutes vues confondues** : le depth buffer n'est presque jamais lu.

- Le recouvrement out-of-order reste compatible avec toutes les mesures.
- **Hypothèse de remplacement** : les erreurs de prédiction de branche. Environ 584 000 tests de pixel par frame, avec des `continue` dont l'issue dépend des données, et chaque erreur vide le pipeline pendant 15 à 20 cycles.
- **Preuve possible** : Intel VTune, catégorie *Bad Speculation*. Non faite.

---

## 6. Le prérequis : preuve de la boîte serrée en `RelWithAsserts`

Le rejet précoce repose sur une seule implication : **boîte serrée vide ⇒ aucun pixel couvert**. L'assertion de la boucle vérifie la contraposée.

**Deux runs refusés** avant le bon :

- les deux en-têtes affichaient `config=Release` ;
- l'exécutable lancé était celui de `x64\Release\`, parce que la recette C2 § 10 contient `cd <racine>\x64\Release` en dur ;
- la binaire s'est dénoncée elle-même par sa métadonnée, conformément à M2 et M6.

**Run recevable** (`config=RelWithAsserts`) :

- **Aucune assertion n'a échoué.** `AssertFailed` appelle `std::abort()`, donc un CSV de 300 frames écrit prouve qu'il n'y a eu aucune violation. Environ 1,4 million de pixels couverts ont été testés contre la boîte serrée.
- **Les compteurs sont identiques** aux runs `Release` : les assertions observent sans perturber.

**Angle mort identifié.** `config=` décrit l'**EXE**, alors que l'assertion vit dans la **LIB**. Le run a été accepté sur un argument structurel : le `.slnx` ne contient aucune association par projet, donc les deux projets sont générés sous le même nom de configuration. Le champ `asserts_lib=` a été ajouté pour que ce point soit désormais prouvé par le fichier (§ 9, M11).

---

## 7. Le code livré

### 7.1 `Rendering/Rasterizer.h` : source unique de la boîte serrée

```cpp
struct PixelBox
{
    int32_t x0, y0, x1, y1;                       // x1 / y1 exclusifs
    [[nodiscard]] bool     Empty() const noexcept { return x0 >= x1 || y0 >= y1; }
    [[nodiscard]] uint32_t Area()  const noexcept
    { return Empty() ? 0u : uint32_t(x1 - x0) * uint32_t(y1 - y0); }
};

template<typename V>
[[nodiscard]] LV3_FORCEINLINE PixelBox TightPixelBox(const V& a, const V& b, const V& c,
                                                     const Viewport& vp) noexcept
{
    PixelBox bx{
        int32_t(std::ceil (std::min({ a.x, b.x, c.x }) - 0.5f)),
        int32_t(std::ceil (std::min({ a.y, b.y, c.y }) - 0.5f)),
        int32_t(std::floor(std::max({ a.x, b.x, c.x }) - 0.5f)) + 1,
        int32_t(std::floor(std::max({ a.y, b.y, c.y }) - 0.5f)) + 1 };
    vp.ClampBox(bx.x0, bx.y0, bx.x1, bx.y1);
    return bx;
}
```

Le rejet et les statistiques utilisent **la même fonction**. Deux copies de la formule finiraient par diverger, et la mesure ne parlerait plus du même objet que le rejet.

### 7.2 `Scene/RenderSystem.cpp` : `EmitClipTriangle`

```cpp
if (IsBackFacing(EdgeFunction(r[0], r[1], r[2]))) return;

LV3_PROF_COUNT(EProfCounter::TrisRasterized, 1);   // APRES backface, AVANT rejet : sens de la Réf. F

if (view.mode != ERenderMode::Wireframe &&
    TightPixelBox(r[0], r[1], r[2], view.viewport).Empty())
{
    LV3_PROF_COUNT(EProfCounter::TrisEarlyRejected, 1);
    return;
}
```

- **Exception `Wireframe` obligatoire.** Bresenham trace le contour même d'un triangle qui ne contient aucun centre de pixel. Dans ce mode, le rejet ne serait plus exact.
- **`TrisRasterized` garde son sens de la Référence F** : il compte les triangles après backface et avant rejet. Les runs restent donc comparables.
- **Nouvel invariant** : Σ `TrisCov*` + `TrisEarlyRejected` = `TrisRasterized`.

### 7.3 Contre-exemple : rejeter sur l'aire

```cpp
// ✗ NE PAS FAIRE
if (std::fabs(EdgeFunction(r[0], r[1], r[2])) < 2.0f) return;   // « moins d'un pixel² »
```

C'est une **approximation déguisée en optimisation**. Un triangle d'aire 0,3 px² peut contenir un centre de pixel, et une aiguille d'aire quasi nulle peut couvrir plusieurs pixels. Le résultat serait des trous dans les astéroïdes, qui varieraient avec la distance. La boîte serrée est un critère **exact**.

---

## 8. Vérifications

### 8.1 Série A : exactitude

Run de comptage en `RelWithAsserts`, ceinture, 4 vues, 1536×900.

| Contrôle | Résultat |
|---|---|
| Σ `TrisCov*` + `TrisEarlyRejected` = `TrisRasterized` | **300 frames sur 300**, écart nul, chauffe comprise |
| `TrisTightEmpty` | 0 sur toutes les frames |
| `TrisEarlyRejected` (médiane / p95 / max) | 134 003 / 135 207 / 135 830, **égal** au `TrisTightEmpty` d'avant |
| `TrisCov1` / `TrisCov2to4` / `TrisCov5plus` | 131 / 50 / 32, inchangés |
| **`PixelsCovered`** | 4 562 / 4 573 / 53 668, **inchangé** : aucun fragment perdu |
| `PixelsTight` | 10 562, inchangé (les triangles rejetés avaient une boîte d'aire nulle) |
| `PixelsTested` | 584 545 → **14 372** (−97,5 %) |

**Erreur de prédiction reconnue.** J'avais annoncé `TrisCov0` = 171, calculé comme 134 174 − 134 003, une **différence de médianes**. Le run donne 166. La médiane d'une différence n'est pas la différence des médianes, comme pour la ligne `RESTE` de C2. La seule vérification valable est l'invariant frame par frame.

### 8.2 Série B : gain

Protocole :

- deux binaires `Release`, avec `LV3Profile=true`, `LV3_RASTER_STATS 0`, `LV3_ABLATION_RASTER 0` ;
- A = LIB `6bc1813` + EXE `e5bbaa7` (Référence F) ; B = code avec le chantier 3b ;
- `start "" /wait /affinity 4`, chemins d'exécutable écrits dans le script, un run jetable puis **A B B A**.

**Recevabilité :**

| | A1 | B1 | B2 | A2 |
|---|---|---|---|---|
| Identité du binaire | pas de `rasterstats=` | `rasterstats=0`, `asserts_lib=0` | idem B1 | idem A1 |
| Témoin T (µs) | 94,6 | 96,6 | 96,7 | 96,2 |
| p95 ÷ médiane de `Render` | 1,067 | 1,051 | 1,051 | 1,062 |

L'écart de T au sein de chaque paire vaut 2,1 % et 0,5 %, sous le seuil de 3 %. `FacesSubmitted` / `TrisRasterized` sont identiques partout (274 988 / 134 389).

**Résultat :**

| | A | B | B ÷ A |
|---|---|---|---|
| Render, paire 1 | 10,075 ms | 7,277 ms | 0,722 |
| Render, paire 2 | 10,614 ms | 7,410 ms | 0,698 |
| **Render, ABBA combiné** | 20,690 ms | 14,687 ms | **0,710, soit −29,0 %, environ −3,0 ms** |
| FRAME, ABBA combiné | 25,03 ms | 19,04 ms | 0,761, soit −24 % (≈ 80 → 105 fps) |

**La Référence F est confirmée.** A1 donne `Render` = 10,08 ms et T = 94,6 µs, contre 10,19 ms et 93,6 µs dans l'avenant C2. Le gain du chantier 2 ne repose plus sur une seule paire.

### 8.3 Lecture du résultat

**1. La prédiction est manquée d'un point, et la règle avait un trou.**

- La règle disait « ≥ 30 % : succès ; < 15 % : mécanisme mal compris ». La zone entre 15 et 30 % n'était pas définie.
- On mesure −29,0 %. Les deux paires s'écartent de 3,4 % entre elles, contre ±0,4 % dans l'avenant C2 : A a dérivé de +5,4 % entre A1 et A2, B seulement de +1,8 %.
- −29 % et −30 % ne se distinguent pas avec ces données.
- **Verdict : succès, avec une réserve de précision.**

**2. La prédiction reposait sur une soustraction interdite.**

- Les « 60 à 65 % de rasterisation » venaient de complet − ablation, ce que M7 amendée interdit.
- Le gain réel (3,0 ms) vaut environ la moitié des 6,3 ms que la soustraction laissait espérer. Trois raisons :
  - un triangle rejeté paie toujours sa division par w, son `ToRaster` et son backface ;
  - le test de boîte serrée a son propre coût ;
  - une partie de la boucle pixel était déjà masquée par l'exécution out-of-order.

**3. La loi de coût change.**

- `Render` ≈ **26,5 ns × `FacesSubmitted`**, contre environ 37 ns à la Référence F.
- Le rasterizer ne reçoit plus que 386 triangles par frame. Tout le coût restant est **en amont**.

---

## 9. Règles de mesure : mises à jour de la série M

- **M2 complétée.** L'en-tête porte aussi `rasterstats=` et `asserts_lib=`, écrits par la LIB.
- **M10 (nouvelle).** Un compteur placé dans une boucle chaude a **son propre commutateur**. Un run qui l'active est un **run de comptage** : ses compteurs sont recevables, ses temps ne le sont pas.
- **M11 (nouvelle).** **Celui qui compte, ou qui vérifie, déclare.** Un champ de métadonnée est écrit par l'unité de compilation qui porte la chose décrite : `rasterstats=` et `asserts_lib=` par la LIB, pas par l'EXE. `config=` ne décrit que l'EXE.
- **M12 (nouvelle).** Une règle de décision et une prédiction s'écrivent **avant** la mesure, avec **toutes les zones définies**, zones intermédiaires comprises. On ne réécrit jamais la règle après avoir vu les chiffres. Un invariant se vérifie **frame par frame**, jamais sur des médianes, et une prédiction ne se calcule jamais par différence ou somme de médianes.
- **M13 (nouvelle).** **On lit l'en-tête avant le premier chiffre.** Une recette de lancement ne contient aucun chemin de configuration implicite : les exécutables A et B sont désignés **dans la commande** (`set EXE_A=…`, `start "" /wait /affinity 4 "%EXE_A%" "%ROOT%" …`), et chaque binaire doit pouvoir être identifié par son propre CSV.

Pour les binaires en ABBA, la bonne pratique retenue est la suivante :

- figer B par un commit ;
- construire A depuis un commit identifié, par `git checkout` du commit puis **Régénérer la solution** ;
- copier chaque dossier `x64\Release\` dans un dossier de banc ;
- ne pas utiliser `git worktree`, puisque `LV3.User.props` contient un chemin absolu vers la LIB.

---

## 10. Référence G : nouveau point de départ

| Référence G | Valeur |
|---|---|
| Machine | i7-1265U, **P-core épinglé** (`affinity=4`), secteur, performances élevées |
| Code | chantiers 2 et 3b intégrés |
| Render | **7,28 à 7,41 ms** (771 ‰) |
| FRAME | **9,43 à 9,61 ms** (≈ 105 fps) |
| Témoin calcul T | ≈ 96,6 µs |
| Témoin mémoire Clear | ≈ 1,08 à 1,11 ms |
| Loi de coût | ≈ **26,5 ns par face soumise** |
| Triangles réellement rasterisés | ≈ 386 par frame |

---

## 11. Conséquences sur le plan d'optimisation

| # | Chantier | État / révision |
|---|---|---|
| 2 | Pré-transformation des sommets | clos (avenant C2) |
| **3b** | Rejet précoce des triangles vides | **CLOS** : −29 % sur `Render`, image identique au fragment près |
| 3a | Boîte serrée dans la boucle pixel | **abandonné pour la ceinture** : `PixelsTested` est déjà passé de 584 545 à 14 372 ; sur v1compat, τ = 0,87 |
| **2b** | Division par w + `ToRaster` par sommet | **remonte** : c'est le coût dominant de chaque triangle rejeté |
| **1** | LOD | **premier sur le fond** : 99,7 % des triangles de la ceinture ne produisent rien, et c'est le seul chantier qui supprime aussi l'amont |
| 4 | AVX2 sur `TransformPositions` | inchangé, rétrogradé |

Le choix entre 2b et 1 se fera sur une nouvelle mesure, avec une règle écrite avant (M12).

---

## 12. Hygiène

- Remettre `LV3Profile` à `false` dans `LV3.Common.props`, régénérer, et vérifier `LV3_PROFILE(exe)=0` dans la ligne `[Build]`.
- Versionner sous `Mesures/` les CSV de couverture et les résumés `3b_A1`, `3b_B1`, `3b_B2`, `3b_A2`.
- Réécrire la recette de C2 § 10 selon M13 : `x64\<Config>`, vérification de la ligne `[Build]`, exécutables désignés dans la commande.
- Supprimer `C:\Bench\` et les anciennes copies de travail `_avant` du chantier 2.
- `ablation=` est toujours écrit par l'EXE (`main.cpp`) alors que l'ablation a lieu dans la LIB : à aligner sur M11 à la prochaine occasion.

---

*Visuel associé : « Compteur de couverture LV3 » (artifact interactif : boîte lâche, boîte serrée, pixels couverts, simulation de population et règle de décision).*
