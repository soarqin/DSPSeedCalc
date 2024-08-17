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
static std::ofstream starOut;

__declspec(dllexport) const char *FILTERAPI init2(PluginAPI *api, int *type, bool hasPlanets) {
    theAPI = api;
    *type = 1;
    planets = hasPlanets;
    starOut = std::ofstream("dp_stars.csv");
    fmt::print(starOut,
               "种子,星系数,母星巨星类型,母星地表可燃冰,母星巨星卫星数,蓝巨1亮度,蓝巨1行星数,蓝巨2亮度,蓝巨2行星数,O星数,水世界数,橙晶数,磁石簇数\n");
    return "DSP-Power Related Output";
}

__declspec(dllexport) void FILTERAPI uninit() {
    starOut.close();
}

__declspec(dllexport) void FILTERAPI output(const dspugen::Galaxy *galaxy) {
    bool isGas = false;
    bool groundFireIce = false;
    int gasCount = 0;
    float blue1Lum = 0;
    int blue1Planets = 0;
    float blue2Lum = 0;
    int blue2Planets = 0;
    int oCount = 0;
    int waterCount = 0;
    int orangeCount = 0;
    int magnetCount = 0;
    if (!planets) theAPI->GenerateAllPlanets(galaxy);
    for (auto *star: galaxy->stars) {
        if (star->index == 0) {
            for (const auto *planet: star->planets) {
                switch (planet->theme) {
                    case 2:
                    case 3:
                    case 21:
                        isGas = true;
                        break;
                    case 4:
                    case 5:
                        isGas = false;
                        break;
                    default:
                        if (planet->orbitAround != 0) {
                            gasCount++;
                        }
                        if (planet->veinSpot[8] > 2) {
                            groundFireIce = true;
                        }
                        break;
                }
            }
        } else {
            switch (star->type) {
                case dspugen::EStarType::GiantStar:
                    if (star->spectr == dspugen::ESpectrType::O) {
                        int cnt = 0;
                        for (auto *planet: star->planets) {
                            switch (planet->theme) {
                                case 2:
                                case 3:
                                case 4:
                                case 5:
                                case 21:
                                    break;
                                default:
                                    cnt++;
                                    break;
                            }
                        }
                        if (blue1Lum == 0.f) {
                            blue1Lum = std::pow(star->luminosity, 0.33000001311302185f);
                            blue1Planets = cnt;
                        } else {
                            blue2Lum = std::pow(star->luminosity, 0.33000001311302185f);
                            blue2Planets = cnt;
                        }
                    }
                    break;
                case dspugen::EStarType::MainSeqStar:
                    if (star->spectr == dspugen::ESpectrType::O) {
                        oCount++;
                    }
                    for (const auto *planet: star->planets) {
                        switch (planet->theme) {
                            case 16:
                                waterCount++;
                                break;
                            case 23:
                                orangeCount++;
                                break;
                            default:
                                break;
                        }
                    }
                    break;
                case dspugen::EStarType::BlackHole:
                case dspugen::EStarType::NeutronStar:
                    for (const auto *planet: star->planets) {
                        magnetCount += planet->veinSpot[14];
                    }
                    break;
                default:
                    break;
            }
        }
    }
    fmt::print(starOut, "{},{},{},{},{},{:.3f},{},{:.3f},{},{},{},{},{}\n",
               galaxy->seed,
               galaxy->starCount,
               isGas ? "气" : "冰",
               groundFireIce ? "Y" : "N",
               gasCount,
               blue1Lum,
               blue1Planets,
               blue2Lum,
               blue2Planets,
               oCount,
               waterCount,
               orangeCount,
               magnetCount);
}

}
