#pragma once
#define GFX_INCLUDE

#include "Rendering/Renderer.h"
#include "Scene/Registry.hpp"
#include "Ressources/ResourceManager.h"
#include "rendering/viewport.h"   // Viewport
#include "rendering/depthbuffer.h" // DepthBuffer
#include "rendering/rasterizer.h"  // RasterizeTriangle
#include "Rendering/ViewData.h"      // ViewData
#include "Rendering/RenderTypes.h"   // ERenderMode


using namespace LV3;

void Init_Render();
void RenderObject(Registry& registry, ResourceManager& rm, FrameBuffer& fb, DepthBuffer& db, ERenderMode mode);
void End_Render();
void kill_Render();
void Clean_Render(FrameBuffer& fb);
void RenderView(Registry& registry, ResourceManager& rm,
    FrameBuffer& fb, DepthBuffer& db,
    const ViewData& view, ERenderMode mode);


#ifdef GFX

#else

#endif
