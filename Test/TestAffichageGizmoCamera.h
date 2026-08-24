#pragma once

#include "Scene/registry.hpp"
#include "Rendering/Viewdata.h"


namespace LV3::Tests
{
	void Test_GizmoCountMatchesCameras(Registry& registry);
	void Test_CameraWorldMatrixIsRigid(Registry& registry);
	void Test_GizmoMatchesFrustum(Registry& registry, const ViewData* views, size_t count);
}