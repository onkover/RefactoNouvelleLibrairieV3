#include "pch.h"
#include <cmath>
#include <cstring>
#include <limits>
#include <random>
#include "rendering/Viewport.h"
#include "rendering/Rasterizer.h"     // MulRow
#include "rendering/VertexStage.h"
#include "core/Logger.h"
#include "TestRandom.h"               // tirages portables MSVC / g++

// ════════════════════════════════════════════════════════════
//  TNR D-a1d : accrochage par nombre magique
//   (1) SnapToSubpixel == reference INDEPENDANTE (double, nearbyint, zero canonique +0) :
//       1 motif de bits sur 127 dans |v| < 2^15 (le passage du domaine 2^14 est traverse),
//       toutes les egalites k/256 + 1/512 sur [-64, 2048[ px et leurs deux voisins, bornes,
//       repli round jusqu'a 2^21 px, NaN ;
//   (2) jamais -0, idempotence ;
//   (3) ProjectPositions8 == ProjectPositions (scalaire) au bit pres sur des sommets poses
//       sur les bords gauche et haut (raster -0,003 .. 0 : l'ancien round rendait -0).
// ════════════════════════════════════════════════════════════
namespace LV3::Tests
{
#if LV3_DEBUG && LV3_RASTER_FIXED
    namespace
    {
        uint32_t Bits(float f) { uint32_t u; std::memcpy(&u, &f, 4); return u; }
        float FromBits(uint32_t u) { float f; std::memcpy(&f, &u, 4); return f; }

        // Reference sans nombre magique ni intrinseque : v*256 exact en double, entier le plus
        // proche au pair (mode par defaut), /256 exact, retour en float exact (|r| < 2^24 / 256 ou
        // deja multiple de 2^-8). Zero CANONIQUE : +0 (contrat D-a1d).
        float RefSnap(float v)
        {
            const float f = float(std::nearbyint(double(v) * 256.0) / 256.0);
            return f == 0.0f ? 0.0f : f;
        }
    }

    bool Test_Da1d_Snap()
    {
        bool ok = true;

        // ── (1) et (2) : la fonction scalaire contre la reference ─────────────
        uint64_t n = 0, bad = 0, minusZero = 0, notIdem = 0;
        auto check = [&](float v)
        {
            const float s = SnapToSubpixel(v), r = RefSnap(v);
            ++n;
            if (Bits(s) != Bits(r))
            {
                if (bad < 5) Logger::error("[D-a1d] v = " + std::to_string(v) + " : snap "
                    + std::to_string(s) + " / reference " + std::to_string(r));
                ++bad;
            }
            minusZero += (Bits(s) == 0x80000000u) ? 1u : 0u;
            notIdem += (Bits(SnapToSubpixel(s)) != Bits(s)) ? 1u : 0u;
        };

        for (uint32_t b = 0; b < 0x47000000u; b += 127)            // |v| < 2^15, les deux signes
        {
            check(FromBits(b));
            check(FromBits(b | 0x80000000u));
        }
        for (int k = -64 * 256; k < 2048 * 256; ++k)                // egalites exactes : le pair decide
        {
            const float t = (float(k) + 0.5f) * kSubpixelInv;
            check(t);
            check(std::nextafter(t, -1.0e9f));
            check(std::nextafter(t, 1.0e9f));
        }
        for (const float e : { 0.0f, -0.0f, -1.0f / 512.0f, -3.0f / 512.0f, 16383.99609375f, 16384.0f,
                               -16384.0f, 32767.998f, 32768.0f, 1048576.0f, -1048576.3f })
        {
            check(e);
            check(std::nextafter(e, 0.0f));
            check(std::nextafter(e, 1.0e30f));
        }
        {   // repli round (chemin clippe, sommets hors ecran) : [2^14, 2^21] px
            std::mt19937 rng(11u);
            TestRand01 U;
            for (int i = 0; i < 200000; ++i)
                check((U(rng) < 0.5f ? -1.0f : 1.0f) * (16384.0f + U(rng) * 2080768.0f));
        }
        const bool nanOk = std::isnan(SnapToSubpixel(std::numeric_limits<float>::quiet_NaN()));

        Logger::info("[D-a1d] SnapToSubpixel : " + std::to_string(n) + " valeurs, " + std::to_string(bad)
            + " differences, " + std::to_string(minusZero) + " -0, " + std::to_string(notIdem)
            + " non idempotentes, NaN " + (nanOk ? "OK" : "KO"));
        ok &= (bad == 0 && minusZero == 0 && notIdem == 0 && nanOk);

        // ── (3) le noyau AVX2 contre le scalaire, sur les bords gauche et haut ────────
        {
            // mvp = identite : clip = (x, y, z, 1), w = 1 > 0 (contrat Inside). Viewport 768 x 450 en (0, 0) :
            //   xr = (x*0.5 + 0.5) * 768  ->  x juste sous -1 donne un raster juste sous 0 ;
            //   yr = (0.5 - y*0.5) * 450  ->  y juste au-dessus de +1, idem en y.
            Matrix44f m;
            for (int r = 0; r < 4; ++r)
                for (int c = 0; c < 4; ++c)
                    m[r][c] = (r == c) ? 1.0f : 0.0f;
            const Viewport vp{ 0, 0, 768, 450 };

            constexpr size_t kN = 64;                                // multiple de 8
            SimdVector<float> px(kN), py(kN), pz(kN);
            std::vector<Vec3f> aos(kN);
            float x = -1.0f, y = 1.0f;
            for (size_t i = 0; i < kN; ++i)                          // 64 float consecutifs vers l'exterieur
            {
                aos[i] = { x, y, 0.5f };
                px[i] = x; py[i] = y; pz[i] = 0.5f;
                x = std::nextafter(x, -2.0f);
                y = std::nextafter(y, 2.0f);
            }

            RasterSpaceBuffer simd, scal;
            ProjectPositions8(m, vp, px.data(), py.data(), pz.data(), kN, simd.Data());
            ProjectPositions(m, vp, aos.data(), kN, scal.Data());

            uint64_t diff = 0, zeros = 0, negZeros = 0;
            for (size_t i = 0; i < kN; ++i)
            {
                const RasterVertex& a = simd.Data()[i];
                const RasterVertex& b = scal.Data()[i];
                diff += std::memcmp(&a, &b, sizeof(RasterVertex)) != 0 ? 1u : 0u;
                zeros += (Bits(a.x) == 0u ? 1u : 0u) + (Bits(a.y) == 0u ? 1u : 0u);
                negZeros += (Bits(a.x) == 0x80000000u ? 1u : 0u) + (Bits(a.y) == 0x80000000u ? 1u : 0u);
            }
            Logger::info("[D-a1d] bords : " + std::to_string(kN) + " sommets, " + std::to_string(diff)
                + " differences AVX2 / scalaire, " + std::to_string(zeros) + " coordonnees +0, "
                + std::to_string(negZeros) + " -0");
            ok &= (diff == 0 && negZeros == 0 && zeros > 0);
        }
        return ok;
    }
#else
    bool Test_Da1d_Snap() { return true; }    // D-a1d inactif (LV3_RASTER_FIXED = 0) ou Release
#endif
}
