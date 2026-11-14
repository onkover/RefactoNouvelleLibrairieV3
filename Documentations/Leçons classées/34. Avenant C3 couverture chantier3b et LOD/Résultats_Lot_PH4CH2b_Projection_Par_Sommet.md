# Résultats — Lot PH4-CH2b : la projection une fois par sommet (chantier 2b), avec le chantier 3a

*Échange du 28 au 30/09/2026 (chantier 2b du plan de la consolidation C2, puis chantier 3a), consigné le 13/11/2026 avec l'artefact « Lot PH4-CH2b — La projection par sommet, pas à pas » (même adresse que l'ancienne page « Taille apparente LV3 », qu'il remplace ; la page « Niveaux de détail LV3 » est conservée pour le futur lot du chantier LOD). Ce fichier liste ce qui a été **décidé et validé** dans l'échange. Les points ouverts relevés à la reconstruction sont dans `Plan_Phase_G_LibraryV3.md`, section « Décisions en attente — lot PH4-CH2b » (PH4CH2b-1 à PH4CH2b-11) : rien de ce qui s'y trouve n'est acté.*

Documents produits par l'échange : `claude/Avenant_C4_Chantier2b_Projection_Par_Sommet.md` (§ 14 = chantier 3a), `claude/Avenant_A12a_Pipeline_Rendu_Apres_2b.md`, `claude/Annexe_A13_LOD_Niveaux_de_Detail.md`, pages « Taille apparente LV3 » (remplacée par ce lot) et « Niveaux de détail LV3 ».

Commits de l'échange : LIB `741952a` (instrument LOD_STATS), `f415b57` (2b), `3710614` (2b + 3a) ; EXE `f79ba96`, `8df2098`, `d2afc84`.

## Périmètre

| Objet | Statut à la fin de l'échange |
|---|---|
| Instrument de taille apparente (`LV3_LOD_STATS`, compteurs `InstPx*` / `FacesPx*`, `ScreenSizeParams`, `ProjectedRadiusPx`, `BoundingRadiusWorld`) | Implémenté par Onky (`741952a`) |
| Règle G1 (2b ou LOD) écrite avant la mesure | Écrite, corrigée avant mesure, **non appliquée** (décision d'Onky) |
| Ordre des chantiers | 2b, puis LOD |
| Leçon LOD (annexe A13) | Livrée (markdown + page « Niveaux de détail LV3 ») |
| `RasterVertex`, `ProjectClip`, `RasterSpaceBuffer`, `ProjectPositions`, `EmitRasterTriangle`, bifurcation Inside / Intersect, `FacesInside` | Implémenté, exact, commité (`f415b57`) |
| Mesure du gain (série B, A B B A, cœur P) | −33,5 % sur `Render` |
| Chantier 3a (boucle pixel sur la boîte serrée) | Rouvert, implémenté, exact, commité (`3710614`) |
| Formalisation | Avenant C4, avenant A12a, M12 complétée, M14, Référence H |

## Décisions

1. **Instrument de taille apparente** : 8ᵉ commutateur `LV3_LOD_STATS` (défaut 0, `#error` sans `LV3_PROFILE`), run de comptage (M10) ; rayon apparent par (instance, vue), en pixels, formule unique perspective / ortho `r_px = r × P[1][1] × H/2 ÷ w_min`, `w_min = w_centre − r × |col₃(VP)|`, profondeur au point le plus proche ; tranches < 1, 1-4, 4-16, ≥ 16 px sans branche ; `static_assert` de contiguïté des tranches.
2. **M11 appliquée à `ablation=`** : écrit par la LIB (`LV3_ABLATION_RASTER`), champ `ProfRunInfo::ablation` et ligne `info.ablation` de `main.cpp` supprimés ; nouveau champ `lodstats=`.
3. **Règle G1** (avant mesure) : G1 = 1 − Σ Fₖ·cₖ ÷ FacesSubmitted, calculée frame par frame ; zones ≥ 0,60 LOD, 0,30-0,60 2b puis LOD, < 0,30 2b. **Amendée avant toute mesure** (question d'Onky sur les autres meshes) : H1 appliquée à tous les meshes, G1 = borne basse déclarée (prudente pour les sphères, optimiste et bornée à < 0,1 % pour les meshes de moins de 64 faces).
4. **Décision d'Onky (28/09)** : le 2b est un passage obligatoire (exact, local, baisse le prix de chaque face) ; il passe avant le LOD. La règle G1 n'est pas appliquée ; l'instrument servira à dimensionner le LOD.
5. **LOD** (annexe A13) : chaîne de niveaux par asset, erreur géométrique mesurée hors ligne (Hausdorff ; écart radial pour un objet étoilé), projetée en pixels, sélection du niveau le plus grossier sous une tolérance (0,5 px) par (instance, vue), jamais stockée ; union des AABB de la chaîne pour le culling ; gizmos exclus ; validation sur la couverture (piège `FaceColor` par indice). Échelle H1 retirée (L1 admis jusqu'à 2,1 px, et non 16).
6. **Chantier 2b, conception** : projection par sommet pour les seules instances `Inside` (w > 0 garanti par le frustum) ; chemin `Intersect` inchangé (clipping, puis projection par coin) ; type distinct `RasterVertex {x, y, z, invW}` 16 o ; `ProjectClip` = seul endroit qui divise par w, utilisé par les deux chemins ; `ProjectPositions` = `MulRow` + `ProjectClip` dans une boucle sans branchement, copies locales (aliasing), `LV3_ASSERT(c.w > 0)` ; `RasterSpaceBuffer` possédé par l'appelant, 1 Mio, jamais partagé ; disposition AoS ; aval unique `EmitRasterTriangle` (backface → `TrisRasterized` → rejet 3b sauf Wireframe → `DrawTriangle`), `EmitClipTriangle` réduite à un adaptateur.
7. **Compteur `FacesInside`** en fin d'enum, sous `LV3_PROFILE` (compté par mesh, hors boucle chaude) ; η = FacesInside / FacesSubmitted.
8. **Prédiction et règle (avant mesure)** : −13 % (−8 à −18 %), loi ≈ 23 ns / face ; préalable exactitude ; zones ≤ −8 % succès, −8 à −3 % partiel, ±3 % neutre, ≥ +3 % retour arrière.
9. **Série A (exactitude) validée** : `RelWithAsserts`, `rasterstats=1` ; invariant 300/300 ; `TrisCov*`, `TrisEarlyRejected`, `PixelsTested`, `PixelsTight` et `PixelsCovered` (4 562 / 4 573 / 53 668) identiques à C3 § 8.1 ; aucune assertion ; compteurs identiques frame par frame avec les runs Release (simulation déterministe) ; η = 0,976.
10. **Run B isolé refusé** (pas de A dans la séance ; `Clear` +20 %, Animation et LocalXform1 +40 à +50 %) ; normalisation par témoin refusée → **M14**.
11. **Série B acceptée** : A = LIB `8c05c84` / EXE `2f75287`, B = 2b, `Release`, `affinity=4`, A B B A ; `Render` 7,491 / 5,141 / 5,159 / 7,986 ms ; **0,665 combiné (−33,5 %)** ; FRAME −25,8 % ; 27,1 → 18,7 ns par face ; témoin T absent, substitut déclaré (Animation, LocalXform1 à ±2,5 %). Verdict : **succès**.
12. **Lecture de l'écart** (−33,5 % contre −13 %) : division comptée au débit au lieu de sa latence sur le chemin critique d'un branchement imprévisible ; **M12 complétée** ; hypothèse cohérente avec C3 § 5.2, non démontrée.
13. **Référence H** : `Render` 5,14 à 5,16 ms, FRAME 7,33 à 7,37 ms (≈ 136 i/s), `Clear` ≈ 1,09 à 1,10 ms, 18,7 ns par face, η = 0,976. Chantier 4 (AVX2) : cible déplacée vers `ProjectPositions`.
14. **Avenant C4 et avenant A12a** demandés par Onky (comparaison ancien / nouveau, fonctionnement, intégration au pipeline) ; A12a remplace A12 § 2 à 7.
15. **Chantier 3a rouvert** (question d'Onky) comme mise en cohérence exacte, sans gain revendiqué ni ABBA : la boucle pixel parcourt `TightPixelBox` ; nouvel invariant `PixelsTested == PixelsTight` ; assertion de C3 retirée (tautologie) ; preuve = comparaison frame par frame avec l'ancienne boucle + TNR.
16. **3a validé** : ceinture (runs `3a` et `3a_affinity_4` contre `2B_serie_A`, tout identique 300/300, tests 14 372 → 10 562) ; `v1compat` (premier envoi non probant, runs 2b sans `rasterstats` ; second envoi `2B_sansbelt`, binaire Référence G recevable, assertion de C3 sur 12 514 966 pixels, tout identique 300/300, τ 0,909 cumulé) — ce run prouve aussi la projection par coin du 2b sur une scène à 93 % de faces clippées ; TNR Debug toute verte. **3a clos** (avenant C4 § 14).

## Chiffres de clôture

| Grandeur (ceinture, 4 vues, 1536×900) | Avant (Réf. G) | Après (Réf. H) |
|---|---|---|
| Projections par roche et par vue | 504 | 117 |
| `FacesSubmitted` / `TrisRasterized` / `TrisEarlyRejected` | 274 988 / 134 389 / 134 003 | identiques |
| `PixelsCovered` | 4 562 | 4 562 |
| `PixelsTested` (3a) | 14 372 | 10 562 |
| `Render`, A B B A cœur P | 7,491 / 7,986 ms | 5,141 / 5,159 ms (rapport 0,665) |
| Coût par face soumise | 27,1 ns | 18,7 ns |

## Erreurs de méthode reconnues pendant l'échange

- Règle G1 libellée « les roches ne sont pas assez petites » alors que le compteur porte sur toutes les instances ; échelle H1 calibrée sur les seules roches (corrigées avant mesure).
- Échelle H1 sans erreur géométrique mesurée (L1 « jusqu'à 16 px » au lieu de 2,1 px), relevée en écrivant A13.
- Annexe A13 : premier énoncé « un sommet de L3 dépasse l'AABB de L0 » faux (c'est L1, mesuré 0,011 à 0,032).
- Prédiction du 2b au débit de la division (−13 % contre −33,5 %).
- Hygiène : `741952a` poussé avec `LV3_LOD_STATS 1` par défaut ; `3710614` poussé avec `LV3_RASTER_STATS 1` (corrigé à `abb71c7`).

## Relevé à la reconstruction (13/11), sans décision

Voir `Plan_Phase_G_LibraryV3.md`, PH4CH2b-1 à PH4CH2b-11, en particulier : CSV de l'échange absents de `Mesures/` (PH4CH2b-1) ; compteurs `VertsInside` et `TrisEmittedInside` déclarés mais jamais incrémentés (PH4CH2b-2) ; commits de clôture avec un commutateur de comptage actif (PH4CH2b-4) ; pas de TNR d'équivalence des deux chemins (PH4CH2b-5) ; place des futurs attributs par sommet (PH4CH2b-6). Amendements de l'artefact : B3 (ProjectPositions8), D-a1 / D-a1d (accrochage dans `ProjectClip`), B4/B5/C3 (aval par lots de 8), LOD (bifurcation après sélection du niveau, bornes de chaîne), A20 (`ProjectClip` pour le rendu en point), C1/C2/D-a1 (boucle pixel refondue).
