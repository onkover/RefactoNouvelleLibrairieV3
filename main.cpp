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

#include <pch.h>          // ← première ligne, toujours
#include "Core/Platform.h"

#include "helper/ConfigManager.h"


//using namespace LibV3;

int main()
{

	SetConsoleMode();	// mode cosole en UTF-8

	///************************************************************
	//Lecture du nom des répertoires depuis la base de registres
	//************************************************************/
	// Utilisation du chemin...
	//std::wstring RepObj;
	//if (LoadGameDirectory(L"REPOBJ", RepObj, REP_OBJ_DEFAULT) != 0)
	//{
	//	std::cout << "\033[31mImpossible de lire la clé de la variable d'environnement OBJ\033[0m" << '\n';
	//	exit(1);
	//}
	//std::wcout << L"Répertoire objets : " << RepObj << std::endl;

	//std::wstring RepGfx;
	//if (LoadGameDirectory(L"RepGfx", RepGfx, REP_GFX_DEFAULT) != 0)
	//{
	//	std::cout << "\033[31mImpossible de lire la clé de la variable d'environnement GFX\033[0m" << '\n';
	//	exit(1);
	//}
	//std::wcout << L"Répertoire graphismes : " << RepGfx << std::endl;



    std::cout << "Hello World!\n";
}
