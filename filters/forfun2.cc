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

static double minDist = 10000000.0;
static double maxDist = 0.0;
std::vector<std::tuple<int, double>> minDistList;
std::vector<std::tuple<int, double>> maxDistList;
std::mutex mtx;

extern "C" {

__declspec(dllexport) const char *FILTERAPI init(PluginAPI *, int *type) {
    *type = 0;
    return "For Fun Seeds 2";
}

__declspec(dllexport) bool FILTERAPI galaxyFilter(const dspugen::Galaxy *g) {
    double dmax = 0.0;
    size_t sz = g->stars.size();
    int magNearPlanets[3] = {0, 0, 0};
    int highHydrogenNearPlanets[3] = {0, 0, 0};
    for (size_t i = 0; i < sz; i++) {
        const auto *star = g->stars[i];
        for (size_t j = i + 1; j < sz; j++) {
            const auto *star2 = g->stars[j];
            auto dist = (star->position - star2->position).sqrMagnitude();
            if (dist > dmax) {
                dmax = dist;
            }
        }
    }
    {
        std::unique_lock lock(mtx);
        if (dmax < minDist) {
            minDist = dmax;
            auto bear = minDist + 1.0;
            for (auto ite = minDistList.begin(); ite != minDistList.end();) {
                if (std::get<1>(*ite) > bear) {
                    ite = minDistList.erase(ite);
                } else {
                    ++ite;
                }
            }
            minDistList.emplace_back(g->seed, minDist);
        } else if (dmax > maxDist) {
            maxDist = dmax;
            auto bear = maxDist - 1.0;
            for (auto ite = maxDistList.begin(); ite != maxDistList.end();) {
                if (std::get<1>(*ite) < bear) {
                    ite = maxDistList.erase(ite);
                } else {
                    ++ite;
                }
            }
            maxDistList.emplace_back(g->seed, maxDist);
        }
    }
    return false;
}

__declspec(dllexport) void FILTERAPI uninit() {
    std::unique_lock lock(mtx);
    std::sort(minDistList.begin(), minDistList.end(), [](const auto &a, const auto &b) {
        return std::get<1>(a) < std::get<1>(b);
    });
    std::sort(maxDistList.begin(), maxDistList.end(), [](const auto &a, const auto &b) {
        return std::get<1>(a) > std::get<1>(b);
    });
    for (auto [seed, dist]: minDistList) {
        fprintf(stdout, "minDist: %g, seed: %d\n", std::sqrt(dist), seed);
    }
    for (auto [seed, dist]: maxDistList) {
        fprintf(stdout, "maxDist: %g, seed: %d\n", std::sqrt(dist), seed);
    }
}

}
