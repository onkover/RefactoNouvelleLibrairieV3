# Audit de SparseSet.hpp

**B1**	Get() sans garde : si l'entité n'a pas le composant, m_Sparse[entity] peut valoir INVALID_INDEX → accès m_Dense[4294967295],
	comportement indéfini silencieux. Les //if (Has(entity)) commentés montrent que tu as hésité.
	La réponse professionnelle est assert(Has(entity)) : gratuit en Release, fatal et bruyant en Debug.																	Majeur
	=> Fait

**B2**	const uint32_t INVALID_INDEX = UINT32_MAX; est un membre d'instance : 4 octets gaspillés par SparseSet,
	non utilisable dans des contextes constexpr, et il empêche la génération de l'opérateur d'affectation par copie.
	Ce doit être static constexpr.																																		Majeur
	=> Fait

**B3**	ConstainsEntity — faute de frappe figée dans une interface virtuelle.
	Chaque classe dérivée devra reproduire la coquille. Corrige en Contains maintenant, avant que ça se propage.														Mineur
	=> Fait, migré sur TriggerComponent

**B4**	#include <iostream> dans un header inclus partout. iostream est l'un des headers les plus lourds de la STL et il n'est même pas utilisé ici.
	Dehors.																																								Mineur
	=> Fait

**B5**	MAX_ENTITIES_INIT est un const size_t à portée de namespace dans un header — chaque unité de traduction en reçoit une copie.
	En C++17+ : inline constexpr.	Mineur
	=> Fait

**B6**	Pas de Emplace avec perfect forwarding : Add(entity, T component) force une construction puis un move. Pour un composant lourd
	(ton TriggerComponent avec ses std::string et son std::set), c'est du travail inutile.																				Amélioration
	=> Fait

**B7**	Le sparse est un std::vector<uint32_t> plat : avec des IDs d'entités élevés, chaque SparseSet paie 4 octets × maxEntityID, même s'il ne stocke que 3 composants.
	EnTT résout ça avec un sparse paginé (pages de 4096 allouées à la demande).
	À garder pour plus tard — pas urgent à ton échelle.																													Amélioration
	=> Fait

**B8**	Auto-swap dans Remove : quand on supprime le dernier élément, m_Dense[i] = std::move(m_Dense[i]) est un self-move-assignment.
	Légal pour les types standards mais l'état résultant est « valide mais non spécifié » — ici sauvé par le pop_back immédiat.
	Un if (index_to_remove != last_index) autour du swap est plus propre et évite un move inutile.																		Mineur



# Audit du Registry et de son utilisation
**C1** de l'audit initial, le double-destroy se déclare mort au bon moment.
	=> Fait

**C2** — 🟠 MAJEUR : pas de versionnage des entités (le problème ABA)
Ton propre commentaire dans CreateEntity identifie le problème — je te le confirme : c'est indispensable, pas optionnel, et je te donne la solution en Partie F. Sans génération, tout système qui garde un Entity en mémoire (ton TriggerComponent::overlapping_entities, par exemple !) peut se retrouver à manipuler une entité recyclée qui n'a plus rien à voir avec l'originale. C'est exactement le problème ABA des structures lock-free, transposé à l'ECS.
	=> Fait, structurellement impossible, pas juste évités par convention — l'assert de DestroyEntity détonnera immédiatement si quelqu'un tente de contourner ça, et l'assert que tu viens d'ajouter dans Add audite silencieusement DestroyEntity à chaque insertion.

**C3** — 🟠 MAJEUR : le ComponentView ignore ses propres pointeurs
Ton ComponentView fait le travail difficile correctement : il capture les pointeurs de storage dans m_storages_ptr_tuple au moment de la construction, et il choisit le plus petit pool comme base d'itération — exactement la bonne stratégie. Et puis... l'itérateur n'utilise jamais ce tuple. SkipInvalidEntities appelle registry->hasComponent<T>() et operator* appelle registry->getComponent<T>(). Chacun de ces appels refait tout le chemin : recalcul du typeID, bounds check sur m_Storages, déréférencement du unique_ptr, static_cast. Par composant, par entité, à chaque frame. Pire : la version non-const de getStorage<T>() peut créer un pool pendant l'itération et redimensionner m_Storages. 
Le tuple contenait déjà les pointeurs typés — c'était le but de son existence. 
Correction en Partie F.
	=> Fait

**C4** — 🟠 MAJEUR : l'itérateur ment sur son type
using reference = value_type&; avec un mutable std::optional<value_type> rempli dans un operator*() const — c'est un contournement d'un problème que le C++ moderne a résolu proprement : le proxy iterator. Quand la valeur est fabriquée à la volée (un tuple de références), on la retourne par valeur : using reference = value_type;. C'est exactement ce que fait std::vector<bool>, et depuis C++20 les concepts d'itérateurs (std::input_iterator) l'acceptent officiellement. Ton hack fonctionne, mais il porte un état mutable inutile, il casse si deux operator* sont en vol, et surtout il montre qu'on a lutté contre le langage au lieu de l'écouter.
=> fait via F1/F3/F6

Bugs dans Systeme.cpp (l'utilisation)
**C5a**	WorldTransformSystem exécute worldIdentityMatrix.rotateX(45 * TO_RADIAN) sur une matrice reçue par référence, à chaque frame.
	Si l'appelant réutilise la même matrice, ta scène entière tourne de 45° supplémentaires par frame. Une matrice nommée « identity »
	qui n'en est plus une : mensonge sémantique + bug cumulatif.																								🔴 Critique
	=> Fait

**C5b**	HierarchyComponent children = registry.getComponent<HierarchyComponent>(entity);
	dans UpdateWorldTransforms : copie profonde du composant — donc du std::vector<Entity>
	— à chaque nœud, à chaque frame, dans une récursion. Il manque deux caractères : auto&.																		🟠 Majeur
	=> fait via F1/F3/F6


**C5c**	#pragma once en première ligne d'un fichier .cpp. Cette directive protège contre l'inclusion multiple d'un header ;
	dans un .cpp elle est du bruit qui trahit un copier-coller.																									Mineur
	=> Fait

**C5d**	TriggerSystem construit un ComponentView complet (recherche du plus petit pool, etc.) dans la boucle interne, pour chaque entité externe.
	Combiné à C3, ton O(N²) de collision est un O(N²) avec un gros facteur constant.
	Construis la vue une fois, ou mieux : collecte d'abord (entity, position) dans un vecteur local, puis fais le N² dessus.									🟠 Majeur
	=> fait via F1/F3/F6

**C5e**	Résidus : int b = 0; dans une branche else, RenderSystem défini dans le .cpp mais absent de System.hpp,
	fichier nommé Systeme.cpp vs header System.hpp — l'incohérence de nommage est une taxe cognitive permanente.												Mineur
	=> fait via F1/F3/F6


# Audit du ResourceManager
##	Constat	Sévérité
**D1**	UnloadMesh fait un scan linéaire O(N) de m_pathToMesh pour trouver le chemin correspondant au handle.
	Avec 500 meshes, décharger un niveau devient quadratique. Il faut une map inverse id → path, ou stocker le chemin dans le mesh.									🟠 Majeur
	=> Fait via F5

**D2**	Le cache n'est pas normalisé : "assets/cube.obj", "Assets/cube.obj" et "assets\\cube.obj" sont trois clés distinctes
	→ le même fichier chargé trois fois en mémoire, silencieusement. std::filesystem::weakly_canonical doit normaliser toute clé avant insertion/recherche.			🟠 Majeur
	=> Fait via F5

**D3**	LoadMesh retourne un handle invalide en cas d'échec, sans dire pourquoi (fichier absent ? OBJ malformé ? mesh vide ?).
	Tu as <expected> dans ton PCH, tu es en C++23 : std::expected<MeshHandle, ELoadError> est le canal d'erreur professionnel, sans exception.						🟠 Majeur
	=> Fait via F5

**D4**	Les handles ne portent pas de génération, mais comme les IDs sont monotones et jamais recyclés,
	un handle périmé après UnloadMesh retourne simplement nullptr via GetMesh.
	C'est sûr. Je le note pour que tu saches que c'est un choix acceptable, pas un oubli — mais documente-le.														Info
	=> reposer la question

**D5**	Aucune thread-safety. Acceptable aujourd'hui (chargement mono-thread au démarrage), mais le jour où tu voudras du chargement asynchrone,
	l'API actuelle (retour de pointeurs bruts vers l'intérieur des maps) devra être repensée. Note-le dans le code.													Amélioration
	=> todo

**D6**	"Ressources" (orthographe française) comme nom de dossier dans un codebase dont le reste est en anglais (Geometry, Rendering, Lighting).
	Cosmétique, mais l'incohérence se paie en #include ratés.																										Mineur
	=> todo

**F1** — Priorité absolue : entités versionnées + destruction sûre
	=> Fait

**F2** — SparseSet professionnalisé
	=> Fait de facto avec F1

**F3** — ComponentView : utiliser le tuple, retourner par valeur
	=> Fait

**F4** — Les composants référencent, ils ne possèdent pas
	=> Fait sur le renderSystem, à généraliser sur tous les composants qui contiennent des handles (TriggerComponent, AudioComponent, etc.)

**F5** — ResourceManager : erreurs typées, chemins normalisés, unload O(1)
	=> Fait

**F6** — Hygiène immédiate dans Systeme.cpp
	=> Fait


# Trigger system :
* optimiser la détection de collision naïve O(N²) en utilisant une broad-phase spatiale (grille, quadtree, etc.) pour réduire le nombre de comparaisons.
=> Todo

# E — La critique architecturale transversale : ton moteur a deux cerveaux
Component.hpp
=> Règle dictatoriale n°1 de cette leçon : un composant ne possède jamais une ressource. Il la référence par handle
=> Fait



# todo
* camera lissé : smoothSpeed, currentSmoothedPos
* tester la caméra CameraFollowComponent
* GetFaceView et son hypothèse de contiguïté
* ComputeMeshAABB() jamais appelée automatiquement, 
* les trois enums de RenderTypes.h sans usage effectif, 
* et les matériaux via submeshes n'ont utilisé.
* le clipping near en espace de clip, 
* l'interpolation perspective-correcte (z linéaire mais UV et couleurs en 1/w), 
* l'exploitation du troisième état du culling,
* cone culling par cluster,




# Les quatre couches sont en place et chacune ignore ce qui la précède
	RenderSystem  géométrie   transforme, cull, projette
	Renderer      état        mode, cibles, viewport
	Rasterizer    balayage    top-left, barycentriques
	Fragment      pixel       profondeur, couleur

```
**RenderView**   →  GÉOMÉTRIE   transforme, cull, projette, produit des RasterTriangle en espace écran
       ↓
**Renderer**     →  ÉTAT        choisit le fragment selon le mode, détient fb / db / viewport
       ↓  
**Rasterizer**   →  BALAYAGE    bounding box, top-left, barycentriques
       ↓ callback
**Fragment**     →  PIXEL       écrit une couleur
```


# Json : entité caméra
Chaque entité doit contenir un controleur et une lentille
```
   ┌─────────────────────────────────────────────────────────┐
   │  Entité « FPS_Camera »                                  │
   │                                                         │
   │   CameraFPS (contrôleur)        Camera (lentille)       │
   │      "enabled": true               "active": true       │
   │           │                             │               │
   │           ▼                             ▼               │
   │   « Ai-je le droit           « Suis-je la caméra        │
   │     de BOUGER cette            qui produit l'IMAGE ? »  │
   │     entité ? »                                          │
   │           │                             │               │
   │           ▼                             ▼               │
   │      m_local ────► Transform ────► View ────► rendu      │
   │      (position, rotation)                               │
   └─────────────────────────────────────────────────────────┘
```
## Exemple
```
    {
      "id": "Follow_Camera",
      "_note": "PAS de parent : CameraFollowSystem produit une position MONDE",
      "components": {
        "Transform": {
          "translation": [ 0.0, 5.0, 0.0 ],
          "rotation": [ 0.0, 0.0, 0.0 ],
          "scale": [ 1.0, 1.0, 1.0 ]
        },
        "Camera": {
          "projection": "perspective",
          "fov": 45.0,
          "near": 0.1,
          "far": 2000.0,
          "active": false,
          "priority": 5
        },
        "CameraFollow": {
          "enabled": true,
          "target": "Cube1",
          "offset": [ 0.0, 5.0, -35.0 ],
          "smoothSpeed": 5.0,
          "lookAtHeight": 0.0
        }
      }
    }
```

## Les quatre combinaisons sont toutes valides
enabled		active		Ce que ça donne
true		true		Caméra libre pilotée au clavier — ton cas
true		false		Elle bouge hors champ. Un observateur qui continue de suivre l'action ; tu bascules dessus (active = true) sans qu'elle ait « sauté » entre-temps
false		true		Caméra fixe. Overview_Camera : elle rend, elle ne bouge pas. Elle n'a aucun contrôleur du tout
false		false		Caméra en réserve, figée

La deuxième ligne est celle qui justifie la séparation. Sans elle, on pourrait effectivement fusionner les deux notions.
