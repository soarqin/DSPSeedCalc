/*
 * Copyright (c) 2024 Soar Qin<soarchin@gmail.com>
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#include "filter.hh"
#include <fmt/format.h>

extern "C" {

static PluginAPI *pluginAPI = nullptr;
static float highestValue[4] = {0.f, 0.f, 0.f, 0.f};
static int highestId[4] = {0, 0, 0, 0};

__declspec(dllexport) const char *FILTERAPI init(PluginAPI *api, int *type) {
    pluginAPI = api;
    *type = 0;
    return "Highest Gas Giant in Birth Star";
}

__declspec(dllexport) void FILTERAPI uninit() {
    fmt::println("Highest Gas Giant in Birth Star: {}({}), {}({}), {}({}), {}({})",
               highestId[0], highestValue[0], highestId[1], highestValue[1],
               highestId[2], highestValue[2], highestId[3], highestValue[3]);
    pluginAPI = nullptr;
}

__declspec(dllexport) bool FILTERAPI galaxyFilter(const dspugen::Galaxy *g) {
    const auto *star = g->stars[0];
    if (!star) { return false; }
    for (const auto *p: star->planets) {
        switch (p->theme) {
            case 2:
            case 3:
            case 21:
                pluginAPI->GeneratePlanetGas(p);
                if (p->gasSpeeds[0] > highestValue[0]) {
                    highestValue[0] = p->gasSpeeds[0];
                    highestId[0] = g->seed;
                }
                if (p->gasSpeeds[1] > highestValue[1]) {
                    highestValue[1] = p->gasSpeeds[1];
                    highestId[1] = g->seed;
                }
                break;
            case 4:
            case 5:
                pluginAPI->GeneratePlanetGas(p);
                if (p->gasSpeeds[0] > highestValue[2]) {
                    highestValue[2] = p->gasSpeeds[0];
                    highestId[2] = g->seed;
                }
                if (p->gasSpeeds[1] > highestValue[3]) {
                    highestValue[3] = p->gasSpeeds[1];
                    highestId[3] = g->seed;
                }
                break;
        }
    }
    return false;
}

}
