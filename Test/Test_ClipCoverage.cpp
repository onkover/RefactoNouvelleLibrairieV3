#include "pch.h"
#include "Maths/Vectorlib.h"
#include "rendering/Viewport.h"
#include "rendering/Rasterizer.h"
#include "maths/Projection.h"
#include "rendering/clipper.h"
#include "Core/Logger.h"

using namespace LV3;

namespace LV3::Tests
{

    static void EmitForTest(const ClipVertex& a, const ClipVertex& b, const ClipVertex& c,
        const Viewport& vp, std::vector<uint16_t>& counts)
    {
        const ClipVertex* v[3] = { &a, &b, &c };
        Vec3f r[3];
        for (int k = 0; k < 3; ++k)
        {
            const float inv = 1.0f / v[k]->clip.w;
            r[k] = vp.ToRaster({ v[k]->clip.x * inv, v[k]->clip.y * inv, v[k]->clip.z * inv });
        }
        if (IsBackFacing(EdgeFunction(r[0], r[1], r[2]))) return;

        struct Ctx { std::vector<uint16_t>* c; int w; } ctx{ &counts, vp.width };

        RasterizeTriangle({ r[0].x, r[0].y }, { r[1].x, r[1].y }, { r[2].x, r[2].y }, vp,
            [](int32_t x, int32_t y, const BarycentricWeights&, void* u)
            {
                auto* c = static_cast<Ctx*>(u);
                ++(*c->c)[size_t(y) * c->w + x];
            }, &ctx);
    }


    bool Test_ClipCoverage_NoCrackNoOverlap()
    {
        const Viewport  vp = Viewport::FullScreen(256, 256);
        const Matrix44f proj = Projection::Perspective(60.f * TO_RADIAN, 1.0f, 0.1f, 100.f);

        // Quad en espace VUE traversant le plan near (d = -z).
        //   A, B : d = 0.05  < near  -> DEHORS
        //   C, D : d = 5.0   > near  -> DEDANS
        const Vec3f q[4] = { {-2,-2,-0.05f}, { 2,-2,-0.05f}, { 2, 2,-5.f}, {-2, 2,-5.f} };

        ClipVertex cv[4];
        for (int k = 0; k < 4; ++k) cv[k].clip = MulRow(proj, q[k]);

        std::vector<uint16_t> counts(size_t(vp.width) * vp.height, 0);

        // Éventail : (0,1,2) et (0,2,3) — ils PARTAGENT l'arête 0—2,
        // parcourue en sens opposé par les deux triangles. C'est LE cas critique.
        for (int t = 0; t + 2 < 4; ++t)
        {
            const ClipVertex tri[3] = { cv[0], cv[t + 1], cv[t + 2] };
            ClipVertex poly[kMaxClipVertices];
            const int32_t n = ClipTriangleNear(tri, poly);
            for (int32_t i = 1; i + 1 < n; ++i)
                EmitForTest(poly[0], poly[i], poly[i + 1], vp, counts);
        }

        // --- Verdict ---
        //int doubles = 0, holes = 0;
        //for (int y = 1; y < vp.height - 1; ++y)
        //    for (int x = 1; x < vp.width - 1; ++x)
        //    {
        //        const uint16_t c = counts[size_t(y) * vp.width + x];
        //        if (c > 1) ++doubles;
        //        // Trou = pixel vide ENTOURÉ de pixels couverts (donc intérieur au quad)
        //        if (c == 0
        //            && counts[size_t(y) * vp.width + (x - 1)]
        //            && counts[size_t(y) * vp.width + (x + 1)]
        //            && counts[size_t(y - 1) * vp.width + x]
        //            && counts[size_t(y + 1) * vp.width + x]) ++holes;
        //    }

        //Logger::log("[COVERAGE] doubles=" + std::to_string(doubles)
        //    + "  trous=" + std::to_string(holes));

        //return doubles == 0 && holes == 0;
        // --- Verdict ---
        const auto At = [&](int x, int y) -> uint16_t
            {
                return counts[size_t(y) * vp.width + x];
            };

        // Trou = pixel vide dont les 4 voisins sont couverts (donc INTÉRIEUR au quad).
        // Ce critère rend le test insensible aux bords : seules les fissures
        // qui ne devraient pas exister sont comptées.
        const auto IsHole = [&](int x, int y)
            {
                return At(x, y) == 0
                    && At(x - 1, y) && At(x + 1, y)
                    && At(x, y - 1) && At(x, y + 1);
            };

        int doubles = 0, holes = 0;
        int firstX = -1, firstY = -1, lastX = -1, lastY = -1;

        for (int y = 1; y < vp.height - 1; ++y)
            for (int x = 1; x < vp.width - 1; ++x)
            {
                if (At(x, y) > 1) ++doubles;

                if (IsHole(x, y))
                {
                    ++holes;
                    if (firstX < 0) { firstX = x; firstY = y; }
                    lastX = x; lastY = y;
                }
            }

        Logger::log("[COVERAGE] doubles=" + std::to_string(doubles)
            + "  trous=" + std::to_string(holes));

        if (holes > 0)
            Logger::log("[COVERAGE] premier (" + std::to_string(firstX) + "," + std::to_string(firstY)
                + ")  dernier (" + std::to_string(lastX) + "," + std::to_string(lastY) + ")");

        return doubles == 0 && holes == 0;
    }

} // namespace LV3::Tests