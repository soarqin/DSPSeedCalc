/*
 * Copyright (c) 2022 Soar Qin<soarchin@gmail.com>
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#pragma once

#include "dsp/galaxy.hh"

extern void loadFilters();
extern bool runFilters(const Galaxy*);
extern bool runOutput(const Galaxy*);

#if defined(_WIN32)
#define FILTERAPI __stdcall
#else
#define FILTERAPI
#endif

struct PluginAPI {
    void (*output)(const Star *star);
};

using PluginInitFunc = const char*(*FILTERAPI)(PluginAPI*, int*);
using SeedBeginFunc = void*(*FILTERAPI)(int);
using GalaxyFilterFunc = bool(*FILTERAPI)(const Galaxy*, void*);
using StarFilterFunc = bool(*FILTERAPI)(const Star*, void*);
using PlanetFilterFunc = bool(*FILTERAPI)(const Planet*, void*);
using SeedEndFunc = bool(*FILTERAPI)(void*);

using OutputFunc = void(*FILTERAPI)(const Galaxy*);
