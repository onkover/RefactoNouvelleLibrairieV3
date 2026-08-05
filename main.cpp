/*
	(c) Onkover

	26/06/26
	Nouvelle gestion de librairie graphique v3
	S'appuie sur le librairie V2.1 entierement réécrite par Claude.ai
	

*/

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


Trigger system :
* optimiser la détection de collision naïve O(N²) en utilisant une broad-phase spatiale (grille, quadtree, etc.) pour réduire le nombre de comparaisons.
=> Todo

E — La critique architecturale transversale : ton moteur a deux cerveaux
Component.hpp
=> Règle dictatoriale n°1 de cette leçon : un composant ne possède jamais une ressource. Il la référence par handle
=> Fait



todo
* camera lissé : smoothSpeed, currentSmoothedPos
* tester la caméra CameraFollowComponent
*/


#define MAIN



#include "pch.h"          // ← première ligne, toujours
#include "main.h"

#include <SDL_ttf.h>
#include <thread>

#include "Core/Platform.h"
#include "Core/Logger.h"
#include "Core/InputState.h"
#include "helper/ConfigManager.h"

// Gestion du scenegraph
#include "Scene/Registry.hpp"
#include "Core/EventBus.hpp"
#include "Scene/SceneGraph.hpp"
#include "Scene/system.hpp"
#include "Scene/Serializer.hpp"
#include "Rendering/Renderer.h"

#if LV3_DEBUG
	#include "Test/Test_TopLeftRule.h"
#endif
using namespace LV3;

void TestF1_EntityVersioning();
void TestF5_ResourceManager_UnloadMesh();
int RunAllCameraMathTests();
int TestProjection();
int TestMatrixLib();
bool Test_TopLeftRule_NoDoubleCoverage();



bool g_running = true;

// ---------- une fois par frame ----------
/*
1. Clavier = état, souris = événement. Le clavier se lit avec SDL_GetKeyboardState (« la touche est-elle enfoncée maintenant »). La souris s'accumule (« de combien a-t-elle bougé depuis la dernière lecture »). Confondre les deux donne une caméra saccadée ou un déplacement qui ne s'arrête pas.
2. InputState est reconstruit entièrement chaque frame. Il est local à BuildInputState(), donc remis à zéro par construction. Si tu en fais une variable globale persistante, wheelDelta et toggleCameraMode s'accumuleront indéfiniment — la caméra basculera de mode à chaque frame
3. mouseDeltaX/Y ne se multiplient jamais par dt. C'est un déplacement en pixels déjà accompli, pas une vitesse. Le clavier, lui, si
*/
LV3::InputState BuildInputState()
{
	LV3::InputState in;

	// 1. Événements ponctuels (molette, actions)
	SDL_Event ev;
	while (SDL_PollEvent(&ev))
	{
		switch (ev.type)
		{
		case SDL_QUIT:       g_running = false; break;
		case SDL_MOUSEWHEEL: in.wheelDelta += ev.wheel.y; break;
		case SDL_KEYDOWN:
			if (!ev.key.repeat && ev.key.keysym.scancode == SDL_SCANCODE_C)
				in.toggleCameraMode = true;          // front montant
			break;
		}
	}

	// 2. Souris relative. SDL remet l'accumulateur à zéro tout seul :
	//    tu ne dois PAS le réinitialiser à la main.
	SDL_GetRelativeMouseState(&in.mouseDeltaX, &in.mouseDeltaY);

	// 3. Clavier : état MAINTENU, pas événement.
	const Uint8* k = SDL_GetKeyboardState(nullptr);
	in.moveForward = k[SDL_SCANCODE_W] || k[SDL_SCANCODE_UP];
	in.moveBackward = k[SDL_SCANCODE_S] || k[SDL_SCANCODE_DOWN];
	in.strafeLeft = k[SDL_SCANCODE_A] || k[SDL_SCANCODE_LEFT];
	in.strafeRight = k[SDL_SCANCODE_D] || k[SDL_SCANCODE_RIGHT];
	in.moveUp = k[SDL_SCANCODE_SPACE];
	in.moveDown = k[SDL_SCANCODE_LCTRL];
	in.sprint = k[SDL_SCANCODE_LSHIFT];

	if (k[SDL_SCANCODE_ESCAPE] || k[SDL_SCANCODE_SPACE])
		g_running = false;

	return in;
}



int main(int argc, char* argv[])
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
	Paramétrage projet
	************************************************************/


	// --- SETUP DE LA SCÈNE ---
	std::string cheminProjet = PROJECT_DIR; // path du projet définit dans l'Explorateur de projet > Propriétés.;
	// C/C++ > Préprocesseur.
	// Définitions de préprocesseur => PROJECT_DIR=R"($(ProjectDir))"
	// (Le R"(...)" est un Raw String Literal en C++, ça permet d'éviter que les antislashs \ de Windows ne fassent planter la chaîne de caractères).

	/************************************************************
	Paramétrage du scenegraph
	************************************************************/
	std::cout << "\n\033[32m=== Lecture de solar_system.json ===\033[0m" << std::endl;

	Registry registry;
	EventBus eventBus;
	HealthSystem healthSys(&registry, eventBus);
	AudioSystem audioSys(eventBus);
	ResourceManager rm;					// Collection de mesh unitaires
	Entity activeCamera = NULL_ENTITY;

	bool success = SceneSerializer::LoadSceneGraph(cheminProjet, "assets/solar_system.json", registry, activeCamera, rm);
	if (!success)
	{
		std::cerr << "Impossible de construire la scène. Arrêt du programme." << std::endl;
		return -1; 
	}
#if LV3_DEBUG
	CheckAnimationBaseline(registry);     // ← TEST A : dt = 0, rien ne bouge

	// --- VÉRIFICATION : AFFICHAGE DE L'ARBRE CONSTRUIT ---
	std::cout << "Structure finale du Scene Graph :" << std::endl;
	DebugDisplaySystem(registry);
	TestF1_EntityVersioning();
	TestF5_ResourceManager_UnloadMesh();

	RunAllCameraMathTests();
	TestProjection();
	TestMatrixLib();
	// Validation avant tout démarrage moteur
	if (!Test_TopLeftRule_NoDoubleCoverage())
	{
		printf("\033[31mECHEC : Test_TopLeftRule\033[0m\n");
		return -1;
	}
	printf("\033[32mOK : Test_TopLeftRule, pas de pixel dessiné 2 fois\033[0m\n");

#endif


	// --- BOUCLE DE JEU ---
	
	SDL_SetMainReady();       // on prend la responsabilité de l'initialisation
	screenWidth = 800;  // Largeur de l'écran
	screenHeight = 600; // Hauteur de l'écran
	if (SDLINIT(screenWidth, screenHeight) != true) return -1;

	// À l'initialisation, une seule fois : souris capturée, deltas illimités
	SDL_SetRelativeMouseMode(SDL_TRUE);


	pitch = 0;
	int frameCount = 0;
	const int maxFrames = 5; // Arrête la simulation après 100 images
	float deltaTime = 0.5f; // Temps fixe pour une simulation stable

	std::map < Entity, std::string> entityNames;	// pour le debugage





	//while (frameCount < maxFrames) {
	while (g_running == true)
	{

		// Nettoie la console (fonctionne sur Linux/macOS, pour Windows utiliser "cls")
		// system("clear"); 

		std::cout << std::endl;
		std::cout << "--- FRAME " << frameCount << " ---" << std::endl;

		Matrix44f mIndentity;

		// 1. Gérer les entrées utilisateur (non implémenté ici)
		PlayerInputSystem(registry, deltaTime);
		const LV3::InputState input = BuildInputState();


		// 2. Mettre à jour la scène
		// L'update commence à la racine, avec une matrice identité car elle n'a pas de parent.

		// --- 1. MISE À JOUR DE L'ÉTAT (Logique pure) ---
		AnimationSystem(registry, deltaTime);


		
		FPSControllerSystem(registry, input, deltaTime);      //  un seul agit,
		CameraFollowSystem(registry, deltaTime);             //  m_isEnabled arbitre




		// --- 2. MISE À JOUR DES MATRICES ---
		//TransformationSystem(registry, deltaTime);
		LocalTransformSystem(registry);       // Construit les matrices locales finales
		WorldTransformSystem(registry);       // Construit les matrices mondes finales

#if LV3_DEBUG
		CheckSceneInvariants(registry);       // ← INVARIANTS, chaque frame
		DebugTraceEntity(registry, "Earth");  // ← TRACE, à retirer une fois la question tranchée
#endif

		//const Entity camEntity = FindActiveCamera(registry);
		//const ViewData view = BuildViewData(*registry.TryGet<TransformComponent>(camEntity),
		//	*registry.TryGet<CameraComponent>(camEntity), viewport);




		// --- 3. DÉTECTION (Physique/Triggers) ---
		// Lit les matrices mondes finales
		TriggerSystem(registry, eventBus);

		//FindActiveCamera + BuildViewData
		//Culling + Rendu

		// --- 4. DESSIN ---
		// Débug de la hiérarchie 
		DebugDisplaySystem(registry);// , entityNames);

		// Draw de la hiérarchie
		RenderSystem(registry, activeCamera, rm);

		//Triangle2D huge{ {-500, -500}, {2000, 400}, {300, 1500}, 0.9f, 0.9f, 0.2f };

		Triangle2D tri1{
			{0,0}, {400,0}, {400,300}, // v0, v1, v2
			0.9f, 0.9f, 0.2f					// z0, z1, z2
		};

			Triangle2D tri2{
		{0,0}, {400,300}, {0,300}, // v0, v1, v2
		0.9f, 0.9f, 0.2f					// z0, z1, z2
			};

//		SDL_LockTexture(SDLtexture, nullptr, (void**)&ptrScreen, &pitch);
		if (SDL_LockTexture(SDLtexture, nullptr, (void**)&ptrScreen, &pitch) != 0)
		{
			SDL_Log("SDL_LockTexture a échoué : %s", SDL_GetError());
			return -1; // ou assert — mais surtout, ne continue PAS avec des valeurs invalides
		}


		FrameBuffer frameBuffer;
		frameBuffer.Bind(ptrScreen, pitch, screenWidth, screenHeight);

		Renderer renderer;
		renderer.DrawTriangle(tri1, frameBuffer, ERenderMode::Solid, Color{ 255, 0, 0, 255 });
		renderer.DrawTriangle(tri2, frameBuffer, ERenderMode::Solid, Color{ 0, 255, 0, 255 });

		SDL_UnlockTexture(SDLtexture);
		SDL_RenderCopy(SDLrenderer, SDLtexture, nullptr, nullptr);
		SDL_RenderPresent(SDLrenderer);


		// Pause pour rendre l'animation lisible dans la console
//		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		frameCount++;
	}


	SDLkill();
	return 0;
}
