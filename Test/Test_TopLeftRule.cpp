#include "pch.h"

#include <vector>
#include <cassert>
#include <cstdint>


#include "rendering/Fragment.h"   // FragmentContext, FragmentCallback
#include "rendering/Rasterizer.h"

namespace LV3::Tests
{

    // Contexte local au test — n'a AUCUNE raison d'exister dans Fragment.h
    //struct CountContext
    //{
    //    std::vector<uint8_t>* counts;
    //    int32_t width;
    //};

    // Callback local au test — n'a AUCUNE raison d'exister dans Fragment.cpp
    //static void ShadeFragment_Count(int32_t x, int32_t y, const BarycentricWeights&, void* userData)
    //{
    //    auto* ctx = static_cast<CountContext*>(userData);
    //    (*ctx->counts)[y * ctx->width + x]++;
    //}

    bool Test_TopLeftRule_NoDoubleCoverage()
    {
        constexpr int32_t W = 400;
        constexpr int32_t H = 300;

        // 1. Le vecteur appartient à CETTE fonction — c'est le point clé
        std::vector<uint8_t> counts(W * H, 0);

        // 2. CountContext ne sert QUE de véhicule pour le void*
        CountContext ctx{ &counts, W };

        // 3. Quad découpé en deux triangles partageant la diagonale (0,0)→(400,300)
        Vec2f a{ 0,   0 };
        Vec2f b{ 400, 0 };
        Vec2f c{ 400, 300 };
        Vec2f d{ 0,   300 };

        const Viewport vp = Viewport::FullScreen(W, H);

        RasterizeTriangle(a, b, c, vp, &ShadeFragment_Count, &ctx);

        // 4. Ici, ctx est devenu inutile. On lit directement `counts`.
        for (int32_t i = 0; i < W * H; ++i)
        {
            if (counts[i] > 1)
            {
                printf("Top-left rule violee : pixel (%d,%d) rasterise %d fois\n",
                    i % W, i / W, counts[i]);
                return false;
            }
        }
        return true;
    }

    // Deux triangles partageant une diagonale doivent couvrir le quad
    // EXACTEMENT une fois : ni double (jointure visible en transparence),
    // ni trou (bruit de fond, le bug du biais -1.0f).
    bool Test_TopLeftRule_ExactCoverage()
    {
        constexpr int W = 64, H = 64;
        std::vector<uint8_t> counts(size_t(W) * H, 0);

        const LV3::Viewport vp = LV3::Viewport::FullScreen(W, H);
        LV3::CountContext   ctx{ &counts, W };

        // Quad convexe, découpé en deux triangles sur la diagonale v0–v2.
        // Coordonnées volontairement non alignées sur la grille.
        const LV3::Vec2f v0{ 10.3f, 10.7f }, v1{ 50.1f, 12.4f },
            v2{ 48.6f, 52.2f }, v3{ 12.8f, 50.9f };

        LV3::RasterizeTriangle(v0, v1, v2, vp, &LV3::ShadeFragment_Count, &ctx);
        LV3::RasterizeTriangle(v0, v2, v3, vp, &LV3::ShadeFragment_Count, &ctx);

        // Un point est STRICTEMENT dedans si les 3 fonctions d'arête ont
        // le signe de l'aire, avec une marge d'un pixel. La marge écarte
        // toute ambiguïté de frontière : on ne teste que le cœur.
        auto core = [](const LV3::Vec2f& a, const LV3::Vec2f& b, const LV3::Vec2f& c,
            float px, float py) noexcept
            {
                const float s = (LV3::EdgeFunction(a, b, c) > 0.f) ? 1.f : -1.f;
                constexpr float margin = 2.0f;              // en unités de fonction d'arête
                return LV3::EdgeFunction(a, b, px, py) * s > margin
                    && LV3::EdgeFunction(b, c, px, py) * s > margin
                    && LV3::EdgeFunction(c, a, px, py) * s > margin;
            };

        int holes = 0, doubles = 0;
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
            {
                const uint8_t n = counts[size_t(y) * W + x];
                const float   px = float(x) + 0.5f, py = float(y) + 0.5f;

                if (n > 1) ++doubles;                                   // recouvrement

                if (core(v0, v1, v2, px, py) || core(v0, v2, v3, px, py))
                    if (n == 0) ++holes;                                // TROU
            }

        if (doubles) std::printf("[FAIL] top-left : %d pixel(s) dessine(s) 2 fois\n", doubles);
        if (holes)   std::printf("[FAIL] top-left : %d TROU(S) — biais dependant de l'echelle ?\n", holes);
        return doubles == 0 && holes == 0;
    }

    // Rasterise un quad convexe de cote `size`, decoupe en DEUX triangles
    // partageant la diagonale v0-v2. Compte :
    //   * les TROUS    : pixel du coeur jamais dessine   -> biais trop agressif
    //   * les DOUBLONS : pixel dessine plus d'une fois   -> regle top-left absente
    void MeasureQuadCoverage(float size, int& outHoles, int& outDoubles)
    {
        using namespace LV3;

        const int W = int(size * 1.6f) + 24;      // marge pour le carre pivote
        const int H = W;
        std::vector<uint8_t> counts(size_t(W) * H, 0);

        const Viewport vp = Viewport::FullScreen(W, H);
        CountContext   ctx{ &counts, W };

        // Carre pivote de 0,3 rad : convexe par construction, et AUCUNE arete
        // alignee sur la grille. On teste le cas general, pas un cas particulier.
        const float cx = W * 0.5f, cy = H * 0.5f, hs = size * 0.5f;
        const float ca = std::cos(0.3f), sa = std::sin(0.3f);
        auto rot = [&](float dx, float dy) noexcept -> Vec2f
            { return { cx + dx * ca - dy * sa, cy + dx * sa + dy * ca }; };

        const Vec2f v0 = rot(-hs, -hs), v1 = rot(hs, -hs),
            v2 = rot(hs, hs), v3 = rot(-hs, hs);

        RasterizeTriangle(v0, v1, v2, vp, &ShadeFragment_Count, &ctx);
        RasterizeTriangle(v0, v2, v3, vp, &ShadeFragment_Count, &ctx);

        // Le "coeur" : strictement dedans, a plus d'un DEMI-PIXEL de toute arete.
        // La marge ecarte l'ambiguite de frontiere — on ne juge que l'interieur.
        // EdgeFunction = |arete| x distance, donc une marge de 0.5*size correspond
        // a 0.5 pixel quelle que soit la taille du triangle.
        const float margin = 0.5f * size;
        auto core = [&](const Vec2f& a, const Vec2f& b, const Vec2f& c,
            float px, float py) noexcept -> bool
            {
                const float s = (EdgeFunction(a, b, c) > 0.f) ? 1.f : -1.f;
                return EdgeFunction(a, b, px, py) * s > margin
                    && EdgeFunction(b, c, px, py) * s > margin
                    && EdgeFunction(c, a, px, py) * s > margin;
            };

        outHoles = outDoubles = 0;
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
            {
                const uint8_t n = counts[size_t(y) * W + x];
                const float   px = float(x) + 0.5f, py = float(y) + 0.5f;

                if (n > 1) ++outDoubles;
                if ((core(v0, v1, v2, px, py) || core(v0, v2, v3, px, py)) && n == 0) ++outHoles;
            }
    }
    bool Test_TopLeftRule_SmallTriangles()
    {
        bool ok = true;
        std::printf("\033[32m--- Couverture exacte selon la taille du triangle ---\033[0m\n");

        for (float size : { 3.0f, 5.0f, 8.0f, 15.0f, 40.0f, 200.0f })
        {
            int holes = 0, doubles = 0;
            MeasureQuadCoverage(size, holes, doubles);

            const bool pass = (holes == 0 && doubles == 0);
            std::printf("  cote %6.1f px : %2d trou(s), %2d doublon(s)   %s\n",
                size, holes, doubles, pass ? "OK" : "<<< FAIL");
            ok = ok && pass;
            assert(pass);
        }
        return ok;
    }
}