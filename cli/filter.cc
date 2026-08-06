/*
 * Copyright (c) 2022 Soar Qin<soarchin@gmail.com>
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#include "filter.hh"

#include "settings.hh"
#include "util/instrumentation.hh"

#include <fmt/ostream.h>
#include <dlfcn.h>
#include <vector>
#include <filesystem>
#include <iostream>
#include <algorithm>
#include <mutex>
#include <chrono>

template<typename F>
static auto runPluginCallback(F &&callback) {
#ifdef DSP_ENABLE_INSTRUMENTATION
    auto start = std::chrono::steady_clock::now();
    auto result = callback();
    auto elapsed = std::chrono::steady_clock::now() - start;
    auto &counters = dspugen::util::instrumentation::local();
    ++counters.pluginCallbacks;
    counters.pluginCallbackNanoseconds += static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count());
    return result;
#else
    return callback();
#endif
}

struct FilterSet {
    SeedBeginFunc seedBegin;
    GalaxyFilterFunc galaxyFilter;
    StarFilterFunc starFilter;
    PlanetFilterFunc planetFilter;
    SeedEndFunc seedEnd;
    PrePlanetFilterFunc prePlanetFilter;
    PluginDescriptorV1 descriptor;
    bool hasDescriptor;
    ThreadInitFunc threadInit;
    ThreadFinishFunc threadFinish;
    MergeThreadResultFunc mergeThreadResult;
};

struct OutputSet {
    OutputFunc output;
    FormatOutputFunc formatOutput;
    PluginDescriptorV1 descriptor;
    bool hasDescriptor;
};

static std::vector<FilterSet> filters;

// Compact dispatch lists built once at load time. hasFinalFilter() and the null-callback
// checks used to be re-evaluated inside the star and planet loops, i.e. up to
// stars * planets * filters times per seed, for a result that is fixed after loadFilters().
// `index` is the position in FilterInvocation::contexts.
template<typename F>
struct FilterEntry {
    F callback;
    std::size_t index;
};
static std::vector<FilterEntry<GalaxyFilterFunc>> galaxyFilterList;
static std::vector<FilterEntry<StarFilterFunc>> starFilterList;
static std::vector<FilterEntry<PlanetFilterFunc>> planetFilterList;
static std::vector<FilterEntry<SeedEndFunc>> seedEndList;

static std::vector<OutputSet> outputFuncs;
static std::vector<PoseFunc> poseFuncs;
static std::vector<PluginUninitFunc> uninitFuncs;
static std::vector<void *> libraryHandles;
static bool hasStarFilter = false;
static bool hasPlanetFilter = false;
static std::uint64_t requiredForFilters = 0;
static std::uint64_t requiredForPrePlanet = 0;
static std::uint64_t requiredForOutput = 0;
static bool allowBlockScheduling = true;
static bool serializeFilters = false;
static std::mutex legacyFilterMutex;
struct PluginThreadState {
    std::vector<void *> states;
};
static thread_local PluginThreadState pluginThreadState;
static std::mutex partialMutex;
static std::vector<std::vector<void *>> partialResults;

static void generateAllPlanets(const dspugen::Galaxy *galaxy) {
    const_cast<dspugen::Galaxy*>(galaxy)->ensureAllPlanets();
}

static void generatePlanetGas(const dspugen::Planet *planet) {
    const_cast<dspugen::Planet*>(planet)->generateGas();
}

static void generateAllVeinAmounts(const dspugen::Galaxy *galaxy) {
    const_cast<dspugen::Galaxy*>(galaxy)->ensureAllVeinAmounts();
}

static const dspugen::VeinAmountSummary *getPlanetVeinAmountSummary(
    const dspugen::Planet *planet) {
    return planet->veinAmountSummary();
}

static PluginAPI api = {
    &generateAllPlanets,
    &generatePlanetGas,
};

static PluginAPIV2 apiV2 = {
    sizeof(PluginAPIV2),
    2,
    &generateAllPlanets,
    &generatePlanetGas,
    &generateAllVeinAmounts,
    &getPlanetVeinAmountSummary,
};

static bool hasFinalFilter(const FilterSet &filter) {
    return !filter.hasDescriptor || !filter.prePlanetFilter ||
           filter.descriptor.filterRequirements != 0 ||
           filter.planetFilter || filter.seedEnd;
}

static void buildDispatchLists() {
    galaxyFilterList.clear();
    starFilterList.clear();
    planetFilterList.clear();
    seedEndList.clear();
    for (std::size_t i = 0; i < filters.size(); ++i) {
        const auto &fs = filters[i];
        if (fs.seedEnd) seedEndList.push_back({fs.seedEnd, i});
        if (!hasFinalFilter(fs)) continue;
        if (fs.galaxyFilter) galaxyFilterList.push_back({fs.galaxyFilter, i});
        if (fs.starFilter) starFilterList.push_back({fs.starFilter, i});
        if (fs.planetFilter) planetFilterList.push_back({fs.planetFilter, i});
    }
}

void loadFilters() {
    filters.clear();
    galaxyFilterList.clear();
    starFilterList.clear();
    planetFilterList.clear();
    seedEndList.clear();
    outputFuncs.clear();
    poseFuncs.clear();
    uninitFuncs.clear();
    libraryHandles.clear();
    hasStarFilter = false;
    hasPlanetFilter = false;
    requiredForFilters = 0;
    requiredForPrePlanet = 0;
    requiredForOutput = 0;
    allowBlockScheduling = true;
    serializeFilters = false;
    const std::filesystem::path sandbox{"filters"};
    if (!std::filesystem::exists(sandbox)) return;
    std::vector<std::filesystem::directory_entry> entries;
    for (const auto &entry: std::filesystem::directory_iterator{sandbox})
        if (entry.is_regular_file()) entries.emplace_back(entry);
    std::sort(entries.begin(), entries.end(), [](const auto &lhs, const auto &rhs) {
        return lhs.path().native() < rhs.path().native();
    });
    for (const auto &dir_entry: entries) {
        if (dir_entry.is_regular_file()) {
            const auto &path = dir_entry.path();
            auto filename = path.string();
            if (auto *lib = dlopen(filename.c_str(), RTLD_LAZY)) {
                int type = 0;
                const char *pname = nullptr;
                const bool isV2 = dlsym(lib, "init3") != nullptr;
                if (const auto init3func = reinterpret_cast<PluginInit3Func>(
                        isV2 ? dlsym(lib, "init3") : nullptr)) {
                    pname = init3func(&apiV2, &type);
                } else if (const auto initfunc = reinterpret_cast<PluginInitFunc>(dlsym(lib, "init"))) {
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
                PluginDescriptorV1 descriptor;
                bool hasDescriptor = false;
                if (isV2) {
                    PluginDescriptorV2 descriptorV2;
                    auto describe = reinterpret_cast<DescribePluginV2Func>(
                        dlsym(lib, "describePlugin"));
                    if (describe && describe(2, &descriptorV2) &&
                        descriptorV2.abiVersion == 2 &&
                        descriptorV2.structSize >= sizeof(PluginDescriptorV2)) {
                        descriptor.structSize = descriptorV2.structSize;
                        descriptor.abiVersion = descriptorV2.abiVersion;
                        descriptor.prePlanetRequirements = descriptorV2.prePlanetRequirements;
                        descriptor.filterRequirements = descriptorV2.filterRequirements;
                        descriptor.outputRequirements = descriptorV2.outputRequirements;
                        descriptor.capabilities = descriptorV2.capabilities;
                        hasDescriptor = true;
                    }
                } else {
                    auto describe = reinterpret_cast<DescribePluginFunc>(
                        dlsym(lib, "describePlugin"));
                    hasDescriptor = describe && describe(1, &descriptor) &&
                                    descriptor.abiVersion == 1 &&
                                    descriptor.structSize >= sizeof(PluginDescriptorV1);
                }
                if (isV2 && !hasDescriptor) {
                    if (!uninitFuncs.empty() &&
                        uninitFuncs.back() == reinterpret_cast<PluginUninitFunc>(
                            dlsym(lib, "uninit"))) {
                        uninitFuncs.pop_back();
                    }
                    dlclose(lib);
                    continue;
                }
                if (!hasDescriptor ||
                    !(descriptor.capabilities & SupportsBlockScheduling) ||
                    !(descriptor.capabilities & OrderIndependent)) {
                    allowBlockScheduling = false;
                }
                switch (type) {
                    case 0: {
                        FilterSet fs{
                            reinterpret_cast<SeedBeginFunc>(dlsym(lib, "seedBegin")),
                            reinterpret_cast<GalaxyFilterFunc>(dlsym(lib, "galaxyFilter")),
                            reinterpret_cast<StarFilterFunc>(dlsym(lib, "starFilter")),
                            reinterpret_cast<PlanetFilterFunc>(dlsym(lib, "planetFilter")),
                            reinterpret_cast<SeedEndFunc>(dlsym(lib, "seedEnd")),
                            reinterpret_cast<PrePlanetFilterFunc>(dlsym(lib, "prePlanetFilter")),
                            descriptor,
                            hasDescriptor,
                            reinterpret_cast<ThreadInitFunc>(dlsym(lib, "threadInit")),
                            reinterpret_cast<ThreadFinishFunc>(dlsym(lib, "threadFinish")),
                            reinterpret_cast<MergeThreadResultFunc>(dlsym(lib, "mergeThreadResult"))
                        };
                        filters.emplace_back(fs);
                        if (!hasDescriptor || !(descriptor.capabilities & ThreadSafeCallbacks))
                            serializeFilters = true;
                        if (hasDescriptor) {
                            requiredForPrePlanet |= descriptor.prePlanetRequirements;
                            requiredForFilters |= descriptor.filterRequirements;
                        }
                        if (fs.galaxyFilter || fs.starFilter || fs.planetFilter || fs.seedEnd) {
                            hasStarFilter = hasStarFilter || fs.starFilter != nullptr;
                            hasPlanetFilter = hasPlanetFilter || fs.planetFilter != nullptr;
                            if (pname) {
                                fmt::print(std::cerr, "Loaded galaxy filter: \"{}\" from [{}]\n", pname, filename);
                            } else {
                                fmt::print(std::cerr, "Loaded galaxy filter: [{}]\n", filename);
                            }
                        }
                        break;
                    }
                    case 1: {
                        auto func = reinterpret_cast<OutputFunc>(dlsym(lib, "output"));
                        auto formatOutput = reinterpret_cast<FormatOutputFunc>(dlsym(lib, "formatOutput"));
                        if (func || formatOutput) {
                            outputFuncs.push_back({func, formatOutput, descriptor, hasDescriptor});
                            if (!hasDescriptor || !(descriptor.capabilities & ThreadSafeCallbacks))
                                serializeFilters = true;
                            if (hasDescriptor) requiredForOutput |= descriptor.outputRequirements;
                            if (pname) {
                                fmt::print(std::cerr, "Loaded output filter: \"{}\" from [{}]\n", pname, filename);
                            } else {
                                fmt::print(std::cerr, "Loaded output filter: [{}]\n", filename);
                            }
                        }
                        break;
                    }
                    case 2: {
                        if (auto func = reinterpret_cast<PoseFunc>(dlsym(lib, "pose"))) {
                            poseFuncs.emplace_back(func);
                            if (pname) {
                                fmt::print(std::cerr, "Loaded pose filter: \"{}\" from [{}]\n", pname, filename);
                            } else {
                                fmt::print(std::cerr, "Loaded pose filter: [{}]\n", filename);
                            }
                        }
                        break;
                    }
                    default:
                        if (!uninitFuncs.empty() &&
                            uninitFuncs.back() == reinterpret_cast<PluginUninitFunc>(dlsym(lib, "uninit"))) {
                            uninitFuncs.pop_back();
                        }
                        dlclose(lib);
                        break;
                }
                if (type >= 0 && type <= 2) libraryHandles.emplace_back(lib);
            }
        }
    }
    buildDispatchLists();
}

bool runPrePlanetFilters(const dspugen::Galaxy *galaxy, FilterInvocation &invocation) {
    invocation.contexts.resize(filters.size());
    for (std::size_t i = 0; i < filters.size(); ++i) {
        auto &fs = filters[i];
        invocation.contexts[i] = fs.seedBegin ? fs.seedBegin(galaxy->seed) : nullptr;
        if (fs.prePlanetFilter &&
            !runPluginCallback([&] { return fs.prePlanetFilter(galaxy, invocation.contexts[i]); })) {
            return false;
        }
    }
    return true;
}

bool runFilters(const dspugen::Galaxy *galaxy, const FilterInvocation &invocation) {
    for (const auto &entry: galaxyFilterList)
        if (!runPluginCallback([&] {
            return entry.callback(galaxy, invocation.contexts[entry.index]);
        })) return false;
    if (hasPlanetFilter) {
        bool pass = true;
        for (auto &s: galaxy->stars) {
            pass = true;
            for (const auto &entry: starFilterList)
                if (!runPluginCallback([&] {
                    return entry.callback(s, invocation.contexts[entry.index]);
                })) {
                    pass = false;
                    break;
                }
            if (!pass) { continue; }
            pass = false;
            for (const auto &p: s->planets) {
                bool planetPass = true;
                for (const auto &entry: planetFilterList)
                    if (!runPluginCallback([&] {
                            return entry.callback(p, invocation.contexts[entry.index]);
                        })) {
                        planetPass = false;
                        break;
                    }
                if (planetPass) {
                    pass = true;
                    break;
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
            pass = true;
            for (const auto &entry: starFilterList)
                if (!runPluginCallback([&] {
                    return entry.callback(s, invocation.contexts[entry.index]);
                })) {
                    pass = false;
                    break;
                }
            if (pass) {
                break;
            }
        }
        if (!pass) { return false; }
    }
    for (const auto &entry: seedEndList)
        if (!runPluginCallback([&] { return entry.callback(invocation.contexts[entry.index]); }))
            return false;
    return true;
}

bool runPoseFilters(int seed, int starCount, const std::vector<dspugen::VectorLF3> &poses) {
    if (poseFuncs.empty()) { return false; }
    for (const auto &func: poseFuncs) {
        runPluginCallback([&] { func(seed, starCount, poses); return true; });
    }
    return true;
}

bool runOutput(const dspugen::Galaxy *g) {
    if (outputFuncs.empty()) { return false; }
    for (const auto &output: outputFuncs) {
        if (output.output)
            runPluginCallback([&] { output.output(g); return true; });
    }
    return true;
}

void unloadFilters() {
    for (const auto &func: uninitFuncs) {
        func();
    }
    filters.clear();
    galaxyFilterList.clear();
    starFilterList.clear();
    planetFilterList.clear();
    seedEndList.clear();
    outputFuncs.clear();
    poseFuncs.clear();
    uninitFuncs.clear();
    for (auto *handle: libraryHandles) dlclose(handle);
    libraryHandles.clear();
}

static void appendOwnedOutput(void *context, std::uint32_t channel,
                              const char *data, std::size_t size) {
    auto &records = *static_cast<std::vector<OwnedOutputRecord>*>(context);
    records.push_back({channel, std::string(data, size)});
}

void formatOutput(const dspugen::Galaxy *galaxy, std::vector<OwnedOutputRecord> &records) {
    OutputSinkV1 sink{&records, &appendOwnedOutput};
    for (const auto &output: outputFuncs)
        if (output.formatOutput)
            runPluginCallback([&] { output.formatOutput(galaxy, &sink); return true; });
}

std::uint64_t filterRequirements() {
    return requiredForFilters;
}

std::uint64_t prePlanetRequirements() {
    return requiredForPrePlanet;
}

std::uint64_t outputRequirements() {
    return requiredForOutput;
}

bool pluginsRequireVeinAmounts() {
    return ((requiredForPrePlanet | requiredForFilters | requiredForOutput) &
            NeedsVeinAmounts) != 0;
}

bool pluginsAllowBlockScheduling() {
    return allowBlockScheduling;
}

bool pluginsRequireSerializedFilters() {
    return serializeFilters;
}

// Lets the caller skip taking the output mutex when no plugin has an output() callback,
// which is the common case for filter-only scans.
bool pluginsHaveOutputCallbacks() {
    for (const auto &output: outputFuncs)
        if (output.output) return true;
    return false;
}

void initPluginThread(std::uint32_t workerIndex) {
    pluginThreadState.states.resize(filters.size());
    for (std::size_t i = 0; i < filters.size(); ++i) {
        pluginThreadState.states[i] = filters[i].threadInit
                                      ? filters[i].threadInit(workerIndex) : nullptr;
    }
}

void finishPluginThread() {
    std::vector<void *> results(filters.size());
    bool hasResult = false;
    for (std::size_t i = 0; i < filters.size(); ++i) {
        if (filters[i].threadFinish) {
            results[i] = filters[i].threadFinish(pluginThreadState.states[i]);
            hasResult = hasResult || results[i] != nullptr;
        }
    }
    pluginThreadState.states.clear();
    if (hasResult) {
        std::lock_guard lock(partialMutex);
        partialResults.emplace_back(std::move(results));
    }
}

void mergePluginThreads() {
    for (const auto &results: partialResults)
        for (std::size_t i = 0; i < filters.size(); ++i)
            if (results[i] && filters[i].mergeThreadResult)
                filters[i].mergeThreadResult(results[i]);
    partialResults.clear();
}
