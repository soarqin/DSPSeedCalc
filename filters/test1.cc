/*
 * Copyright (c) 2024 Soar Qin<soarchin@gmail.com>
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#include "filter.hh"

#include <fmt/std.h>
#include <mutex>
#include <atomic>

extern "C" {

static PluginAPI *theAPI = nullptr;

struct Data {
    std::atomic<int64_t> totalCount = 0;
    std::atomic<int64_t> starCount = 0;
    std::atomic<int64_t> orbit1PlanetCount = 0;
    std::atomic<int64_t> planet1CoatedCount = 0;
    std::atomic<int64_t> planet2CoatedCount = 0;
};
static Data data[14] = {};

static std::mutex mtx;

__declspec(dllexport) const char *FILTERAPI init(PluginAPI *api, int *type) {
    theAPI = api;
    *type = 0;
    return "Filter for planets that full coated by Dyson Sphere";
}

__declspec(dllexport) void FILTERAPI uninit() {
    static const char *name[] = {
        "M", "K", "G", "F", "A", "B", "O", "Red Giant", "Yellow Giant", "White Giant", "Blue Giant", "White Dwarf", "Black Hole", "Neutron Star"
    };
    fmt::print("sep=,\n");
    fmt::print("Type,Star Count,Planet Count,Orbit1 Count,Coated Count(1st planet),Coated Count(2nd planet)\n");
    for (int i = 0; i < 14; i++) {
        auto &d = data[i];
        fmt::print("{},{},{},{},{},{}\n", name[i], d.starCount.load(), d.totalCount.load(), d.orbit1PlanetCount.load(), d.planet1CoatedCount.load(), d.planet2CoatedCount.load());
    }
}

static inline int calcIndex(const dspugen::Star *star) {
    switch (star->type) {
        case dspugen::EStarType::MainSeqStar:
            return int(star->spectr);
        case dspugen::EStarType::GiantStar:
            switch (star->spectr) {
                case dspugen::ESpectrType::M:
                case dspugen::ESpectrType::K:
                    return 7;
                case dspugen::ESpectrType::G:
                case dspugen::ESpectrType::F:
                    return 8;
                case dspugen::ESpectrType::A:
                    return 9;
                default: break;
            }
            return 10;
        case dspugen::EStarType::WhiteDwarf:
            return 11;
        case dspugen::EStarType::BlackHole:
            return 12;
        case dspugen::EStarType::NeutronStar:
            return 13;
    }
    return -1;
}

static inline void calcData(const dspugen::Star *star) {
    auto dysonRad = std::round(float(star->dysonRadius * 40000.0) * 2.0f / 100.0f) * 100.0f;
    auto *firstPlanet = star->planets[0];
    auto firstRad = float(double(firstPlanet->orbitRadius) * 40000.0);
    auto &d = data[calcIndex(star)];
    if (dysonRad - firstRad >= 2199.95f) {
        d.planet1CoatedCount++;
        if (star->planets.size() > 1 && dysonRad > float(double(star->planets[1]->orbitRadius) * 40000.0)) {
            d.planet2CoatedCount++;
            fmt::print("Seed: {}, Star: {}\n", star->galaxy->seed, star->id);
        }
    }
    if (firstPlanet->orbitIndex == 1) {
        d.orbit1PlanetCount++;
    }
    d.starCount++;
    d.totalCount += int64_t(star->planets.size());
}

__declspec(dllexport) bool FILTERAPI galaxyFilter(const dspugen::Galaxy *g) {
    theAPI->GenerateAllPlanets(g);
    for (const auto *star: g->stars) {
        calcData(star);
    }
    return false;
}

}
