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

std::mutex mtx;

extern "C" {

__declspec(dllexport) const char *FILTERAPI init(PluginAPI *, int *type) {
    *type = 0;
    return "Check special seeds";
}

__declspec(dllexport) bool FILTERAPI galaxyFilter(const dspugen::Galaxy *g) {
    int count[3] = {};
    for (const auto *s: g->stars) {
        switch (s->type) {
            case dspugen::EStarType::NeutronStar:
                ++count[0];
                break;
            case dspugen::EStarType::BlackHole:
                ++count[1];
                break;
            case dspugen::EStarType::WhiteDwarf:
                ++count[2];
                break;
            default:
                break;
        }
    }
    if (count[0] != 1 || count[1] != 1 || count[2] > (g->starCount < 38 ? 1 : 2)) {
        fmt::print("{:>8},{:<2}: N={} B={} W={}\n", g->seed, g->starCount, count[0], count[1], count[2]);
    }
    return false;
}

}
