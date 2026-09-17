#include "pch.h"          // ← première ligne, toujours
#include "scene/registry.hpp"
#include "Core/Logger.h"
namespace LV3::Tests
{
    // ============================================================
    // TEST 54 — recyclage du meme slot, a coller temporairement au
    // debut de main(), avant tout chargement de scene.
    // A retirer apres validation. Duree : quasi instantanee
    // (65536 iterations de push/pop sur un vector, rien de lourd).
    // ============================================================
    bool test54_Recyclage()
    {
        using namespace LV3;

        // --- Test A : l'ancien seuil de rupture (256) est desormais sans danger ---
        {
            Registry testRegistry;
            Entity oldHandle = testRegistry.CreateEntity();      // idx X, generation 0
            testRegistry.DestroyEntity(oldHandle);                // generation -> 1

            constexpr int kOldBreakingPoint = 300;                // > 256 : l'ancien point de rupture, avec marge
            for (int i = 1; i < kOldBreakingPoint; ++i)
            {
                Entity e = testRegistry.CreateEntity();           // reutilise forcement le meme index (seul index libre)
                testRegistry.DestroyEntity(e);
            }

            const bool staleStillAlive = testRegistry.IsAlive(oldHandle);
            if(!staleStillAlive)
            { 
            Logger::success(std::string("[Test54-A] apres ") + std::to_string(kOldBreakingPoint)
                + " recyclages — IsAlive(handle perime, gen 0) = " + std::to_string(staleStillAlive)
                + "  (attendu : 0 — l'ancien seuil de rupture a 256 est maintenant loin derriere)");
            }
            else
            {
                Logger::error(std::string("[Test54-A] test54_Recyclage : KO ") + std::to_string(kOldBreakingPoint)
                    + " recyclages — IsAlive(handle perime, gen 0) = " + std::to_string(staleStillAlive)
                    + "  (attendu : 0 — l'ancien seuil de rupture a 256 est maintenant loin derriere)");
				return false;
            }
        }

        // --- Test B : la limite structurelle existe toujours, juste a 65536 ---
        {
            Registry testRegistry;
            Entity oldHandle = testRegistry.CreateEntity();      // idx X, generation 0
            testRegistry.DestroyEntity(oldHandle);                // generation -> 1 (1 destruction)

            constexpr int kGenerationWidth = 65536;               // 2^16 : largeur exacte du champ generation
            for (int i = 1; i < kGenerationWidth; ++i)            // 65535 cycles de plus = 65536 destructions au total
            {
                Entity e = testRegistry.CreateEntity();
                testRegistry.DestroyEntity(e);
            }

            const bool staleRevalidated = testRegistry.IsAlive(oldHandle);
            if (staleRevalidated)
            Logger::success(std::string("[Test54-B] apres ") + std::to_string(kGenerationWidth)
                + " recyclages exactement — IsAlive(handle perime, gen 0) = " + std::to_string(staleRevalidated)
                + "  (attendu : 1 — la generation a bel et bien bouclee ; ABA toujours possible en"
                " theorie, mais il faut maintenant 65536 recyclages du MEME slot, contre 256 avant)");
            else
            {
                Logger::error(std::string("[Test54-B] test54_Recyclage : KO ") + std::to_string(kGenerationWidth)
                    + " recyclages exactement — IsAlive(handle perime, gen 0) = " + std::to_string(staleRevalidated)
                    + "  (attendu : 1 — la generation a bel et bien bouclee ; ABA toujours possible en"
                    " theorie, mais il faut maintenant 65536 recyclages du MEME slot, contre 256 avant)");
                return false;
            }
        }
        return true;
    }

}

