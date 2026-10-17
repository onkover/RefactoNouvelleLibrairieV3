#include "pch.h"
#include <random>
#include <limits>
#include <vector>
#include "rendering/Viewport.h"
#include "rendering/Rasterizer.h"
#include "rendering/FaceStage.h"
#include "core/Logger.h"

namespace LV3::Tests
{
#if LV3_DEBUG
    // B4 : ClassifyFaces8 == scalaire AU BIT PRES, sur les cas qu'aucune scene ne fabrique.
    bool Test_B4_ClassifyFaces8()
    {
        constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();
        constexpr float kInf = std::numeric_limits<float>::infinity();
        const float special[] = { kNaN, kInf, -kInf, 0.0f, -0.0f, 0.5f, -0.5f, 1.5f, 2.5f,
                                  1e10f, -1e10f, 3e9f, -3e9f, 1535.5f, 1536.5f, 899.5f, 900.5f, 768.0f, 768.5f };
        constexpr uint32_t kSpecial = uint32_t(std::size(special));

        constexpr uint32_t kVerts = 1024;
        std::vector<RasterVertex> rv(kVerts);
        std::mt19937 rng(42);   // graine FIXE : un echec se rejoue a l'identique
        std::uniform_real_distribution<float> big(-3000.0f, 3000.0f), frac(0.0f, 4.0f);

        for (uint32_t i = 0; i < kVerts; ++i)
        {
            switch (rng() % 4)
            {
            case 0:  rv[i] = { big(rng), big(rng), 0.0f, 1.0f }; break;
            case 1:  rv[i] = { std::floor(big(rng)) + 0.5f * float(rng() % 3),      // demi-entiers :
                               std::floor(big(rng)) + 0.5f * float(rng() % 3),      // bords exacts de ceil/floor
                               0.0f, 1.0f }; break;
            case 2:  rv[i] = { special[rng() % kSpecial], special[rng() % kSpecial], 0.0f, 1.0f }; break;
            default: rv[i] = { float(rng() % 1600) + frac(rng), float(rng() % 950) + frac(rng), 0.0f, 1.0f }; break;
            }
        }

        Viewport split = Viewport::FullScreen(1536, 900);
        split.x = 768; split.y = 450; split.width = 768; split.height = 450;
        const Viewport vps[] = { Viewport::FullScreen(1536, 900), split, Viewport::FullScreen(0, 0) };

        uint32_t idx[24];
        uint64_t faces = 0, fronts = 0, survivors = 0, mismatches = 0;

        for (int batch = 0; batch < 20000; ++batch)
        {
            for (uint32_t k = 0; k < 24; ++k) idx[k] = rng() % kVerts;

            for (const Viewport& vp : vps)
            {
                const FaceMasks8 m = ClassifyFaces8(rv.data(), idx, vp);
                for (uint32_t j = 0; j < 8; ++j)
                {
                    const RasterVertex& a = rv[idx[j * 3]];
                    const RasterVertex& b = rv[idx[j * 3 + 1]];
                    const RasterVertex& c = rv[idx[j * 3 + 2]];
                    const bool front = !IsBackFacing(EdgeFunction(a, b, c));
                    const bool surv = front && !TightPixelBox(a, b, c, vp).Empty();
                    ++faces; fronts += front; survivors += surv;

                    if (front != bool((m.front >> j) & 1u) || surv != bool((m.survive >> j) & 1u))
                    {
                        if (mismatches < 5)
                            Logger::error("[B4] desaccord : a(" + std::to_string(a.x) + "," + std::to_string(a.y)
                                + ") b(" + std::to_string(b.x) + "," + std::to_string(b.y)
                                + ") c(" + std::to_string(c.x) + "," + std::to_string(c.y) + ")");
                        ++mismatches;
                    }
                }
            }
        }

        // CONTRE-EXEMPLE du test qui ne teste rien : les trois issues doivent exister.
        const bool nonTrivial = fronts > 0 && fronts < faces && survivors > 0 && survivors < fronts;
        Logger::info("[B4] " + std::to_string(faces) + " faces, " + std::to_string(mismatches) + " desaccords");
        return mismatches == 0 && nonTrivial;
    }
#endif
}