#include "pch.h"
#include "Maths/Vectorlib.h"
#include "rendering/Viewport.h"
#include "rendering/Rasterizer.h"
#include "maths/Projection.h"

namespace LV3::Tests
{

    // TestRasterizer — §Winding : la face avant a une aire NEGATIVE en raster
    void TestFrontFaceSign()
    {

        const Viewport  vp = Viewport::FullScreen(800, 800);
        const Matrix44f proj = Projection::Perspective(60.f * TO_RADIAN, 1.0f, 0.1f, 100.f);

        // CCW vu de la camera (origine, regard -Z) => FACE AVANT
        const Vec3f v[3] = { {-1,-1,-5}, {1,-1,-5}, {0,1,-5} };

        Vec3f r[3];
        for (int k = 0; k < 3; ++k)
        {
            const Vec4f c = MulRow(proj, v[k]);
            const float inv = 1.0f / c.w;
            r[k] = vp.ToRaster({ c.x * inv, c.y * inv, c.z * inv });
        }

        const float area = EdgeFunction(r[0], r[1], r[2]);
        assert(area < 0.0f && "\033[31mFace AVANT : l'aire raster doit etre NEGATIVE\033[0m\n");
        assert(IsBackFacing(area) == false);

        // et le miroir : l'ordre inverse doit etre cullé
        assert(IsBackFacing(EdgeFunction(r[0], r[2], r[1])) == true);
        printf("\033[32mSUCCES : TestFrontFaceSign OK \033[0m\n");

    }
}