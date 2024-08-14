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

static double minDist[65] = {};
static int minSeed[65] = {};
static double maxDist[65] = {};
static int maxSeed[65] = {};
std::mutex mtx;

extern "C" {

__declspec(dllexport) const char *FILTERAPI init(PluginAPI *, int *type) {
    *type = 2;
    for (auto &d: minDist) {
        d = 10000000.0;
    }
    for (auto &d: maxDist) {
        d = 0.0;
    }
    return "For Fun Seeds 2";
}

inline void updateDist(size_t i, double dmax, int seed) {
    std::unique_lock lock(mtx);
    if (dmax < minDist[i]) {
        minDist[i] = dmax;
        minSeed[i] = seed;
    }
    if (dmax > maxDist[i]) {
        maxDist[i] = dmax;
        maxSeed[i] = seed;
    }
}

__declspec(dllexport) bool FILTERAPI pose(int seed, std::vector<dspugen::VectorLF3> &poses) {
    for (size_t z = 31; z < 64; z++) {
        double dmax = 0.0;
        for (size_t i = 0; i < z; i++) {
            const auto &pose1 = poses[i];
            auto dist = pose1.sqrMagnitude();
            if (dist > dmax) {
                dmax = dist;
            }
            /*
            for (size_t j = i + 1; j <= z; j++) {
                const auto &pose2 = poses[j];
                auto dist = (pose1 - pose2).sqrMagnitude();
                if (dist > dmax) {
                    dmax = dist;
                }
            }
            */
        }
        updateDist(z + 1, dmax, seed);
    }
    return false;
}

__declspec(dllexport) void FILTERAPI uninit() {
    std::unique_lock lock(mtx);
    for (int i = 32; i <= 64; i++)
        fprintf(stdout, "%d,%d,%g,%d,%g\n", i, minSeed[i], std::sqrt(minDist[i]), maxSeed[i], std::sqrt(maxDist[i]));
}

}
