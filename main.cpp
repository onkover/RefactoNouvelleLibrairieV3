/*
	(c) Onkover

	26/06/26
	Nouvelle gestion de librairie graphique v3
	S'appuie sur le librairie V2.1 entierement réécrite par Claude.ai mais difficilement exploitable en l'état	
	Leçons par Claude => cf. fichier *.md dans le répertoire "Documentation"
		1 : Lecon_01_Config_Enums.md
		2 : Lecon_02_Mathematiques.md
		3 : Lecon_03_ECS_SparseSet_ResourceManager.md
		4.1 : Lecon_04_Rasterizer_Partie1.md
		4.2 : todo
		5 : Lecon_05_Camera_Frustum.md

	todo
		* todo lecture de la texture dans le serializer parsingmesh
		* camera lissé : smoothSpeed, currentSmoothedPos
		* tester la caméra CameraFollowComponent
		* GetFaceView et son hypothèse de contiguïté
		* ComputeMeshAABB() jamais appelée automatiquement, 
		les trois enums de RenderTypes.h sans usage effectif, 
		* et les matériaux via submeshes n'ont utilisé.
		* le clipping near en espace de clip, 
		* l'interpolation perspective-correcte (z linéaire mais UV et couleurs en 1/w), 
		* l'exploitation du troisième état du culling,
		* cone culling par cluster,
*/


#define MAIN

#include "pch.h"          // ← première ligne, toujours
#include "main.h"

#include <SDL_ttf.h>
#include <thread>

#include "Core/engineconfig.h"
#include "Core/Platform.h"
#include "Core/Logger.h"
#include "Core/InputState.h"
#include "helper/ConfigManager.h"
#include "Core/SimulationClock.h"
#include "Core/contentroot.h"

#include "Scene/Registry.hpp"
#include "Core/EventBus.hpp"
#include "Scene/system.hpp"
#include "Scene/Serializer.hpp"
#include "Scene/renderSystem.h"
#include "Rendering/Renderer.h"
#include "Rendering/depthbuffer.h"
#include "Scene/DebugGizmos.hpp"
#include "GFX/gfx.h"
#include "scene/CameraBinding.hpp"
#include "scene/SerializerHelpers.hpp"

#include "Test/RunAllTests.h"
#include "test/TestAffichageGizmoCamera.h"


using namespace LV3;
using namespace LV3::Tests;         // ← ajoute CE using en plus

//**********************************************
bool g_running = true;				// flag de la boucle. Si False, on quitte
DepthBuffer db;
FrameBuffer fb;
Viewport vpLeft, vpRight, vpTitle;
LV3::SimulationClock _clock;

//**********************************************
// 
// Dimension de l'écran
int FrameW, FrameH;					// dimension de l'écran au cours d'une frame
bool resizePending = false;			// Inidique si la taille de l'écran évolue pendant le rendu de celui-ci
int pendingW,pendingH;				// Sauvegarde des dimensions de l'écran modifié lors du rendu. 
									// Elles seront adaptées après le rendu
//**********************************************
// État global de la boucle

bool g_mouseCaptured = true;
static bool g_cycleCam[kMaxCamerasHard] = {};   // [0]=gameplay, [1]=debug
static bool g_cycleMode[kMaxCamerasHard] = {};

static void SetMouseCapture(bool captured)
{
	g_mouseCaptured = captured;
	// Cache le curseur et le confine à la fenêtre — c'est ce qui permet un déplacement souris infini sans buter sur les bords de l'écran
	SDL_SetRelativeMouseMode(captured ? SDL_TRUE : SDL_FALSE);
}


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
		case SDL_WINDOWEVENT:
			if (ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
			{
				// on NOTE, on n'agit pas tout de suite car la SDLtexture pourrait déjà êtré lockée 
				pendingW = ev.window.data1;
				pendingH = ev.window.data2;
				resizePending = true;         
			}
			break;
		case SDL_QUIT:       g_running = false; break;
		case SDL_MOUSEWHEEL: in.wheelDelta += ev.wheel.y; break;
		case SDL_KEYDOWN:
			if (!ev.key.repeat)
			{
				const bool shiftHeld = (ev.key.keysym.mod & KMOD_SHIFT) != 0;

				// Un scancode désigne une position physique
				// un keycode désigne le caractère produit, donc il dépend de la disposition
				switch (ev.key.keysym.scancode)
				{
				case SDL_SCANCODE_F1:
					SetMouseCapture(!g_mouseCaptured);      // libère / recapture la souris
					break;

				case SDL_SCANCODE_F2: g_cycleCam[0] = true; break;   // caméra du slot RENDU
				case SDL_SCANCODE_F3: g_cycleMode[0] = true; break;
				case SDL_SCANCODE_F4: g_cycleCam[1] = true; break;   // caméra du slot DEBUG #1
				case SDL_SCANCODE_F5: g_cycleMode[1] = true; break;
				case SDL_SCANCODE_F6: g_cycleCam[2] = true; break;   // caméra du slot DEBUG #2
				case SDL_SCANCODE_F7: g_cycleMode[2] = true; break;
				case SDL_SCANCODE_F8: g_cycleCam[3] = true; break;   // caméra du slot DEBUG #3
				case SDL_SCANCODE_F9: g_cycleMode[3] = true; break;

				case SDL_SCANCODE_RIGHTBRACKET: 
					_clock.Scale(true, shiftHeld); break;   // ^
				case SDL_SCANCODE_LEFTBRACKET:  
					_clock.Scale(false, shiftHeld); break;   // $
				case SDL_SCANCODE_P:            
					_clock.m_paused = !_clock.m_paused; break;
				case SDL_SCANCODE_0: 
					_clock.Reset(); break;
				case SDL_SCANCODE_C:
					in.toggleCameraMode = true;          // front montant
					break;
				case SDL_SCANCODE_ESCAPE:
					g_running = false;	break;

				default:	break;
				}


			}
		
		}
	}

	// 2. Souris relative. SDL remet l'accumulateur à zéro tout seul :
	//    ne PAS le réinitialiser à la main.
	if (g_mouseCaptured)
		SDL_GetRelativeMouseState(&in.mouseDeltaX, &in.mouseDeltaY);
	else
		in.mouseDeltaX = in.mouseDeltaY = 0;


	// 3. Clavier : état MAINTENU, pas événement.
	const Uint8* k = SDL_GetKeyboardState(nullptr);
	in.moveForward = k[SDL_SCANCODE_W] || k[SDL_SCANCODE_UP];
	in.moveBackward = k[SDL_SCANCODE_S] || k[SDL_SCANCODE_DOWN];
	in.strafeLeft = k[SDL_SCANCODE_A] || k[SDL_SCANCODE_LEFT];
	in.strafeRight = k[SDL_SCANCODE_D] || k[SDL_SCANCODE_RIGHT];
	in.moveUp = k[SDL_SCANCODE_SPACE];
	in.moveDown = k[SDL_SCANCODE_LCTRL];
	in.sprint = k[SDL_SCANCODE_LSHIFT];

	return in;
}


struct Panel
{
//	ECameraCategory category;   // qui candidate ici — politique de l'appli  
	Entity      camera = NULL_ENTITY;		// sélection COURANTE — état de session
	ERenderMode mode = ERenderMode::Solid;	// mode de rendu COURANT — état de session
};

Panel panels[kMaxCamerasHard];

//Panel panels[2] =
//{
//	{ ECameraCategory::Gameplay, NULL_ENTITY, ERenderMode::Solid     },
//	{ ECameraCategory::Debug,    NULL_ENTITY, ERenderMode::Wireframe },
//};


static ERenderMode NextRenderMode(ERenderMode m)
{
	switch (m)
	{
	case ERenderMode::Solid:     return ERenderMode::Wireframe;
	case ERenderMode::Wireframe: return ERenderMode::Depth;
	default:                     return ERenderMode::Solid;
	}
}

/***********************************************
Helpers pour BuildInputState() :
* BuildLayout ne sait produire que 1, 2 ou 4 viewports.
* Toute valeur demandée est ramenée à la plus proche taille supportée
* INFÉRIEURE OU ÉGALE — jamais supérieure, pour ne jamais dépasser maxViewport.
*/

static size_t SnapToSupportedViewportCount(size_t requested)
{
	if (requested >= 4) return 4;
	if (requested >= 2) return 2;
	return 1;
}

// Les slots debug (indices 1..nDebugSlots) partagent le même bassin de
// caméras : deux slots debug ne doivent jamais pointer sur la même caméra
// (BuildCameraBindings l'interdit en debug via LV3_ASSERT — mieux vaut ne
// jamais y arriver que le découvrir au premier crash).
static bool IsCameraUsedByOtherDebugSlot(const Panel* _panels, size_t nDebugSlots, size_t exceptIndex, Entity cam)
{
	for (size_t i = 1; i <= nDebugSlots; ++i)
		if (i != exceptIndex && _panels[i].camera == cam) return true;
	return false;
}

//**********************************************

int main(int argc, char* argv[])
{

	SetConsoleMode();	// mode cosole en UTF-8


	///************************************************************
	// Lecture du répertoire de l'executable
	//************************************************************/
	// LV3_PROJECT_DIR est defini par le projet EXECUTABLE, en Debug uniquement.
	// La LIB ne l'a jamais vu et n'a pas a le voir : c'est un chemin de developpement, donc une donnee de l'APPLICATION.
	std::vector<std::filesystem::path> devCandidates;

	// Racine passee en argument : priorite sur tous les replis.
	// C'est ce qui permettra de mesurer les trois scenes avec UN SEUL binaire.
	if (argc > 1)
		devCandidates.emplace_back(argv[1]);

	#ifdef LV3_PROJECT_DIR
		devCandidates.emplace_back(LV3_PROJECT_DIR);	// path du projet définit dans l'Explorateur de projet > Propriétés.;
		// C/C++ > Préprocesseur.
		// Définitions de préprocesseur => PROJECT_DIR=R"($(ProjectDir))"
		// (Le R"(...)" est un Raw String Literal en C++, ça permet d'éviter que les antislashs \ de Windows ne fassent planter la chaîne de caractères).
	#endif

	const std::filesystem::path contentRoot = LV3::ResolveContentRoot(devCandidates);
	if (contentRoot.empty())
	{
		Logger::error("Arret : aucune racine de contenu.");
		return -1;
	}
	LV3_ASSERT(contentRoot.is_absolute());	// test si le chemin est absolu, par exmeple : c:\chemin\

	///************************************************************
	//Lecture du nom des répertoires depuis la base de registres
	//************************************************************/
	Logger::info(" === Lecture de la configuration du programme ===");
	config cfg;
	if (!ProgrammeConfig("config.json", cfg))
	{
		Logger::error("Impossible de charger la configuration.\n");
		return 1;
	}
	Logger::success("Configuration du programme chargée avec succès\n.");


	/************************************************************
	Paramétrage du moteur de rendu
	************************************************************/
	Logger::info(" === Lecture de la configuration du moteur ===");

	if (!LV3::EngineConfig::Get().LoadFromJson("engine.json"))		
		Logger::warn("EngineConfig — defauts LV3_DEFAULT_* utilises (fichier absent ou invalide)\n");
	else
		Logger::success("Configration du moteur chargée avec succès\n");

	/************************************************************
	Paramétrage projet
	************************************************************/



	/************************************************************
	Paramétrage du scenegraph
	************************************************************/
	Logger::info(" === Lecture et paramétrage du scenegraph ===\n");

	// --- Scenegraph et systèmes ---
	Registry registry;
	EventBus eventBus;
	HealthSystem healthSys(&registry, eventBus);
	AudioSystem audioSys(eventBus);
	ResourceManager rm;					// Collection de mesh unitaires
	CameraBinding bindings[kMaxCamerasHard];
	ViewData      views[kMaxCamerasHard];
	Renderer renderer;


	// --- Lecture de la scène ---
	if (cfg.mapAssets.find("scene_graph") != cfg.mapAssets.end())
	{
		std::string pathScene = LV3::EngineConfig::Get().resources.pathGraphScene + cfg.mapAssets["scene_graph"].object;
		bool success = SceneSerializer::LoadSceneGraph(contentRoot.string(), pathScene, registry, rm);
		if (!success)
		{
			Logger::error("Impossible de construire la scène. Arrêt du programme.\n");
			return -1;
		}

		Logger::info("[Diag] Scene chargee : " + pathScene);
		for (auto&& [e, cam] : registry.ViewGroup<CameraComponent>())
		{
			Logger::info("[Diag] Camera '" + EntityLabel(registry, e) + "' active=" +
				(cam.m_isActive ? std::string("true") : std::string("false")) +
				" categorie=" + std::to_string(static_cast<int>(cam.m_category)));
		}

	}
	else
	{
		Logger::error("Impossible de retrouver le scene graph. Arrêt du programme.\n");
		return -1;
	}



	/************************************************************
	Paramétrage des gizmo des camera
	************************************************************/
	GizmoAssets GizAssets;
	if (cfg.mapAssets.find("gizmo_perspective") != cfg.mapAssets.end() && cfg.mapAssets.find("gizmo_ortho") != cfg.mapAssets.end())
	{
		std::string gizmoPerspect = contentRoot.string() + LV3::EngineConfig::Get().resources.pathMesh + cfg.mapAssets["gizmo_perspective"].object;
		std::string gizmoOrtho = contentRoot.string() + LV3::EngineConfig::Get().resources.pathMesh + cfg.mapAssets["gizmo_ortho"].object;

		 GizAssets = LoadGizmoAssets(rm, gizmoPerspect, gizmoOrtho);
		if (GizAssets.IsValid())
			SpawnCameraGizmos(registry, GizAssets);
		else
			Logger::warn("[Gizmo] assets absents : aucun gizmo de camera ne sera affiché\n");

	#if LV3_DEBUG
		//if (GizAssets.IsValid()) Test_GizmoCountMatchesCameras(registry);
		if (GizAssets.IsValid()) Test_GizmoCountMatchesCameras(registry, GizAssets);
	#endif

	}
	else
	{
		Logger::error("Impossible de retrouver les mesh des gizmo Camera. Arrêt du programme.\n");
		return -1;
	}

	/************************************************************
	VÉRIFICATION : AFFICHAGE DE L'ARBRE CONSTRUIT
	TESTS DE NON-RÉGRESSION — avant toute ressource système
	************************************************************/
	#if LV3_DEBUG
		Logger::info("Structure finale du Scene Graph :");
		CheckAnimationBaseline(registry);     // ← TEST A : dt = 0, rien ne bouge

		Logger::info("[système] Systeme avec matrice vide\n");
		DebugDisplaySystem(registry);
		Logger::info("[système] fin\n");

		if (!LV3::Tests::RunAllTests(registry)) return -1;


//		exit(0); // Arrêt du programme après les tests, avant la boucle de jeu
	#endif

	/************************************************************
	Initialisation, une seule fois
	************************************************************/
	
	FrameW = cfg.screenWidth;  // Largeur de l'écran
	FrameH = cfg.screenHeight; // Hauteur de l'écran
	SDL_SetMainReady();       // on prend la responsabilité de l'initialisation
	if (SDLINIT(FrameW, FrameH) != true) return -1;

	db.Resize(FrameW, FrameH);	// depth buffer

	SetMouseCapture(true);

	// system("clear");		// Nettoie la console (fonctionne sur Linux/macOS, pour Windows utiliser "cls")


	/************************************************************
	Initialisation de l'horloge de simulation
	************************************************************/
	Uint64 prevCounter = SDL_GetPerformanceCounter();
	const double counterFreq = static_cast<double>(SDL_GetPerformanceFrequency());


	// --- Paramétrage de l'horloge de simulation ---
	// Reglage : lu une fois depuis engine.json, constant ensuite.
	_clock.Configure(LV3::EngineConfig::Get().simulation);

	// récupération de la configuration (depuis engine.json) de l'état Frame (change à chaque frame)
	_clock.m_timeScale = LV3::EngineConfig::Get().simulation.m_timeScale;
	_clock.m_simTime = LV3::EngineConfig::Get().simulation.m_simTime;

	/************************************************************
	Boucle du jeu
	************************************************************/
	pitch = 0;

#if LV3_DEBUG
	int frameCount = 0;
#endif
	Logger::info("=== Boucle de jeu ===\n");
	while (g_running == true)
	{
		// --- Mesure du temps réel écoulé
		const Uint64 nowCounter = SDL_GetPerformanceCounter();
		float realDt = static_cast<float>((nowCounter - prevCounter) / counterFreq);
		prevCounter = nowCounter;

		// --- Clamp AVANT toute consommation (bug 44)
		realDt = std::min(realDt, 0.1f);

		// --- Le temps du monde dérive du temps réel, jamais l'inverse
		const float simDt = _clock.Advance(realDt);


		// --- Gérer les entrées utilisateur
		LV3::InputState input = BuildInputState();		// Ordre canonique : construire l'InputState de la frame AVANT tout système qui le consomme.
		PlayerInputSystem(registry, input, realDt);

		// --- Mettre à jour la scène
		CheckControllerExclusivity(registry);       // CHAQUE frame — invariant FPS/Follow

		// --- MISE À JOUR DE L'ÉTAT (Logique pure) ---
		AnimationSystem(registry, simDt);

		// --- MISE À JOUR DES MATRICES ---
		// CameraFollowSystem lit tr.m_worldMatrix de sa cible (le vaisseau) pour se positionner.
		// Sans cette cuisson intermédiaire, il lirait la matrice monde de la frame PRÉCÉDENTE —
		// une frame de retard entre « PlayerInputSystem vient de déplacer le vaisseau » et
		// « la caméra qui le suit en tient compte ». Imperceptible avec du lissage actif, mais faux.
		LocalTransformSystem(registry);       // Construit les matrices locales finales
		WorldTransformSystem(registry);       // Construit les matrices mondes finales

		// --- MISE À JOUR DES SYSTÈMES DE CAMÉRA ---
		CameraFPSControllerSystem(registry, input, realDt);      //  un seul agit,
		CameraFollowSystem(registry, realDt);             //  m_isEnabled arbitre
		CameraZoomSystem(registry, input);



		// --- Sélection : consomme les touches, cicatrise les sélections mortes ---
		//for (int p = 0; p < 2; ++p)
		//{
		//	if (g_cycleCam[p]) { panels[p].camera = NextCamera(registry, panels[p].category, panels[p].camera); g_cycleCam[p] = false; }
		//	if (g_cycleMode[p]) { panels[p].mode = NextRenderMode(panels[p].mode); g_cycleMode[p] = false; }

		//	// Validation par frame : la sélection doit être vivante, active, de la bonne catégorie.
		//	const CameraComponent* cam = registry.TryGet<CameraComponent>(panels[p].camera);
		//	if (!cam || !cam->m_isActive || cam->m_category != panels[p].category)
		//		panels[p].camera = NextCamera(registry, panels[p].category, NULL_ENTITY);  // ré-élection
		//}

	


		//// --- Construction des slots ------------------------------------------------
		//// Bloc de test Quad (bug 61) — corrigé : Gameplay peut légitimement contenir
		//// plusieurs caméras actives à la fois (FPS_Camera ET Follow_Camera, pour que
		//// F2 puisse cycler de l'une à l'autre). Ne JAMAIS aspirer tout le bassin
		//// Gameplay dans les slots : seule celle couramment sélectionnée par le panel
		//// (panels[0].camera) doit en occuper un — les slots restants viennent de
		//// Debug, qui n'a pas cette sémantique de bassin ici.
		//ViewSlot slots[kMaxCamerasHard];
		//size_t nSlots = 0;

		//{
		//	Entity testCams[kMaxCamerasHard];
		//	size_t n = 0;
		//	if (panels[0].camera != NULL_ENTITY)
		//		testCams[n++] = panels[0].camera;
		//	n += CollectActiveCameras(registry, ECameraCategory::Debug, testCams + n, kMaxCamerasHard - n);

		//	if (n >= kMaxCamerasHard)
		//		for (size_t i = 0; i < kMaxCamerasHard; ++i)
		//			slots[nSlots++] = { testCams[i], ERenderMode::Solid };
		//}

		//if (nSlots == 0)
		//{
		//	for (int p = 0; p < 2; ++p)
		//		if (panels[p].camera != NULL_ENTITY)
		//			slots[nSlots++] = { panels[p].camera, panels[p].mode };
		//}

		//const Entity activeCamera = (panels[0].camera != NULL_ENTITY)
		//	? panels[0].camera : slots[0].m_camera;   // le gizmo surligne la vue de JEU

		//const ELayout layout =
		//	(nSlots == 1) ? ELayout::Single :
		//	(nSlots == 4) ? ELayout::Quad :
		//	ELayout::MainSide;

		//const size_t nViews = BuildCameraBindings(layout, slots, nSlots, FrameW, FrameH, bindings);
		//if (nViews == 0)
		//{
		//	Logger::error("Impossible de construire les bindings de caméra.");
		//	g_running = false;
		//	break;
		//}

		// --- Combien de viewports, et pour qui ? (bug 61, généralisé) --------------
		Entity gamingBuf[LV3_MAX_CAMERA];
		const size_t nGamingActive = CollectActiveCameras(registry, ECameraCategory::Gameplay, gamingBuf, std::size(gamingBuf));
		if (nGamingActive == 0)
		{
			Logger::error("Aucune caméra de rendu (Gameplay) active — arrêt du programme.");
			g_running = false;
			break;
		}

		Entity debugBuf[LV3_MAX_CAMERA];
		const size_t nDebugActive = CollectActiveCameras(registry, ECameraCategory::Debug, debugBuf, std::size(debugBuf));

		const size_t requestedV = std::min<size_t>(
			static_cast<size_t>(LV3::EngineConfig::Get().viewport.maxViewport), kMaxCamerasHard);
		const size_t V = SnapToSupportedViewportCount(requestedV);

		size_t nDebugSlots = std::min(V - 1, nDebugActive);
		size_t total = 1 + nDebugSlots;
		if (total == 3) { --nDebugSlots; total = 2; }   // 3 non supporté par BuildLayout : on sacrifie un slot debug

		// --- Avertissement de mode dégradé — une seule fois par changement d'état --
		static size_t s_lastWarnedTotal = SIZE_MAX;
		const size_t bestPossible = 1 + std::min(nDebugActive, kMaxCamerasHard - 1);
		const size_t wanted = std::min(requestedV, bestPossible);
		if (total < wanted)
		{
			if (s_lastWarnedTotal != total)
			{
				Logger::warn("[Viewport] mode degrade : " + std::to_string(total) + " viewport(s) affiche(s) au lieu de "
					+ std::to_string(wanted) + " demande(s)/possible(s) (" + std::to_string(nGamingActive)
					+ " camera(s) rendu, " + std::to_string(nDebugActive) + " camera(s) debug actives).");
				s_lastWarnedTotal = total;
			}
		}
		else
		{
			s_lastWarnedTotal = SIZE_MAX;   // état normal : réarme l'avertissement pour la prochaine dégradation
		}

		// --- Sélection : slot 0 (rendu), bassin dédié, jamais de collision ---------
		if (g_cycleCam[0]) { panels[0].camera = NextCamera(registry, ECameraCategory::Gameplay, panels[0].camera); g_cycleCam[0] = false; }
		if (g_cycleMode[0]) { panels[0].mode = NextRenderMode(panels[0].mode); g_cycleMode[0] = false; }
		{
			const CameraComponent* cam = registry.TryGet<CameraComponent>(panels[0].camera);
			if (!cam || !cam->m_isActive || cam->m_category != ECameraCategory::Gameplay)
				panels[0].camera = NextCamera(registry, ECameraCategory::Gameplay, NULL_ENTITY);
		}

		// --- Sélection : slots 1..nDebugSlots, bassin partagé, collision à éviter --
		for (size_t p = 1; p <= nDebugSlots; ++p)
		{
			if (g_cycleCam[p])
			{
				Entity next = NextCamera(registry, ECameraCategory::Debug, panels[p].camera);
				const Entity start = next;
				while (next != NULL_ENTITY && IsCameraUsedByOtherDebugSlot(panels, nDebugSlots, p, next))
				{
					next = NextCamera(registry, ECameraCategory::Debug, next);
					if (next == start) { next = NULL_ENTITY; break; }   // toutes deja prises ailleurs
				}

				//Logger::info("[Diag] slot" + std::to_string(p) + " : "
				//	+ EntityLabel(registry, panels[p].camera) + " -> " + EntityLabel(registry, next));

				panels[p].camera = next;
				g_cycleCam[p] = false;
			}
			if (g_cycleMode[p]) { panels[p].mode = NextRenderMode(panels[p].mode); g_cycleMode[p] = false; }

			const CameraComponent* cam = registry.TryGet<CameraComponent>(panels[p].camera);
			if (!cam || !cam->m_isActive || cam->m_category != ECameraCategory::Debug
				|| IsCameraUsedByOtherDebugSlot(panels, nDebugSlots, p, panels[p].camera))
			{
				Entity elect = NextCamera(registry, ECameraCategory::Debug, NULL_ENTITY);
				const Entity start = elect;
				while (elect != NULL_ENTITY && IsCameraUsedByOtherDebugSlot(panels, nDebugSlots, p, elect))
				{
					elect = NextCamera(registry, ECameraCategory::Debug, elect);
					if (elect == start) { elect = NULL_ENTITY; break; }
				}
				panels[p].camera = elect;
			}
		}

		// --- Construction des slots -------------------------------------------------
		ViewSlot slots[kMaxCamerasHard];
		size_t nSlots = 0;
		slots[nSlots++] = { panels[0].camera, panels[0].mode };
		for (size_t p = 1; p <= nDebugSlots; ++p)
			slots[nSlots++] = { panels[p].camera, panels[p].mode };

		const Entity activeCamera = panels[0].camera;   // le gizmo surligne toujours la vue de jeu

		const ELayout layout = (total == 1) ? ELayout::Single : (total == 4) ? ELayout::Quad : ELayout::MainSide;
		const size_t nViews = BuildCameraBindings(layout, slots, nSlots, FrameW, FrameH, bindings);
		if (nViews == 0)
		{
			Logger::error("Impossible de construire les bindings de caméra.");
			g_running = false;
			break;
		}


		// --- Le gizmo ecrit m_local.scale AVANT la cuisson.
		CameraGizmoSystem(registry, activeCamera, bindings, nViews, GizAssets);

		// --- matrices des CAMÉRAS (et de leurs gizmos) SEULEMENT ---
		// * LocalTransformSystem ne retraite que ce qui est resté dirty depuis la cuisson n°1 (les caméras, leurs gizmos) — quasi gratuit grâce au drapeau. 
		// * Pour WorldTransformSystem, on n'appelle PAS la version complète : sur une scène à plusieurs centaines d'objets,
		// retraverser tout pour ~2 caméras effectivement changées serait pur gaspillage. 
		// La surcharge ciblée ne repropage que les caméras rendues cette frame (et leurs gizmos, via la hiérarchie) — coût O(nViews), pas O(N).
		LocalTransformSystem(registry);       // Construit les matrices locales finales
		Entity renderedCameras[kMaxCamerasHard];
		for (size_t i = 0; i < nViews; ++i)
			renderedCameras[i] = bindings[i].m_camera;
		WorldTransformSystem(registry, std::span<const Entity>(renderedCameras, nViews));   // Construit les matrices mondes — caméras seulement


		// --- DÉTECTION (Physique/Triggers) ---
		// Lit les matrices mondes finales
		TriggerSystem(registry, eventBus);

		// --- Les vues lisent les matrices de CETTE frame.
		for (size_t i = 0; i < nViews; ++i)
			views[i] = BuildViewData(registry, bindings[i]);

#if LV3_DEBUG
	#if LV3_ASSERTS_ENABLED
		CheckSceneInvariants(registry);       // ← INVARIANTS, chaque frame
	#endif

		// --- DESSIN ---
		// Débug de la hiérarchie 
		//	DebugDisplaySystem(registry);// , entityNames);

	#if LV3_DUMP_HIERARCHY
		// --- Draw de la hiérarchie ---
		std::cout << std::endl;
		std::cout << "--- FRAME " << frameCount << " ---" << std::endl;
		DrawHierarchySystem(registry, rm);
	#endif
		Test_CameraWorldMatrixIsRigid(registry);

		const size_t nGizChecked = Test_GizmoMatchesFrustum(registry, rm, views, nViews, GizAssets);
		// Garde de vacuité DÉPLACÉ, pas supprimé : si les assets sont valides et
		// qu'au moins une caméra a déclaré un gizmo, alors 0 vérification = câblage cassé.
		if (GizAssets.IsValid())
		{
			size_t declared = 0;
			for (auto&& [e, cam] : registry.ViewGroup<CameraComponent>())
				if (cam.m_gizmoLength > 0.0f) ++declared;
			LV3_ASSERT(declared == 0 || nGizChecked > 0);
		}		
#endif

		if (SDL_LockTexture(SDLtexture, nullptr, (void**)&ptrScreen, &pitch) == 0)
		{
			fb.Bind(ptrScreen, pitch,FrameW, FrameH);
			Clean_Render(fb);
			
			renderer.BeginFrame(fb, db);			// --- Plusieurs rendus dans le MÊME buffer ---
			renderer.SetDepthDisplayRange(LV3::EngineConfig::Get().debug.depthDisplayRange); // permet de gérer la profondeur dans le cas par exemple où on voudrait colorier la profondeur à la place des couleurs. Définit dans engine.json

			// --- recontruit les viewport et dessine les triangle
			for (size_t i = 0; i < nViews; ++i)
			{
				RenderView(registry, rm, renderer, views[i]);
				LV3_ASSERT(renderer.GetMode() == views[i].mode);   // personne n'a modifie l'etat en cours de route
			}
			
#if LV3_DEBUG
	#if LV3_VERBOSE_LOG
			ReportCullStats();
	#endif
#endif
			
			// Séparateur vertical entre les différents viewports
			for (int y = 0; y < cfg.mapViewports["left"].hauteur; ++y) 
				fb.SetPixel(cfg.mapViewports["left"].largeur, y, MakeColor(90, 90, 110));
			
			for (int x = 0; x < cfg.mapViewports["title"].largeur; ++x) 
				fb.SetPixel(x, cfg.mapViewports["left"].hauteur, MakeColor(90, 90, 110));


			// Fin du rendu
			renderer.EndFrame();                          // ← le pointeur cesse d'exister
			fb.Unbind();                                  // ← idem
			SDL_UnlockTexture(SDLtexture);

		}
		else
		{
			SDL_Log("SDL_LockTexture a échoué : %s", SDL_GetError());
			SDLkill();   // Bug 27 : SDL était initialisé (fenêtre, renderer, texture) — une sortie ici les laissait fuiter.
			return -1; // ou assert — mais surtout, ne continue PAS avec des valeurs invalides
		}

		//SDL_RenderClear(SDLrenderer);
		SDL_RenderCopy(SDLrenderer, SDLtexture, nullptr, nullptr);
		SDL_RenderPresent(SDLrenderer);


		//SDL_RenderClear(SDLrenderer);

		
		//******************************************
		// resize si besoin après le lock sur la texture SDL, sinon le pitch est mauvais et on écrit hors bornes dans le framebuffer
		if (resizePending)
		{
			resizePending = false;

			if (pendingW > 0 && pendingH > 0)
			{
				FrameW = pendingW;
				FrameH = pendingH;

				// 1. La texture SDL chnage (le pitch change aussi !)
				SDL_DestroyTexture(SDLtexture);
				SDLtexture = SDL_CreateTexture(SDLrenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, FrameW, FrameH);
				if (SDLtexture == nullptr)
				{
					// Bug 27 : un échec ici laissait SDLtexture nul jusqu'à la frame suivante,
					// où SDL_LockTexture(nullptr, ...) aurait échoué loin du site réel de la faute
					// (une resize) — diagnostic à l'aveugle. On échoue ICI, avec le bon message.
					Logger::error(std::string("SDL_CreateTexture (resize) a échoué : ") + SDL_GetError());
					SDLkill();
					return -1;
				}
				// 2. Le Z-buffer
				db.Resize(FrameW, FrameH);

				// 3. Le viewport : 
				// rien à faire : le ou les viewports seront reconstruit durant la boucle de rendu
			}
		}
		#if LV3_DEBUG
			frameCount++;
		#endif
	}
	Logger::info("=== Fin de la boucle de jeu ===\n\n");



	SDLkill();
	return 0;
}
