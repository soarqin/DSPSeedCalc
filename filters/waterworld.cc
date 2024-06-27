/*
 * Copyright (c) 2023 Soar Qin<soarchin@gmail.com>
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#include "filter.hh"

extern "C" {

__declspec(dllexport) const char *FILTERAPI init(PluginAPI *api, int *type) {
    *type = 0;
    return "Water world";
}

__declspec(dllexport) bool FILTERAPI galaxyFilter(const dspugen::Galaxy *g) {
    int cnt = 0, cnt2 = 0, cnt3 = 0;
    for (const auto *star: g->stars) {
        switch (star->type) {
        case dspugen::EStarType::BlackHole:
        case dspugen::EStarType::NeutronStar:
            for (auto *planet: star->planets) {
                cnt3 += planet->veinSpot[14];
            }
            break;
        default:
            for (auto *planet: star->planets) {
                switch (planet->theme) {
                case 16:
                    ++cnt;
                    break;
                case 23:
                    ++cnt2;
                    break;
                default:
                    break;
                }
            }
            break;
        }
    }
    if (cnt > 5 && cnt2 > 9 && cnt3 > 19) {
        fprintf(stdout, "%d: %d %d %d\n", g->seed, cnt, cnt2, cnt3);
        return true;
    }
    return false;
}

}
