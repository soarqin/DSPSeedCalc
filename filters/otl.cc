/*
 * Copyright (c) 2022 Soar Qin<soarchin@gmail.com>
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#include "filter.hh"

extern "C" {

__declspec(dllexport) const char *FILTERAPI init(PluginAPI *api, int *type) {
    *type = 0;
    return "O Star With Tidal-Locked Planets";
}

__declspec(dllexport) bool FILTERAPI galaxyFilter(const dspugen::Galaxy *g) {
    int cnt = 0;
    for (const auto *star: g->stars) {
        if (star->spectr != dspugen::ESpectrType::O) { continue; }
        for (auto *planet: star->planets) {
            if (planet->singularity & dspugen::EPlanetSingularity::TidalLocked) {
                if (++cnt > 0) {
                    return true;
                }
                break;
            }
        }
    }
    return false;
}

}
