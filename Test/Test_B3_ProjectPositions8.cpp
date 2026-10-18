#include "pch.h"
#include <random>
#include <cstring>
#include <vector>
#include "geometry/MeshClass.h"
#include "rendering/Viewport.h"
#include "rendering/Rasterizer.h"     // MulRow
#include "rendering/VertexStage.h"
#include "core/Logger.h"

namespace LV3::Tests
{
#if LV3_DEBUG
    // B3 : ProjectPositions8 == ProjectClip(MulRow(...)) AU BIT PRES, + cuisson SoA (bourrage, alignement).
    bool Test_B3_ProjectPositions8()
    {
        std::mt19937 rng(42);   // graine FIXE : un echec se rejoue a l'identique
        std::uniform_real_distribution<float> u(-1.0f, 1.0f), big(-50.0f, 50.0f), m33d(151.0f, 400.0f);

        // ── Partie 1 : cuisson. 13 sommets -> 16, couloirs 13..15 = sommet 12, alignement 64 o.
        MeshClass mesh;
        for (int i = 0; i < 13; ++i) mesh.AddVertex({ big(rng), big(rng), big(rng) });
        mesh.BakePositionsSoA();
        const PositionsSoA& s = mesh.positionsSoA;
        bool bakeOk = s.count == 13 && s.PaddedCount() == 16
            && (reinterpret_cast<uintptr_t>(s.x.data()) % 64) == 0
            && (reinterpret_cast<uintptr_t>(s.y.data()) % 64) == 0
            && (reinterpret_cast<uintptr_t>(s.z.data()) % 64) == 0;
        for (size_t i = 13; i < 16 && bakeOk; ++i)
            bakeOk = s.x[i] == s.x[12] && s.y[i] == s.y[12] && s.z[i] == s.z[12];

        // ── Partie 2 : noyau contre scalaire, matrices et viewports varies.
        // |x|,|y|,|z| <= 50 et |m03|,|m13|,|m23| <= 1  =>  |somme| <= 150 < m33  =>  w > 0 PARTOUT
        // (l'assertion w > 0 du noyau ne doit jamais tomber ici).
        constexpr size_t kN = 4096;
        SimdVector<float> px(kN), py(kN), pz(kN);
        std::vector<Vec3f> aos(kN);
        RasterSpaceBuffer out;

        Viewport split = Viewport::FullScreen(1536, 900);
        split.x = 768; split.y = 450; split.width = 768; split.height = 450;
        const Viewport vps[] = { Viewport::FullScreen(1536, 900), split, Viewport::FullScreen(1, 1) };

        uint64_t verts = 0, mismatches = 0, outside = 0;
        for (int it = 0; it < 300; ++it)
        {
            Matrix44f m;
            for (int r = 0; r < 4; ++r)
                for (int c = 0; c < 4; ++c)
                    m[r][c] = u(rng) * ((rng() % 4) ? 1.0f : 37.0f);
            m[0][3] = u(rng); m[1][3] = u(rng); m[2][3] = u(rng); m[3][3] = m33d(rng);

            for (size_t i = 0; i < kN; ++i)
            {
                aos[i] = (rng() % 8) ? Vec3f{ big(rng), big(rng), big(rng) }
                : Vec3f{ std::ldexp(u(rng), int(rng() % 40) - 34), u(rng), -0.0f };  // petits, -0
                px[i] = aos[i].x; py[i] = aos[i].y; pz[i] = aos[i].z;
            }

            const Viewport& vp = vps[it % 3];
            ProjectPositions8(m, vp, px.data(), py.data(), pz.data(), kN, out.Data());

            for (size_t i = 0; i < kN; ++i)
            {
                const RasterVertex ref = ProjectClip(MulRow(m, aos[i]), vp);
                const RasterVertex& v = out.Data()[i];
                ++verts;
                outside += (ref.x < 0.0f || ref.x > 1536.0f);
                if (std::memcmp(&ref, &v, sizeof(RasterVertex)) != 0)
                {
                    if (mismatches < 5)
                        Logger::error("[B3] desaccord sommet " + std::to_string(i) + " : x " + std::to_string(v.x)
                            + " / " + std::to_string(ref.x) + ", invW " + std::to_string(v.invW) + " / " + std::to_string(ref.invW));
                    ++mismatches;
                }
            }
        }

        // CONTRE-EXEMPLE du test qui ne teste rien : des sommets dedans ET dehors.
        const bool nonTrivial = outside > 0 && outside < verts;
        Logger::info("[B3] " + std::to_string(verts) + " sommets, " + std::to_string(mismatches)
            + " desaccords, cuisson " + (bakeOk ? "OK" : "KO"));
        return bakeOk && mismatches == 0 && nonTrivial;
    }
#endif
}