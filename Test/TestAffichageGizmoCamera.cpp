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

    void Test_GizmoMatchesFrustum(Registry& registry, ResourceManager& rm,
        const ViewData* views, size_t count,
        const GizmoAssets& assets)
    {
        constexpr float kEps = 1e-4f;
        static const Vec3f kCorners[4] = { {-1,-1,-1}, {1,-1,-1}, {1,1,-1}, {-1,1,-1} };

        size_t checked = 0;

        for (auto&& [e, giz, trGiz, mcGiz] :
            registry.ViewGroup<CameraGizmoComponent, TransformComponent, MeshComponent>())
        {
            const auto& cam = registry.getComponent<CameraComponent>(giz.m_owner);
            const bool  isOrtho = (cam.m_projection == EProjectionType::Orthographic);

            // ── NIVEAU ENTITE ────────────────────────────────────────────
            // 1. Le handle designe le bon asset.
            LV3_ASSERT(mcGiz.m_meshHandle.id == assets.For(cam.m_projection).id);

            // 2. L'asset contient bien la geometrie attendue.
            //    Un handle correct ne prouve rien sur le contenu du fichier.
            const MeshClass* mg = rm.GetMesh(mcGiz.m_meshHandle);
            LV3_ASSERT(mg);
            LV3_ASSERT(mg->vertsPerFace == 3);
            LV3_ASSERT(mg->faceCount() == (isOrtho ? 14u : 8u));

            const float expectedW = isOrtho ? 1.0f : giz.m_length;

            const ViewData* vd = nullptr;
            for (size_t i = 0; i < count; ++i)
                if (views[i].m_sourceCamera == giz.m_owner) { vd = &views[i]; break; }

            if (!vd) continue;   // camera non rendue cette frame : rien a comparer

            // ── NIVEAU COIN ──────────────────────────────────────────────
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

        LV3_ASSERT(checked > 0);
    }
}

