/*
 * Copyright (c) 2023 Soar Qin<soarchin@gmail.com>
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#include "filter.hh"

extern "C" {

__declspec(dllexport) const char *FILTERAPI init(PluginAPI *, int *type) {
    *type = 0;
    return "2 Blue Giants with high luminosity";
}

__declspec(dllexport) bool FILTERAPI galaxyFilter(const dspugen::Galaxy *g) {
    int cnt = 0, cnt2 = 0;
    for (const auto *star: g->stars) {
        if (star->type == dspugen::EStarType::GiantStar && star->spectr == dspugen::ESpectrType::O) {
            ++cnt;
            if (star->luminosity >= 19.832529646959319302266016012115f /* 18.092348467648446913917646190829f 16.064927362833999424416458640845f*/) {
                ++cnt2;
            }
        }
    }
    if (cnt == 2 && cnt2 == 2) {
        fprintf(stdout, "%d,%d\n", g->seed, g->starCount);
        return true;
    }
    return false;
}

}
