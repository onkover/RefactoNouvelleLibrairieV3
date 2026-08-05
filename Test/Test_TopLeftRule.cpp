#include "pch.h"

#include "rendering/Rasterizer.h"
#include <vector>
#include <cassert>
#include <cstdint>

using namespace LV3;

// Contexte local au test — n'a AUCUNE raison d'exister dans Fragment.h
struct CountContext
{
    std::vector<uint8_t>* counts;
    int32_t width;
};

// Callback local au test — n'a AUCUNE raison d'exister dans Fragment.cpp
static void ShadeFragment_Count(int32_t x, int32_t y, const BarycentricWeights&, void* userData)
{
    auto* ctx = static_cast<CountContext*>(userData);
    (*ctx->counts)[y * ctx->width + x]++;
}

bool Test_TopLeftRule_NoDoubleCoverage()
{
    constexpr int32_t W = 400;
    constexpr int32_t H = 300;

    // 1. Le vecteur appartient à CETTE fonction — c'est le point clé
    std::vector<uint8_t> counts(W * H, 0);

    // 2. CountContext ne sert QUE de véhicule pour le void*
    CountContext ctx{ &counts, W };

    // 3. Quad découpé en deux triangles partageant la diagonale (0,0)→(400,300)
    Vec2f a{ 0,   0 };
    Vec2f b{ 400, 0 };
    Vec2f c{ 400, 300 };
    Vec2f d{ 0,   300 };

    RasterizeTriangle(a, b, c, W, H, ShadeFragment_Count, &ctx);
    RasterizeTriangle(a, c, d, W, H, ShadeFragment_Count, &ctx);

    // 4. Ici, ctx est devenu inutile. On lit directement `counts`.
    for (int32_t i = 0; i < W * H; ++i)
    {
        if (counts[i] > 1)
        {
            printf("Top-left rule violee : pixel (%d,%d) rasterise %d fois\n",
                i % W, i / W, counts[i]);
            return false;
        }
    }
    return true;
}

