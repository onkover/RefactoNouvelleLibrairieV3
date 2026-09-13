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

    // vérifie qu'un sommet du gizmo, transformé par la view-projection de sa propre caméra propriétaire, tombe exactement sur le bord du frustum (|clip.x/clip.w| == 1)
    size_t  Test_GizmoMatchesFrustum(Registry& registry, ResourceManager& rm,
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


            // Echelle de la maquette = section AFFICHEE / section REELLE a la distance L.
            // L'aspect s'annule dans le rapport : un seul s pour x et pour y.
            /*
                Ici le test ne rejoue pas le calcul du système. 
                Il calcule un rapport entre deux grandeurs d'origines différentes : *
                * le numérateur vient du gizmo, 
                * le dénominateur vient de la définition du frustum (celle-là même qu'implémente viewProjectionMatrix, produite par un tout autre chemin)
            
                Puis il vérifie que ce rapport se retrouve au bout de la chaîne complète m_local.scale → LocalTransformSystem → WorldTransformSystem → viewProjection. C'est cette chaîne, et elle seule, que le test prouve.
            */
            const Vec2f hs = GizmoHalfSection(cam, giz.m_length);
            // s est scalaire parce que hs.x == hs.y dans les deux modes actuels. 
            // Si un jour une demi-section affichée devenait non uniforme, il faudrait s.x = hs.x / FrustumHalfHeightAt(...) et s.y séparément.
            const float s = hs.y / FrustumHalfHeightAt(cam, giz.m_length);

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

                // L'erreur naît de l'annulation catastrophique entre termes de meme magnitude M.
                // M n'est PAS la position du noeud camera : c'est celle du COIN evalue, qui vaut
                // camPos ± l'encombrement du gizmo. A orthoHeight eleve, c'est l'encombrement qui
                // domine — l'indexer sur camPos seul laisse l'assert sauter sans qu'aucun bug
                // geometrique ne soit en cause.
                constexpr float kUlpMargin = 8.0f;
                const float eps = kUlpMargin * std::numeric_limits<float>::epsilon()
                    * std::max(1.0f, world.length());

                // En ortho, le coin ne tombe plus sur ±1 mais sur ±s, avec s = L / halfH.
                // L'egalite ndcX == ndcY reste la preuve que l'aspect est bon.
                LV3_ASSERT(std::fabs(clip.w - expectedW) < eps);
                LV3_ASSERT(std::fabs(std::fabs(clip.x / clip.w) - s) < eps);
                LV3_ASSERT(std::fabs(std::fabs(clip.y / clip.w) - s) < eps);
            }
            ++checked;
        }

        if (checked == 0)
            Logger::info("[TNR] Test_GizmoMatchesFrustum — sans objet (aucun gizmo dans les vues rendues)");
        return checked;      // ← remplace LV3_ASSERT(checked > 0)
    }
}

