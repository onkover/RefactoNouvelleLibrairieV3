#include "pch.h"
#include <random>
#include <cstring>
#include "rendering/Viewport.h"
#include "rendering/Rasterizer.h"
#include "rendering/FaceStage.h"
#include "rendering/Renderer.h"
#include "core/Logger.h"
#include "TestRandom.h"   // tirages portables MSVC / g++

namespace LV3::Tests
{
#if LV3_DEBUG
    namespace
    {
        constexpr int32_t kW = 211, kH = 157;                 // non multiples de 8
        const Viewport kViews[2] = { Viewport::FullScreen(kW, kH), Viewport{ 23, 11, 150, 120 } };

        // Sommets RASTER deja projetes. Triangles minuscules (V1), quelques moyens (le noyau),
        // sommets sur des centres (w == 0 exact), aretes horizontales, faces des deux sens,
        // debordements de la vue (boite rognee a 1 x 1).
        void MakeMesh(uint32_t seed, int nFaces, std::vector<RasterVertex>& rv, std::vector<uint32_t>& idx)
        {
            std::mt19937 rng(seed);
            //std::uniform_real_distribution<float> U(0.0f, 1.0f);
            TestRand01 U;            // [0, 1), meme suite sur tous les compilateurs

            rv.clear(); idx.clear();
            for (int f = 0; f < nFaces; ++f)
            {
                const float u = U(rng);
                const float size = u < 0.75f ? 0.2f + 1.3f * U(rng)        // boite 1 x 1 le plus souvent
                    : u < 0.95f ? 1.5f + 2.5f * U(rng)        // 2 a 4 pixels
                    : 4.0f + 12.0f * U(rng);      // assez grand pour C2
                const float cx = -3.0f + (kW + 6.0f) * U(rng), cy = -3.0f + (kH + 6.0f) * U(rng);
                RasterVertex v[3];
                for (int k = 0; k < 3; ++k)
                {
                    const float w = 0.5f + 40.0f * U(rng);
                    v[k] = { cx + (U(rng) - 0.5f) * size, cy + (U(rng) - 0.5f) * size, 0.1f / w, 1.0f / w };
                }
                const float e = U(rng);
                if (e < 0.15f)      for (auto& p : v) { p.x = std::floor(p.x) + 0.5f; p.y = std::floor(p.y) + 0.5f; }
                else if (e < 0.25f) v[1].y = v[0].y;                        // arete horizontale
                else if (e < 0.30f) { v[0].x = std::floor(v[0].x) + 0.5f; v[0].y = std::floor(v[0].y) + 0.5f; } // un sommet SUR le centre
                const uint32_t b = uint32_t(rv.size());
                for (auto& p : v) rv.push_back(p);
                idx.push_back(b); idx.push_back(b + 1); idx.push_back(b + 2);

                // EGALITES DE Z entre les deux chemins : un grand triangle (noyau), puis un
                // minuscule (C3) DANS le meme lot, au meme z constant. Le premier dessine gagne
                // (test strict z > dst) : seul l'ordre croissant des couloirs donne la bonne couleur.
                // (La paire tombe le plus souvent dans le meme lot ; sinon elle teste l'ordre entre lots.)
                if (U(rng) < 0.05f)
                {
                    const float z = 0.1f / (0.5f + 40.0f * U(rng));
                    const float px = std::floor(cx) + 0.5f, py = std::floor(cy) + 0.5f;   // un centre
                    RasterVertex big[3] = { { px - 6, py - 5, z, 1 }, { px + 7, py - 4, z, 1 }, { px, py + 7, z, 1 } };
                    RasterVertex small[3] = { { px - 0.3f, py - 0.3f, z, 1 }, { px + 0.4f, py - 0.2f, z, 1 }, { px, py + 0.4f, z, 1 } };
                    for (RasterVertex* tri : { big, small })
                    {
                        if (!(EdgeFunction(tri[0], tri[1], tri[2]) < 0.0f)) std::swap(tri[1], tri[2]);   // face AVANT
                        const uint32_t q = uint32_t(rv.size());
                        for (int k = 0; k < 3; ++k) rv.push_back(tri[k]);
                        idx.push_back(q); idx.push_back(q + 1); idx.push_back(q + 2);
                    }
                    f += 2;
                }
            }
            // Les lots lisent 24 indices : nombre de faces ramene a un multiple de 8.
            while ((idx.size() / 3) % 8 != 0) idx.resize(idx.size() - 3);
        }

        struct Target
        {
            static constexpr int32_t kPitchPx = kW + 5;
            std::vector<uint32_t> pixels = std::vector<uint32_t>(size_t(kPitchPx) * kH, 0u);
            FrameBuffer fb;
            DepthBuffer db;
            Target() { fb.Bind(pixels.data(), kPitchPx * 4, kW, kH); db.Resize(kW, kH); }
        };

        bool SameImage(Target& a, Target& b, size_t& cd, size_t& dd)
        {
            cd = dd = 0;
            for (int32_t y = 0; y < kH; ++y)
                for (int32_t x = 0; x < kW; ++x)
                {
                    if (a.pixels[size_t(y) * Target::kPitchPx + size_t(x)] != b.pixels[size_t(y) * Target::kPitchPx + size_t(x)]) ++cd;
                    if (std::memcmp(&a.db.Row(y)[x], &b.db.Row(y)[x], sizeof(float)) != 0) ++dd;
                }
            return cd == 0 && dd == 0;
        }
    }

    // C3 : (1) chaque couloir de ClassifyFacesT8<true> == code du noyau (Pixel1Reference), au bit pres ;
    //      (2) image et Z-buffer : DrawBatch8 (chemin C3) == DrawTriangle face par face, 5 modes, 2 vues.
    bool Test_C3_Pixel1()
    {
        std::vector<RasterVertex> rv;
        std::vector<uint32_t> idx;
        MakeMesh(23u, 8 * 4000, rv, idx);
        const size_t nFaces = idx.size() / 3;

        bool ok = true;
        uint64_t lanes = 0, pix1 = 0, covered = 0, mism = 0;

        // ── (1) couloir par couloir ───────────────────────────────────
        for (const Viewport& vp : kViews)
            for (size_t base = 0; base < nFaces; base += 8)
            {
                const uint32_t* t8 = idx.data() + base * 3;
                Pixel1Lanes8 p1;
                const FaceMasks8 m = ClassifyFacesT8<true>(rv.data(), t8, vp, &p1);
                for (uint32_t k = 0; k < 8; ++k)
                {
                    if (!((m.survive >> k) & 1u)) continue;          // C3 ne concerne que les survivantes
                    ++lanes;
                    const uint32_t* t = t8 + k * 3;
                    const Pixel1Ref r = Pixel1Reference(rv[t[0]], rv[t[1]], rv[t[2]], vp);
                    bool same = r.pix1 == bool((p1.pix1 >> k) & 1u);
                    if (same && r.pix1)
                    {
                        ++pix1;
                        same = r.x == p1.x[k] && r.y == p1.y[k] && r.covered == bool((p1.covered >> k) & 1u);
                        if (same && r.covered)
                        {
                            ++covered;
                            same = std::memcmp(&r.b.w0, &p1.b0[k], 4) == 0 && std::memcmp(&r.b.w1, &p1.b1[k], 4) == 0
                                && std::memcmp(&r.b.w2, &p1.b2[k], 4) == 0;
                        }
                    }
                    if (!same) ++mism;
                }
            }
        if (mism != 0) { Logger::error("[C3] " + std::to_string(mism) + " couloirs differents du noyau"); ok = false; }
        Logger::info("[C3] couloirs survivants " + std::to_string(lanes) + ", chemin C3 " + std::to_string(pix1)
            + ", centre couvert " + std::to_string(covered));
        if (pix1 == 0 || covered == 0 || pix1 == lanes) { Logger::error("[C3] test trivial"); ok = false; }

        // ── (2) image complete, chemin C3 contre DrawTriangle ─────────
        const ERenderMode modes[5] = { ERenderMode::Solid, ERenderMode::Depth, ERenderMode::BarycentricColors,
                                       ERenderMode::LinearDepth, ERenderMode::ShadeFragment_LinearDepth_WRONG };
        for (const Viewport& vp : kViews)
            for (ERenderMode mode : modes)
            {
                Target A, B;
                Renderer ra, rb;
                ra.BeginFrame(A.fb, A.db); ra.SetViewport(vp); ra.SetMode(mode); ra.SetDepthDisplayRange(25.0f);
                rb.BeginFrame(B.fb, B.db); rb.SetViewport(vp); rb.SetMode(mode); rb.SetDepthDisplayRange(25.0f);
                for (size_t base = 0; base < nFaces; base += 8)
                {
                    const uint32_t* t8 = idx.data() + base * 3;
                    Pixel1Lanes8 p1;
                    const FaceMasks8 m = ClassifyFacesT8<true>(rv.data(), t8, vp, &p1);
                    // A : la reference, face par face par le noyau (comme avant C3)
                    for (uint32_t s = m.survive; s; s &= s - 1u)
                    {
                        const uint32_t k = uint32_t(std::countr_zero(s));
                        const uint32_t* t = t8 + k * 3;
                        const RasterVertex& a = rv[t[0]], & b = rv[t[1]], & c = rv[t[2]];
                        ra.DrawTriangle(RasterTriangle{ { a.x, a.y }, { b.x, b.y }, { c.x, c.y },
                            a.z, b.z, c.z, a.invW, b.invW, c.invW }, FaceColor(int(base + k)));
                    }
                    // B : le chemin C3
                    rb.DrawBatch8(rv.data(), t8, m.survive, p1, false, Color{}, int32_t(base));
                }
                ra.EndFrame(); rb.EndFrame();
                size_t cd = 0, dd = 0;
                if (!SameImage(A, B, cd, dd))
                {
                    Logger::error("[C3] mode " + std::to_string(int(mode)) + " vue " + std::to_string(vp.width)
                        + " : " + std::to_string(cd) + " couleurs, " + std::to_string(dd) + " profondeurs differentes");
                    ok = false;
                }
            }
        return ok;
    }
#endif
}