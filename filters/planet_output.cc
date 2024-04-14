/*
 * Copyright (c) 2022 Soar Qin<soarchin@gmail.com>
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#include "filter.hh"
#include <fmt/ostream.h>
#include <fstream>

extern "C" {

static PluginAPI *theAPI = nullptr;
static bool planets = false;
static std::ofstream planetOut;

__declspec(dllexport) const char *FILTERAPI init2(PluginAPI *api, int *type, bool hasPlanets) {
    theAPI = api;
    *type = 1;
    planets = hasPlanets;
    planetOut = std::ofstream("planets.csv");
    fmt::print(planetOut, "种子,星系数,编号,名字,星球类型\n");
    return "Planet output";
}

__declspec(dllexport) void FILTERAPI uninit() {
    planetOut.close();
}

static inline std::string id2roman(int id) {
    std::string roman;
    if (id >= 100) {
        roman += "C";
        id -= 100;
    }
    if (id >= 90) {
        roman += "XC";
        id -= 90;
    }
    if (id >= 50) {
        roman += "L";
        id -= 50;
    }
    if (id >= 40) {
        roman += "XL";
        id -= 40;
    }
    while (id >= 10) {
        roman += "X";
        id -= 10;
    }
    if (id == 9) {
        roman += "IX";
        id -= 9;
    }
    if (id >= 5) {
        roman += "V";
        id -= 5;
    }
    if (id == 4) {
        roman += "IV";
        id -= 4;
    }
    while (id > 0) {
        roman += "I";
        id -= 1;
    }
    return std::move(roman);
}

__declspec(dllexport) void FILTERAPI output(const Galaxy *galaxy) {
    for (auto *star: galaxy->stars) {
        if (!planets) {
            theAPI->GeneratePlanets(star);
        }
        for (const auto *planet: star->planets) {
            fmt::print(planetOut, "{},{},{},{},{}\n",
               galaxy->seed,
               galaxy->starCount,
               planet->id,
               star->name + ' ' + id2roman(planet->id % 100),
               planet->theme
            );
        }
    }
}

}
