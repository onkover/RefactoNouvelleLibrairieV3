/*
	(c) Onkover

	26/06/26
	Nouvelle gestion de librairie graphique v3
	S'appuie sur le librairie V2.1 entierement réécrite par Claude.ai
	
	todo:

*/

/*
todo



/*

Audit de SparseSet.hpp

B1	Get() sans garde : si l'entité n'a pas le composant, m_Sparse[entity] peut valoir INVALID_INDEX → accès m_Dense[4294967295],
	comportement indéfini silencieux. Les //if (Has(entity)) commentés montrent que tu as hésité.
	La réponse professionnelle est assert(Has(entity)) : gratuit en Release, fatal et bruyant en Debug.																	Majeur
	=> Fait

B2	const uint32_t INVALID_INDEX = UINT32_MAX; est un membre d'instance : 4 octets gaspillés par SparseSet,
	non utilisable dans des contextes constexpr, et il empêche la génération de l'opérateur d'affectation par copie.
	Ce doit être static constexpr.																																		Majeur
	=> Fait

B3	ConstainsEntity — faute de frappe figée dans une interface virtuelle.
	Chaque classe dérivée devra reproduire la coquille. Corrige en Contains maintenant, avant que ça se propage.														Mineur
	=> Fait, migré sur TriggerComponent

B4	#include <iostream> dans un header inclus partout. iostream est l'un des headers les plus lourds de la STL et il n'est même pas utilisé ici.
	Dehors.																																								Mineur
	=> Fait

B5	MAX_ENTITIES_INIT est un const size_t à portée de namespace dans un header — chaque unité de traduction en reçoit une copie.
	En C++17+ : inline constexpr.	Mineur
	=> Fait

B6	Pas de Emplace avec perfect forwarding : Add(entity, T component) force une construction puis un move. Pour un composant lourd
	(ton TriggerComponent avec ses std::string et son std::set), c'est du travail inutile.																				Amélioration
	=> Fait

B7	Le sparse est un std::vector<uint32_t> plat : avec des IDs d'entités élevés, chaque SparseSet paie 4 octets × maxEntityID, même s'il ne stocke que 3 composants.
	EnTT résout ça avec un sparse paginé (pages de 4096 allouées à la demande).
	À garder pour plus tard — pas urgent à ton échelle.																													Amélioration
	=> Fait

B8	Auto-swap dans Remove : quand on supprime le dernier élément, m_Dense[i] = std::move(m_Dense[i]) est un self-move-assignment.
	Légal pour les types standards mais l'état résultant est « valide mais non spécifié » — ici sauvé par le pop_back immédiat.
	Un if (index_to_remove != last_index) autour du swap est plus propre et évite un move inutile.																		Mineur



Audit du Registry et de son utilisation
C1 de l'audit initial, le double-destroy se déclare mort au bon moment.
	=> Fait

C2 — 🟠 MAJEUR : pas de versionnage des entités (le problème ABA)
Ton propre commentaire dans CreateEntity identifie le problème — je te le confirme : c'est indispensable, pas optionnel, et je te donne la solution en Partie F. Sans génération, tout système qui garde un Entity en mémoire (ton TriggerComponent::overlapping_entities, par exemple !) peut se retrouver à manipuler une entité recyclée qui n'a plus rien à voir avec l'originale. C'est exactement le problème ABA des structures lock-free, transposé à l'ECS.
	=> Fait, structurellement impossible, pas juste évités par convention — l'assert de DestroyEntity détonnera immédiatement si quelqu'un tente de contourner ça, et l'assert que tu viens d'ajouter dans Add audite silencieusement DestroyEntity à chaque insertion.

C3 — 🟠 MAJEUR : le ComponentView ignore ses propres pointeurs
Ton ComponentView fait le travail difficile correctement : il capture les pointeurs de storage dans m_storages_ptr_tuple au moment de la construction, et il choisit le plus petit pool comme base d'itération — exactement la bonne stratégie. Et puis... l'itérateur n'utilise jamais ce tuple. SkipInvalidEntities appelle registry->hasComponent<T>() et operator* appelle registry->getComponent<T>(). Chacun de ces appels refait tout le chemin : recalcul du typeID, bounds check sur m_Storages, déréférencement du unique_ptr, static_cast. Par composant, par entité, à chaque frame. Pire : la version non-const de getStorage<T>() peut créer un pool pendant l'itération et redimensionner m_Storages. 
Le tuple contenait déjà les pointeurs typés — c'était le but de son existence. 
Correction en Partie F.
	=> Fait

C4 — 🟠 MAJEUR : l'itérateur ment sur son type
using reference = value_type&; avec un mutable std::optional<value_type> rempli dans un operator*() const — c'est un contournement d'un problème que le C++ moderne a résolu proprement : le proxy iterator. Quand la valeur est fabriquée à la volée (un tuple de références), on la retourne par valeur : using reference = value_type;. C'est exactement ce que fait std::vector<bool>, et depuis C++20 les concepts d'itérateurs (std::input_iterator) l'acceptent officiellement. Ton hack fonctionne, mais il porte un état mutable inutile, il casse si deux operator* sont en vol, et surtout il montre qu'on a lutté contre le langage au lieu de l'écouter.
=> fait via F1/F3/F6

Bugs dans Systeme.cpp (l'utilisation)
C5a	WorldTransformSystem exécute worldIdentityMatrix.rotateX(45 * TO_RADIAN) sur une matrice reçue par référence, à chaque frame.
	Si l'appelant réutilise la même matrice, ta scène entière tourne de 45° supplémentaires par frame. Une matrice nommée « identity »
	qui n'en est plus une : mensonge sémantique + bug cumulatif.																								🔴 Critique
	=> Fait

C5b	HierarchyComponent children = registry.getComponent<HierarchyComponent>(entity);
	dans UpdateWorldTransforms : copie profonde du composant — donc du std::vector<Entity>
	— à chaque nœud, à chaque frame, dans une récursion. Il manque deux caractères : auto&.																		🟠 Majeur
	=> fait via F1/F3/F6


C5c	#pragma once en première ligne d'un fichier .cpp. Cette directive protège contre l'inclusion multiple d'un header ;
	dans un .cpp elle est du bruit qui trahit un copier-coller.																									Mineur
	=> Fait

C5d	TriggerSystem construit un ComponentView complet (recherche du plus petit pool, etc.) dans la boucle interne, pour chaque entité externe.
	Combiné à C3, ton O(N²) de collision est un O(N²) avec un gros facteur constant.
	Construis la vue une fois, ou mieux : collecte d'abord (entity, position) dans un vecteur local, puis fais le N² dessus.									🟠 Majeur
	=> fait via F1/F3/F6

C5e	Résidus : int b = 0; dans une branche else, RenderSystem défini dans le .cpp mais absent de System.hpp,
	fichier nommé Systeme.cpp vs header System.hpp — l'incohérence de nommage est une taxe cognitive permanente.												Mineur
	=> fait via F1/F3/F6


Audit du ResourceManager
#	Constat	Sévérité
D1	UnloadMesh fait un scan linéaire O(N) de m_pathToMesh pour trouver le chemin correspondant au handle.
	Avec 500 meshes, décharger un niveau devient quadratique. Il faut une map inverse id → path, ou stocker le chemin dans le mesh.									🟠 Majeur
	=> Fait via F5

D2	Le cache n'est pas normalisé : "assets/cube.obj", "Assets/cube.obj" et "assets\\cube.obj" sont trois clés distinctes
	→ le même fichier chargé trois fois en mémoire, silencieusement. std::filesystem::weakly_canonical doit normaliser toute clé avant insertion/recherche.			🟠 Majeur
	=> Fait via F5

D3	LoadMesh retourne un handle invalide en cas d'échec, sans dire pourquoi (fichier absent ? OBJ malformé ? mesh vide ?).
	Tu as <expected> dans ton PCH, tu es en C++23 : std::expected<MeshHandle, ELoadError> est le canal d'erreur professionnel, sans exception.						🟠 Majeur
	=> Fait via F5

D4	Les handles ne portent pas de génération, mais comme les IDs sont monotones et jamais recyclés,
	un handle périmé après UnloadMesh retourne simplement nullptr via GetMesh.
	C'est sûr. Je le note pour que tu saches que c'est un choix acceptable, pas un oubli — mais documente-le.														Info
	=> reposer la question

D5	Aucune thread-safety. Acceptable aujourd'hui (chargement mono-thread au démarrage), mais le jour où tu voudras du chargement asynchrone,
	l'API actuelle (retour de pointeurs bruts vers l'intérieur des maps) devra être repensée. Note-le dans le code.													Amélioration
	=> todo

D6	"Ressources" (orthographe française) comme nom de dossier dans un codebase dont le reste est en anglais (Geometry, Rendering, Lighting).
	Cosmétique, mais l'incohérence se paie en #include ratés.																										Mineur
	=> todo

F1 — Priorité absolue : entités versionnées + destruction sûre
	=> Fait

F2 — SparseSet professionnalisé
	=> Fait de facto avec F1

F3 — ComponentView : utiliser le tuple, retourner par valeur
	=> Fait

F4 — Les composants référencent, ils ne possèdent pas
	=> Fait sur le renderSystem, à généraliser sur tous les composants qui contiennent des handles (TriggerComponent, AudioComponent, etc.)

F5 — ResourceManager : erreurs typées, chemins normalisés, unload O(1)
	=> Fait

F6 — Hygiène immédiate dans Systeme.cpp
	=> Fait

=> Ordre de bataille recommandé : F1 (entités versionnées — tout le reste en dépend), F2, F3, F6, puis F4 et F5.

Trigger system :
* optimiser la détection de collision naïve O(N²) en utilisant une broad-phase spatiale (grille, quadtree, etc.) pour réduire le nombre de comparaisons.


E — La critique architecturale transversale : ton moteur a deux cerveaux
Component.hpp
=> Règle dictatoriale n°1 de cette leçon : un composant ne possède jamais une ressource. Il la référence par handle
=> à voir plus tard


*/





#include "pch.h"          // ← première ligne, toujours
#include "Core/Platform.h"
#include "Core/Logger.h"
#include "helper/ConfigManager.h"

// Gestion du scenegraph
#include "Scene/Registry.hpp"
#include "Core/EventBus.hpp"
#include "Scene/SceneGraph.hpp"
#include "Scene/system.hpp"
#include "Scene/Serializer.hpp"

#include <thread>

using namespace LV3;

void TestF1_EntityVersioning()
{
	LV3::Registry reg;

	// --- 1. Le recyclage réutilise l'index mais change le handle ---
	LV3::Entity a = reg.CreateEntity();							// index 0, gen 0
	reg.addComponent(a, LV3::HealthComponent{ 100, 100 });
	reg.DestroyEntity(a);
	LV3::Entity b = reg.CreateEntity();							// index 0, gen 1
	assert(LV3::EntityIndex(a) == LV3::EntityIndex(b));			// même slot...
	assert(a != b);												// ...ticket différent

	// --- 2. Le périmé est mort, le neuf est vivant ---
	assert(!reg.IsAlive(a));
	assert(reg.IsAlive(b));

	// --- 3. Aucun composant fantôme ne traverse les générations ---
	assert(!reg.hasComponent<LV3::HealthComponent>(a));			// handle périmé → refusé par Has()
	assert(!reg.hasComponent<LV3::HealthComponent>(b));			// nouvelle entité → vierge

	// --- 4. Le scénario TriggerComponent : un handle stocké survit à son entité ---
	LV3::Entity held = reg.CreateEntity();
	reg.DestroyEntity(held);
	//reg.DestroyEntity(held);				// Un assert doit se déclencher ici si on tente de détruire une entité déjà morte
	reg.CreateEntity();											// recycle le slot de 'held'
	assert(!reg.IsAlive(held));									// le voisin mémorisé est bien déclaré mort

	std::cout << "[F1] Versionnage des entités : tous les invariants tiennent.\n";
}

void TestF5_ResourceManager_UnloadMesh()
{
	ResourceManager rm;
	OBJLoadOptions opts;

	// Charge plusieurs meshes distincts — adapte ces chemins à des .obj réels de ton projet
	const std::vector<std::string> paths = {
		"Assets/cube.obj",
		"Assets/sphere 10 faces.obj"
	};

	std::vector<MeshHandle> handles;
	for (const auto& p : paths)
	{
		auto result = rm.LoadMeshChecked(p, opts);
		assert(result.has_value() && "Echec de chargement — verifie que les chemins de test existent");
		handles.push_back(*result);
	}

	const size_t countBefore = rm.GetMeshCount();
	assert(countBefore == paths.size());

	// --- CIBLE : le PREMIER mesh de la liste ---
	const MeshHandle target = handles[0];
	const std::string targetPath = paths[0];

	// 1. Invariants AVANT suppression — la cible est bien vivante et retrouvable dans les deux sens
	assert(rm.GetMesh(target) != nullptr);
	assert(rm.IsMeshLoaded(targetPath));
	assert(rm.FindMesh(targetPath) == target);

	// --- ACTION ---
	rm.UnloadMesh(target);

	// 2. Invariants APRÈS suppression
	assert(rm.GetMeshCount() == countBefore - 1);        // exactement un mesh de moins, pas plus
	assert(rm.GetMesh(target) == nullptr);               // le handle périmé résout désormais à null
	assert(!rm.IsMeshLoaded(targetPath));                // preuve que m_meshIdToPath a bien retrouvé
	assert(rm.FindMesh(targetPath) == MeshHandle::Invalid());  // et nettoyé m_pathToMesh — le cœur de F5

	// 3. Les AUTRES meshes ne doivent SUBIR AUCUN effet de bord
	//    (garde contre une éventuelle confusion d'index dans la map inverse)
	for (size_t i = 1; i < handles.size(); ++i)
	{
		assert(rm.GetMesh(handles[i]) != nullptr);
		assert(rm.IsMeshLoaded(paths[i]));
		assert(rm.FindMesh(paths[i]) == handles[i]);
	}

	// 4. Double-unload : doit être un no-op silencieux, jamais un crash
	//    (m_meshIdToPath ne retrouve plus rien pour ce handle -> if() ne s'exécute pas -> erase(id) sur un id déjà absent, sans effet)
	rm.UnloadMesh(target);
	assert(rm.GetMeshCount() == countBefore - 1);        // aucun décrément supplémentaire

	// 5. Recharger le même chemin doit fonctionner normalement, sans résidu de l'ancien handle
	auto reload = rm.LoadMeshChecked(targetPath, opts);
	assert(reload.has_value());
	assert(reload->id != target.id);                     // AllocateMeshHandle ne recycle jamais les ids : nouveau mesh, nouvel id
	assert(rm.IsMeshLoaded(targetPath));
	assert(rm.GetMeshCount() == countBefore);             // on est revenu au compte initial

	std::cout << "[F5] UnloadMesh : tous les invariants tiennent.\n";
}

int main()
{

	SetConsoleMode();	// mode cosole en UTF-8

	///************************************************************
	//Lecture du nom des répertoires depuis la base de registres
	//************************************************************/
	std::string repObjDefault;
	std::string repGfxDefault;

	if (!ProgrammeConfig("config.json", repObjDefault, repGfxDefault))
	{
		Logger::error("\033[31mImpossible de charger la configuration.\033[0m");
		return 1;
	}
	Logger::log("\033[32mConfiguration chargée avec succès.\033[0m");

	
	/************************************************************
	Lecture du scenegraph
	************************************************************/
	std::cout << "\n\033[32m=== Lecture de solar_system.json ===\033[0m" << std::endl;

	Registry registry;
	EventBus eventBus;
	HealthSystem healthSys(&registry, eventBus);
	AudioSystem audioSys(&registry, eventBus);
	ResourceManager rm;					// Collection de mesh unitaires
	Entity activeCamera;

	// --- SETUP DE LA SCÈNE ---
	std::string cheminProjet = PROJECT_DIR; // path du projet définit dans l'Explorateur de projet > Propriétés.;
	// C/C++ > Préprocesseur.
	// Définitions de préprocesseur => PROJECT_DIR=R"($(ProjectDir))"
	// (Le R"(...)" est un Raw String Literal en C++, ça permet d'éviter que les antislashs \ de Windows ne fassent planter la chaîne de caractères).

	bool success = SceneSerializer::LoadSceneGraph(cheminProjet, "assets/solar_system.json", registry, activeCamera, rm);

	if (!success)
	{
		std::cerr << "Impossible de construire la scène. Arrêt du programme." << std::endl;
		return -1; 
	}

	// --- VÉRIFICATION : AFFICHAGE DE L'ARBRE CONSTRUIT ---
	std::cout << "Structure finale du Scene Graph :" << std::endl;
	DebugDisplaySystem(registry);
	TestF1_EntityVersioning();
	TestF5_ResourceManager_UnloadMesh();

	// --- BOUCLE DE JEU ---

	int frameCount = 0;
	const int maxFrames = 5; // Arrête la simulation après 100 images
	float deltaTime = 0.5f; // Temps fixe pour une simulation stable

	std::map < Entity, std::string> entityNames;	// pour le debugage

	while (frameCount < maxFrames) {
		// Nettoie la console (fonctionne sur Linux/macOS, pour Windows utiliser "cls")
		// system("clear"); 

		std::cout << std::endl;
		std::cout << "--- FRAME " << frameCount << " ---" << std::endl;

		Matrix44f mIndentity;

		// 1. Gérer les entrées utilisateur (non implémenté ici)
		PlayerInputSystem(registry, deltaTime);

		// 2. Mettre à jour la scène
		// L'update commence à la racine, avec une matrice identité car elle n'a pas de parent.

		// --- 1. MISE À JOUR DE L'ÉTAT (Logique pure) ---
		AnimationSystem(registry, deltaTime);
		CameraSystem(registry, deltaTime);    // Met à jour les positions lissées => non implémenté ici

		// --- 2. MISE À JOUR DES MATRICES ---
		//TransformationSystem(registry, deltaTime);
		LocalTransformSystem(registry);       // Construit les matrices locales finales
		WorldTransformSystem(registry, mIndentity);       // Construit les matrices mondes finales

		// --- 3. DÉTECTION (Physique/Triggers) ---
		// Lit les matrices mondes finales
		TriggerSystem(registry, eventBus);

		// --- 4. DESSIN ---
		// Débug de la hiérarchie 
		DebugDisplaySystem(registry);// , entityNames);

		// Draw de la hiérarchie
		RenderSystem(registry, activeCamera, rm);

		// Pause pour rendre l'animation lisible dans la console
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		frameCount++;
	}


// TNR , supprime un mesjh	


}
