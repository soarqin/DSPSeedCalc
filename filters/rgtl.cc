/*
 * Copyright (c) 2022 Soar Qin<soarchin@gmail.com>
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#include "filter.hh"

extern "C" {

__declspec(dllexport) const char *FILTERAPI name() {
    return "Red Giant With Volcano/Water and 2 Tidal-Locked Planets";
}

__declspec(dllexport) bool FILTERAPI starFilter(const Star *star) {
    if (star->type == EStarType::GiantStar && star->spectr <= ESpectrType::K) {
        int cnt = 0;
        bool foundV = false;
        bool foundW = false;
        for (auto *planet: star->planets) {
            if (planet->singularity & EPlanetSingularity::TidalLocked) {
                ++cnt;
            }
            switch (planet->theme) {
            case 1:
            case 8:
            case 14:
            case 15:
            case 16:
            case 18: foundV = true;
                break;
            case 13: foundW = true;
                break;
            default: break;
            }
        }
        return foundV && foundW && cnt > 1;
    }
    return false;
}

}
