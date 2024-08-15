/*
 * Copyright (c) 2024 Soar Qin<soarchin@gmail.com>
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#include "filter.hh"

#include <fmt/format.h>
#include <mutex>
#include <vector>

std::mutex mtx;

extern "C" {

__declspec(dllexport) const char *FILTERAPI init(PluginAPI *, int *type) {
    *type = 0;
    return "For Fun 7";
}

__declspec(dllexport) bool FILTERAPI galaxyFilter(const dspugen::Galaxy *g) {
    int bgCnt = 0;
    float lum[3] = {0.f, 0.f, 0.f};
    for (auto *s: g->stars) {
        switch (s->type) {
            case dspugen::EStarType::GiantStar:
                if (s->spectr == dspugen::ESpectrType::O) {
                    int pCnt = 0;
                    for (auto *p: s->planets) {
                        switch (p->theme) {
                            case 2:
                            case 3:
                            case 4:
                            case 5:
                            case 21:
                                break;
                            default:
                                pCnt++;
                                break;
                        }
                    }
                    if (pCnt >= 3) { lum[bgCnt++] = s->luminosity; }
                }
                break;
            default:
                break;
        }
    }
    if (bgCnt >= 2) {
        std::unique_lock lock(mtx);
        fmt::print(" {} {} ", g->seed, bgCnt);
        for (int i = 0; i < bgCnt; i++) {
            fmt::print(" {}", std::pow(lum[i], 0.33000001311302185f));
        }
        fmt::println("");
    }
    return false;
}

}
