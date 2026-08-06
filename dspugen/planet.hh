/*
 * Copyright (c) 2022 Soar Qin<soarchin@gmail.com>
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#pragma once

#include "vein/vein_amounts.hh"

#include <vector>
#include <memory>

namespace dspugen {

enum class EVeinType {
    None,
    Iron,
    Copper,
    Silicium,
    Titanium,
    Stone,
    Coal,
    Oil,
    Fireice,
    Diamond,
    Fractal,
    Crysrub,
    Grat,
    Bamboo,
    Mag,
    Max
};

enum class EPlanetType {
    None,
    Vocano,
    Ocean,
    Desert,
    Ice,
    Gas
};

enum class EPlanetTheme {
    None,
    Mediterranean,
    GasGiant1,
    GasGiant2,
    IceGiant1,
    IceGiant2,
    AridDesert,
    AshenGelisol,
    OceanicJungle,
    Lava,
    IceFieldGelisol,
    BarrenDesert,
    Gobi,
    VolcanicAsh,
    RedStone,
    Prairie,
    Waterworld,
    RockySaltLake,
    SakuraOcean,
    HurricaneStoneForest,
    ScarletIceLake,
    GasGiantHigh,
    Savanna,
    CrystalDesert,
    FrozenTundra,
    PandoraSwamp,
    Max,
};

namespace EPlanetSingularity {

enum : int {
    None = 0,
    TidalLocked = 1,
    TidalLocked2 = 2,
    TidalLocked4 = 4,
    LaySide = 8,
    ClockwiseRotate = 0x10,
    MultipleSatellites = 0x20
};

}

class Star;
class Galaxy;

class Planet {
public:
    static void initThread();
    static void releaseThread();

    void release();
    void reset();

    int id = 0;
    int index = 0;
    int number = 0;
    int seed = 0;

    Galaxy *galaxy = nullptr;
    Star *star = nullptr;
    EPlanetType type = EPlanetType::None;
    int singularity = 0;
    int theme = 0;
    int algoId = 0;

    int orbitAround = 0;
    Planet *orbitAroundPlanet = nullptr;
    int orbitIndex = 0;
    float orbitRadius = 1.0f;
    float radius = 200.0f;
    float scale = 1.0f;
    float habitableBias = 0.0f;
    float sunDistance = 0.0f;
    float temperatureBias = 0.0f;
    float luminosity = 0.0f;

    std::vector<int> gasItems;
    std::vector<float> gasSpeeds;
    int veinSpot[15] = {};
    int themeSeed = 0;

    // Appended parity fields used only by the opt-in terrain/vein path.  Keep
    // them after the historical public members because plugins inline this
    // header and read the old layout directly.
    float orbitInclination = 0.0f;
    double orbitalPeriod = 3600.0;
    float orbitLongitude = 0.0f;
    float orbitPhase = 0.0f;
    float obliquity = 0.0f;
    double rotationPeriod = 480.0;
    float rotationPhase = 0.0f;
    double modX = 0.0;
    double modY = 0.0;
    float waterHeight = 0.0f;
    int waterItemId = 0;

    [[nodiscard]] const VeinAmountSummary *veinAmountSummary() const;
    [[nodiscard]] VeinAmountGenerationResult ensureVeinAmounts();
    [[nodiscard]] VeinAmountGenerationResult veinAmountGenerationResult() const {
        return veinAmountResult;
    }

    static Planet *create(Star *star, int index, int orbitAround, int orbitIndex, int number, bool gasGiant, int infoSeed, int genSeed);

    [[nodiscard]] inline float realRadius() const { return radius * scale; }

    void generateGas();

private:
    void setPlanetTheme(double rand1, double rand2, double rand3, double rand4, int thmSeed);
    void generateVeins();

    std::unique_ptr<VeinAmountSummary> veinAmountData;
    VeinAmountGenerationResult veinAmountResult =
        VeinAmountGenerationResult::NotGenerated;
};

}
