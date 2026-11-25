# LibraryV3 — Plan à suivre, phase G

*État au 21 novembre 2026 (matin, après le lot R3 et la décision « boucles de ligne ») — ROADMAP de la phase G : ce qui reste à faire (prochaines étapes, chantiers futurs, dettes, décisions en attente) et les règles de méthode. Les résultats détaillés de chaque chantier clos ou livré sont dans le dossier `Résultats/` du projet, un fichier par chantier ; ce document n'en garde qu'une ligne de statut.*

## Où en est-on

| Étape | Statut | Document |
|---|---|---|
| LOD des rochers (étapes 1 à 4d) | Terminé | A14 (PDF) |
| 1 — LOD des sphères (LOD optionnel pour tout mesh) | Terminé | A14a (PDF) |
| 2 — `Clear` (stores non temporels SSE2) | Terminé | A15 (PDF) ; `Résultats/Résultats_Lot_PH4CL_Clear.md` ; artefact « Lot PH4-CL — Le chantier Clear, pas à pas » (reconstruit le 20/11) |
| 3 — Dettes (Serializer, JSON, ResourceManager, nettoyage) | Terminé le 11/10 | A16, A17, A18 ; `Résultats/Etapes_3_et_4.md` |
| 4 — Coût fixe de `Render` + rendu en point | Terminé | A19, A20 ; `Résultats/Etapes_3_et_4.md` |
| **Chantier B — vectorisation de l'étage géométrie** | **Terminé : B0, B0b, B1, B2, B4, B3, B5 faits et clos** (pistes B3c, B4c, B5b-d, B6, FMA rangées) | `Résultats/Chantier_B_Vectorisation.md` ; A21, A22, A23 ; artefacts « Lot B3 / B4 / B5 pas à pas » |
| **Chantier C — dessin des survivantes (pixels)** | **C0 clos le 21/10** (R1 + R2) ; **décidé : C1 → C2 → C3** ; **C1 clos le 23/10** (commit LIB `fc2e280` / EXE `d0d66f1` ; exact ; V1 +6,8 %, V2 ≈ −2 %, L bruit) ; **C2 + C2b clos le 25/10 (V2 Render ×2,27, V1 inchangé, L non ralenti) ; commit `deb5286` / `aac839d`** ; **C3 ACCEPTÉ le 29/10 (V1 étage faces −34,6 %, L −1,5 %, V2 +0,4 % sur 16 runs) ; commité (LIB `cd6ec83` / EXE `d5da759`) → **chantier C CLOS**** | `Résultats/Chantier_C_Dessin_des_survivantes.md` ; artefacts « Lot C0 … C3 pas à pas » |
| **Lot V3 — scène de niveau FPS + découpage C0** | **Clos le 31/10** : découpe en 6 pièces retenue (−25,5 %) ; arêtes 41 % du Render → **jalon D ouvert** (Giesen D-a1 puis Abrash D-a2) | `Résultats/Lot_V3_Scene_FPS.md` ; artefact « Lot V3 pas à pas » |
| **Jalon D — D-a1 : arêtes en virgule fixe (Giesen)** | **Mesuré et conservé le 03/11** : rooms −13,9 %, V2 0 %, V1 +4 à +5 % (coût structurel accepté, voir D-a1b) ; ablation : arêtes −51 %, préparation −22 %, fragments +87 % ; **commité le 03/11 (LIB `e97d976` / EXE `90c144b`), configuration de production vérifiée le 04/11** | `Résultats/Lot_Da1_Giesen.md` ; artefact « Lot D-a1 pas à pas » |
| **D-a1b — barycentriques tirées des E' entiers** | **ACCEPTÉ le 06/11** : rooms −6,5 %, V2 ≈ −7 %, V1 0 % ; TNR, filet C3, asm et R1 conformes ; **commité (LIB `93757b1` / EXE `b58c132`)** (banc hors moteur : dessin −8,9 %, 0 couleur changée) ; moitié « 24.8 dès l'étage sommets » **rayée (mesure, décision du 04/11)** ; V1 C B B C : tout le surcoût de Giesen sur V1 est dans l'accrochage (RenderVerts +35 µs) | `Résultats/Lot_Da1b_Barycentriques.md` ; artefact « Lot D-a1b pas à pas » |
| **D-a1d — accrochage des sommets par nombre magique** | **LIVRÉ le 06/11, à appliquer et mesurer** : `x + 1,5·2^15 − 1,5·2^15` au lieu de `round_ps` + `×1/256` ; image identique (exhaustif sur 2,37 G float), −0 → +0 seulement ; banc étage sommets 199 → 187 µs ; prédit V1 RenderVerts −8 à −20 µs. **Constaté le 10/11 : présent dans le dépôt** (nombre magique dans `Viewport.h`, `Test_Da1d_Snap`) ; mesure non reportée | `Résultats/Lot_Da1d_Nombre_magique.md` ; artefact « Lot D-a1d pas à pas » |
| **D-a1c — petits triangles sur les blocs flottants de C2** | **ACCEPTÉ le 09/11 (défaut `LV3_RASTER_SMALL_FLOAT 8`) ; commité (LIB `e7cae94` / EXE `5203980`, « Chantier D-a1c »)** : V4 c3 −10,2 %, mix −4,9 % (12 runs), c5 −2 à −4 %, c7 ≈ 0, témoins V1/c12 0, rooms −2 %, V2 sans régression ; asm conforme (2 `cmp` 2048 sans branche, 0 vfmadd). Gain ≈ 11 ns − 1,4 ns/bloc par triangle routé : point mort ≈ W* = 8. Détail : `Résultats/Lot_Da1c_Petits_triangles.md` | — |
| **D-a2 — hiérarchie d'Abrash (tuiles 8 × 8 sur Giesen)** | **ACCEPTÉ le 14/11 (défaut 16), avec réserves** : V5 c96 −14 %, fan −55 %, c192 −7 à −12 %, rooms −2,3 % (+1,5 % ÷ RenderVerts), V2 conforme ; **c24 +4 %, c48 +10 à +14 %** (branche pleine/partielle imprévisible, H3) ; effet de placement du code ≈ −4 % sur RenderVerts dans B ; à commiter (scène V5 comprise) ; **suite D-a2b** (boucle des tuiles sans branchement). Historique : LIVRÉ le 10/11 (conditionnel) : commutateur `LV3_RASTER_TILES` (config.h §14, T* en px, défaut proposé 16) ; 8 tuiles jugées par registre (coins de rejet / d'acceptation, exacts), tuile pleine sans test (`Shade8Full`), tuile partielle = blocs Fx32 ; image identique au bit ; nouvelle scène V5 (grands triangles) ; banc : rooms −2 à −7 %, V5 c96 −25 %, fan −65 %, V2/V4 0. Règle de déclenchement du plan (arêtes ≥ 40 %) non remplie, lot fait en conditionnel (décision D0). **Constaté le 15/11 : présent dans le dépôt** (LIB `8acd153` / EXE `4286a9c`, scène V5 comprise) | `Résultats/Lot_Da2_Abrash.md` ; artefact « Lot D-a2 pas à pas » |
| **D-a2b — boucle des tuiles sans branche de données** | **CLOS le 20/11 : D12 ACTÉ par Onky** (défaut `LV3_RASTER_TILE_SPLIT 0`, code D-a2b conservé derrière le commutateur pour remesure après le tiling D2 + multithread, D-a2c, AVX-512) ; **commité le 20/11 (LIB `9678f47` / EXE `26ae1f8`, « Lot D-a2b » ; défaut 0 constaté dans `config.h` §14b)** ; mesure : R2 fan B′/B +3,4 % (séquence valide), c48 +1,2 / +1,7 %, rooms −0,5 / +1,1 %, c96 neutre (−0,6 % brut, +1,5 % ÷ ClearDepth), c24 et V1 non valides sans signe de gain ; H3 réfutée sur la cible ; les 4 copies de registres par ligne partielle sont aussi dans B (extraction des gestes, listing du 20/11) : traitées par le lot « boucles de ligne » (étape 2e). Historique : LIVRÉ le 15/11 : commutateur `LV3_RASTER_TILE_SPLIT` (config.h §14b, 0 = boucle D-a2, 1 = deux boucles sur `full` puis `live & ~full`, défaut proposé 1), en-tête CSV `tsplit=` ; trois gestes extraits en `LV3_FORCEINLINE` (`TileEntry8`, `TileFull8`, `TilePartial8`) ; nouveau compteur `Tiles8Switch` ; filet `full ⊂ live` ; TNR `Test_Da2b_TileSplit`. Banc (VM) : image et comptages identiques au R1 de D-a2, B′/B c24 −3 %, c48 −1,4 à −3,6 %, autres ±1,5 % ; plafond calculé ≈ −1 % sur c48 (H3 ne peut expliquer qu'≈ 1/5 de la perte). **Découverte du banc : H4′** (pas de ligne 6 144 o = 1536 px : alias 4 Ko / conflits L1 à 2 lignes dans une tuile) ; c24 B/A +4,9 % au pas du moteur, −4 à −6 % au pas 1552. Diagnostic R3 proposé (fenêtre 1552) | `Résultats/Decisions_Lot_Da2b.md` ; artefact « Lot D-a2b pas à pas » |
| **R3 — diagnostic de H4′ (fenêtre de 1552 px), sans code** | **Protocole LIVRÉ le 20/11, à mesurer** : largeur portée par `config.json` (racine de contenu de l'EXE), clé `screen.width` (lue par `ProgrammeConfig` → `cfg.screenWidth` → `FrameW` → `db.Resize`, texture SDL, `BuildLayout`) ; 1536 → 1552 (+64 o, pas multiple de 64 conservé) ; mêmes binaires A (`tiles=0`) et B (`tiles=16;tsplit=0`), deux racines de contenu passées en argument positionnel, `LV3_CONTENT_ROOT` vidée ; séquence imbriquée `A₁₅₃₆ B₁₅₃₆ A₁₅₅₂ B₁₅₅₂ B₁₅₅₂ A₁₅₅₂ B₁₅₃₆ A₁₅₃₆`, c24 ×2 et c48 ×2 (32 runs), c96 facultatif ; grandeur décisive Δ = (B/A)₁₅₅₂ ÷ (B/A)₁₅₃₆ − 1 ; règle écrite avant la mesure (confirmée si Δc24 ≤ −3 % sur deux séquences valides et Δc48 < 0 ; réfutée si \|Δc24\| ≤ 1,5 % ; sinon R4) ; script `Campagne_R3.bat` dans l'artefact | `Résultats/Decisions_Lot_R3.md` ; artefact « Lot R3 — Le diagnostic H4′ (fenêtre de 1552 px), pas à pas » |

---

## Rappels de méthode (inchangés, obligatoires)

- Toute comparaison se fait **par paires A/B dans un même lot**, ordre A B B A. D'un jour à l'autre, la machine dérive de 12 à 35 %.
- **Mesurer sur secteur, mode d'alimentation « performances »** : le 18/10, un lot en mode économique tournait ≈ ×1,6 plus lentement sur tout le moteur, avec des runs instables. Lot refait en pleine puissance.
- **M19** : un run est recevable si `overhead_ns` reste à ±30 %, et `Animation` / `LocalXform1` à ±15 %. Sur les scènes V, `Animation` et `LocalXform1` valent environ 1 µs (résolution 100 ns) : le critère n'y est pas applicable, on se fie à un thermomètre interne.
- Les compteurs (`TrisRasterized`, `TrisEarlyRejected`, `FacesSubmitted`, `PointSplats`, `LodLevel*`, `TrisObjRejected`…) sont actifs dès `LV3_PROFILE=1` : on les compare dans les summaries des runs de mesure. `LV3_RASTER_STATS=1` et `LV3_LOD_STATS=1` sont des runs de **comptage** (temps non recevables, M10).
- **Thermomètre interne** : quand un lot dérive, on divise par une zone mesurée dans la même frame et dont le code est inchangé entre A et B. B4 : `RenderVerts`. B3 : `Clear` (et l'étage faces `Render − RenderVerts`, vérifié stable contre `Clear`). B5 : `Clear` croisé avec `RenderVerts` (inchangés tous deux). Toujours **croiser deux thermomètres** avant de conclure à une régression d'une zone non modifiée.
- **Nature du thermomètre (21/10)** : `Clear` est limité par la bande passante mémoire, donc presque insensible à la fréquence du cœur. Il ne peut pas écarter une hypothèse « fréquence ». Pour elle, le thermomètre est une zone limitée par le calcul (`RenderVerts`).
- **V2 est bimodal** (constaté au lot B3 et au lot B5) : `Render` saute entre ≈ 2,06-2,23 ms et ≈ 2,54-2,76 ms d'un run à l'autre, à code et compteurs identiques, sans que `Clear` ne bouge. Sur V2, un écart A/B isolé ne prouve rien ; il faut au moins deux runs de chaque côté dans le même mode.
- **Vérifier la 1ʳᵉ ligne de chaque summary** (`scene=…`, `ablation=…`, `fixed=…`, `simd=…`, depuis D-a1c `smallfp=…`, depuis D-a2 `tiles=…`, depuis D-a2b `tsplit=…`, depuis R3 `res=…`) avant de livrer un lot.
- **Règles ajoutées au lot D-a1 (03/11)** : une séquence A B B A est valide si le témoin `ClearDepth` varie de moins de 5 % **et** si |A1 − A2| < 3 % (ClearDepth ne voit pas une baisse de fréquence) ; le témoin se compare entre passages d'un **même mode** d'ablation (il change avec le mode) ; **un palier (S − M, E − S, N − E) ne se calcule qu'avec des modes mesurés dans la même séance** — si besoin, séquence entrelacée (ex. M0 M1 S1 S0 S0 S1 M1 M0, exécutables compilés d'avance).
- **Lire l'assembleur Release, jamais le Debug** : `/FAs` sur le seul `.cpp` concerné. Méthode : chercher `<Fonction>@LV3 ... PROC`, délimiter la boucle (`$LLn@...` → `jb $LLn@...`), y chercher `vfmadd` (aucun) et `call` (seuls ceux attendus).
- **MSVC compile `_mm256_load_ps` / `_mm256_store_ps` en `vmovups`** : une adresse mal alignée ne lève aucune faute en Release. L'alignement sert la performance (pas de ligne coupée) ; seules les `LV3_ASSERT` d'alignement le contrôlent. Un chargement peut aussi disparaître dans l'opérande mémoire d'un `vmulps` : c'est la forme optimale.
- **ABI Windows x64** : la moitié haute des ymm n'est pas préservée à travers un appel. Une constante 256 bits diffusée hors de la boucle est rangée sur la pile et relue après chaque `call` (`DrawTriangle`) : coût L1, négligeable.
- **Bit-exactitude** : un pas de vectorisation est valide seulement si les compteurs raster sont identiques. Pas de FMA ni de `rcp` sans décision explicite. `EdgeFunction` n'utilise **jamais** `fma(a, b, −c·d)` : cela casse le zéro exact et l'antisymétrie (coutures). **Jamais d'arêtes incrémentales en float** (mesuré le 21/10, voir C0). **Une TNR de bit-exactitude compare tout l'état produit, Z-buffer compris** (C1).
- **Validation d'un pas vectoriel, dans l'ordre** : test hors moteur → TNR → run Debug des 3 scènes avec le filet (aucune assertion) → asm Release → lot appairé.
- **Règle ajoutée au lot D-a1d (06/11)** : une TNR qui compare deux implémentations d'une même formule (B3 : AVX2 contre scalaire) ne voit pas une erreur dans la formule ; toute formule nouvelle reçoit aussi une **référence indépendante** (D-a1d : double + nearbyint).
- **Règle ajoutée au lot D-a1c (08/11)** : un décompte d'instructions ne désigne pas le gagnant entre deux boucles quand le triangle est petit (latence à l'entrée de boucle) ; la conception se choisit au banc, la décision se prend au lot A/B.
- **Règles ajoutées au lot D-a2 (14/11)** : run écarté et refait si RenderVerts > 1,15 × la médiane de la scène (si ≥ 10 µs, sinon ClearDepth) : portable bridé ; un témoin hors ±2 % du bon côté se teste contre l'effet de placement (RenderVerts, code non touché, bouge-t-il aussi ?) ; le banc VM surestime les gains (≈ ×2) et ignore le prédicteur de branchement de la machine cible.
- **Règle ajoutée au lot D-a2 (10/11)** : un banc se mesure **dans la scène entière** (rdtsc par triangle, par classe), jamais sur un sous-ensemble isolé : les mêmes grands triangles gagnent −31 à −33 % seuls, −19 à −24 % en contexte. Un banc sur VM se valide par un contrôle A/A (ici ±3 %).
- **Règles proposées au lot D-a2b (15/11), à confirmer** : (1) un banc g++ se compile avec `-ffp-contract=off` (g++ contracte `a*b + c` en `vfmadd` par défaut dès `-mfma`) ; (2) un banc qui compare deux ordres de parcours mémoire fait varier le **pas de ligne** des tampons (le moteur : 6 144 o) ; (3) une TNR « même image » compare aussi le **nombre de pixels couverts** et, quand l'ordre de visite change, les **visites pixel par pixel** (une double écriture à z égal laisse l'image intacte) ; (4) une campagne à trois bras se mesure en séquences `A B B′ B′ B A`, B et B′ construits depuis le même code (seul le commutateur change).
- **Règles proposées au lot R3 (20/11), à confirmer** : (1) un paramètre d'environnement (résolution, pilote…) se teste avec les **mêmes binaires** et se juge sur un **rapport de rapports** (Δ = (B/A)ₓ ÷ (B/A)ᵧ − 1), jamais sur un temps absolu ; (2) deux facteurs croisés se mesurent en **séquence imbriquée** (`A₁ B₁ A₂ B₂ B₂ A₂ B₁ A₁`), qui annule la dérive linéaire sur les trois comparaisons ; (3) les témoins (ClearDepth, |A1 − A2|) se jugent **à largeur égale** ; (4) une racine de contenu de mesure est une copie hors dépôt, jamais le `config.json` du dépôt modifié puis remis.

---

## Prochaines étapes (dans l'ordre)

1. **D-a1d — mesurer** (livré le 06/11 ; présent dans le dépôt, constaté le 10/11). Protocole et règle : `Résultats/Lot_Da1d_Nombre_magique.md`.
2. **D-a2 — commiter** (ACCEPTÉ le 14/11, défaut 16, avec réserves) : LIB (config.h, Profiler.h/.cpp, Rasterizer.h, PixelStage8.h, FrameBuffer.h, Fragment.h), EXE (Test_Da2_Tiles.cpp, RunAllTests.cpp, vcxproj et filters) et **scène V5** (5 .obj/.mtl, 5 .json, générateur). Décisions : `Résultats/Decisions_Lot_Da2.md` ; résultats : `Résultats/Lot_Da2_Abrash.md`. **Constaté le 15/11 : présent dans les dépôts** (LIB `8acd153`, EXE `4286a9c`).
2b. **D-a2b — boucle des tuiles sans branche de données** (**CLOS le 20/11, D12 acté : défaut 0, code conservé ; commité le 20/11 (LIB `9678f47` / EXE `26ae1f8`), constaté au lot R3 ; voir `Résultats/Decisions_Lot_Da2b.md`**) : deux boucles sur les masques `full` puis `live & ~full` au lieu du `if ((full >> j) & 1)` par tuile ; commutateur `LV3_RASTER_TILE_SPLIT`. Protocole : TNR (`Test_Da2b_TileSplit`) → runs Debug → asm Release (B et B′) → R1 (B′_stats, B_stats ; `Tiles8Switch` exact sur V5 : 4 061 / 9 470 / 5 653 / 3 224 / 0) → R2 A / B / B′ en séquences `A B B′ B′ B A` : c48 ×2, c24, c96, rooms ×2, V1 (42 runs ; refait c96 et c48, décision D13 de D-a2). Règle de décision : adopté si c48 B′ ≤ B − 1 % et aucune scène B′ > B + 1,5 % confirmé ; neutre (±1 %) → conservé (D9). Prédit : c48 B′/B −0,5 à −3 %, B′/A encore +6 à +10 %. Détail : `Résultats/Decisions_Lot_Da2b.md`.
2c. **R3 — diagnostic de H4′, sans code** (**prochaine étape ; protocole livré le 20/11, à mesurer**). Fichier et clé : `config.json` de la racine de contenu de l'EXE, `"screen": { "width": … }` (1536 → 1552) ; les entrées `viewports[].largeur` ne servent qu'aux séparateurs et ne changent pas le pas. Mêmes binaires A (`LV3_RASTER_TILES 0`) et B (défaut : `tiles=16;tsplit=0`, B′ écarté par D12) ; deux racines (`W1536`, `W1552`) passées en argument positionnel, `LV3_CONTENT_ROOT` vidée par le script (elle prime sur l'argument). Séquence imbriquée par scène : `A₁₅₃₆ B₁₅₃₆ A₁₅₅₂ B₁₅₅₂ B₁₅₅₂ A₁₅₅₂ B₁₅₃₆ A₁₅₃₆` ; c24 ×2, c48 ×2 (32 runs + 1 jetable), c96 ×1 facultatif. Contrôles : `res=`, `tiles=`, `tsplit=`, `views=4`, `FbDbAlias4K`, compteurs identiques A/B à largeur égale. Grandeur décisive : Δ = (B/A)₁₅₅₂ ÷ (B/A)₁₅₃₆ − 1. Prédit si H4′ tient : c24 (B/A)₁₅₅₂ −3 à −6 %, Δ −6 à −10 % ; sinon Δ 0 ± 1,5 %. Règle : confirmée si Δc24 ≤ −3 % sur les deux séquences valides et Δc48 < 0 → D-a2c = Z-buffer à pas décalé ; réfutée si |Δc24| ≤ 1,5 % → D-a2c = ordre ligne par ligne ; sinon une séquence de plus puis R4. Détail : `Résultats/Decisions_Lot_R3.md` et l'artefact « Lot R3 pas à pas » (script `Campagne_R3.bat`, laboratoire de dépouillement).
2c-bis. **R4 — mesure des erreurs de prédiction avec Intel VTune** (facultatif, **prochain échange dédié** : leçon VTune à prévoir, Onky ne connaît pas encore l'outil ni la lecture de ses résultats) : c48, binaires B et B′, analyse « Microarchitecture Exploration », compteur `BR_MISP_RETIRED.ALL_BRANCHES` restreint à `RasterizeTiles8Fx`. Objet : mesurer α (part des changements de nature mal prédits) ; si B − B′ < ≈ 7 000 erreurs par frame, α < 0,8 et le plafond de D-a2b est encore plus bas. La leçon couvrira : installation, symboles (PDB Release), réglages de collecte, lecture des vues (Top-down / Bottom-up, filtres par fonction, compteurs), pièges d'un portable bridé. Réutilisable pour H4′ (compteurs d'alias 4K `LD_BLOCKS_PARTIAL.ADDRESS_ALIAS`, conflits L1).
2d. **D-a2c — selon R2 et R3, à décider** : Z-buffer à pas décalé (si R3 confirme H4′) ou ordre de visite ligne par ligne dans un groupe (si R3 réfute H4′), ou P1 (réglage de T*).
2e. **D-a2d — boucles de ligne** (**ACCEPTÉ le principe par Onky le 21/11 : à faire juste après R3**, les binaires A et B devant rester figés pendant R3 ; à articuler avec D-a2c si R3 confirme H4′ : mesurer d'abord l'un puis l'autre, pas ensemble). Deux gestes, image et Z identiques au bit : (1) **`flipped` résolu une fois par triangle** : aujourd'hui `test` + `je` + 1 ou 2 `vmovups` à chaque ligne (lignes pleines et partielles des tuiles, et `RasterizeBlocks8Fx`, `PixelStage8.h` l. 306 et 391 : tous les triangles AVX2), alors que `flipped` est constant par triangle (toujours vrai pour une face avant, `Rasterizer.h` l. 177) ; remède : échanger une fois les attributs 1 et 2 du sink, w1 et w2 passés sans échange ; (2) **allocation des lignes rétablie** : supprimer les 4 copies registre à registre de la ligne partielle et les 2 de la ligne pleine nées de l'extraction des gestes (`TileRows8` par valeur ; e0..e2 incrémentés en place, pas en registres ou par référence, comme le D-a2 d'origine : 46 et 31 instructions, 0 copie). Cibles asm : ligne partielle ≤ 43 instructions (50 aujourd'hui), ligne pleine ≤ 28 (32), 0 copie, aucun test de `flipped` dans une boucle de ligne. Plafond fan (frontal limitant) ≈ 3,8 % ; attendu fan −1 à −3 %, rooms −0,5 à −1 %. Protocole : artefact « Lot D-a2d pas à pas », TNR (Test_Da2_Tiles, Test_Da2b_TileSplit, tests des blocs) → runs Debug → listing → mesure avant/après en `A B B A` sur fan, c24, c48, rooms. Détail : `Résultats/Decisions_Lot_Da2b.md` (section « les copies de registres, une optimisation à faire ? »).
3. **D2 — tuiles / binning** : avec la leçon L13 (la grille de D-a2, partie du coin de la boîte, devra être alignée sur l'écran).
4. Décision « voie D-a » à prendre avant L09.

## Chantiers futurs et pistes issus des chantiers B et C

- Scène dominée par les pixels à construire si V2 reste instable (même méthode que les scènes V).
- **Pistes B5 (sans urgence)** : B5b, pré-filtre sur le chemin Intersect avant clipping (l'identité vaut pour tout morceau du plan à w > 0) ; B5c, ne projeter que les sommets utilisés par des faces non rejetées ; B5d, sortir les 4 pointeurs `planes.*.data()` de la boucle des lots (4 lectures scalaires par lot, gain non mesurable seul).
- **B3c** (sans urgence) : `TransformPositions` (chemin Intersect) en SoA, même noyau sans division.
- **B6** — AVX-512 par dispatch CPUID (i9 seulement), alignement 64 o (déjà en place), 32 registres (fin des débordements de B3).
- **FMA** : décision séparée, sur MulRow / ProjectClip / ToRaster seulement, tous les chemins à la fois ; jamais dans `EdgeFunction`.
- **B4c** (sans urgence) : constantes 256 bits et viewport hors de la boucle des lots (≈ 1,5 instruction par face).
- **C3 généralisé** (non lancé) : boîtes étroites de 2 à 4 centres, seulement si une mesure montre un coût.
- **Bimodalité de V2** (hypothèse H3, jumeau SMT) : expérience décisive non faite — occuper le processeur logique 3 (`start "" /affinity 8 cmd /c "for /l %i in (0,0,1) do @rem"`) et voir si V2 passe en mode lent. Détail : `Résultats/Chantier_C_Dessin_des_survivantes.md`, section C0b.
- **Hi-Z par tuile 8 × 8** (issu de D-a2, non lancé) : profondeur la plus lointaine par tuile, tuile entièrement cachée sautée avant tout fragment. À mesurer d'abord (rooms : pass/cov 71 % sur les triangles de 16-31 px, 96 % sur les ≥ 64 px).

## Dettes techniques relevées par les chantiers B et C

- **Miroir** : avec une échelle négative, le test écran (aire raster < 0 = avant) jette les faces qui regardent la caméra : l'objet est dessiné à l'envers. B3 et B5 ont le même comportement (E suit le signe de det(model)). Aucune scène actuelle n'a d'échelle négative. Correction prévue : inverser la convention quand det(model) < 0 (comme Unity), sur les deux tests à la fois (voir aussi C1-5).
- **Z NaN** : `DepthTestAndSet` (`z <= d`) laisse passer un z NaN, qui est écrit dans le Z-buffer ; ensuite tout fragment passe sur ce pixel. Aucune scène de mesure n'en produit.
- **≈ 4 ns par appel du noyau `RasterizeTriangleT`** (relevé en C1 : prologue xmm6-15, cadre, copie du ctx) : retiré pour V1 par C3, reste payé par les autres triangles.
- **Relevés en C3, non corrigés** : `FaceColor` calculée avant le test de couverture (≈ 10 µs sur V1) ; cookie /GS par appel d'`EmitTriangleList8`.

---

## Dettes / correctifs à décider plus tard

Écarts, dettes et correctifs relevés mais non tranchés. Rien dans cette section n'est acté : chaque entrée attend une décision d'Onky.

### Décisions en attente — socle L01 (relevé du 07/11/2026)

**Aucune décision à ce stade : tout ce qui suit reste à acter.**
Origine : artefact « Lot L01 pas à pas — Config, Enums & PCH », section 5 « Écarts entre la leçon et le code d'aujourd'hui » (leçon 01 et points de la leçon 02 qui la touchent, comparés à l'état du dépôt LIB lu en novembre 2026).
La colonne « Nature relevée » décrit ce que l'écart *est* ; ce n'est pas un verdict. La colonne « À décider » dit ce que l'acter ou le refuser engage.

| # | Sujet | Nature relevée | À décider |
|---|-------|----------------|-----------|
| E1 | `config.h` : constantes en `constexpr` dans `LV3` (`M_PI`, `M_PI_2`, `TO_RADIAN`, `INV_PI`…) au lieu de `#define LV3_PI…` | Évolution + dette mineure | Entériner `constexpr` comme forme des constantes du moteur. Renommer ou non `M_PI`, `M_E`… : ces noms sont ceux des macros de `<cmath>` ; un `_USE_MATH_DEFINES` casserait des lignes correctes. |
| E2 | `config.h` : deux familles distinctes, tolérances (`EPSILON`, `EPSILON_FLOAT`) et gardes anti-division (`NEAR_ZERO`, `NEAR_ZERO_SQ`), au lieu d'un seul `LV3_EPSILON` | Évolution (née du bug n°3, leçon 05) | Entériner la séparation tolérance / garde comme règle. |
| E3 | `config.h` : `LV3_ASSERT` appelle `LV3::AssertFailed` (et non `assert`), `sizeof` quand éteint (annexe A10 §8.3) | Évolution | Entériner. Les règles R45 à R47 en découlent. |
| E4 | `config.h` : une douzaine de commutateurs de build vivent dans le PCH (asserts, journaux, mesure, ablations, variantes) | Évolution (née du bug 0.3, ODR) | Préciser D1 : le critère n'est pas seulement « stable » mais « recompilé par le bon nombre de fichiers ». Un commutateur qui change le code de tous les fichiers peut vivre dans le PCH. |
| E5 | `config.h` : `#define LV3_PROFILE 1` alors que le commentaire annonce « Défaut 0 » | Incohérence (toujours présente au 20/11, constatée au lot R3 : le `#pragma message` dit « defaut 0 ») | Remettre le défaut à 0 (la campagne de mesure passe par `LV3.Common.props`, consolidation C2), ou corriger le commentaire. |
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

### Décisions en attente — socle L02 (relevé du 07/11/2026)

**Aucune décision à ce stade : tout ce qui suit reste à acter.**
Origine : artefact « Lot L02 pas à pas — La bibliothèque mathématique », section 5 « Écarts entre la leçon et le code d'aujourd'hui » (document `Lecon_02_Mathematiques.md`, décisions et livraisons de l'échange de la leçon 02, comparés au dossier `Maths/` du dépôt LIB lu le 07/11/2026).
La colonne « Nature relevée » décrit ce que l'écart *est* ; ce n'est pas un verdict. La colonne « À décider » dit ce que l'acter ou le refuser engage.
A vérifier si nature relevée existe toujours.

| # | Sujet | Nature relevée | À décider |
|---|-------|----------------|-----------|
| L2-1 | Repère : le document de leçon dit main gauche, +Z dans l'écran ; le code est main droite, `Forward() = −Z`, verrouillé par trois `static_assert` dans `Vectorlib.h` (la livraison de l'échange portait `Forward() = +Z`, corrigé depuis) | Évolution + incohérence documentaire | Entériner le verrou `static_assert`. Corriger `Lecon_02_Mathematiques.md` (item déjà ouvert). |
| L2-2 | Convention : document en colonne-majeur `M·v`, T·R·S ; échange et code en row-major, vecteur-ligne, S·R·T, monde = local · parent | Évolution actée dans l'échange + incohérence documentaire | Corriger les sections 3 et 8 de `Lecon_02_Mathematiques.md`. |
| L2-3 | Profondeur : `[-1, 1]` livré dans l'échange (`Perspective` de `MatrixLib`), `[0, 1]` reverse-Z dans le code (L05) | Remise en question par L05 | Entériner `[0, 1]` reverse-Z dans la fiche de convention de L02 ; déclarer caduque la décision D14 de l'échange. |
| L2-4 | `LookAt`/`Perspective`/`Orthographic` commentés dans `MatrixLib.h` (≈ 45 lignes, convention `[-1, 1]`) ; projections en fonctions libres `LV3::Projection` | Évolution (règle L05 : `Matrix44` = algèbre pure) + dette « code commenté » | Supprimer le code commenté de `MatrixLib.h`. |
| L2-5 | `Quat::LookRotation` (Shepperd, cas vertical traité) remplace `Matrix44::LookAt` ; garde `dir.norm() <= EPSILON_FLOAT` | Évolution + dette (tolérance utilisée comme garde ; rejette toute direction < 3,16 cm) | Passer la garde de `dir` à `NEAR_ZERO_SQ`. Juger si la garde de `r` (seuil de colinéarité ≈ 1,8°) est voulue. |
| L2-6 | `Matrix44::inverseRigid()` ajouté | Évolution | Entériner ; ajouter un assert de précondition (lignes du bloc 3×3 unitaires). |
| L2-7 | `Transform` purement local (hiérarchie dans `WorldTransformSystem`) ; `Transform.cpp` ne contient plus que `#include "pch.h"` et du code commenté | Évolution (règle S4) + dette mineure | Retirer `Transform.cpp` du projet. |
| L2-8 | Matrice des normales : pas de `Mat3` (décision de l'échange) ; l'application de `(M⁻¹)ᵀ` aux normales n'a pas été vérifiée dans l'étage sommets | À vérifier d'abord | Lire l'étage sommets / l'éclairage : si les normales passent par la matrice monde, corriger (un scale non uniforme fausse l'éclairage). |
| L2-9 | Alignement : le document prévoit `alignas(16)` sur `Vec4`/`Mat4` ; aucun dans le code ; la vectorisation réelle (`ProjectPositions8`) est en SoA | Remise en question | Remplacer la règle par « tableaux SoA alignés `LV3_ALIGN_SIMD` », ou l'appliquer à `Matrix44` seule. |
| L2-10 | `Vectorlib.h` : `<cmath>` inclus deux fois, `<iostream>` pour les `operator<<` (header dans le PCH), `<cstdint>` peut-être inutile | Dette mineure | Nettoyage ; déplacer les `operator<<` dans un header de debug ou passer à `<iosfwd>`. |
| L2-11 | `MathUtils.h` (`Saturate`, `Lerp`) : utilise `LV3_FORCEINLINE` sans inclure `Compiler.h` ; pas d'`ApproxEqual` | Évolution + dette mineure | Ajouter l'include ; ajouter `ApproxEqual(a, b, tol)`. |
| L2-12 | `MathTypes.h` ne contient plus que le cœur ; `AABB3d.h` commenté, `Maths/Geometry/` vide | Évolution | Entériner ; la place de la géométrie se décide dans sa leçon dédiée. |
| L2-13 | `getEulerAngles()` : heuristique héritée, sans TNR | Dette | Écrire la TNR aller-retour Euler → Quat → Matrix44 → Euler, ou retirer la fonction si elle n'est pas appelée. |
| L2-14 | `norm()` renvoie la longueur au carré ; `normalize()` / `Normalized()` mélangent deux casses | À acter | Renommer `norm()` en `lengthSq()` (et passer aux noms courts `dot`/`cross`), ou garder les noms hérités (D17) et documenter. |

**Décisions de lot (artefact L02, section 4) :** D10 (famille templatée canonique), D11 (`LV3`), D12 (row-major, vecteur-ligne, S·R·T), D13 (main droite) : tranchées dans l'échange. D15 (tout en 4×4, pas de `Mat3`) et D16 (fabriques pures hors `Camera`, affinée en L05) : acceptées. D14 (profondeur `[-1, 1]`) : remplacée par L05. **D17** (noms hérités conservés) : proposée, se tranche avec **L2-14**. D18 (`operator[](int)`) : appliquée.

**Ordre de lecture suggéré (sans engagement)** : L2-8 (vérification, impact visuel possible), puis L2-5 (bug latent), L2-1 / L2-2 / L2-3 (documentation, sans coût), L2-4 / L2-7 / L2-10 / L2-11 (nettoyage), L2-9 et L2-14 (conventions à trancher), le reste étant des évolutions en place à entériner.

### Décisions en attente — syntaxe moderne L02b (relevé du 07/11/2026)

**Aucune décision à ce stade : tout ce qui suit reste à acter.**
Origine : artefact « Lot L02b pas à pas — Syntaxe C++ moderne », section 8. Les copies de headers
rangées dans le projet peuvent être antérieures au dépôt : vérifier E14 à E18 sur le disque d'abord.
A vérifier si nature relevée existe toujours.

| # | Sujet | Nature relevée | À décider |
|---|-------|----------------|-----------|
| E14 | Préfixe LV2_ survivant (LV2_ALIGN_SIMD, LV2_ALIGN_CACHE, LV2_FORCEINLINE) ; contredit E9 | Dette mineure, à vérifier | Tout renommer en LV3_, ou alias LV2_ le temps d'une migration. |
| E15 | alignas(16) demandé par L02 sur Vec4/Mat4 ; état actuel inconnu | À vérifier puis acter | Vec4f : 16. Matrix44f (64 o) : 64 pour ne jamais chevaucher deux lignes (16 : 3 cas sur 4). |
| E16 | Vec3f::normalize() const renvoie une copie, sans [[nodiscard]] | Dette, à vérifier | normalized() [[nodiscard]] + normalize() en place, ou [[nodiscard]] seul. |
| E17 | [[nodiscard]] absent des fonctions pures de Vec2f/Vec3f, présent ailleurs | Incohérence | Appliquer D19 à Maths/. |
| E18 | constexpr de la leçon 02 absent des copies du dépôt | À vérifier | Remettre constexpr (compatible avec LV3_FORCEINLINE), sauf ce qui appelle std::sqrt. |
| E19 | /we4834 /we4714 dans LV3.Common.props | Évolution proposée | Activer ou rester en avertissement. |

**Décisions de lot proposées, non actées** (numérotées D19 à D22, à la suite de la D18 de L02) :
- D19 : [[nodiscard]] sur toute fonction pure. Se tranche avec E16, E17, E19.
- D20 : noexcept partout dans Maths/ ; static_assert(is_nothrow_move_constructible_v<T>) pour tout type rangé dans un std::vector ; règle du zéro plutôt qu'un destructeur vide.
- D21 : LV3_FORCEINLINE sur les fonctions triviales des boucles chaudes ; LV3_NOINLINE pour isoler un chemin froid. Se tranche avec E19.
- D22 : LV3_ALIGN_CACHE pour les données écrites par plusieurs threads ; LV3_ALIGN_SIMD si un chargement aligné les consomme ; static_assert(alignof) systématique. Se tranche avec E15.


### Décisions en attente — socle L03 : ECS, SparseSet & ResourceManager (relevé du 08/11/2026)

**Aucune décision à ce stade : tout ce qui suit reste à acter.**
Origine : artefact « Lot L03 pas à pas — ECS, SparseSet & ResourceManager », section 8 « Roadmap, écarts et décisions à prendre », et les deux paragraphes de sa conclusion (« à traiter avant le lot caméra », « autres points ouverts »). Les écarts leçon/dépôt L3-1 et L3-17 viennent du recoupement avec `Carnet_de_route_LibraryV3.md` (bugs 53, 54, 62 et points 0.1 à 0.3).
La colonne « Nature relevée » décrit ce que l'écart *est* ; ce n'est pas un verdict. La colonne « À décider » dit ce que l'acter ou le refuser engage.
A vérifier si nature relevée existe toujours : les points L3-2 à L3-6 s'appuient sur des fichiers collés le 08/11 (`Component.hpp`, `Registry.hpp`, `SparseSet.hpp`, `Entity.hpp`), plus anciens que plusieurs correctifs du Carnet de route. Lire d'abord le dépôt LIB actuel avant d'agir.

**Attention, les sujets sont à vérifier avant toutes modifications.**

| # | Sujet | Nature relevée | À décider |
|---|-------|----------------|-----------|
| L3-1 | Découpage de l'entité : la leçon L03 (et l'artefact) construisent 24 bits d'index / 8 bits de génération ; le Carnet de route (bug 54) acte un repack 16/16 (65 536 entités max, 65 536 générations avant bouclage), choix tranché avec Onky | Écart leçon/dépôt (décision déjà prise, jamais reportée dans la leçon) | Vérifier `Scene/Entity.hpp` du dépôt. Si 16/16 est en place : entériner dans la leçon (§4.2, les trois `static_assert`, étapes 1 et 2 de l'artefact ; le risque « 1/256 » devient « 1/65 536 »). Garder 24/8 comme base didactique ou non. |
| L3-2 | **À traiter avant le lot caméra.** `HierarchyComponent::m_parent` non initialisé dans le `Component.hpp` collé ; `HierarchyComponent{ {}, {}, true }` (racine dans `ParseHierarchy`, parent créé dans `linkChildToParent`) laisse `m_parent` à 0, c'est-à-dire l'entité d'index 0 | Dette latente, à vérifier (le Carnet, bug 53, montre `NULL_ENTITY` déjà utilisé par `DestroyHierarchy`) | Confirmer que `m_parent = NULL_ENTITY` est la valeur par défaut du membre et que les deux sites du Serializer passent `NULL_ENTITY`. Aucun symptôme visible tant que `WorldTransformSystem` lit `m_isRoot` et non `m_parent`. |
| L3-3 | **À traiter avant le lot caméra.** `TriggerComponent` : `radius` et `is_colliding` sans valeur par défaut dans le `Component.hpp` collé (agrégat sans constructeur, ordre positionnel imposé à `emplaceComponent`) | À vérifier (le composant a depuis gagné `role` et un `overlapping_entities` en `vector`, Carnet 0.1 et bug 62) | Restaurer `= 1.0f` et `= false` si absents. Un `TriggerComponent` construit sans tout renseigner aurait un rayon aléatoire ; `ParseTrigger` assigne tout, d'où l'absence de symptôme. |
| L3-4 | `ComponentView` après F3 : `m_registry` et `m_componentTypeIDs` plus lus ; `any_storage_null` jamais vrai (`getStorage` non-const crée le pool) ; construire une vue sur un type absent crée un pool vide | Dette mineure (autres points ouverts) | Supprimer les deux membres, ou les garder pour le diagnostic ; documenter l'effet de bord de `ViewGroup`. |
| L3-5 | `Entity.hpp` : `using Entity` déclaré deux fois (`uint32_t`, puis `std::uint32_t`) | Dette mineure (autres points ouverts) | Garder une seule déclaration (`std::uint32_t`), après `<cstdint>`. |
| L3-6 | Filets de l'ECS : `assert` standard dans `Registry` / `SparseSet` ; `CheckInvariants` proposé sous `LV3_DEBUG` | Incohérence avec L01 (E3) et le Carnet 0.3 : `LV3_DEBUG` existe mais gouverne les dumps ; les filets passent par `LV3_ASSERT` / `LV3_ASSERTS_ENABLED` (indépendants de `assert`) | Passer les asserts de l'ECS à `LV3_ASSERT` ; garder `CheckInvariants` sous `LV3_ASSERTS_ENABLED`. Se tranche avec E3. Un filet sous le mauvais commutateur est inactif en `RelWithAsserts`. |
| L3-7 | Casse des chemins dans `CanonicalKey` : `weakly_canonical` ne ramène la casse à celle du disque que pour les composants existants, selon la plateforme | À vérifier d'abord (autres points ouverts) | Test : charger `Assets/Meshes/cube.obj` puis `assets/meshes/cube.obj`, contrôler `GetMeshCount() == 1`. Sinon passer en minuscules la clé de cache. |
| L3-8 | `ComponentTypeManager::GetTypeID` : compteur non atomique ; nom `s_NextNextID` | Dette (à rouvrir si le Registry devient multi-thread) | `std::atomic<uint32_t>` + `fetch_add(relaxed)` ; renommer le compteur. Se tranche avec D24. |
| L3-9 | `ResourceManager` : aucune thread-safety (D5 de l'audit) ; dossier `Ressources` en français (D6) | Dette documentaire + cosmétique | Écrire le contrat mono-thread dans le header ; renommer le dossier `Resources` (chaque oubli d'`#include` sort en erreur de compilation). |
| L3-10 | `OBJLoader::ParseFile` renvoie un `bool` qui fusionne « fichier vide » et « aucune face » ; test mort `if (rawFaces.empty())` dans `OBJLoader::Load` | Dette | `expected<void, EParseError>` pour séparer les causes ; supprimer le test inatteignable. `EMeshLoadError::ParseFailed` reste ambigu tant que ce n'est pas fait. |
| L3-11 | `TriggerSystem` : collision O(N²) sans broad-phase (le bug 62 a retiré les paires zone-contre-zone, pas le parcours quadratique) | Chantier futur déjà noté | Grille uniforme ou quadtree avant la narrow-phase sphère/sphère ; à lier à L11 (BVH partagé). |
| L3-12 | `AnimationSystem` : `m_currentOrbitAngle` / `m_currentRotationAngle` jamais ramenés dans [0, 2π[ ; en float32 la résolution se dégrade quand l'angle grandit | Hors sujet L03, noté | Décider d'un modulo après mise à jour des angles. |
| L3-13 | Taille de `TransformComponent` annoncée « ~152 octets » à l'audit : le calcul donne 164 (3 × `Vec3f` = 36, 2 × `Matrix44f` = 128, hors padding) | Incohérence documentaire (erreur de calcul de l'audit) | Corriger les supports ; ajouter `static_assert(sizeof(TransformComponent) == …)` après mesure sur MSVC x64. |
| L3-14 | Support `Lecon_03_ECS_SparseSet_ResourceManager.md` : affirmation que l'ordre « notifier puis incrémenter » dans `DestroyEntity` est « sacré » et que l'inverse « détruit tout » | Incohérence documentaire, corrigée dans le Markdown et l'artefact ; `SparseSet::Contains` ne lit jamais `m_Generations`, l'inversion ne casse rien (test mutant, étape 10) | Vérifier qu'aucune annexe ni règle numérotée ne reprend l'ancienne affirmation. L'ordre reste une convention utile à un futur observateur qui consulterait `IsAlive` pendant la notification. |
| L3-15 | Sérialisation d'entités versionnées | Sans objet aujourd'hui (le Serializer identifie les nœuds par nom) | Si une sauvegarde est ajoutée : table de remappage au chargement ; ne jamais écrire un handle brut ni un `typeID`. |
| L3-16 | Groupes possédants (EnTT) / archetypes | En attente du profileur | Ne rien faire tant qu'une vue précise n'est pas désignée comme goulot. Se tranche avec D23. |
| L3-17 | `DestroyEntity` du dépôt : précondition Debug `HasOutgoingLinks` et `DestroyHierarchy` qui dénoue le sous-arbre avant destruction (Carnet, bug 53) ; la leçon L03 décrit un `DestroyEntity` sans ces deux garde-fous | Écart leçon/dépôt (évolution, bug corrigé et validé le 17/09) | Ajouter à la leçon un encadré « hiérarchie et destruction » renvoyant au bug 53, ou le traiter dans la leçon de hiérarchie ; adapter l'étape 2 de l'artefact. |

**Décisions de lot proposées, non actées** (numérotées à la suite de D22) :
- **D23** : pas de groupes ni d'archetypes tant que le profileur ne désigne pas une vue précise comme goulot (YAGNI). Se tranche avec L3-16.
- **D24** : `GetTypeID` atomique le jour où le Registry est touché par plus d'un thread. Se tranche avec L3-8.

**Décisions de lot tranchées et appliquées dans l'échange** (artefact L03, section 1) : entité en ticket daté (F1), `ComponentView` à pointeurs capturés et itérateur proxy (F3), composants par handle (F4), clé de cache canonique, map miroir et `std::expected` (F5), sparse paginé (B7), `Emplace` (B6), handles de ressources sans génération (D4). Seul le découpage 24/8 est remis en cause par L3-1.

**Ordre de lecture suggéré (sans engagement)** : L3-1 (vérification sans coût, change les chiffres de la leçon), puis L3-2 et L3-3 (à lever avant le lot caméra, à vérifier d'abord), L3-6 (convention de filets, avec E3), L3-7 (test de 5 minutes), puis le nettoyage L3-4 / L3-5, la documentation L3-13 / L3-14 / L3-17, le reste attendant son chantier (L3-10, L3-11, L3-16).


### Décisions en attente , non actées (artefact Lot L04 — Le Rasterizer, pas à pas, L04)

**Aucune décision à ce stade : tout ce qui suit reste à acter.**
Origine : artefact « Lot L04 — Le Rasterizer, pas à pas », décisions et livraisons de l'échange de « Lot L04 — Le Rasterizer, pas à pas ».
A vérifier si les sujets relevés existent toujours.

#### 9.1 — Écarts entre la leçon et le dépôt

Rien n’est acté ici. Ces points sont à trancher explicitement, et à reporter dans `Plan_Phase_G_LibraryV3.md`.
**Attention, les sujets sont à vérifier avant toutes modifications.**

| Réf | Écart | Raison, apport, statut |
|-----|-------|------------------------|
| L4-1 | La L05 a été faite avant la fin de la L04 | Le rasterizer a été écrit, puis la caméra, puis le rasterizer révisé. Apport : la révision a été faite avec le reverse-Z et le far infini déjà actés, donc sans compromis. Statut : évolution, rien à décider. |
| L4-2 | `ClipVertex` ne porte ni `uv` ni `normal` | Bloqué par la dette `MeshClass::GetFaceView`. Apport : pas de champs morts dans une structure copiée des milliers de fois par frame. Statut : dette — à lever avec le chantier texturing, en ajoutant simultanément dans `Lerp()`. |
| L4-3 | Règle 19 rétrogradée : pas de `ClipOrientation` | Le culling en espace de clip économiserait 3 divisions sur ≈ 50 % des triangles, pour 14 opérations sur 100 %. Match nul mesuré, et il introduirait un second signe d’orientation à calibrer. Statut : chantier différé sous condition de profilage. |
| L4-4 | Culling en espace objet rejeté | Gain réel ≈ 40 % de transformations sur mesh fermé indexé, 0 % sur terrain et UI ; coût : 12 o/triangle, cas particulier pour le scale négatif, winding encodé à deux endroits. Statut : rejeté, motivé. |
| L4-5 | `ECullMode`, `EDepthTest`, `EBlendMode` non exploités par `Renderer` | Les enums de la L01 existent, le `Renderer` ne les lit pas. `EDepthTest` deviendra propriétaire du flag `EDepthWrite` quand l’alpha-test arrivera (l’early-Z devient alors invalide). Statut : dette. |
| L4-6 | Les quads OBJ sont supposés convexes et planaires | L’éventail de triangulation le supposait déjà avant ce lot ; le clipper, lui, ne le suppose plus (décision 2). Statut : dette à documenter, pas à corriger tant que les assets sont maîtrisés. |

#### 9.2 — Bugs relevés, non corrigés à la clôture du lot

Rien n’est acté ici. Ces points sont à trancher explicitement, et à reporter dans `Plan_Phase_G_LibraryV3.md`.
**Attention, les sujets sont à vérifier avant toutes modifications.**

| Réf | Bug | Détail |
|-----|-----|--------|
| L4-7 | `ParseCamera` : `out_activeCamera != NULL_ENTITY;` | Comparaison au lieu d’affectation — l’expression est jetée. Le paramètre de sortie n’est jamais assigné. MSVC devrait émettre C4553 en `/W4`. Hors périmètre L04, mais trente secondes à corriger. |
| L4-8 | `ShadeFragment_Barycentric` contourne `AsFragmentContext` | `static_cast` direct → la sentinelle `magic` n’est pas vérifiée. Règle grepable : `static_cast<FragmentContext*>` ne doit apparaître qu’une seule fois dans tout le projet. |
| L4-9 | `ShadeFragment_Count` utilise un `CountContext` | Second type derrière le `void*` — le mécanisme exact du bug n° 17. Admissible uniquement dans un test qui n’utilise ni `Renderer` ni `FragmentContext` : les deux types ne peuvent alors pas se croiser. À documenter comme exception, pas à laisser implicite. |
| L4-10 | Exclusion FPS / Follow promise par un commentaire | Les deux systèmes écrivent dans le même `m_local` ; le second appelé gagne. Seul `SwitchCameraMode` arbitre, et il faut l’appeler. Remède : un invariant `CheckControllerExclusivity` au chargement, pas un commentaire. |


### Décisions en attente — Consolidation C1 : la chaîne des espaces (relevé du 08/11/2026)

**Aucune décision à ce stade : tout ce qui suit reste à acter.**
Origine : artefact « Lot Consolidation C1 — La chaîne des espaces, pas à pas ».
**Attention, les sujets sont à vérifier sur le dépôt avant toute modification.**

#### C1.1 — Écarts entre la consolidation d'origine et le dépôt

| Réf | Écart | Raison, apport, statut |
|-----|-------|------------------------|
| C1-0 | Collision de nom : « Consolidation C1/C2 » et « chantier C1/C2 » (phase G) | La section « Dette relevée en C1 » (z NaN) appartient au *chantier* C1, pas à la consolidation. Proposition : renommer la série des consolidations (ex. `K1`, `K2`) et ne plus réutiliser une lettre de chantier. Statut : décision à prendre. |
| C1-1 | Sur le chemin Inside, E5 (clip) et E6 (NDC) ne sont plus matérialisés | Chantier 2b : `ProjectPositions(8)` fait E1 → E7 en un passage ; `RasterVertex` est le type de E7. Apport : 504 → 117 divisions par roche, Render −33 %. Statut : évolution ; `Consolidation_C1_Espaces.md` (§3, §5, §8) décrit encore la frontière « par coin » — à amender. |
| C1-2 | Deux nouveaux espaces : E7·g (grille 1/256 px) et E7·fx (entiers X = x·256) | `LV3_RASTER_FIXED`, lot D-a1 (Giesen). Apport : couverture exacte, un sommet = un accrochage. Statut : évolution ; la carte d'origine (« E7 : 2,5D continu ») est fausse sous FIXED = 1 — à ajouter à la liste des espaces. |
| C1-3 | Frontière E7 → E8 : `floor(x)` dans la carte, centres `x + ½` dans le dépôt (`TightPixelBox`) | La carte d'origine décrivait « dans quel pixel tombe un point », pas « quels centres un triangle couvre ». Statut : correction documentaire. |
| C1-4 | Règle C1.3 (« le winding se teste en E7 ») et écart L4-4 (« culling en espace objet rejeté ») contredits par B5 | B5 a introduit un pré-filtre E1 **conservatif** (s = L·E < −τ), rentable seulement en AVX2 ; E7 garde le dernier mot. Statut : C1.3 amendée dans l'artefact ; **réécrire L4-4** : « rejeté en remplacement, adopté en pré-filtre SIMD (B5) ». |
| C1-5 | Clause « main » : `det(model) < 0` inverse la main de E1 | Dette « Miroir » déjà relevée par B5, rattachée ici à la carte des espaces. Prédiction : propagation à toute la descendance par E2 (L06). Statut : dette, à traiter avant ou pendant la L06, sur les deux tests (écran B4 et objet B5) à la fois. |

#### C1.2 — Dettes de la consolidation d'origine : état au 08/11

| Réf | Dette | Constat au 08/11 | Statut proposé |
|-----|-------|------------------|----------------|
| C1-6 | `ComputeMeshAABB()` appelée nulle part automatiquement | `RegisterMesh` est devenue la porte unique (SoA, plans B5, plafond de sommets) mais n'appelle pas `ComputeMeshAABB()`. Appel dans `OBJLoader` : non vérifié. | dette ; l'endroit naturel existe désormais |
| C1-7 | `GetFaceView` suppose les sommets contigus | Inchangé (`fv.verts = &vertexPositions[indices[base]]`). Bloque EX (tangent), EY (UV) et L4-2. | dette bloquante pour le texturing |
| C1-8 | Leçon 02 : « main gauche, +Z entre dans l'écran » | Traité par les lots L02 / L02-C ? À vérifier dans « socle L02 ». | à vérifier |
| C1-9 | `ECullMode`, `EDepthTest`, `EBlendMode` non exploités | Déjà porté en L4-5. | doublon : renvoyer à L4-5 |
| C1-10 | Chantier B (L04 P2) : interpolation perspective-correcte | Toujours ouvert, rattaché à la L09. Prérequis : C1-7. | reste ouvert |

#### C1.3 — Points nouveaux relevés par la relecture

| Réf | Sujet | Détail |
|-----|-------|--------|
| C1-11 | Le chemin Intersect n'a pas de version AVX2 | `TransformPositions` scalaire, projection par coin. Négligeable en scène astronomique (≈ 2 % des faces), dominant en scène FPS (lot V3 : un niveau d'un seul mesh est toujours Intersect). À relier au plan FPS / culling. |
| C1-12 | Lot D-a1d (accrochage par nombre magique) | Livré le 06/11, « à appliquer » ; le dépôt montre encore `_mm_round_ss` dans `SnapToSubpixel` et `_mm256_round_ps` dans `ProjectPositions8`. À vérifier. **Constaté le 10/11 (lot D-a2)** : `SnapToSubpixel` utilise le nombre magique (domaine \|v\| < 2^14, `_mm_round_ss` gardé hors domaine) et `Test_Da1d_Snap` existe dans l'EXE. |
| C1-13 | `inverseRigid()` sans filet | Aucune assertion d'orthonormalité avant l'appel ; devient probable en L06 (caméra enfant d'une entité à échelle : vue fausse d'un facteur 4 sans erreur). Proposition : `LV3_ASSERT(IsRigid(m))` en Debug. |
| C1-14 | Taille apparente du LOD : une mesure qui saute de E1 à E7 | Calculée depuis l'AABB locale et la `mvp`, sans passer par les sommets. Rien à décider : à documenter dans la carte comme « mesure E1 → E7 par instance ». |

#### Règles issues de la relecture (proposées, non actées)

- **C1.3 amendée** : chaque opération appartient à l'espace où elle est la moins chère et la plus universelle ; un test peut avoir un pré-filtre conservatif dans un espace plus en amont, à condition que l'espace d'origine garde le dernier mot.
- **C1.4 confirmée** : `invW` est le seul passager de la frontière clip → NDC ; le type `RasterVertex` la grave dans le code.
- **Règles grepables** : `/ c.w` n'apparaît que dans `ProjectClip` (+ `_mm256_div_ps(one, cw)`) ; le flip Y que dans `Viewport::ToRaster` (+ sa copie AVX2 gardée par la TNR B3) ; `SnapToSubpixel` que dans `ProjectClip` ; 0 `vfmadd` dans l'asm de `VertexStage` et `RenderSystem`.

### Décisions en attente — lot PH4-CL : le chantier Clear (relevé du 20/11/2026)

**Aucune décision à ce stade : tout ce qui suit reste à acter.**
Origine : artefact « Lot PH4-CL — Le chantier Clear, pas à pas » (reconstruction du 20/11, remplace la page « Anatomie du Clear LV3 », même adresse), section 8. Échange du 05/10 au 07/10/2026, annexe A15. Les décisions prises pendant l'échange sont dans `Résultats/Résultats_Lot_PH4CL_Clear.md`.
**Attention, les sujets sont à vérifier sur le dépôt avant toute modification** (relu le 20/11 sur LIB `8acd153` / EXE `4286a9c`).

| # | Sujet | Nature relevée | À décider |
|---|-------|----------------|-----------|
| PH4CL-1 | `ClearKernel::StreamFill` ne lit pas `LV3_SIMD` ; `config.h` §11 le dit en commentaire (« le Clear, stores non temporels SSE2, primitive mémoire, reste inchangé ») | Écart à la décision du 25/10 (« un seul flag, scalaire partout »), écrit en commentaire mais jamais acté | Acter l'exception (un store non temporel est un type d'écriture, pas une vectorisation de calcul), ou donner un pendant scalaire (`std::fill`) sous `LV3_SIMD = 0`. |
| PH4CL-2 | Noyau d'effacement 256 bits (512 sur l'i9) maintenant que `/arch:AVX2` est global (B1) | Évolution (piste déjà rangée ci-dessous) | Prédiction : égalité à fréquence nominale (M18 refait : V3 ÷ V2 = 1,01 à 1,09). Ne lancer qu'avec B6 (AVX-512, dispatch CPUID, alignement 64) ou si une scène rend le cœur limitant. |
| PH4CL-3 | `config.h` §9 : bloc `LV3_CLEAR_VARIANT` resté commenté, avec `#define LV3_CLEAR_VARIANT 3` | Dette (code mort ; décommenté, il réactiverait une variante rejetée) | Supprimer le bloc. |
| PH4CL-4 | `main.cpp` : ancien bloc `Clear` commenté au-dessus du nouveau | Dette mineure | Supprimer. |
| PH4CL-5 | `Gfx.cpp` : `extern "C" CleanScreenASM / CleanScreenV3` ; `GFX/gfxASM.ASM` compilé ; `Test/TestCleanBuffer.cpp` compilé, déclaré dans `RunAllTests.cpp`, jamais appelé | Dette (code mort ; banc isolé hors contexte : `memset(0)` dans un tampon à soi) | Supprimer, ou réécrire `TestCleanBuffer` en banc de `StreamFill` (couleur non nulle, mémoire froide) appelé sous un commutateur de banc. |
| PH4CL-6 | `Clean_Render` : `MakeColor(0, 0, 255)` (bleu vif) au lieu de `MakeColor(0, 0, 24)` (bleu nuit de la Référence J) | À vérifier (intention non documentée) | Confirmer le choix ; sans effet sur le coût depuis les stores non temporels. |
| PH4CL-7 | Aucune TNR de `StreamFill` | Dette de test | TNR : largeurs 1 à 33 et 1 536, adresses décalées de 4, 8, 12 o, `T = float` avec −0,0 ; comparer tout le buffer et les mots voisins (aucun débordement). |
| PH4CL-8 | L'annexe A15 n'existe qu'en PDF (instructions du projet : version Markdown aussi dans le projet) | Dette documentaire | Produire `claude/Annexe_A15_Chantier_Clear.md` à la demande. |
| PH4CL-9 | Les ≈ 16 tampons de remplissage et ≈ 110 ns de latence (loi de Little) ne sont pas mesurés | Évolution de méthode | Mesurer latence et débit par cœur (Intel MLC, ou VTune : leçon de découverte prévue). |
| PH4CL-10 | « Écrire moins » non étudié : effacement paresseux du depth par tuiles 8 × 8 (grille de D-a2), pas d'effacement couleur si un fond couvre tout l'écran | Évolution | À mesurer après `Present` ; lié à la piste Hi-Z par tuile. |
| PH4CL-11 | `Present` est le premier poste (≈ 35 % V1, ≈ 58 % L) et relit la couleur effacée | Renvoi (piste ci-dessous) | Pilote D3D11, mode de mise à jour, double tampon. |
| PH4CL-12 | H4′ (D-a2b) : un Z-buffer à pas décalé changerait la forme de `DepthBuffer::Clear` | Renvoi, conditionnel au diagnostic R3 | Effacer le remplissage dans le même bloc (le plus simple) ou ligne par ligne (900 appels) ; mesurer `ClearDepth`. |
| PH4CL-13 | Chantier mesuré seulement sur l'i7-1265U | Évolution | Rejouer V0 / V2 / V3 sur l'i9-10900X (mémoire 4 canaux, latence plus longue). |
| PH4CL-14 | Non faits : binaire K en RelWithAsserts ; V3 à 768 × 450 | Mesure en attente | À faire si le noyau est retouché (PH4CL-1, 2, 7). |
| PH4CL-15 | Le pilote SDL n'est pas épinglé (`SDL_HINT_RENDER_DRIVER`) ; il décide de la mémoire couleur et du coût de `Present` | À décider | Fixer `direct3d` par hint pour la reproductibilité des bancs, ou laisser libre et compter sur `sdl=` dans l'en-tête. |
| PH4CL-16 | Les CSV du lot M15 (1536 × 900, décision V2) et du premier M18 ont été écrasés par M16 et M18 refait (mêmes noms de fichiers) ; `Sauvegarde des mesures/clear/` garde M14, M16 (`clear2b_*`), M18 refait (`clear2c_*`) et K | Dette de traçabilité (même cause que « nom du summary écrasé », lot D-a2) | Accepter la perte (résumés dans A15 et l'artefact) ; suffixer automatiquement les noms de CSV (résolution, variante, horodatage). |

**Règles proposées, non actées** :
- **PH4CL.a** : tout compteur ajouté pour exclure une hypothèse doit montrer au moins une fois le phénomène qu'il exclut (contrôle positif) avant que son zéro soit cru.
- **PH4CL.b** : quand un mécanisme matériel ne se lit pas dans le code (RFO d'une instruction de chaîne, prédicteur, ordre mémoire), on encadre la référence par deux variantes de comportement connu, mesurées dans le même lot.
- **PH4CL.c** : une variante de mesure rejetée est supprimée du code livré, jamais commentée ; elle vit dans l'artefact et l'annexe du lot.
- **PH4CL.d** : avant d'élargir des registres, on compare le temps mesuré au temps d'émission des instructions ; un rapport ≫ 1 désigne la mémoire, et la vectorisation est écartée.

**Ordre de lecture suggéré (sans engagement)** : PH4CL-3, 4, 5 (nettoyage sans risque), PH4CL-1 (décision de règle), PH4CL-7 (TNR avant toute retouche du noyau), PH4CL-16 et 15 (traçabilité des bancs), le reste attendant son chantier (Present, B6, R3).

---

## Pistes rangées pour plus tard

- **`Present`** (≈ 35 % de la frame V1, ≈ 58 % sur L) : `UpdateTexture` D3D9 de 5,5 Mo ; pilote D3D11, mode de mise à jour, double tampon.
- **Noyau d'effacement en 256 bits** sous `/arch:AVX2` : à remesurer si utile (voir PH4CL-2).
- **QEM** : à déclencher à l'arrivée d'un mesh sans fonction génératrice.
- **Comparaison i7-1265U / i9-10900X** (attention : gathers ralentis par le microcode GDS sur l'i9).
- **Chantier shading / texture** : UV perspective-correctes (`invW`), Phong par pixel, mipmaps obligatoires.
- **D-a1c, point ouvert P1** : alléger le coût fixe d'entrée de `RasterizeBlocks8Fx` (3 `vpmulld`, multiplications 64 bits de `PrepareLanesFx`) si l'asm confirme qu'il domine sur les petits triangles ; profiterait à tous les triangles AVX2.
- **D-a1c, point ouvert P2** : W* = 6 au lieu de 8 (la courbe c3/c5/c7 place le point mort vers 8 blocs ; les triangles de 7 à 8 lignes routés ne gagnent rien). À mesurer sur c5, c7, mix si un lot de réglage est ouvert.
- **D-a1c, remarque asm R1** : `RasterizeBlocks8Fx` reçoit `EdgeSetupFx` (104 octets) **par valeur** : copie de 4 `vmovups` + `vzeroupper` avant chaque appel (antérieur à D-a1c). Passage par `const&` à évaluer avec P1 (concerne aussi le repli de `RasterizeTiles8Fx`, D-a2).
- **D-a2, points ouverts** : P1, réglage de T* (16 ou 24 ; point mort mesuré au banc entre 16 et 24 px) ; P2, niveau 64 × 64 seulement si une scène à triangles géants le justifie ; P3, voies de tuiles 64 bits (aujourd'hui repli sur les blocs Giesen quand `TilesFitInt32` échoue) ; P4, grille alignée sur l'écran à reprendre avec D2 (aujourd'hui partie du coin de la boîte : contre-exemple 2 du lot). ; **P5 (confirmé le 14/11 sur les listings A et B)** : la lambda `Edge` de `SetupTriangleFx` n'est jamais inlinée (`call <lambda_3>` ×3 par triangle, 6 push par appel), défaut antérieur à D-a2 → la réécrire en fonction `LV3_FORCEINLINE`, lot séparé avec sa propre mesure (tous les triangles Giesen).
- **Évolutions et dettes relevées par le lot D-a2 (14/11), à décider plus tard** :
  - **[Évolution] D-a2b / D-a2c** : la boucle des tuiles coûte 2,6 à 6,1 ns par tuile vivante sur l'i7-1265U (c24 +4 %, c48 +10 à +14 %). H1 (coût par triangle) réfutée ; H3 (branche pleine/partielle) ≈ un tiers ; H2 (mise en route par tuile) et H4 (ordre mémoire 8 × 8 contre lignes) ouvertes. Voir « Prochaines étapes » 2b. **Mis à jour le 15/11** : D-a2b livré ; le décompte de D-a2b borne H3 à ≈ 1/5 de la perte de c48 ; H4 précisée en H4′ (ci-dessous).
  - **[Dette de méthode] Effet de placement du code non maîtrisé** : RenderVerts (code inchangé) 2,5 à 5,2 % plus rapide dans le binaire B, de façon systématique ; les témoins V1 (−6 %) et V4 c12 (−2,3 %) en sortent de ±2 %. Pistes : comparer les `/MAP` de A et B (adresses modulo 64) ; isoler le noyau de rastérisation dans son propre .cpp pour que l'étage sommets ne bouge plus quand le noyau change.
  - **[Dette d'instrumentation] Pas de thermomètre « calcul » sur V5 et V4 c12** : RenderVerts y vaut 1 à 7 µs (résolution 100 ns), inutilisable ; seuls ClearDepth et Clear (mémoire) restent, aveugles à la fréquence. Piste : une zone de calibration à travail constant (boucle de calcul fixe, quelques µs) mesurée à chaque frame.
  - **[Évolution d'outillage] Nom du summary écrasé à chaque run** : le fichier porte le nom de la scène ; un run non renommé écrase le précédent (et un fichier renvoyé deux fois passe inaperçu : c48 au R2). Piste : suffixer automatiquement par binaire (`tiles=`, `ablation=`) et horodatage.
  - **[Méthode] Banc VM** : prédit les comptages exactement, surestime les gains de temps d'un facteur ≈ 2 et ignore prédicteur de branchement, ordre mémoire et alignement de la machine cible. À rappeler à chaque lot (règle ajoutée au 14/11).
  - **[Mesure en attente] R0 de D-a2 non fait** : la règle de déclenchement « arêtes ≥ 40 % du Render » n'a pas été remesurée depuis le 03/11 (24 %). À faire avant D2.
- **Évolutions et dettes relevées par le lot D-a2b (15/11), à décider plus tard** (rien n'est acté) :
  - **[Hypothèse / évolution] H4′ — pas de ligne de 6 144 o** : la fenêtre de 1536 px donne au Z-buffer et au framebuffer un pas de 6 144 o ; les lignes r et r + 2 d'une tuile ont les mêmes 12 bits d'adresse faibles (alias 4 Ko, conflits d'ensembles L1). Banc VM, même binaire : c24 B/A +4,9 % au pas 1536 (reproduit la cible, +4,3 %), +18 à +22 % au pas 1024 (4 096 o), −4 à −6 % aux pas 1552 et 784. Hypothèse sérieuse, non prouvée sur l'i7 : diagnostic R3 (Prochaines étapes 2c). Concerne aussi toute résolution dont la largeur × 4 est un multiple de 2 048 o (1024, 1536, 2048, 2560, 3072 px).
  - **[Évolution, si R3 confirme H4′] Z-buffer à pas décalé** : `DepthBuffer` gagne un `m_Pitch` (largeur + 16 floats = 64 o) distinct de `m_Width` ; `Row(y)` et `ShaderSink8::dStride` l'utilisent ; `Clear` efface aussi le remplissage (ou seulement les lignes utiles). Le framebuffer SDL a un pitch imposé par le pilote : seules les lectures du Z déclenchent l'alias, le Z suffit probablement. Sonde existante : compteur `FbDbAlias4K` (C0b). Redéfinirait D-a2c. (Effet sur l'effacement : voir PH4CL-12.)
  - **[Évolution, si R3 réfute H4′] D-a2c d'origine** : visiter un groupe de 8 tuiles ligne par ligne (8 tuiles × 1 ligne) au lieu de tuile par tuile ; réécriture des deux corps.
  - **[À vérifier à l'asm] Surcoût de registres de B′** : avec g++, `RasterizeTiles8Fx<…, true>` exécute 3,5 à 4,2 % d'instructions de plus que `<…, false>` (sauvegardes ymm à l'entrée des boucles, 16 registres saturés). À vérifier sur le listing MSVC ; si présent, piste : sortir `boxLane`, `minus1`, `allOn` de la liste des paramètres vivants. **Mis à jour le 20/11 (soir), après le listing de B** : les +4 copies registre à registre par ligne partielle sont présentes **dans B comme dans B′** (boucles de ligne identiques) : elles viennent de l'extraction des gestes, pas des deux boucles ; l'hypothèse « registres » est réfutée pour fan. Coût fixe de B′ mesuré ≈ 1,8 ns par tuile partielle (ou ≈ 4,4 ns par groupe), mécanisme ouvert (placement du code probable ; R4). Le gain de la branche est borné à ≈ 1 % sur c48 (plafond absolu 1,8 %), indépendant du binning (les changements de nature se paient par arête, le travail par aire). À remesurer après D2 / D-a2c (commutateur conservé). Rien n'est acté.
  - **[Dette, relevée le 20/11 par le listing de B] Extraction des gestes de tuile** : `TileFull8` / `TilePartial8` / `TileRows8` ont alourdi la variante D-a2 elle-même (ligne partielle 46 → 50 instructions, ligne pleine 31 → 32, 4 et 2 copies registre à registre). R2 ne montre pas de régression de B contre les chiffres de D-a2 (séances différentes). Pistes : `TileRows8` par référence, ou gestes réécrits pour que MSVC garde les lignes en place ; à mesurer A/B contre le binaire D-a2 d'origine. À décider.
  - **[Évolution ACCEPTÉE le 21/11] Lot « boucles de ligne » (D-a2d)** : voir « Prochaines étapes » 2e (après R3). Couvre la dette ci-dessus (copies de l'extraction des gestes) et le test de `flipped` à chaque ligne, relevé le 20/11 sur le listing de B.
  - **[Dette mineure, antérieure] Avertissement `kSmall` inutilisé** sous `LV3_ABLATION_PIXELS = 1` (`Fragment.h`, `RasterizeTriangleFx`) : `[[maybe_unused]]` à ajouter, comme pour `kTile`.
  - **[Méthode] Banc sans rooms** : le banc hors moteur de D-a2b ne reconstruit que V5 (projection simple, pas de clipping) ; `Tiles8Switch` de rooms n'est donc pas prédit au compte près (borne 21 862, estimation 6 000-12 000). Une reconstruction de rooms hors moteur (étage sommets et clipping compris) serait utile aux prochains lots du noyau.
  - **[Observation inexpliquée] VM** : au pas 1552, A elle-même est ≈ 30 % plus lente qu'au pas 1536 sur la VM (même binaire). Sans conséquence pour les rapports B/A et B′/B mesurés à pas égal ; à garder en tête si le Z-buffer à pas décalé est mesuré (vérifier que A ne ralentit pas sur l'i7). **R3 le mesure directement** (effet de largeur sur A, runs 1-8 contre 3-6 ; attendu +0,5 à +2 % sur l'i7).
- **Évolutions et dettes relevées par le lot R3 (20/11), à décider plus tard** (rien n'est acté ; relevé sur LIB `9678f47` / EXE `26ae1f8`) :
  - **[Bug visuel] R3-a — séparateurs de vues** (`main.cpp`, après le Render) : `cfg.mapViewports["title"]` n'existe pas (`config.json` définit `title1` et `title2`) ; `operator[]` insère une entrée vide (largeur 0) et la ligne horizontale n'est jamais tracée. La verticale est tracée à x = `viewports["left"].largeur` = 1136 quelle que soit la disposition (en Quad, la frontière est à `FrameW / 2`). À décider : tracer les séparateurs à partir des vues de `BuildLayout`, ou les retirer.
  - **[Dette] R3-b — recherches dans la boucle des séparateurs** : `cfg.mapViewports["left"]` est évalué dans la condition de la boucle (≈ 801 recherches de `unordered_map` par frame, avec construction d'une `std::string` à chaque fois), dans la frame mesurée, hors des zones du Profiler. Coût non mesuré (estimé à quelques dizaines de µs). À décider : lire les valeurs une fois avant la boucle (se tranche avec R3-a).
  - **[Dette, à vérifier] R3-c — alignement du Z-buffer** : `DepthBuffer` range ses données dans un `std::vector<float>` (base alignée sur 16 o seulement), alors que les tampons sommets utilisent `SimdVector` (64 o). À décider avec le Z-buffer à pas décalé (même classe) et P4 de D-a2 (grille de tuiles alignée sur l'écran).
  - **[Évolution d'outillage] R3-d — pitch de la couleur non journalisé** : le pitch rendu par `SDL_LockTexture` n'est écrit nulle part ; `ProbeAlias4K` suppose « pitch = W × 4 ». Piste : `pitch=` dans la 1ʳᵉ ligne du summary.
  - **[Évolution d'outillage] R3-e — résolution en ligne de commande** : aucun argument ne fixe la résolution ; changer de largeur exige une copie de la racine de contenu. Piste : `--res=WxH` (prime sur `config.json`, écrit dans l'en-tête), sur le modèle de `--scene=`.
  - **[Dette documentaire] R3-f — section `viewports` de `config.json`** : ne décrit plus les vues réelles (calculées par `BuildLayout` à partir de `FrameW`, `FrameH` et `maxViewport`). À retirer ou à brancher (se tranche avec R3-a).


## Décisions en attente : lot A6 (JsonReader & EngineConfig), rien n’est acté (relevé du 08/11/2026)

Rien n’est acté dans cette liste. Elle regroupe les écarts entre l’annexe, le dépôt et ce que cet échange a fait apparaître. Les lignes A6-5, A6-10 et A6-12 touchent des sujets d’autres lots (Serializer, événements) : elles sont notées ici sans être traitées.
Les sujets sont à vérifier sur le dépôt avant toute modification.

- **A6-1** Défaut de couleur Vec3f(155) dans ParseLight : Confirmer l’échelle de LightComponent::m_color sur le dépôt (0–255 ou 0–1) ; proposition : Vec3f::One(). [à vérifier]
- **A6-2** Défaut 0.5f de la vitesse dans ParsePlayerControl : Recouper avec l’initialiseur de PlayerControlComponent::m_speed. [à vérifier]
- **A6-3** ParseCamera sans std::move(c) : Correctif trivial, cohérence avec R24. [à décider]
- **A6-4** ParseMesh, ParseHealth encore en lecture manuelle : Migrer vers JsonReader (recette de l’étape 7). [à décider]
- **A6-5** Clé absente : le lecteur collé dans cet échange n’avertit pas : La règle du 07/10 demande que le journal dise quand un défaut a été pris, et la dette « clés optionnelles » du Serializer propose de tester Has() avant les clés réellement optionnelles. Vérifier sur le dépôt si Read avertit ou non, puis trancher avec cette dette. Remet en question une décision de ce lot. [à vérifier puis décider]
- **A6-6** Namespace LibV3 (Serializer) contre LV3 (le reste) : Incohérence connue, non traitée dans ce lot. [à décider]
- **A6-7** LoadFromJson peut laisser un état partiel : Une exception en cours de route laisse les sections déjà lues écrasées, alors que le commentaire de l’en-tête promet « jamais un état partiel ». Correctif proposé : travailler sur une copie (diff « Proposé 3 »). [proposé]
- **A6-8** Child silencieux sur une section du mauvais type : "renderer": 3 donne une section vide sans message. Correctif proposé : avertir (diff « Proposé 2 »). [proposé]
- **A6-9** ForEachChild et les clés « _ » : WarnUnread saute les clés commençant par « _ », pas ForEachChild : un _comment dans assets produit un avertissement. Correctif proposé : même convention (diff « Proposé 1 »). [proposé]
- **A6-10** Doublon de clé dans le texte JSON : Non détectable par le lecteur. Options : ne rien faire ; relecture/outil de validation externe ; détecteur SAX avant l’analyse. À décider si le problème se répète. [à décider]
- **A6-11** Annexe A6 (markdown du projet) en retard : Elle ne contient pas ForEachChild, ForEachElement, Child élargi, la garde de WarnUnread, les doublons ni les points A6-7 à A6-9. Mise à jour à ta demande. [à ta demande]
- **A6-12** Événements du Trigger (chaînes sur un composant chaud) : Reporté par Onky à une annexe dédiée ; attend le contenu de Core/EventNames.h et la déclaration de TriggerComponent. [reporté]
- **A6-13** engine.json dans le projet EXE : Application du déplacement et vérification du répertoire de travail : à confirmer. [à confirmer]
- **A6-14** Macro NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE pour les assets : Choisir entre la macro actuelle et ForEachChild pour AssetStruct. [à décider]
