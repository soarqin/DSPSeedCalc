/*
 * Copyright (c) 2022 Soar Qin<soarchin@gmail.com>
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#pragma once

#include "galaxy.hh"

#include <cstdint>
#include <string>
#include <vector>

extern void loadFilters();
struct FilterInvocation {
    std::vector<void *> contexts;
};
struct OwnedOutputRecord {
    std::uint32_t channel;
    std::string data;
};
extern bool runPrePlanetFilters(const dspugen::Galaxy*, FilterInvocation&);
extern bool runFilters(const dspugen::Galaxy*, const FilterInvocation&);
extern bool runPoseFilters(int, int, const std::vector<dspugen::VectorLF3>&);
extern bool runOutput(const dspugen::Galaxy*);
extern void formatOutput(const dspugen::Galaxy*, std::vector<OwnedOutputRecord>&);
extern void unloadFilters();
extern std::uint64_t filterRequirements();
extern std::uint64_t prePlanetRequirements();
extern std::uint64_t outputRequirements();
extern bool pluginsAllowBlockScheduling();
extern bool pluginsRequireSerializedFilters();
extern bool pluginsHaveOutputCallbacks();
extern void initPluginThread(std::uint32_t);
extern void finishPluginThread();
extern void mergePluginThreads();

#if defined(_WIN32)
#define FILTERAPI __stdcall
#else
#define FILTERAPI
#endif

struct PluginAPI {
    void (*GenerateAllPlanets)(const dspugen::Galaxy *galaxy);
    void (*GeneratePlanetGas)(const dspugen::Planet *planet);
};

enum PluginRequirement : std::uint64_t {
    NeedsPositions = 1ULL << 0U,
    NeedsStars = 1ULL << 1U,
    NeedsNames = 1ULL << 2U,
    NeedsPlanetTopology = 1ULL << 3U,
    NeedsPlanetTheme = 1ULL << 4U,
    NeedsGas = 1ULL << 5U,
    NeedsVeinMetadata = 1ULL << 6U,
};

enum PluginCapability : std::uint64_t {
    FilterIsPure = 1ULL << 0U,
    ThreadSafeCallbacks = 1ULL << 1U,
    OrderIndependent = 1ULL << 2U,
    SupportsBlockScheduling = 1ULL << 3U,
    SupportsThreadMerge = 1ULL << 4U,
    SupportsOwnedOutput = 1ULL << 5U,
    NeedsEverySeed = 1ULL << 6U,
};

struct PluginDescriptorV1 {
    std::uint32_t structSize = sizeof(PluginDescriptorV1);
    std::uint32_t abiVersion = 1;
    std::uint64_t prePlanetRequirements = 0;
    std::uint64_t filterRequirements = 0;
    std::uint64_t outputRequirements = 0;
    std::uint64_t capabilities = 0;
};

struct OutputSinkV1 {
    void *context;
    void (*append)(void *, std::uint32_t, const char *, std::size_t);
};

using PluginInitFunc = const char*(FILTERAPI*)(PluginAPI*, int*);
using PluginInit2Func = const char*(FILTERAPI*)(PluginAPI*, int*, bool);
using PluginUninitFunc = void(FILTERAPI*)();
using SeedBeginFunc = void*(FILTERAPI*)(int);
using GalaxyFilterFunc = bool(FILTERAPI*)(const dspugen::Galaxy*, void*);
using StarFilterFunc = bool(FILTERAPI*)(const dspugen::Star*, void*);
using PlanetFilterFunc = bool(FILTERAPI*)(const dspugen::Planet*, void*);
using SeedEndFunc = bool(FILTERAPI*)(void*);
using PrePlanetFilterFunc = bool(FILTERAPI*)(const dspugen::Galaxy*, void*);
using DescribePluginFunc = bool(FILTERAPI*)(std::uint32_t, PluginDescriptorV1*);
using ThreadInitFunc = void*(FILTERAPI*)(std::uint32_t);
using ThreadFinishFunc = void*(FILTERAPI*)(void*);
using MergeThreadResultFunc = void(FILTERAPI*)(void*);
using FormatOutputFunc = void(FILTERAPI*)(const dspugen::Galaxy*, OutputSinkV1*);

using OutputFunc = void(FILTERAPI*)(const dspugen::Galaxy*);
using PoseFunc = void(FILTERAPI*)(int, int, const std::vector<dspugen::VectorLF3>&);
