#include "pch.h"
#include "Test_ClipCoverage.h"
#include "Test_Clipper.h"
#include "Test_Winding.h"
#include "Core/Logger.h"
#include "Test_TopLeftRule.h"
#include "scene/registry.hpp"
#include "RunAllTests.h" 
#include "Test_Depth.h"
#include "TestAffichageGizmoCamera.h"

namespace LV3::Tests
{
    bool test_hierarchy();
    void TestF1_EntityVersioning();
    void TestF5_ResourceManager_UnloadMesh();
    int RunAllCameraMathTests();
    void TestCameraZoom();
    int TestProjection();
    int TestMatrixLib();
    bool Test_TopLeftRule_NoDoubleCoverage();
    bool Test_TopLeftRule_ExactCoverage();
    bool Test_TopLeftRule_SmallTriangles();
    int TestCleanBuffer();
    void TestFrontFaceSign();
    void DebugDumpControllers(Registry& reg);
    [[nodiscard]] bool Test_ClipCoverage_NoCrackNoOverlap();
    bool Test_Rasterizer_EmptyBoxes();
    void Test_SimulationClock();


#if LV3_DEBUG    
    static int s_failures = 0;

    static void Run(const char* name, bool (*fn)())
    {
        const bool ok = fn();
        Logger::info(std::string(ok ? " [OK] " : " [ECHEC] ") + name);
        if (!ok) ++s_failures;
    }
#endif
    bool RunAllTests(Registry & registry)
    {
#if LV3_DEBUG

        Logger::info("================== TNR ==================");

        if (!test_hierarchy())
        {
            Logger::error("ECHEC : Test 0 hierarchy failed");
            return false;
        }
        else
        {
            Logger::success("SUCCES : Test 0 hierarchy success");
        }

        TestF1_EntityVersioning();
        TestF5_ResourceManager_UnloadMesh();
        RunAllCameraMathTests();
        TestCameraZoom();
        TestProjection();
        TestMatrixLib();
        TestFrontFaceSign();

        if (!Test_Depth_CrossingTriangles())
        {
            Logger::error("ECHEC : Test_Depth_CrossingTriangles");
            return false;
        }
        else
        {
            Logger::success("SUCCES : Test_Depth_CrossingTriangles");
        }


        if (!Test_Rasterizer_EmptyBoxes())
        {
            Logger::error("ECHEC : Test_Rasterizer_EmptyBoxes");
            return false;
        }
        else
        {
            Logger::success("SUCCES : Test_Rasterizer_EmptyBoxes");
        }

        // Validation avant tout démarrage moteur
        if (!Test_TopLeftRule_NoDoubleCoverage())
        {
            Logger::error("ECHEC : Test_TopLeftRule");
            return false;
        }
        Logger::success("OK : Test_TopLeftRule, pas de pixel dessiné 2 fois\n");

        if (!Test_TopLeftRule_ExactCoverage())
        {
            Logger::error("ECHEC : Test_TopLeftRule_ExactCoverage");
            return false;
        }
        Logger::success("OK : Test_TopLeftRule_ExactCoverage, couverture exacte\n");

        if (!Test_TopLeftRule_SmallTriangles())
        {
            Logger::error("ECHEC : Test_TopLeftRule_SmallTriangles");
            return false;
        }
        Logger::success("OK : Test_TopLeftRule_SmallTriangles, couverture exacte\n");

        DebugDumpControllers(registry);

        Logger::info("=== Tests de non-regression ===");
        s_failures = 0;

        Run("Winding — face avant = aire raster negative", Test_FrontFaceSign);
        Run("Clipper — 4 configurations", Test_ClipTriangleNear_Cases);
        Run("Clipper — winding preserve", Test_ClipPreservesWinding);
        Run("Coverage — ni fissure ni doublon apres clip", Test_ClipCoverage_NoCrackNoOverlap);
        Test_SimulationClock();

        Logger::info(s_failures == 0
            ? "=== Tous les tests passent ===\n"
            : "=== " + std::to_string(s_failures) + " echec(s) ===\n");
        return s_failures == 0;

        #else
            (void)registry;
            Logger::info("[TNR] Suite de tests desactivee en Release : aucun test execute.");
            return true;
        #endif
    }



}