#include "pch.h"

//#include <vector>
//#include <cassert>
//#include <cstdint>

#include "core/simulationclock.h"


namespace LV3::Tests
{
    void Test_SimulationClock()
    {
        LV3::SimulationClockSettings cfg;    // valeurs par defaut
        LV3::SimulationClock c;
        c.Configure(cfg);
        c.m_timeScale = 1.0f;

        // 1. Advance est lineaire et cumulatif
        c.Advance(0.5f); c.Advance(0.5f);
        LV3_ASSERT(std::abs(c.m_simTime - 1.0) < 1e-6);

        // 2. La pause fige la date, sans perdre l'echelle
        c.m_paused = true;
        const double before = c.m_simTime;
        LV3_ASSERT(c.Advance(1.0f) == 0.0f);      // le retour aussi doit etre nul
        LV3_ASSERT(c.m_simTime == before);
        LV3_ASSERT(c.m_timeScale == 1.0f);
        c.m_paused = false;

        // 3. Scale est multiplicatif ET borne DES DEUX COTES
        c.Scale(true, false);
        LV3_ASSERT(std::abs(c.m_timeScale - cfg.m_step) < 1e-6f);
        for (int i = 0; i < 100; ++i) c.Scale(true, true);
        LV3_ASSERT(c.m_timeScale <= c.m_cfg.m_max);
        for (int i = 0; i < 200; ++i) c.Scale(false, true);
        LV3_ASSERT(c.m_timeScale >= c.m_cfg.m_min);

        // 4. Le clamp bas n'est pas absorbant : on doit pouvoir remonter
        c.Scale(true, false);
        LV3_ASSERT(c.m_timeScale > c.m_cfg.m_min);

        // 5. Deux horloges sont independantes — la raison d'etre du refus du static
        LV3::SimulationClock a, b;
        a.Configure(cfg); b.Configure(cfg);
        a.Advance(1.0f);
        LV3_ASSERT(b.m_simTime == 0.0);
    }


}