/*
 * Copyright (c) 2022 Soar Qin<soarchin@gmail.com>
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#pragma once

#include "star.hh"
#include <vector>
#include <memory>

namespace dspugen {

enum : int {
    DefaultAlgoVersion = 20200403,
};

class Galaxy {
public:
    static constexpr double AU = 40000.0;
    static constexpr double LY = 2400000.0;

    static Galaxy *create(int algoVersion, int galaxySeed, int starCount, bool genName = false, bool hasPlanets = true);

public:
    ~Galaxy();
    void release();
/*
    int birthPlanetId = 0;
*/
    int birthStarId = 0;
    int HabitableCount = 0;
    int seed = 0;
    int starCount = 0;

    std::vector<Star *> stars;

    [[nodiscard]] inline Star *starById(int starId) const {
        auto num = starId - 1;
        if (num < 0 || num >= stars.size()) return nullptr;
        return stars[num];
    }

    [[nodiscard]] inline Planet *planetById(int planetId) const {
        auto num = planetId / 100 - 1;
        auto num2 = planetId % 100 - 1;
        if (num < 0 || num >= stars.size()) return nullptr;
        auto star = stars[num];
        if (!star) return nullptr;
        if (num2 < 0 || num2 >= star->planets.size()) return nullptr;
        return star->planets[num2];
    }
};

}
