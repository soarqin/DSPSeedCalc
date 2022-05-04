/*
 * Copyright (c) 2022 Soar Qin<soarchin@gmail.com>
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#pragma once

#include "planet.hh"
#include "vectors.hh"
#include <vector>
#include <memory>

class Galaxy;

enum class EStarType {
    MainSeqStar,
    GiantStar,
    WhiteDwarf,
    NeutronStar,
    BlackHole
};

enum class ESpectrType {
    M,
    K,
    G,
    F,
    A,
    B,
    O,
    X
};

class Star {
public:
    using Ptr = std::unique_ptr<Star>;
    static constexpr float kPhysicsRadiusRatio = 1200.0f;

    Galaxy *galaxy = nullptr;
    int index = 0;
    float level = 0;
    int id = 1;
    int seed = 0;

    EStarType type = EStarType::MainSeqStar;
    ESpectrType spectr = ESpectrType::M;

    float mass = 1.0f;
    float lifetime = 50.0f;
    float age;
    float temperature = 8500.0f;
    float luminosity = 1.0f;
    float radius = 1.0f;
    float habitableRadius = 1.0f;
    float lightBalanceRadius = 1.0f;
    float orbitScaler = 1.0f;
    float dysonRadius = 10.0f;

    VectorLF3 position;
/*
    float color;
    float classFactor;
    float resourceCoef = 0.6f;
    float acdiskRadius;
    VectorLF3 uPosition;
    float asterBelt1OrbitIndex;
    float asterBelt2OrbitIndex;
*/
    std::vector<Planet::Ptr> planets;

    static Star::Ptr createStar(Galaxy *galaxy, VectorLF3 pos, int id, int seed, EStarType needtype,
                                ESpectrType needSpectr = ESpectrType::X);
    static Star::Ptr createBirthStar(Galaxy *galaxy, int seed);
    void createStarPlanets();

    [[nodiscard]] inline float physicsRadius() const { return radius * kPhysicsRadiusRatio; }

private:
    void setStarAge(double rn, double rt);
};
