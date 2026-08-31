#include "pch.h"          // ← première ligne, toujours
#include "Core/Logger.h"
#include "ConfigManager.h"
#include "Registry/RegistryHelper.h"
#include <fstream>
#include <unordered_map>
#include "core/JsonReader.h"

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
    using nlo_json = nlohmann::json;

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
        nlo_json root;
        try
        {
            file >> root;
        }
        catch (const nlo_json::parse_error& e)
        {
            Logger::error(std::string("Config — JSON malformé : ") + e.what());
            return false;
        }

        JsonReader r(root, "Programme", path);

        try
        {
            JsonReader rr = r.Child("configuration");

            // Lecture sécurisée avec valeurs par défaut si la clé est absente
            cfg.repObjDefault = rr.Read("REP_OBJ_DEFAULT", std::string("G:\\Projects Visual Studio\\OBJ\\"));
            cfg.repGfxDefault = rr.Read("REP_GFX_DEFAULT", std::string("G:\\Projects Visual Studio\\Graphs\\"));

            rr.WarnUnread();
        }
        catch (const nlo_json::exception& e) {
            Logger::error(std::string("Erreur lors du parsing JSON des répertoires des assets ") + e.what());
            return false;
        }

        try
        {
            // lecture de la raille de l'écran
            JsonReader rr = r.Child("screen");
            cfg.screenWidth = rr.Read("width", 1920);
            cfg.screenHeight = rr.Read("height", 1080);            
            rr.WarnUnread();
        }
        catch (const nlo_json::exception& e)
        {
            Logger::error(std::string("Erreur lors du parsing JSON de la taille de l'écran : ") + e.what());
            return false;
        }

        try
        {
            if (r.Has("assets"))
            {
                JsonReader ra = r.Child("assets");
                ra.ForEachChild([&](const std::string& id, LV3::JsonReader entry)
                {
                    AssetStruct a;
                    a.type = entry.Read("type", std::string{});
                    a.object = entry.Read("object", std::string{});

                    if (a.type.empty() || a.object.empty())
                        Logger::warn("[Assets] '" + id + "' : 'type' ou 'object' manquant — entree ignoree");
                    // pas de doublon ici, le parsing du json écrase la clé déjà existeante
                    //else if (cfg.mapAssets.contains(id))
                    //    Logger::warn("[Assets][" + id + "] nom '" + id + "' deja utilise, ignore");
                    else
                        cfg.mapAssets.emplace(id, std::move(a));

                    entry.WarnUnread();   // detecte une faute de frappe DANS cette entree ("objet" au lieu de "object")
                });
                
                Logger::success("=== Chargement des Assets : " + std::to_string(cfg.mapAssets.size()) + " assets chargés ===");
                //// --- 1. Parcours de tous les assets ---
                //for (const auto& [id, AssetGroup] : cfg.mapAssets) {
                //    std::cout << "ID   : " << id << "\n"
                //        << "Type : " << AssetGroup.type << "\n"
                //        << "object : " << AssetGroup.object << "\n"
                //        << "-----------------------\n";
                //}
            }                  
        }
        catch (const nlo_json::exception& e) {
            std::cerr << "Erreur lors du parsing JSON du gizmo ou de lien du scènegraph: " << e.what() << std::endl;
       }

        try
        {

            if (r.Has("viewports"))
            {
                JsonReader rv = r.Child("viewports");
                rv.ForEachElement([&](std::size_t index, LV3::JsonReader elem)
                    {
                        ViewportStruct v;
                        v.nom = elem.Read("nom", std::string{});
                        v.largeur = elem.Read("largeur", 0);
                        v.hauteur = elem.Read("hauteur", 0);

                        if (v.nom.empty() || v.largeur <= 0 || v.hauteur <= 0)
                            Logger::warn("[Viewports][" + std::to_string(index) + "] entree invalide, ignoree");
                        else if (cfg.mapViewports.contains(v.nom))
                            Logger::warn("[Viewports][" + std::to_string(index) + "] nom '" + v.nom + "' deja utilise, ignore");
                        else
                            cfg.mapViewports.emplace(v.nom, std::move(v));

                        elem.WarnUnread();   // detecte une faute de frappe DANS cette entree ("objet" au lieu de "object")
                    });

                Logger::success("=== Chargement des viewports : " + std::to_string(cfg.mapViewports.size()) + " viewports detectes ===");
                //int width = 0, height = 0;
                //for (const auto& [nom, vp] : cfg.mapViewports)
                //{
                //    std::cout << "[Configuration] Viewport '" << nom << "' -> Dimensions : " << vp.largeur << "x" << vp.hauteur << "\n";
                //}

            }

        }
        catch (const nlo_json::exception& e) {
            std::cerr << "Erreur lors du parsing JSON des viewport: " << e.what() << std::endl;
        }

        r.WarnUnread();
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
