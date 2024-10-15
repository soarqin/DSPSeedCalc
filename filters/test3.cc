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

static PluginAPI *theAPI = nullptr;
__declspec(dllexport) const char *FILTERAPI init(PluginAPI *api, int *type) {
    *type = 0;
    theAPI = api;
    return "Check special seeds";
}

__declspec(dllexport) bool FILTERAPI galaxyFilter(const dspugen::Galaxy *g) {
    int count = {};
    double dist[2];
    for (int i = 62; i < 64; i++) {
        auto *star = g->stars[i];
        dist[i - 62] = star->position.magnitude();
        for (auto &p: star->planets) {
            count += p->veinSpot[14];
        }
    }
    std::lock_guard<std::mutex> lock(mtx);
    if (count < 3 && dist[0] + dist[1] < 10.0)
        fmt::print("{},{},{:.3},{:.3}\n", g->seed, g->starCount,dist[0], dist[1]);
    return false;
}

}
