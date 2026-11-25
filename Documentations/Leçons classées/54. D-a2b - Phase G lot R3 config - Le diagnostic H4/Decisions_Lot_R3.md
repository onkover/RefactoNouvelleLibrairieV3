# Décisions — Lot R3 (diagnostic de H4′ : fenêtre de 1552 px, sans code)

*Échange du 20 novembre 2026 (soir). Artefact : « Lot R3 — Le diagnostic H4′ (fenêtre de 1552 px), pas à pas ». Roadmap : `Plan_Phase_G_LibraryV3.md` (ligne R3, prochaine étape 2c, « Évolutions et dettes relevées par le lot R3 »). Lot précédent : `Résultats/Decisions_Lot_Da2b.md` (D10, D12).*

## Statut

**Protocole LIVRÉ le 20/11/2026 ; MESURÉ le 24-25/11 (32 runs) ; CLOS le 25/11 : décision d'Onky (« troisième voie »), voir la section « Décision d'Onky (25/11) ».** Aucune ligne de C++ modifiée. Les décisions R3-D1 à R3-D8 sont des **propositions** du lot, non actées tant qu'Onky ne les a pas validées.

- **Base** : dépôts du 20/11, 21:49 (LIB `9678f47`, EXE `26ae1f8`, commits « Lot D-a2b »).
- **Objet** : tester H4′ sur l'i7-1265U. Avec 1536 px, le Z-buffer (et la couleur) ont un pas de 6 144 o ; les lignes r et r + 2 d'une tuile 8 × 8 ont les mêmes 12 bits d'adresse faibles (alias 4 Ko, ensembles L1). À 1552 px (pas 6 208 o, toujours multiple de 64), plus aucune ligne d'une tuile ne se confond.

## Constats sur les dépôts (20/11)

- **D12 appliqué et commité** : `Core/config.h` §14b, `#define LV3_RASTER_TILE_SPLIT 0` ; `LV3_RASTER_TILES 16` inchangé ; `Test_Da2b_TileSplit.cpp` présent, appelé par `RunAllTests.cpp`, déclaré dans le vcxproj. Rien à modifier avant R3.
- **Fichier et clé de la résolution** : `config.json` à la racine de contenu de l'EXE (le dossier qui contient `engine.json`), objet `screen`, clé **`width`** (et `height`). Chaîne : `ProgrammeConfig` (`Helper/ConfigManager.cpp`, `cfg.screenWidth = rr.Read("width", 1920)`) → `FrameW` (`main.cpp`) → `SDLINIT` (texture streaming) et `db.Resize(FrameW, FrameH)` → `DepthBuffer::Row(y) = data + y·m_Width` ; `BuildCameraBindings(layout, …, FrameW, FrameH)` → `BuildLayout` (Quad : vues `FrameW/2 × FrameH/2`) ; en-tête du summary `res=WxH`.
- Les entrées `viewports[].largeur` de `config.json` ne servent qu'aux séparateurs dessinés après le Render : les modifier ne change pas le pas.
- `engine.json` ne contient aucune résolution (`viewport.maxViewport = 4` → disposition Quad sur les scènes V5 à 4 caméras).
- Racine de contenu : `LV3_CONTENT_ROOT` (si posée, elle prime sur tout, sans repli), puis l'argument positionnel, puis `LV3_PROJECT_DIR` (Debug), puis le dossier de l'exécutable.
- `config.h` : `LV3_PROFILE` vaut toujours 1 par défaut alors que le message annonce 0 (E5, déjà en roadmap ; sans incidence pour R3).

## Décisions proposées

| # | Proposition | Raison | Écarté |
|---|---|---|---|
| R3-D1 | Une seule variable : `screen.width` 1536 → 1552 (+16 px, +64 o) | Casse la périodicité de 2 048 o, garde chaque ligne alignée sur 64 o (6 208 = 97 × 64), +1,04 % de pixels seulement ; fov vertical : cellules de même taille en px | 1540 / 1544 (pas non multiple de 64 : coupures de lignes de cache, seconde variable) ; 1024 / 2048 (autre scène) ; modifier `viewports` |
| R3-D2 | Mêmes binaires A (`LV3_RASTER_TILES 0`) et B (défaut : `tiles=16;tsplit=0`) aux deux largeurs ; deux racines de contenu (`C:\Banc_R3\W1536`, `W1552`) passées en argument positionnel ; `LV3_CONTENT_ROOT` vidée par le script | Même placement du code ; aucune édition du dépôt | Quatre copies d'exécutables ; éditer le `config.json` du dépôt |
| R3-D3 | Séquence imbriquée par scène `A₁₅₃₆ B₁₅₃₆ A₁₅₅₂ B₁₅₅₂ B₁₅₅₂ A₁₅₅₂ B₁₅₃₆ A₁₅₃₆` ; c24 ×2, c48 ×2 (32 runs + 1 jetable), c96 ×1 facultatif ; AVX2 seulement (`LV3_SIMD 1`) | Dérive linéaire annulée sur B/A à chaque largeur et sur l'effet de largeur ; c24 et c48 sont les scènes qui perdent | ABBA séparées par largeur (effet de largeur biaisé par un palier) ; B′ (D12) ; pendant scalaire |
| R3-D4 | Grandeur décisive : Δ = (B/A)₁₅₅₂ ÷ (B/A)₁₅₃₆ − 1 (différence de différences) | Retire le +1 % de pixels et la dérive | (B/A)₁₅₅₂ seul comparé au B/A d'un autre jour (dispersion de c24 d'un jour à l'autre : +4 % le 14/11, ≈ +2,3 % le 20/11) |
| R3-D5 | Règle de décision écrite avant la mesure (ci-dessous) | Règle du projet | — |
| R3-D6 | Contrôles avant tout calcul : `res=`, `tiles=`, `tsplit=0`, `views=4`, `rasterstats=0`, `config=Release`, `FbDbAlias4K` relevé aux deux largeurs, `TrisRasterized` / `FacesSubmitted` identiques entre A et B à largeur égale, ClearDepth ≤ 1,15 × la médiane de sa largeur | Un run à la mauvaise largeur ou sur le mauvais binaire se voit dans le fichier | Se fier aux noms des fichiers |
| R3-D7 | Le `config.json` du dépôt reste à 1536 ; les racines R3 sont des copies hors dépôt ; les CSV vont dans `<racine>\Mesures\R3\`, noms suffixés (scène, séquence, position, bras, largeur) | Aucun risque de commiter une largeur de mesure ni d'écraser un CSV | Éditer puis « penser à remettre » |
| R3-D8 | Suites : confirmée → D-a2c = Z-buffer à pas décalé (`m_Pitch = W + 16`) ; réfutée → D-a2c = ordre ligne par ligne dans un groupe (ou P1) ; non conclusive → une séquence c24 de plus, puis R4 (VTune, `LD_BLOCKS_PARTIAL.ADDRESS_ALIAS`) avant tout code | Branches 2c / 2c-bis / 2d du plan | Coder les deux corrections en parallèle |

## Validité d'une séquence (règles du 03/11 appliquées par largeur)

|A₁ − A₈| < 3 % et |A₃ − A₆| < 3 % ; ClearDepth varie de moins de 5 % entre les runs d'une même largeur (il grandit d'environ 1 % à 1552). Correction de contrôle : B/A ÷ ClearDepth à chaque largeur. RenderVerts (≈ 5 µs sur V5) inutilisable.

## Prédictions (écrites avant la mesure)

| Grandeur | Si H4′ tient | Si H4′ est fausse |
|---|---|---|
| c24 (B/A)₁₅₃₆ | +2 à +5 % | +2 à +5 % |
| c24 (B/A)₁₅₅₂ | −3 à −6 % | +2 à +5 % |
| c24 Δ | −6 à −10 % | 0 ± 1,5 % |
| c48 (B/A)₁₅₃₆ | +4 à +10 % | +4 à +10 % |
| c48 Δ | ≤ −3 % | 0 ± 1,5 % |
| Effet de largeur sur A | +0,5 à +2 % (pixels +1,04 % ; la VM donnait +30 %, non expliqué) | idem |

Calcul d'adresses (étape 3) : à 1536 px, dans chaque tuile, les lectures des lignes 2 à 7 tombent sur les mêmes 12 bits que l'écriture faite deux lignes plus tôt (6 sur 8) ; à 1552 px, aucune (écart de 128 o entre lignes successives modulo 4 096).

## Règle de décision

- **H4′ confirmée** si Δc24 ≤ −3 % sur les deux séquences valides et Δc48 < 0 sur au moins une séquence valide.
- **H4′ réfutée** si |Δc24| ≤ 1,5 % sur les deux séquences valides.
- **Non conclusive** sinon (zone −3 à −1,5 %, séquences contradictoires, une seule valide).

R3 décale aussi le pas de la couleur ; le Z-buffer à pas décalé ne décalerait que le Z. Si H4′ est confirmée, la mesure de D-a2c dira la part de la couleur.

## Livrables

- Artefact « Lot R3 — Le diagnostic H4′ (fenêtre de 1552 px), pas à pas » : pourquoi, rappel des lots, algorithme global surligné, contexte et objectif, besoin et méthodes (alias 4 Ko, ensembles L1, différence de différences, ABBA imbriqué), 4 laboratoires (adresses d'une tuile selon W, ordre de visite contre file des écritures, dérive et ordre des runs, dépouillement d'une séquence), algorithme détaillé avant / après (10 maillons), fonctionnement technique, 8 étapes (dont le script `Campagne_R3.bat`), 5 contre-exemples chiffrés, exemples d'utilisation, dettes R3-a à R3-f.
- Aucune modification de code.

## Dettes et évolutions relevées (inscrites en roadmap, rien n'est acté)

R3-a (séparateurs : clé `title` absente, verticale fixe à 1136), R3-b (recherches `unordered_map` dans la condition de boucle, chaque frame), R3-c (`DepthBuffer` en `std::vector<float>`, base alignée sur 16 o), R3-d (pitch SDL non journalisé), R3-e (pas d'argument `--res=`), R3-f (section `viewports` de `config.json` obsolète).

## Première moitié reçue (25/11) : les 16 runs à 1536 px (positions 1, 2, 7, 8)

En-têtes conformes sur les 16 fichiers : `res=1536x900`, `views=4`, `rasterstats=0`, `config=Release`, `affinity=4`, `sdl=direct3d` ; A `tiles=0;tsplit=0`, B `tiles=16;tsplit=0`. Compteurs identiques entre A et B (c24 : 11 368 faces, 6 146 rejetées tôt ; c48 : 2 880 et 1 440). Les runs à 1552 px (positions 3 à 6) ne sont **pas encore reçus** : Δ, la grandeur qui décide, ne peut pas être calculé.

| Séquence | A₁ | B₂ | B₇ | A₈ | (B/A)₁₅₃₆ brut | paires B₂/A₁, B₇/A₈ | ÷ ClearDepth | ÷ RenderVerts | \|A₁ − A₈\| | étendue ClearDepth |
|---|---|---|---|---|---|---|---|---|---|---|
| c24 s1 | 2 571,4 | 2 692,5 | 2 714,9 | 2 739,7 | **+1,8 %** | +4,7 / −0,9 | +3,6 % | +0,6 % | 6,5 % | 5,0 % |
| c24 s2 | 2 745,9 | 2 800,8 | 2 884,4 | 2 863,7 | **+1,4 %** | +2,0 / +0,7 | +0,4 % | +1,6 % | 4,3 % | 5,3 % |
| c48 s1 | 2 222,6 | 2 426,7 | 2 247,2 | 2 075,2 | **+8,8 %** | +9,2 / +8,3 | +10,0 % | (5 µs) | 7,1 % | 11,7 % |
| c48 s2 | 2 060,2 | 2 291,2 | 2 265,4 | 2 201,5 | **+6,9 %** | +11,2 / +2,9 | +8,2 % | (5 µs) | 6,9 % | 6,3 % |

(médianes de `Render` en µs)

Lecture :
- **B/A à 1536 reproduit les lots précédents** : c24 +1,4 / +1,8 % (prédit +2 à +5 % ; R2 ≈ +2 %, D-a2 +4 %), c48 +6,9 / +8,8 % (prédit +4 à +10 %). La référence de Δ est donc en place.
- **Validité : aucune moitié ne passe la règle du 03/11 telle quelle** (|A₁ − A₈| de 4,3 à 7,1 %). Mais cette règle a été calibrée sur `A B B A`, où les deux A sont à 3 runs d'écart ; dans la séquence imbriquée, A₁ et A₈ sont à 7 runs d'écart, la dérive entre eux est mécaniquement plus grande. Dans trois séquences sur quatre, A et ClearDepth dérivent dans le même sens et de la même quantité (c24 s1 : A +6,5 %, ClearDepth +5,0 % ; c24 s2 : +4,3 / +2,7 % ; c48 s2 : +6,9 / +6,3 %) : la machine entière a ralenti, ce que la symétrie de la séquence annule. c48 s1 est moins propre (A −6,6 %, ClearDepth −10,5 %, B₂ lent en début de séquence).
- **Proposition de règle R3-V (à acter avant de calculer Δ)** : une séquence imbriquée est valide si, à chaque largeur, |A₁ − A₈| < 3 % **ou** si A₈/A₁ et ClearDepth₈/ClearDepth₁ vont dans le même sens et diffèrent de moins de 3 points (dérive de toute la machine) ; et si aucun run n'a ClearDepth > 1,15 × la médiane de sa largeur. Appliquée à la moitié 1536 : c24 s1, c24 s2, c48 s2 valides ; c48 s1 limite (écart 3,9 points). À confirmer sur les positions 3 à 6.
- **`FbDbAlias4K` = 0 dans les 16 runs : ce n'est pas une sonde morte.** C0b (21/10) l'avait établi : couleur et profondeur sont toutes deux à « page + 32 » (même chemin d'allocation de grand bloc), donc (couleur − profondeur) mod 4096 = 0 par construction, à toute largeur. La valeur ne change pas entre 1536 et 1552 : ce n'est pas une seconde variable de R3. (Correction de l'artefact, § 7.5 et étape 5, qui demandaient une valeur « loin de 0 ».)
- **RenderVerts** est utilisable sur c24 (≈ 17 µs, aucun run au-delà de 1,06 × la médiane) : B/A ÷ RenderVerts +0,6 / +1,6 %, cohérent avec le brut.


## Seconde moitié reçue (25/11) : les 16 runs à 1552 px, et Δ

En-têtes conformes : `res=1552x900`, `views=4`, mêmes binaires (A `tiles=0`, B `tiles=16;tsplit=0`). Compteurs identiques entre A et B à 1552 (c24 : 11 368 faces, **6 106** rejetées tôt contre 6 146 à 1536 ; c48 : 2 880, **1 430** contre 1 440) : la vue plus large change bien un peu la scène, comme prévu. `FbDbAlias4K` = 0 aux deux largeurs.

Médianes de `Render` (µs), positions 3 à 6 :

| Séquence | A₃ | B₄ | B₅ | A₆ |
|---|---|---|---|---|
| c24 s1 | 2 581,0 | 2 724,6 | 2 539,9 | 2 526,2 |
| c24 s2 | 2 666,6 | 2 646,0 | 2 793,9 | 2 530,4 |
| c48 s1 | 2 439,6 | 2 437,6 | 2 510,7 | 2 406,9 |
| c48 s2 | 2 376,7 | 2 295,4 | 2 538,4 | 2 253,5 |

| Séquence | (B/A)₁₅₃₆ | (B/A)₁₅₅₂ | **Δ brut** | Δ ÷ ClearDepth | Δ ÷ RenderVerts | paires à 1552 (B₄/A₃, B₅/A₆) | validité R3-V (1536 / 1552) |
|---|---|---|---|---|---|---|---|
| c24 s1 | +1,8 % | +3,1 % | **+1,2 %** | +0,8 % | +1,8 % | +5,6 / +0,5 | oui / oui |
| c24 s2 | +1,4 % | +4,7 % | **+3,3 %** | +2,2 % | −0,7 % | −0,8 / +10,4 | oui / **non** (A −5,1 %, ClearDepth −1,2 %) |
| c48 s1 | +8,8 % | +2,1 % | **−6,1 %** | −3,1 % | (5 µs) | −0,1 / +4,3 | limite / oui |
| c48 s2 | +6,9 % | +4,4 % | **−2,4 %** | −4,7 % | (5 µs) | −3,4 / +12,6 | oui / oui (écart 2,8 points) |

Effet de la largeur, mêmes binaires, runs entrelacés :

| Séquence | Render A 1552/1536 | Render B 1552/1536 | ClearDepth | ClearColor |
|---|---|---|---|---|
| c24 s1 | −3,8 % | −2,6 % | +28,8 % | +16,7 % |
| c24 s2 | −7,4 % | −4,3 % | +25,5 % | +21,0 % |
| c48 s1 | +12,8 % | +5,9 % | +41,6 % | +33,5 % |
| c48 s2 | +8,7 % | +6,1 % | +24,3 % | +19,0 % |

## Lecture

1. **Règle écrite le 20/11 : non conclusif.** Ni « confirmée » (Δc24 ≤ −3 % sur deux séquences valides : Δc24 est positif dans les deux), ni « réfutée » au sens strict (|Δc24| ≤ 1,5 % sur deux séquences valides : s1 +1,2 % oui, s2 +3,3 % et non valide à 1552).
2. **c24 contredit H4′.** H4′ prédisait son plus grand effet sur c24 (Δ −6 à −10 %, la VM donnait −9 à −10 points). On mesure +1,2 et +3,3 % brut, +0,8 et +2,2 % ÷ ClearDepth, +1,8 et −0,7 % ÷ RenderVerts (seul thermomètre « calcul » recevable, 17 µs) : six lectures entre −0,7 et +3,3 %, aucune du côté prédit. Sur la scène où H4′ devait se voir le mieux, rien ne se voit.
3. **c48 va dans le sens de H4′, mais sur des paires qui se contredisent.** Δ −6,1 et −2,4 % brut, −3,1 et −4,7 % ÷ ClearDepth ; les deux paires d'une même largeur s'écartent de 4 à 16 points (c48 s2 à 1552 : −3,4 et +12,6 %), soit plus que l'effet cherché. Un Δ c48 sans Δ c24 ne colle pas non plus au mécanisme : c24 a 95 % de tuiles partielles, celles qui relisent la ligne r + 2.
4. **L'expérience n'était pas à une variable.** À 1552, ClearDepth prend +24 à +42 % et ClearColor +17 à +34 % pour 1 % de pixels en plus, alors que ce sont des écritures en continu sans aucune relecture de lignes ; A, qui ne visite jamais une tuile, change de −7 % (c24) à +13 % (c48). La largeur a déplacé autre chose que le pas de ligne : le placement des tampons (à 1536, couleur et profondeur sont à « page + 32 » ; à 1552 inconnu) ou le pitch SDL (non journalisé, R3-d). C'est l'« observation inexpliquée » de la VM (A ≈ +30 % au pas 1552), reproduite sur l'i7. Le rapport de rapports retire ce qui touche A et B de la même façon, pas ce qui touche différemment les blocs (A) et les tuiles (B) ; il ne protège donc pas contre ce nouvel effet.
5. **(B/A)₁₅₃₆ est stable et conforme** (c24 +1,4 / +1,8 %, c48 +6,9 / +8,8 %) : la perte des tuiles sur c24 est petite (≈ 1,5 %), celle de c48 reste nette (≈ 8 %).

## Verdict proposé (R3-D9, à acter par Onky)

- **H4′ écartée comme cause de la perte de c24** (six lectures de Δ, aucune du côté prédit) ; **non tranchée pour c48**, dans une expérience contaminée par un effet de placement mémoire.
- **Pas de Z-buffer à pas décalé** (la branche « confirmée » de R3-D8 n'est pas ouverte).
- **D-a2d (boucles de ligne, accepté le 21/11) devient la prochaine étape** : les binaires de R3 ne sont plus figés.
- **H4′ sur c48 se juge sans changer la largeur** : R4 (VTune) compte directement les blocages d'alias 4K (`LD_BLOCKS_PARTIAL.ADDRESS_ALIAS`) dans `RasterizeTiles8Fx`, binaire B, c48 et c24, à 1536. Si ce compteur rapporté au nombre de lectures du Z est faible, H4′ tombe aussi pour c48.
- **D-a2c reste à choisir après R4** (ordre ligne par ligne dans un groupe, ou P1).
- **Nouvelle observation R3-g** (roadmap) : l'effet de la largeur sur Clear et sur A. Moyen proposé : réactiver une fois la sonde provisoire C0b (bases des tampons dans leur page) et journaliser le pitch SDL, un run à chaque largeur.

## Règles proposées (25/11, à confirmer)

- **R3-V** : dans une séquence imbriquée, les deux A d'une largeur sont à 7 runs d'écart ; validité si |A₁ − A₈| < 3 % ou si A et ClearDepth dérivent dans le même sens à moins de 3 points près ; aucun run avec ClearDepth > 1,15 × la médiane de sa largeur.
- **Contrôle du paramètre** : avant de juger un rapport de rapports, comparer les zones témoins entre les deux valeurs du paramètre changé ; si une zone dont le code ne dépend pas de l'hypothèse bouge de plus que prévu (ici Clear +24 à +42 % au lieu de +1 %), l'expérience n'est pas à une variable et son verdict ne peut pas être « confirmée ».

## Leçon de méthode

La prédiction calculée (alias des lignes r et r + 2) était juste sur le papier et sur la VM ; elle ne garantissait pas que la largeur ne change que cela. Le témoin qui aurait dû être neutre (Clear) a été le premier à le dire.

## Décision d'Onky (25/11, 08:37) : la « troisième voie »

- **R3-V ACTÉE pour les lots futurs seulement**, non appliquée à R3 : elle a été écrite après la mesure, et l'appliquer à R3 adapterait le critère aux résultats.
- **R3 reste jugé par la règle du 03/11** : les quatre séquences sont formellement non valides (|A₁ − A₈| de 4,3 à 7,1 %). Aucun Δ de R3 n'est recevable comme preuve.
- **R3-D9 retenu sur la lecture d'ensemble**, sans valeur de preuve : H4′ n'est pas retenue comme cause de la perte de c24 (six lectures de Δ entre −0,7 et +3,3 %, aucune du côté prédit) ; non tranchée pour c48 ; pas de Z-buffer à pas décalé.
- **Suite inchangée** : D-a2d (boucles de ligne), puis R4 (leçon VTune : alias 4K dans `RasterizeTiles8Fx`, c24 et c48 à 1536, sans changer la largeur), puis choix de D-a2c.
- Réserve conservée avec R3-V : ClearDepth est limité par la mémoire ; « A et ClearDepth dérivent ensemble » ne prouve pas que les parties limitées par le calcul ont dérivé de même. Quand RenderVerts dépasse 10 µs, le croiser avec ClearDepth avant d'appliquer la seconde condition.

## Suite

Décision d'Onky sur R3-D9 (et R3-V) → D-a2d (boucles de ligne) → R4 (leçon VTune, alias 4K sur c24 et c48 à 1536) → choix de D-a2c. En parallèle, facultatif : sonde C0b à 1536 et 1552 (R3-g).
