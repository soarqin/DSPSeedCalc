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
static bool hasStarFilter = false;
static bool hasPlanetFilter = false;

extern void outputFunc(const Star *star);

static PluginAPI api = {
    &outputFunc,
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
                auto initfunc = (PluginInitFunc)dlsym(lib, "init");
                if (!initfunc) {
                    dlclose(lib);
                    continue;
                }
                int type = 0;
                const char *pname = initfunc(&api, &type);
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
                default:
                    dlclose(lib);
                    continue;
                }
            }
        }
    }
}

bool runFilters(const Galaxy *galaxy) {
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

bool runOutput(const Galaxy *g) {
    if (outputFuncs.empty()) { return false; }
    for (auto &func: outputFuncs) {
        func(g);
    }
    return true;
}
