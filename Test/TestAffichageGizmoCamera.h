#pragma once

#include "Scene/registry.hpp"
#include "Rendering/Viewdata.h"
//#include "Scene/SpawnCameraGizmos.hpp"
#include "Scene/DebugGizmos.hpp"


namespace LV3::Tests
{
	void Test_GizmoCountMatchesCameras(Registry& registry);
	void Test_CameraWorldMatrixIsRigid(Registry& registry);
	//void Test_GizmoMatchesFrustum(Registry& registry, const ViewData* views, size_t count);
	//void Test_GizmoMatchesFrustum(Registry& registry, const ViewData* views, size_t count, const GizmoAssets& assets);
	void Test_GizmoMatchesFrustum(Registry& registry, ResourceManager& rm,
		const ViewData* views, size_t count,
		const GizmoAssets& assets);

}