/*
 * Copyright (c) 2022 Soar Qin<soarchin@gmail.com>
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#pragma once

#include "dspugen/galaxy.hh"

extern void loadFilters();
extern bool runFilters(const dspugen::Galaxy*);
extern bool runPoseFilters(int, const std::vector<dspugen::VectorLF3>&);
extern bool runOutput(const dspugen::Galaxy*);
extern void unloadFilters();

#if defined(_WIN32)
#define FILTERAPI __stdcall
#else
#define FILTERAPI
#endif

struct PluginAPI {
    void (*GenerateAllPlanets)(const dspugen::Galaxy *galaxy);
    void (*GeneratePlanetGas)(const dspugen::Planet *planet);
};

using PluginInitFunc = const char*(FILTERAPI*)(PluginAPI*, int*);
using PluginInit2Func = const char*(FILTERAPI*)(PluginAPI*, int*, bool);
using PluginUninitFunc = void(FILTERAPI*)();
using SeedBeginFunc = void*(FILTERAPI*)(int);
using GalaxyFilterFunc = bool(FILTERAPI*)(const dspugen::Galaxy*, void*);
using StarFilterFunc = bool(FILTERAPI*)(const dspugen::Star*, void*);
using PlanetFilterFunc = bool(FILTERAPI*)(const dspugen::Planet*, void*);
using SeedEndFunc = bool(FILTERAPI*)(void*);

using OutputFunc = void(FILTERAPI*)(const dspugen::Galaxy*);
using PoseFunc = void(FILTERAPI*)(int, const std::vector<dspugen::VectorLF3>&);
