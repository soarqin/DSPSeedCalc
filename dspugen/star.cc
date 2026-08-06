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
#include "settings.hh"
#include "util/dotnet35random.hh"
#include "util/maths.hh"
#include "util/mempool.hh"

#include <algorithm>
#include <cmath>

namespace dspugen {

// Mono evaluates a managed float expression in double and narrows once, where
// the reference stores into a float32 location.  dbl() widens an operand,
// flt() is that single narrowing.
static constexpr double dbl(float value) { return static_cast<double>(value); }
static constexpr float flt(double value) { return static_cast<float>(value); }

// StarGen.RandNormal returns float32:
//   return averageValue + standardDeviation * (float)(Math.Sqrt(..) * Math.Sin(..));
// so only the inner cast and the return narrow -- the multiply and the add stay
// in double.  This is the origin of the 1 ULP error in starData.mass.
static float randNormal(float averageValue, float standardDeviation, double r1, double r2) {
    return flt(dbl(averageValue) +
               dbl(standardDeviation) *
                   dbl(flt(std::sqrt(-2.0 * std::log(1.0 - r1)) *
                           std::sin(M_PI * 2.0 * r2))));
}

static thread_local util::MemPool<Star> *spool;

void Star::initThread() {
    spool = new util::MemPool<Star>();
}

void Star::releaseThread() {
    delete spool;
}

Star::~Star() {
    for (auto *p: planets) {
        p->release();
    }
}

void Star::reset() {
    for (auto *planet: planets) {
        if (planet) planet->release();
    }
    planets.clear();
    name.clear();
    galaxy = nullptr;
    index = 0;
    level = 0.0f;
    id = 1;
    seed = 0;
    type = EStarType::MainSeqStar;
    spectr = ESpectrType::M;
    mass = 1.0f;
    lifetime = 50.0f;
    age = 0.0f;
    temperature = 8500.0f;
    luminosity = 1.0f;
    radius = 1.0f;
    habitableRadius = 1.0f;
    lightBalanceRadius = 1.0f;
    orbitScaler = 1.0f;
    dysonRadius = 10.0f;
    position = {};
    color = 0.0f;
    classFactor = 0.0f;
    resourceCoef = 0.0f;
    habitableRadiusSqrt = 0.0f;
    planetGenSeed = 0;
}

void Star::release() {
    spool->release(this);
}

Star *Star::createStar(Galaxy *galaxy,
                       const VectorLF3 &pos,
                       int id,
                       int seed,
                       EStarType needtype,
                       ESpectrType needSpectr) {
    static const auto log10_26 = std::log10(2.6);
    static const auto log10_5 = std::log10(5.0);

    auto *star = spool->alloc();
    star->galaxy = galaxy;
    star->index = id - 1;
    if (galaxy->starCount > 1)
        star->level = float(star->index) / float(galaxy->starCount - 1);
    else
        star->level = 0.0f;
    star->id = id;
    star->seed = seed;
    util::DotNet35Random dotNet35Random(seed);
    auto seed2 = dotNet35Random.next();
    auto seed3 = dotNet35Random.next();
    dotNet35Random.next();
    star->planetGenSeed = dotNet35Random.next();
    star->position = pos;
    auto distance = float(pos.magnitude());
    auto distanceFactor = distance / 32.0f;
    if (distanceFactor > 1.0f) {
        distanceFactor = util::mathfLog(distanceFactor) + 1.0f;
        distanceFactor = util::mathfLog(distanceFactor) + 1.0f;
        distanceFactor = util::mathfLog(distanceFactor) + 1.0f;
        distanceFactor = util::mathfLog(distanceFactor) + 1.0f;
        distanceFactor = util::mathfLog(distanceFactor) + 1.0f;
    }
    star->resourceCoef = util::mathfPow(7.0f, distanceFactor) * 0.6f;

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
    num10 = flt(dbl(std::clamp(num10, -2.4f, 4.65f)) + dbl(float(num5)) + 1.0);
    switch (needtype) {
        case EStarType::BlackHole:
            star->mass = flt(18.0 + dbl(float(num2 * num3)) * 30.0);
            break;
        case EStarType::NeutronStar:
            star->mass = flt(7.0 + dbl(float(num2)) * 11.0);
            break;
        case EStarType::WhiteDwarf:
            star->mass = flt(1.0 + dbl(float(num3)) * 5.0);
            break;
        default:
            star->mass = util::mathfPow(2.0f, num10);
            break;
    }

    // The giant branch used to overwrite this assignment with the same expression and a
    // 0.58 factor, so the default result was dead work for giants: one pow and two log10
    // per giant star. Selecting the factor up front computes it once. For mass >= 2.0f the
    // divisor is exactly 5.0, so the cached log10_5 is the same bit pattern std::log10(d)
    // would return.
    auto d = star->mass < 2.0f ? (2.0 + 0.4 * (1.0 - star->mass)) : 5.0;
    auto logD = star->mass < 2.0f ? std::log10(d) : log10_5;
    auto massFactor = needtype == EStarType::GiantStar ? 0.58 : 0.5;
    star->lifetime = 10000.0 * std::pow(0.1, std::log10(star->mass * massFactor) / logD + 1.0) * num6;
    switch (needtype) {
        case EStarType::GiantStar:
            star->age = flt(dbl(float(num4)) * dbl(0.04f) + dbl(0.96f));
            break;
        case EStarType::WhiteDwarf:
        case EStarType::NeutronStar:
        case EStarType::BlackHole:
            star->age = flt(dbl(float(num4)) * dbl(0.4f) + 1.0);
            switch (needtype) {
                case EStarType::WhiteDwarf:
                    star->lifetime += 10000.0f;
                    break;
                case EStarType::NeutronStar:
                    star->lifetime += 1000.0f;
                    break;
                default:
                    break;
            }

            break;
        default:
            if (star->mass < 0.5)
                star->age = flt(dbl(float(num4)) * dbl(0.12f) + dbl(0.02f));
            else if (star->mass < 0.8)
                star->age = flt(dbl(float(num4)) * dbl(0.4f) + dbl(0.1f));
            else
                star->age = flt(dbl(float(num4)) * dbl(0.7f) + dbl(0.2f));
            break;
    }

    auto num11 = star->lifetime * star->age;
    if (num11 > 5000.0f) num11 = flt((dbl(util::mathfLog(num11 / 5000.0f)) + 1.0) * 5000.0);
    if (num11 > 8000.0f)
        num11 = flt((dbl(util::mathfLog(flt(dbl(util::mathfLog(flt(dbl(util::mathfLog(num11 / 8000.0f)) + 1.0))) + 1.0))) + 1.0) * 8000.0);
    star->lifetime = num11 / star->age;
    auto num12 = flt((1.0 - dbl(util::mathfPow(util::clamp01(star->age), 20.0f)) * 0.5) * dbl(star->mass));
    star->temperature = flt(std::pow(dbl(num12), 0.56 + 0.14 / (std::log10(dbl(num12) + 4.0) / log10_5)) * 4450.0 + 1300.0);
    auto num13 = std::log10((star->temperature - 1300.0) / 4500.0) / log10_26 - 0.5;
    if (num13 < 0.0) num13 *= 4.0;
    if (num13 > 2.0)
        num13 = 2.0;
    else if (num13 < -4.0) num13 = -4.0;
    star->spectr = (ESpectrType)util::mathfRoundToInt(float(num13) + 4.0f);
    // 0.2f widens to 0.20000000298023224, not to 0.2.  Spelling it as the
    // double literal 0.2 mismatches 17% of stars.
    star->color = util::clamp01(flt((dbl(float(num13)) + 3.5) * dbl(0.2f)));
    star->classFactor = float(num13);
    star->luminosity = util::mathfPow(num12, 0.7f);
    star->radius = std::pow(star->mass, 0.4) * num8;
/*
    star->acdiskRadius = 0.0f;
*/
    auto p = float(num13) + 2.0f;
    auto lightBalanceRadius = util::mathfPow(1.7f, p);
    star->habitableRadius = flt(dbl(lightBalanceRadius) + 0.25 * dbl(std::min(1.0f, star->orbitScaler)));
    star->lightBalanceRadius = lightBalanceRadius;
    star->orbitScaler = util::mathfPow(1.35f, p);
    if (star->orbitScaler < 1.0f) star->orbitScaler = util::lerp(star->orbitScaler, 1.0f, 0.6f);
    star->setStarAge(rn, rt);
    star->dysonRadius = star->orbitScaler * 0.28f;
    // physicsRadius returns float32, but the * 1.5f product is never stored to a
    // float32 location, so the IL keeps it in double for both the comparison and
    // the divide. Narrowing it, or pre-dividing it into a float bound to compare
    // against, flips 20 stars per 101 seeds.
    auto physicsScaled = dbl(star->physicsRadius()) * 1.5;
    if (dbl(star->dysonRadius) * 40000.0 < physicsScaled)
        star->dysonRadius = flt(physicsScaled / 40000.0);
/*
    star->uPosition = star->position * 2400000.0;
*/
    if (settings.genName) {
        star->name = NameGen::randomStarName(seed2, star, galaxy);
    }
    return star;
}

Star *Star::createBirthStar(Galaxy *galaxy, int seed) {
    static const auto log10_26 = std::log10(2.6);
    static const auto log10_5 = std::log10(5.0);

    auto star = spool->alloc();
    star->galaxy = galaxy;
    star->index = 0;
    star->level = 0.0f;
    star->id = 1;
    star->seed = seed;
    star->position = {};
    star->resourceCoef = 0.6f;
    util::DotNet35Random dotNet35Random(seed);
    auto seed2 = dotNet35Random.next();
    auto seed3 = dotNet35Random.next();
    dotNet35Random.next();
    star->planetGenSeed = dotNet35Random.next();
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
    star->mass = util::mathfPow(2.0f, value);
    auto num4 = 2.0 + 0.4 * (1.0 - star->mass);
    star->lifetime = 10000.0 * std::pow(0.1, std::log10(star->mass * 0.5) / std::log10(num4) + 1.0) * num2;
    star->age = float(num * 0.4 + 0.3);
    auto num5 = flt((1.0 - dbl(util::mathfPow(util::clamp01(star->age), 20.0f)) * 0.5) * dbl(star->mass));
    star->temperature = flt(std::pow(dbl(num5), 0.56 + 0.14 / (std::log10(dbl(num5) + 4.0) / log10_5)) * 4450.0 + 1300.0);
    auto num6 = std::log10((star->temperature - 1300.0) / 4500.0) / log10_26 - 0.5;
    if (num6 < 0.0) num6 *= 4.0;
    if (num6 > 2.0)
        num6 = 2.0;
    else if (num6 < -4.0) num6 = -4.0;
    star->spectr = (ESpectrType)util::mathfRoundToInt(float(num6) + 4.0f);
    star->color = util::clamp01(flt((dbl(float(num6)) + 3.5) * dbl(0.2f)));
    star->classFactor = float(num6);
    star->luminosity = util::mathfPow(num5, 0.7f);
    star->radius = std::pow(star->mass, 0.4) * num3;
/*
    star->acdiskRadius = 0.0f;
*/
    auto p = float(num6) + 2.0f;
    auto lightBalanceRadius = util::mathfPow(1.7f, p);
    star->habitableRadius = flt(dbl(lightBalanceRadius) + dbl(0.2f) * dbl(std::min(1.0f, star->orbitScaler)));
    star->lightBalanceRadius = lightBalanceRadius;
    star->orbitScaler = util::mathfPow(1.35f, p);
    if (star->orbitScaler < 1.0f) star->orbitScaler = util::lerp(star->orbitScaler, 1.0f, 0.6f);
    star->setStarAge(rn, rt);
    star->dysonRadius = star->orbitScaler * 0.28f;
    auto physicsScaled = dbl(star->physicsRadius()) * 1.5;
    if (dbl(star->dysonRadius) * 40000.0 < physicsScaled)
        star->dysonRadius = flt(physicsScaled / 40000.0);
    if (settings.genName) {
        star->name = NameGen::randomStarName(seed2, star, galaxy);
    }
    return star;
}

void Star::setStarAge(double rn, double rt) {
    auto num = float(rn * 0.1 + 0.95);
    auto num2 = float(rt * 0.4 + 0.8);
    auto num3 = float(rt * 9.0 + 1.0);
    if (age >= 1.0f) {
        if (mass >= 18.0f) {
            type = EStarType::BlackHole;
            spectr = ESpectrType::X;
            mass = flt(dbl(mass) * (2.5 * dbl(num2)));
            radius *= 1.0f;
/*
            acdiskRadius = radius * 5.0f;
*/
            temperature = 0.0f;
            luminosity = flt(dbl(luminosity) * (dbl(0.001f) * dbl(num)));
            habitableRadius = 0.0f;
            lightBalanceRadius = flt(dbl(lightBalanceRadius) * (dbl(0.4f) * dbl(num)));
            color = 1.0f;
        } else if (mass >= 7.0f) {
            type = EStarType::NeutronStar;
            spectr = ESpectrType::X;
            mass = flt(dbl(mass) * (dbl(0.2f) * dbl(num)));
            radius *= 0.15f;
/*
            acdiskRadius = radius * 9.0f;
*/
            temperature = num3 * 1E+07f;
            luminosity = flt(dbl(luminosity) * (dbl(0.1f) * dbl(num)));
            habitableRadius = 0.0f;
            lightBalanceRadius = flt(dbl(lightBalanceRadius) * (3.0 * dbl(num)));
            orbitScaler = flt(dbl(orbitScaler) * (1.5 * dbl(num)));
            color = 1.0f;
        } else {
            type = EStarType::WhiteDwarf;
            spectr = ESpectrType::X;
            mass = flt(dbl(mass) * (dbl(0.2f) * dbl(num)));
            radius *= 0.2f;
/*
            acdiskRadius = 0.0f;
*/
            temperature = num2 * 150000.0f;
            luminosity = flt(dbl(luminosity) * (dbl(0.04f) * dbl(num2)));
            habitableRadius = flt(dbl(habitableRadius) * (dbl(0.15f) * dbl(num2)));
            lightBalanceRadius = flt(dbl(lightBalanceRadius) * (dbl(0.2f) * dbl(num)));
            color = 0.7f;
        }
    } else if (age >= 0.96f) {
        auto num4 = float(std::pow(5.0, std::abs(std::log10(double(mass)) - 0.7)) * 5.0);
        if (num4 > 10.0f) num4 = flt((dbl(util::mathfLog(num4 * 0.1f)) + 1.0) * 10.0);
        auto num5 = flt(1.0 - dbl(util::mathfPow(age, 30.0f)) * 0.5);
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
    if (habitableRadiusSqrt == 0.0f) habitableRadiusSqrt = util::mathfSqrt(habitableRadius);
    // planetGenSeed is the fourth value of the generator seeded with `seed`, which star
    // creation already walked; see Star::planetGenSeed.
    util::DotNet35Random dotNet35Random2(planetGenSeed);
    auto num = dotNet35Random2.nextDouble();
    auto num2 = dotNet35Random2.nextDouble();
    auto num3 = dotNet35Random2.nextDouble();
    auto num4 = dotNet35Random2.nextDouble();
    auto num5 = dotNet35Random2.nextDouble();
    auto num6 = dotNet35Random2.nextDouble() * 0.2 + 0.9;
    auto num7 = dotNet35Random2.nextDouble() * 0.2 + 0.9;
    // int planetCount;
    if (type == EStarType::BlackHole) {
        // planetCount = 1;
        planets.resize(1);
        auto infoSeed = dotNet35Random2.next();
        auto genSeed = dotNet35Random2.next();
        planets[0] = Planet::create(this, 0, 0, 3, 1, false, infoSeed, genSeed);
    } else if (type == EStarType::NeutronStar) {
        // planetCount = 1;
        planets.resize(1);
        auto infoSeed2 = dotNet35Random2.next();
        auto genSeed2 = dotNet35Random2.next();
        planets[0] = Planet::create(this, 0, 0, 3, 1, false, infoSeed2, genSeed2);
    } else if (type == EStarType::WhiteDwarf) {
        if (num < 0.699999988079071) {
            // planetCount = 1;
            planets.resize(1);
            auto infoSeed3 = dotNet35Random2.next();
            auto genSeed3 = dotNet35Random2.next();
            planets[0] = Planet::create(this, 0, 0, 3, 1, false, infoSeed3, genSeed3);
        } else {
            // planetCount = 2;
            planets.resize(2);
            int num8;
            int num9;
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
            // planetCount = 1;
            planets.resize(1);
            auto infoSeed4 = dotNet35Random2.next();
            auto genSeed4 = dotNet35Random2.next();
            planets[0] = Planet::create(this, 0, 0,
                                        num3 > 0.5 ? 3 : 2, 1, false, infoSeed4, genSeed4);
        } else if (num < 0.800000011920929) {
            // planetCount = 2;
            planets.resize(2);
            if (num2 < 0.25) {
                auto num10 = dotNet35Random2.next();
                auto num11 = dotNet35Random2.next();
                planets[0] = Planet::create(this, 0, 0,
                                            num3 > 0.5 ? 3 : 2, 1, false, num10, num11);
                num10 = dotNet35Random2.next();
                num11 = dotNet35Random2.next();
                planets[1] = Planet::create(this, 1, 0,
                                            num3 > 0.5 ? 4 : 3, 2, false, num10, num11);
            } else {
                auto num10 = dotNet35Random2.next();
                auto num11 = dotNet35Random2.next();
                planets[0] = Planet::create(this, 0, 0, 3, 1, true,
                                            num10, num11);
                num10 = dotNet35Random2.next();
                num11 = dotNet35Random2.next();
                planets[1] = Planet::create(this, 1, 1, 1, 1,
                                            false, num10, num11);
            }
        } else {
            // planetCount = 3;
            planets.resize(3);
            if (num2 < 0.15000000596046448) {
                auto num12 = dotNet35Random2.next();
                auto num13 = dotNet35Random2.next();
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
                auto num12 = dotNet35Random2.next();
                auto num13 = dotNet35Random2.next();
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
                auto num12 = dotNet35Random2.next();
                auto num13 = dotNet35Random2.next();
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
        int planetCount;
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
            /*if (planetCount <= 3) {
                pGas[0] = 0.2;
                pGas[1] = 0.2;
            } else*/ {
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
                    auto a = num20 / float(num21);
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

/*
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
*/
}

float Star::updateResourceCoef() {
    if (resourceCoef == 0.0f) {
        auto distance = float(position.magnitude());
        auto distanceFactor = distance / 32.0f;
        if (distanceFactor > 1.0f) {
            distanceFactor = util::mathfLog(distanceFactor) + 1.0f;
            distanceFactor = util::mathfLog(distanceFactor) + 1.0f;
            distanceFactor = util::mathfLog(distanceFactor) + 1.0f;
            distanceFactor = util::mathfLog(distanceFactor) + 1.0f;
            distanceFactor = util::mathfLog(distanceFactor) + 1.0f;
        }
        resourceCoef = util::mathfPow(7.0f, distanceFactor) * 0.6f;
    }
    return resourceCoef;
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
                default:
                    break;
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
                default:
                    break;
            }
            break;
    }
    return "";
}

}
