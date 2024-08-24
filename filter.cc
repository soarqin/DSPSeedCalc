/*
 * Copyright (c) 2022 Soar Qin<soarchin@gmail.com>
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#include "filter.hh"

#include "settings.hh"

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
static std::vector<PluginUninitFunc> uninitFuncs;
static bool hasStarFilter = false;
static bool hasPlanetFilter = false;

static void generateAllPlanets(const dspugen::Galaxy *galaxy) {
    if (!galaxy->stars[0]->planets.empty()) return;
    for (auto *star: galaxy->stars) {
        star->createStarPlanets();
    }
}

static void generatePlanetGas(const dspugen::Planet *planet) {
    const_cast<dspugen::Planet*>(planet)->generateGas();
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
            if (auto *lib = dlopen(filename.c_str(), RTLD_LAZY)) {
                int type = 0;
                const char *pname;
                if (const auto initfunc = reinterpret_cast<PluginInitFunc>(dlsym(lib, "init"))) {
                    pname = initfunc(&api, &type);
                } else {
                    if (const auto init2func = reinterpret_cast<PluginInit2Func>(dlsym(lib, "init2"))) {
                        pname = init2func(&api, &type, dspugen::settings.hasPlanets);
                    } else {
                        dlclose(lib);
                        continue;
                    }
                }
                if (auto uninitfunc = reinterpret_cast<PluginUninitFunc>(dlsym(lib, "uninit"))) {
                    uninitFuncs.emplace_back(uninitfunc);
                }
                switch (type) {
                    case 0: {
                        FilterSet fs{
                            reinterpret_cast<SeedBeginFunc>(dlsym(lib, "seedBegin")),
                            reinterpret_cast<GalaxyFilterFunc>(dlsym(lib, "galaxyFilter")),
                            reinterpret_cast<StarFilterFunc>(dlsym(lib, "starFilter")),
                            reinterpret_cast<PlanetFilterFunc>(dlsym(lib, "planetFilter")),
                            reinterpret_cast<SeedEndFunc>(dlsym(lib, "seedEnd"))
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
                        if (auto func = reinterpret_cast<OutputFunc>(dlsym(lib, "output"))) {
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
                        if (auto func = reinterpret_cast<PoseFunc>(dlsym(lib, "pose"))) {
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
        bool pass = true;
        for (auto &s: galaxy->stars) {
            pass = true;
            for (const auto &fs: filters) {
                if(fs.starFilter && !fs.starFilter(s, fs.userp)) {
                    pass = false;
                    break;
                }
            }
            if (!pass) { continue; }
            pass = false;
            for (const auto &p: s->planets) {
                for (const auto &fs: filters) {
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
            for (const auto &fs: filters) {
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
    for (const auto &fs: filters) {
        if (fs.seedEnd && !fs.seedEnd(fs.userp)) {
            return false;
        }
    }
    return true;
}

bool runPoseFilters(int seed, const std::vector<dspugen::VectorLF3> &poses) {
    if (poseFuncs.empty()) { return false; }
    for (const auto &func: poseFuncs) {
        func(seed, poses);
    }
    return true;
}

bool runOutput(const dspugen::Galaxy *g) {
    if (outputFuncs.empty()) { return false; }
    for (const auto &func: outputFuncs) {
        func(g);
    }
    return true;
}

void unloadFilters() {
    for (const auto &func: uninitFuncs) {
        func();
    }
    filters.clear();
    outputFuncs.clear();
    uninitFuncs.clear();
}
