/*
 * Copyright (c) 2024 Soar Qin<soarchin@gmail.com>
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#include "filter.hh"

extern "C" {

static PluginAPI *theAPI = nullptr;
static bool planets = false;

__declspec(dllexport) const char *FILTERAPI init2(PluginAPI *api, int *type, bool hasPlanets) {
    theAPI = api;
    *type = 0;
    planets = hasPlanets;
    return "Filter for Manufacturing Universe Matrices";
}

inline bool isThemeFullPower(int theme, double orbitRadius, double dysonRadius) {
    switch (theme) {
        case 2:
        case 3:
        case 4:
        case 5:
        case 21:
            return false;
        case 11:
            return orbitRadius <= dysonRadius * 0.738461538;
        case 7:
            return orbitRadius <= dysonRadius * 1.357047245;
        case 10:
        case 20:
        case 23:
        case 24:
            return orbitRadius <= dysonRadius * 1.402768768;
        case 6:
        case 13:
        case 19:
            return orbitRadius <= dysonRadius * 1.537522017;
        default:
            return orbitRadius <= dysonRadius * 1.448041666;
    }
}

__declspec(dllexport) bool FILTERAPI galaxyFilter(const Galaxy *g) {
    int cnt = 0;
    int giants = 0;
    for (auto *star: g->stars) {
        if (star->type == EStarType::GiantStar) {
            if (giants++ > 0) { return false; }
        }
        if (star->type == EStarType::MainSeqStar && star->luminosity >= 4.99263753f) {
            cnt++;
        }
    }
    if (cnt < 5) return false;
    cnt = 0;
    int umCount = 0;
    for (auto *star: g->stars) {
        if (!planets) {
            theAPI->GeneratePlanets(star);
        }
        switch (star->type) {
            case EStarType::MainSeqStar:
                if (star->luminosity >= 4.99263753f) {
                    auto *planet = star->planets[0];
                    auto dysonRad = std::round(star->dysonRadius * 40000.0 / 100.0) * 100.0;
                    if (isThemeFullPower(planet->theme, planet->orbitRadius * 40000.0, dysonRad))
                        cnt++;
                    if (star->luminosity >= 8.675074184f) {
                        planet = star->planets[1];
                        if (isThemeFullPower(planet->theme, planet->orbitRadius * 40000.0, dysonRad))
                            cnt++;
                    }
                    ++cnt;
                }
                break;
            case EStarType::BlackHole:
            case EStarType::NeutronStar: {
                auto *planet = star->planets[0];
                umCount += planet->veinSpot[14];
                ++cnt;
                break;
            }
        }
    }
    if (cnt < 5 || umCount < 20) return false;
    int water = 0, orange = 0, highHydrogen = 0, steeps = 0;
    for (auto *star: g->stars) {
        for (const auto *planet: star->planets) {
            switch (planet->theme) {
                case 16:
                    water++;
                    break;
                case 23:
                    orange++;
                    break;
                case 21:
                    highHydrogen++;
                    break;
            }
            steeps += planet->veinSpot[7];
        }
    }
    if (water < 4 || orange < 2 || highHydrogen < 5 || steeps < 350) return false;
    fprintf(stdout, "%d,%d\n", g->seed, g->starCount);
    return true;
}

}
