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

    void TestF1_EntityVersioning();
    void TestF5_ResourceManager_UnloadMesh();
    int RunAllCameraMathTests();
    int TestProjection();
    int TestMatrixLib();
    bool Test_TopLeftRule_NoDoubleCoverage();
    bool Test_TopLeftRule_ExactCoverage();
    bool Test_TopLeftRule_SmallTriangles();
    int TestCleanBuffer();
    void TestFrontFaceSign();
    void DebugDumpControllers(Registry& reg);
    void CheckControllerExclusivity(Registry& reg);
    [[nodiscard]] bool Test_ClipCoverage_NoCrackNoOverlap();
    bool Test_Rasterizer_EmptyBoxes();





    static int s_failures = 0;

    static void Run(const char* name, bool (*fn)())
    {
        const bool ok = fn();
        Logger::info(std::string(ok ? "\033[32m  [OK]   \033[0m"
            : "\033[31m  [ECHEC]\033[0m") + name);
        if (!ok) ++s_failures;
    }

    bool RunAllTests(Registry & registry)
    {
        TestF1_EntityVersioning();
        TestF5_ResourceManager_UnloadMesh();
        RunAllCameraMathTests();
        TestProjection();
        TestMatrixLib();
        TestFrontFaceSign();




        if (!Test_Depth_CrossingTriangles())
        {
            printf("\033[31mECHEC : Test_Depth_CrossingTriangles\033[0m\n");
            return false;
        }
        else
        {
            printf("\033[32mSUCCES : Test_Depth_CrossingTriangles\033[0m\n");
        }


        if (!Test_Rasterizer_EmptyBoxes())
        {
            printf("\033[31mECHEC : Test_Rasterizer_EmptyBoxes\033[0m\n");
            return false;
        }
        else
        {
            printf("\033[32mSUCCES : Test_Rasterizer_EmptyBoxes\033[0m\n");
        }

        // Validation avant tout démarrage moteur
        if (!Test_TopLeftRule_NoDoubleCoverage())
        {
            printf("\033[31mECHEC : Test_TopLeftRule\033[0m\n");
            return false;
        }
        printf("\033[32mOK : Test_TopLeftRule, pas de pixel dessiné 2 fois\033[0m\n");

        if (!Test_TopLeftRule_ExactCoverage())
        {
            printf("\033[31mECHEC : Test_TopLeftRule_ExactCoverage\033[0m\n");
            return false;
        }
        printf("\033[32mOK : Test_TopLeftRule_ExactCoverage, couverture exacte\033[0m\n");

        if (!Test_TopLeftRule_SmallTriangles())
        {
            printf("\033[31mECHEC : Test_TopLeftRule_SmallTriangles\033[0m\n");
            return false;
        }
        printf("\033[32mOK : Test_TopLeftRule_SmallTriangles, couverture exacte\033[0m\n");

        DebugDumpControllers(registry);
        CheckControllerExclusivity(registry);

        Logger::info("=== Tests de non-regression ===");
        s_failures = 0;

        Run("Winding — face avant = aire raster negative", Test_FrontFaceSign);
        Run("Clipper — 4 configurations", Test_ClipTriangleNear_Cases);
        Run("Clipper — winding preserve", Test_ClipPreservesWinding);
        Run("Coverage — ni fissure ni doublon apres clip", Test_ClipCoverage_NoCrackNoOverlap);

        Logger::info(s_failures == 0
            ? "\033[32m=== Tous les tests passent ===\033[0m"
            : "\033[31m=== " + std::to_string(s_failures) + " echec(s) ===\033[0m");
        return s_failures == 0;


    }



}