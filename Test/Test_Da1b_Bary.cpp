#include "pch.h"
#include <random>
#include <cstring>
#include <cmath>
#include "rendering/Viewport.h"
#include "rendering/Rasterizer.h"
#include "rendering/PixelStage8.h"
#include "core/Logger.h"
#include "TestRandom.h"   // tirages portables MSVC / g++

// ════════════════════════════════════════════════════════════
//  TNR D-a1b : barycentriques tirées des E' entiers
//   (1) reconstruction EXACTE : 256 E' + c == E calculé directement en int64, en tout pixel ;
//   (2) précision : |b - E / aire| <= 5 * 2^-24 * b (cinq arrondis au plus), comparée à D-a1 ;
//   (3) Int64x8ToFloat == float(int64) du scalaire, au bit près (voies 64 bits == scalaire).
//  (L'égalité C3 == noyau au bit près est vérifiée par Test_C3_Pixel1, inchangé.)
// ════════════════════════════════════════════════════════════
namespace LV3::Tests
{
#if LV3_DEBUG && LV3_RASTER_FIXED
    namespace
    {
        // E en un centre de pixel, DIRECTEMENT en int64 (forme antisymétrique de C2).
        int64_t EdgeDirect(int64_t xa, int64_t ya, int64_t xb, int64_t yb, int32_t x, int32_t y)
        {
            const int64_t px = (int64_t(x) << kSubpixelBits) + 128, py = (int64_t(y) << kSubpixelBits) + 128;
            return (xa - px) * (yb - py) - (ya - py) * (xb - px);
        }

        std::string Sci(double v) { char b[32]; std::snprintf(b, sizeof b, "%.3g", v); return b; }

        struct BarySink
        {
            std::vector<int32_t>* xs; std::vector<int32_t>* ys; std::vector<BarycentricWeights>* bs;
            int32_t y = 0;
            void BeginRow(int32_t r) noexcept { y = r; }
            void Pixel(int32_t x, const BarycentricWeights& b) noexcept { xs->push_back(x); ys->push_back(y); bs->push_back(b); }
        };
    }

    bool Test_Da1b_Bary()
    {
        bool ok = true;
        const Viewport vp = Viewport::FullScreen(1536, 900);

        // ── (1) + (2) ─────────────────────────────────────────────
        {
            std::mt19937 rng(17u);
            TestRand01 U;
            uint64_t tris = 0, probes = 0, badRebuild = 0, pixels = 0, badBound = 0;
            double maxRelB = 0.0, maxRelA = 0.0;              // B = D-a1b, A = formule flottante de D-a1
            std::vector<int32_t> xs, ys; std::vector<BarycentricWeights> bs;
            for (int i = 0; i < 4000; ++i)
            {
                const float size = U(rng) < 0.5f ? 2.0f + 30.0f * U(rng) : 30.0f + 270.0f * U(rng);
                const float cx = 1536.0f * U(rng), cy = 900.0f * U(rng);
                Vec2f v[3];
                for (auto& p : v) p = { SnapToSubpixel(cx + (U(rng) - 0.5f) * size), SnapToSubpixel(cy + (U(rng) - 0.5f) * size) };
                if (U(rng) < 0.05f) v[0] = { SnapToSubpixel(v[0].x + 3.0e5f * (U(rng) - 0.5f)), SnapToSubpixel(v[0].y + 3.0e5f * (U(rng) - 0.5f)) };
                TriangleSetup s; EdgeSetupFx fx;
                if (!SetupTriangleFx(v[0], v[1], v[2], vp, s, fx)) continue;
                ++tris;

                // Sommets entiers APRÈS échange, dans l'ordre des arêtes de C1.
                const int64_t X0 = ToFixed(s.p0.x), Y0 = ToFixed(s.p0.y), X1 = ToFixed(s.p1.x), Y1 = ToFixed(s.p1.y);
                const int64_t X2 = ToFixed(s.p2.x), Y2 = ToFixed(s.p2.y);
                const int64_t area = (X0 - X2) * (Y1 - Y2) - (Y0 - Y2) * (X1 - X2);
                auto E = [&](int k, int32_t x, int32_t y)
                    {
                        return k == 0 ? EdgeDirect(X1, Y1, X2, Y2, x, y) : k == 1 ? EdgeDirect(X2, Y2, X0, Y0, x, y) : EdgeDirect(X0, Y0, X1, Y1, x, y);
                    };

                // (1) 32 pixels tirés dans la boîte : E' par pas entiers, puis 256 E' + c == E.
                for (int j = 0; j < 32; ++j)
                {
                    const int32_t dx = int32_t(U(rng) * float(s.bb.x1 - s.bb.x0 - 1) + 0.5f);
                    const int32_t dy = int32_t(U(rng) * float(s.bb.y1 - s.bb.y0 - 1) + 0.5f);
                    for (int k = 0; k < 3; ++k)
                    {
                        const int64_t ep = fx.e[k] + fx.a[k] * dx + fx.b[k] * dy;
                        const int64_t c = int64_t(fx.ofs[k] * kSubpixelScale);      // exact
                        ++probes;
                        if ((ep << kSubpixelBits) + c != E(k, s.bb.x0 + dx, s.bb.y0 + dy) || c < 0 || c > 256) ++badRebuild;
                    }
                }

                // (2) tous les pixels couverts : b contre E / aire exact (long double, 64 bits de mantisse).
                xs.clear(); ys.clear(); bs.clear();
                BarySink sink{ &xs, &ys, &bs };
                (void)ForEachCoveredPixelFx(s, fx, sink);
                for (size_t p = 0; p < bs.size(); ++p)
                {
                    ++pixels;
                    const float bB[3] = { bs[p].w0, s.flipped ? bs[p].w2 : bs[p].w1, s.flipped ? bs[p].w1 : bs[p].w2 };
                    const float px = float(xs[p]) + 0.5f, py = float(ys[p]) + 0.5f;
                    const float wA[3] = { EdgeFunction(s.p1, s.p2, px, py), EdgeFunction(s.p2, s.p0, px, py), EdgeFunction(s.p0, s.p1, px, py) };
                    for (int k = 0; k < 3; ++k)
                    {
                        const long double ex = (long double)E(k, xs[p], ys[p]) / (long double)area;
                        if (ex <= 0.0L) continue;                 // sur l'arête : erreur relative indéfinie
                        const double relB = double(std::fabs((long double)bB[k] - ex) / ex);
                        const double relA = double(std::fabs((long double)(wA[k] * s.invArea) - ex) / ex);
                        maxRelB = std::max(maxRelB, relB);
                        maxRelA = std::max(maxRelA, relA);
                        if (relB > 5.0 * 0x1p-24) ++badBound;
                    }
                }
            }
            Logger::info("[D-a1b] " + std::to_string(tris) + " triangles, " + std::to_string(probes) + " sondes E' -> E, "
                + std::to_string(badRebuild) + " reconstructions fausses ; " + std::to_string(pixels) + " pixels couverts, "
                + std::to_string(badBound) + " hors borne 5 ulp ; erreur relative max D-a1b " + Sci(maxRelB)
                + " contre D-a1 " + Sci(maxRelA));
            ok &= (badRebuild == 0 && badBound == 0 && tris > 0 && pixels > 0);
        }

        // ── (3) int64 -> float : voies 64 bits == scalaire ─────────────
        {
            std::mt19937_64 rng(23u);
            uint64_t bad = 0, n = 0;
            const int64_t edge[8] = { 0, -1, (1LL << 24) + 1, -(1LL << 24) - 3, (1LL << 50) - 1, -(1LL << 50) + 1, 16777217LL * 257, -33554435LL };
            for (int i = 0; i < 200000; ++i)
            {
                alignas(32) int64_t v[8];
                for (int k = 0; k < 8; ++k)
                {
                    const int sh = 1 + int(rng() % 50);                           // |v| < 2^50
                    v[k] = i == 0 ? edge[k] : int64_t(rng() >> (64 - sh)) * ((rng() & 1) ? 1 : -1);
                }
                alignas(32) float f[8];
                _mm256_store_ps(f, Int64x8ToFloat(_mm256_load_si256(reinterpret_cast<const __m256i*>(v)),
                    _mm256_load_si256(reinterpret_cast<const __m256i*>(v + 4))));
                for (int k = 0; k < 8; ++k, ++n)
                {
                    const float ref = float(v[k]);
                    if (std::memcmp(&ref, &f[k], 4) != 0) ++bad;
                }
            }
            Logger::info("[D-a1b] int64 -> float : " + std::to_string(n) + " valeurs, " + std::to_string(bad) + " differences");
            ok &= (bad == 0);
        }
        return ok;
    }
#else
    bool Test_Da1b_Bary() { return true; }    // D-a1 inactif (LV3_RASTER_FIXED = 0) ou Release
#endif
}