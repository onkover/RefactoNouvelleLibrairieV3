#include "pch.h"
#include "TestAffichageGizmoCamera.h"
#include "Rendering/ViewData.h"
#include "maths/vectorlib.h"
#include "Scene/system.hpp"
#include "Rendering/rasterizer.h"
#include "Core/logger.h"

//, const std::string gizmoMesh
namespace LV3::Tests
{
    void Test_GizmoCountMatchesCameras(Registry& registry)
    {
        size_t expected = 0;
        for (auto&& [e, cam] : registry.ViewGroup<CameraComponent>())
            if (cam.m_gizmoLength > 0.0f) ++expected;

        size_t actual = 0;
        for (auto&& [e, giz] : registry.ViewGroup<CameraGizmoComponent>())
        {
            ++actual;
            LV3_ASSERT(registry.hasComponent<CameraComponent>(giz.m_owner));  // pas d'orphelin
            LV3_ASSERT(registry.hasComponent<DebugVisualComponent>(e));
        }
        LV3_ASSERT(actual == expected);
    }

    void Test_CameraWorldMatrixIsRigid(Registry& registry)
    {
        constexpr float kEps = 1e-4f;
        for (auto&& [e, cam, tr] : registry.ViewGroup<CameraComponent, TransformComponent>())
        {
            const Matrix44f& w = tr.m_worldMatrix;
            for (int r = 0; r < 3; ++r)
            {
                const Vec3f axis{ w[r][0], w[r][1], w[r][2] };
                LV3_ASSERT(std::fabs(axis.length() - 1.0f) < kEps);   // norme unitaire
            }
            // orthogonalite : sinon inverseRigid() ment sans planter
            const Vec3f X{ w[0][0], w[0][1], w[0][2] };
            const Vec3f Y{ w[1][0], w[1][1], w[1][2] };
            const Vec3f Z{ w[2][0], w[2][1], w[2][2] };
            LV3_ASSERT(std::fabs(X.dotProduct(Y)) < kEps);
            LV3_ASSERT(std::fabs(X.dotProduct(Z)) < kEps);
            LV3_ASSERT(std::fabs(Y.dotProduct(Z)) < kEps);
        }
    }

    //void Test_GizmoMatchesFrustum(Registry& registry, const ViewData* views, size_t count)
    //{

    //    constexpr float kEps = 1e-4f;
    //    static const Vec3f kCorners[4] = { {-1,-1,-1}, {1,-1,-1}, {1,1,-1}, {-1,1,-1} };

    //    size_t checked = 0;

    //    for (auto&& [e, giz, trGiz] : registry.ViewGroup<CameraGizmoComponent, TransformComponent>())
    //    {
    //        // Retrouve LA vue reellement construite pour la camera de ce gizmo.
    //        const ViewData* vd = nullptr;
    //        for (size_t i = 0; i < count; ++i)
    //            if (views[i].m_sourceCamera == giz.m_owner) { vd = &views[i]; break; }

    //        if (!vd) continue;   // camera non rendue cette frame : rien a comparer

    //        //const auto& cam = registry.getComponent<CameraComponent>(giz.m_owner);
    //        //Logger::log("--- gizmo owner=" + std::to_string(giz.m_owner)
    //        //    + " fov=" + std::to_string(cam.m_fovYDeg)
    //        //    + " L=" + std::to_string(giz.m_length)
    //        //    + " scale=(" + std::to_string(trGiz.m_local.scale.x) + ", "
    //        //    + std::to_string(trGiz.m_local.scale.y) + ", "
    //        //    + std::to_string(trGiz.m_local.scale.z) + ")"
    //        //    + " vpAspect=" + std::to_string(vd->viewport.Aspect())
    //        //    + " vp=" + std::to_string(vd->viewport.width) + "x"
    //        //    + std::to_string(vd->viewport.height));


    //        for (const Vec3f& c : kCorners)
    //        {

    //            const Vec4f w4 = MulRow(trGiz.m_worldMatrix, c);
    //            LV3_ASSERT(std::fabs(w4.w - 1.0f) < kEps);

    //            const Vec3f world{ w4.x, w4.y, w4.z };
    //            const Vec4f clip = MulRow(vd->viewProjectionMatrix, world);

    //            //Logger::log("    ndcX=" + std::to_string(clip.x / clip.w)
    //            //    + "  ndcY=" + std::to_string(clip.y / clip.w)
    //            //    + "  clipW=" + std::to_string(clip.w));

    //            // La camera parente est rigide (test 3) : la norme des axes de la matrice
    //            // monde du gizmo EST son scale local.
    //            const Matrix44f& W = trGiz.m_worldMatrix;
    //            const float lenX = (Vec3f{ W[0][0], W[0][1], W[0][2] }).length();
    //            const float lenY = (Vec3f{ W[1][0], W[1][1], W[1][2] }).length();
    //            const float lenZ = (Vec3f{ W[2][0], W[2][1], W[2][2] }).length();

    //            LV3_ASSERT(std::fabs(lenX - trGiz.m_local.scale.x) < 1e-4f);
    //            LV3_ASSERT(std::fabs(lenY - trGiz.m_local.scale.y) < 1e-4f);
    //            //LV3_ASSERT(std::fabs(lenZ - giz.m_length) < 1e-4f);

    //            LV3_ASSERT(clip.w > 0.0f);
    //            LV3_ASSERT(std::fabs(std::fabs(clip.x / clip.w) - 1.0f) < kEps);
    //            LV3_ASSERT(std::fabs(std::fabs(clip.y / clip.w) - 1.0f) < kEps);
    //        }
    //        ++checked;
    //    }

    //    // SANS CECI, un test qui ne verifie RIEN passe au vert.
    //    LV3_ASSERT(checked > 0);
    //}

    void Test_GizmoMatchesFrustum(Registry& registry, const ViewData* views, size_t count, const GizmoAssets& assets)
    {
        constexpr float kEps = 1e-4f;
        static const Vec3f kCorners[4] = { {-1,-1,-1}, {1,-1,-1}, {1,1,-1}, {-1,1,-1} };

        size_t checked = 0;

        // MeshComponent ajoute au groupe : le test verifie desormais AUSSI l'asset.
        for (auto&& [e, giz, trGiz, mcGiz] :
            registry.ViewGroup<CameraGizmoComponent, TransformComponent, MeshComponent>())
        {
            const auto& cam = registry.getComponent<CameraComponent>(giz.m_owner);

            // ── NIVEAU ENTITE : une seule fois par gizmo ──────────────────
            LV3_ASSERT(mcGiz.m_meshHandle.id == assets.For(cam.m_projection).id);

            const bool  isOrtho = (cam.m_projection == EProjectionType::Orthographic);
            const float expectedW = isOrtho ? 1.0f : giz.m_length;

            // Retrouve LA vue reellement construite pour la camera de ce gizmo.
            const ViewData* vd = nullptr;
            for (size_t i = 0; i < count; ++i)
                if (views[i].m_sourceCamera == giz.m_owner) { vd = &views[i]; break; }

            if (!vd) continue;   // camera non rendue cette frame : rien a comparer

            //Logger::log("  proj=" + std::string(cam.m_projection == EProjectionType::Orthographic
            //    ? "ORTHO" : "PERSP")
            //    + "  meshId=" + std::to_string(mcGiz.m_meshHandle.id)
            //    + "  wantId=" + std::to_string(assets.For(cam.m_projection).id)
            //    + "  orthoHeight=" + std::to_string(cam.m_orthoHeight));


            // ── NIVEAU COIN : quatre fois par gizmo ───────────────────────
            for (const Vec3f& c : kCorners)
            {
                const Vec4f w4 = MulRow(trGiz.m_worldMatrix, c);
                LV3_ASSERT(std::fabs(w4.w - 1.0f) < kEps);          // matrice affine

                const Vec3f world{ w4.x, w4.y, w4.z };
                const Vec4f clip = MulRow(vd->viewProjectionMatrix, world);

                LV3_ASSERT(std::fabs(clip.w - expectedW) < kEps);   // L, ou 1 en ortho
                LV3_ASSERT(std::fabs(std::fabs(clip.x / clip.w) - 1.0f) < kEps);
                LV3_ASSERT(std::fabs(std::fabs(clip.y / clip.w) - 1.0f) < kEps);
            }
            ++checked;
        }

        LV3_ASSERT(checked > 0);   // un test qui n'a rien verifie n'est pas vert
    }
}

