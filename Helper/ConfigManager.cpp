#include "pch.h"          // ← première ligne, toujours
#include "Core/Logger.h"

#include "../ressources/json.hpp"
#include "ConfigManager.h"
#include "Registry/RegistryHelper.h"
#include <fstream>
#include <unordered_map>

using namespace LV3;



    /************************************************************
        Assure que la console Windows utilise UTF-8 pour afficher les caractères correctement
        (utile si les chemins ou messages contiennent des caractères non-ASCII)
    ************************************************************/
    void SetConsoleMode()
    {
        SetConsoleOutputCP(CP_UTF8);    // Configure la page de code pour la sortie (Output).
        SetConsoleCP(CP_UTF8);          // Configure la page de code pour l'entrée (Input)
#if defined(LV3_PLATFORM_WINDOWS)
        SetConsoleOutputCP(CP_UTF8);    // Configure la page de code pour la sortie (Output).
        SetConsoleCP(CP_UTF8);          // Configure la page de code pour l'entrée (Input)
#endif
    }


    //************************************************************
    using json = nlohmann::json;


    // 2. Binding automatique avec nlohmann/json
    // Associe automatiquement les champs "nom", "largeur" et "hauteur" du JSON
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
        ViewportStruct,
        nom,
        largeur,
        hauteur)

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
        AssetStruct,
        type,
        path)

//    bool ProgrammeConfig(const std::string& path, std::string& repObjDefault, std::string& repGfxDefault)
    bool ProgrammeConfig(const std::string& path, config& cfg)
    {
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
        
        try
        {
            // Lecture sécurisée avec valeurs par défaut si la clé est absente
            cfg.repObjDefault = root["configuration"].value("REP_OBJ_DEFAULT", "G:\\Projects Visual Studio\\OBJ\\");
            cfg.repGfxDefault = root["configuration"].value("REP_GFX_DEFAULT", "G:\\Projects Visual Studio\\Graphs\\");

        }
        catch (const json::exception& e) {
            Logger::error(std::string("Erreur lors du parsing JSON des répertoires des assets ") + e.what());
            return false;
        }

        try
        {
            // lecture de la raille de l'écran
            cfg.screenWidth = root["screen"].value("width", 1920); // 1920 par défaut si manquant
            cfg.screenHeight = root["screen"].value("height", 1080); // 1080 par défaut si manquant
        }
        catch (const json::exception& e) 
        {
            Logger::error(std::string("Erreur lors du parsing JSON de la taille de l'écran : ") + e.what());
            return false;
        }

        try
        {
            //for (const auto& item : root["assets"]) {
            //    AssetStruct vp = item.get<AssetStruct>(); // Utilise la macro automatique
            //    cfg.mapAssets[vp.type] = vp;
            //}
            
            auto assetsMap = root["assets"].get<std::unordered_map<std::string, AssetStruct>>();
            cfg.mapAssets = assetsMap;
            std::cout << "=== Assets charges (" << cfg.mapAssets.size() << ") ===\n\n";
            
            // --- 1. Parcours de tous les assets ---
            for (const auto& [id, AssetGroup] : cfg.mapAssets) {
                std::cout << "ID   : " << id << "\n"
                    << "Type : " << AssetGroup.type << "\n"
                    << "Path : " << AssetGroup.path << "\n"
                    << "-----------------------\n";
            }

            // Lecture sécurisée avec valeurs par défaut si la clé est absente
            //cfg.GizmoMeshPersective = root["GizmoMeshPersective"];
            //cfg.GizmoMeshOrthographiq = root["GizmoMeshOrthographiq"];
            //cfg.GraphSceneName = root["GraphSceneName"];
        }
        catch (const json::exception& e) {
            std::cerr << "Erreur lors du parsing JSON du gizmo ou de lien du scènegraph: " << e.what() << std::endl;
       }


        int b = 0;
        try
        {
            for (const auto& item : root["viewports"]) {
                ViewportStruct vp = item.get<ViewportStruct>(); // Utilise la macro automatique
                cfg.mapViewports[vp.nom] = vp;
            }
            std::cout << "=== Chargement reussi : " << cfg.mapViewports.size() << " viewports detectes ===\n\n";

            // Parcours pour configurer chaque viewport
            int width = 0, height = 0;
            for (const auto& [nom, vp] : cfg.mapViewports)
            {
                std::cout << "[Configuration] Viewport '" << nom << "' -> Dimensions : " << vp.largeur << "x" << vp.hauteur << "\n";
                //width += vp.largeur;
                //height += vp.hauteur;
            }

            //LV3_ASSERT(width == cfg.screenWidth && height == cfg.screenHeight);

                // Exemple : Appliquer les parametres dans ton moteur graphiques/IHM
                // configurerViewport(vp.nom, vp.largeur, vp.hauteur);

                //// Accès direct rapide par clé :
                //if (cfg.mapViewports.find("titre") != cfg.mapViewports.end()) {
                //    std::cout << "\nLe viewport 'titre' existe, sa hauteur est : "
                //        << cfg.mapViewports["titre"].hauteur << "px\n";
                //}

                    // Accès direct rapide par clé :
                /*if (cfg.mapViewports.find("titre") != cfg.mapViewports.end()) {
                    std::cout << "\nLe viewport 'titre' existe, sa hauteur est : "
                        << cfg.mapViewports["titre"].hauteur << "px\n";
                }*/

            //}

        }
        catch (const json::exception& e) {
            std::cerr << "Erreur lors du parsing JSON des viewport: " << e.what() << std::endl;
        }

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
