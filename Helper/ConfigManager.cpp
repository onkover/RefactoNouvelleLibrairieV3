#include <pch.h>          // ← première ligne, toujours


#include <Core/Logger.h>

#include "../ressources/json.hpp"
#include "ConfigManager.h"
#include "Registry/RegistryHelper.h"

#include <fstream>

//using namespace LibV3;

namespace LibV3
{


    /************************************************************
        Assure que la console Windows utilise UTF-8 pour afficher les caractères correctement
        (utile si les chemins ou messages contiennent des caractères non-ASCII)
    ************************************************************/
    void SetConsoleMode()
    {
#if defined(LV2_PLATFORM_WINDOWS)
        SetConsoleOutputCP(CP_UTF8);    // Configure la page de code pour la sortie (Output).
        SetConsoleCP(CP_UTF8);          // Configure la page de code pour l'entrée (Input)
#endif
    }



    //************************************************************
    //************************************************************

    bool ProgrammeConfig(const std::string& path)
    {
        using json = nlohmann::json;
        namespace fs = std::filesystem;

        // 1. Ouvrir le fichier
        std::ifstream file(path);
        if (!file.is_open()) {
            Logger::error("Config — fichier introuvable : " + path);
            return false;
        }

        // 2. Parser le JSON
        json root;
        try
        {
            file >> root;
        }
        catch (const json::parse_error& e)
        {
            Logger::error(std::string("Config — JSON malformé : ") + e.what());
            return false;
        }

        // Lecture sécurisée avec valeurs par défaut si la clé est absente
        std::string repObjDefault = root["configuration"].value("REP_OBJ_DEFAULT", "G:\\Projects Visual Studio\\OBJ\\");
        std::string repGfxDefault = root["configuration"].value("REP_GFX_DEFAULT", "G:\\Projects Visual Studio\\Graphs\\");

        int screenWidth = root["screen"].value("width", 1920); // 1920 par défaut si manquant
        int screenHeight = root["screen"].value("height", 1080); // 1080 par défaut si manquant

        return true;
    }





    /// <summary>
    /// Charge le répertoire du jeu à partir de la base de registres.
    /// </summary>
    /// <param name="nameVariable"></param>
    /// <param name="output"></param>
    /// <param name="defaultPath"></param>
    /// <returns></returns>
    int LoadGameDirectory(std::wstring nameVariable, std::wstring& output, const std::wstring defaultPath)
    {

        // Clé applicative sous HKCU
        const std::wstring keyPath = L"Environment\\Onky";

        try
        {
            output = GetOrCreateString(
                HKEY_CURRENT_USER,
                keyPath,
                nameVariable,// L"REPOBJ",
                defaultPath);

            return 0;
        }

        catch (const std::runtime_error& e)
        {
            // Erreur spécifique levée par GetOrCreateString (accès refusé, clé invalide...)
            std::cerr << "\n\033[32m=== Erreur registre : " << e.what() << " ===\033[0m" << std::endl;
            return 1;
        }
        catch (const std::exception& e)
        {
            // Toute autre exception standard
            std::cerr << "\n\033[32m=== Erreur inattendue : " << e.what() << " ===\033[0m" << std::endl;
            return 1;
        }
    }


}