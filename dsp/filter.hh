/*
 * Copyright (c) 2022 Soar Qin<soarchin@gmail.com>
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#pragma once

#include "galaxy.hh"

extern void loadFilters();
extern bool runFilters(const Galaxy*);

#if defined(_WIN32)
#define FILTERAPI __stdcall
#else
#define FILTERAPI
#endif
using PluginNameFunc = const char*(*FILTERAPI)();
using SeedBeginFunc = void*(*FILTERAPI)(int);
using GalaxyFilterFunc = bool(*FILTERAPI)(const Galaxy*, void*);
using StarFilterFunc = bool(*FILTERAPI)(const Star*, void*);
using PlanetFilterFunc = bool(*FILTERAPI)(const Planet*, void*);
using SeedEndFunc = bool(*FILTERAPI)(void*);
