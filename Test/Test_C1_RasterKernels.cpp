#include "pch.h"
#include <random>
#include <cstring>
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
        constexpr int32_t kW = 320, kH = 200;

        // Une cible PROPRE au test : couleur (pitch volontairement > largeur, comme SDL) + profondeur.
        struct Target
        {
            static constexpr int32_t kPitchPx = kW + 16;          // lignes espacees : Row() doit utiliser le pitch
            std::vector<uint32_t> pixels = std::vector<uint32_t>(size_t(kPitchPx) * kH, 0u);
            FrameBuffer fb;
            DepthBuffer db;
            Target() { fb.Bind(pixels.data(), kPitchPx * 4, kW, kH); db.Resize(kW, kH); db.Clear(); }
        };

        struct Tri { Vec2f v[3]; float z[3], invW[3]; Color col; };

        // Triangles de toutes tailles, des deux sens, avec les cas limites de la regle top-left.
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
                const float size = u < 0.4f ? 0.3f + 1.2f * U(rng)        // V1 : boite d'un pixel
                    : u < 0.8f ? 3.0f + 27.0f * U(rng)        // V2 : quelques dizaines
                    : 30.0f + 270.0f * U(rng);     // grands, debordent la vue
                const float cx = -20.0f + (kW + 40.0f) * U(rng), cy = -20.0f + (kH + 40.0f) * U(rng);
                Tri t{};
                for (int k = 0; k < 3; ++k)
                {
                    t.v[k] = { cx + (U(rng) - 0.5f) * size, cy + (U(rng) - 0.5f) * size };
                    const float w = 0.5f + 40.0f * U(rng);        // distance a l'oeil
                    t.invW[k] = 1.0f / w;
                    t.z[k] = 0.1f / w;                             // reverse-Z : proche -> grand
                }
                const float e = U(rng);
                if (e < 0.20f)                                     // sommets SUR des centres de pixels : w == 0 exact
                    for (auto& p : t.v) p = { std::floor(p.x) + 0.5f, std::floor(p.y) + 0.5f };
                else if (e < 0.30f) t.v[1].y = t.v[0].y;           // arete horizontale (top)
                else if (e < 0.35f) t.v[2] = { 0.5f * (t.v[0].x + t.v[1].x), 0.5f * (t.v[0].y + t.v[1].y) }; // degenere
                t.col = MakeColor(uint8_t(rng()), uint8_t(rng()), uint8_t(rng()));
                tris.push_back(t);

                if (U(rng) < 0.1f)                                 // MEME triangle, sommets tournes : egalites de Z
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

        // Compare au BIT pres : couleur (lignes utiles seulement) et profondeur (bit a bit, pas ==).
        bool SameImage(Target& a, Target& b, size_t& colorDiff, size_t& depthDiff)
        {
            colorDiff = depthDiff = 0;
            for (int32_t y = 0; y < kH; ++y)
                for (int32_t x = 0; x < kW; ++x)
                {
                    const size_t i = size_t(y) * Target::kPitchPx + size_t(x);
                    if (a.pixels[i] != b.pixels[i]) ++colorDiff;
                    if (std::memcmp(&a.db.Row(y)[x], &b.db.Row(y)[x], sizeof(float)) != 0) ++depthDiff;
                }
            return colorDiff == 0 && depthDiff == 0;
        }

        // Generique (pointeur de fonction par pixel, = chemin d'avant C1) contre noyau specialise.
        template<class Shader>
        bool CheckShader(const char* name, FragmentCallback generic, const std::vector<Tri>& tris,
            const Viewport& vp, size_t& litOut)
        {
            Target A, B;
            FragmentContext ca{}, cb{};
            ca.fb = &A.fb; ca.db = &A.db;
            cb.fb = &B.fb; cb.db = &B.db;
            for (const Tri& t : tris)
            {
                Fill(ca, t); RasterizeTriangle(t.v[0], t.v[1], t.v[2], vp, generic, &ca);
                Fill(cb, t); RasterizeTriangleT<Shader>(t.v[0], t.v[1], t.v[2], vp, cb);
            }
            size_t cd = 0, dd = 0;
            bool ok = SameImage(A, B, cd, dd);
#if LV3_RASTER_STATS
            // Compteur C0 : meme nombre de fragments passes au Z par les deux chemins.
            if (ca.depthPassed != cb.depthPassed)
            {
                Logger::error(std::string("[C1] ") + name + " : depthPassed " + std::to_string(ca.depthPassed)
                    + " (generique) contre " + std::to_string(cb.depthPassed) + " (noyau)");
                ok = false;
            }
#endif
            litOut = 0;
            for (int32_t y = 0; y < kH; ++y)
                for (int32_t x = 0; x < kW; ++x)
                    litOut += A.pixels[size_t(y) * Target::kPitchPx + size_t(x)] != 0u;
            if (!ok)
                Logger::error(std::string("[C1] ") + name + " : " + std::to_string(cd) + " pixels couleur et "
                    + std::to_string(dd) + " profondeurs differentes");
            return ok;
        }
    }

    // C1 : noyau specialise par shader == chemin generique, AU BIT PRES, pour les 5 shaders,
    //      plein ecran et sous-viewport decale (scissor), pitch > largeur.
    bool Test_C1_RasterKernels()
    {
        const std::vector<Tri> tris = MakeTriangles(7u, 6000);
        const Viewport views[2] = { Viewport::FullScreen(kW, kH), Viewport{ 37, 21, 200, 150 } };

        bool ok = true;
        for (const Viewport& vp : views)
        {
            size_t lit[5] = {};
            ok &= CheckShader<ShaderSolid>("Solid", &ShadeFragment_Solid, tris, vp, lit[0]);
            ok &= CheckShader<ShaderBarycentric>("Barycentric", &ShadeFragment_Barycentric, tris, vp, lit[1]);
            ok &= CheckShader<ShaderLinearDepth>("LinearDepth", &ShadeFragment_LinearDepth, tris, vp, lit[2]);
            ok &= CheckShader<ShaderDepth>("Depth", &ShadeFragment_Depth, tris, vp, lit[3]);
            ok &= CheckShader<ShaderLinearDepthWRONG>("LinearDepth_WRONG", &ShadeFragment_LinearDepth_WRONG, tris, vp, lit[4]);

            // CONTRE-EXEMPLE du test qui ne teste rien : il faut des pixels allumes ET des Z-tests perdus.
            std::vector<uint8_t> counts(size_t(kW) * kH, 0);
            CountContext cc{ &counts, kW };
            for (const Tri& t : tris) RasterizeTriangle(t.v[0], t.v[1], t.v[2], vp, &ShadeFragment_Count, &cc);
            size_t covered = 0, multi = 0;
            for (uint8_t n : counts) { covered += n; multi += n > 1; }

            Logger::info("[C1] vue " + std::to_string(vp.width) + "x" + std::to_string(vp.height)
                + " : " + std::to_string(tris.size()) + " triangles, " + std::to_string(covered)
                + " fragments, " + std::to_string(lit[0]) + " pixels allumes (Solid), "
                + std::to_string(multi) + " pixels recouverts");
            if (lit[0] == 0 || multi == 0)
            {
                Logger::error("[C1] test trivial : aucun pixel allume ou aucun recouvrement");
                ok = false;
            }
        }
        return ok;
    }
#endif
}