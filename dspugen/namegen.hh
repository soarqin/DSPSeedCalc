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

namespace dspugen {

class NameGen {
public:
    static std::string randomName(int seed);

    static std::string randomStarName(int seed, int lcid, Star *starData, Galaxy *galaxy);

private:
    static std::string _randomStarName(int seed, int lcid, Star *starData);
    static std::string randomStarNameFromRawNames(int seed, int lcid);
    static std::string randomStarNameWithConstellationAlpha(int seed, int lcid);
    static std::string randomStarNameWithConstellationNumber(int seed, int lcid);
    static std::string randomGiantStarNameFromRawNames(int seed, int lcid);
    static std::string randomGiantStarNameWithConstellationAlpha(int seed, int lcid);
    static std::string randomGiantStarNameWithFormat(int seed);
    static std::string randomNeutronStarNameWithFormat(int seed);
    static std::string randomBlackHoleNameWithFormat(int seed);
};

}
