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
//  TNR D-a2 : hiérarchie de tuiles 8 x 8 (Abrash) posée sur Giesen
//   (1) MÊME IMAGE au bit près (couleur ET profondeur, 5 shaders + couverture brute) :
//       référence = blocs Giesen sans tuiles (kTilesOff), contre
//         - kTilePx = 1   : TOUT triangle AVX2 non petit passe par les tuiles (même 3 x 3 px) ;
//         - kTileMinPx    : la configuration de production (T* de config.h) ;
//         - scalaire + tuiles (kScalarOnly, kTilePx = 1) : le pendant scalaire, chemin indépendant.
//       Vues rognées (bords du viewport non multiples de 8, pitch non multiple de 8), grands triangles,
//       lames, arêtes horizontales, centres exactement sur une arête, dégénérés, z NaN.
//   (2) RÉFÉRENCE INDÉPENDANTE des verdicts de tuile (règle D-a1d) : pour chaque tuile de la grille,
//       les 64 E' sont recalculés en int64 depuis les entiers de la préparation ; « rejetée » doit
//       vouloir dire « aucun pixel ne passe l'arête », « acceptée » « les 64 passent ».
//   (3) CONTRE-EXEMPLE intégré : prendre le MAUVAIS coin (le min pour le rejet) DOIT fausser des
//       verdicts. Une TNR qui ne voit jamais d'erreur ne prouve rien.
//   (4) Domaine int32 : sommets lointains (clip near) qui font échouer TilesFitInt32 -> repli Giesen,
//       même image.
// ════════════════════════════════════════════════════════════
namespace LV3::Tests
{
#if LV3_DEBUG && LV3_RASTER_FIXED
    namespace
    {
        constexpr int32_t kW = 333, kH = 211;                     // largeur et pitch non multiples de 8

        struct Target
        {
            static constexpr int32_t kPitchPx = kW + 13;
            std::vector<uint32_t> pixels = std::vector<uint32_t>(size_t(kPitchPx) * kH, 0u);
            FrameBuffer fb;
            DepthBuffer db;
            Target() { fb.Bind(pixels.data(), kPitchPx * 4, kW, kH); db.Resize(kW, kH); db.Clear(); }
        };

        struct Tri { Vec2f v[3]; float z[3], invW[3]; Color col; };

        // Étendues tirées dans [lo, hi[ px ; 1 triangle sur 6 est une LAME (deux sommets proches, le
        // troisième loin) : boîte immense, couverture minuscule, le cas qui rejette le plus de tuiles.
        std::vector<Tri> MakeTriangles(uint32_t seed, int count, float lo, float hi, float margin)
        {
            std::mt19937 rng(seed);
            TestRand01 U;
            std::vector<Tri> tris;
            tris.reserve(size_t(count) + size_t(count) / 10);
            for (int i = 0; i < count; ++i)
            {
                const float ext = lo + (hi - lo) * U(rng);
                const float cx = -margin + (kW + 2.0f * margin) * U(rng), cy = -margin + (kH + 2.0f * margin) * U(rng);
                Tri t{};
                for (int k = 0; k < 3; ++k)
                {
                    t.v[k] = { cx + (U(rng) - 0.5f) * ext, cy + (U(rng) - 0.5f) * ext };
                    const float w = 0.5f + 40.0f * U(rng);
                    t.invW[k] = 1.0f / w;
                    t.z[k] = 0.1f / w;
                }
                const float e = U(rng);
                if (e < 0.17f) { t.v[1] = { t.v[0].x + 1.5f * U(rng), t.v[0].y + 1.5f * U(rng) }; }              // lame
                else if (e < 0.30f) for (auto& p : t.v) p = { std::floor(p.x) + 0.5f, std::floor(p.y) + 0.5f };   // w == 0 exact
                else if (e < 0.38f) t.v[1].y = t.v[0].y;                                                           // arête horizontale
                else if (e < 0.42f) t.v[2].x = t.v[1].x;                                                           // arête verticale
                else if (e < 0.45f) t.v[2] = { 0.5f * (t.v[0].x + t.v[1].x), 0.5f * (t.v[0].y + t.v[1].y) };      // dégénéré
                else if (e < 0.47f) t.z[0] = std::numeric_limits<float>::quiet_NaN();                               // z NaN
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

        // Référence (Giesen sans tuiles, AVX2) contre une configuration B ; rend couleurs + profondeurs différentes.
        template<class Shader, int32_t kMinB, int32_t kTileB>
        size_t Compare(const std::vector<Tri>& tris, const Viewport& vp, size_t& cd, size_t& dd)
        {
            Target A, B;
            FragmentContext ca{}, cb{};
            ca.fb = &A.fb; ca.db = &A.db; cb.fb = &B.fb; cb.db = &B.db;
            for (const Tri& t : tris)
            {
                Fill(ca, t); RasterizeTriangleT<Shader, kSimdMinBoxWidth, EFixedLanes::Auto, kSmallFloatPx, kTilesOff>(t.v[0], t.v[1], t.v[2], vp, ca);
                Fill(cb, t); RasterizeTriangleT<Shader, kMinB, EFixedLanes::Auto, kSmallFloatPx, kTileB>(t.v[0], t.v[1], t.v[2], vp, cb);
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
            size_t c1 = 0, d1 = 0, cP = 0, dP = 0, cS = 0, dS = 0;
            const size_t bad = Compare<Shader, kSimdMinBoxWidth, 1>(tris, vp, c1, d1)
                             + Compare<Shader, kSimdMinBoxWidth, kTileMinPx>(tris, vp, cP, dP)
                             + Compare<Shader, kScalarOnly, 1>(tris, vp, cS, dS);
            if (bad != 0)
                Logger::error(std::string("[D-a2] ") + name + " : tuiles partout " + std::to_string(c1) + " couleurs / " + std::to_string(d1)
                    + " profondeurs ; production " + std::to_string(cP) + " / " + std::to_string(dP)
                    + " ; scalaire " + std::to_string(cS) + " / " + std::to_string(dS));
            return bad == 0;
        }

        // ── (2) + (3) : verdicts de tuile contre la force brute (64 E' par tuile, int64).
        //    wrongCorner = true : le contre-exemple (rejet jugé au coin du MIN, acceptation au coin du MAX).
        struct TileAudit { uint64_t tiles = 0, rejected = 0, accepted = 0, errors = 0; };

        TileAudit AuditTiles(const std::vector<Tri>& tris, const Viewport& vp, bool wrongCorner)
        {
            TileAudit a;
            for (const Tri& t : tris)
            {
                if (!InFixedGuard(t.v[0], t.v[1], t.v[2])) continue;
                TriangleSetup s; EdgeSetupFx fx;
                if (!SetupTriangleFx(t.v[0], t.v[1], t.v[2], vp, s, fx)) continue;
                for (int32_t ty = s.bb.y0; ty < s.bb.y1; ty += kTileSize)
                    for (int32_t tx = s.bb.x0; tx < s.bb.x1; tx += kTileSize)
                    {
                        ++a.tiles;
                        bool rej = false, acc = true;
                        for (int k = 0; k < 3; ++k)
                        {
                            const int64_t c = fx.e[k] + fx.a[k] * (tx - s.bb.x0) + fx.b[k] * (ty - s.bb.y0);   // E' au coin haut-gauche
                            const int64_t rj = wrongCorner ? TileAcceptOfs(fx.a[k], fx.b[k]) : TileRejectOfs(fx.a[k], fx.b[k]);
                            const int64_t ac = wrongCorner ? TileRejectOfs(fx.a[k], fx.b[k]) : TileAcceptOfs(fx.a[k], fx.b[k]);
                            rej |= (c + rj) < 0;
                            acc &= (c + ac) >= 0;
                            // Force brute : les 64 centres de la tuile (même hors boîte : E' est défini partout).
                            int64_t mn = INT64_MAX, mx = INT64_MIN;
                            for (int32_t j = 0; j < kTileSize; ++j)
                                for (int32_t i = 0; i < kTileSize; ++i)
                                {
                                    const int64_t e = c + fx.a[k] * i + fx.b[k] * j;
                                    mn = e < mn ? e : mn; mx = e > mx ? e : mx;
                                }
                            const bool truthRej = mx < 0, truthAcc = mn >= 0;
                            const bool sayRej = (c + rj) < 0, sayAcc = (c + ac) >= 0;
                            if (sayRej != truthRej || sayAcc != truthAcc) ++a.errors;
                        }
                        a.rejected += rej ? 1u : 0u;
                        a.accepted += (!rej && acc) ? 1u : 0u;
                    }
            }
            return a;
        }
    }

    bool Test_Da2_Tiles()
    {
        bool ok = true;
        // Vues : pleine, intérieure à bords non multiples de 8, collée au bord droit (rognage à droite).
        const Viewport views[3] = { Viewport::FullScreen(kW, kH), Viewport{ 45, 29, 211, 151 }, Viewport{ kW - 75, 9, 75, 190 } };

        // ── (1) même image : étendues 2 à 160 px, débordant de la vue (marge 60 px) ──
        const std::vector<Tri> tris = MakeTriangles(53u, 6000, 2.0f, 160.0f, 60.0f);
        for (const Viewport& vp : views)
        {
            ok &= CheckShader<ShaderCount>("Couverture", tris, vp);
            ok &= CheckShader<ShaderSolid>("Solid", tris, vp);
            ok &= CheckShader<ShaderBarycentric>("Barycentric", tris, vp);
            ok &= CheckShader<ShaderDepth>("Depth", tris, vp);
            ok &= CheckShader<ShaderLinearDepth>("LinearDepth", tris, vp);
            ok &= CheckShader<ShaderLinearDepthWRONG>("LinearDepth_WRONG", tris, vp);
            const TileAudit au = AuditTiles(tris, vp, false);
            Logger::info("[D-a2] vue " + std::to_string(vp.width) + "x" + std::to_string(vp.height) + " : "
                + std::to_string(tris.size()) + " triangles, " + std::to_string(au.tiles) + " tuiles ("
                + std::to_string(au.rejected) + " rejetees, " + std::to_string(au.accepted) + " pleines), erreurs de verdict "
                + std::to_string(au.errors) + " (attendu 0)");
            ok &= (au.errors == 0) && (au.rejected > 0) && (au.accepted > 0);
        }

        // ── (3) contre-exemple : le mauvais coin DOIT fausser des verdicts ──
        {
            const TileAudit bad = AuditTiles(tris, views[0], true);
            Logger::info("[D-a2] contre-exemple (coins echanges) : " + std::to_string(bad.errors)
                + " verdicts d'arete faux (attendu > 0)");
            ok &= (bad.errors > 0);
        }

        // ── (4) domaine int32 : sommets très lointains (longues arêtes clippées au near) ──
        {
            std::vector<Tri> far = MakeTriangles(59u, 300, 40.0f, 120.0f, 0.0f);
            std::mt19937 rng(61u);
            TestRand01 U;
            for (Tri& t : far)                                                   // un sommet projeté à ~2^17-2^19 px
            {
                const float s = (U(rng) < 0.5f ? -1.0f : 1.0f) * (131072.0f + 393216.0f * U(rng));
                t.v[2] = { SnapToSubpixel(t.v[0].x + s), SnapToSubpixel(t.v[0].y + 0.37f * s) };
            }
            uint32_t fallback = 0;
            for (const Tri& t : far)
            {
                TriangleSetup s; EdgeSetupFx fx;
                if (InFixedGuard(t.v[0], t.v[1], t.v[2]) && SetupTriangleFx(t.v[0], t.v[1], t.v[2], views[0], s, fx)
                    && IsTiledTri<1>(s) && !TilesFitInt32(s, fx)) ++fallback;
            }
            ok &= CheckShader<ShaderBarycentric>("Lointains, Barycentric", far, views[0]);
            ok &= CheckShader<ShaderLinearDepth>("Lointains, LinearDepth", far, views[0]);
            Logger::info("[D-a2] lointains : " + std::to_string(far.size()) + " triangles, " + std::to_string(fallback)
                + " replis Giesen (E' hors int32 sur les tuiles, attendu > 0)");
            ok &= (fallback > 0);
        }
        return ok;
    }
#else
    bool Test_Da2_Tiles() { return true; }   // D-a1 inactif (LV3_RASTER_FIXED = 0) ou Release
#endif
}
