#pragma once
// ════════════════════════════════════════════════════════════
//  TestRandom.h — tirages PORTABLES pour les TNR (EXE, dossier Test)
//
//  La norme C++ fixe std::mt19937 au bit pres (meme suite sur tous les compilateurs),
//  mais PAS std::uniform_real_distribution : sa formule depend de la bibliotheque.
//    - MSVC STL : garde les 24 bits de poids fort du tirage (troncature) ;
//    - libstdc++ (g++) : convertit les 32 bits en float (arrondi au plus proche).
//  Environ la moitie des tirages different d'un ulp. Effet mesure sur les TNR :
//    C1 : 1 898 722 fragments (MSVC) contre 1 898 720 (g++) ; C3 : 3 364 centres contre 3 362.
//
//  TestRand01 reproduit EXACTEMENT le tirage MSVC (les journaux actuels ne changent pas)
//  et donne la meme suite partout : un ecart entre deux machines signale alors un vrai
//  probleme, plus jamais la bibliotheque standard.
// ════════════════════════════════════════════════════════════
#include <cstdint>
#include <random>

namespace LV3::Tests
{
    // Meme syntaxe d'appel qu'une distribution : U(rng). Valeurs dans [0, 1).
    struct TestRand01
    {
        float operator()(std::mt19937& g) const noexcept
        {
            // 24 bits de poids fort -> entier < 2^24, EXACT en float ; * 2^-24 est EXACT.
            // Maximum : (2^24 - 1) / 2^24 < 1. Jamais 1.0, aucun cas particulier.
            return float(g() >> 8) * 0x1p-24f;
        }
    };
}
