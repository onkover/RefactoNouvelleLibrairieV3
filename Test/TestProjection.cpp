// ============================================================
//  Validation Projection::Perspective  —  auto-vérifiante
//  Retourne le nombre d'échecs (0 = OK).
// ============================================================
#include <cassert>
#include <cstdio>
#include <cmath>
#include "Maths/Projection.h"

static int TestProjectionPerspective()
{
    using namespace LV3;

    int failures = 0, checks = 0;

    // Tolérance RELATIVE : indispensable, on compare des valeurs
    // allant de 1e-4 à 2.4.
    auto approx = [](float a, float b, float eps = 1e-4f) noexcept {
        return std::fabs(a - b) <= eps * (1.0f + std::fabs(a) + std::fabs(b));
        };

    // assert() disparaît en Release (NDEBUG) : on double d'un compteur
    // pour que le test reste utile dans les deux configurations.
#define CHECK(cond, fmt, ...)                                              \
        do {                                                                   \
            ++checks;                                                          \
            if (!(cond)) {                                                     \
                ++failures;                                                    \
                std::printf("[FAIL] L%d  " fmt "\n", __LINE__, __VA_ARGS__);   \
            }                                                                  \
            assert(cond);                                                      \
        } while (0)

    // --- Paramètres --------------------------------------------------
    const float fovY = 45.0f * TO_RADIAN;
    const float aspect = 16.0f / 9.0f;        // /!\ les .0f sont OBLIGATOIRES
    const float zNear = 0.1f;
    const float zFar = 1000.0f;

    // Piège n°1 : 16/9 en entier donne 1. On le verrouille explicitement.
    CHECK(approx(aspect, 1.7777778f),
        "aspect = %.6f au lieu de 1.777778 -> division ENTIERE 16/9 ?", aspect);

    const Matrix44f P = Projection::Perspective(fovY, aspect, zNear, zFar);

    // --- Multiplication vecteur-ligne EXPLICITE, sans division par w --
    struct Clip { float x, y, z, w; };
    auto mulRow = [&P](float px, float py, float pz) noexcept -> Clip {
        return { px * P[0][0] + py * P[1][0] + pz * P[2][0] + P[3][0],
                 px * P[0][1] + py * P[1][1] + pz * P[2][1] + P[3][1],
                 px * P[0][2] + py * P[1][2] + pz * P[2][2] + P[3][2],
                 px * P[0][3] + py * P[1][3] + pz * P[2][3] + P[3][3] };
        };
    auto ndcZ = [&](float d) noexcept {           // d = distance devant l'oeil
        const Clip c = mulRow(0.0f, 0.0f, -d);    // main droite : z = -d
        return c.z / c.w;
        };

    // --- 1. Coefficients ---------------------------------------------
    CHECK(approx(P[0][0], 1.35799527f), "m00 = %.8f attendu 1.35799527", P[0][0]);
    CHECK(approx(P[1][1], 2.41421356f), "m11 = %.8f attendu 2.41421356", P[1][1]);
    CHECK(approx(P[2][2], 1.00010001e-4f),
        "m22 = %.10f attendu 1.0001e-4 -> reverse-Z absent ?", P[2][2]);
    CHECK(approx(P[3][2], 0.100010001f),
        "m32 = %.8f attendu 0.10001001 -> reverse-Z absent ?", P[3][2]);
    CHECK(approx(P[2][3], -1.0f),
        "m23 = %.2f attendu -1 (main droite : w = -z)", P[2][3]);
    CHECK(approx(P[3][3], 0.0f),
        "m33 = %.2f attendu 0 -> Zeroed() oublie dans Projection.cpp ?", P[3][3]);
    CHECK(approx(P[2][0], 0.0f) && approx(P[2][1], 0.0f),
        "frustum non symetrique : m20=%.6f m21=%.6f", P[2][0], P[2][1]);

    // --- 2. REVERSE-Z : near -> 1, far -> 0 --------------------------
    CHECK(approx(ndcZ(zNear), 1.0f),
        "near -> z_ndc = %.6f attendu 1.0 (inverse ? version standard [0,1] ?)", ndcZ(zNear));
    CHECK(approx(ndcZ(zFar), 0.0f),
        "far  -> z_ndc = %.6f attendu 0.0", ndcZ(zFar));
    CHECK(approx(ndcZ(1.0f), 0.0999100f),
        "1 m  -> z_ndc = %.6f attendu 0.099910", ndcZ(1.0f));

    // --- 3. w porte bien la distance ---------------------------------
    CHECK(approx(mulRow(0, 0, -zNear).w, zNear), "w au near = %.6f attendu %.6f",
        mulRow(0, 0, -zNear).w, zNear);
    CHECK(approx(mulRow(0, 0, -42.0f).w, 42.0f), "w a 42 m = %.6f attendu 42.0",
        mulRow(0, 0, -42.0f).w);

    // --- 4. Monotonie DECROISSANTE et bornes [0,1] -------------------
    {
        float prev = 2.0f;
        for (float d : { 0.1f, 0.25f, 0.5f, 1.0f, 10.0f, 100.0f, 1000.0f })
        {
            const float z = ndcZ(d);
            CHECK(z <= prev + 1e-5f,
                "z_ndc doit DECROITRE : a %.2f m -> %.6f, precedent %.6f", d, z, prev);
            CHECK(z >= -1e-5f && z <= 1.0f + 1e-5f,
                "z_ndc hors [0,1] a %.2f m : %.6f", d, z);
            prev = z;
        }
    }

    // --- 5. Bords du frustum -> NDC = +-1 ----------------------------
    //  A d = 1 m : demi-hauteur = tan(22.5) = 0.41421356
    //              demi-largeur = 0.41421356 * aspect = 0.73637967
    {
        const float halfH = 0.41421356f, halfW = 0.73637967f;

        const Clip tr = mulRow(halfW, halfH, -1.0f);
        CHECK(approx(tr.x / tr.w, 1.0f), "bord droit -> x_ndc = %.6f attendu +1", tr.x / tr.w);
        CHECK(approx(tr.y / tr.w, 1.0f), "bord haut  -> y_ndc = %.6f attendu +1", tr.y / tr.w);

        const Clip bl = mulRow(-halfW, -halfH, -1.0f);
        CHECK(approx(bl.x / bl.w, -1.0f), "bord gauche -> x_ndc = %.6f attendu -1", bl.x / bl.w);
        CHECK(approx(bl.y / bl.w, -1.0f), "bord bas    -> y_ndc = %.6f attendu -1", bl.y / bl.w);

        const Clip c = mulRow(0.0f, 0.0f, -5.0f);
        CHECK(approx(c.x / c.w, 0.0f) && approx(c.y / c.w, 0.0f),
            "le centre doit rester au centre : (%.6f, %.6f)", c.x / c.w, c.y / c.w);

        // Le FOV est VERTICAL : c'est X qui encaisse le ratio.
        CHECK(approx(halfW / halfH, aspect, 1e-3f),
            "FOV vertical : halfW/halfH = %.6f doit valoir aspect", halfW / halfH);
    }

    // --- 6. Un point DERRIERE l'oeil a w < 0 -------------------------
    //  C'est ce qui justifie de clipper AVANT la division par w.
    CHECK(mulRow(0.0f, 0.0f, 5.0f).w < 0.0f,
        "point derriere l'oeil : w = %.6f doit etre NEGATIF", mulRow(0, 0, 5.0f).w);

    // --- 7. Precision au loin : 999 m et 1000 m doivent differer -----
    CHECK(ndcZ(999.0f) != ndcZ(1000.0f),
        "reverse-Z : 999 m et 1000 m collapsent sur %.9f", ndcZ(1000.0f));
    CHECK(ndcZ(999.0f) > ndcZ(1000.0f),
        "999 m doit avoir un z_ndc SUPERIEUR a 1000 m (reverse-Z)");

#undef CHECK
	if (failures > 0)
		std::printf("\033[31m=== Projection::Perspective : %d verifications, %d echec(s) ===\n\033[0m",checks, failures);
	else
        std::printf("\033[32m=== Projection::Perspective : %d verifications, %d echec(s) ===\n\033[0m",checks, failures);
    return failures;
}

int TestProjection()
{
    if (TestProjectionPerspective() != 0)
    {
        std::printf("TNR ROUGE - arret.\n");
        return -1;                    // on ne demarre pas sur une projection fausse
    }
    return 0;
}