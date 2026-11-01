#include "pch.h"
#include <random>
#include <cstring>
#include <limits>
#include "rendering/Viewport.h"
#include "rendering/Rasterizer.h"
#include "rendering/Fragment.h"
#include "rendering/VertexStage.h"
#include "core/Logger.h"
#include "TestRandom.h"   // tirages portables MSVC / g++

// ════════════════════════════════════════════════════════════
//  TNR D-a1 : arêtes en virgule fixe (Giesen)
//   (1) scalaire int64 == AVX2 int32 == aiguillage moteur == AVX2 int64 forcé, AU BIT PRÈS
//       (5 shaders + couverture brute, 3 vues), avec des triangles larges et hors garde ;
//   (2) boîte entière de SetupTriangleFx == TightPixelBox (rejet 3b) sur sommets accrochés,
//       et règle top-left entière == IsTopLeft ;
//   (3) étanchéité : éventails à arêtes partagées, 0 trou, 0 doublon ;
//   (4) l'étage sommets accroche : ProjectClip sort des multiples de 1/256.
// ════════════════════════════════════════════════════════════
namespace LV3::Tests
{
#if LV3_DEBUG && LV3_RASTER_FIXED
    namespace
    {
        constexpr int32_t kW = 317, kH = 203;                 // non multiples de 8

        struct Target
        {
            static constexpr int32_t kPitchPx = kW + 11;
            std::vector<uint32_t> pixels = std::vector<uint32_t>(size_t(kPitchPx) * kH, 0u);
            FrameBuffer fb;
            DepthBuffer db;
            Target() { fb.Bind(pixels.data(), kPitchPx * 4, kW, kH); db.Resize(kW, kH); db.Clear(); }
        };

        struct Tri { Vec2f v[3]; float z[3], invW[3]; Color col; };

        // Famille de la TNR C2, plus : un sommet très lointain (clippé au near : voies 64 bits
        // ou arête acceptée), et un sommet hors garde (repli flottant C2).
        std::vector<Tri> MakeTriangles(uint32_t seed, int count)
        {
            std::mt19937 rng(seed);
            TestRand01 U;
            std::vector<Tri> tris;
            tris.reserve(size_t(count));
            for (int i = 0; i < count; ++i)
            {
                const float u = U(rng);
                const float size = u < 0.3f ? 0.3f + 1.2f * U(rng)
                    : u < 0.5f ? 1.5f + 6.5f * U(rng)
                    : u < 0.85f ? 8.0f + 22.0f * U(rng)
                    : 30.0f + 270.0f * U(rng);
                const float cx = -20.0f + (kW + 40.0f) * U(rng), cy = -20.0f + (kH + 40.0f) * U(rng);
                Tri t{};
                for (int k = 0; k < 3; ++k)
                {
                    t.v[k] = { cx + (U(rng) - 0.5f) * size, cy + (U(rng) - 0.5f) * size };
                    const float w = 0.5f + 40.0f * U(rng);
                    t.invW[k] = 1.0f / w;
                    t.z[k] = 0.1f / w;
                }
                const float e = U(rng);
                if (e < 0.20f) for (auto& p : t.v) p = { std::floor(p.x) + 0.5f, std::floor(p.y) + 0.5f };
                else if (e < 0.30f) t.v[1].y = t.v[0].y;
                else if (e < 0.35f) t.v[2] = { 0.5f * (t.v[0].x + t.v[1].x), 0.5f * (t.v[0].y + t.v[1].y) };
                else if (e < 0.37f) t.z[0] = std::numeric_limits<float>::quiet_NaN();
                else if (e < 0.45f)                                // sommet LOINTAIN : voies 64 bits
                {
                    const float d = U(rng) < 0.5f ? 4.0e4f : 6.0e5f;
                    t.v[0] = { t.v[0].x + (U(rng) - 0.5f) * d, t.v[0].y + (U(rng) - 0.5f) * d };
                }
                else if (e < 0.47f) t.v[1] = { 3.0e6f, -2.0e6f };   // HORS GARDE : repli C2
                // Contrat de l'étage sommets : x et y sur la grille des 1/256 de pixel.
                for (auto& p : t.v) p = { SnapToSubpixel(p.x), SnapToSubpixel(p.y) };
                t.col = MakeColor(uint8_t(rng()), uint8_t(rng()), uint8_t(rng()));
                tris.push_back(t);
            }
            return tris;
        }

        void Fill(FragmentContext& c, const Tri& t)
        {
            c.color = t.col;
            c.z0 = t.z[0];       c.z1 = t.z[1];       c.z2 = t.z[2];
            c.invW0 = t.invW[0]; c.invW1 = t.invW[1]; c.invW2 = t.invW[2];
            c.depthDisplayRange = 25.0f;
        }

        // Shader de TEST : compte les passages par pixel (copie de la TNR C2).
        struct ShaderCount
        {
            static LV3_FORCEINLINE bool Shade(const FragmentContext&, const BarycentricWeights&,
                float&, uint32_t& pixel) noexcept
            {
                ++pixel;
                return true;
            }
            static constexpr bool kHasShade8 = true;
            static LV3_FORCEINLINE uint32_t Shade8(const Uniforms8&, __m256, __m256, __m256,
                float*, uint32_t* pixel, __m256 cover) noexcept
            {
                const __m256i m = _mm256_castps_si256(cover);
                const __m256i v = _mm256_maskload_epi32(reinterpret_cast<const int*>(pixel), m);
                _mm256_maskstore_epi32(reinterpret_cast<int*>(pixel), m, _mm256_add_epi32(v, _mm256_set1_epi32(1)));
                return uint32_t(_mm256_movemask_ps(cover));
            }
        };

        size_t Diff(Target& a, Target& b, size_t& depthDiff)
        {
            size_t colorDiff = 0; depthDiff = 0;
            for (int32_t y = 0; y < kH; ++y)
            {
                for (int32_t x = 0; x < kW; ++x)
                {
                    const size_t i = size_t(y) * Target::kPitchPx + size_t(x);
                    if (a.pixels[i] != b.pixels[i]) ++colorDiff;
                    if (std::memcmp(&a.db.Row(y)[x], &b.db.Row(y)[x], sizeof(float)) != 0) ++depthDiff;
                }
                for (int32_t x = kW; x < Target::kPitchPx; ++x)      // le pitch n'est jamais touché
                    if (b.pixels[size_t(y) * Target::kPitchPx + size_t(x)] != 0u) ++colorDiff;
            }
            return colorDiff;
        }

        // Scalaire (A) contre AVX2 forcé (B), aiguillage moteur (C), AVX2 int64 forcé (D).
        template<class Shader>
        bool CheckShader(const char* name, const std::vector<Tri>& tris, const Viewport& vp)
        {
            Target A, B, C, D;
            FragmentContext ca{}, cb{}, cc{}, cd{};
            ca.fb = &A.fb; ca.db = &A.db;  cb.fb = &B.fb; cb.db = &B.db;
            cc.fb = &C.fb; cc.db = &C.db;  cd.fb = &D.fb; cd.db = &D.db;
            for (const Tri& t : tris)
            {
                Fill(ca, t); RasterizeTriangleT<Shader, kScalarOnly>(t.v[0], t.v[1], t.v[2], vp, ca);
                Fill(cb, t); RasterizeTriangleT<Shader, kSimdAlways>(t.v[0], t.v[1], t.v[2], vp, cb);
                Fill(cc, t); RasterizeTriangleT<Shader>(t.v[0], t.v[1], t.v[2], vp, cc);
                Fill(cd, t); RasterizeTriangleT<Shader, kSimdAlways, EFixedLanes::Force64>(t.v[0], t.v[1], t.v[2], vp, cd);
            }
            size_t d1, d2, d3;
            const size_t c1 = Diff(A, B, d1), c2 = Diff(A, C, d2), c3 = Diff(A, D, d3);
            const bool ok = (c1 | d1 | c2 | d2 | c3 | d3) == 0;
            if (!ok)
                Logger::error(std::string("[D-a1] ") + name + " : AVX2 " + std::to_string(c1) + "/" + std::to_string(d1)
                    + " ; moteur " + std::to_string(c2) + "/" + std::to_string(d2)
                    + " ; int64 force " + std::to_string(c3) + "/" + std::to_string(d3) + " (couleurs/profondeurs)");
            return ok;
        }

        struct CountSink
        {
            std::vector<uint8_t>* c; int32_t y = 0;
            void BeginRow(int32_t r) noexcept { y = r; }
            void Pixel(int32_t x, const BarycentricWeights&) noexcept { (*c)[size_t(y) * kW + size_t(x)]++; }
        };
    }

    bool Test_Da1_Fixed()
    {
        bool ok = true;

        // ── (1) quatre chemins, un seul résultat ───────────────────────
        {
            const std::vector<Tri> tris = MakeTriangles(11u, 6000);
            const Viewport views[3] = { Viewport::FullScreen(kW, kH), Viewport{ 37, 21, 203, 150 }, Viewport{ kW - 61, 9, 61, 180 } };
            for (const Viewport& vp : views)
            {
                ok &= CheckShader<ShaderCount>("Couverture", tris, vp);
                ok &= CheckShader<ShaderSolid>("Solid", tris, vp);
                ok &= CheckShader<ShaderBarycentric>("Barycentric", tris, vp);
                ok &= CheckShader<ShaderLinearDepth>("LinearDepth", tris, vp);
                ok &= CheckShader<ShaderDepth>("Depth", tris, vp);

                // CONTRE-EXEMPLE du test qui ne teste rien : chaque chemin a servi.
                uint64_t fixed = 0, wide = 0, guard = 0, accepted = 0;
                for (const Tri& t : tris)
                {
                    if (!InFixedGuard(t.v[0], t.v[1], t.v[2])) { ++guard; continue; }
                    TriangleSetup s; EdgeSetupFx fx;
                    if (!SetupTriangleFx(t.v[0], t.v[1], t.v[2], vp, s, fx)) continue;
                    ++fixed;
                    PrepareLanesFx(s, fx);
                    wide += fx.wide ? 1u : 0u;
                    for (int k = 0; k < 3; ++k) accepted += (fx.a[k] == 0 && fx.b[k] == 0 && fx.e[k] == 0) ? 1u : 0u;
                }
                Logger::info("[D-a1] vue " + std::to_string(vp.width) + "x" + std::to_string(vp.height) + " : "
                    + std::to_string(fixed) + " triangles en virgule fixe, " + std::to_string(wide) + " en voies 64 bits, "
                    + std::to_string(guard) + " hors garde, " + std::to_string(accepted) + " aretes acceptees");
                if (fixed == 0 || wide == 0 || guard == 0 || accepted == 0)
                {
                    Logger::error("[D-a1] test trivial : un chemin n'a pas servi");
                    ok = false;
                }
            }
        }

        // ── (2) boîte et top-left entiers == flottants, sur sommets accrochés ──
        {
            std::mt19937 rng(5u);
            TestRand01 U;
            const Viewport vp = Viewport::FullScreen(1536, 900);
            uint64_t n = 0, badBox = 0, badTl = 0;
            for (int i = 0; i < 300000; ++i)
            {
                const float sc = U(rng) < 0.5f ? 2000.0f : (U(rng) < 0.5f ? 70000.0f : 900000.0f);
                Vec2f v[3];
                for (auto& p : v) p = { SnapToSubpixel((U(rng) - 0.3f) * sc), SnapToSubpixel((U(rng) - 0.3f) * sc) };
                TriangleSetup s; EdgeSetupFx fx;
                if (!SetupTriangleFx(v[0], v[1], v[2], vp, s, fx)) continue;
                ++n;
                const PixelBox fb = TightPixelBox(v[0], v[1], v[2], vp);
                if (fb.x0 != s.bb.x0 || fb.x1 != s.bb.x1 || fb.y0 != s.bb.y0 || fb.y1 != s.bb.y1) ++badBox;
                if (IsTopLeft(s.p1, s.p2) != s.tl0 || IsTopLeft(s.p2, s.p0) != s.tl1 || IsTopLeft(s.p0, s.p1) != s.tl2) ++badTl;
            }
            Logger::info("[D-a1] boite et top-left : " + std::to_string(n) + " triangles, "
                + std::to_string(badBox) + " boites et " + std::to_string(badTl) + " top-left differents");
            ok &= (badBox == 0 && badTl == 0 && n > 0);
        }

        // ── (3) étanchéité : éventails de 8 triangles autour d'un centre ──
        {
            std::mt19937 rng(7u);
            TestRand01 U;
            const Viewport vp = Viewport::FullScreen(kW, kH);
            uint64_t inner = 0, holes = 0, doubles = 0;
            std::vector<uint8_t> cnt(size_t(kW) * kH);
            for (int f = 0; f < 600; ++f)
            {
                std::fill(cnt.begin(), cnt.end(), uint8_t(0));
                const Vec2f c{ SnapToSubpixel(60.0f + 200.0f * U(rng)), SnapToSubpixel(50.0f + 100.0f * U(rng)) };
                const float R = 10.0f + 40.0f * U(rng);
                Vec2f ring[8];
                for (int k = 0; k < 8; ++k)
                {
                    const float a = (float(k) + 0.6f * (U(rng) - 0.5f)) * 0.785398f;
                    ring[k] = { SnapToSubpixel(c.x + R * std::cos(a)), SnapToSubpixel(c.y + R * std::sin(a)) };
                }
                for (int k = 0; k < 8; ++k)
                {
                    TriangleSetup s; EdgeSetupFx fx; CountSink sink{ &cnt };
                    if (SetupTriangleFx(c, ring[k], ring[(k + 1) % 8], vp, s, fx)) (void)ForEachCoveredPixelFx(s, fx, sink);
                }
                for (int32_t y = 0; y < kH; ++y)
                    for (int32_t x = 0; x < kW; ++x)
                    {
                        const size_t i = size_t(y) * kW + size_t(x);
                        const float dx = float(x) + 0.5f - c.x, dy = float(y) + 0.5f - c.y;
                        if (dx* dx + dy * dy < 0.49f * R * R) { ++inner; holes += cnt[i] == 0 ? 1u : 0u; }
                        doubles += cnt[i] > 1 ? 1u : 0u;
                    }
            }
            Logger::info("[D-a1] eventails : " + std::to_string(inner) + " pixels interieurs, "
                + std::to_string(holes) + " trous, " + std::to_string(doubles) + " doublons");
            ok &= (holes == 0 && doubles == 0 && inner > 0);
        }

        // ── (4) l'étage sommets accroche ─────────────────────────────
        {
            std::mt19937 rng(3u);
            TestRand01 U;
            const Viewport vp{ 3, 5, 768, 450 };
            uint64_t off = 0;
            for (int i = 0; i < 100000; ++i)
            {
                const Vec4f c{ (U(rng) - 0.5f) * 20.0f, (U(rng) - 0.5f) * 20.0f, 0.1f, 0.1f + 30.0f * U(rng) };
                const RasterVertex r = ProjectClip(c, vp);
                if (r.x * kSubpixelScale != std::nearbyint(r.x * kSubpixelScale)
                    || r.y * kSubpixelScale != std::nearbyint(r.y * kSubpixelScale)) ++off;
            }
            if (off != 0) Logger::error("[D-a1] " + std::to_string(off) + " sommets hors de la grille 1/256");
            ok &= (off == 0);
        }
        return ok;
    }
#else
    bool Test_Da1_Fixed() { return true; }    // D-a1 inactif (LV3_RASTER_FIXED = 0) ou Release
#endif
}