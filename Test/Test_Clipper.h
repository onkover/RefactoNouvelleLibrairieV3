#pragma once
namespace LV3::Tests
{
    [[nodiscard]] bool Test_FrontFaceSign();
    [[nodiscard]] bool Test_ClipTriangleNear_Cases();
    [[nodiscard]] bool Test_ClipPreservesWinding();
    [[nodiscard]] bool Test_ClipCoverage_NoCrackNoOverlap();   // celui que tu as déjà
}