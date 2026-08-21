#include "pch.h"
#include "Test_Depth.h"
#include "Maths/Projection.h"
#include "Rendering/Viewport.h"
#include "Rendering/Rasterizer.h"
#include "Rendering/DepthBuffer.h"
#include "Core/Logger.h"

namespace LV3::Tests
{

    // ────────────────────────────────────────────────────────────
    //  Contexte local au test.
    //  Second type derrière le void* : ADMISSIBLE ici (exception
    //  documentée à la Règle 21) car ce test n'utilise ni Renderer
    //  ni FragmentContext — les deux types ne peuvent pas se croiser.
    // ────────────────────────────────────────────────────────────
    struct DepthProbeCtx
    {
        DepthBuffer* db = nullptr;
        std::vector<uint8_t>* winner = nullptr;   // 0 = personne, 1 = quad A, 2 = quad B
        int32_t               stride = 0;
        uint8_t               id = 0;
        float                 z0 = 0.f, z1 = 0.f, z2 = 0.f;
    };

    static void ShadeFragment_DepthProbe(int32_t x, int32_t y,
        const BarycentricWeights& b, void* u)
    {
        auto* c = static_cast<DepthProbeCtx*>(u);

        // z_ndc s'interpole AFFINEMENT : il est linéaire en espace écran
        // (fonction affine de 1/w, lui-même linéaire). Aucune correction perspective.
        const float z = b.w0 * c->z0 + b.w1 * c->z1 + b.w2 * c->z2;

        if (!c->db->TestAndSet(x, y, z)) return;      // reverse-Z, GREATER
        (*c->winner)[size_t(y) * c->stride + x] = c->id;
    }

    // ────────────────────────────────────────────────────────────
    //  Une passe complète, paramétrée par l'ordre de soumission.
    //  Le Z-buffer doit trancher — PAS l'ordre.
    // ────────────────────────────────────────────────────────────
    static bool RunCrossing(bool aFirst)
    {
        constexpr int32_t W = 128, H = 128;
        const Viewport  vp = Viewport::FullScreen(W, H);
        const Matrix44f proj = Projection::Perspective(60.f * TO_RADIAN, 1.0f, 0.1f, 100.f);

        DepthBuffer db;
        db.Resize(W, H);
        db.Clear();

        std::vector<uint8_t> winner(size_t(W) * H, 0);

        // Deux quads verticaux qui se croisent au centre.
        //   A : PROCHE à gauche (z=-3)   -> lointain à droite (z=-9)
        //   B : lointain à gauche (z=-9) -> PROCHE à droite (z=-3)
        // Attendu : A gagne la moitié gauche, B la moitié droite.
        const Vec3f A[4] = { {-3,-3,-3}, { 3,-3,-9}, { 3, 3,-9}, {-3, 3,-3} };
        const Vec3f B[4] = { {-3,-3,-9}, { 3,-3,-3}, { 3, 3,-3}, {-3, 3,-9} };

        auto Draw = [&](const Vec3f q[4], uint8_t id)
            {
                Vec3f r[4];
                for (int k = 0; k < 4; ++k)
                {
                    const Vec4f c = MulRow(proj, q[k]);
                    const float inv = 1.0f / c.w;
                    r[k] = vp.ToRaster({ c.x * inv, c.y * inv, c.z * inv });
                }

                // Éventail : (0,1,2) puis (0,2,3)
                for (int t = 0; t + 2 < 4; ++t)
                {
                    DepthProbeCtx ctx;
                    ctx.db = &db;  ctx.winner = &winner;  ctx.stride = W;  ctx.id = id;
                    ctx.z0 = r[0].z;  ctx.z1 = r[t + 1].z;  ctx.z2 = r[t + 2].z;

                    RasterizeTriangle({ r[0].x,     r[0].y },
                        { r[t + 1].x, r[t + 1].y },
                        { r[t + 2].x, r[t + 2].y },
                        vp, ShadeFragment_DepthProbe, &ctx);
                }
            };

        if (aFirst) { Draw(A, 1); Draw(B, 2); }
        else { Draw(B, 2); Draw(A, 1); }

        // ── Verdict ──
        // Bande centrale verticale seulement (H/4 .. 3H/4) : on évite les coins,
        // où les deux quads sont presque coplanaires.
        // Marge de 6 px de part et d'autre de la couture : à l'intersection exacte
        // les profondeurs sont égales, le gagnant y est légitimement indécis.
        int wrongLeft = 0, wrongRight = 0, empty = 0;

        for (int32_t y = H / 4; y < 3 * H / 4; ++y)
        {
            for (int32_t x = 4; x < W / 2 - 6; ++x)
            {
                const uint8_t w = winner[size_t(y) * W + x];
                if (w == 0)      ++empty;
                else if (w != 1) ++wrongLeft;
            }
            for (int32_t x = W / 2 + 6; x < W - 4; ++x)
            {
                const uint8_t w = winner[size_t(y) * W + x];
                if (w == 0)      ++empty;
                else if (w != 2) ++wrongRight;
            }
        }

        const bool ok = (wrongLeft == 0 && wrongRight == 0 && empty == 0);

        Logger::log(std::string("[DEPTH] ordre=") + (aFirst ? "A,B" : "B,A")
            + "  gauche_faux=" + std::to_string(wrongLeft)
            + "  droite_faux=" + std::to_string(wrongRight)
            + "  vides=" + std::to_string(empty)
            + (ok ? "  OK" : "  ECHEC"));
        return ok;
    }

    bool Test_Depth_CrossingTriangles()
    {
        // Les deux ordres DOIVENT donner le même résultat.
        // C'est la définition opérationnelle d'un Z-buffer.
        const bool ab = RunCrossing(true);
        const bool ba = RunCrossing(false);
        return ab && ba;
    }

} // namespace LV3::Tests