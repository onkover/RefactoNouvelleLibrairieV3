#include "pch.h"
#include <random>
#include <cstring>
#include <limits>
#include "rendering/Viewport.h"
#include "rendering/Rasterizer.h"
#include "rendering/Fragment.h"
#include "core/Logger.h"
#include "TestRandom.h"   // tirages portables MSVC / g++

// ════════════════════════════════════════════════════════════
//  TNR D-a1c : petits triangles sur les blocs FLOTTANTS de C2
//   (1) MÊME IMAGE au bit près (couleur ET profondeur, 5 shaders + couverture brute) :
//       référence = Giesen pour tous (kSmallFloatOff), contre W* = 16 (plafond exact) et
//       contre la configuration de production (kSmallFloatPx). Chemin AVX2 forcé (kSimdAlways) :
//       même les boîtes de 1 ou 2 px passent par l'aiguillage.
//   (2) CONTRE-EXEMPLE intégré : W* = 64 (interdit par config.h) DOIT produire des différences.
//       Une TNR qui ne voit jamais de différence ne prouve rien (règle du lot D-a1d).
//   (3) Bord exact : étendues tirées dans [15,9 ; 16[ px, le pire cas permis.
//  Contrat : sommets ACCROCHÉS au 1/256 px, comme les rend l'étage sommets.
// ════════════════════════════════════════════════════════════
namespace LV3::Tests
{
#if LV3_DEBUG && LV3_RASTER_FIXED
    namespace
    {
        constexpr int32_t kW = 317, kH = 203;                     // largeur et pitch non multiples de 8

        struct Target
        {
            static constexpr int32_t kPitchPx = kW + 11;
            std::vector<uint32_t> pixels = std::vector<uint32_t>(size_t(kPitchPx) * kH, 0u);
            FrameBuffer fb;
            DepthBuffer db;
            Target() { fb.Bind(pixels.data(), kPitchPx * 4, kW, kH); db.Resize(kW, kH); db.Clear(); }
        };

        struct Tri { Vec2f v[3]; float z[3], invW[3]; Color col; };

        // Étendues tirées dans [lo, hi[ px, autour des deux seuils (8 et 16) ; cas particuliers de la TNR C2.
        std::vector<Tri> MakeTriangles(uint32_t seed, int count, float lo, float hi)
        {
            std::mt19937 rng(seed);
            TestRand01 U;
            std::vector<Tri> tris;
            tris.reserve(size_t(count) + size_t(count) / 10);
            for (int i = 0; i < count; ++i)
            {
                const float ext = lo + (hi - lo) * U(rng);
                const float cx = -10.0f + (kW + 20.0f) * U(rng), cy = -10.0f + (kH + 20.0f) * U(rng);
                Tri t{};
                for (int k = 0; k < 3; ++k)
                {
                    t.v[k] = { cx + U(rng) * ext, cy + U(rng) * ext };
                    const float w = 0.5f + 40.0f * U(rng);
                    t.invW[k] = 1.0f / w;
                    t.z[k] = 0.1f / w;
                }
                const float e = U(rng);
                if (e < 0.20f) for (auto& p : t.v) p = { std::floor(p.x) + 0.5f, std::floor(p.y) + 0.5f };  // w == 0 exact
                else if (e < 0.30f) t.v[1].y = t.v[0].y;                                                      // arête horizontale
                else if (e < 0.35f) t.v[2] = { 0.5f * (t.v[0].x + t.v[1].x), 0.5f * (t.v[0].y + t.v[1].y) }; // dégénéré
                else if (e < 0.37f) t.z[0] = std::numeric_limits<float>::quiet_NaN();                          // z NaN
                for (auto& p : t.v) p = { SnapToSubpixel(p.x), SnapToSubpixel(p.y) };   // contrat de l'étage sommets
                t.col = MakeColor(uint8_t(rng()), uint8_t(rng()), uint8_t(rng()));
                tris.push_back(t);
                if (U(rng) < 0.1f)                                                     // sommets tournés : égalités de Z
                    tris.push_back(Tri{ { t.v[1], t.v[2], t.v[0] }, { t.z[1], t.z[2], t.z[0] },
                                        { t.invW[1], t.invW[2], t.invW[0] }, t.col });
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

        struct ShaderCount                                     // couverture brute (copie de la TNR C2)
        {
            static LV3_FORCEINLINE bool Shade(const FragmentContext&, const BarycentricWeights&, float&, uint32_t& pixel) noexcept
            {
                ++pixel; return true;
            }
            static constexpr bool kHasShade8 = true;
            static LV3_FORCEINLINE uint32_t Shade8(const Uniforms8&, __m256, __m256, __m256, float*, uint32_t* pixel, __m256 cover) noexcept
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
                for (int32_t x = kW; x < Target::kPitchPx; ++x)             // remplissage de pitch intact
                    if (b.pixels[size_t(y) * Target::kPitchPx + size_t(x)] != 0u) ++colorDiff;
            }
            return colorDiff;
        }

        // Combien de triangles prennent réellement les blocs flottants pour un W* donné.
        template<int32_t kSmallPx>
        uint32_t CountSmall(const std::vector<Tri>& tris, const Viewport& vp)
        {
            uint32_t n = 0;
            for (const Tri& t : tris)
            {
                if (!InFixedGuard(t.v[0], t.v[1], t.v[2])) continue;
                TriangleSetup s; EdgeSetupFx fx;
                if (SetupTriangleFx(t.v[0], t.v[1], t.v[2], vp, s, fx) && IsSmallFloatTri<kSmallPx>(fx)) ++n;
            }
            return n;
        }

        // Référence Giesen (Off) contre W* = kTest ; rend le nombre de couleurs + profondeurs différentes.
        template<class Shader, int32_t kTest>
        size_t Compare(const std::vector<Tri>& tris, const Viewport& vp, size_t& cd, size_t& dd)
        {
            Target A, B;
            FragmentContext ca{}, cb{};
            ca.fb = &A.fb; ca.db = &A.db; cb.fb = &B.fb; cb.db = &B.db;
            for (const Tri& t : tris)
            {
                Fill(ca, t); RasterizeTriangleT<Shader, kSimdAlways, EFixedLanes::Auto, kSmallFloatOff>(t.v[0], t.v[1], t.v[2], vp, ca);
                Fill(cb, t); RasterizeTriangleT<Shader, kSimdAlways, EFixedLanes::Auto, kTest>(t.v[0], t.v[1], t.v[2], vp, cb);
            }
            cd = Diff(A, B, dd);
#if LV3_RASTER_STATS
            if (ca.depthPassed != cb.depthPassed) ++cd;
#endif
            return cd + dd;
        }

        template<class Shader>
        bool CheckShader(const char* name, const std::vector<Tri>& tris, const Viewport& vp)
        {
            size_t c16 = 0, d16 = 0, cP = 0, dP = 0;
            const size_t bad = Compare<Shader, 16>(tris, vp, c16, d16) + Compare<Shader, kSmallFloatPx>(tris, vp, cP, dP);
            if (bad != 0)
                Logger::error(std::string("[D-a1c] ") + name + " : W*=16 " + std::to_string(c16) + " couleurs / " + std::to_string(d16)
                    + " profondeurs ; production " + std::to_string(cP) + " / " + std::to_string(dP));
            return bad == 0;
        }
    }

    bool Test_Da1c_SmallFloat()
    {
        bool ok = true;
        const Viewport views[3] = { Viewport::FullScreen(kW, kH), Viewport{ 57, 31, 203, 150 }, Viewport{ kW - 61, 11, 61, 180 } };

        // ── (1) même image, étendues 0,3 à 20 px (les deux seuils dedans) ──
        const std::vector<Tri> tris = MakeTriangles(31u, 9000, 0.3f, 20.0f);
        for (const Viewport& vp : views)
        {
            ok &= CheckShader<ShaderCount>("Couverture", tris, vp);
            ok &= CheckShader<ShaderSolid>("Solid", tris, vp);
            ok &= CheckShader<ShaderBarycentric>("Barycentric", tris, vp);
            ok &= CheckShader<ShaderDepth>("Depth", tris, vp);
            ok &= CheckShader<ShaderLinearDepth>("LinearDepth", tris, vp);
            ok &= CheckShader<ShaderLinearDepthWRONG>("LinearDepth_WRONG", tris, vp);
            const uint32_t n8 = CountSmall<kSmallFloatPx>(tris, vp), n16 = CountSmall<16>(tris, vp);
            Logger::info("[D-a1c] vue " + std::to_string(vp.width) + "x" + std::to_string(vp.height) + " : "
                + std::to_string(tris.size()) + " triangles, " + std::to_string(n8) + " petits (W*=" + std::to_string(kSmallFloatPx)
                + "), " + std::to_string(n16) + " (W*=16)");
            ok &= (n16 > 0) && (kSmallFloatPx == 0 || n8 > 0);
        }

        // ── (3) bord exact : étendues dans [15,9 ; 16[ px ──
        {
            const std::vector<Tri> edge = MakeTriangles(37u, 4000, 15.9f, 16.0f);
            const Viewport& vp = views[0];
            ok &= CheckShader<ShaderBarycentric>("Bord 16 px, Barycentric", edge, vp);
            ok &= CheckShader<ShaderLinearDepth>("Bord 16 px, LinearDepth", edge, vp);
            Logger::info("[D-a1c] bord : " + std::to_string(edge.size()) + " triangles, " + std::to_string(CountSmall<16>(edge, vp)) + " petits (W*=16)");
        }

        // ── (2) contre-exemple : W* = 64 DOIT changer l'image (étendues 16 à 60 px) ──
        {
            const std::vector<Tri> big = MakeTriangles(41u, 4000, 16.0f, 60.0f);
            size_t cd = 0, dd = 0;
            const size_t diff = Compare<ShaderBarycentric, 64>(big, views[0], cd, dd);
            Logger::info("[D-a1c] contre-exemple W*=64 : " + std::to_string(cd) + " couleurs et " + std::to_string(dd)
                + " profondeurs differentes (attendu > 0)");
            ok &= (diff > 0);
        }
        return ok;
    }
#else
    bool Test_Da1c_SmallFloat() { return true; }   // D-a1 inactif (LV3_RASTER_FIXED = 0) ou Release
#endif
}
