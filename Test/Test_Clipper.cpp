#include "pch.h"
#include "Test_Clipper.h"
#include "Maths/Projection.h"
#include "Rendering/Viewport.h"
#include "Rendering/Rasterizer.h"
#include "Rendering/Clipper.h"

#include "Core/Logger.h"


using namespace LV3;

namespace LV3::Tests
{

    // ── Helper commun : sommet en espace VUE → sommet raster ──
    static Vec3f ToRasterFromView(const Matrix44f& proj, const Viewport& vp, const Vec3f& viewPos)
    {
        const Vec4f c = MulRow(proj, viewPos);
        const float inv = 1.0f / c.w;
        return vp.ToRaster({ c.x * inv, c.y * inv, c.z * inv });
    }

    // ════════════════════════════════════════════════════════════
    //  TEST 1 — La face AVANT a une aire raster NÉGATIVE.
    //  Invariant : ne dépend ni de la scène, ni des assets, ni de la caméra.
    //  C'est le test qui manquait en L05 et qui a laissé passer 
    //  le culling inversé pendant toute une leçon.
    // ════════════════════════════════════════════════════════════
    bool Test_FrontFaceSign()
    {
        const Viewport  vp = Viewport::FullScreen(800, 800);
        const Matrix44f proj = Projection::Perspective(60.0f * TO_RADIAN, 1.0f, 0.1f, 100.0f);

        // CCW vu de la caméra (origine, regard -Z) => FACE AVANT.
        // Normale (b-a)x(c-a) = (0,0,4) : pointée vers la caméra. Vérifiable à la main.
        const Vec3f v[3] = { {-1,-1,-5}, {1,-1,-5}, {0,1,-5} };

        Vec3f r[3];
        for (int k = 0; k < 3; ++k) r[k] = ToRasterFromView(proj, vp, v[k]);

        const float areaFront = EdgeFunction(r[0], r[1], r[2]);
        const float areaBack = EdgeFunction(r[0], r[2], r[1]);   // ordre inversé

        bool ok = true;
        if (!(areaFront < 0.0f))
        {
            Logger::error("[WINDING] face AVANT : aire = " + std::to_string(areaFront)
                + " — attendue NEGATIVE (flip Y du viewport)");
            ok = false;
        }
        if (IsBackFacing(areaFront)) { Logger::error("[WINDING] face avant culled"); ok = false; }
        if (!IsBackFacing(areaBack)) { Logger::error("[WINDING] face arriere gardee"); ok = false; }

        // Antisymétrie EXACTE de EdgeFunction (Règle 24)
        if (areaFront != -areaBack)
        {
            Logger::error("[WINDING] EdgeFunction non antisymetrique : "
                + std::to_string(areaFront) + " vs " + std::to_string(-areaBack));
            ok = false;
        }
        return ok;
    }

    // ════════════════════════════════════════════════════════════
    //  TEST 2 — Les 4 configurations de ClipTriangleNear.
    //  Le cas "2 gardés -> 4 sommets" est celui qui impose que la
    //  fonction retourne un quadrilatère, pas un triangle.
    // ════════════════════════════════════════════════════════════
    bool Test_ClipTriangleNear_Cases()
    {
        const Matrix44f proj = Projection::Perspective(60.0f * TO_RADIAN, 1.0f, 0.1f, 100.0f);

        auto Build = [&](const Vec3f& a, const Vec3f& b, const Vec3f& c, ClipVertex out[3])
            {
                out[0].clip = MulRow(proj, a);
                out[1].clip = MulRow(proj, b);
                out[2].clip = MulRow(proj, c);
            };

        ClipVertex src[3], dst[kMaxClipVertices];
        bool ok = true;

        auto Check = [&](const char* label, int32_t expected)
            {
                const int32_t n = ClipTriangleNear(src, dst);
                if (n != expected)
                {
                    Logger::error(std::string("[CLIP] ") + label + " : n=" + std::to_string(n)
                        + " attendu " + std::to_string(expected));
                    ok = false;
                }
                for (int32_t i = 0; i < n; ++i)
                {
                    if (!std::isfinite(dst[i].clip.w) || dst[i].clip.w <= 0.0f)
                    {
                        Logger::error(std::string("[CLIP] ") + label + " : w invalide en sortie");
                        ok = false;
                    }
                }
                return n;
            };

        // --- a) 3 sommets devant : accept trivial, copie BIT A BIT ---
        Build({ -1,-1,-5 }, { 1,-1,-5 }, { 0,1,-5 }, src);
        ClipVertex before[3] = { src[0], src[1], src[2] };
        if (Check("3 devant", 3) == 3)
            for (int k = 0; k < 3; ++k)
                if (std::memcmp(&dst[k].clip, &before[k].clip, sizeof(Vec4f)) != 0)
                {
                    Logger::error("[CLIP] chemin rapide : sommets MODIFIES — le trivial accept ne s'applique pas");
                    ok = false;
                }

        // --- b) 3 sommets derrière le near (d = 0.05 < 0.1) ---
        Build({ -1,-1,-0.05f }, { 1,-1,-0.05f }, { 0,1,-0.05f }, src);
        Check("3 derriere", 0);

        // --- c) 1 sommet devant -> triangle ---
        Build({ 0,1,-5 }, { 1,-1,-0.05f }, { -1,-1,-0.05f }, src);
        Check("1 devant", 3);

        // --- d) 2 sommets devant -> QUADRILATERE ---
        Build({ -1,-1,-5 }, { 1,-1,-5 }, { 0,1,-0.05f }, src);
        Check("2 devant", 4);

        // --- e) sommet EXACTEMENT sur le plan : aucun NaN ---
        Build({ -1,-1,-5 }, { 1,-1,-5 }, { 0,1,-0.1f }, src);
        Check("sommet sur le plan", 3);

        return ok;
    }

    // ════════════════════════════════════════════════════════════
    //  TEST 3 — Le clipping PRÉSERVE le winding.
    //  C'est ce qui garantit que le backface culling reste valide
    //  sur les triangles issus du clipper.
    // ════════════════════════════════════════════════════════════
    bool Test_ClipPreservesWinding()
    {
        const Viewport  vp = Viewport::FullScreen(256, 256);
        const Matrix44f proj = Projection::Perspective(60.0f * TO_RADIAN, 1.0f, 0.1f, 100.0f);

        // Triangle FRONT-FACE traversant le plan near
        const Vec3f v[3] = { {-2,-2,-5}, {2,-2,-5}, {0,2,-0.05f} };

        ClipVertex src[3];
        for (int k = 0; k < 3; ++k) src[k].clip = MulRow(proj, v[k]);

        // Référence : le signe AVANT clipping, sur les 3 sommets d'origine
        Vec3f r0[3];
        for (int k = 0; k < 3; ++k) r0[k] = ToRasterFromView(proj, vp, v[k]);
        const bool refBack = IsBackFacing(EdgeFunction(r0[0], r0[1], r0[2]));

        ClipVertex poly[kMaxClipVertices];
        const int32_t n = ClipTriangleNear(src, poly);
        if (n < 3) { Logger::error("[WINDING] clipping a tout rejete"); return false; }

        bool ok = true;
        for (int32_t q = 1; q + 1 < n; ++q)
        {
            const ClipVertex* t[3] = { &poly[0], &poly[q], &poly[q + 1] };
            Vec3f r[3];
            for (int k = 0; k < 3; ++k)
            {
                const float inv = 1.0f / t[k]->clip.w;
                r[k] = vp.ToRaster({ t[k]->clip.x * inv, t[k]->clip.y * inv, t[k]->clip.z * inv });
            }
            if (IsBackFacing(EdgeFunction(r[0], r[1], r[2])) != refBack)
            {
                Logger::error("[WINDING] triangle " + std::to_string(q)
                    + " issu du clipping a un winding INVERSE");
                ok = false;
            }
        }
        return ok;
    }

} // namespace LV3::Tests