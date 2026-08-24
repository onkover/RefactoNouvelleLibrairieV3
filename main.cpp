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
#include "Scene/renderSystem.h"
#include "Rendering/Renderer.h"
#include "Rendering/depthbuffer.h"
#include "Test/RunAllTests.h" 

#include "GFX/gfx.h"





using namespace LV3;
using namespace LV3::Tests;         // ← ajoute CE using en plus

bool g_running = true;
DepthBuffer db;
FrameBuffer fb;
Viewport vpLeft;
Viewport vpRight;
int WinW, WinH;

int pendingW,pendingH;
bool resizePending = false;
//**********************************************

// État global de la boucle

bool g_mouseCaptured = true;

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
				// on NOTE, on n'agit pas tout de suite car la SDLtexture pourrait dajà étré lockée 
				pendingW = ev.window.data1;
				pendingH = ev.window.data2;
				resizePending = true;         


				//const int w = ev.window.data1, h = ev.window.data2;
				//if (w <= 20 || h <= 20) break;              // fenêtre minimisée

				//WinW = w;
				//WinH = h;
				//// 1. La texture SDL (le pitch change aussi !)
				//SDL_DestroyTexture(SDLtexture);
				//SDLtexture = SDL_CreateTexture(SDLrenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, WinW, WinH);

				//// 2. Le Z-buffer
				//db.Resize(WinW, WinH);

				//// 3. Le viewport
				//vpLeft.Resize(0, 0, WinW / 2, WinH);
				//vpRight.Resize(WinW / 2, 0, WinW / 2, WinH);

				//// 4. L'aspect ratio est dérivé du viewport dans BuildViewData :
				////    rien à faire, il suivra tout seul.

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

//**********************************************

int main(int argc, char* argv[])
{

	SetConsoleMode();	// mode cosole en UTF-8

	///************************************************************
	//Lecture du nom des répertoires depuis la base de registres
	//************************************************************/

	config cfg;

	if (!ProgrammeConfig("config.json", cfg))
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


	// --- Scenegraph et systèmes ---
	Registry registry;
	EventBus eventBus;
	HealthSystem healthSys(&registry, eventBus);
	AudioSystem audioSys(eventBus);
	ResourceManager rm;					// Collection de mesh unitaires
	Entity activeCamera = NULL_ENTITY;

	//OBJLoadOptions opts;
	//opts.flipUVsVertically = false;
	//opts.generateNormalsIfMissing = true;

	//const std::string name = "assets/Meshes/camera_gizmo.obj";
	//auto h = rm.LoadMeshChecked(name, {});
	//if (!h.has_value())
	//{
	//	int a = 0;
	//}
	//const MeshHandle hMesh = *h;

	//const MeshClass* mesh = rm.GetMesh(hMesh);
	//LV3_ASSERT(hMesh.IsValid());

	//const MeshClass* m = rm.GetMesh(hMesh);
	//LV3_ASSERT(m->vertsPerFace == 3 && m->faceCount() == 6);


	/************************************************************
	Paramétrage du scenegraph
	************************************************************/
	std::cout << "\n\033[32m=== Lecture du scenegraph ===\033[0m" << std::endl;

	bool success = SceneSerializer::LoadSceneGraph(cheminProjet, cfg.GraphSceneName, registry, activeCamera, rm);
	if (!success)
	{
		std::cerr << "\033[31mImpossible de construire la scène. Arrêt du programme.\033[0m" << std::endl;
		return -1; 
	}

	SceneSerializer::SpawnCameraGizmos(registry, rm, cfg.gizmoMesh);

	// ── TESTS DE NON-RÉGRESSION — avant toute ressource système ──
	#ifdef _DEBUG
		CheckAnimationBaseline(registry);     // ← TEST A : dt = 0, rien ne bouge

		// --- VÉRIFICATION : AFFICHAGE DE L'ARBRE CONSTRUIT ---
		std::cout << "Structure finale du Scene Graph :" << std::endl;
		DebugDisplaySystem(registry);

		if (!LV3::Tests::RunAllTests(registry)) return -1;

	//	exit(0); // Arrêt du programme après les tests, avant la boucle de jeu
	#endif



	
	// ═══ Initialisation, une seule fois ═══
	WinW = cfg.screenWidth;  // Largeur de l'écran
	WinH = cfg.screenHeight; // Hauteur de l'écran
	SDL_SetMainReady();       // on prend la responsabilité de l'initialisation
	if (SDLINIT(WinW, WinH) != true) return -1;


	db.Resize(WinW, WinH);

	// Les deux régions. Découpage décidé ICI, par l'application.
	//const Viewport vpLeft { 0, 0, WinW / 2, WinH };
	//const Viewport vpRight { WinW / 2, 0, WinW / 2, WinH };
//	const Viewport vp{ 0, 0, WinW, WinH };
	vpLeft.Resize(0, 0, WinW / 2, WinH);
	vpRight.Resize(WinW / 2, 0, WinW / 2, WinH);

	const Entity camFollow = FindCameraByName(registry, "FPS_Camera");
	const Entity camOverview = FindCameraByName(registry, "Overview_Camera");


	SetMouseCapture(true);
//	SDL_SetRelativeMouseMode(SDL_TRUE); 

	pitch = 0;
	int frameCount = 0;
	const int maxFrames = 5; // Arrête la simulation après 100 images
	float deltaTime = 0.5f; // Temps fixe pour une simulation stable

//	std::map < Entity, std::string> entityNames;	// pour le debugage


	// Nettoie la console (fonctionne sur Linux/macOS, pour Windows utiliser "cls")
	// system("clear"); 

	while (g_running == true)
	{

		// 1. Gérer les entrées utilisateur (non implémenté ici)
		PlayerInputSystem(registry, deltaTime);
		LV3::InputState input = BuildInputState();


		// 2. Mettre à jour la scène
		// L'update commence à la racine, avec une matrice identité car elle n'a pas de parent.

		CheckControllerExclusivity(registry);       // CHAQUE frame — invariant FPS/Follow

		// --- 1. MISE À JOUR DE L'ÉTAT (Logique pure) ---
		AnimationSystem(registry, deltaTime);
		FPSControllerSystem(registry, input, deltaTime);      //  un seul agit,
		CameraFollowSystem(registry, deltaTime);             //  m_isEnabled arbitre
		CameraGizmoSystem(registry, activeCamera, vpLeft.Aspect());// (float)WinW / (float)WinH);

		// --- 2. MISE À JOUR DES MATRICES ---
		//TransformationSystem(registry, deltaTime);
		LocalTransformSystem(registry);       // Construit les matrices locales finales
		WorldTransformSystem(registry);       // Construit les matrices mondes finales

		// --- 3. DÉTECTION (Physique/Triggers) ---
		// Lit les matrices mondes finales
		TriggerSystem(registry, eventBus);


#if LV3_DEBUG
//		std::cout << std::endl;
//		std::cout << "--- FRAME " << frameCount << " ---" << std::endl;

		CheckSceneInvariants(registry);       // ← INVARIANTS, chaque frame
//		DebugTraceEntity(registry, "Cube1");  // ← TRACE, à retirer une fois la question tranchée

		// --- 4. DESSIN ---
		// Débug de la hiérarchie 
//		DebugDisplaySystem(registry);// , entityNames);

		// Draw de la hiérarchie
//		RenderSystem(registry, activeCamera, rm);

#endif

		//***************************************
		//RasterTriangle huge{ {-500, -500}, {2000, 400}, {300, 1500}, 0.9f, 0.9f, 0.2f };

		//RasterTriangle tri1{
		//	{0,0}, {400,0}, {400,300}, // v0, v1, v2
		//	0.9f, 0.9f, 0.2f					// z0, z1, z2
		//};

		//	RasterTriangle tri2{
		//{0,0}, {400,300}, {0,300}, // v0, v1, v2
		//0.9f, 0.9f, 0.2f					// z0, z1, z2
		//	};




		//***************************************


		// --- 4. DEUX points de vue, construits par la MÊME fonction ---
		const ViewData viewLeft = BuildViewData(*registry.TryGet<TransformComponent>(camFollow),
			*registry.TryGet<CameraComponent>(camFollow),
			vpLeft, camFollow);

		const ViewData viewRight = BuildViewData(*registry.TryGet<TransformComponent>(camOverview),
			*registry.TryGet<CameraComponent>(camOverview),
			vpRight, camOverview);

		//const ViewData view = BuildViewData(*registry.TryGet<TransformComponent>(camOverview),
		//	*registry.TryGet<CameraComponent>(camOverview),
		//	vp);

		


		// --- 5. UN seul verrou, UN seul effacement ---
		if (SDL_LockTexture(SDLtexture, nullptr, (void**)&ptrScreen, &pitch) == 0)
		{

			fb.Bind(ptrScreen, pitch,WinW, WinH);
			fb.Clear(MakeColor(0, 0, 24));

			// --- 3. DEUX rendus dans le MÊME buffer ---
			Renderer renderer;
			renderer.BeginFrame(fb, db); //

			renderer.SetDepthDisplayRange(80); // permet de gérer la profondeur dans le cas par exemple où on voudrait l'afficher à la place des couleurs
			renderer.SetMode(LV3::ERenderMode::Solid);
			RenderView(registry, rm, renderer, viewLeft);

			renderer.SetMode(LV3::ERenderMode::Solid);
			RenderView(registry, rm, renderer, viewRight);
#ifdef _DEBUG
			ReportCullStats();
#endif
			renderer.EndFrame();
			
// Séparateur vertical
			for (int y = 0; y < WinH; ++y) fb.SetPixel(WinW / 2, y, MakeColor(90, 90, 110));


			// -- test via des triangles 2D - DEB
			//fb.Bind(ptrScreen, pitch, WinW, WinH);
			//fb.Clear(MakeColor(255, 255, 24));		// clear du framebuffer
			////db.Clear();
			//LV3_ASSERT(vp.IsValid());

			//Renderer renderer;
			//renderer.BeginFrame(fb, db);

			//renderer.SetMode(LV3::ERenderMode::Solid);
			//renderer.SetDepthRange(view.nearPlane, view.farPlane);	// permet de gérer la profondeur dans le cas par exemple où on voudrait l'afficher à la place des couleurs
			//renderer.SetViewport(view.viewport);
			//renderer.DrawTriangle(tri1,Color{ 255, 0, 0, 255 });

			//renderer.SetMode(LV3::ERenderMode::Depth);
			//renderer.SetDepthRange(view.nearPlane, view.farPlane);	// permet de gérer la profondeur dans le cas par exemple où on voudrait l'afficher à la place des couleurs
			//renderer.SetViewport(view.viewport);
			//renderer.DrawTriangle(tri2, Color{ 0, 255, 0, 255 });

			// -- test via des triangles 2D - FIN


			renderer.EndFrame();                          // ← le pointeur cesse d'exister
			fb.Unbind();                                  // ← idem
			SDL_UnlockTexture(SDLtexture);

		}
		else
		{
			SDL_Log("SDL_LockTexture a échoué : %s", SDL_GetError());
			return -1; // ou assert — mais surtout, ne continue PAS avec des valeurs invalides
		}

		//SDL_RenderClear(renderer_sdl);
		SDL_RenderCopy(SDLrenderer, SDLtexture, nullptr, nullptr);
		SDL_RenderPresent(SDLrenderer);


		//SDL_RenderClear(SDLrenderer);
		Clean_Render(fb);
		
		// resize si besoin après le lock sur la texture SDL, sinon le pitch est mauvais et on écrit hors bornes dans le framebuffer
		if (resizePending)
		{
			resizePending = false;

			if (pendingW > 0 && pendingH > 0)
			{
				WinW = pendingW;
				WinH = pendingH;

				// 1. La texture SDL (le pitch change aussi !)
				SDL_DestroyTexture(SDLtexture);
				SDLtexture = SDL_CreateTexture(SDLrenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, WinW, WinH);

				// 2. Le Z-buffer
				db.Resize(WinW, WinH);

				// 3. Le viewport
				vpLeft.Resize(0, 0, WinW / 2, WinH);
				vpRight.Resize(WinW / 2, 0, WinW / 2, WinH);

				// 4. L'aspect ratio est dérivé du viewport dans BuildViewData :
				//    rien à faire, il suivra tout seul.

			}
		}

		// Pause pour rendre l'animation lisible dans la console
//		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		frameCount++;
	}


	SDLkill();
	return 0;
}
