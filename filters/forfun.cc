/*
 * Copyright (c) 2024 Soar Qin<soarchin@gmail.com>
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#include "filter.hh"
#include <mutex>
#include <vector>
#include <tuple>
#include <algorithm>

static float minLum = 100000.f;
static float maxLum = 0.f;
std::vector<std::tuple<int, float>> minLumList;
std::vector<std::tuple<int, float>> maxLumList;
std::mutex mtx;

extern "C" {

__declspec(dllexport) const char *FILTERAPI init(PluginAPI *, int *type) {
    *type = 0;
    return "For Fun Seeds";
}

__declspec(dllexport) bool FILTERAPI galaxyFilter(const dspugen::Galaxy *g) {
    const auto *star = g->stars[0];
    {
        std::unique_lock lock(mtx);
        if (star->luminosity < minLum) {
            minLum = star->luminosity;
            auto bear = minLum + 0.0001f;
            for (auto ite = minLumList.begin(); ite != minLumList.end();) {
                if (std::get<1>(*ite) > bear) {
                    ite = minLumList.erase(ite);
                } else {
                    ++ite;
                }
            }
            minLumList.emplace_back(g->seed, minLum);
        } else if (star->luminosity > maxLum) {
            maxLum = star->luminosity;
            auto bear = maxLum - 0.0001f;
            for (auto ite = maxLumList.begin(); ite != maxLumList.end();) {
                if (std::get<1>(*ite) < bear) {
                    ite = maxLumList.erase(ite);
                } else {
                    ++ite;
                }
            }
            maxLumList.emplace_back(g->seed, maxLum);
        }
    }
    return false;
}

__declspec(dllexport) void FILTERAPI uninit() {
    std::unique_lock lock(mtx);
    std::sort(minLumList.begin(), minLumList.end(), [](const auto &a, const auto &b) {
        return std::get<1>(a) < std::get<1>(b);
    });
    std::sort(maxLumList.begin(), maxLumList.end(), [](const auto &a, const auto &b) {
        return std::get<1>(a) > std::get<1>(b);
    });
    for (auto [seed, lum]: minLumList) {
        fprintf(stdout, "minLum: %f, seed: %d\n", std::pow(lum, 0.33000001311302185f), seed);
    }
    for (auto [seed, lum]: maxLumList) {
        fprintf(stdout, "maxLum: %f, seed: %d\n", std::pow(lum, 0.33000001311302185f), seed);
    }
}

}
