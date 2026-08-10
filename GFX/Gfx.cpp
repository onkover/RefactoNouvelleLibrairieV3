#define GFX
#include "pch.h"          // ← première ligne, toujours
#include "gfx.h"

#include "Scene/system.hpp"

extern "C" void CleanScreenASM(__m256i*, int);	// ASM clearScreen
extern "C" void CleanScreenV3(void* p, unsigned long long bytes);




//************************************
void Init_Render()
{


	
}

void Clean_Render(FrameBuffer& fb)
{
	//myZBuf->CleandepthBuffer(float(-pFrustum->farClippingPlane));
//	CleanScreenASM((__m256i*)(fb.frameBuffer()), (fb.Width() >> 3) * (fb.Height()));
//	CleanScreenV3(fb.m_Pixels, (fb.m_Width >> 3) * (fb.m_Height));frameBuffer
}


void RenderObject(Registry& registry, ResourceManager& rm, FrameBuffer& fb, DepthBuffer& db, ERenderMode mode)
{
	/*if (!fb.IsBound()) { Logger::warn("RenderSystem : framebuffer non bind"); return; }

	const Entity camEntity = FindActiveCamera(registry);
	if (camEntity == NULL_ENTITY)
	{
		Logger::warn("RenderSystem : aucune camera active");
		return;
	}

	const ViewData view = BuildViewData(
		*registry.TryGet<TransformComponent>(camEntity),
		*registry.TryGet<CameraComponent>(camEntity),
		vp);*/



	//DrawCameraOrientation(myFrustum, myWorld, &myScreen);
}
//void RenderView(Registry& registry, ResourceManager& rm,
//    FrameBuffer& fb, DepthBuffer& db,
//    const ViewData& view, ERenderMode mode)
//{
//    const Viewport& vp = view.viewport;
//    if (!vp.IsValid() || !fb.IsBound()) return;
//
//    LV3_ASSERT(db.Width() == fb.Width() &&
//        db.Height() == fb.Height() &&
//        "DepthBuffer et FrameBuffer doivent avoir la meme taille");
//
//
//    for (auto&& [entity, meshComp, transform] : registry.ViewGroup<MeshComponent, TransformComponent>())
//    {
//        const MeshClass* mesh = rm.GetMesh(meshComp.m_meshHandle);
//        if (!mesh || mesh->faceCount() == 0) continue;
//
//        // 5. Matrice de modèle
//        const Matrix44f& modelMatrix = transform.m_worldMatrix;
//
//        // 6 + 7. AABB monde, puis culling à trois états
//        const AABB3d worldAABB = mesh->GetMeshAABB().Transformed(modelMatrix);
//        if (view.frustum.Classify(worldAABB) == EIntersect::Outside) continue;
//
//        // 9 + 10. UNE matrice Model · View · Projection
//        const Matrix44f mvp = modelMatrix * view.viewProjectionMatrix;
//
//        const uint8_t vpf = mesh->vertsPerFace;      // 3 (triangles) ou 4 (quads)
//        const size_t  fc = mesh->faceCount();
//
//        for (size_t f = 0; f < fc; ++f)
//        {
//            const uint32_t base = static_cast<uint32_t>(f) * vpf;
//
//            // --- 10a. Local -> CLIP. SoA INDEXÉ : indices[] puis vertexPositions[]
//            //     /!\ mesh->indices[base + k], PAS indices[base] + k
//            Vec4f c[4];
//            for (uint8_t k = 0; k < vpf; ++k)
//                c[k] = MulRow(mvp, mesh->vertexPositions[mesh->indices[base + k]]);
//
//            // --- 10b. Rejet du near sur TOUS les sommets.
//            //     Diviser par un w négatif ramènerait le point en MIROIR.
//            bool behind = false;
//            for (uint8_t k = 0; k < vpf; ++k)
//                if (c[k].w <= view.nearPlane) { behind = true; break; }
//            if (behind) continue;
//
//            // --- 10c + 10d. /w -> NDC -> RASTER (flip Y dans ToRaster)
//            Vec3f r[4];
//            for (uint8_t k = 0; k < vpf; ++k)
//            {
//                const float inv = 1.0f / c[k].w;
//                r[k] = vp.ToRaster({ c[k].x * inv, c[k].y * inv, c[k].z * inv });
//            }
//
//            // --- 8. Backface culling. Une face plane et convexe a un sens de
//            //     parcours unique : le triangle (0,1,2) décide pour toute la face.
//const float area = EdgeFunction(r[0], r[1], r[2]);
//            if (area <= 0.0f) continue;                      // backface
//
//            if (mode == ERenderMode::Wireframe)
//            {
//                const Color w = MakeColor(255, 255, 255);
//                for (uint8_t k = 0; k < vpf; ++k)
//                    DrawLineClipped(fb, vp, r[k], r[(k + 1) % vpf], w);
//            }
//            else
//            {
//                const Color col = FaceColor(int(f));
//                for (uint8_t t = 0; t + 2 < vpf; ++t)
//                {
//                    SolidContext ctx{ &fb, &db, col,
//                                      r[0].z, r[t + 1].z, r[t + 2].z };
//
//                    RasterizeTriangle({ r[0].x,     r[0].y     },
//                                      { r[t + 1].x, r[t + 1].y },
//                                      { r[t + 2].x, r[t + 2].y },
//                                      vp, &ShadeFragment_Solid, &ctx);
//                }
//            }
//        }
//    }
//}

void End_Render()
{

}