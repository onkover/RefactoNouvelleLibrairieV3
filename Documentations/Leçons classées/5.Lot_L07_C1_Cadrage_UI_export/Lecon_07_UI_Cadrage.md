# Leçon 07 — Interface utilisateur : cadrage

> **Statut : document de cadrage — premier document de la Leçon 07.**
> Cet échange précède toute implémentation. Il fixe les besoins, écarte les technologies inadaptées, et pose les règles dictatoriales qui contraindront les parties P1, P2 et P3. Aucune ligne de code n'a été écrite. L'implémentation démarre après clôture de la Leçon 06 (scenegraph), sur go explicite.
> Artefact interactif associé : **Lot L07·C1 — Cadrage UI pas à pas** (`Lot_L07_C1_Cadrage_UI_export.html`).
> Prérequis : L04 (rasterizer, `EdgeFunction`, interpolation), L05 (`ViewData`, `Layout`, split-screen), A6 (`JsonReader`, `WarnUnread()`).

---

## 1. Les deux besoins

1. **Debug** — afficher des informations développeur : caméra active, temps de frame en ms, FPS, choix de la caméra. Outil, jamais livré au joueur.
2. **UI joueur** — menus (titre, options, pause) et un HUD simple (texte, barres, icônes). Produit, livré au joueur.

Ces deux besoins n'ont pas la même nature. Les professionnels les traitent avec deux systèmes distincts, sans exception notable : un outil immediate mode pour le debug, un système retained pour le joueur.

---

## 2. Le point qui inverse toute la comparaison

Dans un moteur GPU, intégrer une bibliothèque d'UI coûte quelques centaines de lignes de backend : un shader, un vertex buffer, un scissor. **Chez nous, il n'y a pas de GPU : le backend est un rasterizer 2D que nous devons écrire nous-mêmes.** Le premier critère de choix n'est donc ni l'ergonomie ni l'esthétique, mais la nature de ce que la bibliothèque demande de dessiner.

Trois familles :

| Famille | Ce que la lib produit | Coût pour nous |
|---|---|---|
| **A — listes de triangles** | Sommets `(pos, uv, couleur)`, atlas de police, rectangles de scissor | Un cas dégénéré de notre pipeline L04 : pas de projection, pas de depth, alpha blend, scissor. Un backend sert plusieurs libs. |
| **B — batches avec shaders** | Géométrie supposant des dizaines de variantes de shaders (texte SDF, dégradés, anti-aliasing vectoriel, effets) | Réimplémenter leur pipeline de rendu à la main. Rédhibitoire. |
| **C — bitmap rendue en interne** | Une image finale | Un blit. Mais on embarque un moteur web, un interpréteur JS, des threads, des dizaines de Mo. |

---

## 3. Comparatif

| Bibliothèque | Paradigme | Famille | Verdict |
|---|---|---|---|
| Dear ImGui | Immediate, code C++, MIT | A | **Retenu** pour le debug |
| Nuklear | Immediate, C, MIT | A | Redondant avec ImGui |
| RmlUi | Retained, HTML/CSS, MIT | A | Écarté (voir §4), porte de sortie documentée |
| NoesisGUI | Retained, XAML, commercial | B | Écarté |
| Coherent Gameface | Retained, HTML/JS, commercial | B/C | Écarté |
| Ultralight | HTML/JS, rendu interne | C | Écarté, disproportionné |
| Clay | Layout seul, header-only | — | Référence d'étude pour le layout maison, non adopté |
| Maison | Retained minimal, DOD | A | **Retenu** pour menus et HUD |

Les conditions de licence des solutions commerciales évoluent ; elles n'ont pas été vérifiées et n'ont pas pesé dans la décision.

### Ce que font les professionnels

- **Outillage / debug :** Dear ImGui est quasi universel, dans les moteurs propriétaires AAA comme chez les indépendants. Toujours derrière un flag de compilation, absent de la release.
- **UI joueur :** système retained propriétaire (Slate/UMG chez Unreal, UGUI/UI Toolkit chez Unity) ou middleware (NoesisGUI pour Baldur's Gate 3, Gameface sur de gros titres multijoueurs). Scaleform (Flash) fut le standard 2005–2018 ; il est mort.
- **Invariant :** deux systèmes. Personne ne met ImGui devant le joueur.

---

## 4. Contre-exemples

1. **Gameface sur CPU « parce que les artistes connaissent le HTML ».** Défendable sur GPU. Chez nous : des semaines de backend pour un menu de cinq boutons.
2. **Un arbre de widgets retained maison pour afficher trois lignes de FPS.** Le debug ne mérite pas une architecture.
3. **Un système de layout JSON pour l'overlay de debug.** ImGui n'a aucune position fixe : ses fenêtres se dimensionnent à leur contenu, se déplacent à la souris, et il persiste lui-même leur position dans `imgui.ini`. L'overlay ancré dans un coin se positionne relativement à `GetMainViewport()->WorkSize` et suit le redimensionnement. Écrire un layout JSON pour l'outil, c'est construire un second système d'UI pour éviter d'utiliser celui que la lib donne gratuitement.
4. **RmlUi pour un menu de trois écrans.** Ce que HTML/CSS apporterait réellement : un moteur de layout général, une cascade de styles, le prototypage dans un navigateur. Ce que ça coûte : ~40 000 lignes, un parser CSS, un DOM, une navigation manette de seconde classe. Un menu est une liste verticale centrée, un écran d'options est une grille label/valeur, un HUD simple est un jeu d'ancres. Trois schémas de layout, quelques centaines de lignes. Importer un moteur de layout général pour en utiliser 5 %, c'est adopter un framework pour se battre ensuite contre les 95 % restants.
5. **ImGui pour le menu du jeu, parce qu'il gère la manette.** Son immediate mode retraverse et re-layoute toute l'UI chaque frame, son style est celui d'un outil, et l'invariant professionnel tient.
6. **Positions en pixels codées en dur.** `DrawButton(412, 380, "Quitter")` : on change de résolution, tout casse. C'est le contre-exemple des jeux de jam.

---

## 5. Réponses au questionnaire de cadrage

| Question | Réponse | Conséquence |
|---|---|---|
| Entrées du menu | Clavier/souris, manette plus tard | Mapping d'actions (R7.1) ; la manette est une source ajoutée sans toucher aux widgets |
| Qui définit le menu | JSON + C++ | Contenu en données, comportement en code, liaison par identifiant (R7.2) |
| Dépendances tierces | Open source MIT (ImGui, stb, RmlUi) | ImGui et `stb_truetype` acceptés ; RmlUi écarté sur le fond, pas sur la licence |
| Périmètre UI joueur | Menus + HUD simple (texte, barres, icônes) | Atlas de sprites en plus de l'atlas de police ; positionnement par ancres (R7.4) |
| Ambition de rendu | Coins arrondis, dégradés, 9-slice, animations, transitions | Tout se réduit au primitif unique (R7.3) ; cycle de vie des écrans dès P2, tweens en P3 |
| Overlay debug en split-screen | Global, au-dessus de tout | Dessiné en dernier, espace fenêtre, ignore `ViewData` ; labels par vue possibles via `Layout` |
| Résolution du `FrameBuffer` | Suit la fenêtre (piste A) | `Rasterizer2D` écrit dans le `FrameBuffer` (R7.6) ; découplage reporté en dette D7.B |
| Menu de référence | Aucun | Conception pour les archétypes : liste verticale, grille d'options, HUD ancré |

### Transitions : pourquoi le cycle de vie est obligatoire dès P2

Une transition (« le menu pause glisse depuis la droite en 300 ms ») exige une horloge, des propriétés interpolables, des courbes d'easing, et un cycle de vie par écran : `Hidden → Entering → Active → Exiting`. Un écran en `Entering` ou `Exiting` est dessiné mais refuse les entrées ; sans cela, on clique sur un bouton qui n'est pas encore apparu. Le cycle de vie est présent dès P2, même si `Entering` et `Exiting` durent 0 ms ; le tableau de tweens de P3 s'y branche sans refonte. L'ajouter après coup est le refactor que la L06 impose actuellement au scenegraph.

En DOD, les tweens ne demandent aucun arbre d'objets : un tableau plat `{cible, propriété, de, vers, t0, durée, easing}` parcouru linéairement chaque frame, dans l'esprit des systèmes ECS ; les tweens terminés sont retirés par swap-and-pop.

### Coût du blend plein écran

Un panneau semi-transparent plein écran en 1080p, c'est 2,1 M de pixels blendés : plusieurs ms en scalaire. C'est le premier client naturel d'AVX2 dans le moteur (8 pixels par instruction). Écrit scalaire d'abord, vectorisé ensuite.

---

## 6. Piste A vs piste B : résolution du `FrameBuffer`

La texture SDL n'a aucune obligation d'être à la taille de la fenêtre ; `SDL_RenderCopy` l'étire sur le GPU, sans coût CPU, avec le filtre choisi (`nearest` = pixels nets, `linear` = lissage léger).

- **Piste A (retenue)** — `FrameBuffer`, texture et fenêtre à la même taille, recréés au redimensionnement. Une seule surface, l'UI se dessine directement dedans. Le coût du rasterizer 3D suit la taille de la fenêtre : passer de 1280×720 à 2560×1440 le quadruple.
- **Piste B (dette D7.B)** — résolution interne fixe pour la 3D (ex. 640×360, neuf fois moins cher qu'en 1080p), étirée par SDL. L'UI ne peut pas être dessinée dans ce `FrameBuffer` : un glyphe de 12 px étiré ×3 est illisible. Il faut soit un second buffer de composition à la taille de la fenêtre, soit deux textures SDL superposées (3D + RGBA transparente pour l'UI). La « bouillie » de la 3D ne vient pas d'un algorithme manquant mais du fait qu'on rend avec 9× moins d'échantillons : choix esthétique assumé (pixel art 3D + texte net), pas faiblesse technique.

---

## 7. Règles dictatoriales

| Règle | Énoncé |
|---|---|
| **R7.0** | L'UI est une couche 2D composée dans le `FrameBuffer` après `RenderView×N`, sans `DepthBuffer`, sans projection. Elle ne passe jamais par `ViewData`. |
| **R7.1** | Le menu ne lit jamais un périphérique. Il consomme des actions (`Up`, `Down`, `Left`, `Right`, `Confirm`, `Back`, `Pointer`) produites par une couche de mapping clavier/manette/souris, elle-même configurée en JSON. La souris n'est qu'une source de focus parmi d'autres. |
| **R7.2** | Le contenu du menu et du HUD (items, hiérarchie, textes, style, ancres) vit en JSON chargé par `JsonReader` avec `WarnUnread()`. Le C++ ne définit que le comportement et ne référence les items que par identifiant, jamais par position. Aucune coordonnée en pixels dans le code. Le JSON emprunte le vocabulaire de CSS (`anchor`, `padding`, `margin`, `direction`, `gap`) sans son parser ni sa cascade. |
| **R7.3** | `Rasterizer2D` n'expose qu'un primitif : le triangle texturé, à couleur par sommet, sous scissor et alpha blend. Le backend ImGui, le menu et le HUD tessellent vers lui (coins arrondis = arcs tessellés, dégradé = couleurs de sommet, 9-slice = neuf quads). Rien d'autre ne touche le `FrameBuffer`. |
| **R7.4** | Tout élément d'UI joueur est positionné par ancre (9 points : coins, milieux des bords, centre) plus offset et taille en unités de référence, jamais en pixels absolus. |
| **R7.5** | Ordre de composition, chaque couche pouvant être absente : (1) rendu 3D `RenderView×N` ; (2) HUD, uniquement en jeu ; (3) écran de menu plein écran, avec quad semi-transparent assombrissant ce qui est derrière — la 3D continue derrière un menu pause, peut être sautée derrière l'écran titre ; (4) overlay debug, toujours en dernier, jamais assombri. Un menu plein écran n'est pas un mode de rendu, c'est une couche qui recouvre. |
| **R7.6** | `Rasterizer2D` écrit dans le `FrameBuffer` courant, à la résolution de la fenêtre. Il reçoit sa surface cible en paramètre (pointeur, largeur, hauteur, pitch) et ne connaît pas le `FrameBuffer` par son nom. Le jour de D7.B, on lui passe une autre surface ; rien d'autre ne bouge. |

---

## 8. Architecture retenue

Trois couches, une seule rasterisation :

1. **`Rasterizer2D` (LIB)** — le primitif R7.3. Atlas de police construit au démarrage avec `stb_truetype` (MIT, header-only, déjà embarqué dans ImGui : aucune dépendance nouvelle). Atlas de sprites pour les icônes du HUD.
2. **Backend Dear ImGui (debug)** — traduit `ImDrawData` (`ImDrawVert` = pos, uv, col ; `ClipRect` ; `TextureId` de l'atlas Alpha8/RGBA32) vers la couche 1. Transmet la taille de fenêtre à `io.DisplaySize` chaque frame. Compilé derrière un flag, absent de la release. Le seul JSON côté debug : panneaux visibles au démarrage et coin de l'overlay.
3. **`MenuSystem` maison (jeu)** — retained et DOD : tableaux d'items chargés du JSON, index de focus, table de navigation, style, cycle de vie des écrans, pile d'écrans. Quelques centaines de lignes. Émet dans la couche 1.

Technologies : Dear ImGui, `stb_truetype`, tout le reste maison. Écartés : RmlUi, NoesisGUI, Coherent Gameface, Ultralight, Nuklear.

---

## 9. Plan de la leçon

| Partie | Contenu | Besoin couvert |
|---|---|---|
| **P1** | `Rasterizer2D`, atlas de police et de sprites, backend ImGui, overlay debug (caméra, ms, FPS) | Besoin 1 |
| **P2** | Mapping d'actions (R7.1), schéma JSON (R7.2, R7.4), `MenuSystem` retained, focus et navigation, cycle de vie des écrans | Besoin 2 (menus) |
| **P3** | Tableau de tweens, transitions, pile d'écrans, HUD lié à l'état du jeu | Besoin 2 (HUD, animations) |

Chaque partie suit le contrat pédagogique : théorie → audit → proposition justifiée → implémentation → test TNR → markdown.

---

## 10. Écarts avec la roadmap

Relevés en relisant le carnet de route et les annexes lors de la construction du lot interactif L07·C1. **Rien n'est acté** : ce sont des décisions à prendre plus tard, inscrites à la roadmap.

| Source | Ce qu'elle dit | Ce que dit le cadrage | Statut |
|---|---|---|---|
| **A9 §9** | Overlay de debug *par viewport*, portant `orthoHeight` / `fov` / `aspect` (le scalaire perdu par le gizmo ortho) | Overlay *global*, au-dessus de tout | **Compatible**, à confirmer. L'overlay global place un panneau ancré dans le coin de chaque viewport en lisant le `Layout`. Évolution **E7.1**. Sans elle, la dette A9 §9 — celle qui « bloque L07 » dans le carnet — ne serait pas soldée par la leçon. |
| **Roadmap L07** | « Police bitmap embarquée, coût nul en release » | `stb_truetype` + atlas | **Compatible**, à acter. ImGui embarque sa police par défaut (ProggyClean, compilée dans le binaire, rasterisée à l'init par `stb_truetype`) : aucun fichier à livrer. Le coût nul en release vient du flag de compilation. Décision **E7.2** : police par défaut pour le debug, TTF chargée pour le menu joueur. |
| **Roadmap L07** | « Overlay de debug + profiler » | Profiler non abordé | **Écart réel.** Un profiler (temps par système de l'ordre canonique S13) est la suite naturelle de la campagne de mesure G. Dette **D7.Prof** : périmètre à décider avant P1. |
| **Carnet — différés à dessein** | `EBlendMode` inutilisé | L'UI a besoin d'alpha blend | **À réexaminer en P1.** L'UI sera le premier client de `EBlendMode` : réutiliser l'enum, ou la laisser à la 3D et garder le blend implicite dans `Rasterizer2D`. |

### Points à trancher en P1, issus du lot interactif

- **Arrondi du blend en entiers.** Diviser par 256 (`>> 8`) au lieu de 255 est faux : `blend(255, 255, 255)` donne 254, et quatre couches de blanc à `a = 128` empilées sur du blanc dérivent en 254 → 253 → 253 → 253 au lieu de rester à 255. Le blend se fera en float32 avec arrondi (`+ 0.5f` en scalaire, `_mm256_cvtps_epi32` en AVX2), ou en entier avec une division par 255 exacte. Le test TNR « blend des extrêmes » le verrouillera.
- **Transitions sur `realDt`.** Les tweens d'UI tournent sur le temps réel de `SimulationClock`, jamais sur `simDt` : un menu pause qui se fige parce que la simulation est en pause serait absurde.

---

## 11. Dettes ouvertes

| Dette | Description |
|---|---|
| **D7.B** | Résolution interne découplée de la fenêtre (piste B). Nécessite une seconde surface de composition ou deux textures SDL superposées. R7.6 rend le passage indolore pour `Rasterizer2D`. |
| **D7.AVX** | Vectorisation AVX2 du blend plein écran. Scalaire d'abord. |
| **D7.Rml** | RmlUi reste la porte de sortie documentée si le HUD devient riche (inventaire, minimap). Même famille A : le backend `Rasterizer2D` resterait valable. |
| **D7.Pad** | Navigation manette : source d'actions ajoutée après P2, sans modification des widgets. |
| **D7.Prof** | Profiler intégré à l'overlay de debug, annoncé par la roadmap mais absent du cadrage. Périmètre à décider avant P1. |

Évolutions à confirmer : **E7.1** (panneaux par viewport dans l'overlay global, solde A9 §9), **E7.2** (police ImGui embarquée pour le debug, TTF pour le menu).

---

## 12. Prochaine étape

Clôture de la Leçon 06 (scenegraph). Puis, sur go explicite : Leçon 07 P1.
