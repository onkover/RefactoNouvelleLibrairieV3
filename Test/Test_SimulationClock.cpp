#include "pch.h"

//#include <vector>
//#include <cassert>
//#include <cstdint>

#include "core/simulationclock.h"


namespace LV3::Tests
{
    void Test_SimulationClock()
    {
        LV3::SimulationClock c;
        c.m_timeScale = 1.0f;

        // 1. Advance est lineaire et cumulatif
        c.Advance(0.5f); c.Advance(0.5f);
        LV3_ASSERT(std::abs(c.m_simDays - 1.0) < 1e-6);

        // 2. La pause fige la date simulee, sans perdre l'echelle
        c.m_paused = true;
        const double before = c.m_simDays;
        c.Advance(1.0f);
        LV3_ASSERT(c.m_simDays == before);
        LV3_ASSERT(c.m_timeScale == 1.0f);
        c.m_paused = false;

        // 3. Scale est multiplicatif ET borne des deux cotes
        c.Scale(true, false);
        LV3_ASSERT(std::abs(c.m_timeScale - 1.5f) < 1e-6f);
        for (int i = 0; i < 100; ++i) c.Scale(true, true);
        LV3_ASSERT(c.m_timeScale <= LV3::SimulationClock::kMax);
        for (int i = 0; i < 200; ++i) c.Scale(false, true);
        LV3_ASSERT(c.m_timeScale >= LV3::SimulationClock::kMin);
    }


}