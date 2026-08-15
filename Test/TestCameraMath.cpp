// ============================================================
//  Tests/TestCameraMath.cpp — TNR caméra / projection / frustum
//  Convention testée : main DROITE, vecteur-ligne (v' = v·M),
//                      NDC x,y ∈ [-1,1] ; z ∈ [0,1] REVERSE-Z
//  Appeler RunAllCameraMathTests() au démarrage en _DEBUG.
// ============================================================
#include <cassert>
#include <cstdio>
#include <cmath>

#include "Maths/MatrixLib.h"
#include "Maths/QuaternionLib.h"
#include "Maths/Projection.h"
#include "Maths/Geometry/AABB3d.h"
#include "Maths/Geometry/Frustum.h"
#include "Rendering/Viewport.h"
#include "scene/registry.hpp"
#include "Core/Logger.h"


using namespace LV3;

// ------------------------------------------------------------
//  Infrastructure
// ------------------------------------------------------------
namespace {

    int g_failures = 0;
    int g_checks = 0;

    // assert() disparaît en Release (NDEBUG) : on double d'un compteur
    // pour que la TNR reste utile dans les deux configurations.
#define TCHECK(cond, msg)                                                      \
    do {                                                                       \
        ++g_checks;                                                            \
        if (!(cond)) {                                                         \
            ++g_failures;                                                      \
            std::printf("[FAIL] %s:%d  %s\n", __FILE__, __LINE__, (msg));      \
        }                                                                      \
        assert((cond) && msg);                                                 \
    } while (0)

    constexpr float EPS = 1e-4f;   // tolérance générale
    constexpr float EPS_LOOSE = 1e-3f;   // tolérance sur les grands nombres

    bool Approx(float a, float b, float eps = EPS) noexcept
    {
        return std::fabs(a - b) <= eps * (1.0f + std::fabs(a) + std::fabs(b));
    }
    bool Approx(const Vec3f& a, const Vec3f& b, float eps = EPS) noexcept
    {
        return Approx(a.x, b.x, eps) && Approx(a.y, b.y, eps) && Approx(a.z, b.z, eps);
    }

    // Multiplication EXPLICITE vecteur-ligne, SANS division par w.
    // Sert à valider la convention elle-même, et à inspecter w.
    Vec4f MulRow(const Matrix44f& m, const Vec3f& p) noexcept
    {
        return {
            p.x * m[0][0] + p.y * m[1][0] + p.z * m[2][0] + m[3][0],
            p.x * m[0][1] + p.y * m[1][1] + p.z * m[2][1] + m[3][1],
            p.x * m[0][2] + p.y * m[1][2] + p.z * m[2][2] + m[3][2],
            p.x * m[0][3] + p.y * m[1][3] + p.z * m[2][3] + m[3][3]
        };
    }

    // Clip -> NDC
    Vec3f ToNDC(const Vec4f& c) noexcept
    {
        const float inv = (std::fabs(c.w) > 1e-12f) ? 1.0f / c.w : 1.0f;
        return { c.x * inv, c.y * inv, c.z * inv };
    }

} // namespace anonyme


// ============================================================
//  1. Projection::Perspective — le test demandé
//     fovY = 45°, aspect = 16/9, near = 0.1, far = 1000
// ============================================================
static void Test_Projection_Perspective()
{
    const float fovY = 45.0f * TO_RADIAN;
    const float aspect = 16.0f / 9.0f;
    const float n = 0.1f;
    const float f = 1000.0f;

    const Matrix44f P = Projection::Perspective(fovY, aspect, n, f);

    // --- 1.1 Coefficients attendus ---------------------------
    // th   = tan(22.5°)      = 0.41421356
    // m00  = 1/(aspect*th)   = 1.35799527
    // m11  = 1/th            = 2.41421356
    // m22  = n/(f-n)         = 1.00010001e-4     (reverse-Z)
    // m32  = n*f/(f-n)       = 0.100010001       (reverse-Z)
    // m23  = -1                                   (recopie -z dans w)
    // m33  = 0                                    (matrice projective)
    TCHECK(Approx(P[0][0], 1.35799527f), "m00 : echelle X");
    TCHECK(Approx(P[1][1], 2.41421356f), "m11 : echelle Y");
    TCHECK(Approx(P[2][2], 1.00010001e-4f), "m22 : reverse-Z");
    TCHECK(Approx(P[3][2], 0.100010001f), "m32 : reverse-Z");
    TCHECK(Approx(P[2][3], -1.0f), "m23 : doit valoir -1 (main droite)");
    TCHECK(Approx(P[3][3], 0.0f), "m33 : doit valoir 0 (projective)");

    // Symétrie : aucun decentrement
    TCHECK(Approx(P[2][0], 0.0f), "m20 : frustum symetrique -> 0");
    TCHECK(Approx(P[2][1], 0.0f), "m21 : frustum symetrique -> 0");

    // --- 1.2 REVERSE-Z : near -> 1, far -> 0 -----------------
    const Vec4f cNear = MulRow(P, Vec3f(0.0f, 0.0f, -n));
    TCHECK(Approx(cNear.w, n), "w au near doit valoir la distance");
    TCHECK(Approx(ToNDC(cNear).z, 1.0f), "REVERSE-Z : near -> z_ndc = 1");

    const Vec4f cFar = MulRow(P, Vec3f(0.0f, 0.0f, -f));
    TCHECK(Approx(cFar.w, f), "w au far doit valoir la distance");
    TCHECK(Approx(ToNDC(cFar).z, 0.0f), "REVERSE-Z : far -> z_ndc = 0");

    // --- 1.3 Monotonie DECROISSANTE de la profondeur ---------
    float prev = 2.0f;
    for (float d : { 0.1f, 0.5f, 1.0f, 10.0f, 100.0f, 1000.0f })
    {
        const float z = ToNDC(MulRow(P, Vec3f(0.0f, 0.0f, -d))).z;
        TCHECK(z <= prev + EPS, "REVERSE-Z : z_ndc doit DECROITRE avec la distance");
        TCHECK(z >= -EPS && z <= 1.0f + EPS, "z_ndc doit rester dans [0,1]");
        prev = z;
    }
    // Valeur de reference a d = 1 m : 0.09991
    TCHECK(Approx(ToNDC(MulRow(P, Vec3f(0, 0, -1.0f))).z, 0.0999100f),
        "z_ndc a 1 m");

    // --- 1.4 Bords du frustum -> NDC = +-1 -------------------
    // A d = 1 : demi-hauteur = th*d = 0.41421356
    //           demi-largeur = th*d*aspect = 0.73637967
    const float halfH = 0.41421356f;
    const float halfW = 0.73637967f;
    const Vec3f ndcTR = ToNDC(MulRow(P, Vec3f(halfW, halfH, -1.0f)));
    TCHECK(Approx(ndcTR.x, 1.0f), "bord droit -> x_ndc = +1");
    TCHECK(Approx(ndcTR.y, 1.0f), "bord haut  -> y_ndc = +1");

    const Vec3f ndcBL = ToNDC(MulRow(P, Vec3f(-halfW, -halfH, -1.0f)));
    TCHECK(Approx(ndcBL.x, -1.0f), "bord gauche -> x_ndc = -1");
    TCHECK(Approx(ndcBL.y, -1.0f), "bord bas    -> y_ndc = -1");

    // Le centre reste au centre
    const Vec3f ndcC = ToNDC(MulRow(P, Vec3f(0.0f, 0.0f, -5.0f)));
    TCHECK(Approx(ndcC.x, 0.0f) && Approx(ndcC.y, 0.0f), "centre -> (0,0)");

    // --- 1.5 L'aspect ratio est bien HORIZONTAL --------------
    TCHECK(Approx(halfW / halfH, aspect, 1e-3f),
        "FOV vertical fixe : le ratio agit sur X");

    // --- 1.6 Un point DERRIERE l'oeil a w < 0 ----------------
    TCHECK(MulRow(P, Vec3f(0.0f, 0.0f, +5.0f)).w < 0.0f,
        "point derriere l'oeil -> w negatif (a clipper AVANT /w)");

    // --- 1.7 Precision : reverse-Z separe bien 999 m de 1000 m
    const float z999 = ToNDC(MulRow(P, Vec3f(0, 0, -999.0f))).z;
    const float z1000 = ToNDC(MulRow(P, Vec3f(0, 0, -1000.0f))).z;
    TCHECK(z999 != z1000, "reverse-Z : 999 m et 1000 m doivent differer");
}

// ============================================================
//  2. Projection::PerspectiveInfinite
// ============================================================
static void Test_Projection_PerspectiveInfinite()
{
    const float n = 0.1f;
    const Matrix44f P = Projection::PerspectiveInfinite(45.0f * TO_RADIAN, 16.0f / 9.0f, n);

    TCHECK(Approx(P[2][2], 0.0f), "far infini : m22 = 0");
    TCHECK(Approx(P[3][2], n), "far infini : m32 = near");
    TCHECK(Approx(P[2][3], -1.0f), "far infini : m23 = -1");

    // z_ndc = near / distance
    TCHECK(Approx(ToNDC(MulRow(P, Vec3f(0, 0, -n))).z, 1.0f), "near -> 1");
    TCHECK(Approx(ToNDC(MulRow(P, Vec3f(0, 0, -1000.f))).z, 1e-4f), "1000 m -> 1e-4");
    TCHECK(ToNDC(MulRow(P, Vec3f(0, 0, -1e9f))).z > 0.0f, "jamais negatif");
    TCHECK(ToNDC(MulRow(P, Vec3f(0, 0, -1e9f))).z < 1e-6f, "tend vers 0");

    // X/Y identiques a la version finie
    const Matrix44f Pf = Projection::Perspective(45.0f * TO_RADIAN, 16.0f / 9.0f, n, 1000.f);
    TCHECK(Approx(P[0][0], Pf[0][0]), "far infini : X inchange");
    TCHECK(Approx(P[1][1], Pf[1][1]), "far infini : Y inchange");
}

// ============================================================
//  3. Projection::Orthographic (reverse-Z)
// ============================================================
static void Test_Projection_Orthographic()
{
    const float n = 0.1f, f = 1000.0f;
    const Matrix44f O = Projection::Orthographic(-2.f, 2.f, -1.f, 1.f, n, f);

    TCHECK(Approx(O[2][3], 0.0f), "ortho : pas de division perspective (m23 = 0)");
    TCHECK(Approx(O[3][3], 1.0f), "ortho : m33 = 1");

    TCHECK(Approx(ToNDC(MulRow(O, Vec3f(0, 0, -n))).z, 1.0f), "ortho reverse-Z : near -> 1");
    TCHECK(Approx(ToNDC(MulRow(O, Vec3f(0, 0, -f))).z, 0.0f), "ortho reverse-Z : far  -> 0");

    TCHECK(Approx(ToNDC(MulRow(O, Vec3f(2.f, 1.f, -10.f))).x, 1.0f), "ortho : bord droit");
    TCHECK(Approx(ToNDC(MulRow(O, Vec3f(-2.f, -1.f, -10.f))).y, -1.0f), "ortho : bord bas");

    // Pas de perspective : la taille ecran ne depend pas de la distance
    TCHECK(Approx(ToNDC(MulRow(O, Vec3f(1.f, 0, -10.f))).x,
        ToNDC(MulRow(O, Vec3f(1.f, 0, -900.f))).x),
        "ortho : taille independante de la distance");
}

// ============================================================
//  4. Matrix44f::inverseRigid
// ============================================================
static void Test_Matrix_InverseRigid()
{
    const Quatf q = Quatf::LookRotation(Vec3f(1.f, -0.5f, -2.f));
    Matrix44f world = q.ToMatrix44();
    world[3][0] = 3.0f; world[3][1] = -7.0f; world[3][2] = 12.0f;

    const Matrix44f inv = world.inverseRigid();

    // 4.1 world * inv == identite
    const Matrix44f id = world * inv;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            TCHECK(Approx(id[i][j], (i == j) ? 1.0f : 0.0f, 1e-3f),
                "inverseRigid : world * inv != identite");

    // 4.2 aller-retour sur un point
    const Vec3f p(5.f, 2.f, -8.f);
    Vec3f a, b;
    world.multVecMatrix(p, a);
    inv.multVecMatrix(a, b);
    TCHECK(Approx(p, b, 1e-3f), "inverseRigid : aller-retour d'un point");

    // 4.3 identique a l'inverse generique (mais sans Gauss-Jordan)
    const Matrix44f gen = world.inverse();
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            TCHECK(Approx(inv[i][j], gen[i][j], 1e-3f),
                "inverseRigid doit egaler inverse() sur une isometrie");

    // 4.4 la View ramene bien la camera a l'origine
    Vec3f eyeInView;
    inv.multVecMatrix(Vec3f(world[3][0], world[3][1], world[3][2]), eyeInView);
    TCHECK(Approx(eyeInView, Vec3f(0, 0, 0), 1e-3f), "View : l'oeil doit tomber a l'origine");
}

// ============================================================
//  5. Quatf::LookRotation
// ============================================================
static void Test_Quat_LookRotation()
{
    // PREREQUIS : Vec3f::Forward() doit valoir {0,0,-1} en main droite.
    TCHECK(Approx(Vec3f::Forward(), Vec3f(0, 0, -1)),
        "main droite : Forward() doit valoir -Z");

    // 5.1 direction canonique -> identite
    {
        const Quatf q = Quatf::LookRotation(Vec3f(0, 0, -1));
        TCHECK(Approx(std::fabs(q.r), 1.0f), "LookRotation(-Z) -> identite (r = +-1)");
        TCHECK(Approx(q.v, Vec3f(0, 0, 0)), "LookRotation(-Z) -> partie vectorielle nulle");
    }

    // 5.2 regarder vers +X  ->  rotation de -90 deg autour de Y
    {
        const Quatf q = Quatf::LookRotation(Vec3f(1, 0, 0));
        TCHECK(Approx(std::fabs(q.r), 0.70710678f), "LookRotation(+X) : r");
        TCHECK(Approx(std::fabs(q.v.y), 0.70710678f), "LookRotation(+X) : axe = Y");
        TCHECK(Approx(q.rotate(Vec3f::Forward()), Vec3f(1, 0, 0)),
            "LookRotation(+X) : l'avant doit pointer vers +X");
        TCHECK(Approx(q.rotate(Vec3f::Up()), Vec3f(0, 1, 0)),
            "LookRotation(+X) : le haut doit rester +Y");
    }

    // 5.3 CAS DEGENERE : regarder pile vers le haut (colineaire a worldUp)
    {
        const Quatf q = Quatf::LookRotation(Vec3f(0, 1, 0));
        const Vec3f fwd = q.rotate(Vec3f::Forward());
        TCHECK(!std::isnan(fwd.x) && !std::isnan(fwd.y) && !std::isnan(fwd.z),
            "cas degenere : AUCUN NaN");
        TCHECK(Approx(fwd, Vec3f(0, 1, 0)), "regard vertical : l'avant doit pointer vers +Y");
        TCHECK(Approx(q.rotate(Vec3f::Right()).length(), 1.0f), "base restee orthonormee");
    }
    {
        const Quatf q = Quatf::LookRotation(Vec3f(0, -1, 0));
        const Vec3f fwd = q.rotate(Vec3f::Forward());
        TCHECK(Approx(fwd, Vec3f(0, -1, 0)), "regard vers le bas : avant = -Y");
    }

    // 5.4 direction nulle -> identite, pas de NaN
    {
        const Quatf q = Quatf::LookRotation(Vec3f(0, 0, 0));
        TCHECK(Approx(q.r, 1.0f) && Approx(q.v, Vec3f(0, 0, 0)),
            "direction nulle -> identite");
    }

    // 5.5 direction quelconque : l'avant doit toujours coincider
    for (Vec3f d : { Vec3f(1, 2, 3), Vec3f(-4, 0.5f, -2), Vec3f(0, 0, 7), Vec3f(-1, -1, -1) })
    {
        const Quatf q = Quatf::LookRotation(d);
        TCHECK(Approx(q.rotate(Vec3f::Forward()), d.Normalized(), 1e-3f),
            "LookRotation : rotate(Forward) doit egaler dir normalisee");
        TCHECK(Approx(q.r * q.r + q.v.norm(), 1.0f), "quaternion unitaire");
    }

    // 5.6 COHERENCE LookRotation <-> ToMatrix44  (le test critique)
    {
        const Vec3f eye(0.f, 5.f, 10.f), target(0.f, 0.f, 0.f);
        const Quatf q = Quatf::LookAt(eye, target);
        const Matrix44f m = q.ToMatrix44();

        Vec3f fwd; m.multDirMatrix(Vec3f::Forward(), fwd);
        TCHECK(Approx(fwd, (target - eye).Normalized(), 1e-3f),
            "ToMatrix44 et LookRotation doivent partager la meme convention");

        Vec3f right; m.multDirMatrix(Vec3f::Right(), right);
        TCHECK(Approx(right.dotProduct(fwd), 0.0f), "base orthogonale");
        TCHECK(Approx(right.length(), 1.0f), "base normee");
    }

    // 5.7 le haut reste vers le haut (pas de roulis parasite)
    {
        const Quatf q = Quatf::LookRotation(Vec3f(1, 0, -1));
        TCHECK(q.rotate(Vec3f::Up()).y > 0.0f, "aucun roulis : le haut reste en haut");
        TCHECK(Approx(q.rotate(Vec3f::Right()).y, 0.0f),
            "aucun roulis : le droit reste horizontal");
    }
}

// ============================================================
//  6. AABB3d : PVertex / NVertex
// ============================================================
static void Test_AABB_PNVertex()
{
    const AABB3d box(Vec3f(-1.f, -2.f, -3.f), Vec3f(4.f, 5.f, 6.f));

    TCHECK(Approx(box.PVertex(Vec3f(1, 1, 1)), Vec3f(4, 5, 6)), "PVertex(+++)");
    TCHECK(Approx(box.NVertex(Vec3f(1, 1, 1)), Vec3f(-1, -2, -3)), "NVertex(+++)");
    TCHECK(Approx(box.PVertex(Vec3f(-1, -1, -1)), Vec3f(-1, -2, -3)), "PVertex(---)");
    TCHECK(Approx(box.PVertex(Vec3f(1, -1, 1)), Vec3f(4, -2, 6)), "PVertex(+-+)");
    TCHECK(Approx(box.NVertex(Vec3f(1, -1, 1)), Vec3f(-1, 5, -3)), "NVertex(+-+)");

    // Invariant : PVertex est toujours plus avance que NVertex le long de n
    for (Vec3f n : { Vec3f(1, 0, 0), Vec3f(-1, 2, -3), Vec3f(0.3f, -0.7f, 0.1f) })
        TCHECK(box.PVertex(n).dotProduct(n) >= box.NVertex(n).dotProduct(n),
            "PVertex doit dominer NVertex le long de n");
}

// ============================================================
//  7. Frustum::Build + Contains
//     Camera a l'origine, regard vers -Z  =>  view = identite
// ============================================================
static void Test_Frustum_Build()
{
    const float n = 0.1f, f = 1000.0f;
    const Matrix44f view = Matrix44f::Identity();
    const Matrix44f proj = Projection::Perspective(45.0f * TO_RADIAN, 16.0f / 9.0f, n, f);
    const Matrix44f vp = view * proj;              // vecteur-ligne : v.V.P

    Frustum fr;
    fr.Build(vp, /*reverseZ*/ true, /*infiniteFar*/ false);

    TCHECK(fr.Count() == Frustum::PlaneCount, "far fini : 6 plans");

    // 7.1 Plans normalises
    for (int i = 0; i < fr.Count(); ++i)
        TCHECK(Approx(fr[i].normal.length(), 1.0f), "plan non normalise");

    // 7.2 Le plan NEAR doit etre a la distance 'near', normale vers -Z
    TCHECK(Approx(fr[Frustum::Near].normal, Vec3f(0, 0, -1), 1e-3f), "normale du plan NEAR");
    TCHECK(Approx(fr[Frustum::Near].d, -n, 1e-3f), "distance du plan NEAR");

    // 7.3 Le plan FAR : normale vers +Z, a la distance 'far'
    TCHECK(Approx(fr[Frustum::Far].normal, Vec3f(0, 0, 1), 1e-3f), "normale du plan FAR");
    TCHECK(Approx(fr[Frustum::Far].d, f, 1e-2f), "distance du plan FAR");

    // 7.4 Contains
    TCHECK(fr.Contains(Vec3f(0, 0, -10.f)), "point devant -> dedans");
    TCHECK(fr.Contains(Vec3f(0, 0, -0.2f)), "juste apres le near -> dedans");
    TCHECK(!fr.Contains(Vec3f(0, 0, 10.f)), "point DERRIERE la camera -> dehors");
    TCHECK(!fr.Contains(Vec3f(0, 0, -0.05f)), "plus proche que le near -> dehors");
    TCHECK(!fr.Contains(Vec3f(0, 0, -2000.f)), "au dela du far -> dehors");
    TCHECK(!fr.Contains(Vec3f(0, 0, 0.f)), "l'oeil lui-meme -> dehors (near)");

    // 7.5 Bords lateraux : a d = 1, demi-largeur = 0.73638
    TCHECK(fr.Contains(Vec3f(0.50f, 0, -1.f)), "0.50 < 0.736 -> dedans");
    TCHECK(!fr.Contains(Vec3f(1.00f, 0, -1.f)), "1.00 > 0.736 -> dehors (droite)");
    TCHECK(!fr.Contains(Vec3f(-1.00f, 0, -1.f)), "-1.00 -> dehors (gauche)");
    TCHECK(fr.Contains(Vec3f(0, 0.40f, -1.f)), "0.40 < 0.414 -> dedans");
    TCHECK(!fr.Contains(Vec3f(0, 0.50f, -1.f)), "0.50 > 0.414 -> dehors (haut)");

    // 7.6 Le frustum s'evase avec la distance
    TCHECK(fr.Contains(Vec3f(5.0f, 0, -10.f)), "a 10 m, 5 m de large -> dedans");

    // 7.7 FAR INFINI : 5 plans, plus aucune limite lointaine
    Frustum fi;
    fi.Build(view * Projection::PerspectiveInfinite(45.0f * TO_RADIAN, 16.0f / 9.0f, n),
        true, true);
    TCHECK(fi.Count() == Frustum::Far, "far infini : 5 plans seulement");
    TCHECK(fi.Contains(Vec3f(0, 0, -1e6f)), "far infini : rien n'est trop loin");
    TCHECK(!fi.Contains(Vec3f(0, 0, -0.05f)), "far infini : le near s'applique toujours");

    // 7.8 Camera translatee : les plans sont bien en espace MONDE
    Matrix44f world = Matrix44f::Identity();
    world[3][0] = 100.0f;
    Frustum ft;
    ft.Build(world.inverseRigid() * proj, true, false);
    TCHECK(ft.Contains(Vec3f(100.f, 0, -10.f)), "camera en x=100 : point devant elle");
    TCHECK(!ft.Contains(Vec3f(0.f, 0, -10.f)), "camera en x=100 : l'origine sort du champ");
}

// ============================================================
//  8. Frustum::Classify — les TROIS etats
// ============================================================
static void Test_Frustum_ClassifyAABB()
{
    const Matrix44f proj = Projection::Perspective(45.0f * TO_RADIAN, 16.0f / 9.0f, 0.1f, 1000.f);
    Frustum fr;
    fr.Build(Matrix44f::Identity() * proj, true, false);

    // 8.1 INSIDE : petite boite bien centree
    TCHECK(fr.Classify(AABB3d(Vec3f(-0.5f, -0.5f, -10.5f), Vec3f(0.5f, 0.5f, -9.5f)))
        == EIntersect::Inside, "petite boite devant -> Inside");

    // 8.2 OUTSIDE : derriere la camera
    TCHECK(fr.Classify(AABB3d(Vec3f(-1, -1, 5), Vec3f(1, 1, 7)))
        == EIntersect::Outside, "boite derriere -> Outside");

    // 8.3 OUTSIDE : au dela du far
    TCHECK(fr.Classify(AABB3d(Vec3f(-1, -1, -2000), Vec3f(1, 1, -1500)))
        == EIntersect::Outside, "boite au dela du far -> Outside");

    // 8.4 OUTSIDE : franchement sur le cote
    TCHECK(fr.Classify(AABB3d(Vec3f(500, -1, -10), Vec3f(502, 1, -8)))
        == EIntersect::Outside, "boite loin sur la droite -> Outside");

    // 8.5 INTERSECT : a cheval sur le plan near
    TCHECK(fr.Classify(AABB3d(Vec3f(-1, -1, -1), Vec3f(1, 1, 1)))
        == EIntersect::Intersect, "boite englobant l'oeil -> Intersect");

    // 8.6 INTERSECT : depasse sur la droite
    TCHECK(fr.Classify(AABB3d(Vec3f(0, -0.1f, -1.1f), Vec3f(3, 0.1f, -0.9f)))
        == EIntersect::Intersect, "boite debordant a droite -> Intersect");

    // 8.7 Une boite Inside le reste apres translation de la camera
    Matrix44f world = Matrix44f::Identity();
    world[3][2] = 20.0f;                              // camera reculee en +Z
    Frustum ft; ft.Build(world.inverseRigid() * proj, true, false);
    TCHECK(ft.Classify(AABB3d(Vec3f(-0.5f, -0.5f, -0.5f), Vec3f(0.5f, 0.5f, 0.5f)))
        == EIntersect::Inside, "camera reculee : cube a l'origine -> Inside");

    // 8.8 Coherence Classify / Contains
    const AABB3d tiny(Vec3f(-0.01f, -0.01f, -10.01f), Vec3f(0.01f, 0.01f, -9.99f));
    TCHECK((fr.Classify(tiny) != EIntersect::Outside) == fr.Contains(Vec3f(0, 0, -10.f)),
        "Classify et Contains doivent s'accorder");
}

// ============================================================
//  9. Viewport : NDC -> raster, et le FLIP Y
// ============================================================
static void Test_Viewport_ToRaster()
{
    const Viewport vp{ 0, 0, 1280, 720 };
    float x, y;

    TCHECK(Approx(vp.Aspect(), 1280.0f / 720.0f), "aspect du viewport");

    vp.ToRaster(-1.f, 1.f, x, y);
    TCHECK(Approx(x, 0.f) && Approx(y, 0.f), "NDC(-1,+1) -> coin HAUT-GAUCHE (0,0)");

    vp.ToRaster(1.f, -1.f, x, y);
    TCHECK(Approx(x, 1280.f) && Approx(y, 720.f), "NDC(+1,-1) -> coin BAS-DROITE");

    vp.ToRaster(0.f, 0.f, x, y);
    TCHECK(Approx(x, 640.f) && Approx(y, 360.f), "NDC(0,0) -> centre ecran");

    // Le flip Y : y_ndc croissant => y_raster DECROISSANT
    float y1, y2, xd;
    vp.ToRaster(0.f, -0.5f, xd, y1);
    vp.ToRaster(0.f, 0.5f, xd, y2);
    TCHECK(y2 < y1, "FLIP Y : NDC vers le haut = raster vers le bas");

    // Viewport decale
    const Viewport sub{ 100, 50, 320, 240 };
    sub.ToRaster(-1.f, 1.f, x, y);
    TCHECK(Approx(x, 100.f) && Approx(y, 50.f), "viewport decale : origine respectee");
}

// ============================================================
//  Point d'entree
// ============================================================
int RunAllCameraMathTests()
{
    g_failures = 0; g_checks = 0;

    Test_Projection_Perspective();
    Test_Projection_PerspectiveInfinite();
    Test_Projection_Orthographic();
    Test_Matrix_InverseRigid();
    Test_Quat_LookRotation();
    Test_AABB_PNVertex();
    Test_Frustum_Build();
    Test_Frustum_ClassifyAABB();
    Test_Viewport_ToRaster();

    if (g_failures > 0)
        std::printf("\033[31m\n=== TNR Camera/Projection/Frustum : %d verifications, %d echec(s) ===\n\033[0m",
            g_checks, g_failures);

    if (g_failures == 0)
    {
        std::printf("\033[32m=== TOUTES LES VERIFICATIONS ONT REUSSI ===\n\033[0m");
        return 0;
    }
    else
    {
        std::printf("\033[31m=== %d VERIFICATION(S) ONT ECHOUE ===\n\033[0m", g_failures);
        return -1;                    // on ne demarre pas sur une projection fausse
    }
}

    void CheckControllerExclusivity(Registry& reg)
    {
        for (auto&& [e, fps] : reg.ViewGroup<FPSControllerComponent>())
        {
            const auto* follow = reg.TryGet<CameraFollowComponent>(e);
            if (follow && fps.m_isEnabled && follow->m_isEnabled)
            {
                Logger::error("\033[31mI[INVARIANT] " + reg.getComponent<NameComponent>(e).m_id
                    + " : FPS et Follow actifs simultanement — ils s'ecrasent mutuellement\033[0m");
                LV3_ASSERT(false);
            }
        }
    }
