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
#include <string>
#include <vector>
#include <memory>

namespace dspugen {

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
    ~Star();
    void release();

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
    float color;
/*
    float classFactor;
    float resourceCoef = 0.6f;
    float acdiskRadius;
    VectorLF3 uPosition;
    float asterBelt1OrbitIndex;
    float asterBelt2OrbitIndex;
*/
    std::vector<Planet*> planets;
    std::string name;

    static Star *createStar(Galaxy *galaxy, VectorLF3 pos, int id, int seed, EStarType needtype,
                                ESpectrType needSpectr = ESpectrType::X, bool genName = false);
    static Star *createBirthStar(Galaxy *galaxy, int seed, bool genName = false);
    void createStarPlanets();
    [[nodiscard]] const char *typeName() const;
    [[nodiscard]] inline float physicsRadius() const { return radius * kPhysicsRadiusRatio; }

private:
    void setStarAge(double rn, double rt);
};

}
