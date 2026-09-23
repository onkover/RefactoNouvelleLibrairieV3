#pragma once
// ============================================================
//  Helper/AppArgs.h — ligne de commande de l'APPLICATION
//
//  Le moteur ignore qu'une ligne de commande existe : il recoit
//  une racine deja resolue et un nom de scene deja valide.
//
//  Grammaire, volontairement pauvre :
//     <exe> [<racine_de_contenu>] [--cle=valeur ...]
//  Un seul positionnel (la racine), et toute option porte sa
//  valeur avec elle. Toute option inconnue est une ERREUR.
// ============================================================

#include <cstdint>
#include <filesystem>
#include <string>

struct AppArgs
{
	std::filesystem::path contentRoot;      // positionnel, optionnel (replis : env, LV3_PROJECT_DIR, dossier exe)
	std::string scene;                      // --scene=   : relatif a resources.scene, prime sur config.json
	std::string csv;                        // --csv=     : relatif a contentRoot
	uint32_t    benchFrames = 0;           // --bench=N  : 0 = session interactive normale
	uint32_t    warmupFrames = 60;          // --warmup=N : frames exclues du resume
	bool        wantsHelp = false;
	bool        valid = true;               // false -> l'appelant DOIT s'arreter
};

[[nodiscard]] AppArgs ParseArgs(int argc, char* argv[]);
void PrintUsage();