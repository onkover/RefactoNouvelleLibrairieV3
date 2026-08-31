#include "pch.h"
#include "Scene/registry.hpp"
#include "Scene/system.hpp"

using namespace LV3;

namespace LV3::Tests
{

#ifdef _DEBUG

    // ════════════════════════════════════════════════════════════════
    //  TestCameraZoom — verrouille CameraZoomSystem (Annexe A7).
    //  Chaque cas construit sa propre entite : aucun etat partage entre
    //  sections, donc aucune section ne peut faire reussir la suivante
    //  par accident (meme discipline que TestRasterizer, L04 P2).
    // ════════════════════════════════════════════════════════════════
    void TestCameraZoom()
    {
        Logger::info("[TNR] TestCameraZoom...\n");

        constexpr float kRatio = 0.90f;
        constexpr float kEpsilon = 1e-4f;

        auto MakeControlledCamera = [](Registry& reg, EProjectionType proj, ELensModel lens,
            bool controllerEnabled = true) -> Entity
            {
                Entity e = reg.CreateEntity();

                CameraComponent cam{};
                cam.m_projection = proj;
                cam.m_lensModel = lens;
                cam.m_orthoHeight = 120.0f;
                cam.m_fovYDeg = 45.0f;
                cam.m_focalLengthMm = 35.0f;
                reg.addComponent(e, cam);

                FPSControllerComponent ctrl{};
                ctrl.m_isEnabled = controllerEnabled;
                ctrl.m_sprintMultiplier = 3.0f;
                reg.addComponent(e, ctrl);

                return e;
            };

        // ----------------------------------------------------------------
        // §1 — Orthographique : un cran avant retrecit de 10% pile.
        // ----------------------------------------------------------------
        {
            Registry reg;
            Entity cam = MakeControlledCamera(reg, EProjectionType::Orthographic, ELensModel::FieldOfView);

            InputState in{};
            in.wheelDelta = 1;
            CameraZoomSystem(reg, in, 0.0f);

            const float expected = 120.0f * kRatio;
            const float actual = reg.getComponent<CameraComponent>(cam).m_orthoHeight;
            assert(std::fabs(actual - expected) < kEpsilon);
        }

        // ----------------------------------------------------------------
        // §2 — Dezoomer est l'inverse exact de zoomer.
        // ----------------------------------------------------------------
        {
            Registry reg;
            Entity cam = MakeControlledCamera(reg, EProjectionType::Orthographic, ELensModel::FieldOfView);

            InputState in{};
            in.wheelDelta = -1;
            CameraZoomSystem(reg, in, 0.0f);

            const float expected = 120.0f / kRatio;   // agrandit
            const float actual = reg.getComponent<CameraComponent>(cam).m_orthoHeight;
            assert(std::fabs(actual - expected) < kEpsilon);
            assert(actual > 120.0f);                  // sens qualitatif, independant de la formule
        }

        // ----------------------------------------------------------------
        // §3 — Perspective FieldOfView : meme sens que l'ortho (retrecit).
        // ----------------------------------------------------------------
        {
            Registry reg;
            Entity cam = MakeControlledCamera(reg, EProjectionType::Perspective, ELensModel::FieldOfView);

            InputState in{};
            in.wheelDelta = 1;
            CameraZoomSystem(reg, in, 0.0f);

            const float expected = 45.0f * kRatio;
            const float actual = reg.getComponent<CameraComponent>(cam).m_fovYDeg;
            assert(std::fabs(actual - expected) < kEpsilon);
            assert(actual < 45.0f);                   // retrecit, comme l'ortho
        }

        // ----------------------------------------------------------------
        // §4 — Filmback : sens INVERSE. Le piege central de l'A7 §5.
        // ----------------------------------------------------------------
        {
            Registry reg;
            Entity cam = MakeControlledCamera(reg, EProjectionType::Perspective, ELensModel::Filmback);

            InputState in{};
            in.wheelDelta = 1;
            CameraZoomSystem(reg, in, 0.0f);

            const float expected = 35.0f / kRatio;   // ALLONGE
            const float wrongIfCopied = 35.0f * kRatio;    // ce que donnerait un copier-coller naif
            const float actual = reg.getComponent<CameraComponent>(cam).m_focalLengthMm;

            assert(std::fabs(actual - expected) < kEpsilon);
            assert(actual > 35.0f);                        // sens qualitatif
            assert(std::fabs(actual - wrongIfCopied) > kEpsilon);  // prouve qu'on n'a PAS le defaut
        }

        // ----------------------------------------------------------------
        // §5 — Sprint multiplie l'exposant, pas seulement l'affichage.
        // ----------------------------------------------------------------
        {
            Registry reg;
            Entity cam = MakeControlledCamera(reg, EProjectionType::Orthographic, ELensModel::FieldOfView);

            InputState in{};
            in.wheelDelta = 1;
            in.sprint = true;                     // ctrl.m_sprintMultiplier == 3.0f
            CameraZoomSystem(reg, in, 0.0f);

            const float expected = 120.0f * std::pow(kRatio, 3.0f);
            const float actual = reg.getComponent<CameraComponent>(cam).m_orthoHeight;
            assert(std::fabs(actual - expected) < kEpsilon);

            // Non-regression directe : le sprint doit produire un effet MESURABLEMENT
            // different d'un cran normal, pas juste "un peu plus".
            const float withoutSprint = 120.0f * kRatio;
            assert(std::fabs(actual - withoutSprint) > 1.0f);
        }

        // ----------------------------------------------------------------
        // §6 — Portee : sans controleur actif, la molette ne touche rien.
        //      Verrouille la decision d'architecture de l'A7 §6
        //      (FPSControllerComponent, PAS m_isActive).
        // ----------------------------------------------------------------
        {
            Registry reg;
            Entity cam = MakeControlledCamera(reg, EProjectionType::Orthographic, ELensModel::FieldOfView,
                /*controllerEnabled=*/false);

            InputState in{};
            in.wheelDelta = 5;
            CameraZoomSystem(reg, in, 0.0f);

            const float actual = reg.getComponent<CameraComponent>(cam).m_orthoHeight;
            assert(std::fabs(actual - 120.0f) < kEpsilon);   // inchange
        }

        // ----------------------------------------------------------------
        // §7 — wheelDelta == 0 : early-return, aucun effet de bord.
        // ----------------------------------------------------------------
        {
            Registry reg;
            Entity cam = MakeControlledCamera(reg, EProjectionType::Perspective, ELensModel::Filmback);

            InputState in{};   // wheelDelta == 0 par defaut
            CameraZoomSystem(reg, in, 0.0f);

            const float actual = reg.getComponent<CameraComponent>(cam).m_focalLengthMm;
            assert(std::fabs(actual - 35.0f) < kEpsilon);
        }

        // ----------------------------------------------------------------
        // §8 — Clamps sous rafale extreme : ni inf, ni NaN, ni depassement.
        // ----------------------------------------------------------------
        {
            struct Case { EProjectionType proj; ELensModel lens; int wheel; const char* label; };
            const Case cases[] = {
                { EProjectionType::Orthographic, ELensModel::FieldOfView, +200, "ortho zoom max"   },
                { EProjectionType::Orthographic, ELensModel::FieldOfView, -200, "ortho dezoom max" },
                { EProjectionType::Perspective,  ELensModel::FieldOfView, +200, "fov zoom max"     },
                { EProjectionType::Perspective,  ELensModel::FieldOfView, -200, "fov dezoom max"   },
                { EProjectionType::Perspective,  ELensModel::Filmback,    +200, "focale zoom max"  },
                { EProjectionType::Perspective,  ELensModel::Filmback,    -200, "focale dezoom max"},
            };

            for (const Case& c : cases)
            {
                Registry reg;
                Entity cam = MakeControlledCamera(reg, c.proj, c.lens);

                InputState in{};
                in.wheelDelta = c.wheel;
                CameraZoomSystem(reg, in, 0.0f);

                const CameraComponent& result = reg.getComponent<CameraComponent>(cam);
                const float values[] = { result.m_orthoHeight, result.m_fovYDeg, result.m_focalLengthMm };

                for (float v : values)
                {
                    assert(std::isfinite(v));   // pas d'inf, pas de NaN
                    assert(v > 0.0f);           // aucune borne minimale n'est <= 0
                }

                printf("  [%-20s] orthoHeight=%8.2f fov=%6.2f focal=%7.1f\n",
                    c.label, result.m_orthoHeight, result.m_fovYDeg, result.m_focalLengthMm);
            }
        }

        Logger::success("[TNR] TestCameraZoom : tous les cas passent\n\n");
    }

#endif // _DEBUG
}