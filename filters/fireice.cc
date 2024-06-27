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
    return "Has Fire-Ice on any planet in birth star, with at least 2 O-star luminosity >= 2.4 and at least one with tidy-locked planet(s)";
}

__declspec(dllexport) bool FILTERAPI galaxyFilter(const dspugen::Galaxy *g) {
    const auto *star = g->starById(g->birthStarId);
    if (!star) { return false; }
    bool foundFI = false, foundGas = false;
    for (const auto *p: star->planets) {
        if (p->orbitAround > 0 && p->theme == 7 && p->veinSpot[8] > 3) {
            foundFI = true;
            if (foundGas) { return true; }
        } else if (p->theme == 2 || p->theme == 3 || p->theme == 21) {
            foundGas = true;
            if (foundFI) { return true; }
        }
    }
    return false;
}

}
