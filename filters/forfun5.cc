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

extern "C" {

std::vector<int> bSeeds;
int bMaxCount = 26;

__declspec(dllexport) const char *FILTERAPI init(PluginAPI *, int *type) {
    *type = 0;
    return "Max B Seeds";
}

__declspec(dllexport) void FILTERAPI uninit() {
    fmt::println("Max B Count: {}", bMaxCount);
    for (auto seed: bSeeds) {
        fmt::print(" {}", seed);
    }
    fmt::println("");
}

std::mutex mtx;
__declspec(dllexport) bool FILTERAPI galaxyFilter(const dspugen::Galaxy *g) {
    int bCount = 0;
    for (auto *s: g->stars) {
        if (s->type != dspugen::EStarType::MainSeqStar) continue;
        switch (s->spectr) {
            case dspugen::ESpectrType::B:
                bCount++;
                break;
            default:
                break;
        }
    }
    std::unique_lock lock(mtx);
    if (bCount > bMaxCount) {
        bMaxCount = bCount;
        bSeeds = {g->seed};
    } else if (bCount == bMaxCount) {
        bSeeds.push_back(g->seed);
    }
    return false;
}

}
