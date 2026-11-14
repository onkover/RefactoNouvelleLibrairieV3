# Décisions — Lot D-a2 (hiérarchie d'Abrash posée sur Giesen)

*Échange des 10 au 14 novembre 2026. Résultats détaillés : `Résultats/Lot_Da2_Abrash.md`. Artefact : « Lot D-a2 pas à pas » (export `Lot_Da2_pas_a_pas.html`). Roadmap : `Plan_Phase_G_LibraryV3.md`.*

## Statut final

**D-a2 ACCEPTÉ le 14/11/2026, défaut `LV3_RASTER_TILES 16`, avec deux réserves écrites.** À commiter (LIB + EXE + scène V5).

## Décisions de conception (10/11, avant mesure)

| # | Décision | Raison | Écarté |
|---|---|---|---|
| D0 | Faire le lot, conditionnel (commutateur), bien que la règle « arêtes ≥ 40 % du Render » ne soit pas remplie (24 % au 03/11) | Demande d'Onky ; socle de D2 (binning) et d'un Hi-Z par tuile ; gain attendu sur les grands triangles | Attendre que la règle redevienne vraie |
| D1 | Seulement le chemin Giesen AVX2 en voies 32 bits, boîte ≥ T* dans les deux axes | Les petits (D-a1c) n'ont rien à découper ; voies 64 bits trop rares | Tuiles pour tout triangle AVX2 (classe 8-15 : +5 à +25 %) |
| D2 | Tuiles 8 × 8, grille partant du coin (x0, y0) de la boîte | Une tuile partielle = exactement 8 blocs Fx32 | Grille alignée écran (+9 % classe 16-24) ; à revoir avec le binning |
| D3 | Deux niveaux : groupes de 8 tuiles par registre, puis blocs | Un groupe de 64 × 8 jugé en ≈ 20 µops | Niveau 64 × 64 au-dessus (gain < 1 %) |
| D4 | Tuile pleine : `Shade8Full` (lecture Z simple, écritures masquées par le seul test Z) | Ni masque de couverture ni `vmaskmovps` en lecture | `blendv` + écriture pleine |
| D5 | Garde `TilesFitInt32` sur [x0, x1 + 7] × [y0, y1 + 7], sinon repli Giesen | E' affine, extrêmes à deux coins | Voies de tuiles en int64 |
| D6 | Fonction séparée `RasterizeTiles8Fx`, hors ligne | Prologue et placement de Fx32 inchangés (vérifié à l'asm : 0 différence) | Hiérarchie dans `RasterizeBlocks8Fx` (V2 +4 %) |
| D7 | `NextRow` : pointeurs avancés par addition | BeginRow par ligne et par tuile : +3 % sur c24 | BeginRow à chaque ligne |
| D8 | Commutateur `LV3_RASTER_TILES` = T* (défaut 16), en-tête CSV `tiles=` | Point mort au banc entre 16 et 24 px | Un simple booléen |
| D9 | Pendant scalaire `ForEachCoveredPixelFxTiled` | Principe jouable en scalaire, chemin indépendant pour la TNR | Hiérarchie AVX2 seule |
| D10 | Compteurs `TrisTiled`, `Tiles8Rejected/Full/Partial`, `BlocksFull8` ; LaneBlocks8 = blocs testés | Prédire et vérifier au compte près (R1) | — |

## Décisions prises pendant l'application et la mesure (14/11)

| # | Décision | Raison |
|---|---|---|
| D11 | **Accepté, défaut 16, avec réserves** : (1) les triangles tuilés de 24 à 48 px régressent sur l'i7-1265U (c24 +4,3 %, c48 +10 à +14 %) ; (2) une part du gain de rooms peut venir du placement du code | Tous les critères écrits avant mesure tenus : c96 −14 % (seuil −8 %), fan −55 % (seuil −25 %), rooms −2,3 % brut / +1,5 % ÷ RenderVerts (seuil +2 %), V2 conforme ; V1 et V4 c12 hors ±2 % du bon côté, expliqués par le placement |
| D12 | c24 rendu obligatoire au R2 | Indice du R1 (temps du run de comptage : c24 +5 %, c48 +14 %) |
| D13 | c96 et c48 ne sont pas refaits avant le commit ; ils le seront dans la campagne de D-a2b (A / B / B′ dans la même séance) | Verdict c96 robuste (pire paire −9,4 %) ; c48 n'est pas un critère et son signe est confirmé deux fois |
| D14 | A_stats V5 conservé comme référence (reçu en plus) | 23 compteurs communs identiques à B_stats ; invariant de rejet vérifié sur la mesure moteur |
| D15 | Règle de séance : run écarté et refait si RenderVerts > 1,15 × la médiane de la scène (si RenderVerts ≥ 10 µs, sinon ClearDepth) ; pause fixe de 60 s entre runs ; secteur, performances optimales, mode efficacité désactivé | Runs « bridés » sur V2 (tout le frame ×2) et rooms A4 (×1,27) : portable à 15 W |
| D16 | Quand les deux thermomètres se contredisent et que A1 ≈ A2 (< 1 %), on lit le brut | Pas de dérive dans la séquence ; diviser n'ajoute que le bruit du thermomètre |
| D17 | Un témoin hors ±2 % du bon côté se teste contre l'effet de placement : RenderVerts (code non touché) bouge-t-il aussi ? | RenderVerts B/A : rooms −3,8 %, V1 −5,2 %, c24 −3,7 %, V2 −2,5 % : systématique, lié au binaire |
| D18 | Hypothèses de la perte de c48 : H1 (coût par triangle) **réfutée** par c24 ; H3 (branche pleine/partielle) n'explique qu'environ un tiers (laboratoire 18c) ; H2 (mise en route par tuile) et H4 (ordre mémoire) restent ouvertes | 24 ns/triangle sur c24 contre 159 sur c48 ; 2,6 ns/tuile contre 6,1 |
| D19 | **Suite : lot D-a2b** (boucle des tuiles sans branche de données : masques `full` puis `live & ~full`), mesuré en A / B / B′ sur c24, c48, c96, rooms, V1 ; si la perte demeure, D-a2c (boucles interverties, ordre mémoire ligne par ligne) ou P1 (réglage de T*) | Tester H3 directement, puis H4 |
| D20 | Piste P5 confirmée et séparée : la lambda `Edge` de `SetupTriangleFx` n'est jamais inlinée (aussi dans le binaire A) | Défaut antérieur à D-a2, lot propre avec sa mesure |
| D21 | R0 (ablation N E S M) non fait : la règle des 40 % reste à remesurer, avant D2 | Facultatif pour ce lot (D0) |

## Leçons de méthode retenues

- Le banc sur VM prédit les **comptages** au compte près (R1 exact sur 14 scènes) mais pas les **temps** : il surestime les gains d'un facteur ≈ 2 (c96 −25 → −13 %, c192 −30 → −7 %) et se trompe de signe quand l'écart tient au prédicteur de branchement, à l'ordre mémoire ou à l'alignement (c48 −10 → +10 %). Un banc choisit une conception ; le lot A/B sur la machine cible décide.
- Mesurer au moins une scène par régime de tuiles : beaucoup de rejet (fan), beaucoup de pleines (c96, c192), beaucoup de partielles (c24, c48), une scène réelle (rooms), un témoin (V2). c96 seul aurait caché la régression de c48.
- L'accrochage au 1/256 px rend les comptages insensibles au bruit de projection float / double (V5 exact au lieu de ±0,1 %).
