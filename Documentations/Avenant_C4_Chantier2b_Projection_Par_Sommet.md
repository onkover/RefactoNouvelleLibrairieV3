# Avenant C4 — Chantier 2b : la projection une fois par sommet

> **Statut** : clos le 28/09/2026.
> **Rattachement** : avenant C3 (`Avenant_C3_Couverture_Chantier3b.md`) § 11 ; avenant C2 § 8 (première mention de l'étape 2b) ; annexe A12 § 5 et § 6.
> **Portée** : LibraryV3 (LIB) + RefactoNouvelleLibrairieV3 (EXE), `Release` et `RelWithAsserts`, x64.
> **Machine** : i7-1265U (portable, Alder Lake hybride), secteur, performances élevées. Chronométrages épinglés sur P-core (`affinity=4`).
> **Nature** : ce document compare l'ancien système au nouveau, décrit le fonctionnement du nouveau, le situe dans le pipeline, et fige la règle de décision, les chiffres, les erreurs de méthode et les règles qui en sortent. Il **amende** l'annexe A12 (voir l'avenant A12a) et le plan d'optimisation de C3 § 11.

---

## 1. La question posée

L'avenant C3 laissait deux candidats : le **2b** (baisser le prix d'une face) et le **LOD** (baisser leur nombre). Une règle de décision chiffrée avait été préparée pour les départager.

Elle a été abandonnée **avant toute mesure**, par une décision de conception : le 2b est un passage obligatoire quel que soit le résultat, parce qu'il est exact (image identique au bit près), local, et qu'il baisse le prix de **chaque** face, y compris celles qu'un LOD conservera. L'ordre retenu est donc : **2b, puis LOD**.

L'instrument de taille apparente (`LV3_LOD_STATS`, commit LIB `741952a`) est conservé : il servira à **dimensionner** le LOD, plus à choisir entre les deux chantiers.

---

## 2. L'ancien système et le nouveau

### 2.1 Ce qui a changé, en une phrase

> La division par w et la conversion en pixels (`ToRaster`) se faisaient **une fois par coin de triangle**. Pour les meshes `Inside`, elles se font désormais **une fois par sommet**, dans l'étage sommets, avant la boucle des faces.

### 2.2 Le flux de données, avant et après

**Avant (Référence G), pour toutes les instances :**

```
positions ──► TransformPositions ──► ClipSpaceBuffer (Vec4f, espace clip)
              MulRow × nVerts                │
                                             ▼
               boucle des FACES : cv[k] = clip[indices[..]]      (copie 3 × 16 o)
                                  tri[3] = { cv[0], cv[t+1], cv[t+2] } (copie 48 o)
                                             │
                                             ▼
               EmitClipTriangle (appel non inline, par triangle)
                  pour k = 0..2 : invW = 1 / w          ◄── DIVISION PAR COIN
                                  r[k] = ToRaster(clip × invW)
                  backface → TrisRasterized → rejet 3b → DrawTriangle
```

**Après (chantier 2b) : deux chemins, choisis une fois par (instance, vue) :**

```
                       ┌── Inside (97,6 % des faces) ─────────────────────────────────┐
positions ──► tri ─────┤  ProjectPositions : MulRow + 1/w + ToRaster × nVerts         │
  par instance         │        ──► RasterSpaceBuffer (RasterVertex, pixels)          │
                       │  boucle des FACES : lit 3 RasterVertex, aucune division ─────┼──► EmitRasterTriangle
                       │                                                              │    backface
                       └── Intersect (2,4 %) ─────────────────────────────────────────┤    TrisRasterized
                          TransformPositions ──► ClipSpaceBuffer                      │    rejet 3b
                          boucle des FACES : test near, ClipTriangleNear              │    DrawTriangle
                          EmitClipTriangle : ProjectClip × 3 (par coin, APRÈS clip) ──┘
```

### 2.3 Comparaison chiffrée, pour une roche `Inside` (117 sommets, 168 faces), par vue

| Opération | Avant | Après | Rapport |
|---|---|---|---|
| `MulRow` | 117 | 117 | inchangé (chantier 2) |
| Divisions par w | **504** (3 × 168) | **117** | ÷ 4,3 |
| `ToRaster` | **504** | **117** | ÷ 4,3 |
| Copies de `ClipVertex` dans la boucle des faces | 168 × (3 + 3) × 16 o ≈ 16 Kio | 0 | supprimées |
| Appels de fonction non inline par triangle | 168 (`EmitClipTriangle`) | 0 (`EmitRasterTriangle` est `LV3_FORCEINLINE`) | supprimés |
| Divisions sur le chemin critique d'un branchement | 3 par triangle | 0 | voir § 8 |
| Mémoire lue par la boucle des faces | `Vec4f` clip, 16 o par sommet | `RasterVertex`, 16 o par sommet | identique |

### 2.4 Comparaison sur la frame (ceinture, 4 vues, 1536×900)

| Grandeur | Avant | Après | Source |
|---|---|---|---|
| `FacesSubmitted` | 274 988 | 274 988 | mesuré, identique frame par frame |
| `FacesInside` (chemin par sommet) | — | **269 092** (η = 0,976) | mesuré |
| Divisions par w par frame | ≈ 825 000 (3 × faces émises) | ≈ 195 000 | **estimé** : sommets `Inside` + 3 × faces `Intersect` ; `VertsInside` n'a pas été relevé dans ce run |
| `TrisRasterized` / `TrisEarlyRejected` | 134 389 / 134 003 | identiques | mesuré |
| `PixelsCovered` | 4 562 / 4 573 / 53 668 | identiques | mesuré |
| `Render` | 7,49 à 7,99 ms | **5,14 à 5,16 ms** | ABBA, § 7.3 |
| Coût par face soumise | 27,1 ns | **18,7 ns** | mesuré |

### 2.5 Le code, avant et après

**Avant**, `EmitClipTriangle` faisait tout, pour tous les triangles :

```cpp
static void EmitClipTriangle(Renderer& renderer, const ViewData& view,
    const ClipVertex& a, const ClipVertex& b, const ClipVertex& c, Color col)
{
    const ClipVertex* v[3] = { &a, &b, &c };
    Vec3f r[3]; float invW[3];
    for (int k = 0; k < 3; ++k)
    {
        invW[k] = 1.0f / v[k]->clip.w;                       // division PAR COIN
        r[k] = view.viewport.ToRaster({ v[k]->clip.x * invW[k],
                                        v[k]->clip.y * invW[k],
                                        v[k]->clip.z * invW[k] });
    }
    if (IsBackFacing(EdgeFunction(r[0], r[1], r[2]))) return;
    // ... TrisRasterized, rejet 3b, ablation, DrawTriangle
}
```

**Après**, la projection a une source unique, et l'aval est partagé par les deux chemins :

```cpp
// VertexStage.h — LE seul endroit du moteur où l'on divise par w
[[nodiscard]] LV3_FORCEINLINE RasterVertex ProjectClip(const Vec4f& c, const Viewport& vp) noexcept
{
    const float invW = 1.0f / c.w;
    const Vec3f r = vp.ToRaster({ c.x * invW, c.y * invW, c.z * invW });
    return { r.x, r.y, r.z, invW };
}

// RenderSystem.cpp — chemin Inside : les faces ne font que LIRE
ProjectPositions(mvp, view.viewport, mesh->vertexPositions.data(), nVerts, rasterBuf.Data());
for (size_t f = 0; f < nFaces; ++f)
    for (uint8_t t = 0; t + 2 < vpf; ++t)
        EmitRasterTriangle(renderer, view, rv[idx[0]], rv[idx[t + 1]], rv[idx[t + 2]], col);

// RenderSystem.cpp — chemin Intersect : projection par coin, APRÈS le clipping
EmitRasterTriangle(renderer, view, ProjectClip(a.clip, vp), ProjectClip(b.clip, vp),
                                   ProjectClip(c.clip, vp), col);
```

---

## 3. Comment fonctionne le nouveau système

### 3.1 Le principe : la règle de fréquence (A12 § 1)

> Un calcul s'exécute au niveau le **moins fréquent** où son résultat est encore constant.

La projection d'un sommet ne dépend que du sommet, de `mvp` et du viewport : elle est constante pour un sommet, dans une (instance, vue). Sa place est donc la bande **sommet**, pas la bande **triangle**. Le chantier 2 y avait fait remonter `MulRow` ; le 2b y fait remonter la suite, jusqu'aux pixels.

### 3.2 La condition : w > 0 pour chaque sommet

Diviser par w n'a de sens que si w > 0. Un sommet derrière l'œil (w < 0) serait projeté **de l'autre côté de l'écran**, inversé. C'est la raison d'être du clipping homogène (Blinn-Newell) : on coupe en espace clip, puis on divise.

La garantie vient du **frustum culling par instance**, qui existait déjà :

- **`Inside`** : l'AABB monde de l'instance est entièrement dans les 6 plans, donc devant le near. Chaque sommet est dans l'AABB, donc **w ≥ near > 0** en perspective, et w = 1 en ortho. La division par sommet est légitime.
- **`Intersect`** : un sommet peut être derrière le near. Ce chemin reste en espace clip : test near, `ClipTriangleNear`, puis projection **par coin**, parce que les sommets créés par le clipping n'existent dans aucun buffer.
- **`Outside`** : rejeté avant l'étage sommets, comme avant.

Le contrat est **vérifié**, pas supposé : `LV3_ASSERT(c.w > 0.0f)` dans `ProjectPositions`, prouvé sur environ 5 × 10⁷ sommets en `RelWithAsserts` (§ 7.1).

### 3.3 Les quatre pièces

**1. `RasterVertex`** (`Rendering/VertexStage.h`)

```cpp
struct alignas(16) RasterVertex { float x, y, z, invW; };   // 16 octets
```

- x, y en pixels (Y déjà retourné par `ToRaster`), z = z_ndc reverse-Z, invW = 1/w, le dénominateur perspectif.
- 16 octets : un quart de ligne de cache, la même empreinte que `Vec4f`. Pour une roche, 117 × 16 o = 1,8 Kio, qui restent en L1 pendant tout le parcours des faces.
- **Un type distinct de `Vec4f`** : une coordonnée clip et une coordonnée pixel ne sont jamais interchangeables, et le compilateur le garantit.
- Disposition **AoS** (un sommet = une structure) retenue parce que la boucle des faces lit les sommets **par indice** : une seule ligne de cache par sommet lu. Le chantier 4 (AVX2) transposera en registres si nécessaire.

**2. `ProjectClip`** : la source unique de la division par w. Les deux chemins l'appellent. Les mêmes opérations sur les mêmes entrées donnent les mêmes bits : l'image est identique **par construction**, et le run d'exactitude l'a confirmé.

**3. `ProjectPositions`** : l'étage sommets du chemin `Inside`.

```cpp
void ProjectPositions(const Matrix44f& mvp, const Viewport& vp,
                      const Vec3f* src, size_t count, RasterVertex* dst) noexcept
{
    const Matrix44f m = mvp;       // copies LOCALES : les écritures dans dst
    const Viewport  v = vp;        // ne peuvent pas les aliaser
    for (size_t i = 0; i < count; ++i)
    {
        const Vec4f c = MulRow(m, src[i]);
        LV3_ASSERT(c.w > 0.0f);    // contrat Inside
        dst[i] = ProjectClip(c, v);
    }
}
```

Une boucle contiguë, **sans branchement** en Release. C'est désormais la boucle qui projette 97,6 % des faces, et donc la cible naturelle du chantier 4 (AVX2).

**4. `EmitRasterTriangle`** : l'aval commun, en une seule copie.

```
backface (EdgeFunction sur x, y)  →  TrisRasterized  →  rejet 3b (TightPixelBox)  →  DrawTriangle
```

Même doctrine que `TightPixelBox` : deux copies de la même logique finissent par diverger. La seconde aurait un jour oublié l'exception `Wireframe` du rejet 3b.

### 3.4 Le tampon `RasterSpaceBuffer`

Même politique que `ClipSpaceBuffer` : possédé par l'**appelant** (`main.cpp`), alloué une fois (`kMaxMeshVerticesHard` × 16 o = 1 Mio), jamais partagé entre deux threads. Son contenu n'est valide que pendant **une (instance, vue)** : l'instance suivante l'écrase. `RenderView` reçoit les deux tampons :

```cpp
void RenderView(Registry&, ResourceManager&, Renderer&, const ViewData&,
                ClipSpaceBuffer& clipBuf, RasterSpaceBuffer& rasterBuf);
```

### 3.5 Le compteur `FacesInside`

Compté une fois par mesh `Inside`, hors de la boucle chaude, donc sous `LV3_PROFILE` comme `FacesSubmitted` (M10 ne s'applique pas). Il donne **η = `FacesInside / FacesSubmitted`**, la part des faces qui prennent le nouveau chemin : **0,976** en médiane, 0,937 au minimum sur la ceinture.

---

## 4. Place dans le pipeline

Le détail est dans l'**avenant A12a**. En résumé :

- la bande **sommet** a maintenant **deux branches** : `ProjectPositions` (Inside, jusqu'aux pixels) et `TransformPositions` (Intersect, jusqu'à l'espace clip) ;
- le choix de la branche se fait dans la bande **instance × vue**, par la classification du frustum, qui existait déjà ;
- la bande **triangle** ne divise plus jamais pour les meshes `Inside` ;
- les deux branches se rejoignent dans `EmitRasterTriangle`, et tout ce qui suit (backface, 3b, setup raster, pixels) est **inchangé**.

---

## 5. Contre-exemples

```cpp
// ✗ 1. Diviser EN PLACE dans clipBuf pour économiser 1 Mio
clip[i] = { c.x / c.w, c.y / c.w, c.z / c.w, 1.0f / c.w };
```
Le même type `Vec4f` change de sens selon le chemin. Le jour où l'ordre des étapes bouge, le clipping near lira des coordonnées déjà divisées.

```cpp
// ✗ 2. Étendre le chemin par sommet aux meshes Intersect
```
Un sommet derrière l'œil (w < 0) serait projeté de l'autre côté de l'écran, inversé.

```cpp
// ✗ 3. « Accélérer » avec _mm_rcp_ps (1/w approché sur 12 bits)
```
L'image n'est plus identique au bit près, et les coutures verrouillées par `Test_ClipCoverage` (`EdgeFunction` antisymétrique + `ClipLess`) peuvent se rouvrir.

```cpp
// ✗ 4. Recopier backface + 3b + DrawTriangle dans le nouveau chemin
```
Deux copies divergent. `EmitRasterTriangle` est l'aval unique.

---

## 6. Règle de décision et prédiction (écrites avant la mesure, M12)

**Estimation :** environ 1,5 ns par coin (une division comptée à son **débit**, environ 4 cycles, plus une dizaine d'opérations flottantes). Par face : 4,5 ns avant, 1,05 ns après, plus environ 1 ns de copies supprimées.

**Prédiction :** `Render` −13 %, fourchette −8 à −18 %, pondérée par η supposé à 0,9. Loi de coût prédite : environ 23 ns par face.

**Préalable :** exactitude. Un seul écart de compteur arrête tout.

**Règle sur le gain (B ÷ A de `Render`, ABBA combiné) :**

| Zone | Décision |
|---|---|
| Δ ≤ −8 % | succès, mécanisme confirmé |
| −8 % < Δ ≤ −3 % | succès partiel : on garde (code exact, prépare le chantier 4) |
| −3 % < Δ < +3 % | neutre : on garde pour la structure, échec de prédiction documenté, hypothèse vers VTune |
| Δ ≥ +3 % | régression : retour arrière |

---

## 7. Vérifications

### 7.1 Série A : exactitude

Run de comptage `RelWithAsserts`, `LV3_RASTER_STATS 1`, ceinture, 4 vues, 1536×900. En-tête : `config=RelWithAsserts;affinity=fff;ablation=none;rasterstats=1;lodstats=0;asserts_lib=1`.

| Contrôle | Résultat |
|---|---|
| Σ `TrisCov*` + `TrisEarlyRejected` = `TrisRasterized` | **300 frames sur 300**, écart nul |
| `TrisTightEmpty` | 0 sur toutes les frames |
| `TrisCov0 / 1 / 2to4 / 5plus` | 166 / 131 / 50 / 32, identiques à C3 § 8.1 |
| `TrisEarlyRejected` | 134 003 / 135 207 / 135 830, identique |
| **`PixelsCovered`** | **4 562 / 4 573 / 53 668, identique** |
| `PixelsTested` / `PixelsTight` | 14 372 / 10 562, identiques |
| Assertions (`c.w > 0`, indices) | CSV complet de 300 frames : aucune violation (`AssertFailed` appelle `abort()`) |

Les compteurs sont en outre **identiques frame par frame** entre ce run et tous les runs Release : la simulation est déterministe dans les deux configurations.

`ProjectClip`, inliné dans deux contextes, a produit les mêmes bits. Les projets ne fixent aucun `FloatingPointModel` : c'est `/fp:precise`, qui ne contracte pas en FMA par défaut depuis Visual Studio 2022.

### 7.2 Un run refusé

Un premier run B isolé (`Release`, `affinity=4`) donnait `Render` = 8,64 ms, soit +17 % par rapport à la Référence G. Il a été **refusé** :

- il n'avait **pas de run A dans la même séance** ;
- son témoin mémoire `Clear` valait **1,31 ms**, contre 1,08 à 1,11 ms pour la Référence G : la machine était plus lente (bridage, batterie ou chaleur) ;
- les zones non touchées par le 2b (Animation, LocalXform1) étaient elles aussi 40 à 50 % plus lentes.

**Contre-exemple refusé :** normaliser par le témoin (`Render ÷ Clear`). `Clear` est limité par la mémoire, `Render` par le calcul : le bridage ne les ralentit pas dans la même proportion. Le rapport n'est pas un invariant.

**Indice non recevable :** la série A (RelWithAsserts, non épinglée, compteurs actifs) donnait `Render` = 5,75 ms avec un `Clear` normal (1,13 ms). Un binaire alourdi qui bat la Référence G annonçait un gain réel. Noté comme hypothèse, pas comme résultat (M10).

### 7.3 Série B : gain, en ABBA

Protocole M13 : deux binaires `Release` dans des dossiers de banc séparés, `start "" /wait /affinity 4`, un run jetable puis A B B A.

- A = LIB `8c05c84` + EXE `2f75287` (Référence G) ;
- B = code 2b.

**Recevabilité :**

| | A1 | B1 | B2 | A2 |
|---|---|---|---|---|
| Identité du binaire | pas de `lodstats=` | `lodstats=0`, colonne `FacesInside` | idem B1 | idem A1 |
| Clear (témoin mémoire) | 1,112 ms | 1,099 | 1,093 | 1,039 |
| Animation (témoin calcul) | 36,9 µs | 36,7 | 36,3 | 37,8 |
| LocalXform1 (témoin calcul) | 21,5 µs | 21,4 | 21,5 | 21,9 |
| p95 ÷ médiane de `Render` | 1,052 | 1,110 | 1,055 | 1,086 |

- Tous les p95 ÷ médiane sont sous 1,15.
- Compteurs identiques **frame par frame** sur les quatre runs.
- **Le témoin T n'a pas été relevé.** Animation et LocalXform1, deux zones de calcul que le 2b ne touche pas, le remplacent ici : elles restent dans ±2,5 %. Substitut déclaré.
- A1 (7,49 ms) confirme la Référence G (7,28 à 7,41 ms) à 1 à 3 % près.

**Résultat :**

| | A | B | B ÷ A |
|---|---|---|---|
| Render, paire 1 | 7,491 ms | 5,141 ms | 0,686 |
| Render, paire 2 | 7,986 ms | 5,159 ms | 0,646 |
| **Render, ABBA combiné** | 15,478 ms | 10,300 ms | **0,665, soit −33,5 %, environ −2,6 ms** |
| FRAME, ABBA combiné | 19,82 ms | 14,70 ms | 0,742, soit −25,8 % (≈ 101 → 136 fps) |
| Coût par face soumise | 27,1 ns | 18,7 ns | −8,4 ns |

La médiane des rapports **frame par frame** (possible parce que la simulation est déterministe) donne les mêmes valeurs : 0,686 et 0,646.

**Application de la règle :** Δ = −33,5 % ≤ −8 % → **succès**. Les deux paires s'écartent de 4 points (réserve de précision, comme en C3), mais chacune est très loin sous le seuil : le verdict n'en dépend pas.

---

## 8. Lecture du résultat : la prédiction est manquée d'un facteur 2,5

La règle dit succès. Mais −33,5 % au lieu de −13 % signifie que **le mécanisme était mal compris**.

### 8.1 L'erreur : le débit au lieu de la latence

J'ai estimé la division par son **débit** (environ 4 cycles), ce qui suppose que le processeur enchaîne les divisions de triangles successifs. Or l'ancien chemin formait, par triangle, une **chaîne de dépendances** terminée par un branchement imprévisible :

```
÷w (latence ≈ 11 cycles) → ToRaster → EdgeFunction → if (IsBackFacing) → if (boîte vide)
                                                           ↑ environ 50 % de dos, sans motif
```

- Environ la moitié des triangles sont vus de dos, sans régularité : le prédicteur de branchement se trompe souvent, et **chaque erreur vide le pipeline** (15 à 20 cycles).
- Après un vidage, le triangle suivant repart de zéro : ses divisions sont **en attente sur le chemin critique**, et l'exécution out-of-order ne peut plus les recouvrir avec le travail d'un autre triangle.
- Le gain mesuré, **8,4 ns par face, soit de l'ordre de 35 à 40 cycles**, est cohérent avec « latence de division + chaîne + vidage » et pas avec le débit seul (3 à 4 ns).

### 8.2 Ce que fait le nouveau système

- `ProjectPositions` est **sans branchement** : les divisions s'y enchaînent à leur débit, et 117 sommets remplacent 504 coins.
- La boucle des faces ne lit plus que des données prêtes en L1. Une erreur de prédiction y coûte beaucoup moins, parce qu'aucune division n'attend derrière.

### 8.3 Statut de l'explication

Elle est **cohérente avec l'hypothèse de C3 § 5.2** (erreurs de prédiction de branchement), qui expliquait déjà pourquoi le 3b avait rapporté moins que prévu. Deux chantiers sur deux vont dans le même sens. **Elle n'est pas prouvée** : seul Intel VTune le dirait (catégories *Bad Speculation* et *Core Bound / Divider*).

---

## 9. Anomalie : A dérive, B ne dérive pas

| | Premier run | Dernier run | Dérive |
|---|---|---|---|
| A (C3) | 10,08 ms | 10,61 ms | +5,4 % |
| A (2b) | 7,49 ms | 7,99 ms | **+6,6 %**, uniforme sur les 240 frames |
| B (2b) | 5,14 ms | 5,16 ms | +0,4 % |

Les témoins de calcul ne bougent pas. Deux séances de suite, c'est le **dernier run** qui dérive, et c'est chaque fois un A. On ne peut pas séparer « dernier de la séance » de « l'ancien code est plus sensible à l'état de la machine ». **La prochaine séance se fera en BAAB** : si B2 dérive alors, la cause est l'ordre des runs.

---

## 10. Règles de mesure : mises à jour de la série M

- **M12 complétée.** Pour prédire le coût d'une opération placée dans une boucle à branchements imprévisibles, on compte sa **latence sur le chemin critique**, pas son débit. Le débit ne vaut que pour une boucle sans branchement.
- **M14 (nouvelle).** **Un temps n'est comparé qu'à un temps de la même séance.** Un run isolé n'est jamais confronté à une référence d'une autre séance, même quand la machine semble identique. Le témoin mémoire (`Clear`) et les zones que le chantier ne touche pas servent à **refuser** un run, jamais à le **corriger** par un rapport.
- **Pratique à confirmer (pas encore une règle) :** alterner ABBA et BAAB d'une séance à l'autre, pour séparer l'effet de l'ordre de celui du code.
- **Action :** écrire le témoin T **dans le CSV** (en-tête ou colonne), par la LIB qui le mesure (M11), pour qu'il ne puisse plus manquer.

---

## 11. Référence H : nouveau point de départ

| Référence H | Valeur |
|---|---|
| Machine | i7-1265U, P-core épinglé (`affinity=4`), secteur, performances élevées |
| Code | chantiers 2, 3b et 2b intégrés |
| Render | **5,14 à 5,16 ms** (≈ 700 ‰ de la frame) |
| FRAME | **7,33 à 7,37 ms** (≈ 136 fps) |
| Témoin mémoire Clear | ≈ 1,09 à 1,10 ms |
| Loi de coût | ≈ **18,7 ns par face soumise** |
| η (faces sur le chemin par sommet) | 0,976 |

Chemin parcouru dans la phase G : `Render` est passé de 11,48 ms (C2, 4 vues) à 5,15 ms, **÷ 2,2, sans retirer une seule face**.

| Étape | Render | Coût par face |
|---|---|---|
| C2, avant chantier 2 | 11,48 ms | 41,7 ns |
| Référence F (chantier 2) | 10,19 ms | ≈ 37 ns |
| Référence G (chantier 3b) | 7,28 à 7,41 ms | 26,5 ns |
| **Référence H (chantier 2b)** | **5,14 à 5,16 ms** | **18,7 ns** |

---

## 12. Conséquences sur le plan d'optimisation

| # | Chantier | État / révision |
|---|---|---|
| 2 | Pré-transformation des sommets | clos (avenant C2) |
| 3b | Rejet précoce des triangles vides | clos (avenant C3) |
| **2b** | **Projection par sommet** | **CLOS : −33,5 % sur `Render`, image identique** |
| 3a | Boîte serrée dans la boucle pixel | abandonné pour la ceinture (C3) |
| **1** | **LOD** | **suivant.** Dimensionnement par un run de comptage `LV3_LOD_STATS`, seuils issus de l'erreur mesurée (annexe A13), règle écrite avant la mesure |
| **4** | **AVX2** | **cible déplacée.** `TransformPositions` ne sert plus qu'aux meshes `Intersect` (2,4 % des faces) : la vectoriser n'apporterait rien. La cible est désormais **`ProjectPositions`**, qui fait toute la projection de 97,6 % des faces dans une boucle contiguë sans branchement. La division doit rester exacte (`vdivps`, pas `vrcpps`) |
| L13 | Fonctions d'arête incrémentales | inchangé |

---

## 13. Hygiène

- **`LV3_LOD_STATS` vaut `1` par défaut dans `Core/config.h` du commit LIB `741952a`.** Il doit valoir `0`, comme tous les commutateurs de comptage. En l'état, tout build avec `LV3_PROFILE=1` devient un run de comptage sans le savoir, et tout build avec `LV3_PROFILE=0` échoue sur le `#error`.
- **Le code du 2b n'est pas encore dans le dépôt** (dernier commit LIB : `741952a`). Le commiter, avec un message qui porte son identité, par exemple « Chantier 2b clos, Référence H ».
- Remettre `LV3Profile` à `false` dans `LV3.Common.props` et vérifier `LV3_PROFILE(exe)=0` dans la ligne `[Build]`.
- Versionner sous `Mesures/` : `2B_serie_A`, `2B_A1`, `2B_B1`, `2B_B2`, `2B_A2` (CSV et résumés), et marquer le run B isolé comme **refusé**.
- Supprimer les dossiers de banc une fois les CSV versionnés.

---

*Visuels associés : « Pipeline LV3 » (à mettre à jour selon l'avenant A12a), « Étage sommets LV3 » (chantier 2).*
