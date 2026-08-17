#pragma once

namespace LV3
{
    class Registry;                    // si la signature en a besoin
}

namespace LV3::Tests
{
    [[nodiscard]] bool RunAllTests(Registry& registry);
}