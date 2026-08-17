// ============================================================
//  Tests/TestMatrixLib.cpp — TNR de l'algèbre matricielle LV3
//  Convention : row-major x[row][col], vecteur-ligne v' = v·M,
//               translation en LIGNE 3, composition S·R·T,
//               main DROITE.
//
//  Si tu places cette fonction dans le même .cpp que
//  TestCameraMath, supprime le bloc "Infrastructure" ci-dessous
//  et réutilise le TCHECK existant.
// ============================================================
#include <cassert>
#include <cstdio>
#include <cmath>

#include "Maths/MatrixLib.h"
#include "Maths/QuaternionLib.h"
#include "Maths/Vectorlib.h"

namespace LV3::Tests
{
    int TestMatrixLib()
    {
        using M4 = Matrix44f;
        using V3 = Vec3f;
        using Q = Quatf;

        int failures = 0, checks = 0, warnings = 0;

        // ---------- Infrastructure ----------------------------------
        auto approx = [](float a, float b, float eps = 1e-4f) noexcept {
            return std::fabs(a - b) <= eps * (1.0f + std::fabs(a) + std::fabs(b));
            };
        auto approxV = [&](const V3& a, const V3& b, float eps = 1e-4f) noexcept {
            return approx(a.x, b.x, eps) && approx(a.y, b.y, eps) && approx(a.z, b.z, eps);
            };
        auto approxM = [&](const M4& a, const M4& b, float eps = 1e-4f) noexcept {
            for (int i = 0; i < 4; ++i)
                for (int j = 0; j < 4; ++j)
                    if (!approx(a[i][j], b[i][j], eps)) return false;
            return true;
            };

#define CHECK(cond, fmt, ...)                                              \
            do { ++checks;                                                         \
                 if (!(cond)) { ++failures;                                        \
                    std::printf("[FAIL] L%d  " fmt "\n", __LINE__, __VA_ARGS__); } \
                 assert(cond); } while (0)

        // Contrôle NON bloquant : signale sans faire échouer la TNR.
#define WARN(cond, fmt, ...)                                               \
            do { ++checks;                                                         \
                 if (!(cond)) { ++warnings;                                        \
                    std::printf("[WARN] L%d  " fmt "\n", __LINE__, __VA_ARGS__); } \
            } while (0)

        const float R90 = 90.0f * TO_RADIAN;

        // ============================================================
        //  1. CONVENTION — la partie la plus importante du fichier
        // ============================================================
        {
            // 1.1 Constructeur par défaut = identité
            const M4 I;
            for (int i = 0; i < 4; ++i)
                for (int j = 0; j < 4; ++j)
                    CHECK(approx(I[i][j], (i == j) ? 1.0f : 0.0f),
                        "M4 par defaut : [%d][%d] = %.4f", i, j, I[i][j]);
            CHECK(approxM(I, M4::Identity()), "M4() doit egaler Identity()");

            // 1.2 La translation vit en LIGNE 3
            const M4 T = M4::Translation(V3(7.0f, -3.0f, 2.0f));
            CHECK(approx(T[3][0], 7.0f) && approx(T[3][1], -3.0f) && approx(T[3][2], 2.0f),
                "translation attendue en LIGNE 3 : (%.2f, %.2f, %.2f)", T[3][0], T[3][1], T[3][2]);
            CHECK(approx(T[0][3], 0.0f) && approx(T[1][3], 0.0f) && approx(T[2][3], 0.0f),
                "colonne 3 doit rester nulle -> tu es en COLONNE-majeur ?");

            // 1.3 data() : 16 floats contigus, row-major
            const float* d = T.data();
            CHECK(approx(d[12], 7.0f),
                "data()[12] = %.2f attendu 7.0 (row-major : ligne 3 en fin de buffer)", d[12]);
            CHECK(approx(d[4], T[1][0]), "data()[4] doit etre x[1][0]");

            // 1.4 multVecMatrix applique la translation, multDirMatrix NON
            V3 pt, dir;
            T.multVecMatrix(V3(1.0f, 1.0f, 1.0f), pt);
            T.multDirMatrix(V3(1.0f, 1.0f, 1.0f), dir);
            CHECK(approxV(pt, V3(8.0f, -2.0f, 3.0f)),
                "multVecMatrix (POINT) doit translater : (%.2f,%.2f,%.2f)", pt.x, pt.y, pt.z);
            CHECK(approxV(dir, V3(1.0f, 1.0f, 1.0f)),
                "multDirMatrix (DIRECTION) ne doit PAS translater : (%.2f,%.2f,%.2f)",
                dir.x, dir.y, dir.z);

            // 1.5 Scale
            const M4 S = M4::Scale(V3(2.0f, 3.0f, 4.0f));
            V3 s; S.multVecMatrix(V3(1.0f, 1.0f, 1.0f), s);
            CHECK(approxV(s, V3(2.0f, 3.0f, 4.0f)), "Scale : (%.2f,%.2f,%.2f)", s.x, s.y, s.z);
        }

        // ============================================================
        //  2. PRODUIT MATRICIEL — l'ordre de composition
        // ============================================================
        {
            const M4 I = M4::Identity();
            const M4 S = M4::Scale(V3(2.0f, 2.0f, 2.0f));
            const M4 T = M4::Translation(V3(10.0f, 0.0f, 0.0f));

            // 2.1 Neutralité de l'identité
            CHECK(approxM(S * I, S), "M * I doit valoir M");
            CHECK(approxM(I * S, S), "I * M doit valoir M");

            // 2.2 VECTEUR-LIGNE : A*B signifie "A PUIS B"
            //     v · (S·T) = (v·S)·T -> on scale d'abord, on translate ensuite
            V3 a, b;
            (S * T).multVecMatrix(V3(1.0f, 0.0f, 0.0f), a);
            CHECK(approxV(a, V3(12.0f, 0.0f, 0.0f)),
                "S*T : scale PUIS translate -> attendu (12,0,0), obtenu (%.2f,%.2f,%.2f)",
                a.x, a.y, a.z);

            (T * S).multVecMatrix(V3(1.0f, 0.0f, 0.0f), b);
            CHECK(approxV(b, V3(22.0f, 0.0f, 0.0f)),
                "T*S : translate PUIS scale -> attendu (22,0,0), obtenu (%.2f,%.2f,%.2f)",
                b.x, b.y, b.z);
            CHECK(!approxV(a, b), "S*T et T*S doivent DIFFERER (produit non commutatif)");

            // 2.3 Associativité
            const M4 Rz = M4().rotateZ(R90);
            CHECK(approxM((S * Rz) * T, S * (Rz * T), 1e-3f), "produit non associatif");

            // 2.4 Transposition
            CHECK(approxM(S.transposed().transposed(), S), "(M^T)^T doit valoir M");
            CHECK(approxM((S * T).transposed(), T.transposed() * S.transposed(), 1e-3f),
                "(A*B)^T doit valoir B^T * A^T");
        }

        // ============================================================
        //  3. ROTATIONS — sens et main droite
        // ============================================================
        {
            // 3.1 rotateX : +Y -> +Z
            V3 r;
            M4().rotateX(R90).multDirMatrix(V3::Up(), r);
            CHECK(approxV(r, V3(0, 0, 1), 1e-3f),
                "rotateX(90) : +Y doit aller vers +Z, obtenu (%.3f,%.3f,%.3f)", r.x, r.y, r.z);

            // 3.2 rotateY : +Z -> +X
            M4().rotateY(R90).multDirMatrix(V3(0, 0, 1), r);
            CHECK(approxV(r, V3(1, 0, 0), 1e-3f),
                "rotateY(90) : +Z doit aller vers +X, obtenu (%.3f,%.3f,%.3f)", r.x, r.y, r.z);

            // 3.3 rotateY : +X -> -Z   (le corollaire, souvent oublié)
            M4().rotateY(R90).multDirMatrix(V3::Right(), r);
            CHECK(approxV(r, V3(0, 0, -1), 1e-3f),
                "rotateY(90) : +X doit aller vers -Z, obtenu (%.3f,%.3f,%.3f)", r.x, r.y, r.z);

            // 3.4 rotateZ : +X -> +Y
            M4().rotateZ(R90).multDirMatrix(V3::Right(), r);
            CHECK(approxV(r, V3(0, 1, 0), 1e-3f),
                "rotateZ(90) : +X doit aller vers +Y, obtenu (%.3f,%.3f,%.3f)", r.x, r.y, r.z);

            // 3.5 Une rotation et son opposée s'annulent
            CHECK(approxM(M4().rotateY(0.7f) * M4().rotateY(-0.7f), M4::Identity(), 1e-3f),
                "R(a) * R(-a) doit valoir l'identite");

            // 3.6 Orthonormalité : R * R^T = I
            //Le §6.3 est le plus inhabituel : il assert qu'une fonction échoue. C'est volontaire — il documente le contrat de façon exécutable.Le jour où quelqu'un « améliorera » inverseRigid pour gérer le scale, le test le signalera, et on discutera plutôt que de découvrir la régression six mois plus tard.
            const M4 Rq = M4().rotateX(0.3f).rotateY(-1.1f).rotateZ(2.4f);
            CHECK(approxM(Rq * Rq.transposed(), M4::Identity(), 1e-3f),
                "matrice de rotation non orthonormee : R * R^T != I");

            // 3.7 Une rotation conserve les longueurs et les angles
            // Le §7.4 peut échouer — l'ordre de composition Hamilton vs matrice est le piège classique. S'il sort rouge, inverse q2 * q1 en q1 * q2 dans le test et dis-le-moi : cela signifiera que ton operator* de quaternion compose dans l'ordre inverse de ton operator* de matrice, ce qui est une incohérence à corriger dans la lib, pas dans le test.
            const V3 v(1.0f, -2.0f, 3.0f), w(0.5f, 4.0f, -1.0f);
            V3 rv, rw;
            Rq.multDirMatrix(v, rv);
            Rq.multDirMatrix(w, rw);
            CHECK(approx(rv.length(), v.length(), 1e-3f),
                "rotation : longueur modifiee %.4f -> %.4f", v.length(), rv.length());
            CHECK(approx(rv.dotProduct(rw), v.dotProduct(w), 1e-2f),
                "rotation : produit scalaire modifie");
        }

        // ============================================================
        //  4. CHAÎNAGE S·R·T — l'ordre documenté dans MatrixLib.h
        // ============================================================
        {
            M4 m;
            m.scale(V3(2.0f, 2.0f, 2.0f)).rotateY(R90).translate(V3(10.0f, 0.0f, 0.0f));

            // (1,0,0) --scale2--> (2,0,0) --rotY90--> (0,0,-2) --T--> (10,0,-2)
            V3 out;
            m.multVecMatrix(V3(1.0f, 0.0f, 0.0f), out);
            CHECK(approxV(out, V3(10.0f, 0.0f, -2.0f), 1e-3f),
                "chaine S.R.T : attendu (10,0,-2), obtenu (%.3f,%.3f,%.3f)", out.x, out.y, out.z);

            // Équivalence avec le produit explicite
            const M4 explicite = M4::Scale(V3(2, 2, 2)) * M4().rotateY(R90) * M4::Translation(V3(10, 0, 0));
            CHECK(approxM(m, explicite, 1e-3f),
                "le chainage doit egaler S * R * T ecrit explicitement");
        }

        // ============================================================
        //  5. INVERSE générique
        // ============================================================
        {
            M4 m;
            m.scale(V3(2.0f, 3.0f, 0.5f)).rotateX(0.4f).rotateZ(-1.2f)
                .translate(V3(5.0f, -2.0f, 8.0f));

            CHECK(approxM(m * m.inverse(), M4::Identity(), 1e-3f), "M * M^-1 != I");
            CHECK(approxM(m.inverse() * m, M4::Identity(), 1e-3f), "M^-1 * M != I");
            CHECK(approxM(m.inverse().inverse(), m, 1e-2f), "(M^-1)^-1 doit valoir M");

            // Aller-retour sur un point
            const V3 p(3.0f, -1.0f, 7.0f);
            V3 t1, t2;
            m.multVecMatrix(p, t1);
            m.inverse().multVecMatrix(t1, t2);
            CHECK(approxV(p, t2, 1e-2f), "aller-retour par l'inverse : (%.3f,%.3f,%.3f)",
                t2.x, t2.y, t2.z);

            // Contrat documenté : une matrice singulière renvoie l'IDENTITÉ
            M4 sing;
            for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j) sing[i][j] = 0.0f;
            CHECK(approxM(sing.inverse(), M4::Identity()),
                "matrice singuliere : inverse() doit renvoyer l'identite");
        }

        // ============================================================
        //  6. INVERSE RIGIDE — et sa limite
        // ============================================================
        {
            // 6.1 Sur une isométrie : identique à inverse(), sans Gauss-Jordan
            M4 rigide = Q::LookRotation(V3(1.0f, -0.5f, -2.0f)).ToMatrix44();
            rigide[3][0] = 3.0f; rigide[3][1] = -7.0f; rigide[3][2] = 12.0f;

            CHECK(approxM(rigide * rigide.inverseRigid(), M4::Identity(), 1e-3f),
                "inverseRigid : M * M^-1 != I");
            CHECK(approxM(rigide.inverseRigid(), rigide.inverse(), 1e-3f),
                "inverseRigid doit egaler inverse() sur une isometrie");

            // 6.2 La View ramène l'oeil à l'origine
            V3 eye(rigide[3][0], rigide[3][1], rigide[3][2]), inView;
            rigide.inverseRigid().multVecMatrix(eye, inView);
            CHECK(approxV(inView, V3::Zero(), 1e-3f),
                "View : l'oeil doit tomber a l'origine, obtenu (%.4f,%.4f,%.4f)",
                inView.x, inView.y, inView.z);

            // 6.3 CONTRAT : avec un scale, inverseRigid est FAUX par conception.
            //     Ce test verrouille la limite : ne l'utilise QUE sur une isométrie.
            M4 avecScale;
            avecScale.scale(V3(2, 2, 2)).rotateY(0.5f).translate(V3(1, 2, 3));
            CHECK(!approxM(avecScale * avecScale.inverseRigid(), M4::Identity(), 1e-3f),
                "inverseRigid ne doit PAS fonctionner avec un scale (contrat)");
            CHECK(approxM(avecScale * avecScale.inverse(), M4::Identity(), 1e-3f),
                "avec scale, seul inverse() generique est valide");
        }

        // ============================================================
        //  7. COHÉRENCE Quaternion <-> Matrice  (le test critique)
        // ============================================================
        {
            // 7.1 Quat(axe Y, 90°) doit produire EXACTEMENT rotateY(90°)
            Q qy; qy.SetAxisAngle(V3::Up(), R90);
            CHECK(approxM(qy.ToMatrix44(), M4().rotateY(R90), 1e-3f),
                "Quat(Y,90) et rotateY(90) doivent donner la MEME matrice");

            Q qx; qx.SetAxisAngle(V3::Right(), R90);
            CHECK(approxM(qx.ToMatrix44(), M4().rotateX(R90), 1e-3f),
                "Quat(X,90) et rotateX(90) divergent");

            Q qz; qz.SetAxisAngle(V3(0, 0, 1), R90);
            CHECK(approxM(qz.ToMatrix44(), M4().rotateZ(R90), 1e-3f),
                "Quat(Z,90) et rotateZ(90) divergent");

            // 7.2 q.rotate(v) == multDirMatrix(v)  -- deux chemins, un résultat
            const Q q = Q::LookRotation(V3(1.0f, 2.0f, -3.0f));
            const M4 m = q.ToMatrix44();
            for (V3 v : { V3(1, 0, 0), V3(0, 1, 0), V3(0, 0, 1), V3(1, -2, 3) })
            {
                V3 viaMat; m.multDirMatrix(v, viaMat);
                const V3 viaQuat = q.rotate(v);
                CHECK(approxV(viaMat, viaQuat, 1e-3f),
                    "quat.rotate=(%.3f,%.3f,%.3f) vs matrice=(%.3f,%.3f,%.3f)",
                    viaQuat.x, viaQuat.y, viaQuat.z, viaMat.x, viaMat.y, viaMat.z);
            }

            // 7.3 La matrice d'un quaternion est orthonormée
            CHECK(approxM(m * m.transposed(), M4::Identity(), 1e-3f),
                "ToMatrix44 doit produire une matrice orthonormee");

            // 7.4 Composition : (q1*q2).ToMatrix == q1.ToMatrix * q2.ToMatrix ?
            //     L'ORDRE est la question. En vecteur-ligne, A*B = "A puis B".
            Q q1; q1.SetAxisAngle(V3::Up(), 0.6f);
            Q q2; q2.SetAxisAngle(V3::Right(), -0.9f);
            const V3 v(1.0f, 2.0f, 3.0f);

            const V3 parQuat = (q2 * q1).rotate(v);          // Hamilton : q2 puis q1... a verifier
            V3 parMat; (q1.ToMatrix44() * q2.ToMatrix44()).multDirMatrix(v, parMat);
            CHECK(approxV(parQuat, parMat, 1e-3f),
                "composition quat/matrice incoherente : quat=(%.3f,%.3f,%.3f) mat=(%.3f,%.3f,%.3f)",
                parQuat.x, parQuat.y, parQuat.z, parMat.x, parMat.y, parMat.z);
        }

        // ============================================================
        //  8. LookRotation — ton snippet, rendu auto-vérifiant
        // ============================================================
        {
            // 8.1 Direction canonique -> identité
            const Q q1 = Q::LookRotation(V3(0, 0, -1));
            CHECK(approx(std::fabs(q1.r), 1.0f), "LookRotation(-Z) : r = %.4f attendu +-1", q1.r);
            CHECK(approxV(q1.v, V3::Zero(), 1e-4f),
                "LookRotation(-Z) : v = (%.4f,%.4f,%.4f) attendu (0,0,0)", q1.v.x, q1.v.y, q1.v.z);

            // 8.2 Regarder vers +X
            const Q q2 = Q::LookRotation(V3(1, 0, 0));
            const V3 f2 = q2.rotate(V3::Forward());
            CHECK(approxV(f2, V3(1, 0, 0), 1e-4f),
                "LookRotation(+X) : avant = (%.4f,%.4f,%.4f) attendu (1,0,0)", f2.x, f2.y, f2.z);
            CHECK(approxV(q2.rotate(V3::Up()), V3(0, 1, 0), 1e-4f),
                "LookRotation(+X) : le haut doit rester +Y");

            // 8.3 Cas dégénéré : regarder pile vers le haut
            const Q q3 = Q::LookRotation(V3(0, 1, 0));
            const V3 f3 = q3.rotate(V3::Forward());
            CHECK(!std::isnan(f3.x) && !std::isnan(f3.y) && !std::isnan(f3.z),
                "cas degenere : NaN detecte (%.4f,%.4f,%.4f)", f3.x, f3.y, f3.z);
            CHECK(approxV(f3, V3(0, 1, 0), 1e-4f),
                "regard vertical : avant = (%.4f,%.4f,%.4f) attendu (0,1,0)", f3.x, f3.y, f3.z);

            // 8.4 Cohérence avec la matrice
            const V3 eye(0, 5, 10), target(0, 0, 0);
            const Q q4 = Q::LookAt(eye, target);
            V3 f4; q4.ToMatrix44().multDirMatrix(V3::Forward(), f4);
            const V3 attendu = (target - eye).Normalized();
            CHECK(approxV(f4, attendu, 1e-3f),
                "LookAt/ToMatrix44 : obtenu (%.4f,%.4f,%.4f) attendu (%.4f,%.4f,%.4f)",
                f4.x, f4.y, f4.z, attendu.x, attendu.y, attendu.z);

            // 8.5 Quaternion toujours unitaire
            for (V3 d : { V3(1, 2, 3), V3(-4, 0.5f, -2), V3(0, 0, 7), V3(-1, -1, -1) })
            {
                const Q q = Q::LookRotation(d);
                CHECK(approx(q.r * q.r + q.v.norm(), 1.0f, 1e-4f), "quaternion non unitaire");
                CHECK(approxV(q.rotate(V3::Forward()), d.Normalized(), 1e-3f),
                    "rotate(Forward) doit egaler dir normalisee");
            }
        }

        // ============================================================
        //  9. getEulerAngles — SECTION NON BLOQUANTE
        //     Le header l'annonce lui-meme comme "heuristique sensible
        //     a la convention". Rien dans le moteur n'en depend :
        //     les rotations sont des quaternions. Si ces WARN sortent,
        //     la bonne decision est de SUPPRIMER la fonction, pas de
        //     la reparer.
        // ============================================================
        {
            const V3 e = M4().rotateY(R90).getEulerAngles();
            WARN(approx(std::fabs(e.y), 90.0f, 1e-2f) || approx(std::fabs(e.y), 270.0f, 1e-2f),
                "getEulerAngles(rotY 90) -> (%.2f, %.2f, %.2f)", e.x, e.y, e.z);

            const V3 e0 = M4::Identity().getEulerAngles();
            WARN(approx(e0.x, 0.0f, 1e-2f) && approx(e0.y, 0.0f, 1e-2f) && approx(e0.z, 0.0f, 1e-2f),
                "getEulerAngles(identite) -> (%.2f, %.2f, %.2f) attendu (0,0,0)", e0.x, e0.y, e0.z);
        }

#undef CHECK
#undef WARN


        if (failures > 0 || warnings > 0)
            std::printf("\033[31m=== TNR MatrixLib : %d verifications, %d echec(s), %d avertissement(s) ===\n\033[0m",
                checks, failures, warnings);
        else
            std::printf("\033[32m=== TNR MatrixLib : %d verifications, %d echec(s), %d avertissement(s) ===\n\033[0m",
                checks, failures, warnings);

        return failures;
    }

}