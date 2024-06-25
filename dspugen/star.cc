/*
 * Copyright (c) 2022 Soar Qin<soarchin@gmail.com>
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

/* for visual studio:
 *   _USE_MATH_DEFINES: required for M_PI
 */
#if defined(_MSC_VER)
#define _USE_MATH_DEFINES
#endif
#include "galaxy.hh"
#include "star.hh"
#include "namegen.hh"
#include "util/dotnet35random.hh"
#include "util/maths.hh"
#include "util/mempool.hh"

#include <algorithm>
#include <functional>
#include <cmath>

namespace dspugen {

static float randNormal(float averageValue, float standardDeviation, double r1, double r2) {
    return averageValue + standardDeviation *
        (float)(std::sqrt(-2.0 * std::log(1.0 - r1)) * std::sin(M_PI * 2.0 * r2));
}

static thread_local util::MemPool<Star> spool;
static auto log10_26 = std::log10(2.6);
static auto log10_5 = std::log10(5.0);

Star::~Star() {
    for (auto *p: planets) {
        p->release();
    }
}

void Star::release() {
    spool.release(this);
}

Star *Star::createStar(Galaxy *galaxy,
                       VectorLF3 pos,
                       int id,
                       int seed,
                       EStarType needtype,
                       ESpectrType needSpectr,
                       bool genName) {
    auto *star = spool.alloc();
    star->galaxy = galaxy;
    star->index = id - 1;
    if (galaxy->starCount > 1)
        star->level = (float)star->index / (float)(galaxy->starCount - 1);
    else
        star->level = 0.0f;
    star->id = id;
    star->seed = seed;
    util::DotNet35Random dotNet35Random(seed);
    auto seed2 = dotNet35Random.next();
    auto seed3 = dotNet35Random.next();
    star->position = pos;
/*
    auto num = (float)pos.magnitude() / 32.0f;
    if (num > 1.0f) {
        num = (float)std::log(num) + 1.0f;
        num = (float)std::log(num) + 1.0f;
        num = (float)std::log(num) + 1.0f;
        num = (float)std::log(num) + 1.0f;
        num = (float)std::log(num) + 1.0f;
    }
    star->resourceCoef = (float)std::pow(7.0f, num) * 0.6f;
*/
    util::DotNet35Random dotNet35Random2(seed3);
    auto num2 = dotNet35Random2.nextDouble();
    auto num3 = dotNet35Random2.nextDouble();
    auto num4 = dotNet35Random2.nextDouble();
    auto rn = dotNet35Random2.nextDouble();
    auto rt = dotNet35Random2.nextDouble();
    auto num5 = (dotNet35Random2.nextDouble() - 0.5) * 0.2;
    auto num6 = dotNet35Random2.nextDouble() * 0.2 + 0.9;
    auto num7 = dotNet35Random2.nextDouble() * 0.4 - 0.2;
    auto num8 = std::pow(2.0, num7);
    auto num9 = util::lerp(-0.98f, 0.88f, star->level);
    num9 = num9 >= 0.0f ? num9 + 0.65f : num9 - 0.65f;
    auto standardDeviation = 0.33f;
    if (needtype == EStarType::GiantStar) {
        num9 = num7 > -0.08 ? -1.5f : 1.6f;
        standardDeviation = 0.3f;
    }

    float num10;
    switch (needSpectr) {
        case ESpectrType::M:
            num10 = -3.0f;
            break;
        case ESpectrType::O:
            num10 = 3.0f;
            break;
        default:
            num10 = randNormal(num9, standardDeviation, num2, num3);
            break;
    }

    num10 = num10 <= 0.0f ? num10 * 1.0f : num10 * 2.0f;
    num10 = std::clamp(num10, -2.4f, 4.65f) + (float)num5 + 1.0f;
    switch (needtype) {
        case EStarType::BlackHole:
            star->mass = 18.0f + (float)(num2 * num3) * 30.0f;
            break;
        case EStarType::NeutronStar:
            star->mass = 7.0f + (float)num2 * 11.0f;
            break;
        case EStarType::WhiteDwarf:
            star->mass = 1.0f + (float)num3 * 5.0f;
            break;
        default:
            star->mass = (float)std::pow(2.0f, num10);
            break;
    }

    auto d = 5.0;
    if (star->mass < 2.0f) d = 2.0 + 0.4 * (1.0 - star->mass);
    star->lifetime =
        (float)(10000.0 * std::pow(0.1, std::log10(star->mass * 0.5) / std::log10(d) + 1.0) * num6);
    switch (needtype) {
        case EStarType::GiantStar:
            star->lifetime = (float)(10000.0 *
                std::pow(0.1, std::log10(star->mass * 0.58) / std::log10(d) + 1.0) *
                num6);
            star->age = (float)num4 * 0.04f + 0.96f;
            break;
        case EStarType::WhiteDwarf:
        case EStarType::NeutronStar:
        case EStarType::BlackHole:
            star->age = (float)num4 * 0.4f + 1.0f;
            switch (needtype) {
                case EStarType::WhiteDwarf:
                    star->lifetime += 10000.0f;
                    break;
                case EStarType::NeutronStar:
                    star->lifetime += 1000.0f;
                    break;
            }

            break;
        default:
            if (star->mass < 0.5)
                star->age = (float)num4 * 0.12f + 0.02f;
            else if (star->mass < 0.8)
                star->age = (float)num4 * 0.4f + 0.1f;
            else
                star->age = (float)num4 * 0.7f + 0.2f;
            break;
    }

    auto num11 = star->lifetime * star->age;
    if (num11 > 5000.0f) num11 = ((float)std::log(num11 / 5000.0f) + 1.0f) * 5000.0f;
    if (num11 > 8000.0f)
        num11 = ((float)std::log((float)std::log((float)std::log(num11 / 8000.0f) + 1.0f) + 1.0f) + 1.0f) * 8000.0f;
    star->lifetime = num11 / star->age;
    auto num12 = (1.0f - (float)std::pow(util::clamp01(star->age), 20.0f) * 0.5f) * star->mass;
    star->temperature =
        (float)(std::pow(num12, 0.56 + 0.14 / (std::log10(num12 + 4.0f) / log10_5)) * 4450.0 + 1300.0);
    auto num13 = std::log10((star->temperature - 1300.0) / 4500.0) / log10_26 - 0.5;
    if (num13 < 0.0) num13 *= 4.0;
    if (num13 > 2.0)
        num13 = 2.0;
    else if (num13 < -4.0) num13 = -4.0;
    star->spectr = (ESpectrType)(int)std::round((float)num13 + 4.0f);
    star->color = util::clamp01(((float)num13 + 3.5f) * 0.2f);
/*
    star->classFactor = (float)num13;
*/
    star->luminosity = (float)std::pow(num12, 0.7f);
    star->radius = (float)(std::pow(star->mass, 0.4) * num8);
/*
    star->acdiskRadius = 0.0f;
*/
    auto p = (float)num13 + 2.0f;
    star->habitableRadius = (float)std::pow(1.7f, p) + 0.25f * std::min(1.0f, star->orbitScaler);
    star->lightBalanceRadius = (float)std::pow(1.7f, p);
    star->orbitScaler = (float)std::pow(1.35f, p);
    if (star->orbitScaler < 1.0f) star->orbitScaler = util::lerp(star->orbitScaler, 1.0f, 0.6f);
    star->setStarAge(rn, rt);
    star->dysonRadius = star->orbitScaler * 0.28f;
    auto radMin = (float)(star->physicsRadius() * 1.5 / 40000.0);
    if (star->dysonRadius < radMin)
        star->dysonRadius = radMin;
/*
    star->uPosition = star->position * 2400000.0;
*/
    if (genName) {
        star->name = NameGen::randomStarName(seed2, star, galaxy);
    }
    return star;
}

Star *Star::createBirthStar(Galaxy *galaxy, int seed, bool genName) {
    auto star = spool.alloc();
    star->galaxy = galaxy;
    star->seed = seed;
    util::DotNet35Random dotNet35Random(seed);
    auto seed2 = dotNet35Random.next();
    auto seed3 = dotNet35Random.next();
    util::DotNet35Random dotNet35Random2(seed3);
    auto r = dotNet35Random2.nextDouble();
    auto r2 = dotNet35Random2.nextDouble();
    auto num = dotNet35Random2.nextDouble();
    auto rn = dotNet35Random2.nextDouble();
    auto rt = dotNet35Random2.nextDouble();
    auto num2 = dotNet35Random2.nextDouble() * 0.2 + 0.9;
    auto y = dotNet35Random2.nextDouble() * 0.4 - 0.2;
    auto num3 = std::pow(2.0, y);
    auto value = randNormal(0.0f, 0.08f, r, r2);
    value = std::clamp(value, -0.2f, 0.2f);
    star->mass = std::pow(2.0f, value);
    auto num4 = 5.0;
    num4 = 2.0 + 0.4 * (1.0 - star->mass);
    star->lifetime =
        (float)(10000.0 * std::pow(0.1, std::log10(star->mass * 0.5) / std::log10(num4) + 1.0) * num2);
    star->age = (float)(num * 0.4 + 0.3);
    auto num5 = (1.0f - (float)std::pow(std::clamp(star->age, 0.0f, 1.0f), 20.0f) * 0.5f) * star->mass;
    star->temperature =
        (float)(std::pow(num5, 0.56 + 0.14 / (std::log10(num5 + 4.0f) / log10_5)) * 4450.0 + 1300.0);
    auto num6 = std::log10((star->temperature - 1300.0) / 4500.0) / log10_26 - 0.5;
    if (num6 < 0.0) num6 *= 4.0;
    if (num6 > 2.0)
        num6 = 2.0;
    else if (num6 < -4.0) num6 = -4.0;
    star->spectr = (ESpectrType)(int)std::round((float)num6 + 4.0f);
    star->color = std::clamp(((float)num6 + 3.5f) * 0.2f, 0.0f, 1.0f);
/*
    star->classFactor = (float)num6;
*/
    star->luminosity = (float)std::pow(num5, 0.7f);
    star->radius = (float)(std::pow(star->mass, 0.4) * num3);
/*
    star->acdiskRadius = 0.0f;
*/
    auto p = (float)num6 + 2.0f;
    star->habitableRadius = (float)std::pow(1.7f, p) + 0.2f * std::min(1.0f, star->orbitScaler);
    star->lightBalanceRadius = (float)std::pow(1.7f, p);
    star->orbitScaler = (float)std::pow(1.35f, p);
    if (star->orbitScaler < 1.0f) star->orbitScaler = util::lerp(star->orbitScaler, 1.0f, 0.6f);
    star->setStarAge(rn, rt);
    star->dysonRadius = star->orbitScaler * 0.28f;
    if (star->dysonRadius * 40000.0f < star->physicsRadius() * 1.5f)
        star->dysonRadius = star->physicsRadius() * 1.5f / 40000.0f;
    if (genName) {
        star->name = NameGen::randomStarName(seed2, star, galaxy);
    }
    return star;
}

void Star::setStarAge(double rn, double rt) {
    auto num = (float)(rn * 0.1 + 0.95);
    auto num2 = (float)(rt * 0.4 + 0.8);
    auto num3 = (float)(rt * 9.0 + 1.0);
    if (age >= 1.0f) {
        if (mass >= 18.0f) {
            type = EStarType::BlackHole;
            spectr = ESpectrType::X;
            mass *= 2.5f * num2;
            radius *= 1.0f;
/*
            acdiskRadius = radius * 5.0f;
*/
            temperature = 0.0f;
            luminosity *= 0.001f * num;
            habitableRadius = 0.0f;
            lightBalanceRadius *= 0.4f * num;
            color = 1.0f;
        } else if (mass >= 7.0f) {
            type = EStarType::NeutronStar;
            spectr = ESpectrType::X;
            mass *= 0.2f * num;
            radius *= 0.15f;
/*
            acdiskRadius = radius * 9.0f;
*/
            temperature = num3 * 1E+07f;
            luminosity *= 0.1f * num;
            habitableRadius = 0.0f;
            lightBalanceRadius *= 3.0f * num;
            orbitScaler *= 1.5f * num;
            color = 1.0f;
        } else {
            type = EStarType::WhiteDwarf;
            spectr = ESpectrType::X;
            mass *= 0.2f * num;
            radius *= 0.2f;
/*
            acdiskRadius = 0.0f;
*/
            temperature = num2 * 150000.0f;
            luminosity *= 0.04f * num2;
            habitableRadius *= 0.15f * num2;
            lightBalanceRadius *= 0.2f * num;
            color = 0.7f;
        }
    } else if (age >= 0.96f) {
        auto num4 = (float)(std::pow(5.0, std::abs(std::log10(mass) - 0.7)) * 5.0);
        if (num4 > 10.0f) num4 = ((float)std::log(num4 * 0.1f) + 1.0f) * 10.0f;
        auto num5 = 1.0f - (float)std::pow(age, 30.0f) * 0.5f;
        type = EStarType::GiantStar;
        mass = num5 * mass;
        radius = num4 * num2;
/*
        acdiskRadius = 0.0f;
*/
        temperature = num5 * temperature;
        luminosity = 1.6f * luminosity;
        habitableRadius = 9.0f * habitableRadius;
        lightBalanceRadius = 3.0f * habitableRadius;
        orbitScaler = 3.3f * orbitScaler;
    }
}

void Star::createStarPlanets() {
    util::DotNet35Random dotNet35Random(seed);
    dotNet35Random.next();
    dotNet35Random.next();
    dotNet35Random.next();
    util::DotNet35Random dotNet35Random2(dotNet35Random.next());
    auto num = dotNet35Random2.nextDouble();
    auto num2 = dotNet35Random2.nextDouble();
    auto num3 = dotNet35Random2.nextDouble();
    auto num4 = dotNet35Random2.nextDouble();
    auto num5 = dotNet35Random2.nextDouble();
    auto num6 = dotNet35Random2.nextDouble() * 0.2 + 0.9;
    auto num7 = dotNet35Random2.nextDouble() * 0.2 + 0.9;
    int planetCount;
    if (type == EStarType::BlackHole) {
        planetCount = 1;
        planets.resize(1);
        auto infoSeed = dotNet35Random2.next();
        auto genSeed = dotNet35Random2.next();
        planets[0] = Planet::create(this, 0, 0, 3, 1, false, infoSeed, genSeed);
    } else if (type == EStarType::NeutronStar) {
        planetCount = 1;
        planets.resize(1);
        auto infoSeed2 = dotNet35Random2.next();
        auto genSeed2 = dotNet35Random2.next();
        planets[0] = Planet::create(this, 0, 0, 3, 1, false, infoSeed2, genSeed2);
    } else if (type == EStarType::WhiteDwarf) {
        if (num < 0.699999988079071) {
            planetCount = 1;
            planets.resize(1);
            auto infoSeed3 = dotNet35Random2.next();
            auto genSeed3 = dotNet35Random2.next();
            planets[0] = Planet::create(this, 0, 0, 3, 1, false, infoSeed3, genSeed3);
        } else {
            planetCount = 2;
            planets.resize(2);
            auto num8 = 0;
            auto num9 = 0;
            if (num2 < 0.30000001192092896) {
                num8 = dotNet35Random2.next();
                num9 = dotNet35Random2.next();
                planets[0] = Planet::create(this, 0, 0, 3, 1,
                                            false, num8, num9);
                num8 = dotNet35Random2.next();
                num9 = dotNet35Random2.next();
                planets[1] = Planet::create(this, 1, 0, 4, 2,
                                            false, num8, num9);
            } else {
                num8 = dotNet35Random2.next();
                num9 = dotNet35Random2.next();
                planets[0] = Planet::create(this, 0, 0, 4, 1, true,
                                            num8, num9);
                num8 = dotNet35Random2.next();
                num9 = dotNet35Random2.next();
                planets[1] = Planet::create(this, 1, 1, 1, 1,
                                            false, num8, num9);
            }
        }
    } else if (type == EStarType::GiantStar) {
        if (num < 0.30000001192092896) {
            planetCount = 1;
            planets.resize(1);
            auto infoSeed4 = dotNet35Random2.next();
            auto genSeed4 = dotNet35Random2.next();
            planets[0] = Planet::create(this, 0, 0,
                                        num3 > 0.5 ? 3 : 2, 1, false, infoSeed4, genSeed4);
        } else if (num < 0.800000011920929) {
            planetCount = 2;
            planets.resize(2);
            auto num10 = 0;
            auto num11 = 0;
            if (num2 < 0.25) {
                num10 = dotNet35Random2.next();
                num11 = dotNet35Random2.next();
                planets[0] = Planet::create(this, 0, 0,
                                            num3 > 0.5 ? 3 : 2, 1, false, num10, num11);
                num10 = dotNet35Random2.next();
                num11 = dotNet35Random2.next();
                planets[1] = Planet::create(this, 1, 0,
                                            num3 > 0.5 ? 4 : 3, 2, false, num10, num11);
            } else {
                num10 = dotNet35Random2.next();
                num11 = dotNet35Random2.next();
                planets[0] = Planet::create(this, 0, 0, 3, 1, true,
                                            num10, num11);
                num10 = dotNet35Random2.next();
                num11 = dotNet35Random2.next();
                planets[1] = Planet::create(this, 1, 1, 1, 1,
                                            false, num10, num11);
            }
        } else {
            planetCount = 3;
            planets.resize(3);
            auto num12 = 0;
            auto num13 = 0;
            if (num2 < 0.15000000596046448) {
                num12 = dotNet35Random2.next();
                num13 = dotNet35Random2.next();
                planets[0] = Planet::create(this, 0, 0,
                                            num3 > 0.5 ? 3 : 2, 1, false, num12, num13);
                num12 = dotNet35Random2.next();
                num13 = dotNet35Random2.next();
                planets[1] = Planet::create(this, 1, 0,
                                            num3 > 0.5 ? 4 : 3, 2, false, num12, num13);
                num12 = dotNet35Random2.next();
                num13 = dotNet35Random2.next();
                planets[2] = Planet::create(this, 2, 0,
                                            num3 > 0.5 ? 5 : 4, 3, false, num12, num13);
            } else if (num2 < 0.75) {
                num12 = dotNet35Random2.next();
                num13 = dotNet35Random2.next();
                planets[0] = Planet::create(this, 0, 0,
                                            num3 > 0.5 ? 3 : 2, 1, false, num12, num13);
                num12 = dotNet35Random2.next();
                num13 = dotNet35Random2.next();
                planets[1] = Planet::create(this, 1, 0, 4, 2, true,
                                            num12, num13);
                num12 = dotNet35Random2.next();
                num13 = dotNet35Random2.next();
                planets[2] = Planet::create(this, 2, 2, 1, 1,
                                            false, num12, num13);
            } else {
                num12 = dotNet35Random2.next();
                num13 = dotNet35Random2.next();
                planets[0] = Planet::create(this, 0, 0,
                                            num3 > 0.5 ? 4 : 3, 1, true, num12, num13);
                num12 = dotNet35Random2.next();
                num13 = dotNet35Random2.next();
                planets[1] = Planet::create(this, 1, 1, 1, 1,
                                            false, num12, num13);
                num12 = dotNet35Random2.next();
                num13 = dotNet35Random2.next();
                planets[2] = Planet::create(this, 2, 1, 2, 2,
                                            false, num12, num13);
            }
        }
    } else {
        double pGas[10];
        if (index == 0) {
            planetCount = 4;
            pGas[0] = 0.0;
            pGas[1] = 0.0;
            pGas[2] = 0.0;
        } else if (spectr == ESpectrType::M) {
            if (num < 0.1)
                planetCount = 1;
            else if (num < 0.3)
                planetCount = 2;
            else if (num < 0.8)
                planetCount = 3;
            else
                planetCount = 4;
            if (planetCount <= 3) {
                pGas[0] = 0.2;
                pGas[1] = 0.2;
            } else {
                pGas[0] = 0.0;
                pGas[1] = 0.2;
                pGas[2] = 0.3;
            }
        } else if (spectr == ESpectrType::K) {
            if (num < 0.1)
                planetCount = 1;
            else if (num < 0.2)
                planetCount = 2;
            else if (num < 0.7)
                planetCount = 3;
            else if (num < 0.95)
                planetCount = 4;
            else
                planetCount = 5;
            if (planetCount <= 3) {
                pGas[0] = 0.18;
                pGas[1] = 0.18;
            } else {
                pGas[0] = 0.0;
                pGas[1] = 0.18;
                pGas[2] = 0.28;
                pGas[3] = 0.28;
            }
        } else if (spectr == ESpectrType::G) {
            if (num < 0.4)
                planetCount = 3;
            else if (num < 0.9)
                planetCount = 4;
            else
                planetCount = 5;
            if (planetCount <= 3) {
                pGas[0] = 0.18;
                pGas[1] = 0.18;
            } else {
                pGas[0] = 0.0;
                pGas[1] = 0.2;
                pGas[2] = 0.3;
                pGas[3] = 0.3;
            }
        } else if (spectr == ESpectrType::F) {
            if (num < 0.35)
                planetCount = 3;
            else if (num < 0.8)
                planetCount = 4;
            else
                planetCount = 5;
            if (planetCount <= 3) {
                pGas[0] = 0.2;
                pGas[1] = 0.2;
            } else {
                pGas[0] = 0.0;
                pGas[1] = 0.22;
                pGas[2] = 0.31;
                pGas[3] = 0.31;
            }
        } else if (spectr == ESpectrType::A) {
            if (num < 0.3)
                planetCount = 3;
            else if (num < 0.75)
                planetCount = 4;
            else
                planetCount = 5;
            if (planetCount <= 3) {
                pGas[0] = 0.2;
                pGas[1] = 0.2;
            } else {
                pGas[0] = 0.1;
                pGas[1] = 0.28;
                pGas[2] = 0.3;
                pGas[3] = 0.35;
            }
        } else if (spectr == ESpectrType::B) {
            if (num < 0.3)
                planetCount = 4;
            else if (num < 0.75)
                planetCount = 5;
            else
                planetCount = 6;
            if (planetCount <= 3) {
                pGas[0] = 0.2;
                pGas[1] = 0.2;
            } else {
                pGas[0] = 0.1;
                pGas[1] = 0.22;
                pGas[2] = 0.28;
                pGas[3] = 0.35;
                pGas[4] = 0.35;
            }
        } else if (spectr == ESpectrType::O) {
            planetCount = num < 0.5 ? 5 : 6;
            pGas[0] = 0.1;
            pGas[1] = 0.2;
            pGas[2] = 0.25;
            pGas[3] = 0.3;
            pGas[4] = 0.32;
            pGas[5] = 0.35;
        } else {
            planetCount = 1;
        }

        planets.resize(planetCount);
        auto num14 = 0;
        auto num15 = 0;
        auto num16 = 0;
        auto num17 = 1;
        for (auto i = 0; i < planetCount; i++) {
            auto infoSeed5 = dotNet35Random2.next();
            auto genSeed5 = dotNet35Random2.next();
            auto num18 = dotNet35Random2.nextDouble();
            auto num19 = dotNet35Random2.nextDouble();
            auto flag = false;
            if (num16 == 0) {
                num14++;
                if (i < planetCount - 1 && num18 < pGas[i]) {
                    flag = true;
                    if (num17 < 3) num17 = 3;
                }

                while (true) {
                    if (index == 0 && num17 == 3) {
                        flag = true;
                        break;
                    }

                    auto num20 = planetCount - i;
                    auto num21 = 9 - num17;
                    if (num21 <= num20) break;
                    auto a = num20 / (float)num21;
                    a = num17 <= 3 ? util::lerp(a, 1.0f, 0.15f) + 0.01f : util::lerp(a, 1.0f, 0.45f) + 0.01f;
                    if (dotNet35Random2.nextDouble() < a) break;
                    num17++;
                }
            } else {
                num15++;
            }

            planets[i] = Planet::create(this,
                                        i,
                                        num16,
                                        num16 == 0 ? num17 : num15,
                                        num16 == 0 ? num14 : num15,
                                        flag,
                                        infoSeed5,
                                        genSeed5);
            num17++;
            if (flag) {
                num16 = num14;
                num15 = 0;
            }

            if (num15 >= 1 && num19 < 0.8) {
                num16 = 0;
                num15 = 0;
            }
        }
    }

    auto num22 = 0;
    auto num23 = 0;
    auto num24 = 0;
    auto num25 = 0;
    for (auto j = 0; j < planetCount; j++)
        if (planets[j]->type == EPlanetType::Gas) {
            num22 = planets[j]->orbitIndex;
            break;
        }

    for (auto k = 0; k < planetCount; k++)
        if (planets[k]->orbitAround == 0)
            num23 = planets[k]->orbitIndex;
    if (num22 > 0) {
        auto num26 = num22 - 1;
        auto flag2 = true;
        for (auto l = 0; l < planetCount; l++)
            if (planets[l]->orbitAround == 0 && planets[l]->orbitIndex == num22 - 1) {
                flag2 = false;
                break;
            }

        if (flag2 && num4 < 0.2 + num26 * 0.2) num24 = num26;
    }

    num25 = num5 < 0.2 ? num23 + 3 : num5 < 0.4 ? num23 + 2 : num5 < 0.8 ? num23 + 1 : 0;
    if (num25 != 0 && num25 < 5) num25 = 5;
/*
    asterBelt1OrbitIndex = num24;
    asterBelt2OrbitIndex = num25;
*/
}

const char *Star::typeName() const {
    switch (type) {
        case EStarType::BlackHole:
            return "Black Hole";
        case EStarType::NeutronStar:
            return "Neutron Star";
        case EStarType::WhiteDwarf:
            return "White Dwarf";
        case EStarType::GiantStar:
            switch (spectr) {
                case ESpectrType::M:
                case ESpectrType::K:
                    return "Red Giant";
                case ESpectrType::G:
                case ESpectrType::F:
                    return "Yellow Giant";
                case ESpectrType::A:
                    return "White Giant";
                case ESpectrType::B:
                case ESpectrType::O:
                    return "Blue Giant";
            }
            break;
        case EStarType::MainSeqStar:
            switch (spectr) {
                case ESpectrType::M:
                    return "M";
                case ESpectrType::K:
                    return "K";
                case ESpectrType::G:
                    return "G";
                case ESpectrType::F:
                    return "F";
                case ESpectrType::A:
                    return "A";
                case ESpectrType::B:
                    return "B";
                case ESpectrType::O:
                    return "O";
            }
            break;
    }
    return "";
}

}
