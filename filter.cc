/*
 * Copyright (c) 2022 Soar Qin<soarchin@gmail.com>
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#include "filter.hh"
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <dlfcn.h>
#include <vector>
#include <filesystem>
#include <iostream>

struct FilterSet {
    SeedBeginFunc seedBegin;
    GalaxyFilterFunc galaxyFilter;
    StarFilterFunc starFilter;
    PlanetFilterFunc planetFilter;
    SeedEndFunc seedEnd;
    void *userp;
};

static std::vector<FilterSet> filters;
static std::vector<OutputFunc> outputFuncs;
static std::vector<PoseFunc> poseFuncs;
static std::vector<void(FILTERAPI*)()> uninitFuncs;
static bool hasStarFilter = false;
static bool hasPlanetFilter = false;

extern bool hasPlanets;

static void generateAllPlanets(const dspugen::Galaxy *galaxy) {
    if (!galaxy->stars[0]->planets.empty()) return;
    for (auto *star: galaxy->stars) {
        star->createStarPlanets();
    }
}

static void generatePlanetGas(const dspugen::Planet *planet) {
    ((dspugen::Planet *)planet)->generateGas();
}

static PluginAPI api = {
    &generateAllPlanets,
    &generatePlanetGas,
};

void loadFilters() {
    filters.clear();
    const std::filesystem::path sandbox{"filters"};
    for (const std::filesystem::directory_entry& dir_entry :
        std::filesystem::directory_iterator{sandbox})
    {
        if (dir_entry.is_regular_file()) {
            const auto &path = dir_entry.path();
            auto filename = path.string();
            auto *lib = dlopen(filename.c_str(), RTLD_LAZY);
            if (lib) {
                int type = 0;
                const char *pname = nullptr;
                auto initfunc = (PluginInitFunc)dlsym(lib, "init");
                if (initfunc) {
                    pname = initfunc(&api, &type);
                } else {
                    auto init2func = (PluginInit2Func)dlsym(lib, "init2");
                    if (init2func) {
                        pname = init2func(&api, &type, hasPlanets);
                    } else {
                        dlclose(lib);
                        continue;
                    }
                }
                auto uninitfunc = (void(FILTERAPI*)())dlsym(lib, "uninit");
                if (uninitfunc) {
                    uninitFuncs.emplace_back(uninitfunc);
                }
                switch (type) {
                    case 0: {
                        FilterSet fs{
                            (SeedBeginFunc)dlsym(lib, "seedBegin"),
                            (GalaxyFilterFunc)dlsym(lib, "galaxyFilter"),
                            (StarFilterFunc)dlsym(lib, "starFilter"),
                            (PlanetFilterFunc)dlsym(lib, "planetFilter"),
                            (SeedEndFunc)dlsym(lib, "seedEnd")
                        };
                        filters.emplace_back(fs);
                        if (fs.galaxyFilter || fs.starFilter || fs.planetFilter || fs.seedEnd) {
                            hasStarFilter = hasStarFilter || fs.starFilter != nullptr;
                            hasPlanetFilter = hasStarFilter || fs.planetFilter != nullptr;
                            if (pname) {
                                fmt::print(std::cout, "Loaded galaxy filter: \"{}\" from [{}]\n", pname, filename);
                            } else {
                                fmt::print(std::cout, "Loaded galaxy filter: [{}]\n", filename);
                            }
                        }
                        break;
                    }
                    case 1: {
                        auto func = (OutputFunc)dlsym(lib, "output");
                        if (func) {
                            outputFuncs.emplace_back(func);
                            if (pname) {
                                fmt::print(std::cout, "Loaded output filter: \"{}\" from [{}]\n", pname, filename);
                            } else {
                                fmt::print(std::cout, "Loaded output filter: [{}]\n", filename);
                            }
                        }
                        break;
                    }
                    case 2: {
                        auto func = (PoseFunc)dlsym(lib, "pose");
                        if (func) {
                            poseFuncs.emplace_back(func);
                            if (pname) {
                                fmt::print(std::cout, "Loaded pose filter: \"{}\" from [{}]\n", pname, filename);
                            } else {
                                fmt::print(std::cout, "Loaded pose filter: [{}]\n", filename);
                            }
                        }
                        break;
                    }
                    default:
                        dlclose(lib);
                        break;
                }
            }
        }
    }
}

bool runFilters(const dspugen::Galaxy *galaxy) {
    for (auto &fs: filters) {
        fs.userp = fs.seedBegin ? fs.seedBegin(galaxy->seed) : nullptr;
        if (fs.galaxyFilter && !fs.galaxyFilter(galaxy, fs.userp)) {
            return false;
        }
    }
    if (hasPlanetFilter) {
        bool pass;
        for (auto &s: galaxy->stars) {
            pass = true;
            for (auto &fs: filters) {
                if(fs.starFilter && !fs.starFilter(s, fs.userp)) {
                    pass = false;
                    break;
                }
            }
            if (!pass) { continue; }
            pass = false;
            for (auto &p: s->planets) {
                for (auto &fs: filters) {
                    if (!fs.planetFilter || fs.planetFilter(p, fs.userp)) {
                        pass = true;
                    }
                }
            }
            if (pass) {
                break;
            }
        }
        if (!pass) { return false; }
    } else if (hasStarFilter) {
        bool pass = true;
        for (auto &s: galaxy->stars) {
            for (auto &fs: filters) {
                if(fs.starFilter && !fs.starFilter(s, fs.userp)) {
                    pass = false;
                    break;
                }
            }
            if (pass) {
                break;
            }
        }
        if (!pass) { return false; }
    }
    for (auto &fs: filters) {
        if (fs.seedEnd && !fs.seedEnd(fs.userp)) {
            return false;
        }
    }
    return true;
}

extern bool runPoseFilters(int seed, const std::vector<dspugen::VectorLF3> &poses) {
    if (poseFuncs.empty()) { return false; }
    for (auto &func: poseFuncs) {
        func(seed, poses);
    }
    return true;
}

bool runOutput(const dspugen::Galaxy *g) {
    if (outputFuncs.empty()) { return false; }
    for (auto &func: outputFuncs) {
        func(g);
    }
    return true;
}

void unloadFilters() {
    for (auto &func: uninitFuncs) {
        func();
    }
    filters.clear();
    outputFuncs.clear();
    uninitFuncs.clear();
}
