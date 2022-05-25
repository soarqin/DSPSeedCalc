/*
 * Copyright (c) 2022 Soar Qin<soarchin@gmail.com>
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#pragma once

#include "star.hh"
#include "galaxy.hh"
#include <string>
#include <cstdint>

class NameGen {
public:
    static std::string randomName(int seed);

    static std::string randomStarName(int seed, Star *starData, Galaxy *galaxy);

private:
    static std::string _randomStarName(int seed, Star *starData);
    static std::string randomStarNameFromRawNames(int seed);
    static std::string randomStarNameWithConstellationAlpha(int seed);
    static std::string randomStarNameWithConstellationNumber(int seed);
    static std::string randomGiantStarNameFromRawNames(int seed);
    static std::string randomGiantStarNameWithConstellationAlpha(int seed);
    static std::string randomGiantStarNameWithFormat(int seed);
    static std::string randomNeutronStarNameWithFormat(int seed);
    static std::string randomBlackHoleNameWithFormat(int seed);
};
