/*
 * Copyright (c) 2022 Soar Qin<soarchin@gmail.com>
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#include "planet.hh"

#include "star.hh"
#include "galaxy.hh"
#include "protoset.hh"
#include "util/dotnet35random.hh"
#include "util/maths.hh"

static constexpr float OrbitRadiusFactor[17] = {
    0.0f, 0.4f, 0.7f, 1.0f, 1.4f, 1.9f, 2.5f, 3.3f, 4.3f, 5.5f,
    6.9f, 8.4f, 10.0f, 11.7f, 13.5f, 15.4f, 17.5f
};

Planet::Ptr Planet::create(Star *star, int index, int orbitAround, int orbitIndex, int number,
                           bool gasGiant, int infoSeed, int genSeed) {
    auto planet = new Planet;
    DotNet35Random dotNet35Random(infoSeed);
    auto *galaxy = star->galaxy;
    planet->index = index;
    planet->galaxy = galaxy;
    planet->star = star;
    planet->seed = genSeed;
/*
    planet->infoSeed = infoSeed;
*/
    planet->orbitAround = orbitAround;
    planet->orbitIndex = orbitIndex;
    planet->number = number;
    planet->id = star->id * 100 + index + 1;
    auto &stars = galaxy->stars;
    auto num = 0;
    for (auto i = 0; i < star->index; i++) num += stars[i]->planets.size();
    num += index;
    if (orbitAround > 0) {
        auto planetCount = int(star->planets.size());
        for (auto j = 0; j < planetCount; j++)
            if (orbitAround == star->planets[j]->number && star->planets[j]->orbitAround == 0) {
                planet->orbitAroundPlanet = star->planets[j].get();
                if (orbitIndex > 1)
                    planet->orbitAroundPlanet->singularity |= EPlanetSingularity::MultipleSatellites;
                break;
            }
    }
    auto num2 = dotNet35Random.nextDouble();
    auto num3 = dotNet35Random.nextDouble();
    auto num4 = dotNet35Random.nextDouble();
    auto num5 = dotNet35Random.nextDouble();
    auto num6 = dotNet35Random.nextDouble();
    auto num7 = dotNet35Random.nextDouble();
    auto num8 = dotNet35Random.nextDouble();
    auto num9 = dotNet35Random.nextDouble();
    auto num10 = dotNet35Random.nextDouble();
    auto num11 = dotNet35Random.nextDouble();
    auto num12 = dotNet35Random.nextDouble();
    auto num13 = dotNet35Random.nextDouble();
    auto rand = dotNet35Random.nextDouble();
    auto num14 = dotNet35Random.nextDouble();
    auto rand2 = dotNet35Random.nextDouble();
    auto rand3 = dotNet35Random.nextDouble();
    auto rand4 = dotNet35Random.nextDouble();
    auto themeSeed = dotNet35Random.next();
    auto num15 = (float)std::pow(1.2f, (float)(num2 * (num3 - 0.5) * 0.5));
    auto num16 = 0.0f;
    if (orbitAround == 0) {
        num16 = OrbitRadiusFactor[orbitIndex] * star->orbitScaler;
        auto num17 = (num15 - 1.0f) / std::max(1.0f, num16) + 1.0f;
        num16 *= num17;
    } else {
        num16 = (float)(((1600.0f * orbitIndex + 200.0f) * (float)std::pow(star->orbitScaler, 0.3f) *
            maths::lerp(num15, 1.0f, 0.5f) + planet->orbitAroundPlanet->realRadius()) / 40000.0);
    }

    planet->orbitRadius = num16;
/*
    planet->orbitInclination = (float)(num4 * 16.0 - 8.0);
    if (orbitAround > 0) planet->orbitInclination *= 2.2f;
    planet->orbitLongitude = (float)(num5 * 360.0);
    if (star->type >= EStarType::NeutronStar) {
        if (planet->orbitInclination > 0.0f)
            planet->orbitInclination += 3.0f;
        else
            planet->orbitInclination -= 3.0f;
    }

    planet->orbitalPeriod =
        std::sqrt(39.478417604357432 * num16 * num16 * num16 / (planet->orbitAroundPlanet == nullptr
                                                                ? 1.3538551990520382E-06 * star->mass
                                                                : 1.0830842106853677E-08));
    planet->orbitPhase = (float)(num6 * 360.0);
*/
    if (num14 < 0.039999999105930328) {
        // planet->obliquity = (float)(num7 * (num8 - 0.5) * 39.9);
        // if (planet->obliquity < 0.0f)
        //     planet->obliquity -= 70.0f;
        // else
        //     planet->obliquity += 70.0f;
        planet->singularity |= EPlanetSingularity::LaySide;
    } else if (num14 < 0.10000000149011612) {
        // planet->obliquity = (float)(num7 * (num8 - 0.5) * 80.0);
        // if (planet->obliquity < 0.0f)
        //     planet->obliquity -= 30.0f;
        // else
        //     planet->obliquity += 30.0f;
    } else {
        // planet->obliquity = (float)(num7 * (num8 - 0.5) * 60.0);
    }

/*
    planet->rotationPeriod = (num9 * num10 * 1000.0 + 400.0) *
        (orbitAround == 0 ? (float)std::pow(num16, 0.25f) : 1.0f) *
        (gasGiant ? 0.2f : 1.0f);
    if (!gasGiant) {
        if (star->type == EStarType::WhiteDwarf)
            planet->rotationPeriod *= 0.5;
        else if (star->type == EStarType::NeutronStar)
            planet->rotationPeriod *= 0.20000000298023224;
        else if (star->type == EStarType::BlackHole) planet->rotationPeriod *= 0.15000000596046448;
    }

    planet->rotationPhase = (float)(num11 * 360.0);
*/
    planet->sunDistance =
        orbitAround == 0 ? planet->orbitRadius : planet->orbitAroundPlanet->orbitRadius;
    planet->scale = 1.0f;
/*
    auto num18 = orbitAround == 0 ? planet->orbitalPeriod : planet->orbitAroundPlanet->orbitalPeriod;
    planet->rotationPeriod = 1.0 / (1.0 / num18 + 1.0 / planet->rotationPeriod);
*/
    if (orbitAround == 0 && orbitIndex <= 4 && !gasGiant) {
        if (num14 > 0.95999997854232788) {
/*
            planet->obliquity *= 0.01f;
            planet->rotationPeriod = planet->orbitalPeriod;
*/
            planet->singularity |= EPlanetSingularity::TidalLocked;
        } else if (num14 > 0.93000000715255737) {
/*
            planet->obliquity *= 0.1f;
            planet->rotationPeriod = planet->orbitalPeriod * 0.5;
*/
            planet->singularity |= EPlanetSingularity::TidalLocked2;
        } else if (num14 > 0.89999997615814209) {
/*
            planet->obliquity *= 0.2f;
            planet->rotationPeriod = planet->orbitalPeriod * 0.25;
*/
            planet->singularity |= EPlanetSingularity::TidalLocked4;
        }
    }

    if (num14 > 0.85 && num14 <= 0.9) {
/*
        planet->rotationPeriod = 0.0 - planet->rotationPeriod;
*/
        planet->singularity |= EPlanetSingularity::ClockwiseRotate;
    }

    auto habitableRadius = star->habitableRadius;
    if (gasGiant) {
        planet->type = EPlanetType::Gas;
        planet->radius = 80.0f;
        planet->scale = 10.0f;
        planet->habitableBias = 100.0f;
    } else {
        auto num19 = (float)std::ceil(galaxy->starCount * 0.29f);
        if (num19 < 11.0f) num19 = 11.0f;
        auto num20 = num19 - galaxy->HabitableCount;
        float num21 = galaxy->starCount - star->index;
        auto sunDistance = planet->sunDistance;
        auto num22 = 1000.0f;
        auto num23 = 1000.0f;
        if (habitableRadius > 0.0f && sunDistance > 0.0f) {
            num23 = sunDistance / habitableRadius;
            num22 = std::abs((float)std::log(num23));
        }

        auto num24 = std::clamp((float)std::sqrt(habitableRadius), 1.0f, 2.0f) - 0.04f;
        auto a = num20 / num21;
        a = maths::lerp(a, 0.35f, 0.5f);
        a = std::clamp(a, 0.08f, 0.8f);
        planet->habitableBias = num22 * num24;
        planet->temperatureBias = 1.2f / (num23 + 0.2f) - 1.0f;

        auto f = maths::clamp01(planet->habitableBias / a);
        auto p = a * 10.0f;
        f = (float)std::pow(f, p);
        if (num12 > f && star->index > 0 ||
            planet->orbitAround > 0 && planet->orbitIndex == 1 && star->index == 0) {
            planet->type = EPlanetType::Ocean;
            galaxy->HabitableCount++;
        } else if (num23 < 0.833333f) {
            auto num25 = std::max(0.15f, num23 * 2.5f - 0.85f);
            planet->type = num13 < num25 ? EPlanetType::Desert : EPlanetType::Vocano;
        } else if (num23 < 1.2f) {
            planet->type = EPlanetType::Desert;
        } else {
            auto num26 = 0.9f / num23 - 0.1f;
            planet->type = num13 < num26 ? EPlanetType::Desert : EPlanetType::Ice;
        }

        planet->radius = 200.0f;
    }

/*
    if (planet->type != EPlanetType::Gas && planet->type != EPlanetType::None) {
        planet->precision = 200;
        planet->segment = 5;
    } else {
        planet->precision = 64;
        planet->segment = 2;
    }
*/

    planet->luminosity =
        (float)std::pow(planet->star->lightBalanceRadius / (planet->sunDistance + 0.01f), 0.6f);
    if (planet->luminosity > 1.0f) {
        planet->luminosity = (float)std::log(planet->luminosity) + 1.0f;
        planet->luminosity = (float)std::log(planet->luminosity) + 1.0f;
        planet->luminosity = (float)std::log(planet->luminosity) + 1.0f;
    }

    planet->luminosity = (float)std::round(planet->luminosity * 100.0f) / 100.0f;
    planet->setPlanetTheme(rand, rand2, rand3, rand4, themeSeed);
    planet->generateVeins();
    return Ptr(planet);
}

void Planet::setPlanetTheme(double rand1, double rand2, double rand3, double rand4, int themeSeed) {
    int tmpTheme[32];
    int tmpThemeCount = 0;
    for (const auto &themeProto: themeProtoSet.dataArray) {
        auto flag = false;
        if (star->index == 0 && type == EPlanetType::Ocean) {
            if (themeProto.distribute == EThemeDistribute::Birth) flag = true;
        } else if (themeProto.planetType == (int)type &&
            themeProto.temperature * temperatureBias >= -0.1f) {
            if (star->index == 0) {
                if (themeProto.distribute == EThemeDistribute::Default) flag = true;
            } else if (themeProto.distribute != EThemeDistribute::Birth) {
                flag = true;
            }
        }

        if (flag)
            for (auto j = 0; j < index; j++)
                if (star->planets[j]->theme == themeProto.id) {
                    flag = false;
                    break;
                }

        if (flag) tmpTheme[tmpThemeCount++] = themeProto.id;
    }

    if (tmpThemeCount == 0)
        for (const auto &themeProto2: themeProtoSet.dataArray) {
            auto flag2 = themeProto2.planetType == (int)EPlanetType::Desert;
            if (flag2)
                for (auto l = 0; l < index; l++)
                    if (star->planets[l]->theme == themeProto2.id) {
                        flag2 = false;
                        break;
                    }

            if (flag2) tmpTheme[tmpThemeCount++] = themeProto2.id;
        }

    if (tmpThemeCount == 0)
        for (const auto &themeProto3: themeProtoSet.dataArray) {
            if (themeProto3.planetType == (int)EPlanetType::Desert) tmpTheme[tmpThemeCount++] = themeProto3.id;
        }

    theme = tmpTheme[(int)(rand1 * tmpThemeCount) % tmpThemeCount];
    const auto *themeProto4 = themeProtoSet.select(theme);
    algoId = 0;
    if (themeProto4 != nullptr && !themeProto4->algos.empty()) {
        auto count = int(themeProto4->algos.size());
        algoId = themeProto4->algos[(int)(rand2 * count) % count];
/*
        modX = themeProto4->modX.x + rand3 * (themeProto4->modX.y - themeProto4->modX.x);
        modY = themeProto4->modY.x + rand4 * (themeProto4->modY.y - themeProto4->modY.x);
*/
    }

    if (themeProto4 == nullptr) return;
    type = EPlanetType(themeProto4->planetType);
/*
    style = themeSeed % 60;
    ionHeight = themeProto4->ionHeight;
    windStrength = themeProto4->wind;
    waterHeight = themeProto4->waterHeight;
    waterItemId = themeProto4->waterItemId;
    levelized = themeProto4->useHeightForBuild;
    iceFlag = themeProto4->iceFlag;
    if (type == EPlanetType::Gas) {
        auto num2 = int(themeProto4->gasItems.size());
        auto num3 = int(themeProto4->gasSpeeds.size());
        gasItems = themeProto4->gasItems;
        gasSpeeds.resize(num3);
        gasHeatValues.resize(num2);
        auto num4 = 0.0;
        DotNet35Random dotNet35Random(themeSeed);
        for (auto num5 = 0; num5 < num3; num5++) {
            auto num6 = themeProto4->gasSpeeds[num5];
            num6 *= (float)dotNet35Random.nextDouble() * 0.190909147f + 0.9090909f;
            gasSpeeds[num5] = num6 * std::pow(star->resourceCoef, 0.3f);
            auto *itemProto = itemProtoSet.select(gasItems[num5]);
            gasHeatValues[num5] = itemProto->heatValue;
            num4 += gasHeatValues[num5] * gasSpeeds[num5];
        }

        gasTotalHeat = num4;
    }
*/
}

void Planet::generateVeins() {
    if (algoId >= 1 && algoId <= 9) {
        const auto *themeProto = themeProtoSet.select(theme);
        if (themeProto == nullptr) return;
        DotNet35Random dotNet35Random(seed);
        dotNet35Random.next();
        dotNet35Random.next();
        dotNet35Random.next();
        dotNet35Random.next();
        dotNet35Random.next();
        DotNet35Random dotNet35Random2(dotNet35Random.next());
        auto num = 2.1f / radius;
        memcpy(&veinSpot[1], &themeProto->veinSpot[0], sizeof(int) * std::min(14, int(themeProto->veinSpot.size())));
        auto p = 1.0f;
        auto spectr = star->spectr;
        switch (star->type) {
        case EStarType::MainSeqStar:
            switch (spectr) {
            case ESpectrType::M:p = 2.5f;
                break;
            case ESpectrType::K:p = 1.0f;
                break;
            case ESpectrType::G:p = 0.7f;
                break;
            case ESpectrType::F:p = 0.6f;
                break;
            case ESpectrType::A:p = 1.0f;
                break;
            case ESpectrType::B:p = 0.4f;
                break;
            case ESpectrType::O:p = 1.6f;
                break;
            }

            break;
        case EStarType::GiantStar:p = 2.5f;
            break;
        case EStarType::WhiteDwarf: {
            p = 3.5f;
            veinSpot[9] += 2;
            for (auto j = 1; j < 12; j++) {
                if (dotNet35Random.nextDouble() >= 0.44999998807907104) break;
                veinSpot[9]++;
            }

            veinSpot[10] += 2;
            for (auto k = 1; k < 12; k++) {
                if (dotNet35Random.nextDouble() >= 0.44999998807907104) break;
                veinSpot[10]++;
            }

            veinSpot[12]++;
            for (auto l = 1; l < 12; l++) {
                if (dotNet35Random.nextDouble() >= 0.5) break;
                veinSpot[12]++;
            }

            break;
        }
        case EStarType::NeutronStar: {
            p = 4.5f;
            veinSpot[14]++;
            for (auto m = 1; m < 12; m++) {
                if (dotNet35Random.nextDouble() >= 0.64999997615814209) break;
                veinSpot[14]++;
            }
            break;
        }
        case EStarType::BlackHole: {
            p = 5.0f;
            veinSpot[14]++;
            for (auto i = 1; i < 12; i++) {
                if (dotNet35Random.nextDouble() >= 0.64999997615814209) break;
                veinSpot[14]++;
            }

            break;
        }
        }

        auto rareVeinsSize = int(themeProto->rareVeins.size());
        for (auto n = 0; n < rareVeinsSize; n++) {
            auto num2 = themeProto->rareVeins[n];
            auto num3 = star->index == 0
                       ? themeProto->rareSettings[n * 4]
                       : themeProto->rareSettings[n * 4 + 1];
            auto num4 = themeProto->rareSettings[n * 4 + 2];
            num3 = 1.0f - (float)std::pow(1.0f - num3, p);
            if (dotNet35Random.nextDouble() >= num3) continue;
            veinSpot[num2]++;
            for (auto num7 = 1; num7 < 12; num7++) {
                if (dotNet35Random.nextDouble() >= num4) break;
                veinSpot[num2]++;
            }
        }
    }
}
