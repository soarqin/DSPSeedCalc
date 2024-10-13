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
#include <mutex>

extern "C" {

static PluginAPI *theAPI = nullptr;
static std::ofstream starOut;

__declspec(dllexport) const char *FILTERAPI init(PluginAPI *api, int *type) {
    theAPI = api;
    *type = 1;
    starOut = std::ofstream("stars.csv");
    fmt::print(starOut, "种子,星数,编号,名字,亮度,类型\n");
    return "Star output";
}

__declspec(dllexport) void FILTERAPI uninit() {
    starOut.close();
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

inline static const char *SpectrToString(dspugen::EStarType type, dspugen::ESpectrType spectr) {
    switch (type) {
        case dspugen::EStarType::MainSeqStar:
            switch (spectr) {
                case dspugen::ESpectrType::O:
                    return "O";
                case dspugen::ESpectrType::B:
                    return "B";
                case dspugen::ESpectrType::A:
                    return "A";
                case dspugen::ESpectrType::F:
                    return "F";
                case dspugen::ESpectrType::G:
                    return "G";
                case dspugen::ESpectrType::K:
                    return "K";
                case dspugen::ESpectrType::M:
                    return "M";
                default:
                    return "X";
            }
            break;
        case dspugen::EStarType::GiantStar:
            return "Giant";
        case dspugen::EStarType::WhiteDwarf:
            return "WhiteDwarf";
        case dspugen::EStarType::NeutronStar:
            return "NeutronStar";
        case dspugen::EStarType::BlackHole:
            return "BlackHole";
    }
    return "Unknown";
}

static std::mutex mtx;
__declspec(dllexport) void FILTERAPI output(const dspugen::Galaxy *galaxy) {
    std::lock_guard lk(mtx);
    for (auto *star: galaxy->stars) {
        fmt::print(starOut, "{},{},{},{},{},{}\n",
                   galaxy->seed,
                   galaxy->starCount,
                   star->id,
                   star->name,
                   std::pow(star->luminosity, 0.33000001311302185f),
                   SpectrToString(star->type, star->spectr)
        );
    }
}

}
