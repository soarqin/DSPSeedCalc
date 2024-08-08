/*
 * Copyright (c) 2024 Soar Qin<soarchin@gmail.com>
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#include "filter.hh"

#include <fmt/std.h>
#include <fstream>
#include <set>
#include <mutex>

extern "C" {

static PluginAPI *theAPI = nullptr;

static std::ofstream *ofs = nullptr;
static std::mutex mtx;

__declspec(dllexport) const char *FILTERAPI init(PluginAPI *api, int *type) {
    theAPI = api;
    *type = 0;
    ofs = new std::ofstream("formatrix.csv", std::ios::out | std::ios::trunc);
    {
        std::unique_lock lk(mtx);
        fmt::print(*ofs,
                   "种子,星系数,距离母星最远,最大跨度,巨星数,全球锅行星数,全球锅行星列表,水世界数,橙晶数,高产气巨,油井数,磁石堆数,黑洞/中子星附近行星数A,B,C,高产气巨附近行星数A,B,C\n");
    }
    return "Filter for Manufacturing Universe Matrices";
}

__declspec(dllexport) void FILTERAPI uninit() {
    ofs->close();
    delete ofs;
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

__declspec(dllexport) bool FILTERAPI galaxyFilter(const dspugen::Galaxy *g) {
    int lcnt = 0;
    int giants = 0;
    for (auto *star: g->stars) {
        switch (star->type) {
            case dspugen::EStarType::GiantStar:
                if (giants++ > 0) { return false; }
                break;
            case dspugen::EStarType::MainSeqStar:
                if (star->luminosity >= 4.99263753f)
                    lcnt++;
                break;
        }
    }
    if (lcnt < 5) return false;

    int lumId[64];
    int hgId[64];
    int lumCnt = 0;
    int umCount = 0;
    theAPI->GenerateAllPlanets(g);
    for (const auto *star: g->stars) {
        switch (star->type) {
            case dspugen::EStarType::MainSeqStar:
                /* Luminosity >= 1.7f, can have al least 1 full planet photon receivers */
                if (star->luminosity >= 4.99263753f) {
                    auto *planet = star->planets[0];
                    auto dysonRad = std::round(star->dysonRadius * 40000.0 / 100.0) * 100.0;
                    if (isThemeFullPower(planet->theme, planet->orbitRadius * 40000.0, dysonRad)) {
                        lumId[lumCnt++] = planet->id;
                    }
                    /* Luminosity >= 2.04f, can have 2 full planet photon receivers */
                    if (star->luminosity >= 8.675074184f) {
                        planet = star->planets[1];
                        if (isThemeFullPower(planet->theme, planet->orbitRadius * 40000.0, dysonRad)) {
                            lumId[lumCnt++] = planet->id;
                        }
                    }
                }
                break;
            case dspugen::EStarType::BlackHole:
            case dspugen::EStarType::NeutronStar: {
                auto *planet = star->planets[0];
                umCount += planet->veinSpot[14];
                break;
            }
        }
    }
    if (lumCnt < 11 || umCount < 20) return false;
    int water = 0, orange = 0, highHydrogen = 0;
    for (const auto *star: g->stars) {
        switch (star->type) {
            case dspugen::EStarType::MainSeqStar:
            case dspugen::EStarType::GiantStar:
                for (const auto *planet: star->planets) {
                    switch (planet->theme) {
                        case 16:
                            water++;
                            break;
                        case 23:
                            orange++;
                            break;
                        case 21: {
                            theAPI->GeneratePlanetGas(planet);
                            auto &gasItems = planet->gasItems;
                            size_t sz = gasItems.size();
                            for (size_t i = 0; i < sz; i++) {
                                /* Gas Giant with Deuterium(1121) >= 0.15 (0.1875x0.8 under 0.1x resource) */
                                if (gasItems[i] == 1121 && planet->gasSpeeds[i] >= 0.1875f) {
                                    hgId[highHydrogen++] = planet->id;
                                }
                            }
                            break;
                        }
                    }
                }
                break;
            default: break;
        }
    }
    if (water < 5 || orange < 1 || highHydrogen < 5) return false;

    size_t sz = g->stars.size();

    double maxDist = 0;
    int steeps = 0;
    for (const auto *star: g->stars) {
        auto dist = star->position.sqrMagnitude();
        if (dist > maxDist) {
            maxDist = dist;
        }
        for (const auto *planet: star->planets) {
            steeps += planet->veinSpot[7];
        }
    }

    std::set<int> lumSet(lumId, lumId + lumCnt);
    std::set<int> countedStars;
    auto counter = [&lumSet](const dspugen::Star *star, std::set<int> &countedStars, int cnt[3]) {
        if (countedStars.find(star->id) != countedStars.end()) {
            return;
        }
        countedStars.insert(star->id);
        cnt[0] += int(star->planets.size());
        for (const auto *planet: star->planets) {
            if (lumSet.find(planet->id) != lumSet.end()) {
                continue;
            }
            switch (planet->theme) {
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 8:
                case 14:
                case 15:
                case 16:
                case 18:
                case 21:
                case 22:
                case 25:
                    break;
                case 6:
                case 11:
                case 12:
                    cnt[1]++;
                    break;
                default:
                    cnt[1]++;
                    cnt[2]++;
                    break;
            }
        }
    };
    double maxDist2 = 0;
    int magNearPlanets[3] = {0, 0, 0};
    int highHydrogenNearPlanets[3] = {0, 0, 0};
    for (size_t i = 0; i < sz; i++) {
        const auto *star = g->stars[i];
        for (size_t j = i + 1; j < sz; j++) {
            const auto *star2 = g->stars[j];
            auto dist = (star->position - star2->position).sqrMagnitude();
            if (dist > maxDist2) {
                maxDist2 = dist;
            }
        }
        if (star->type == dspugen::EStarType::BlackHole || star->type == dspugen::EStarType::NeutronStar) {
            for (size_t j = 0; j < sz; j++) {
                const auto *star2 = g->stars[j];
                if ((star->position - star2->position).sqrMagnitude() > 225.0) continue;
                counter(star2, countedStars, magNearPlanets);
            }
        }
    }
    countedStars.clear();
    std::set<int> hgSet(hgId, hgId + highHydrogen);
    for (auto planetId: hgSet) {
        auto sid = planetId / 100;
        const auto * star = g->starById(sid);
        for (size_t j = 0; j < sz; j++) {
            const auto *star2 = g->stars[j];
            if ((star->position - star2->position).sqrMagnitude() > 225.0) continue;
            counter(star2, countedStars, highHydrogenNearPlanets);
        }
    }

    std::string allLumPlanets;
    for (int i = 0; i < lumCnt; i++) {
        if (i > 0) {
            allLumPlanets += '|';
        }
        auto *star = g->starById(lumId[i] / 100);
        allLumPlanets += star->name;
        allLumPlanets += ' ';
        allLumPlanets += id2roman(lumId[i] % 100);
    }
    {
        std::unique_lock lk(mtx);
        fmt::print(*ofs, "{},{},{:.3f},{:.3f},{},{},{},{},{},{},{},{},{},{},{},{},{},{}\n",
                   g->seed,
                   g->starCount,
                   std::sqrt(maxDist),
                   std::sqrt(maxDist2),
                   giants,
                   lumCnt,
                   allLumPlanets,
                   water,
                   orange,
                   highHydrogen,
                   steeps,
                   umCount,
                   magNearPlanets[0],
                   magNearPlanets[1],
                   magNearPlanets[2],
                   highHydrogenNearPlanets[0],
                   highHydrogenNearPlanets[1],
                   highHydrogenNearPlanets[2]);
    }
    return true;
}

}
