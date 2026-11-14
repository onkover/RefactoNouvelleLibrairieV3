# Annexe A13 — Niveaux de détail (LOD) : technique, erreur géométrique, sélection

> **Statut** : leçon préparatoire. Le chantier 1 (LOD) s'ouvrira après la clôture du chantier 2b.
> **Rattachement** : avenant C3 § 11 (chantier 1 « premier sur le fond ») et discussion du 28/09/2026.
> **Portée** : LibraryV3 (LIB) et `gen_meshes.py` (EXE, `Documentations/fichiers de génération/`).
> **Nature** : ce document explique ce qu'est un LOD, comment on mesure l'erreur d'un niveau, comment on la projette en pixels, et comment on choisit un niveau par (instance, vue). Il **corrige** l'hypothèse H1 proposée dans la même discussion (§ 11).

---

## 1. Le problème

Les chiffres de l'avenant C3, sur la ceinture en 4 vues et 1536×900 :

| Grandeur | Valeur |
|---|---|
| Triangles envoyés au rasterizer | 134 389 |
| Pixels réellement couverts | **4 562** |
| Triangles qui ne couvrent aucun pixel | **99,8 %** |
| Taille d'un astéroïde à l'écran | environ 3,5 pixels |
| Faces d'un astéroïde | 168 |

Un pixel est la plus petite chose que l'écran puisse afficher. Un détail géométrique plus petit qu'un pixel est **invisible par construction** : l'échantillonnage au centre du pixel ne peut pas le voir.

Le moteur paie pourtant chaque face au même prix, qu'elle produise quelque chose ou non. Pour une roche, par vue :

- 117 `MulRow` ;
- 504 divisions par w et 504 `ToRaster` (avant le 2b) ;
- 168 tests de backface ;
- 168 boîtes serrées.

La loi de coût de la Référence G le résume : `Render ≈ 26,5 ns × FacesSubmitted`. **Le nombre de faces soumises est la seule variable qui compte**, et il ne dépend aujourd'hui ni de la distance, ni de la taille à l'écran.

---

## 2. Définition

> **LOD (Level Of Detail)** : on prépare plusieurs versions du même objet, de moins en moins détaillées. À chaque frame, pour chaque instance **et pour chaque vue**, on dessine la version la plus légère dont la différence avec l'original reste invisible.

### 2.1 L'analogie des mipmaps

Une texture de 1024×1024 vue sur 8×8 pixels ne montre que 64 texels utiles. On lui prépare donc des versions réduites (512, 256, … 1), et le sampler choisit celle dont le texel fait environ un pixel.

Le LOD fait exactement la même chose pour la géométrie :

| | Mipmaps | LOD géométrique |
|---|---|---|
| Ressource | texture | maillage |
| Versions | niveaux 0, 1, 2… (÷2 par axe) | L0, L1, L2… (moins de faces) |
| Critère | taille d'un texel en pixels | erreur géométrique en pixels |
| Choisi par | pixel | paire (instance, vue) |

### 2.2 Ce que le LOD apporte

1. **Il agit sur la variable de la loi de coût.** Le chantier 2b baisse le **prix** d'une face, le LOD baisse leur **nombre**. Les deux gains se multiplient.
2. **Il supprime du travail partout à la fois.** Le 3b jetait les triangles vides *après* les avoir transformés et projetés. Le LOD ne les crée jamais.
3. **Il rend le coût proportionnel à l'image.** Une scène remplie d'objets lointains coûte à peu près ce que coûte ce qu'on en voit.

### 2.3 Ce que le LOD n'apporte pas

- Rien sur un objet proche : une roche de 20 px de rayon garde ses 168 faces.
- Rien sur un objet hors champ : c'est le travail du frustum culling, qui passe **avant**.
- Il ne remplace pas le 2b : sur les faces conservées, c'est le prix unitaire qui compte.

---

## 3. La chaîne de LOD

### 3.1 LOD discret

LV3 utilisera des **LOD discrets** : un petit nombre de maillages complets, préparés hors ligne, entre lesquels on choisit. C'est la technique de Unity (`LODGroup`) et de la plupart des moteurs. Les variantes plus avancées sont citées au § 10.

Chaque niveau est un **maillage fermé et complet**, pas un sous-ensemble des faces du niveau précédent.

### 3.2 Générer les niveaux du rocher

`gen_meshes.py` construit le rocher comme une sphère UV déformée par une somme d'harmoniques :

```python
def rock(seed, seg=12, rings=8, amp=0.30):
    ...
    def noise(i, j, n):          # n = direction unitaire du sommet
        d = sum(w * sin(a*n[0]*3.1 + b*n[1]*3.1 + c*n[2]*3.1 + ph) for ...)
        return 1.0 + amp * d / len(waves)
    return uv_sphere(seg, rings, 1.0, noise)
```

Le déplacement dépend de la **direction** du sommet, pas de son indice. Appeler `rock()` avec une grille plus grossière donne donc **le même rocher, échantillonné moins finement**. C'est la façon la plus simple d'obtenir une chaîne cohérente.

Pour une sphère UV, le nombre de faces vaut `2 × seg × (rings − 1)`, parce que les quads dégénérés des pôles ne produisent qu'un triangle :

| Niveau | seg × rings | Faces | Sommets |
|---|---|---|---|
| L0 | 12 × 8 | 168 | 117 |
| L1 | 8 × 5 | 64 | 54 |
| L2 | 4 × 3 | 16 | 20 |
| L3 | 4 × 2 | 8 | 15 |

Les sphères disposent déjà d'une chaîne : `sphere_hi` (3 968 faces), `sphere_mid` (960), `sphere_lo` (288).

---

## 4. L'erreur géométrique d'un niveau

### 4.1 Ce qu'on veut mesurer

Un niveau Lₖ est acceptable si **aucun point de sa surface ne s'écarte trop de la surface de L0**, et réciproquement. La mesure standard de cet écart est la **distance de Hausdorff** entre deux surfaces A et B :

```
h(A, B) = max  min  |a − b|         (distance ORIENTÉE de A vers B)
          a∈A  b∈B

H(A, B) = max( h(A, B), h(B, A) )   (distance SYMÉTRIQUE)
```

Autrement dit : on prend le point de A le plus mal placé par rapport à B, et on mesure sa distance au point de B le plus proche. On fait de même dans l'autre sens, et on garde le pire des deux.

**Pourquoi les deux sens ?** Un niveau grossier peut « couper » une bosse de L0 : les points de la bosse sont loin de Lₖ, alors que tous les points de Lₖ sont proches de L0. Un seul sens ne verrait rien.

**Pourquoi le maximum et pas la moyenne ?** On veut une garantie sur la silhouette : **aucun** pixel ne doit bouger de plus que la tolérance. Une moyenne faible peut cacher une pointe entière effacée. La moyenne sert seulement à juger la qualité globale d'un niveau, jamais à le sélectionner.

On note **ε_k** l'erreur du niveau k, exprimée en **unités locales du mesh**, c'est-à-dire avant le `Transform`.

### 4.2 Méthode 1 : l'écart radial (objets étoilés)

Un rocher est **étoilé** par rapport à son centre : chaque rayon issu du centre traverse sa surface une seule fois. On peut alors mesurer l'écart **le long de ces rayons** :

1. tirer N directions uniformes sur la sphère unité ;
2. pour chaque direction d, lancer un rayon depuis le centre et trouver l'intersection avec L0, à la distance t₀(d), puis avec Lₖ, à la distance tₖ(d) ;
3. ε_k ≈ max sur d de |tₖ(d) − t₀(d)|.

C'est la méthode utilisée pour les chiffres de ce document (400 directions, intersection rayon-triangle de Möller-Trumbore) :

```python
def ray_r(V, F, d):                        # distance du centre à la surface, direction d
    best = None
    for f in F:
        a, b, c = V[f]
        e1, e2 = b - a, c - a
        p = np.cross(d, e2); det = e1 @ p
        if abs(det) < 1e-12: continue      # rayon parallèle au triangle
        inv = 1 / det; s = -a              # origine du rayon = centre (0,0,0)
        u = (s @ p) * inv
        if u < 0 or u > 1: continue
        q = np.cross(s, e1); v = (d @ q) * inv
        if v < 0 or u + v > 1: continue
        t = (e2 @ q) * inv
        if t > 0: best = t if best is None else min(best, t)
    return best

dirs = normalise(rng.normal(size=(400, 3)))
r0 = [ray_r(V0, F0, d) for d in dirs]
rk = [ray_r(Vk, Fk, d) for d in dirs]
eps_max = max(abs(rk - r0)); eps_moy = mean(abs(rk - r0))
```

**Résultats pour `rock_a`** (rayon moyen 1, rayon réel entre 0,80 et 1,13) :

| Niveau | Faces | ε_max | ε_moyen |
|---|---|---|---|
| L0 | 168 | 0 | 0 |
| L1 | 64 | **0,235** | 0,060 |
| L2 | 16 | **0,484** | 0,192 |
| L3 | 8 | **0,537** | 0,284 |

**Limites de la méthode radiale :**

- elle ne vaut que pour un objet étoilé. Une tasse, un vaisseau ou un anneau ne le sont pas ;
- la distance le long du rayon est **supérieure ou égale** à la distance la plus courte entre les surfaces : c'est un majorant, donc un choix prudent ;
- elle est **échantillonnée** : une pointe très fine entre deux directions peut être manquée. 400 directions suffisent pour un rocher de 168 faces, mais pas pour un mesh de 100 000 faces.

### 4.3 Méthode 2 : Hausdorff échantillonné (cas général)

Pour un mesh quelconque :

1. échantillonner M points sur la surface de A, **proportionnellement à l'aire** de chaque triangle (sinon les grands triangles sont sous-représentés) ;
2. pour chaque point, calculer la distance au triangle le plus proche de B (fonction `ClosestPtPointTriangle` d'Ericson, *Real-Time Collision Detection*, § 5.1.5) ;
3. h(A, B) = maximum de ces distances ;
4. recommencer de B vers A, et garder le maximum.

Le coût est O(M × F), mais ce calcul se fait **hors ligne**, dans le générateur ou au chargement, jamais par frame. Une BVH sur B le ramène à O(M log F) pour les gros maillages.

L'échantillonnage sous-estime toujours légèrement le vrai maximum. On le compense par un léger coefficient de sécurité (par exemple ×1,1), déclaré avec la valeur stockée.

### 4.4 Où vit ε

ε_k est une **propriété de l'asset**, calculée une fois et stockée avec le niveau. Le moteur ne la recalcule jamais. Elle s'exprime en **unités locales** : l'échelle du `Transform` s'applique au moment de la sélection (§ 5.3).

### 4.5 Ce que la table apprend : la courbe faces / erreur

| Passage | Faces gagnées | Erreur ajoutée |
|---|---|---|
| L0 → L1 | −104 | +0,235 |
| L1 → L2 | −48 | +0,249 |
| L2 → L3 | −8 | **+0,053** |

L3 a moitié moins de faces que L2 pour presque la même erreur. **L2 est dominé** : partout où L2 est admis, L3 l'est presque aussi. Une chaîne de LOD se construit en suivant la courbe faces / erreur, pas en divisant le nombre de faces par deux à l'aveugle.

Règle pratique : un niveau n'a sa place dans la chaîne que s'il **réduit nettement les faces pour un coût d'erreur modéré** par rapport à ses voisins. Sinon on le retire.

---

## 5. Projeter l'erreur en pixels

Une erreur de 0,5 unité ne dit rien en soi : elle est énorme sur une roche qui remplit l'écran et invisible sur une roche de 2 pixels. Il faut la convertir en pixels, **pour cette instance, dans cette vue**.

### 5.1 Combien de pixels fait une longueur ℓ à l'écran ?

On suit une longueur ℓ perpendiculaire à l'axe de vue, à la profondeur de vue d. En perspective, avec la convention vecteur-ligne de LV3 :

1. **Espace clip** : `y_clip = y_vue × P[1][1]`, et `w_clip = −z_vue = d`.
2. **NDC** : `y_ndc = y_clip / w_clip = y_vue × P[1][1] / d`.
3. **Pixels** (`Viewport::ToRaster`) : `y_raster = y0 + (0,5 − y_ndc / 2) × H`.

En dérivant par rapport à y_vue :

```
|d y_raster / d y_vue| = P[1][1] × H / (2 d)
```

Donc une longueur ℓ fait à l'écran :

```
ℓ_px = ℓ × P[1][1] × H/2 ÷ d = ℓ × kPx ÷ w        avec kPx = P[1][1] × H/2
```

Pour une perspective symétrique, `P[1][1] = 1 / tan(fovY / 2)`. Exemple : fovY = 60°, H = 450 px, donc kPx = 1,732 × 225 ≈ **390 px**. Un objet de 1 m à 100 m de distance fait donc 3,9 px.

### 5.2 Une seule formule pour la perspective et l'ortho

`w_clip` est **affine** en position monde : `w(p) = p · c + VP[3][3]`, où c est la colonne 3 (lignes 0 à 2) de la matrice VP.

| Projection | Colonne 3 de P | Norme de c | w |
|---|---|---|---|
| Perspective | (0, 0, −1, 0) | 1 (vue rigide) | profondeur de vue |
| Orthographique | (0, 0, 0, 1) | 0 | 1 |

En ortho, `P[1][1] = 2 / (t − b)` et w = 1, donc `ℓ_px = ℓ × H / (t − b)` : la taille ne dépend pas de la distance, ce qui est le comportement attendu. **La même formule couvre les deux projections sans aucun drapeau de mode.**

### 5.3 Du mesh à l'instance

Pour une instance, on connaît :

- **R** : le rayon de la sphère circonscrite à l'AABB locale, soit `|Extent()|` de l'AABB locale ;
- **s_max** : la plus grande échelle d'axe de la matrice monde, c'est-à-dire la plus grande norme des lignes 0 à 2 (convention vecteur-ligne). Avec une échelle non uniforme, on prend la plus grande : c'est le choix prudent ;
- **w_c** : le w_clip du centre de l'AABB, soit `MulRow(mvp, box.Center()).w`, une seule `MulRow` puisque mvp est déjà calculée ;
- **|c|** et **kPx** : calculés une fois par vue.

On prend la profondeur au point **le plus proche** de la sphère :

```
w_min = w_c − R × s_max × |c|
```

Si |c| > 0 et w_min ≤ near, la sphère touche le plan near : l'objet est aussi proche que possible et on garde L0.

Sinon, on définit le **facteur d'échelle écran** de l'instance :

```
q = s_max × kPx ÷ w_min           (pixels par unité LOCALE du mesh)
```

et l'erreur du niveau k à l'écran vaut simplement :

```
e_k(px) = ε_k × q
```

Le rayon apparent utilisé par les compteurs `LV3_LOD_STATS` est de même `r_px = R × q`.

### 5.4 Limite connue

Hors de l'axe optique, une sphère se projette en **ellipse**, un peu plus grande que le cercle calculé ici (environ 15 % au bord d'un champ de 60°). La formule ne le modélise pas. On le couvre par la marge de la tolérance τ, et on le déclare plutôt que de le cacher.

---

## 6. Sélectionner le niveau

### 6.1 La règle

> On choisit **le niveau le plus grossier dont l'erreur à l'écran reste sous la tolérance τ** :
> `k* = max { k : ε_k × q ≤ τ }`

Les ε_k sont croissants avec k, donc on cherche en partant du niveau le plus grossier et on s'arrête au premier admis.

### 6.2 Le choix de τ

| τ | Sens | Usage |
|---|---|---|
| 0,5 px | aucun centre de pixel ne peut changer de côté de la silhouette par plus d'un demi-pixel | **défaut LV3** : image pratiquement identique |
| 1 px | la silhouette peut bouger d'un pixel | acceptable pour des objets de décor nombreux |
| > 1 px | le changement devient visible | à réserver à un mode « performance » explicite |

### 6.3 Les seuils du rocher

Comme `r_px ≈ R × q` avec R ≈ 1, un niveau k est admis tant que `r_px ≤ τ / ε_k` :

| Niveau | ε_k | Admis si r_px ≤ … (τ = 0,5) | … (τ = 1) |
|---|---|---|---|
| L1 | 0,235 | **2,1 px** | 4,3 px |
| L2 | 0,484 | 1,0 px | 2,1 px |
| L3 | 0,537 | **0,9 px** | 1,9 px |

Sur la ceinture, un astéroïde couvre environ 3,5 pixels d'aire, soit environ 1 px de rayon. À τ = 0,5, une grande partie des roches passerait de 168 à 8 ou 64 faces.

### 6.4 Précalculer les seuils (DOD)

La comparaison `ε_k × q ≤ τ` s'écrit aussi `q ≤ τ / ε_k`. Le membre de droite ne dépend que de l'asset et de τ : on le calcule **une fois au chargement** et on le stocke dans un petit tableau contigu. La sélection par (instance, vue) devient alors une suite de comparaisons, sans division ni multiplication :

```cpp
// ResourceManager : une chaîne par asset, 64 octets max, une seule ligne de cache.
struct MeshLODChain
{
    static constexpr uint8_t kMax = 4;
    MeshHandle level[kMax];          // L0 .. L(n-1)
    float      qMax[kMax];           // tau / eps_k, precalcule. qMax[0] = +inf
    uint8_t    count = 1;
    AABB3d     bounds;               // UNION des AABB de tous les niveaux (§ 7.2)
};

// RenderView, par (instance, vue), APRES le frustum culling.
[[nodiscard]] LV3_FORCEINLINE uint8_t SelectLOD(const MeshLODChain& c, float q) noexcept
{
    for (uint8_t k = uint8_t(c.count - 1); k > 0; --k)
        if (q <= c.qMax[k]) return k;    // le plus grossier admis
    return 0;
}
```

Ce code reste un **croquis de leçon**. L'implémentation réelle se fera au chantier 1, avec sa propre règle de décision écrite avant la mesure (M12).

---

## 7. Place dans l'architecture de LV3

### 7.1 Le choix appartient à la paire (instance, vue)

La ceinture se rend en **4 vues**. Une même roche peut être à 2 px dans la vue de suivi et à 40 px dans la vue FPS. Le niveau choisi est donc :

- **calculé** dans `RenderView`, pour la vue en cours ;
- **consommé** immédiatement ;
- **jamais stocké** dans un composant partagé par les vues.

C'est la même règle que `CameraBinding` : les associations statiques (quelle chaîne pour quel mesh) sont stockées, les valeurs dérivées de la vue ne le sont pas.

### 7.2 Frustum culling et LOD

Le culling classe l'AABB **avant** la sélection du niveau. Les sommets d'un niveau grossier ne sont pas forcément à l'intérieur de l'AABB de L0. Pour le rocher, c'est **mesuré** :

- L2 et L3 ont leurs sommets à des longitudes multiples de 90°, qui appartiennent à la grille de L0 (multiples de 30°) : leurs sommets sont des sommets de L0, ils ne dépassent pas ;
- L1 (multiples de 45°) échantillonne le bruit là où L0 n'a pas de sommet : il dépasse l'AABB de L0 de **0,011, 0,032 et 0,007** pour `rock_a`, `rock_b` et `rock_c`.

On utilise donc **l'union des AABB de tous les niveaux**, calculée au chargement. Sinon, un objet classé `Inside` pourrait avoir un sommet hors du frustum, et le contrat du 2b (w > 0 pour tout sommet) ne serait plus garanti par construction.

### 7.3 Ordre dans `RenderView`

```
pour chaque (instance, vue) :
    frustum culling sur l'AABB union          → Outside : on passe
    q = s_max × kPx ÷ w_min                   → une MulRow, une division
    k = SelectLOD(chaine, q)                  → 1 à 3 comparaisons
    mesh = chaine.level[k]
    FacesSubmitted += mesh.faceCount()        → le compteur voit les faces RÉELLES
    chemin Inside (2b) ou Intersect           → inchangé
```

Le reste du pipeline ne sait pas qu'un LOD a été choisi : il reçoit simplement un maillage plus léger.

### 7.4 Ce qui ne reçoit jamais de LOD

- **Les gizmos de debug** : un outil de diagnostic ne doit pas se dégrader avec la distance, sinon il ment sur ce qu'il montre. Ils n'ont pas de chaîne et restent à L0.
- **Les meshes sans chaîne** (vaisseau, meshes de test) : `count = 1`, donc L0. Il n'y a aucun cas particulier dans le code.

---

## 8. Piège propre à LV3 : la couleur par indice de face

Aujourd'hui, une face sans teinte de debug est colorée par :

```cpp
const Color col = hasTint ? tint : FaceColor(int(f));   // hash de l'INDICE de face
```

Un niveau grossier a **d'autres faces, avec d'autres indices**. Les couleurs changeront donc entièrement au passage d'un niveau à l'autre, même quand la silhouette est identique au pixel près. Pour les roches de la ceinture, qui n'ont pas de `DebugVisualComponent`, c'est le cas.

Conséquences :

- l'**erreur géométrique** ne garantit que la **silhouette** (quels pixels sont couverts), pas la couleur ;
- la validation du chantier 1 portera donc sur la **couverture** (`PixelsCovered`, masque de pixels) et non sur une comparaison de couleurs ;
- une image stable demandera une couleur de **matériau** ou un éclairage calculé à partir des normales, et non un hash d'indice. C'est un autre chantier, à noter comme dette.

---

## 9. Transitions : le « popping »

Quand une instance franchit un seuil, elle change brusquement de maillage : c'est le **popping**.

- **Avec τ = 0,5 px**, le saut est, par définition, inférieur à un demi-pixel de silhouette. En pratique, il ne se voit pas. C'est l'argument principal pour une tolérance serrée.
- **Oscillation.** Une caméra qui tremble autour d'un seuil peut faire alterner deux niveaux à chaque frame. Le remède classique est l'**hystérésis** : on passe au niveau plus grossier à τ, et on ne revient au niveau plus fin qu'au-dessus de τ × 1,2, par exemple. Mais cela demande de **mémoriser le niveau précédent par (instance, vue)**, donc un état par vue, ce que LV3 évite aujourd'hui. On ne l'ajoute que si une mesure ou une observation le justifie.
- **Techniques plus coûteuses**, pour information : fondu enchaîné entre deux niveaux (dithering), *geomorphing* (interpolation des positions des sommets pendant la transition). Aucune n'est prévue.

---

## 10. Pour situer LV3 : ce que font les autres

| Technique | Principe | Où |
|---|---|---|
| **LOD discret** | quelques maillages préparés, sélection par taille à l'écran | Unity `LODGroup` (seuils en « hauteur relative à l'écran »), Unreal (« screen size ») |
| **Maillages progressifs** | une suite continue d'effondrements d'arêtes, on s'arrête au nombre voulu | Hoppe, *Progressive Meshes*, SIGGRAPH 1996 |
| **Hiérarchie de clusters** | le maillage est découpé en groupes de triangles, chacun avec sa propre erreur, choisis indépendamment | Unreal Nanite |
| **Imposteurs** | un objet lointain est remplacé par une image plaquée sur un quad | forêts, foules |
| **HLOD** | plusieurs objets lointains fusionnés en un seul maillage simplifié | grands mondes ouverts |

LV3 commence par le LOD discret. C'est le plus simple, il suffit largement à la ceinture, et il installe les trois briques communes à toutes les autres techniques : **erreur géométrique, projection en pixels, sélection par vue**.

---

## 11. Correction de l'hypothèse H1

Dans la même discussion, pour estimer le gain du LOD, j'avais proposé une échelle H1 : L1 admis jusqu'à 16 px de rayon, L2 jusqu'à 4 px, L3 sous 1 px.

**Cette échelle était fausse.** Elle fixait des seuils sans aucune erreur mesurée. Avec l'erreur réelle et τ = 0,5, L1 n'est admis que jusqu'à **2,1 px**, pas 16. Le gain réel du LOD sera donc plus faible que ce que G1 aurait annoncé avec H1.

C'est l'illustration de la règle de ce document : **un seuil de LOD n'existe que comme conséquence d'une erreur mesurée**. H1 est retirée. Le dimensionnement du chantier 1 utilisera les ε_k mesurés et les compteurs de taille apparente.

---

## 12. Contre-exemples

```cpp
// ✗ 1. Choix sur la DISTANCE, rangé dans le composant
meshComp.m_lod = (dist > 50.0f) ? 3 : 0;
```
- faux en ortho : la taille ne dépend pas de la distance ;
- faux sous zoom : `CameraZoomSystem` change le FOV, pas la distance ;
- faux quand la résolution change : 1536×900 et 768×450 donneraient le même niveau ;
- faux en multi-vues : la 4ᵉ vue écrase le choix des trois autres.

```cpp
// ✗ 2. Fabriquer un niveau en sautant une face sur deux
for (size_t f = 0; f < faceCount; f += 2) Emit(...);
```
La surface est trouée : on voit à travers la roche. Un niveau est un **autre maillage fermé**.

```cpp
// ✗ 3. Seuils sans erreur mesurée
if (rPx < 16.0f) useL1;     // c'était l'hypothèse H1 (§ 11)
```

```cpp
// ✗ 4. Sélectionner sur l'erreur MOYENNE
if (epsMean[k] * q <= tau) ...
```
Une moyenne faible peut cacher une pointe entière effacée de la silhouette.

```cpp
// ✗ 5. Culler avec l'AABB de L0 et dessiner L1
```
Un sommet de L1 sort de l'AABB de L0 : le contrat `Inside` du chantier 2b n'est plus garanti.

---

## 13. Récapitulatif des formules

| Grandeur | Formule | Calculée |
|---|---|---|
| Erreur d'un niveau | ε_k = H(L0, Lₖ), distance de Hausdorff, en unités locales | hors ligne, une fois |
| Pixels par unité monde à w = 1 | kPx = P[1][1] × H / 2 | une fois par vue |
| Pente de w | \|c\| = norme de la colonne 3 de VP | une fois par vue |
| Profondeur au plus près | w_min = w_c − R × s_max × \|c\| | par (instance, vue) |
| Facteur d'échelle écran | q = s_max × kPx ÷ w_min | par (instance, vue) |
| Erreur à l'écran | e_k = ε_k × q | par (instance, vue) |
| Seuil précalculé | qMax_k = τ / ε_k | au chargement |
| Niveau choisi | k* = max { k : q ≤ qMax_k } | par (instance, vue) |

---

## 14. Glossaire

- **LOD** : *Level Of Detail*, niveau de détail.
- **Chaîne de LOD** : la suite L0 … Lₙ₋₁ des maillages d'un même objet.
- **Distance de Hausdorff** : plus grand écart entre deux surfaces, mesuré dans les deux sens.
- **Erreur à l'écran** : erreur géométrique convertie en pixels pour une instance dans une vue.
- **τ (tolérance)** : erreur à l'écran maximale acceptée.
- **Popping** : changement brusque et visible lors d'un changement de niveau.
- **Hystérésis** : seuils différents pour monter et pour descendre de niveau, afin d'éviter l'oscillation.
- **Objet étoilé** : objet dont chaque rayon issu d'un centre traverse la surface une seule fois.

---

*Visuels associés : « Niveaux de détail LV3 » (rocher rasterisé à quatre niveaux, erreur projetée, pixels qui changent) et « Taille apparente LV3 » (rayon apparent contre LOD par distance).*
