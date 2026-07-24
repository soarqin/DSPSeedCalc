/*
 * Copyright (c) 2023 Soar Qin<soarchin@gmail.com>
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#include "filter.hh"

#include <fmt/format.h>

extern "C" {

__declspec(dllexport) const char *FILTERAPI init(PluginAPI*, int *type) {
    *type = 0;
    return "2 Blue Giants with high luminosity";
}

__declspec(dllexport) bool FILTERAPI galaxyFilter(const dspugen::Galaxy *g) {
    int cnt = 0, cnt2 = 0;
    float lum = 0;
    for (const auto *star: g->stars) {
        if (star->type == dspugen::EStarType::GiantStar && star->spectr == dspugen::ESpectrType::O) {
            if (++cnt == 1) {
                lum = star->luminosity;
            }
            if (star->luminosity >= 21.800680734017587301436790397276f) {
                ++cnt2;
            }
        }
    }
    if (cnt == 1 && cnt2 > 0) {
        fmt::print(stdout, "{},{},{:.4f}\n", g->seed, g->starCount, std::pow(lum, 0.33000001311302185f));
        return true;
    }
    return false;
}

}
