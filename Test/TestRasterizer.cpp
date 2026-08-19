#include "pch.h"
#include "Maths/Vectorlib.h"
#include "rendering/Viewport.h"
#include "rendering/Rasterizer.h"
#include "maths/Projection.h"
#include "core/Logger.h"

namespace LV3::Tests
{

    // TestRasterizer — §Winding : la face avant a une aire NEGATIVE en raster
    void TestFrontFaceSign()
    {

        const Viewport  vp = Viewport::FullScreen(800, 800);
        const Matrix44f proj = Projection::Perspective(60.f * TO_RADIAN, 1.0f, 0.1f, 100.f);

        // CCW vu de la camera (origine, regard -Z) => FACE AVANT
        const Vec3f v[3] = { {-1,-1,-5}, {1,-1,-5}, {0,1,-5} };

        Vec3f r[3];
        for (int k = 0; k < 3; ++k)
        {
            const Vec4f c = MulRow(proj, v[k]);
            const float inv = 1.0f / c.w;
            r[k] = vp.ToRaster({ c.x * inv, c.y * inv, c.z * inv });
        }

        const float area = EdgeFunction(r[0], r[1], r[2]);
        assert(area < 0.0f && "\033[31mFace AVANT : l'aire raster doit etre NEGATIVE\033[0m\n");
        assert(IsBackFacing(area) == false);

        // et le miroir : l'ordre inverse doit etre cullé
        assert(IsBackFacing(EdgeFunction(r[0], r[2], r[1])) == true);
        printf("\033[32mSUCCES : TestFrontFaceSign OK \033[0m\n");

    }

    static int s_calls = 0;
    static void CountingFragment(int32_t, int32_t, const BarycentricWeights&, void*)
    {
        ++s_calls;
    }

    bool Test_Rasterizer_EmptyBoxes()
    {
        bool ok = true;

        auto Expect = [&](const char* label, const Viewport& vp,
            Vec2f a, Vec2f b, Vec2f c, int expected)
            {
                s_calls = 0;
                RasterizeTriangle(a, b, c, vp, CountingFragment, nullptr);
                if (expected == 0 ? (s_calls != 0) : (s_calls == 0))
                {
                    Logger::error(std::string("[RASTER] ") + label
                        + " : " + std::to_string(s_calls) + " fragments");
                    ok = false;
                }
            };

        const Viewport vp = Viewport::FullScreen(64, 64);

        // a) entièrement à gauche du viewport  -> maxX <= 0 -> minX >= maxX
        Expect("hors ecran gauche", vp, { -100,10 }, { -60,10 }, { -80,40 }, 0);

        // b) entièrement en bas                -> minY >= maxY
        Expect("hors ecran bas", vp, { 10,200 }, { 40,200 }, { 25,240 }, 0);

        // c) triangle dégénéré (aire nulle)    -> rejet sur area == 0
        Expect("degenere colineaire", vp, { 10,10 }, { 20,20 }, { 30,30 }, 0);

        // d) triangle réduit à un point
        Expect("degenere point", vp, { 10,10 }, { 10,10 }, { 10,10 }, 0);

        // e) viewport invalide (0x0) — le cas de la fenêtre minimisée
        const Viewport vpNull = Viewport::FullScreen(0, 0);
        Expect("viewport nul", vpNull, { 10,10 }, { 50,10 }, { 30,50 }, 0);

        // f) CONTRE-EXEMPLE : un triangle valide DOIT produire des fragments.
        //    Sans lui, une fonction qui ne fait jamais rien passerait tous les tests.
        Expect("triangle valide", vp, { 10,10 }, { 50,10 }, { 30,50 }, 1);



        return ok;
    }
}