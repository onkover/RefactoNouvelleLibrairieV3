#include "pch.h"
#include "scene/registry.hpp"
#include "Core/Logger.h"

namespace LV3::Tests
{
    void DebugDumpControllers(Registry& reg)
    {
        Logger::log("=== Controleurs de camera ===");
        for (auto&& [e, cam] : reg.ViewGroup<CameraComponent>())
        {
            const std::string n = reg.TryGet<NameComponent>(e)
                ? reg.getComponent<NameComponent>(e).m_id : "<sans nom>";

            const auto* fps = reg.TryGet<FPSControllerComponent>(e);
            const auto* follow = reg.TryGet<CameraFollowComponent>(e);

            Logger::log("  " + n
                + " | active=" + std::to_string(cam.m_isActive)
                + " prio=" + std::to_string(cam.m_priority)
                + " | FPS=" + (fps ? (fps->m_isEnabled ? "ON" : "OFF") : "ABSENT")
                + " | Follow=" + (follow ? (follow->m_isEnabled ? "ON" : "OFF") : "ABSENT")
                + " | Transform=" + (reg.TryGet<TransformComponent>(e) ? "oui" : "NON"));
        }
    }
}