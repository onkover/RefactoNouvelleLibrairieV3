# Consolidation C2 — Campagne de mesure (phase G)

> **Statut** : close le 26/09/2026.
> **Nature** : consolidation transversale. Aucun code nouveau n'est introduit par ce document ;
> il fige l'instrument, le protocole, les chiffres de référence et les décisions qui en découlent.
> **Portée** : LibraryV3 (LIB) + RefactoNouvelleLibrairieV3 (EXE), `Release`, x64, machine Core 9.

---

## 1. Pourquoi cette phase a existé

Le carnet de route posait une question unique et coûteuse :

> *A3 — traversée aplatie du scenegraph. Deux à trois séances. Les chiffres décideront si ça vaut le coup.*

Il n'existait alors **aucune borne de temps dans le moteur**. La décision aurait donc été prise
sur une impression (« ça rame avec la ceinture »). La phase G a construit l'instrument, le
protocole, puis produit les chiffres.

**Réponse : non.** Et le chemin a produit bien plus que la réponse — un diagnostic chiffré qui
réoriente les quatre prochaines leçons.

---

## 2. L'instrument : `Core/Profiler`

### 2.1 Principes de conception

| Choix | Raison |
|---|---|
| `enum class` dense indexant un tableau | zéro hachage, zéro allocation, zéro indirection dans la boucle chaude |
| entiers de nanosecondes accumulés | pas de conversion flottante par borne, pas d'erreur cumulée sur 10 000 frames |
| `std::vector` réservé **une fois** dans `Begin()` | une réallocation en cours de campagne produirait une frame aberrante, lue ensuite comme un pic du moteur |
| budget épuisé → **arrêt de l'enregistrement**, jamais de réallocation | idem |
| `steady_clock` et non `system_clock` | monotone : un ajustement NTP produirait une durée négative |
| l'instrument **se mesure lui-même** (`OverheadNs`, `resolution_ns`) | un chiffre dont on ignore la marge d'erreur n'est pas un chiffre |
| tableaux de noms **parallèles** aux enums + `static_assert` | un ajout dans l'enum sans ajout du nom est une erreur de compilation, pas un décalage silencieux des colonnes du CSV |

### 2.2 Le commutateur `LV3_PROFILE`

Sixième commutateur indépendant, défaut `0` dans `Core/config.h`. **Ne dérive ni de `_DEBUG`
ni de `LV3_DEBUG`, volontairement** : on mesure en `Release`, jamais en `Debug` — l'*iterator
debugging* de la STL fausse d'un facteur 10 à 50 le coût des `ViewGroup`, et ne préserve même
pas les **rapports** entre systèmes.

Défini en un seul foyer, `Build/LV3.Common.props`, pour la LIB **et** l'EXE :

```xml
<PropertyGroup>
  <LV3Profile Condition="'$(LV3Profile)'==''">false</LV3Profile>
</PropertyGroup>
<ItemDefinitionGroup Condition="'$(LV3Profile)'=='true'">
  <ClCompile>
    <PreprocessorDefinitions>LV3_PROFILE=1;%(PreprocessorDefinitions)</PreprocessorDefinitions>
  </ClCompile>
</ItemDefinitionGroup>
```

Activation sans éditer le fichier : `msbuild ... /p:LV3Profile=true`.

### 2.3 Sortie

Deux fichiers par campagne, sous `contentRoot/Mesures/` :

- `<scene>_<config>.csv` — une ligne par frame : **la preuve** ;
- `<scene>_<config>_summary.csv` — médiane / p95 / max par zone : **la réponse**.

Séparateur `;`, **toutes les valeurs en entier** (temps en ns, temps simulé en milli-jours) :
un `.` décimal dans un Excel francophone est lu comme du texte, et une colonne de temps devenue
texte est une campagne à refaire.

En-tête auto-descriptif : scène, configuration, résolution, **nombre de vues**, frames mesurées,
frames de chauffe, `overhead_ns`, `resolution_ns`.

---

## 3. Le protocole

### 3.1 Mode banc

```
exe.exe "<racine_de_contenu>" --scene=<fichier.json> --bench=<N> [--warmup=<N>] [--csv=<chemin>]
```

Trois sources de variation supprimées :

1. **`dt` fixe** (1/60 s) en entrée de la simulation — le temps *réel* continue d'être mesuré,
   c'est lui qui remplit `frame_ns`. Sans cela, à la frame 100 les planètes ne sont pas au même
   endroit d'une exécution à l'autre, et le delta entre deux scènes mélange le coût du moteur et
   le hasard de l'ordonnanceur.
2. **Souris non capturée** — capturée, le moindre mouvement fait pivoter la caméra FPS et change
   le nombre de pixels rasterisés.
3. **Sortie automatique** après N frames, fermeture de frame *avant* incrémentation du compteur
   (sinon la dernière ligne du CSV manque).

Les événements SDL continuent d'être dépilés : une fenêtre qui ne dépile pas ses messages est
déclarée « ne répond pas » par Windows, et l'on mesurerait un zombie.

**Preuve de déterminisme obtenue** : les compteurs des runs A, C et D sont identiques au chiffre
près (2580 / 1283 / 274 988 / 134 389), à deux résolutions différentes et avec le rasterizer
débranché.

### 3.2 Frames de chauffe

60 frames exclues du résumé (défauts de page, premier contact avec le depth buffer, caches
froids). **Elles restent dans la série brute**, marquées par la colonne `warmup` : les exclure du
calcul est de la méthode, les effacer du fichier serait une falsification.

### 3.3 Statistiques

Médiane, p95, max. **Jamais de moyenne**, et jamais de FPS moyens : `moyenne(1/t) ≠ 1/moyenne(t)`.
Un moteur dont la médiane est bonne et le p95 catastrophique est un moteur qui saccade.

Ligne `RESTE` = `frame_ns` moins la somme des zones, calculée **frame par frame puis résumée**
(la médiane d'une somme n'est pas la somme des médianes). Mesurée à **3 ‰** : les bornes couvrent
99,7 % de la frame, la ventilation est crédible.

### 3.4 Témoins et plancher de bruit

`Clear` et `Present` ne dépendent que de la résolution. À résolution constante, ils doivent rester
stables : ce sont des **contrôles négatifs**, des grandeurs dont la variation invalide l'expérience.

Observé sur `Clear` à résolution identique : 1085, 953, 969, 996 µs — une dispersion de **12 %**,
probablement due à la fréquence du CPU, plus haute quand la machine est chargée en continu.

> **Tout écart inférieur à ~15 % entre deux exécutions n'est pas significatif.**
> Plancher de bruit mesuré, pas supposé.

### 3.5 Limites de l'instrument

`overhead_ns` a varié de 32 à 123 ns selon l'état de la machine au moment de la calibration ;
`resolution_ns` vaut 100 (QPC à 10 MHz). **L'unité est la nanoseconde, la granularité est 100 ns.**

Conséquence : toute zone sous ~1 µs est illisible. `LocalXform2` et `WorldXform2` sortent à
200–400 ns — cela ne veut pas dire « gratuit » mais « trop petit pour être vu ». Ces deux valeurs
ne doivent jamais être citées comme des mesures.

---

## 4. Bugs corrigés pendant la phase

| N° | Objet | Nature |
|---|---|---|
| **67** | `config.json`, `engine.json` et `tahoma.ttf` lus depuis le **répertoire courant** au lieu de `contentRoot` | deux sources de vérité ; sous VS elles coïncidaient par hasard, en `Release` lancé par script la copie post-build gagnait |
| **68** | Une racine de contenu invalide passée en argument était **sautée en silence** au profit d'un repli | le programme produisait des chiffres exacts sur un autre contenu |
| **69** | `vi` déclaré sous `#ifdef _DEBUG`, utilisé sous `#if LV3_DEBUG` dans `RenderView` | un build `NDEBUG + LV3_DEBUG=1` ne compilait pas ; les deux commutateurs sont indépendants **par dessein** |
| — | `LV3_PROF_*` : formes désactivées collées dans la branche active (`C4005`) | `BeginFrame`/`EndFrame` seraient devenus des no-ops → CSV vide, sans aucune erreur |
| — | Divergence `LV3_PROFILE` entre EXE et LIB | résolue ; **garde-fou permanent** installé (§ 5, M6) |

Le bug 68 a produit la distinction qui structure désormais toute la résolution de chemins :

> Une chaîne de replis est la bonne réponse à **l'absence** d'information.
> Elle est la mauvaise réponse à une information **erronée**.

| Intention explicite | Repli |
|---|---|
| racine passée en argument, `LV3_CONTENT_ROOT` posée, `--scene=` | `LV3_PROJECT_DIR`, dossier de l'exécutable, scène de `config.json` |
| invalide → **arrêt**, jamais de repli | invalide → candidat suivant, silence légitime |

`ResolveContentRoot` reçoit une liste **anonyme** et ne peut pas distinguer un ordre d'un défaut :
la validation des intentions appartient donc à l'**application**, avec le critère du moteur
(`IsContentRoot`, publié pour cela), jamais avec un `exists()` qui laisserait passer un dossier
sans marqueur.

---

## 5. Règles de mesure (M1–M7)

Série volontairement distincte de la numérotation `R`/`S` existante, pour éviter toute collision.

- **M1** — On mesure en `Release`. Un chiffre obtenu en `Debug` n'est pas un chiffre : la STL
  instrumentée ne préserve même pas les rapports entre systèmes.
- **M2** — Un fichier de mesure porte **en lui** son contexte : scène, configuration, résolution,
  nombre de vues, marge d'erreur de l'instrument. Un chiffre sans contexte ne vaut rien dans six mois.
- **M3** — Toute campagne comporte au moins un **témoin** : une grandeur qui ne doit pas varier.
  Sa variation donne le plancher de bruit et, si elle est grande, invalide la comparaison.
- **M4** — On ne compare que des exécutions où **une seule variable** change. Le cas contraire se
  détecte par les métadonnées, pas par la mémoire de l'opérateur.
- **M5** — Médiane, p95, max. Jamais de moyenne, jamais de FPS moyens.
- **M6** — Un commutateur de compilation est défini dans **un seul foyer** (`LV3.Common.props`),
  et la binaire **déclare son propre état** au démarrage (ligne `[Build]` : configuration,
  commutateurs, date/heure de compilation, comparaison EXE/LIB). Une binaire périmée ou divergente
  doit se dénoncer elle-même.
- **M7** — Quand le chronomètre ne peut pas trancher sans se perturber, on **retranche** au lieu
  de mesurer : ablation d'un étage, comparaison des totaux. Le bloc d'ablation porte un marqueur
  voyant et se retire immédiatement après la mesure.

---

## 6. Chiffres de référence

Machine Core 9, `Release` x64, `LV3Profile=true`, 300 frames dont 60 de chauffe, `dt` = 1/60 s.
Médianes. Fichiers versionnés dans `Mesures/`.

### 6.1 Référence A — `solar_system_v1compat`, 2 vues, 1536×900

| Zone | Médiane | ‰ de la frame |
|---|---|---|
| FRAME | **2,507 ms** (≈ 400 fps) | 1000 |
| Clear | 1085 µs | 432 |
| Present | 930 µs | 370 |
| Render | 426 µs | 169 |
| Trigger | 13,3 µs | 5 |
| Input | 12,9 µs | 5 |
| Cameras | 6,9 µs | 2 |
| Animation | 6,1 µs | 2 |
| WorldXform1 | 5,4 µs | 2 |
| LocalXform1 | 2,3 µs | 0 |
| LocalXform2 / WorldXform2 | 200 ns | *sous le seuil* |
| RESTE | 9,5 µs | 3 |

Compteurs : 86 meshes testés, 83 rejetés, 4272 faces, 151 triangles.

**Toute la simulation — entrées, animation, deux cuissons, caméras, triggers, vues — pèse 48 µs,
soit 1,9 % de la frame.** 80 % du temps part dans l'écran (`Clear` + `Present`), grandeurs
indépendantes de la scène.

### 6.2 Référence B — `solar_system_v1compat_belt`, 4 vues, 1536×900

| Zone | Médiane | ‰ |
|---|---|---|
| FRAME | **13,69 ms** (≈ 73 fps) | 1000 |
| Render | **11,48 ms** | 838 |
| Present | 1026 µs | 74 |
| Clear | 973 µs | 71 |
| Animation | 38,4 µs | 2 |
| WorldXform1 | 23,7 µs | 1 |
| LocalXform1 | 20,4 µs | 1 |
| Trigger | 12,4 µs | 0 |
| RESTE | 10,6 µs | 0 |

Compteurs : **2580** meshes testés, **1283** rejetés par le frustum (49,7 %), **274 988** faces
soumises, **134 389** triangles rasterisés.

### 6.3 Deltas A → B

| Zone | A | B | Rapport |
|---|---|---|---|
| Render | 0,426 ms | 11,48 ms | ×27 |
| LocalXform1 | 2,3 µs | 20,4 µs | ×8,9 |
| Animation | 6,1 µs | 38,4 µs | ×6,3 |
| **WorldXform1** | **5,4 µs** | **23,7 µs** | **×4,4** |
| Trigger | 13,3 µs | 12,4 µs | ×0,9 — **inchangé** |

> ⚠ **Confusion détectée, puis levée** : B rend **4 viewports**, A en rend **2**. Repérée par la
> ligne `views=` des métadonnées (M2, M4) ; sans elle, toute la hausse de `Render` aurait été
> attribuée à la ceinture. Le run E (§ 6.6) isole les deux effets : à nombre de vues égal, la
> ceinture seule multiplie `Render` par **12,8**, dont **×29,5 sur les faces soumises**.

### 6.4 Expérience de contrôle C — B à 768×450 (4 fois moins de pixels)

| Zone | 1536×900 | 768×450 | Lecture |
|---|---|---|---|
| Clear | 969 µs | **194 µs** | ÷5 — la résolution a bien changé |
| Present | 1030 µs | 681 µs | ÷1,5 |
| **Render** | **11,59 ms** | **11,40 ms** | **−1,6 %, sous le plancher de bruit** |

**Le rendu n'est pas limité par le remplissage.** Le rasterizer, candidat naturel à une
vectorisation en L13, est déjà négligeable à cette charge.

### 6.5 Expérience d'ablation D — `DrawTriangle` débranché (M7)

| | Render | Déduction |
|---|---|---|
| complet | 11,59 ms | — |
| sans rasterisation | 5,95 ms | **géométrie = 5,95 ms** |
| différence | — | **rasterisation = 5,63 ms** |

Compteurs strictement identiques : l'ablation n'a retiré que ce qu'elle devait retirer.
Retrait vérifié ensuite par retour à 11,48 ms.

---

### 6.6 Référence E — `belt` ramené à **2 vues**, 1536×900

Run destiné à lever la confusion du § 6.3 : une seule variable change par rapport à B, le nombre
de viewports.

| Grandeur | E (2 vues) | B (4 vues) | Rapport |
|---|---|---|---|
| FRAME | 7,758 ms (≈ 129 fps) | 13,687 ms | ×1,76 |
| **Render** | **5,600 ms** | **11,477 ms** | **×2,05** |
| Meshes testés | 1290 | 2580 | ×2,00 |
| Faces soumises | 126 102 | 274 988 | ×2,18 |
| Triangles rasterisés | 62 977 | 134 389 | ×2,13 |
| Coût par face | 44,4 ns | 41,7 ns | ×0,94 |
| Clear / Present | 1001 / 945 µs | 973 / 1026 µs | témoins stables |

Taux de rejet par le frustum stable : 51,2 % (E) contre 49,7 % (B). Compteurs de nouveau
identiques entre médiane, p95 et max — déterminisme confirmé une fois de plus.

**Comparaison à nombre de vues égal** (A et E, 2 vues chacun) :

| | A — v1compat | E — belt | Rapport |
|---|---|---|---|
| Render | 0,437 ms | 5,600 ms | ×12,8 |
| Faces soumises | 4 272 | 126 102 | ×29,5 |
| Coût par face | **102 ns** | **44 ns** | ×0,43 |

Le coût par face est **deux fois plus élevé sur la scène légère** : ses rares faces sont de
grandes surfaces à l'écran, où le remplissage compte encore. Sur la ceinture, les faces sont
minuscules et le coût converge vers le prix fixe par primitive.

---

## 7. Analyse

### 7.1 La contradiction C ↔ D, et sa résolution

C dit « quatre fois moins de pixels ne change rien » ; D dit « supprimer la rasterisation divise
par deux ». Les deux sont vrais :

> **Le coût de la rasterisation n'est pas dans les pixels, il est dans la préparation de chaque
> triangle** — fonctions d'arête, boîte englobante, gradients — travail payé une fois par
> primitive et indifférent à sa surface.

### 7.2 Coûts unitaires

| Grandeur | Calcul | Résultat |
|---|---|---|
| Préparation par triangle | 5,63 ms ÷ 134 389 | **41,9 ns** |
| Amont par face | 5,95 ms ÷ 274 988 | **21,6 ns** |
| Amont par sommet | 21,6 ns ÷ 3 | **≈ 7,2 ns** — l'ordre de grandeur d'un `MulRow` scalaire |
| Faces par mesh visible | 274 988 ÷ 1297 | **212** |
| Triangles ÷ faces | 134 389 ÷ 274 988 | **0,489** |

Ce dernier rapport suggère des faces **triangulaires** avec ~51 % éliminées (dos + near-clip) —
lecture la plus simple, à confirmer par `vertsPerFace` avant d'en faire une hypothèse de travail.

### 7.2 bis — La loi de coût du rendu

Trois points de mesure (B, C, E) donnent la même valeur à 6 % près :

> **`Render` ≈ 42 ns × `FacesSubmitted`**, indépendamment de la **résolution** et du **nombre de
> vues**. Un viewport supplémentaire ne coûte rien en soi : il coûte exactement les faces qu'il
> resoumet.

La conséquence dirige toute l'optimisation à venir : **la seule variable qui compte est le nombre
de faces soumises**. Le chantier 1 (LOD) agit dessus directement, le chantier 2 agit sur le prix
unitaire, et le chantier 3 sur la moitié aval de ce prix.

### 7.3 Diagnostic

> **Le moteur est limité par le nombre de primitives, pas par leur taille.**
> 134 000 triangles pour couvrir une fraction de l'écran : les astéroïdes sont des mailles
> détaillées rendues sur quelques pixels. Problème classique des micro-triangles, où chaque
> primitive coûte son prix fixe quelle que soit sa surface.

Défaut structurel identifié dans `RenderView` :

```cpp
for (uint8_t k = 0; k < vpf; ++k)
    cv[k].clip = MulRow(mvp, mesh->vertexPositions[mesh->indices[base + k]]);
```

`MulRow` est appelé **par coin de face**, pas par sommet. Un sommet partagé par 4 à 6 faces est
transformé 4 à 6 fois par vue, avec le même résultat à chaque fois.

### 7.4 Le point 7 du carnet : la détection N²

`Trigger` **ne bouge pas** (13,3 → 12,4 µs) alors que la scène gagne des centaines d'entités : la
ceinture n'ajoute pas d'entités *porteuses de trigger*. Le N² de `TriggerSystem` porte sur le
nombre d'entités à collider, pas sur la taille de la scène.

Le refactor broad-phase (grille/quadtree, lié à L11) reste justifié sur le fond, mais **ce n'est
pas un problème de performance aujourd'hui**. Le compteur `TriggerPairs` reste à brancher pour
le confirmer.

---

## 8. Décisions

### 8.1 A3 — **abandonnée**

`WorldXform1` sur la scène lourde : **23,7 µs, soit 1 ‰ de la frame**. Rendre la traversée du
scenegraph *gratuite* ferait gagner un millième du temps de frame, pour deux à trois séances de
travail sur la structure la plus centrale du moteur.

> **Critère de réouverture** : une mesure future montrant `WorldXform1` au-delà de **5 %** de la frame.

### 8.2 Chantiers, classés par rentabilité mesurée

| # | Chantier | Gain attendu | Fondement chiffré |
|---|---|---|---|
| 1 | **LOD** — maille réduite pour les objets de quelques pixels | jusqu'à ×5 sur les **deux** moitiés à la fois | 212 faces par astéroïde minuscule |
| 2 | **Pré-transformation des sommets** par (mesh, vue) : tableau plat en espace clip, puis indexation | ≈ ×4 sur 5,95 ms, soit ~4,4 ms | chaque sommet transformé 4 à 6 fois |
| 3 | **Rejet précoce des triangles sub-pixel** avant préparation | à chiffrer sur 5,63 ms | 41,9 ns de préparation pour parfois 2 pixels |
| 4 | **SIMD AVX2** sur la passe de pré-transformation | multiplicatif avec le 2 | — |
| ~~A3~~ | ~~traversée aplatie~~ | ~~0,2 %~~ | abandonnée |

**L'ordre n'est pas négociable.** Vectoriser (4) une boucle qui transforme quatre fois trop de
sommets reviendrait à optimiser le gaspillage ; et le chantier 2 est le préalable structurel du 4,
puisque c'est lui qui crée le tableau contigu sur lequel AVX2 travaillera par paquets de huit.

---

## 9. Ce qui n'a pas été mesuré

À ne pas confondre avec ce qui a été mesuré et jugé négligeable.

1. **`RelWithAsserts`** — aucune exécution. Le protocole initial la prévoyait pour vérifier que
   les invariants tiennent sur les deux scènes. À faire, sans valeur de performance.
2. **La machine portable (AVX2 seul)** — non mesurée. Tous les chiffres valent pour le Core 9.
3. **Les pixels couverts** — aucun compteur. L'expérience C prouve que le remplissage est
   négligeable, mais on ignore *à quel point* les triangles sont petits, information nécessaire
   pour dimensionner les chantiers 1 et 3.
4. **La décomposition par vue** — les compteurs cumulent les vues. Le run E montre que le coût
   est proportionnel aux faces resoumises, mais la part propre de chaque viewport reste inconnue.
5. **`Clear` à 1 ms** — non expliqué. ~11 Mo effacés en 970 µs, soit ~11 Go/s : bien en deçà de ce
   qu'un `memset` vectorisé atteint. Sur la scène légère, c'est 43 % de la frame. Piste à part
   entière, indépendante des chantiers 1 à 4.

---

## 10. Reproduire la campagne

```bat
:: 1. Build\LV3.Common.props : <LV3Profile ...>true</LV3Profile>
:: 2. Régénérer la SOLUTION (les deux projets)
:: 3. Vérifier la ligne [Build] : LV3_PROFILE(exe)=1 | LV3_PROFILE(lib)=1

cd <racine>\x64\Release

RefactoNouvelleLibrairieV3.exe "<racine>" --scene=solar_system_v1compat.json      --bench=300
RefactoNouvelleLibrairieV3.exe "<racine>" --scene=solar_system_v1compat_belt.json --bench=300

:: 4. Remettre LV3Profile à false, régénérer, vérifier LV3_PROFILE(exe)=0
```

Sorties : `<racine>\Mesures\<scene>_Release.csv` et `..._summary.csv`.

**Avant toute comparaison** : vérifier dans l'en-tête que `res`, `views` et `config` sont
identiques entre les deux fichiers, et que `Clear` n'a pas bougé de plus de 15 %.

---

## 11. Bilan

La phase G devait répondre à une question binaire sur A3. Elle a produit :

- un **instrument permanent**, éteint par défaut, au coût mesuré (~0,01 % de la frame) ;
- un **protocole reproductible**, prouvé déterministe au compteur près ;
- **quatre chiffres de référence** versionnés, opposables à toute optimisation future ;
- **trois bugs** corrigés, dont deux qui auraient silencieusement corrompu les mesures ;
- l'**abandon motivé** d'un chantier de trois séances ;
- et une **feuille de route d'optimisation** fondée sur des mesures, non sur des intuitions.

Le bénéfice durable n'est aucun de ces chiffres : c'est que chaque optimisation à venir sera
désormais **vérifiable** au lieu d'être crue.

---

*Prochaine étape : leçon 07 (overlay de debug + `MenuSystem`), ou ouverture du chantier 2
(pré-transformation des sommets) si la performance de la scène lourde devient bloquante.*
