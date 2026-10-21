#include "pch.h"
#include <random>
#include <cmath>
#include "geometry/MeshClass.h"
#include "maths/Projection.h"
#include "rendering/Viewport.h"
#include "rendering/Rasterizer.h"
#include "rendering/VertexStage.h"
#include "rendering/FaceStage.h"
#include "core/Logger.h"

namespace LV3::Tests
{
#if LV3_DEBUG
    namespace
    {
        // Sphere UV, MEME ordre de faces qu'un mesh genere (anneaux) : les lots de 8 sont coherents.
        void BuildUVSphere(MeshClass& m, int rings, int segs)
        {
            for (int r = 0; r <= rings; ++r)
                for (int s = 0; s <= segs; ++s)
                {
                    const float t = 3.14159265f * float(r) / float(rings);
                    const float p = 6.2831853f * float(s) / float(segs);
                    m.AddVertex({ std::sin(t) * std::cos(p), std::cos(t), std::sin(t) * std::sin(p) });
                }
            for (int r = 0; r < rings; ++r)
                for (int s = 0; s < segs; ++s)
                {
                    const uint32_t a = uint32_t(r * (segs + 1) + s), b = a + uint32_t(segs + 1);
                    m.AddFace(a, b + 1, b);          // orientation sans importance : on compare deux calculs du MEME signe
                    m.AddFace(a, a + 1, b + 1);
                }
            m.AddFace(0, 0, 1);                      // face DEGENEREE volontaire : plan nul, jamais rejetee
        }
    }

    // B5 : pre-filtre "face arriere en espace OBJET".
    //  (1) ObjBackMask8 == scalaire au bit pres
    //  (2) rejetee en objet  =>  arriere pour l'aire EXACTE (double). Si l'ecran float dit "avant",
    //      c'est lui qui se trompe : on le compte (corrections), on ne le tolere pas comme une erreur.
    //  (3) identite : signe(L.E) == signe ecran des que |aire| >= 1 px2  (valide HomogeneousEye)
    // Perspective, orthographique, MIROIR (det < 0), echelle non uniforme moderee.
    bool Test_B5_ObjBackface()
    {
        MeshClass mesh;
        BuildUVSphere(mesh, 31, 64);                 // 3969 faces : le dernier lot est INCOMPLET (1 face)
        mesh.BakePositionsSoA();
        mesh.BakeFacePlanesSoA();
        const FacePlanesSoA& pl = mesh.facePlanesSoA;
        const size_t nF = mesh.faceCount(), nV = mesh.vertexCount();

        const Viewport vp = Viewport::FullScreen(1536, 900);
        const float aspect = 1536.0f / 900.0f;
        SimdVector<RasterVertex> rv(pl.PaddedCount() + nV + 8);   // large : PaddedCount() sommets ecrits

        std::mt19937 rng(42);   // graine FIXE
        std::uniform_real_distribution<float> U(-1.0f, 1.0f), A(0.0f, 6.2831853f);

        uint64_t faces = 0, rejected = 0, bitMis = 0, corrections = 0, violations = 0, idMis = 0;
        uint64_t mirrorInst = 0, orthoInst = 0, skipped = 0, batches = 0;

        for (int inst = 0; inst < 3000; ++inst)
        {
            const bool ortho  = (inst % 5 == 4);
            const bool mirror = (inst % 3 == 1);
            const float sc = std::pow(10.0f, 2.0f * U(rng));          // 0,01 .. 100
            Matrix44f S;
            S[0][0] = (mirror ? -sc : sc) * (1.0f + 0.5f * U(rng));   // non uniforme, x0,5 .. x1,5
            S[1][1] = sc * (1.0f + 0.5f * U(rng));
            S[2][2] = sc * (1.0f + 0.5f * U(rng));

            const float a = A(rng), b = A(rng);
            Matrix44f Ry, Rx;
            Ry[0][0] = std::cos(a); Ry[0][2] = -std::sin(a); Ry[2][0] = std::sin(a); Ry[2][2] = std::cos(a);
            Rx[1][1] = std::cos(b); Rx[1][2] =  std::sin(b); Rx[2][1] = -std::sin(b); Rx[2][2] = std::cos(b);

            const float dist = 3.0f * sc * std::pow(10.0f, 1.2f * std::fabs(U(rng)));
            Matrix44f T;
            T[3][0] = 0.3f * dist * U(rng); T[3][1] = 0.2f * dist * U(rng); T[3][2] = -dist;

            const Matrix44f proj = ortho
                ? Projection::OrthographicCentered(1.2f * dist, aspect, 0.1f, 4.0f * dist)
                : Projection::PerspectiveInfinite(1.0f, aspect, 0.1f);
            const Matrix44f mvp = S * Rx * Ry * T * proj;     // vue = identite

            // Contrat Inside : w > 0 partout, sinon l'instance n'est pas de ce chemin.
            bool inside = true;
            for (size_t i = 0; i < nV && inside; ++i) inside = MulRow(mvp, mesh.vertexPositions[i]).w > 0.0f;
            if (!inside) continue;
            mirrorInst += mirror; orthoInst += ortho;

            ProjectPositions8(mvp, vp, mesh.positionsSoA.x.data(), mesh.positionsSoA.y.data(),
                mesh.positionsSoA.z.data(), mesh.positionsSoA.PaddedCount(), rv.data());

            const Vec4f e = HomogeneousEye(mvp);
            const float tau = ObjBackThreshold(e, pl.maxAbsD);
            const __m256 ex = _mm256_set1_ps(e.x), ey = _mm256_set1_ps(e.y);
            const __m256 ez = _mm256_set1_ps(e.z), ew = _mm256_set1_ps(e.w);
            const __m256 nt = _mm256_set1_ps(-tau);

            for (size_t base = 0; base < nF; base += 8)
            {
                const size_t   n = std::min<size_t>(8, nF - base);
                const uint32_t valid = (1u << n) - 1u;
                const uint32_t ob = valid & ObjBackMask8(pl.nx.data() + base, pl.ny.data() + base,
                    pl.nz.data() + base, pl.d.data() + base, ex, ey, ez, ew, nt);
                ++batches; skipped += (ob == valid);

                for (uint32_t k = 0; k < n; ++k)
                {
                    const size_t f = base + k;
                    ++faces;
                    const float s = ((pl.nx[f] * e.x + pl.ny[f] * e.y) + pl.nz[f] * e.z) + pl.d[f] * e.w;
                    if (bool((ob >> k) & 1u) != (s < -tau)) ++bitMis;                       // (1)

                    const uint32_t* t = &mesh.indices[f * 3];
                    const float area = EdgeFunction(rv[t[0]], rv[t[1]], rv[t[2]]);
                    if (std::fabs(area) >= 1.0f && ((s > 0.0f) != !IsBackFacing(area))) ++idMis;   // (3)

                    if ((ob >> k) & 1u)                                                        // (2)
                    {
                        ++rejected;
                        if (!IsBackFacing(area)) ++corrections;   // l'ecran float dit "avant"
                        const double exact = ExactRasterArea(mvp, vp, mesh.vertexPositions[t[0]],
                            mesh.vertexPositions[t[1]], mesh.vertexPositions[t[2]]);
                        if (exact < -1e-9)                      // la VERITE dit "avant" : vrai rejet fautif
                        {
                            if (violations < 5)
                                Logger::error("[B5] violation : inst " + std::to_string(inst) + " face " + std::to_string(f)
                                    + " s=" + std::to_string(s) + " tau=" + std::to_string(tau) + " aire=" + std::to_string(area));
                            ++violations;
                        }
                    }
                }
            }
        }

        // CONTRE-EXEMPLE du test qui ne teste rien : miroir, ortho et rejets doivent exister.
        const bool nonTrivial = mirrorInst > 0 && orthoInst > 0 && rejected > 0 && rejected < faces;
        Logger::info("[B5] " + std::to_string(faces) + " faces, " + std::to_string(rejected) + " rejetees en objet, "
            + std::to_string(skipped) + "/" + std::to_string(batches) + " lots sautes, desaccords SIMD "
            + std::to_string(bitMis) + ", identite " + std::to_string(idMis) + ", corrections de l'ecran " + std::to_string(corrections)
            + ", violations " + std::to_string(violations));
        return bitMis == 0 && idMis == 0 && violations == 0 && nonTrivial;
    }
#endif
}
