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

// Gestion du scenegraph
#include "Scene/Registry.hpp"
#include "Core/EventBus.hpp"
//#include "Scene/SceneGraph.hpp"
#include "Scene/system.hpp"
#include "Scene/Serializer.hpp"
#include "Scene/renderSystem.h"
#include "Rendering/Renderer.h"
#include "Rendering/depthbuffer.h"
//#include "Scene/SpawnCameraGizmos.hpp"
#include "Scene/DebugGizmos.hpp"


#include "Test/RunAllTests.h"
#include "test/TestAffichageGizmoCamera.h"

#include "GFX/gfx.h"
#include "scene/CameraBinding.hpp"


using namespace LV3;
using namespace LV3::Tests;         // ← ajoute CE using en plus

//**********************************************

bool g_running = true;				// flag de la boucle. Si False, on quitte
DepthBuffer db;
FrameBuffer fb;
Viewport vpLeft, vpRight, vpTitle;

// Dimension de l'écran
int FrameW, FrameH;					// dimension de l'écran au cours d'une frame
bool resizePending = false;			// Inidique si la taille de l'écran évolue pendant le rendu de celui-ci
int pendingW,pendingH;				// Sauvegarde des dimensions de l'écran modifié lors du rendu. 
									// Elles seront adaptées après le rendu


//**********************************************
// État global de la boucle

bool g_mouseCaptured = true;
static bool g_cycleCam[2] = { false, false };   // [0]=gameplay, [1]=debug
static bool g_cycleMode[2] = { false, false };

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
				switch (ev.key.keysym.scancode)
				{
				case SDL_SCANCODE_F1:
					SetMouseCapture(!g_mouseCaptured);      // libère / recapture la souris
					break;
				case SDL_SCANCODE_F2: g_cycleCam[0] = true; break;   // caméra du panneau jeu
				case SDL_SCANCODE_F3: g_cycleMode[0] = true; break;   // mode du panneau jeu
				case SDL_SCANCODE_F4: g_cycleCam[1] = true; break;   // caméra du panneau debug
				case SDL_SCANCODE_F5: g_cycleMode[1] = true; break;   // mode du panneau debug
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
	ECameraCategory category;   // qui candidate ici — politique de l'appli
	Entity          camera;     // sélection COURANTE — état de session
	ERenderMode     mode;       // mode de rendu COURANT — état de session
};

Panel panels[2] =
{
	{ ECameraCategory::Gameplay, NULL_ENTITY, ERenderMode::Solid     },
	{ ECameraCategory::Debug,    NULL_ENTITY, ERenderMode::Wireframe },
};

static ERenderMode NextRenderMode(ERenderMode m)
{
	switch (m)
	{
	case ERenderMode::Solid:     return ERenderMode::Wireframe;
	case ERenderMode::Wireframe: return ERenderMode::Depth;
	default:                     return ERenderMode::Solid;
	}
}

//**********************************************

int main(int argc, char* argv[])
{

	SetConsoleMode();	// mode cosole en UTF-8

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

	// --- SETUP DE LA SCÈNE ---
	std::string cheminProjet = PROJECT_DIR; // path du projet définit dans l'Explorateur de projet > Propriétés.;
	// C/C++ > Préprocesseur.
	// Définitions de préprocesseur => PROJECT_DIR=R"($(ProjectDir))"
	// (Le R"(...)" est un Raw String Literal en C++, ça permet d'éviter que les antislashs \ de Windows ne fassent planter la chaîne de caractères).


	// --- Scenegraph et systèmes ---
	Registry registry;
	EventBus eventBus;
	HealthSystem healthSys(&registry, eventBus);
	AudioSystem audioSys(eventBus);
	ResourceManager rm;					// Collection de mesh unitaires
	Entity activeCamera = NULL_ENTITY;

	
	/************************************************************
	Paramétrage du scenegraph
	************************************************************/
	Logger::info(" === Lecture du scenegraph ===");

	if (cfg.mapAssets.find("scene_test") != cfg.mapAssets.end())
	{
		std::string pathScene = LV3::EngineConfig::Get().resources.pathGraphScene + cfg.mapAssets["scene_test"].object;
		bool success = SceneSerializer::LoadSceneGraph(cheminProjet, pathScene, registry, rm);// activeCamera, rm);
		if (!success)
		{
			Logger::error("Impossible de construire la scène. Arrêt du programme.\n");
			return -1;
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
		std::string gizmoPerspect = LV3::EngineConfig::Get().resources.pathMesh + cfg.mapAssets["gizmo_perspective"].object;
		std::string gizmoOrtho = LV3::EngineConfig::Get().resources.pathMesh + cfg.mapAssets["gizmo_ortho"].object;

		 GizAssets = LoadGizmoAssets(rm, gizmoPerspect, gizmoOrtho);
		if (GizAssets.IsValid())
			SpawnCameraGizmos(registry, GizAssets);
		else
			Logger::warn("[Gizmo] assets absents : aucun gizmo de camera ne sera affiché\n");

	#ifdef _DEBUG
		if (GizAssets.IsValid()) Test_GizmoCountMatchesCameras(registry);
	#endif

	}
	else
	{
		Logger::error("Impossible de retrouver les mesh des gizmo Camera. Arrêt du programme.\n");
		return -1;
	}


	// --- VÉRIFICATION : AFFICHAGE DE L'ARBRE CONSTRUIT ---
	// ── TESTS DE NON-RÉGRESSION — avant toute ressource système ──
#ifdef _DEBUG
	Logger::info("Structure finale du Scene Graph :");
	CheckAnimationBaseline(registry);     // ← TEST A : dt = 0, rien ne bouge

	Logger::info("[système] \n");
	DebugDisplaySystem(registry);
	Logger::info("[système] fin\n");

	if (!LV3::Tests::RunAllTests(registry)) return -1;
//	exit(0); // Arrêt du programme après les tests, avant la boucle de jeu
#endif

	
	// ═══ Initialisation, une seule fois ═══
	FrameW = cfg.screenWidth;  // Largeur de l'écran
	FrameH = cfg.screenHeight; // Hauteur de l'écran
	SDL_SetMainReady();       // on prend la responsabilité de l'initialisation
	if (SDLINIT(FrameW, FrameH) != true) return -1;

	db.Resize(FrameW, FrameH);	// depth buffer

	// Les deux régions. Découpage décidé ICI, par l'application.
	//if (cfg.mapViewports.find("title") != cfg.mapViewports.end() &&
	//	cfg.mapViewports.find("Right") != cfg.mapViewports.end() &&
	//	cfg.mapViewports.find("left") != cfg.mapViewports.end())
	//{
	///*	vpTitle.Resize(0, 0, cfg.mapViewports["title"].largeur, cfg.mapViewports["title"].hauteur);
	//	vpLeft.Resize(0, 0, cfg.mapViewports["left"].largeur, cfg.mapViewports["left"].hauteur);
	//	vpRight.Resize(cfg.mapViewports["left"].largeur, 0, cfg.mapViewports["Right"].largeur, cfg.mapViewports["Right"].hauteur);*/
	//}
	//else
	//{
	//	Logger::error("Impossible de créer les viewport. Arrêt du programme.\n");
	//	return -1;
	//}


	//const Entity camActive = FindCameraByName(registry, "Follow_Camera");
	//const Entity camOverview = FindCameraByName(registry, "Top_Camera");// Top_Camera");





	SetMouseCapture(true);

	pitch = 0;
	int frameCount = 0;
	const int maxFrames = 5; // Arrête la simulation après 100 images
	float deltaTime = 0.5f; // Temps fixe pour une simulation stable

	// system("clear");		// Nettoie la console (fonctionne sur Linux/macOS, pour Windows utiliser "cls")

	CameraBinding bindings[4];
	ViewData      views[4];
	Renderer renderer;

	// Rendu pour chaque viewport
	//const ViewSlot slots[] = 
	//{
	//	{ camActive,   ERenderMode::Solid     },
	//	{ camOverview, ERenderMode::Wireframe },
	//};

	while (g_running == true)
	{

		// --- Gérer les entrées utilisateur (non implémenté ici)
		PlayerInputSystem(registry, deltaTime);
		LV3::InputState input = BuildInputState();

		// --- Mettre à jour la scène
		CheckControllerExclusivity(registry);       // CHAQUE frame — invariant FPS/Follow

		// --- MISE À JOUR DE L'ÉTAT (Logique pure) ---
		AnimationSystem(registry, deltaTime);
		CameraFPSControllerSystem(registry, input, deltaTime);      //  un seul agit,
		CameraFollowSystem(registry, deltaTime);             //  m_isEnabled arbitre
		CameraZoomSystem(registry, input, deltaTime);




		// --- Élection par frame : la scène dit QUI et dans quel ORDRE ---
		//Entity cams[4];
		//const size_t nCams = CollectActiveCameras(registry, cams, std::size(cams));
		//if (nCams == 0)
		//{
		//	Logger::error("Aucune caméra active dans la scène — rien à rendre.");
		//	g_running = false;                 // arrêt propre, pas un assert dans les entrailles
		//	break;
		//}
		//const Entity activeCamera = cams[0];   // LA vérité — le gizmo surligne celle-ci

		//// --- L'EXE dit OÙ et COMMENT : modes par PANNEAU, pas par caméra ---
		//static constexpr ERenderMode paneModes[] = { ERenderMode::Solid, ERenderMode::Wireframe };

		//const size_t nSlots = std::min(nCams, std::size(paneModes));
		//if (nCams > nSlots)
		//	Logger::warn("[Layout] " + std::to_string(nCams - nSlots) + " caméra(s) active(s) sans panneau — ignorée(s)");

		//ViewSlot slots[4];
		//for (size_t i = 0; i < nSlots; ++i)
		//	slots[i] = { cams[i], paneModes[i] };


		//const ELayout layout = (nSlots == 1) ? ELayout::Single : ELayout::MainSide;



		//// --- L'association : viewport , camera et mode de rendu par rapport à une dimensionnée d'écran (redimensionable)												
		//const size_t nViews = BuildCameraBindings(layout,	// type de disopsition dew viewport
		//										slots,					// déclaration camera et type de rendu
		//										nSlots,		// taille du slot	
		//										FrameW, FrameH,			// taille de l'écran global
		//										bindings,				// paramètre de sortie, contient l/les viewport taillés
		//										std::size(bindings));	// nb de viewport

		// --- Sélection : consomme les touches, cicatrise les sélections mortes ---
		for (int p = 0; p < 2; ++p)
		{
			if (g_cycleCam[p]) { panels[p].camera = NextCamera(registry, panels[p].category, panels[p].camera); g_cycleCam[p] = false; }
			if (g_cycleMode[p]) { panels[p].mode = NextRenderMode(panels[p].mode); g_cycleMode[p] = false; }

			// Validation par frame : la sélection doit être vivante, active, de la bonne catégorie.
			const CameraComponent* cam = registry.TryGet<CameraComponent>(panels[p].camera);
			if (!cam || !cam->m_isActive || cam->m_category != panels[p].category)
				panels[p].camera = NextCamera(registry, panels[p].category, NULL_ENTITY);  // ré-élection
		}

		// --- Construction des slots : seuls les panneaux pourvus rendent ---
		ViewSlot slots[2];
		size_t nSlots = 0;
		for (int p = 0; p < 2; ++p)
			if (panels[p].camera != NULL_ENTITY)
				slots[nSlots++] = { panels[p].camera, panels[p].mode };

		if (nSlots == 0)
		{
			Logger::error("Aucune caméra active dans la scène — rien à rendre.");
			g_running = false;
			break;
		}

		const Entity activeCamera = (panels[0].camera != NULL_ENTITY)
			? panels[0].camera : slots[0].m_camera;   // le gizmo surligne la vue de JEU

		const ELayout layout = (nSlots == 1) ? ELayout::Single : ELayout::MainSide;
		const size_t nViews = BuildCameraBindings(layout, slots, nSlots, FrameW, FrameH, bindings, std::size(bindings));

		// --- Le gizmo ecrit m_local.scale AVANT la cuisson.
		CameraGizmoSystem(registry, activeCamera, bindings, nViews, GizAssets);

		// --- MISE À JOUR DES MATRICES ---
		//TransformationSystem(registry, deltaTime);
		LocalTransformSystem(registry);       // Construit les matrices locales finales
		WorldTransformSystem(registry);       // Construit les matrices mondes finales

		// --- DÉTECTION (Physique/Triggers) ---
		// Lit les matrices mondes finales
		TriggerSystem(registry, eventBus);

		// --- Les vues lisent les matrices de CETTE frame.
		for (size_t i = 0; i < nViews; ++i)
			views[i] = BuildViewData(registry, bindings[i]);

#if LV3_DEBUG
//		std::cout << std::endl;
//		std::cout << "--- FRAME " << frameCount << " ---" << std::endl;

		CheckSceneInvariants(registry);       // ← INVARIANTS, chaque frame
//		DebugTraceEntity(registry, "Cube1");  // ← TRACE, à retirer une fois la question tranchée

		// --- DESSIN ---
		// Débug de la hiérarchie 
		DebugDisplaySystem(registry);// , entityNames);

		// --- Draw de la hiérarchie ---
		RenderSystem(registry, activeCamera, rm);

#endif

		
#ifdef _DEBUG

		CheckControllerExclusivity(registry);
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
			fb.Clear(MakeColor(0, 0, 24));
			
			renderer.BeginFrame(fb, db);		// --- Plusieurs rendus dans le MÊME buffer ---
			renderer.SetDepthDisplayRange(80); // permet de gérer la profondeur dans le cas par exemple où on voudrait colorier la profondeur à la place des couleurs

			// --- recontruit les viewport et dessine les triangle
			for (size_t i = 0; i < nViews; ++i)
			{
				RenderView(registry, rm, renderer, views[i]);
				LV3_ASSERT(renderer.GetMode() == views[i].mode);   // personne n'a modifie l'etat en cours de route
			}
			
//#ifdef _DEBUG
//			ReportCullStats();
//#endif
			
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
			return -1; // ou assert — mais surtout, ne continue PAS avec des valeurs invalides
		}

		//SDL_RenderClear(SDLrenderer);
		SDL_RenderCopy(SDLrenderer, SDLtexture, nullptr, nullptr);
		SDL_RenderPresent(SDLrenderer);


		//SDL_RenderClear(SDLrenderer);
		Clean_Render(fb);
		
		//******************************************
		// resize si besoin après le lock sur la texture SDL, sinon le pitch est mauvais et on écrit hors bornes dans le framebuffer
		if (resizePending)
		{
			resizePending = false;

			if (pendingW > 0 && pendingH > 0)
			{
				FrameW = pendingW;
				FrameH = pendingH;

				// 1. La texture SDL (le pitch change aussi !)
				SDL_DestroyTexture(SDLtexture);
				SDLtexture = SDL_CreateTexture(SDLrenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, FrameW, FrameH);

				// 2. Le Z-buffer
				db.Resize(FrameW, FrameH);

				// 3. Le viewport : le ou les vioewports seront reconstruit durant la boucle de rendu

			}
		}

		// Pause pour rendre l'animation lisible dans la console
//		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		frameCount++;
	}


	SDLkill();
	return 0;
}
