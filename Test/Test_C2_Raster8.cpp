#include "pch.h"
#include <random>
#include <cstring>
#include <limits>
#include "rendering/Viewport.h"
#include "rendering/Rasterizer.h"
#include "rendering/Fragment.h"
#include "core/Logger.h"
#include "TestRandom.h"   // tirages portables MSVC / g++


namespace LV3::Tests
{
#if LV3_DEBUG
    namespace
    {
        // Largeur NON multiple de 8 : le dernier bloc d'une ligne déborde de la boîte,
        // et pour une boîte qui touche le bord droit, de la LIGNE. Les accès masqués
        // doivent le rendre inoffensif.
        constexpr int32_t kW = 317, kH = 203;

        struct Target
        {
            static constexpr int32_t kPitchPx = kW + 11;          // pitch > largeur, et non multiple de 8
            std::vector<uint32_t> pixels = std::vector<uint32_t>(size_t(kPitchPx) * kH, 0u);
            FrameBuffer fb;
            DepthBuffer db;
            Target() { fb.Bind(pixels.data(), kPitchPx * 4, kW, kH); db.Resize(kW, kH); db.Clear(); }
        };

        struct Tri { Vec2f v[3]; float z[3], invW[3]; Color col; };

        // Même famille que la TNR C1, plus : boîtes larges de 1 à 20 px (le seuil),
        // et des triangles à profondeur NaN (comportement de la dette C1 à conserver).
        std::vector<Tri> MakeTriangles(uint32_t seed, int count)
        {
            std::mt19937 rng(seed);                                // graine FIXE
            //std::uniform_real_distribution<float> U(0.0f, 1.0f);
            TestRand01 U;            // [0, 1), meme suite sur tous les compilateurs

            std::vector<Tri> tris;
            tris.reserve(size_t(count) + size_t(count) / 10);
            for (int i = 0; i < count; ++i)
            {
                const float u = U(rng);
                const float size = u < 0.3f ? 0.3f + 1.2f * U(rng)        // V1 : boîte d'un pixel
                    : u < 0.5f ? 1.5f + 6.5f * U(rng)         // autour du seuil de largeur
                    : u < 0.85f ? 8.0f + 22.0f * U(rng)       // V2 : quelques dizaines
                    : 30.0f + 270.0f * U(rng);     // grands, débordent la vue
                const float cx = -20.0f + (kW + 40.0f) * U(rng), cy = -20.0f + (kH + 40.0f) * U(rng);
                Tri t{};
                for (int k = 0; k < 3; ++k)
                {
                    t.v[k] = { cx + (U(rng) - 0.5f) * size, cy + (U(rng) - 0.5f) * size };
                    const float w = 0.5f + 40.0f * U(rng);
                    t.invW[k] = 1.0f / w;
                    t.z[k] = 0.1f / w;                             // reverse-Z
                }
                const float e = U(rng);
                if (e < 0.20f)                                     // sommets sur des centres : w == 0 exact
                    for (auto& p : t.v) p = { std::floor(p.x) + 0.5f, std::floor(p.y) + 0.5f };
                else if (e < 0.30f) t.v[1].y = t.v[0].y;           // arête horizontale (top)
                else if (e < 0.35f) t.v[2] = { 0.5f * (t.v[0].x + t.v[1].x), 0.5f * (t.v[0].y + t.v[1].y) };
                else if (e < 0.37f) t.z[0] = std::numeric_limits<float>::quiet_NaN();   // z NaN
                t.col = MakeColor(uint8_t(rng()), uint8_t(rng()), uint8_t(rng()));
                tris.push_back(t);

                if (U(rng) < 0.1f)                                 // même triangle, sommets tournés : égalités de Z
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

        // Shader de TEST : compte les passages par pixel, sans test de profondeur.
        // Il a les DEUX formes : la TNR vérifie que le masque de couverture AVX2
        // désigne exactement les pixels du scalaire, pixel par pixel, recouvrements compris.
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

        bool SameImage(Target& a, Target& b, size_t& colorDiff, size_t& depthDiff)
        {
            colorDiff = depthDiff = 0;
            for (int32_t y = 0; y < kH; ++y)
            {
                for (int32_t x = 0; x < kW; ++x)
                {
                    const size_t i = size_t(y) * Target::kPitchPx + size_t(x);
                    if (a.pixels[i] != b.pixels[i]) ++colorDiff;
                    if (std::memcmp(&a.db.Row(y)[x], &b.db.Row(y)[x], sizeof(float)) != 0) ++depthDiff;
                }
                // Le remplissage de fin de ligne (pitch) ne doit JAMAIS être touché.
                for (int32_t x = kW; x < Target::kPitchPx; ++x)
                    if (b.pixels[size_t(y) * Target::kPitchPx + size_t(x)] != 0u) ++colorDiff;
            }
            return colorDiff == 0 && depthDiff == 0;
        }

        // Chemin C1 (kScalarOnly) contre chemin C2 forcé (kSimdAlways) et chemin moteur (seuil).
        template<class Shader>
        bool CheckShader(const char* name, const std::vector<Tri>& tris, const Viewport& vp)
        {
            Target A, B, C;
            FragmentContext ca{}, cb{}, cc{};
            ca.fb = &A.fb; ca.db = &A.db;
            cb.fb = &B.fb; cb.db = &B.db;
            cc.fb = &C.fb; cc.db = &C.db;
            for (const Tri& t : tris)
            {
                Fill(ca, t); RasterizeTriangleT<Shader, kScalarOnly>(t.v[0], t.v[1], t.v[2], vp, ca);
                Fill(cb, t); RasterizeTriangleT<Shader, kSimdAlways>(t.v[0], t.v[1], t.v[2], vp, cb);
                Fill(cc, t); RasterizeTriangleT<Shader>(t.v[0], t.v[1], t.v[2], vp, cc);
            }
            size_t cd = 0, dd = 0, cd2 = 0, dd2 = 0;
            bool ok = SameImage(A, B, cd, dd) & SameImage(A, C, cd2, dd2);
#if LV3_RASTER_STATS
            if (ca.depthPassed != cb.depthPassed || ca.depthPassed != cc.depthPassed)
            {
                Logger::error(std::string("[C2] ") + name + " : depthPassed " + std::to_string(ca.depthPassed)
                    + " / " + std::to_string(cb.depthPassed) + " / " + std::to_string(cc.depthPassed));
                ok = false;
            }
#endif
            if (!ok)
                Logger::error(std::string("[C2] ") + name + " : SIMD force " + std::to_string(cd) + " couleurs / "
                    + std::to_string(dd) + " profondeurs ; seuil " + std::to_string(cd2) + " / " + std::to_string(dd2));
            return ok;
        }
    }

    // C2 : chemin AVX2 == chemin scalaire C1, AU BIT PRES, pour les 5 shaders + couverture brute.
    //      Largeur et pitch non multiples de 8, sous-viewport décalé, viewport collé au bord droit.
    bool Test_C2_Raster8()
    {
        const std::vector<Tri> tris = MakeTriangles(11u, 6000);
        const Viewport views[3] = { Viewport::FullScreen(kW, kH),
                                    Viewport{ 37, 21, 203, 150 },       // décalé, largeur non multiple de 8
                                    Viewport{ kW - 61, 9, 61, 180 } };  // collé au bord DROIT du tampon

        bool ok = true;
        for (const Viewport& vp : views)
        {
            ok &= CheckShader<ShaderCount>("Couverture", tris, vp);
            ok &= CheckShader<ShaderSolid>("Solid", tris, vp);
            ok &= CheckShader<ShaderBarycentric>("Barycentric", tris, vp);
            ok &= CheckShader<ShaderLinearDepth>("LinearDepth", tris, vp);
            ok &= CheckShader<ShaderDepth>("Depth", tris, vp);
            ok &= CheckShader<ShaderLinearDepthWRONG>("LinearDepth_WRONG", tris, vp);

            // CONTRE-EXEMPLE du test qui ne teste rien : le chemin AVX2 doit avoir servi,
            // avec des blocs partiels, des recouvrements et des NaN.
            Target T;
            FragmentContext c{};
            c.fb = &T.fb; c.db = &T.db;
            for (const Tri& t : tris) { Fill(c, t); RasterizeTriangleT<ShaderCount, kSimdAlways>(t.v[0], t.v[1], t.v[2], vp, c); }
            size_t covered = 0, multi = 0, nanDepth = 0;
            for (int32_t y = 0; y < kH; ++y)
                for (int32_t x = 0; x < kW; ++x)
                {
                    const uint32_t n = T.pixels[size_t(y) * Target::kPitchPx + size_t(x)];
                    covered += n; multi += n > 1;
                }
            {
                Target S;
                FragmentContext cs{};
                cs.fb = &S.fb; cs.db = &S.db;
                for (const Tri& t : tris) { Fill(cs, t); RasterizeTriangleT<ShaderSolid, kSimdAlways>(t.v[0], t.v[1], t.v[2], vp, cs); }
                for (int32_t y = 0; y < kH; ++y)
                    for (int32_t x = 0; x < kW; ++x) nanDepth += std::isnan(S.db.Row(y)[x]) ? 1u : 0u;
            }
            Logger::info("[C2] vue " + std::to_string(vp.width) + "x" + std::to_string(vp.height)
                + " : " + std::to_string(tris.size()) + " triangles, " + std::to_string(covered)
                + " fragments, " + std::to_string(multi) + " pixels recouverts, "
                + std::to_string(nanDepth) + " profondeurs NaN");
            if (covered == 0 || multi == 0 || nanDepth == 0)
            {
                Logger::error("[C2] test trivial : aucun fragment, aucun recouvrement ou aucun NaN");
                ok = false;
            }
        }
        return ok;
    }
#endif
}