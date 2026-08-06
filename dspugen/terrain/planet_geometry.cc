#include "planet_geometry.hh"

#include "../planet.hh"
#include "../star.hh"
#include "../util/dotnet35random.hh"
#include "../util/maths.hh"

#include <algorithm>
#include <cmath>

namespace dspugen::terrain {

namespace {

constexpr float Pi = 3.14159274101257324219f;
constexpr Vector3 Up{0.0f, 1.0f, 0.0f};
constexpr Vector3 Down{0.0f, -1.0f, 0.0f};
constexpr Vector3 Forward{0.0f, 0.0f, 1.0f};

Quaternion orbitRotation(const Planet &planet) {
    auto rotation = angleAxis(planet.orbitLongitude, Up) *
                    angleAxis(planet.orbitInclination, Forward);
    if (planet.orbitAroundPlanet != nullptr)
        rotation = orbitRotation(*planet.orbitAroundPlanet) * rotation;
    return rotation;
}

Quaternion systemRotation(const Planet &planet) {
    return orbitRotation(planet) * angleAxis(planet.obliquity, Forward);
}

Vector3 inverseRotateLf(const Quaternion &rotation,
                        double x, double y, double z) {
    x *= 2.0;
    y *= 2.0;
    z *= 2.0;
    const auto num = static_cast<double>(rotation.w * rotation.w) - 0.5;
    const auto num2 = static_cast<double>(rotation.x) * x +
                      static_cast<double>(rotation.y) * y +
                      static_cast<double>(rotation.z) * z;
    return {
        static_cast<float>(x * num -
                           (static_cast<double>(rotation.y) * z -
                            static_cast<double>(rotation.z) * y) *
                               static_cast<double>(rotation.w) +
                           static_cast<double>(rotation.x) * num2),
        static_cast<float>(y * num -
                           (static_cast<double>(rotation.z) * x -
                            static_cast<double>(rotation.x) * z) *
                               static_cast<double>(rotation.w) +
                           static_cast<double>(rotation.y) * num2),
        static_cast<float>(z * num -
                           (static_cast<double>(rotation.x) * y -
                            static_cast<double>(rotation.y) * x) *
                               static_cast<double>(rotation.w) +
                           static_cast<double>(rotation.z) * num2),
    };
}

int determineLongitudeSegmentCount(int latitudeIndex, int segment) {
    const auto angle = static_cast<float>(latitudeIndex) /
                       (static_cast<float>(segment) / 4.0f) * Pi * 0.5f;
    const auto cosine = static_cast<float>(std::cos(static_cast<double>(angle)));
    auto count = static_cast<int>(std::ceil(
        static_cast<double>(std::abs(cosine) * static_cast<float>(segment))));
    if (count >= 500) return ((count + 49) / 100) * 100;
    if (count <= 0) return 1;
    if (count <= 7) return 4;
    if (count <= 15) return 8;
    if (count <= 19) return 16;
    if (count <= 27) return 20;
    if (count <= 39) return 32;
    if (count <= 53) return 40;
    if (count <= 72) return 60;
    if (count <= 90) return 80;
    if (count <= 113) return 100;
    if (count <= 139) return 120;
    if (count <= 176) return 160;
    if (count <= 220) return 200;
    if (count <= 270) return 240;
    if (count <= 350) return 300;
    if (count <= 450) return 400;
    return 500;
}

}

PlanetPose predictPose(const Planet &planet, double time) {
    auto orbitCycle = time / planet.orbitalPeriod +
                      static_cast<double>(planet.orbitPhase) / 360.0;
    orbitCycle -= static_cast<double>(static_cast<int>(orbitCycle + 0.1));
    const auto orbitRadians = orbitCycle * 3.14159265358979323846 * 2.0;

    auto rotationCycle = time / planet.rotationPeriod +
                         static_cast<double>(planet.rotationPhase) / 360.0;
    const auto rotationDegrees =
        (rotationCycle -
         static_cast<double>(static_cast<int>(rotationCycle + 0.1))) *
        360.0;

    const auto orbit = orbitRotation(planet);
    Vector3 local{
        static_cast<float>(std::cos(orbitRadians)) * planet.orbitRadius,
        0.0f,
        static_cast<float>(std::sin(orbitRadians)) * planet.orbitRadius,
    };
    auto position = orbit * local;
    if (planet.orbitAroundPlanet != nullptr) {
        const auto parentPose = predictPose(*planet.orbitAroundPlanet, time);
        position.x += parentPose.position.x;
        position.y += parentPose.position.y;
        position.z += parentPose.position.z;
    }
    return {position,
            systemRotation(planet) *
                angleAxis(static_cast<float>(rotationDegrees), Down)};
}

Vector3 predictLocalStarDirection(const Planet &planet, double time) {
    const auto pose = predictPose(planet, time);
    const auto starX = planet.star->position.x * 2400000.0;
    const auto starY = planet.star->position.y * 2400000.0;
    const auto starZ = planet.star->position.z * 2400000.0;
    auto direction = inverseRotateLf(
        pose.rotation,
        starX - static_cast<double>(pose.position.x) * 40000.0,
        starY - static_cast<double>(pose.position.y) * 40000.0,
        starZ - static_cast<double>(pose.position.z) * 40000.0);
    direction.normalize();
    return direction;
}

BirthPoints generateBirthPoints(const Planet &planet,
                                const PlanetRawData &rawData) {
    util::DotNet35Random seedRandom(planet.seed);
    seedRandom.next();
    seedRandom.next();
    seedRandom.next();
    seedRandom.next();
    return generateBirthPoints(planet, rawData, seedRandom.next());
}

BirthPoints generateBirthPoints(const Planet &planet,
                                const PlanetRawData &rawData,
                                int birthSeed) {
    BirthPoints result{};
    if (planet.type == EPlanetType::Gas) {
        result.birthPoint = {0.0f, 0.0f,
                             0.0f - planet.realRadius() - 25.0f};
        result.resourcePoint0 = Up;
        result.resourcePoint1 = Down;
        return result;
    }

    util::DotNet35Random random(birthSeed);

    auto vector = predictLocalStarDirection(planet, 85.0);
    auto axis0 = cross(vector, Up).normalized();
    auto axis1 = cross(axis0, vector).normalized();

    int i = 0;
    constexpr int attemptCount = 256;
    for (; i < attemptCount; ++i) {
        const auto offset0 = static_cast<float>(
                                 random.nextDouble() * 2.0 - 1.0) *
                             0.5f;
        const auto offset1 = static_cast<float>(
                                 random.nextDouble() * 2.0 - 1.0) *
                             0.5f;
        auto center = vector + offset0 * axis0 + offset1 * axis1;
        center.normalize();
        result.birthPoint =
            center * (planet.realRadius() + 0.2f + 1.45f);
        axis0 = cross(center, Up).normalized();
        axis1 = cross(axis0, center).normalized();

        bool accepted = false;
        for (int j = 0; j < 10; ++j) {
            const auto x = static_cast<float>(random.nextDouble() * 2.0 - 1.0);
            const auto y = static_cast<float>(random.nextDouble() * 2.0 - 1.0);
            auto point0 = Vector2{x, y}.normalized() * 0.1f;
            auto point1 = -point0;
            point1.x += static_cast<float>(random.nextDouble() * 2.0 - 1.0) *
                        0.06f;
            point1.y += static_cast<float>(random.nextDouble() * 2.0 - 1.0) *
                        0.06f;
            auto resource0 =
                (center + point0.x * axis0 + point0.y * axis1).normalized();
            auto resource1 =
                (center + point1.x * axis0 + point1.y * axis1).normalized();
            result.resourcePoint0 = resource0.normalized();
            result.resourcePoint1 = resource1.normalized();
            const auto minimumHeight = planet.realRadius() + 0.2f;
            if (!(rawData.queryHeight(center) > minimumHeight &&
                  rawData.queryHeight(resource0) > minimumHeight &&
                  rawData.queryHeight(resource1) > minimumHeight))
                continue;
            const Vector3 checks[] = {
                resource0 + axis0 * 0.03f,
                resource0 - axis0 * 0.03f,
                resource0 + axis1 * 0.03f,
                resource0 - axis1 * 0.03f,
                resource1 + axis0 * 0.03f,
                resource1 - axis0 * 0.03f,
                resource1 + axis1 * 0.03f,
                resource1 - axis1 * 0.03f,
            };
            accepted = true;
            for (const auto &check: checks) {
                if (rawData.queryHeight(check) > minimumHeight) continue;
                accepted = false;
                break;
            }
            if (accepted) break;
        }
        if (accepted) break;
    }
    if (i >= attemptCount)
        result.birthPoint = {0.0f, planet.realRadius() + 5.0f, 0.0f};
    return result;
}

Vector3 rawSnap(Vector3 position, float planetRadius) {
    position.normalize();
    const auto segment = static_cast<int>(planetRadius / 4.0f + 0.1f) * 4;
    auto latitude =
        static_cast<float>(std::asin(static_cast<double>(position.y)));
    auto longitude = static_cast<float>(std::atan2(
        static_cast<double>(position.x), static_cast<double>(0.0f - position.z)));
    auto latitudeGrid = latitude / (Pi * 2.0f) * static_cast<float>(segment);
    const auto latitudeIndex = static_cast<int>(std::floor(static_cast<double>(
        std::max(0.0f, std::abs(latitudeGrid) - 0.1f))));
    const auto longitudeSegments =
        static_cast<float>(determineLongitudeSegmentCount(latitudeIndex, segment));
    auto longitudeGrid = longitude / (Pi * 2.0f) * longitudeSegments;
    latitudeGrid = util::mathfRound(latitudeGrid * 5.0f) / 5.0f;
    longitudeGrid = util::mathfRound(longitudeGrid * 5.0f) / 5.0f;
    latitude = latitudeGrid / static_cast<float>(segment) * (Pi * 2.0f);
    longitude = longitudeGrid / longitudeSegments * (Pi * 2.0f);
    const auto y = static_cast<float>(std::sin(static_cast<double>(latitude)));
    const auto latitudeCosine =
        static_cast<float>(std::cos(static_cast<double>(latitude)));
    const auto longitudeSine =
        static_cast<float>(std::sin(static_cast<double>(longitude)));
    const auto longitudeCosine =
        static_cast<float>(std::cos(static_cast<double>(longitude)));
    return {latitudeCosine * longitudeSine,
            y,
            latitudeCosine * (0.0f - longitudeCosine)};
}

}
