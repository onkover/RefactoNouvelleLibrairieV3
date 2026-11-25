# Décisions — Lot D-a2b (boucle des tuiles sans branche de données)

*Échange du 15 novembre 2026. Artefact : « Lot D-a2b pas à pas » (export `Lot_Da2b_pas_a_pas.html`). Roadmap : `Plan_Phase_G_LibraryV3.md` (ligne D-a2b, prochaines étapes 2b à 2d, « Évolutions et dettes relevées par le lot D-a2b »). Lot précédent : `Résultats/Decisions_Lot_Da2.md` (D13, D18, D19).*

## Statut

**LIVRÉ le 15/11/2026 ; mesuré le 20/11 ; CLOS le 20/11 : D12 acté par Onky (défaut `LV3_RASTER_TILE_SPLIT 0`, code D-a2b conservé derrière le commutateur).** Aucune décision de ce fichier n'est actée tant qu'Onky ne l'a pas validée ; les décisions D1 à D11 sont des **propositions** du lot.

- **Base** : dépôts du 14/11 (LIB `8acd153`, EXE `4286a9c`) ; D-a2 présent (défaut `LV3_RASTER_TILES 16`), scène V5 présente.
- **Objet** : tester directement l'hypothèse H3 de D-a2 (la branche `if ((full >> j) & 1)` par tuile, imprévisible quand pleines et partielles alternent) en la supprimant : deux boucles par groupe, sur `full` puis `live & ~full`. Même image au bit près ; seul l'ordre de visite change à l'intérieur d'un groupe.
- **Commutateur** : `LV3_RASTER_TILE_SPLIT` (config.h §14b) : 0 = boucle D-a2, 1 = boucle D-a2b (défaut proposé). En-tête CSV `tsplit=`.

## Décisions proposées

| # | Proposition | Raison | Écarté |
|---|---|---|---|
| D1 | Une seule variable change : la boucle de visite des tuiles vivantes ; jugement, entrée de tuile, corps pleine / partielle identiques | Attribuer l'écart de mesure à H3 seule | Corriger H2 ou H4 dans le même lot |
| D2 | Trois gestes extraits en fonctions `LV3_FORCEINLINE` (`TileEntry8`, `TileFull8`, `TilePartial8`, structure `TileRows8`), appelées par les deux variantes | Même code machine dans B et B′ hors de la boucle ; pas de duplication | Lambdas (P5 : MSVC ne les inline pas) ; copier les corps |
| D3 | `LV3_RASTER_TILE_SPLIT` relayé par un paramètre de template `kSplit` à valeur par défaut sur `ForEachCoveredTile8Fx32` et `RasterizeTiles8Fx` | B et B′ depuis le même code ; la TNR compare les deux dans un seul binaire ; appels existants inchangés | Remplacer la boucle sans commutateur |
| D4 | Tri à l'échelle du groupe de 8 tuiles | Masques déjà disponibles ; ≤ 4 Ko de tampons par groupe (L1) | Tri par triangle |
| D5 | Pendant scalaire `ForEachCoveredPixelFxTiled` inchangé | Référence indépendante, ordre de D-a2 | Trier aussi le scalaire |
| D6 | Nouveau compteur `Tiles8Switch` (RASTER_STATS) : changements de nature entre tuiles vivantes consécutives, ordre D-a2, par `pext` ; `Tiles8Full`, `Tiles8Partial`, `LaneBlocks8` comptés par `popcnt` des masques au niveau du groupe | Prédire et vérifier H3 sur toute scène ; mêmes valeurs quelle que soit la variante | Compter dans les corps |
| D7 | Filet Debug `(full & ~live) == 0` (« D-a2b : tuile pleine non vivante ») | D-a2b suppose les pleines vivantes | — |
| D8 | Campagne A / B / B′ en séquences `A B B′ B′ B A`, B reconstruit depuis le nouveau code (`TILE_SPLIT 0`) ; c48 ×2, c24, c96, rooms ×2, V1 (42 runs), fan et c192 facultatifs ; refait c96 et c48 (D13 de D-a2) | Comparer B′ à un B de même placement de code | Comparer à l'ancien binaire de D-a2 |
| D9 | Résultat neutre (\|Δ\| ≤ 1 % partout) : D-a2b conservé | Plus de branche de données, code équivalent | Retour à D-a2 par défaut |
| D10 | Diagnostic R3 de H4′ sans code : fenêtre 1552 × 900, c24 et c48 en A / B / B′ | Tester H4′ sur la cible ; décide de la forme de D-a2c | Écrire tout de suite le Z-buffer à pas décalé |
| D11 | H4′, D-a2c redéfini et Z-buffer à pas décalé inscrits en roadmap, à décider plus tard | Règle du projet | — |

## Règle de décision (écrite avant les temps)

Adopté (défaut `LV3_RASTER_TILE_SPLIT 1`) si : TNR vertes (journaux des autres TNR inchangés) ; runs Debug sans assertion ; R1 conforme au compte près ; asm conforme ; **c48 : B′ ≤ B − 1 %** ; aucune scène (c24, c96, rooms, V1) avec **B′ > B + 1,5 %** confirmé par une seconde séquence. Neutre (±1 %) : conservé (D9). Régression > 1,5 % confirmée : défaut 0 et boucle à deux masques retirée au lot suivant.

## Vérifié hors moteur (15/11, VM Xeon 2 vCPU, g++ 13, copie de travail de la LIB)

- **Compilation** sans avertissement nouveau : défaut, `RASTER_STATS 1`, `ABLATION_PIXELS 1` et `2`, `SIMD 0`, `TILES 0`, `FIXED 0`, `TILE_SPLIT 0` ; `TILE_SPLIT 2` → `#error`. (Avertissement `kSmall` inutilisé sous `ABLATION_PIXELS 1` : antérieur.)
- **TNR** : `Test_Da2_Tiles` vert, journal identique à la référence de D-a2 (104 347 tuiles, 55 450 rejetées, 10 768 pleines ; 58 399 ; 319 replis), avec `TILE_SPLIT` 1 et 0, avec et sans RASTER_STATS. `Test_Da2b_TileSplit` vert, journal :
  - `vue 333x211 : 17548 groupes vivants (5565 mixtes), 8394 changements de nature (ordre D-a2) ; doubles visites A/B' 0/0, blocs pleins apres une partielle A/B' 88216/0, visites identiques oui`
  - `vue 211x151 : 8418 (2493 mixtes), 3670 changements ; 0/0 ; 37368/0 ; oui`
  - `vue 75x190 : 3859 (1155 mixtes), 1734 changements ; 0/0 ; 15688/0 ; oui`
  - `auto-controle (chaque triangle dessine deux fois) : 1946338 doubles visites`
- **Banc V5** (vrai noyau, scène reconstruite depuis les .obj, projection double + `SnapToSubpixel`) : comptages **identiques au R1 moteur de D-a2** pour c24, c48, c96, c192 (fan : 1 tuile sur 1,19 M, une lame) ; B et B′ : mêmes compteurs, même image, 1 382 400 pixels couverts.
- **Tiles8Switch** (4 vues) : c24 4 061, c48 9 470, c96 5 653, c192 3 224, fan 0. Groupes vivants / avec pleines / mixtes : c24 18 529 / 2 036 / 2 036 ; c48 8 558 / 4 792 / 4 790 ; c96 6 368 / 4 496 / 4 455 ; c192 4 498 / 3 469 / 2 785 ; fan 39 863 / 0 / 0.
- **Plafond de D-a2b** (décompte, au pire) : gain ≈ (α × changements − γ × groupes avec pleines) × pénalité / fréquence ; c48 ≈ 20 µs (≈ −1 %) avec α = 0,8, γ = 0,5, 17 cycles, 4,4 GHz, contre +206 µs à regagner. **H3 ne peut expliquer qu'environ 1/5 de la perte de c48.**
- **Temps VM au pas du moteur (1536 px), dessin seul, même binaire, 2 passes** : B′/B c24 −3,1 / −3,3 % ; c48 −3,6 / −1,4 % ; c96 −0,3 / −1,0 % ; c192 −0,6 / −1,4 % ; fan −1,3 / +0,8 %. B/A c24 +4,7 / +5,0 %.
- **Découverte : H4′ (pas de ligne de 6 144 o)**. c24 B/A selon le pas (même binaire) : 768 px −4,0 / −4,6 % ; 784 −3,7 / −3,8 % ; **1024 (4 096 o) +17,7 / +21,7 %** ; 1040 −6,5 / +0,6 % ; **1536 (moteur) +4,9 / +4,9 %** ; 1552 −4,3 / −5,6 %. La perte de c24 n'apparaît sur la VM qu'aux pas multiples de 2 048 o. Mécanismes proposés : alias 4 Ko (lecture du Z de la ligne r + 2 contre l'écriture en vol de la ligne r) et conflits d'ensembles L1. Non prouvé sur l'i7 : R3.
- **Contre-exemples mesurés** (pas 1536) : boucle 2 sans `& ~full` → image identique, pixels couverts +9,4 / +42 / +70 / +83 % (c24 / c48 / c96 / c192), temps −0,4 / +8,4 / +15,2 / +21,5 % ; chemin uniforme (aucun corps « pleine ») → −2,9 / +4,5 / +7,8 / +9,1 %.
- **cachegrind** (g++) : B′ exécute 3,5 à 4,2 % d'instructions de plus que B dans `RasterizeTiles8Fx` (sauvegardes de registres ymm aux entrées de boucle), 1 à 2 % de sauts en moins. À vérifier sur l'asm MSVC.
- **Méthode** : premier banc compilé avec `-mfma` → g++ a contracté l'interpolation de profondeur en `vfmadd` ; banc final en `-ffp-contract=off` sans `-mfma`.

## Prédictions (écrites avant la mesure)

- **R1** (B′_stats, `tiles=16`, `tsplit=1`) : compteurs communs identiques au R1 de D-a2 sur les 14 scènes ; `Tiles8Switch` exact sur V5 (4 061 / 9 470 / 5 653 / 3 224 / 0), 0 sur V1, V2, V3, V4 ; rooms entre 6 000 et 12 000 (borne 21 862) ; invariants Tiles8Switch ≤ 2 × Full et ≤ 2 × Partial ; B_stats = B′_stats.
- **Asm** : `RasterizeTiles8Fx<ShaderSolid,1>` sans test de `full` par tuile, deux boucles `tzcnt`/`blsr`/`jne`, 3 `vpermd` par tuile, un `andn` par groupe, 0 `vfmadd`, un seul `call` (repli), aucun `call` vers TileEntry8 / TileFull8 / TilePartial8 ; `<…,0>` comparable au listing du 14/11 ; `RasterizeBlocks8Fx` et `RasterizeTriangleT` identiques.
- **R2** : B′/B : c48 −0,5 à −3 % ; c24 0 à −2 % ; c96 0 à −2 % ; c192, fan, rooms 0 ± 1,5 % ; V1 0 ± 2 % (placement). B′/A : c48 +6 à +10 % ; c24 +2 à +4 % ; c96 −10 à −15 % ; c192 −7 à −12 % ; fan −50 à −60 % ; rooms −1 à −3 %.
- **R3** (fenêtre 1552) : si H4′ tient, c24 B/A −3 à −6 % (au lieu de +4,3 %) et c48 B/A nettement sous +10 % ; sinon H4′ réfutée sur la cible.

## Livrables

- Artefact « Lot D-a2b pas à pas » (pourquoi, carte du moteur, contexte, objectif, besoin et méthodes, 4 laboratoires : prédicteur de branchement, ordre de visite sur triangles réels, plafond par scène, adresses d'une tuile selon le pas ; variantes comparées ; décisions ; algorithme détaillé avant/après ; banc ; 7 étapes calculées couloir par couloir sur un groupe réel de c48 ; code avant/après par fichier ; 6 contre-exemples ; prédictions ; vérifier ; feuille de route ; exemples d'utilisation ; suite).
- Artefact annexe « Lot D-a2b — annexe : le parcours des tuiles » (16/11, à la demande d'Onky : l'ordre D-a2b du laboratoire 2 n'était pas intuitif) : trois niveaux de boucle, algorithmes des deux parcours et règle de numérotation, comparaison point par point, avantage et limites, laboratoires (groupe à composer, parcours côte à côte avec trace des masques, suite des sauts devant un prédicteur modèle : erreurs évitées sur la scène c24 0 %, c48 43 %, c96 24 %, c192 23 %), démonstration « l'ordre ne change ni l'image ni la mémoire », pas à pas scalaire / BMI, contre-exemples (parcours des 8 positions : jusqu’à ×1,4 (c192) à ×7,4 (fan) tests de plus que de tours de boucle). Export `Lot_Da2b_annexe_parcours.html`. Renvoi ajouté dans le laboratoire 2 de l'artefact principal.
- Fichier nouveau `Test/Test_Da2b_TileSplit.cpp` (EXE).
- Modifications avant/après : LIB `Core/config.h` (§14b), `Core/Profiler.h` (Tiles8Switch), `Core/Profiler.cpp` (`tsplit=` ×2), `Rendering/PixelStage8.h` (gestes extraits, `kSplit`, boucle à deux masques, compteurs, filet), `Rendering/Fragment.h` (`RasterizeTiles8Fx<Shader, kSplit>`) ; EXE `RunAllTests.cpp`, vcxproj, filters.

## Application et contrôles (19/11)

- **Compilation** Debug et Release : OK (Onky).
- **TNR** : `Test_Da2b_TileSplit` vert, journal **identique au compte près** à la prédiction du banc (17 548 / 5 565 / 8 394 / 88 216 ; 8 418 / 2 493 / 3 670 / 37 368 ; 3 859 / 1 155 / 1 734 / 15 688 ; auto-contrôle 1 946 338). `Test_Da2_Tiles` vert, journal inchangé (104 347 / 55 450 / 10 768 ; 58 399 ; 327 / 319). Autres TNR : journaux inchangés.
- **Runs Debug** : OK, aucune assertion.
- **Asm : NON RECEVABLE, à refaire.** Les deux listings `RenderSystem.asm` reçus ont un code identique (seuls les chemins `LibraryV3` / `LibraryV3-master` et le hachage de l'espace de noms anonyme diffèrent) et tous deux sont compilés depuis le `PixelStage8.h` **d'avant D-a2b** : lignes source 518 `live = _blsr_u32(live);` et 528 `if ((full >> j) & 1u)` (celles du commit `8acd153`), une seule boucle par groupe avec `shrx` + `test al, 1` + `je` par tuile, symbole `RasterizeTiles8Fx<ShaderSolid>` sans paramètre `kSplit` (pas de `$0A@` / `$00@` dans le nom décoré), aucune ligne `TileEntry8` / `TileFull8`. Hypothèses : listing non régénéré (RenderSystem.cpp non recompilé), ou LIB compilée depuis une copie non modifiée alors que l'EXE (qui compile la TNR) voit la copie modifiée. **À vérifier avant R1/R2** : un binaire B′ dont la LIB n'aurait pas D-a2b mesurerait B contre B. Contrôle sans asm : la 1ʳᵉ ligne du summary doit contenir `tsplit=` (écrit par Profiler.cpp de la LIB).

### Asm, second envoi (19/11, 08:41)

- **`8d6aa28f` (copie `LibraryV3-master`)** : toujours le code d'avant D-a2b (ligne 528 `if ((full >> j) & 1u)`). Cette copie n'a pas reçu les modifications : ne pas l'utiliser pour les binaires de mesure.
- **`e0a8fe16` (copie `LibraryV3`)** : code D-a2b, symbole `RasterizeTiles8Fx<ShaderSolid,1>` (`$00@`). **Mais compilé avec `LV3_RASTER_TILES 0`** : `RasterizeTriangleT<…,3,0,8,0>` et `IsTiledTri<0>`, aucun appel au noyau de tuiles (le noyau est émis mais jamais appelé). Ce n'est donc pas le binaire B′ (TILES 16, SPLIT 1), mais le corps du noyau est indépendant de `TILES` et peut être relu.
- **Corps de `RasterizeTiles8Fx<ShaderSolid,1>` : CONFORME** aux prédictions : deux boucles `tzcnt` / `blsr` / `jne` par groupe (pleines, puis partielles), **0 `shrx` / `test al, 1`** (plus aucun test de `full` par tuile), 2 `andn` (le second, `live & ~full`, positionne ZF : le test d'entrée de la boucle des partielles est gratuit), 3 `vpermd` par tuile dans chaque boucle, 0 `vfmadd`, 0 `pext` (RASTER_STATS 0), un seul `call` (repli `RasterizeBlocks8Fx`), aucun `call` vers TileEntry8 / TileFull8 / TilePartial8, 7 push, cadre 1 008 o (= D-a2). 706 instructions contre 659 pour D-a2 (+47 ; prédit +30 à +80).
- **Remarques (comparaison avec le D-a2 d'origine, faute de B reconstruit)** : `c0..c2`, `off0`, `off1` relus sur la pile à chaque tuile (comme D-a2). Ligne de tuile pleine : 33 instructions contre 32 (+2 copies registre à registre, −1 chargement). **Ligne de tuile partielle : 51 contre 47** (+4 `vmovdqu` registre à registre, `vB1` / `vB2` relus en mémoire au lieu de `bf`) : la pression sur les 16 registres ymm vue avec g++ existe aussi avec MSVC. Les copies registre à registre sont en général éliminées au renommage (latence nulle) mais occupent le frontal. **Prédiction amendée avant mesure : c24 B′/B −2 à +2 %** (au lieu de 0 à −2 %), 95 % de tuiles partielles et peu de changements de nature.
- **Encore attendu** : listing B reconstruit depuis la copie `LibraryV3` (`TILES 16`, `TILE_SPLIT 0`), pour une comparaison B / B′ sur le même code (D8) ; en-têtes des summaries `tiles=16;tsplit=0` (B) et `tiles=16;tsplit=1` (B′).

### Premier couple B / B′ (20/11, V5 fan, un run chacun)

- En-têtes conformes : `tiles=16;tsplit=0` (B) et `tiles=16;tsplit=1` (B′), `rasterstats=0`, copie modifiée bien utilisée.
- Render : B 5 232,4 µs, B′ 5 485,6 µs → **+4,8 % brut**. Thermomètres : ClearDepth +2,8 %, Clear +3,2 %, ClearColor +6,3 % (machine ≈ 3 % plus lente pendant B′ ; frame max B′ 23,7 ms, pic isolé) → **÷ ClearDepth +2,0 %, ÷ Clear +1,6 %**. RenderVerts (4,2 / 4,3 µs) inutilisable (résolution 100 ns).
- **Non recevable** (une paire, pas de séquence, pas de A) ; signal à surveiller : fan n'a aucune tuile pleine, la seule différence B / B′ y est la boucle des partielles. Hypothèse : les +4 copies registre à registre par ligne de tuile partielle vues dans l'asm de B′ (689 264 lignes testées par frame × 4 µops ≈ 2,8 M µops ≈ 0,1 ms ≈ +2 %), à confirmer par le listing de B (si B a les mêmes copies, l'hypothèse tombe).
- Suite : listing B (copie `LibraryV3`, TILES 16, SPLIT 0) ; séquence fan `A B B′ B′ B A` ; puis le R2 complet.

### R2, premiers résultats (20/11, séquences A B B′ B′ B A, script `Campagne_Da2b.bat`)

En-têtes conformes sur les 30 summaries (A `tiles=0`, B `tiles=16;tsplit=0`, B′ `tiles=16;tsplit=1`). Écarts = (X1 + X2) / (Y1 + Y2) − 1 sur la médiane de Render.

| Séquence | Validité | B/A | B′/A | **B′/B brut** | paires B′/B | ÷ ClearDepth | ÷ RenderVerts |
|---|---|---|---|---|---|---|---|
| fan s1 | **valide** (CD 3,0 %, \|A1−A2\| 0,0 %) | −56,9 % | −55,4 % | **+3,4 %** | +4,1 / +2,7 | +5,1 % | (4 µs, inutilisable) |
| c48 s1 | non valide : B′1 bridé (ClearDepth 203 contre ≈ 178, Clear +12 %) | +8,4 % | +16,9 % | +7,8 % | +14,4 / **+1,2** | +1,0 % | (5 µs) |
| rooms s1 | limite (CD 6,3 % dû au seul A1 ; \|A1−A2\| 1,4 %) | −2,4 % | −2,9 % | **−0,5 %** | −0,8 / −0,2 | −1,4 % | −1,7 % |
| rooms s2 | **valide** (CD 3,2 %, \|A1−A2\| 1,7 %) | −2,2 % | −1,0 % | **+1,1 %** | −0,2 / +2,5 | −0,1 % | −3,4 % |
| V1 s1 | non valide (\|A1−A2\| 12 %, CD 14,6 %) | −0,3 % | +1,2 % | +1,5 % | −5,8 / +9,8 | −0,4 % | −1,3 % |

Lecture provisoire (c48 s2, c24 et c96 non reçus) :
- **fan : régression confirmée** (+3,4 % sur une séquence valide, les deux paires du même côté ; le couple isolé du 20/11 donnait +4,8 % brut). Hors de la prédiction (0 ± 1,5 %) et au-delà du seuil de rejet (+1,5 %). fan n'a aucune tuile pleine : seul le corps de la boucle des partielles diffère, ce qui désigne les +4 copies registre à registre par ligne partielle vues à l'asm (≈ 689 000 lignes par frame).
- **c48 : pas de gain** (paire recevable B′2/B2 +1,2 %, ÷ ClearDepth +1,0 %) ; prédiction −0,5 à −3 % non tenue à ce stade. B/A +8,4 %, cohérent avec D-a2 (+10 à +14 %).
- **rooms : neutre** (−0,5 % et +1,1 %, thermomètres entre −3,6 et −0,1 %). B/A −2,2 à −2,4 %, comme D-a2 (−2,3 %).
- **V1 : séquence non valide** ; ÷ RenderVerts (thermomètre recevable sur V1) B′/B −1,3 %, B/A +1,2 % : aucun signe de régression ; à refaire seulement pour la forme.
- Si c48 s2, c24 et c96 confirment, la règle de décision conduit au **rejet** de D-a2b sous sa forme actuelle (défaut 0) : H3 ne rapporte rien de mesurable sur la cible (comme le plafond le laissait prévoir, ≈ −1 %), et la pression sur les registres coûte sur les tuiles partielles. Le listing B (copie `LibraryV3`, TILES 16, SPLIT 0) dira si cette pression est propre aux deux boucles.

### R2, second envoi (20/11 au soir) : c48 s2 et c24 s1

c48 s1 renvoyé à l'identique (déjà dépouillé). c96 non reçu.

| Séquence | Validité | B/A | B′/A | **B′/B brut** | paires B′/B | ÷ ClearDepth | ÷ Clear | ÷ RenderVerts |
|---|---|---|---|---|---|---|---|---|
| c48 s2 | **recevable, limite** (\|A1−A2\| 2,8 % ; ClearDepth de A1 à 1,08 × la médiane, les cinq autres runs à ±1 %) | +4,3 % | +6,1 % | **+1,7 %** | +0,8 / +2,7 | +1,7 % (+1,8 / +1,5) | +0,4 % | (5 µs, inutilisable) |
| c24 s1 | **non valide** : dérive thermique (A2 +11 % sur A1 ; ClearDepth B2 0,98 → B5 1,06 → A2 1,15 ; Clear de A2 +18 %) | −3,1 % | −1,3 % | +1,8 % | +5,5 / −1,7 | +4,6 % | +5,7 % | +5,3 % |

Lecture :
- **c48 : aucun gain, sur les deux séquences.** s2 donne +1,7 % (les deux paires du même côté, et le même chiffre une fois corrigé par ClearDepth) ; s1 donnait +1,2 % sur sa seule paire recevable. La prédiction (−0,5 à −3 %) est fausse et le critère d'adoption (c48 B′ ≤ B − 1 %) n'est pas rempli.
- **c24 : séquence à jeter**, mais aucun indicateur ne montre de gain : brut +1,8 % en moyenne, tous les thermomètres entre +4,6 et +5,7 %. B/A n'est pas lisible (A2 bridé ; contre A1 seul, B/A ≈ +2,3 %, dans la ligne du +4 % de D-a2).
- **Bilan R2** : fan +3,4 % (valide), c48 +1,2 / +1,7 %, rooms −0,5 / +1,1 %, V1 et c24 non valides sans signe de gain. **H3 est réfutée sur la cible** : supprimer la branche pleine/partielle ne rapporte rien de mesurable (le plafond calculé était ≈ −1 %), et la structure à deux boucles coûte sur les tuiles partielles (pression sur les registres, +4 copies par ligne partielle à l'asm).

### R2, dernier envoi (20/11) : c96 s1 (fan s1 renvoyé, identique)

| Séquence | Validité | B/A | B′/A | B′/B brut | paires B′/B | ÷ ClearDepth | ÷ Clear |
|---|---|---|---|---|---|---|---|
| c96 s1 | **non valide** : B′1 bridé (ClearDepth 1,15 × la médiane, Clear +15 %, frame 4,44 ms) ; A2 plus frais (ClearDepth 0,88) ; \|A1−A2\| 7 % | −3,5 % brut, **−11,8 % ÷ ClearDepth** | +7,1 % | +11,0 % | +23,4 / **−0,6** | +4,7 % (paire 2 : +1,5 %) | +4,6 % (paire 2 : +2,2 %) |

- Seule la paire B′2 / B2 (runs 4 et 5) est recevable : **−0,6 % brut, +1,5 % ÷ ClearDepth, +2,2 % ÷ Clear → neutre, pas de gain** (prédit 0 à −2 %).
- B/A corrigé par ClearDepth ≈ −12 %, cohérent avec D-a2 (−14 %).

## Synthèse de la campagne R2 (8 séquences, 48 runs + 1 couple isolé)

| Scène | Séquences | B′/B retenu | Prédit | B/A (rappel D-a2) |
|---|---|---|---|---|
| fan | s1 valide | **+3,4 %** (÷ CD +5,1 %) | 0 ± 1,5 % | −56,9 % (−55 %) |
| c48 | s1 (1 paire), s2 recevable | **+1,2 / +1,7 %** | −0,5 à −3 % | +8,4 / +4,3 % (+10 à +14 %) |
| c96 | s1 (1 paire) | −0,6 % brut, +1,5 % ÷ CD | 0 à −2 % | ≈ −12 % ÷ CD (−14 %) |
| c24 | s1 non valide | +1,8 % brut, +4,6 à +5,7 % corrigé | −2 à +2 % (amendé) | ≈ +2 % contre A1 (+4 %) |
| rooms | s1 limite, s2 valide | −0,5 / +1,1 % | 0 ± 1,5 % | −2,4 / −2,2 % (−2,3 %) |
| V1 | s1 non valide | −1,3 % ÷ RenderVerts | 0 ± 2 % | +1,2 % ÷ RV |

Constats : aucune scène ne gagne ; fan régresse au-delà du seuil ; B (D-a2) se retrouve partout conforme au lot D-a2. Verdict proposé inchangé (D12 : rejet, défaut 0).

## Verdict proposé (20/11, règle écrite le 15/11)

- Règle : adopté si c48 B′ ≤ B − 1 % et aucune scène B′ > B + 1,5 % confirmée ; neutre (±1 %) → conservé.
- Constat : c48 ≈ +1,5 % (pas de gain), fan +3,4 % confirmé (au-delà du seuil de rejet).
- **Proposition D12 : D-a2b rejeté ; `LV3_RASTER_TILE_SPLIT` passe à 0 par défaut.** Le code reste derrière le commutateur jusqu'à la clôture de D-a2c (il sert de témoin et de leçon) ; son retrait éventuel sera décidé avec le nettoyage du noyau (proposition D13, à décider).
- c96 (reçu ensuite) : neutre sur sa seule paire recevable, les deux boucles ne gagnent pas non plus sur les grands triangles.
- Le listing de B (copie `LibraryV3`, TILES 16, SPLIT 0) reste utile pour la leçon : il dira si les copies de registres sont propres aux deux boucles.
- **Priorité suivante : R3** (fenêtre 1552, c24 et c48, `A B` suffit puisque B′ est écarté), qui décide de la forme de D-a2c (H4′).

## Question de fond (20/11) : pourquoi aucun gain, et le tiling l'augmentera-t-il ?

Réponse détaillée dans l'artefact, section 19b (laboratoire 6). Modèle à deux termes : gain = (α × changements − γ × groupes avec pleines) × 3,9 ns ; coût = tuiles partielles × 1,8 ns (calé sur fan, où le gain vaut 0 : prédit +3,0 %, mesuré +3,4 %). Sans autre réglage : c48 +1,2 % (mesuré +1,2 / +1,7), c96 +0,6 % (−0,6 / +1,5), c24 +2,6 % (+1,8, non valide). Plafond absolu sur c48 (α = 1, γ = 0, coût 0) : 1,8 %. Le binning (D2), la grille alignée et le Z à pas décalé ne changent ni les changements ni les tuiles partielles : le gain ne grandira pas par le tiling ; seule l'AVX-512 (coût de registres ≈ 0) rendrait B′ légèrement gagnant (c48 ≈ −1 %). Vérification facultative de α : VTune, `BR_MISP_RETIRED.ALL_BRANCHES` sur c48, B contre B′.

## Décision d'Onky (20/11, 21:38)

- **D12 ACTÉ** : défaut `LV3_RASTER_TILE_SPLIT 0` ; le code D-a2b est conservé derrière le commutateur (Onky : « peut-être que le tiling + multithreading apportera une bonne surprise ») ; à remesurer après D2 (binning, multithread), après D-a2c et en AVX-512. D13 (retrait éventuel) sans objet pour l'instant.
- **R4 (VTune)** inscrit en roadmap (étape 2c-bis), à traiter dans un échange dédié avec une leçon VTune.
- **Run B isolé reçu** (`bench_V_V5_new_B`, fan, `tiles=16;tsplit=0`) : en-tête conforme au binaire B reconstruit depuis la copie `LibraryV3`. Render 6 589 µs contre 5 143 en R2, mais machine nettement plus lente pendant ce run (ClearDepth +18 %, Clear +27 %, Present +23 %, overhead 45 ns) : ÷ Clear le rapport est identique à R2 (16,3 contre 16,3). Run non recevable pour une mesure, conforme pour l'identité du binaire. Ce n'est pas le listing asm attendu (`RenderSystem.asm`, `/FAs`), qui reste facultatif.

## Listing de B (20/11, 21:41) : correction de l'explication de fan

- Listing `RenderSystem.asm` de B reçu : `RasterizeTiles8Fx<ShaderSolid,0>`, `IsTiledTri<16>` appelé (TILES 16, SPLIT 0), copie `LibraryV3`. 669 instructions (B′ 706, D-a2 d'origine 659).
- **Boucles de ligne de B et de B′ identiques au nom des registres près** : ligne pleine 32 instructions (2 copies registre à registre), ligne partielle 50 instructions (4 copies). Le D-a2 d'origine (listing du 19/11 `de44baa5`) : 31 et 46, sans copie. **Les copies viennent de l'extraction des gestes (`TileFull8`, `TilePartial8`, `TileRows8`), dans les deux variantes ; elles ne sont pas propres à B′.** L'hypothèse du 20/11 (« les +4 copies de B′ expliquent fan ») est **réfutée**.
- Par tuile partielle, B′ est un peu plus court que B (plus de `shrx` / `test al,1` / `je` ; `vB2` relu une fois par groupe au lieu d'une fois par tuile). Par groupe, B′ a ≈ 14 instructions de plus (95 + 42 contre 88 + 35 hors boucles de tuile) : ≈ 0,6 % au plus sur fan (39 863 groupes).
- **Le reste de la régression de fan (+3,4 %) n'est pas expliqué par le code.** Piste la plus probable : effet de placement du code (alignement des boucles, cache de µops / DSB), déjà mesuré au lot D-a2. Le coût fixe de B′ reste un fait mesuré (≈ 1,8 ns par tuile partielle ou ≈ 4,4 ns par groupe, les deux lectures prédisent les autres scènes à ±1 point) ; mécanisme ouvert, à examiner avec VTune (R4 : « Front-End Bound », DSB) ou `/FAcs` (adresses des boucles modulo 64).
- Conséquences : D12 inchangé. La section 19b de l'artefact est corrigée (le scénario « AVX-512 » ne supprime plus le coût, il devient « coût fixe nul »). **Nouvelle dette (à décider)** : l'extraction des gestes a alourdi B lui-même par rapport à D-a2 (46 → 50 et 31 → 32 instructions par ligne) ; pas de régression visible dans R2 contre les chiffres de D-a2, mais séances différentes.
- Leçon de méthode proposée : une explication tirée d'un listing se vérifie sur le listing de l'autre bras avant d'être écrite.

## Question d'Onky (20/11, 22:26) : les copies de registres, une optimisation à faire ?

Oui, mais elle concerne **B, c'est-à-dire le code par défaut** (D12), pas D-a2b. Relecture des boucles de ligne de B (listing du 20/11, 21:41) :

- **Ligne partielle, 50 instructions** : en fin de ligne, `e1` et `e2` (dans ymm8 / ymm9) sont recopiés **chacun deux fois** (`vmovdqu ymm2, ymm8` ; `vmovdqu ymm3, ymm8` ; `vmovdqu ymm0, ymm9` ; `vmovdqu ymm4, ymm9`) vers les registres que lit le corps de la ligne suivante ; les pas `vB1` et `vB2` sont relus sur la pile à chaque ligne. Le D-a2 d'origine (46 instructions) gardait `e0`, `e1`, `e2` dans ymm5 / ymm6 / ymm7, incrémentés en place par des pas tenus en registres (ymm12 / 13 / 14), sans copie. Cause probable : `TileRows8` passé par valeur et pression sur les 16 registres ymm ; l'allocateur de MSVC ne fusionne plus la valeur portée par la boucle et la valeur lue par le corps.
- **Second défaut, plus ancien (déjà dans le D-a2 d'origine)** : le test de `flipped` est fait **à chaque ligne** (`test dil, dil` ; `je` ; puis `jmp` + 1 `vmovups`, ou 2 `vmovups`), dans les lignes pleines comme partielles, et aussi dans `RasterizeBlocks8Fx` (chemin de tous les triangles AVX2 non tuilés, `PixelStage8.h` l. 306 et 391). Or `flipped` est constant par triangle, et même toujours vrai pour une face avant (`Rasterizer.h` l. 177 : `s.flipped = (area < 0)`, faces arrière déjà rejetées). Remède : échanger une fois par triangle les attributs 1 et 2 du sink (le shader reçoit alors w1 et w2 sans échange) ; même image au bit (même produit, mêmes opérandes).
- **Ordre de grandeur** : ≈ 7 à 8 µops par ligne de moins sur ≈ 50 (ligne partielle) et ≈ 5 sur ≈ 32 (ligne pleine). Les copies registre à registre sont en général éliminées au renommage (latence nulle, pas de port d'exécution) mais occupent les 6 places par cycle du renommage de Golden Cove. Plafond si la boucle est limitée par le frontal : fan ≈ 689 000 lignes partielles × 7,5 µops ≈ 5,2 M µops ≈ 0,86 M cycles ≈ 195 µs ≈ 3,8 % du Render. Attendu plus bas, car la boucle attend aussi la mémoire (`vmaskmovps`, `vpmaskmovd`) : **fan −1 à −3 %, rooms −0,5 à −1 %**, à mesurer.
- **Proposition (à décider)** : un lot court « boucles de ligne » (nom provisoire D-a2d), **après R3** (les binaires A et B doivent rester figés pendant R3). (1) `flipped` résolu une fois par triangle ; (2) allocation des lignes rétablie (e0..e2 locaux incrémentés en place, pas en registres ou passés par référence) ; vérification au listing : ligne partielle ≤ 43 instructions, ligne pleine ≤ 28, 0 copie, aucun test de `flipped` dans une boucle de ligne ; TNR image et Z identiques au bit ; mesure A/B (avant / après) en `A B B A` sur fan, c24, c48, rooms. Profite aussi à B′ (comparaison D-a2b conservée).
- **Décision d'Onky (21/11, 05:42) : lot accepté, à faire juste après R3.** Inscrit au `Plan_Phase_G_LibraryV3.md`, « Prochaines étapes » 2e (fusionné avec la version du lot R3).

## Suite

Appliquer → TNR → runs Debug → asm (B et B′) → R1 → R2 (42 runs) → R3 (12 runs) ; puis décision D-a2b et choix de D-a2c (Z-buffer à pas décalé si R3 confirme H4′ ; ordre ligne par ligne dans un groupe sinon ; P1 à défaut).
