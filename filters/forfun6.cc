/*
 * Copyright (c) 2024 Soar Qin<soarchin@gmail.com>
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#include "filter.hh"

#include <fmt/format.h>
#include <mutex>
#include <vector>

std::mutex mtx;

template<typename T>
void updateMinSeed(int seed, T data, T &compareData, std::vector<std::pair<int, T>> &seeds) {
    std::unique_lock lock(mtx);
    if (seeds.empty() || data < seeds.back().second) {
        if (data < compareData) {
            compareData = data;
        }
        auto pair = std::make_pair(seed, data);
        seeds.emplace(std::upper_bound(seeds.begin(), seeds.end(), pair, [](const auto &lhs, const auto &rhs) {
            return lhs.second < rhs.second;
        }), std::move(pair));
        if (seeds.size() > 10) {
            seeds.pop_back();
        }
    }
}

template<typename T>
void updateMaxSeed(int seed, T data, T &compareData, std::vector<std::pair<int, T>> &seeds) {
    std::unique_lock lock(mtx);
    if (seeds.empty() || data > seeds.back().second) {
        if (data > compareData) {
            compareData = data;
        }
        auto pair = std::make_pair(seed, data);
        seeds.emplace(std::upper_bound(seeds.begin(), seeds.end(), pair, [](const auto &lhs, const auto &rhs) {
            return lhs.second > rhs.second;
        }), std::move(pair));
        if (seeds.size() > 10) {
            seeds.pop_back();
        }
    }
}

extern "C" {

std::vector<std::pair<int, float>> bMinSeeds;
std::vector<std::pair<int, float>> bMaxSeeds;
float bMinLum = 1000000.f;
float bMaxLum = 0.f;
std::vector<std::pair<int, float>> oMinSeeds;
std::vector<std::pair<int, float>> oMaxSeeds;
float oMinLum = 1000000.f;
float oMaxLum = 0.f;
std::vector<std::pair<int, float>> bgMaxSeeds;
float bgMaxLum = 0.f;
std::vector<std::pair<int, double>> oMaxDistSeeds;
double oMaxDist = 0;
std::vector<std::pair<int, double>> umMinDistSeeds;
std::vector<std::pair<int, double>> umMaxDistSeeds;
double umMinDist = 100000000000;
double umMaxDist = 0;
std::vector<std::pair<int, double>> umMaxDistTotalSeeds;
double umMaxTotalDist = 0;

__declspec(dllexport) const char *FILTERAPI init(PluginAPI *, int *type) {
    *type = 0;
    return "For Fun 6";
}

__declspec(dllexport) void FILTERAPI uninit() {
    fmt::print("B Min Lum: ");
    for (auto seed: bMinSeeds) {
        fmt::print(" {}({})", seed.first, std::pow(seed.second, 0.33000001311302185f));
    }
    fmt::println("");

    fmt::print("B Max Lum: ");
    for (auto seed: bMaxSeeds) {
        fmt::print(" {}({})", seed.first, std::pow(seed.second, 0.33000001311302185f));
    }
    fmt::println("");

    fmt::print("O Min Lum: ");
    for (auto seed: oMinSeeds) {
        fmt::print(" {}({})", seed.first, std::pow(seed.second, 0.33000001311302185f));
    }
    fmt::println("");

    fmt::print("O Max Lum: ");
    for (auto seed: oMaxSeeds) {
        fmt::print(" {}({})", seed.first, std::pow(seed.second, 0.33000001311302185f));
    }
    fmt::println("");

    fmt::print("BG Max Lum: ");
    for (auto seed: bgMaxSeeds) {
        fmt::print(" {}({})", seed.first, std::pow(seed.second, 0.33000001311302185f));
    }
    fmt::println("");

    fmt::print("O Max Dist: ");
    for (auto seed: oMaxDistSeeds) {
        fmt::print(" {}({})", seed.first, std::sqrt(seed.second));
    }
    fmt::println("");

    fmt::print("UM Min Dist: ");
    for (auto seed: umMinDistSeeds) {
        fmt::print(" {}({})", seed.first, std::sqrt(seed.second));
    }
    fmt::println("");

    fmt::print("UM Max Dist: ");
    for (auto seed: umMaxDistSeeds) {
        fmt::print(" {}({})", seed.first, std::sqrt(seed.second));
    }
    fmt::println("");

    fmt::print("UM Max Resource Coef: ");
    for (auto seed: umMaxDistTotalSeeds) {
        fmt::print(" {}({})", seed.first, std::sqrt(seed.second));
    }
    fmt::println("");
}

__declspec(dllexport) bool FILTERAPI galaxyFilter(const dspugen::Galaxy *g) {
    float bminl = 1000000.f;
    float bmaxl = 0.f;
    float ominl = 1000000.f;
    float omaxl = 0.f;
    float bgmaxl = 0.f;
    double omaxd = 0;
    double ummind = 100000000000;
    double ummaxd = 0;
    double umtotald = 0;
    for (auto *s: g->stars) {
        switch (s->type) {
            case dspugen::EStarType::MainSeqStar:
                switch (s->spectr) {
                    case dspugen::ESpectrType::B: {
                        auto l = s->luminosity;
                        if (l < bminl) {
                            bminl = l;
                        } else if (l > bmaxl) {
                            bmaxl = l;
                        }
                        break;
                    }
                    case dspugen::ESpectrType::O: {
                        auto l = s->luminosity;
                        if (l < ominl) {
                            ominl = l;
                        } else if (l > omaxl) {
                            omaxl = l;
                        }
                        auto d = s->position.sqrMagnitude();
                        if (d > omaxd) {
                            omaxd = d;
                        }
                        break;
                    }
                    default:
                        break;
                }
                break;
            case dspugen::EStarType::GiantStar: {
                auto l = s->luminosity;
                if (l > bgmaxl) {
                    bgmaxl = l;
                }
                break;
            }
            case dspugen::EStarType::BlackHole:
            case dspugen::EStarType::NeutronStar: {
                auto d = s->position.sqrMagnitude();
                auto distanceFactor = static_cast<float>(std::sqrt(d)) / 32.0f;
                if (distanceFactor > 1.0f) {
                    distanceFactor = std::log(distanceFactor) + 1.0f;
                    distanceFactor = std::log(distanceFactor) + 1.0f;
                    distanceFactor = std::log(distanceFactor) + 1.0f;
                    distanceFactor = std::log(distanceFactor) + 1.0f;
                    distanceFactor = std::log(distanceFactor) + 1.0f;
                }
                umtotald += std::pow(7.0f, distanceFactor) * 0.6f;
                if (d < ummind) {
                    ummind = d;
                } else if (d > ummaxd) {
                    ummaxd = d;
                }
                break;
            }
            default:
                break;
        }
    }
    updateMinSeed(g->seed, bminl, bMinLum, bMinSeeds);
    updateMaxSeed(g->seed, bmaxl, bMaxLum, bMaxSeeds);

    updateMinSeed(g->seed, ominl, oMinLum, oMinSeeds);
    updateMaxSeed(g->seed, omaxl, oMaxLum, oMaxSeeds);
    updateMaxSeed(g->seed, bgmaxl, bgMaxLum, bgMaxSeeds);
    updateMaxSeed(g->seed, omaxd, oMaxDist, oMaxDistSeeds);

    updateMinSeed(g->seed, ummind, umMinDist, umMinDistSeeds);
    updateMaxSeed(g->seed, ummaxd, umMaxDist, umMaxDistSeeds);
    updateMaxSeed(g->seed, umtotald, umMaxTotalDist, umMaxDistTotalSeeds);
    return false;
}

}
