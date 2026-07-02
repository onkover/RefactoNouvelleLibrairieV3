/*
	(c) Onkover

	26/06/26
	Nouvelle gestion de librairie graphique v3
	S'appuie sur le librairie V2.1 entierement réécrite par Claude.ai
	
	todo:
	* tout ce qui concerne la lecture de la scène (fichier texte) et la création des objets graphiques à partir de cette scène
	 doit être déplacé dans une classe dédiée (ex: SceneLoader) pour séparer les responsabilités et améliorer la maintenabilité du code.
	 Cette classe pourrait être responsable de :
		- Lire le fichier de scène
		- Parser les données de la scène
		- Créer les objets graphiques correspondants en utilisant les répertoires spécifiés dans la base de registres

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

using namespace LV3;

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
	std::cout << "\n\033[32m=== Lecture de scene.json ===\033[0m" << std::endl;

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

}
