# Résultats — Lot PH4-CH1 : le LOD des rochers

*Échange du 30/09 au 04/10/2026 (chantier 1 du plan de la consolidation C2 : « LOD »), consigné le 13/11/2026 avec l'artefact « Lot PH4-CH1 — Le LOD des rochers, pas à pas » (même adresse que l'ancienne page « Niveaux de détail LV3 », qu'il remplace). Ce fichier liste ce qui a été **décidé et validé** dans l'échange. Les points ouverts relevés à la reconstruction sont dans `Plan_Phase_G_LibraryV3.md`, section « Décisions en attente — lot PH4-CH1 » (PH4CH1-1 à PH4CH1-17) : rien de ce qui s'y trouve n'est acté.*

Documents produits par l'échange : annexe A13 bis (leçon LOD, PDF), annexe A14 (leçon complète du chantier, PDF, 13 pages), `Plan_Phase_G_LibraryV3.md` (première version), `gen_rock_lod.py`, `rock_a/b/c_L1..L3.obj`, `rock_a/b/c.lod.json`.

Commits de l'échange : non consignés dans l'échange (à relever sur le dépôt).

## Périmètre

| Objet | Statut à la fin de l'échange |
|---|---|
| Leçon LOD (A13 bis) | Livrée (PDF) |
| Instrument de taille apparente | Retrouvé mort (code commenté par le 2b), réinstallé dans le code actif, validé (identités 240/240) |
| Règle de décision écrite avant l'implémentation | Écrite, corrigée avant mesure (q = rayon compté ÷ 1,8) |
| Niveaux des rochers (L1 à L3) et descripteurs | Générés par Claude (Onky ne lance pas de Python), versionnables |
| `LodChain`, `SelectLodLevel`, stockage, chargeur, cache, chaînes implicites | Implémentés par Onky, compilés sans avertissement, tests verts en Debug |
| `MeshComponent::m_lodChain`, τ par vue, sélection dans `RenderView`, compteurs `LodLevel*` | Implémentés, trois runs à compteurs identiques avant branchement |
| Mesure | Lot M13 sur l'i7 : Render −71 %, frame −51 % ; Référence I fixée |
| Formalisation | Annexe A14 ; plan de la suite |

## Décisions

1. **Calcul des niveaux hors ligne**, jamais dans la LIB : `gen_rock_lod.py` joue le rôle de l'étape de build ; le moteur charge des niveaux déjà prêts.
2. **Le ResourceManager charge les niveaux comme des meshes** (cache et déduplication de la leçon 3) et construit un objet nouveau, la chaîne.
3. **τ appartient à la vue** : la chaîne stocke `invEps = 1/ε` (propriété de l'asset), la vue stocke `lodParams.kPx / τ` (amendement à A13 § 6.4).
4. **Résumés de leçons** : jamais de Word ; PDF s'il y a des graphiques, markdown sinon.
5. **Instrument** : réinstallé au point ① LOD (après le frustum, avant Inside / Intersect), sans `VertsInside` ni `TrisEmittedInside` ; tranches 1 / 4 / 16 px inchangées pour garder la comparabilité.
6. **Règle de décision (M12)** : GO si la borne basse garantie dépasse 25 % de `FacesSubmitted` ; prédiction `FacesSubmitted` ≤ 107 000 et `LodLevel3` ≥ 1 053 ; pas d'histogramme plus fin (il ne pouvait pas changer la décision).
7. **Fabrication des rochers** par ré-échantillonnage de `rock()` : L0 12 × 8, L1 8 × 5, L2 4 × 3, L3 4 × 2 ; niveaux non emboîtés, disposition A (un `MeshClass` par niveau).
8. **Mesure de ε** : écart radial, directions ciblées sur les sommets des deux maillages + 100 000 aléatoires, convergence vérifiée, × 1,1, arrondi vers le haut au 1/10 000.
9. **Descripteur `lv3.lodchain` v1** : `format`, `version`, `name`, `units: "local"`, niveaux `mesh` (relatif au descripteur), `vertices`, `faces` (contrôles, comptés comme le moteur : sommets référencés par une face), `epsilon` ; documentation sous des clés préfixées par `_` (convention de `WarnUnread`) ; ni τ ni AABB dans le fichier.
10. **`LodChain`** : `alignas(64)`, 64 octets, ordre des membres = ordre des lectures (bounds union, invEps, levels, levelCount), pas de séparation chaud/froid ; `static_assert` de taille et de copie triviale ; C4324 désactivé sur cette structure seulement.
11. **`SelectLodLevel`** : comptage des comparaisons `q′ ≤ invEps[i]` sur 3 tours fixes, cases inutilisées à −∞ ; +∞ et NaN rendent L0.
12. **Stockage** : `std::vector<LodChain>` indexé par l'id, case 0 sentinelle ; pointeur temporaire, handle durable ; `UnloadAll` vide les cases sans rétrécir le vecteur ; pas de version non const de `GetLodChain`.
13. **Porte d'entrée unique** `RegisterLodChain` : six invariants (nombre de niveaux, `invEps[0] = +∞`, bounds non vide, meshes chargés, `invEps` strictement décroissant et > 0 sous forme niée, cases inutilisées neutres).
14. **Correction de `JsonReader::ForEachElement`** (premier garde supprimé).
15. **Chargeur** : cinq phases, lecture brute avant tout chargement, défauts empoisonnés, sept erreurs typées ; cache par chemin canonique, succès seuls mis en cache ; meshes déjà chargés laissés en cache si la chaîne est refusée (effet assumé).
16. **Test `TestLodChain_Load`** avec un chemin d'échec volontaire (`FileNotFound`), annoncé dans le log.
17. **Composant** : remplacer `MeshHandle m_meshHandle` par `LodChainHandle m_lodChain` (option 1), pour que le compilateur inventorie les lecteurs ; même position du membre.
18. **Chaînes implicites** : tout mesh sans descripteur reçoit une chaîne de longueur 1, une par mesh, partagée, enregistrée par la porte d'entrée ; les gizmos portent des chaînes implicites, testées de longueur 1.
19. **τ** : section `lod` d'`engine.json` (réglage de qualité, pas de debug), défaut 0,5 px, garde `!(τ > 0) || !isfinite(τ)` ; surcharge par caméra ; résolution et division dans `BuildViewData`.
20. **Ordre de `RenderView`** : chaîne → culling sur l'union → `mvp` → q′ → niveau → mesh → gardes → `FacesSubmitted` ; l'instrument réutilise `rW` et `wC`. Optimisation du rayon local reportée (sans mesure).
21. **Compteurs `LodLevel0..3`** actifs dès `LV3_PROFILE`, en fin d'énumération, `static_assert` = `kMaxLevels` dans `RenderSystem.cpp`.
22. **Scène** : aiguillage explicite par l'extension `.lod.json` ; copie `solar_system_v1compat_belt_lod.json`, la scène de référence n'est pas modifiée.
23. **Options de chargement** : transmises au chargeur de chaînes sans valeur par défaut, une seule définition dans `ParseMesh`.
24. **L2 conservé** malgré sa dominance ; hystérésis, fondu et niveau « Culled » non implémentés.
25. **Popping** : aucun observé en vol (validation de τ = 0,5).
26. **Mesure** : le lot i9 non épinglé est non recevable ; mesure de référence par lot M13 sur l'i7 (jetable, A1 B1 B2 A2, `affinity 4`) ; aucune comparaison de temps entre machines (M14).
27. **Référence I** (i7-1265U épinglé, ceinture, 4 vues, 1536 × 900, Release) : Render 1,67 à 1,69 ms, frame 3,87 ms ; réserve déclarée sur B2 (p95 ÷ médiane 1,166 ; dispersion absolue 0,28 ms comme en A).
28. **Dettes notées pour après le chantier** : avertissements des clés optionnelles et surcharges invalides silencieuses (Serializer) ; cache de `LoadMeshChecked` sans les options (ResourceManager).
29. **Plan de la suite** : LOD des sphères, avec l'exigence que tout mesh puisse avoir ou non une chaîne ; puis `Clear` ; puis les dettes.

## Mesures

| | A (sans LOD) | B (avec LOD) | B ÷ A |
|---|---|---|---|
| Render, A1/B1 | 5,737 ms | 1,672 ms | 0,291 |
| Render, A2/B2 | 5,890 ms | 1,688 ms | 0,287 |
| Frame (médianes par run) | 7,885 / 8,052 ms | 3,873 / 3,877 ms | ≈ 0,49 |
| FacesSubmitted | 274 988 | 82 988 | 0,302 |
| VertsTransformed | 181 061 | 58 661 | 0,324 |
| LodLevel3 | — | 1 200 (chaque frame) | — |

Comptabilité exacte : 192 000 faces = 1 200 × (168 − 8) ; 122 400 sommets = 1 200 × (115 − 13). `MeshesCulled` identique. Compteurs identiques entre `RelWithAsserts` et `Release`, entre i9 et i7.

## Ce que la suite a modifié

Voir l'artefact, section 6 (AM1 à AM10), et la roadmap : sphères en LOD et interdiction de monter (A14a), dessin en point avant la sélection (A20), surcharges et caches refondus (A16 à A18), « 20 ns par face » requalifié en rapport et non en pente (A14a).
