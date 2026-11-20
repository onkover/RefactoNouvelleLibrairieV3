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
//  TNR D-a2b : boucle des tuiles sans branche de données (deux masques full, puis live & ~full)
//   (1) MÊME IMAGE au bit près (couleur ET profondeur, 5 shaders + couverture brute), MÊME nombre
//       de pixels couverts, pour TOUT triangle tuilable (kTilePx = 1) :
//         R  = blocs Giesen sans tuiles (RasterizeBlocks8Fx), la référence ;
//         A  = RasterizeTiles8Fx<Shader, false> (D-a2 : branche par tuile) ;
//         B' = RasterizeTiles8Fx<Shader, true>  (D-a2b : deux boucles).
//   (2) VISITES : chaque pixel est visité AU PLUS UNE FOIS par triangle (tampon d'estampilles),
//       et A et B' visitent exactement les mêmes pixels. C'est ce que l'image ne voit pas :
//       une tuile pleine redessinée en partielle laisse l'image intacte (le test Z échoue à z égal).
//   (3) ORDRE (propriété de D-a2b) : dans un groupe de 8 tuiles, aucune tuile PLEINE n'est visitée
//       après une tuile PARTIELLE. Et l'ordre de D-a2 la viole (sinon le test ne prouve rien).
//   (4) AUTO-CONTRÔLE du détecteur de (2) : dessiner deux fois le même triangle sans changer
//       d'estampille DOIT être signalé.
//   (5) Référence INDÉPENDANTE du compteur Tiles8Switch (règle D-a1d) : changements de nature
//       recomptés en int64 depuis les entiers de la préparation, tuile par tuile (journal).
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

        // Mêmes familles que la TNR D-a2 (lames, w == 0, arêtes horizontales/verticales, dégénérés, z NaN),
        // graine différente, étendues 2 à 200 px : groupes à 1..8 tuiles, pleines et partielles mêlées.
        std::vector<Tri> MakeTriangles(uint32_t seed, int count, float lo, float hi, float margin)
        {
            std::mt19937 rng(seed);
            TestRand01 U;
            std::vector<Tri> tris;
            tris.reserve(size_t(count));
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
                if (e < 0.15f) { t.v[1] = { t.v[0].x + 1.5f * U(rng), t.v[0].y + 1.5f * U(rng) }; }              // lame
                else if (e < 0.28f) for (auto& p : t.v) p = { std::floor(p.x) + 0.5f, std::floor(p.y) + 0.5f };   // w == 0 exact
                else if (e < 0.36f) t.v[1].y = t.v[0].y;                                                           // arête horizontale
                else if (e < 0.40f) t.v[2].x = t.v[1].x;                                                           // arête verticale
                else if (e < 0.43f) t.v[2] = { 0.5f * (t.v[0].x + t.v[1].x), 0.5f * (t.v[0].y + t.v[1].y) };      // dégénéré
                else if (e < 0.45f) t.z[0] = std::numeric_limits<float>::quiet_NaN();                               // z NaN
                for (auto& p : t.v) p = { SnapToSubpixel(p.x), SnapToSubpixel(p.y) };   // contrat de l'étage sommets
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

        // Le triangle prendrait-il la hiérarchie si T* valait 1 ? (même aiguillage que RasterizeTriangleFx)
        bool Tileable(const Tri& t, const Viewport& vp, TriangleSetup& s, EdgeSetupFx& fx)
        {
            return InFixedGuard(t.v[0], t.v[1], t.v[2]) && SetupTriangleFx(t.v[0], t.v[1], t.v[2], vp, s, fx)
                && (s.bb.x1 - s.bb.x0) >= kSimdMinBoxWidth && !IsSmallFloatTri<kSmallFloatPx>(fx) && TilesFitInt32(s, fx);
        }

        // kWay : 0 = R (blocs Giesen), 1 = A (D-a2), 2 = B' (D-a2b). Rend les pixels couverts des triangles tuilables.
        template<class Shader, int kWay>
        uint64_t DrawAll(const std::vector<Tri>& tris, const Viewport& vp, Target& T, uint64_t& depthPassed)
        {
            FragmentContext c{}; c.fb = &T.fb; c.db = &T.db;
            uint64_t covered = 0;
            for (const Tri& t : tris)
            {
                Fill(c, t);
                TriangleSetup s; EdgeSetupFx fx;
                if (Tileable(t, vp, s, fx))
                {
                    if constexpr (kWay == 0) covered += RasterizeBlocks8Fx<Shader, false>(s, fx, c);
                    else                     covered += RasterizeTiles8Fx<Shader, kWay == 2>(s, fx, c);
                }
                else
                    RasterizeTriangleT<Shader, kSimdMinBoxWidth, EFixedLanes::Auto, kSmallFloatPx, kTilesOff>(t.v[0], t.v[1], t.v[2], vp, c);
            }
#if LV3_RASTER_STATS
            depthPassed = c.depthPassed;
#else
            depthPassed = 0;
#endif
            return covered;
        }

        template<class Shader>
        bool CheckShader(const char* name, const std::vector<Tri>& tris, const Viewport& vp)
        {
            Target R, A, B;
            uint64_t dpR = 0, dpA = 0, dpB = 0;
            const uint64_t covR = DrawAll<Shader, 0>(tris, vp, R, dpR);
            const uint64_t covA = DrawAll<Shader, 1>(tris, vp, A, dpA);
            const uint64_t covB = DrawAll<Shader, 2>(tris, vp, B, dpB);
            size_t dA = 0, dB = 0;
            const size_t cA = Diff(R, A, dA), cB = Diff(R, B, dB);
            const bool ok = cA == 0 && dA == 0 && cB == 0 && dB == 0 && covR == covA && covR == covB && dpR == dpA && dpR == dpB;
            if (!ok)
                Logger::error(std::string("[D-a2b] ") + name + " : D-a2 " + std::to_string(cA) + " couleurs / " + std::to_string(dA)
                    + " profondeurs ; D-a2b " + std::to_string(cB) + " / " + std::to_string(dB)
                    + " ; couverts R/A/B' " + std::to_string(covR) + "/" + std::to_string(covA) + "/" + std::to_string(covB));
            return ok;
        }

        // ── (2) + (3) : puits qui ESTAMPILLE chaque pixel visité et surveille l'ordre plein / partiel.
        struct AuditSink8
        {
            std::vector<uint32_t>* stamp = nullptr;     // kW x kH : n° du dernier triangle qui a visité le pixel
            std::vector<uint32_t>* visits = nullptr;    // kW x kH : nombre de visites (comparaison A / B')
            uint32_t triId = 1;
            int32_t  bx0 = 0, y = 0, tileY = 0;
            int32_t  lastGroupKey = INT32_MIN;          // (rangée de tuiles, groupe) courant
            bool     partialSeen = false;
            uint64_t doubleVisits = 0, fullAfterPartial = 0;

            LV3_FORCEINLINE void BeginRow(int32_t yy) noexcept { y = yy; tileY = yy; }
            LV3_FORCEINLINE void NextRow() noexcept { ++y; }
            void Visit(int32_t x, uint32_t bits, bool full) noexcept
            {
                const int32_t key = tileY * 4096 + ((x - bx0) >> 6);           // un groupe = 64 pixels de large
                if (key != lastGroupKey) { lastGroupKey = key; partialSeen = false; }
                if (full && partialSeen) ++fullAfterPartial;
                if (!full) partialSeen = true;                                  // une tuile partielle a déjà écrit dans ce groupe
                for (uint32_t m = bits; m != 0; m &= m - 1)
                {
                    const size_t i = size_t(y) * kW + size_t(x + int32_t(_tzcnt_u32(m)));
                    if ((*stamp)[i] == triId) ++doubleVisits;
                    (*stamp)[i] = triId;
                    ++(*visits)[i];
                }
            }
            LV3_FORCEINLINE void Block(int32_t x, __m256, uint32_t bits, __m256, __m256, __m256) noexcept { Visit(x, bits, false); }
            LV3_FORCEINLINE void BlockFull(int32_t x, __m256, __m256, __m256, __m256) noexcept { Visit(x, 0xFFu, true); }
        };

        struct VisitAudit { uint64_t doubleVisits = 0, fullAfterPartial = 0, visitDiff = 0; };

        template<bool kSplit>
        VisitAudit Audit(const std::vector<Tri>& tris, const Viewport& vp, std::vector<uint32_t>& visits, bool reuseStamp)
        {
            std::vector<uint32_t> stamp(size_t(kW) * kH, 0u);
            AuditSink8 sink; sink.stamp = &stamp; sink.visits = &visits;
            uint32_t id = 1;
            for (const Tri& t : tris)
            {
                TriangleSetup s; EdgeSetupFx fx;
                if (!Tileable(t, vp, s, fx)) continue;
                sink.triId = id; sink.bx0 = s.bb.x0; sink.lastGroupKey = INT32_MIN;
                (void)ForEachCoveredTile8Fx32<AuditSink8, kSplit>(s, fx, sink);
                if (reuseStamp) { sink.lastGroupKey = INT32_MIN; (void)ForEachCoveredTile8Fx32<AuditSink8, kSplit>(s, fx, sink); }   // (4)
                ++id;
            }
            return { sink.doubleVisits, sink.fullAfterPartial, 0 };
        }

        // ── (5) Changements de nature entre tuiles vivantes consécutives d'un groupe, en int64, tuile par tuile.
        uint64_t BruteSwitches(const std::vector<Tri>& tris, const Viewport& vp, uint64_t& groups, uint64_t& mixed)
        {
            uint64_t sw = 0; groups = 0; mixed = 0;
            for (const Tri& t : tris)
            {
                TriangleSetup s; EdgeSetupFx fx;
                if (!Tileable(t, vp, s, fx)) continue;
                const int32_t ntx = (s.bb.x1 - s.bb.x0 + kTileSize - 1) / kTileSize, nty = (s.bb.y1 - s.bb.y0 + kTileSize - 1) / kTileSize;
                for (int32_t ty = 0; ty < nty; ++ty)
                    for (int32_t g = 0; g < ntx; g += 8)
                    {
                        int prev = -1; bool anyF = false, anyP = false;
                        for (int32_t j = 0; j < 8 && g + j < ntx; ++j)
                        {
                            const int32_t tx = g + j; bool rej = false, acc = true;
                            for (int k = 0; k < 3; ++k)
                            {
                                const int64_t c = fx.e[k] + fx.a[k] * (tx * kTileSize) + fx.b[k] * (ty * kTileSize);
                                rej |= (c + TileRejectOfs(fx.a[k], fx.b[k])) < 0;
                                acc &= (c + TileAcceptOfs(fx.a[k], fx.b[k])) >= 0;
                            }
                            if (rej) continue;
                            const bool fit = s.bb.x0 + (tx + 1) * kTileSize <= s.bb.x1 && s.bb.y0 + (ty + 1) * kTileSize <= s.bb.y1;
                            const int kind = (acc && fit) ? 1 : 0;
                            anyF |= kind == 1; anyP |= kind == 0;
                            if (prev >= 0 && kind != prev) ++sw;
                            prev = kind;
                        }
                        if (prev >= 0) { ++groups; mixed += (anyF && anyP) ? 1u : 0u; }
                    }
            }
            return sw;
        }
    }

    bool Test_Da2b_TileSplit()
    {
        bool ok = true;
        const Viewport views[3] = { Viewport::FullScreen(kW, kH), Viewport{ 45, 29, 211, 151 }, Viewport{ kW - 75, 9, 75, 190 } };
        const std::vector<Tri> tris = MakeTriangles(71u, 5000, 2.0f, 200.0f, 60.0f);

        for (const Viewport& vp : views)
        {
            // ── (1) même image, mêmes pixels couverts ──
            ok &= CheckShader<ShaderCount>("Couverture", tris, vp);
            ok &= CheckShader<ShaderSolid>("Solid", tris, vp);
            ok &= CheckShader<ShaderBarycentric>("Barycentric", tris, vp);
            ok &= CheckShader<ShaderDepth>("Depth", tris, vp);
            ok &= CheckShader<ShaderLinearDepth>("LinearDepth", tris, vp);

            // ── (2) + (3) visites et ordre ──
            std::vector<uint32_t> vA(size_t(kW) * kH, 0u), vB(size_t(kW) * kH, 0u);
            const VisitAudit a = Audit<false>(tris, vp, vA, false);
            const VisitAudit b = Audit<true>(tris, vp, vB, false);
            const bool sameVisits = (vA == vB);
            uint64_t groups = 0, mixed = 0;
            const uint64_t sw = BruteSwitches(tris, vp, groups, mixed);
            Logger::info("[D-a2b] vue " + std::to_string(vp.width) + "x" + std::to_string(vp.height) + " : "
                + std::to_string(groups) + " groupes vivants (" + std::to_string(mixed) + " mixtes), "
                + std::to_string(sw) + " changements de nature (ordre D-a2) ; doubles visites A/B' "
                + std::to_string(a.doubleVisits) + "/" + std::to_string(b.doubleVisits) + " (attendu 0/0), blocs pleins apres une partielle A/B' "
                + std::to_string(a.fullAfterPartial) + "/" + std::to_string(b.fullAfterPartial) + " (attendu >0 / 0), visites identiques "
                + (sameVisits ? "oui" : "NON"));
            ok &= a.doubleVisits == 0 && b.doubleVisits == 0 && sameVisits && b.fullAfterPartial == 0 && a.fullAfterPartial > 0 && mixed > 0;
        }

        // ── (4) le détecteur de doubles visites doit voir un triangle dessiné deux fois ──
        {
            std::vector<uint32_t> v(size_t(kW) * kH, 0u);
            const VisitAudit twice = Audit<true>(tris, views[0], v, true);
            Logger::info("[D-a2b] auto-controle (chaque triangle dessine deux fois) : " + std::to_string(twice.doubleVisits)
                + " doubles visites (attendu > 0)");
            ok &= twice.doubleVisits > 0;
        }
        return ok;
    }
#else
    bool Test_Da2b_TileSplit() { return true; }   // D-a1 inactif (LV3_RASTER_FIXED = 0) ou Release
#endif
}
