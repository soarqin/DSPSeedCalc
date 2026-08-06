#include "vein_amounts.hh"

#include "../galaxy.hh"
#include "../planet.hh"
#include "../protoset.hh"
#include "../star.hh"
#include "../terrain/planet_algorithm.hh"
#include "../terrain/planet_geometry.hh"
#include "../terrain/planet_raw_data.hh"
#include "../terrain/unity_math.hh"
#include "../util/dotnet35random.hh"
#include "../util/maths.hh"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace dspugen {

namespace vein_detail {

bool acceptsVeinCenter(int algorithmId, EVeinType type, float height,
                       float radius, float realRadius) {
    if (algorithmId == 7) {
        if (type == EVeinType::Bamboo)
            return !(height > realRadius - 4.0f);
        return true;
    }
    if (height < radius) return false;
    if (type == EVeinType::Oil && height < radius + 0.5f) return false;
    if (algorithmId == 11) {
        if (static_cast<int>(type) <= 2 && height > radius + 0.7f)
            return false;
        if ((type == EVeinType::Silicium || type == EVeinType::Titanium) &&
            height <= radius + 0.7f)
            return false;
    } else if (algorithmId == 12) {
        if (type == EVeinType::Fireice && height < radius + 1.2f)
            return false;
    } else if (algorithmId == 13) {
        if (static_cast<int>(type) <= 4 && height > radius + 0.7f)
            return false;
    }
    return true;
}

bool acceptsVeinNode(int algorithmId, int waterItemId, float height,
                     float radius) {
    if (algorithmId == 7) return true;
    return waterItemId == 0 || !(height < radius);
}

}

namespace {

using terrain::PlanetRawData;
using terrain::Vector2;
using terrain::Vector3;
bool isBirthPlanet(const Planet &planet) {
    return planet.galaxy != nullptr &&
           planet.galaxy->birthPlanetId == planet.id;
}

float resourceExponent(const Star &star) {
    switch (star.type) {
    case EStarType::MainSeqStar:
        switch (star.spectr) {
        case ESpectrType::M: return 2.5f;
        case ESpectrType::K: return 1.0f;
        case ESpectrType::G: return 0.7f;
        case ESpectrType::F: return 0.6f;
        case ESpectrType::A: return 1.0f;
        case ESpectrType::B: return 0.4f;
        case ESpectrType::O: return 1.6f;
        default: return 1.0f;
        }
    case EStarType::GiantStar: return 2.5f;
    case EStarType::WhiteDwarf: return 3.5f;
    case EStarType::NeutronStar: return 4.5f;
    case EStarType::BlackHole: return 5.0f;
    }
    return 1.0f;
}

void applyStarVeinBonuses(const Planet &planet,
                          util::DotNet35Random &random,
                          std::array<int, VeinTypeCount> &spot,
                          std::array<float, VeinTypeCount> &count,
                          std::array<float, VeinTypeCount> &opacity) {
    const auto p = resourceExponent(*planet.star);
    switch (planet.star->type) {
    case EStarType::WhiteDwarf:
        spot[9] += 2;
        for (int i = 1; i < 12 && random.nextDouble() < 0.44999998807907104; ++i)
            ++spot[9];
        count[9] = 0.7f;
        opacity[9] = 1.0f;
        spot[10] += 2;
        for (int i = 1; i < 12 && random.nextDouble() < 0.44999998807907104; ++i)
            ++spot[10];
        count[10] = 0.7f;
        opacity[10] = 1.0f;
        ++spot[12];
        for (int i = 1; i < 12 && random.nextDouble() < 0.5; ++i)
            ++spot[12];
        count[12] = 0.7f;
        opacity[12] = 0.3f;
        break;
    case EStarType::NeutronStar:
    case EStarType::BlackHole: {
        ++spot[14];
        auto probability = planet.star->type == EStarType::NeutronStar
                               ? 0.6499999761581421
                               : 0.6499999761581421;
        for (int i = 1; i < 12 && random.nextDouble() < probability; ++i)
            ++spot[14];
        count[14] = 0.7f;
        opacity[14] = 0.3f;
        break;
    }
    default:
        break;
    }

    // The caller performs the rare-vein rolls because they consume the same
    // random stream used for candidate placement.  Keep p available there.
    (void)p;
}

void addRareVeins(const Planet &planet,
                  const ThemeProto &theme,
                  float p,
                  util::DotNet35Random &random,
                  std::array<int, VeinTypeCount> &spot,
                  std::array<float, VeinTypeCount> &count,
                  std::array<float, VeinTypeCount> &opacity) {
    for (std::size_t i = 0; i < theme.rareVeins.size(); ++i) {
        auto type = theme.rareVeins[i];
        if (type <= 0 || type >= static_cast<int>(VeinTypeCount)) continue;
        auto baseOffset = i * 4;
        if (baseOffset + 3 >= theme.rareSettings.size()) break;
        auto probability = theme.rareSettings[baseOffset +
                                               (planet.star->index == 0 ? 0 : 1)];
        auto clusterProbability = theme.rareSettings[baseOffset + 2];
        auto opacityValue = theme.rareSettings[baseOffset + 3];
        probability = 1.0f - util::mathfPow(1.0f - probability, p);
        opacityValue = 1.0f - util::mathfPow(1.0f - opacityValue, p);
        // The managed implementation uses the transformed opacity for both
        // count and amount opacity and performs one gate roll before adding a
        // rare vein family.
        if (random.nextDouble() >= static_cast<double>(probability)) continue;
        ++spot[static_cast<std::size_t>(type)];
        count[static_cast<std::size_t>(type)] = opacityValue;
        opacity[static_cast<std::size_t>(type)] = opacityValue;
        for (int j = 1; j < 12; ++j) {
            if (random.nextDouble() >= static_cast<double>(clusterProbability))
                break;
            ++spot[static_cast<std::size_t>(type)];
        }
    }
}

void generateBaseSummary(const Planet &planet,
                         const ThemeProto &theme,
                         PlanetRawData &data,
                         VeinAmountSummary &summary) {
    std::array<int, VeinTypeCount> spot{};
    std::array<float, VeinTypeCount> veinCount{};
    std::array<float, VeinTypeCount> veinOpacity{};
    for (std::size_t i = 0; i < theme.veinSpot.size() && i + 1 < VeinTypeCount; ++i)
        spot[i + 1] = theme.veinSpot[i];
    for (std::size_t i = 0; i < theme.veinCount.size() && i + 1 < VeinTypeCount; ++i)
        veinCount[i + 1] = theme.veinCount[i];
    for (std::size_t i = 0; i < theme.veinOpacity.size() && i + 1 < VeinTypeCount; ++i)
        veinOpacity[i + 1] = theme.veinOpacity[i];

    util::DotNet35Random random(planet.seed);
    random.next();
    random.next();
    random.next();
    random.next();
    const auto birthSeed = random.next();
    util::DotNet35Random placementRandom(random.next());

    auto p = resourceExponent(*planet.star);
    applyStarVeinBonuses(planet, random, spot, veinCount, veinOpacity);
    addRareVeins(planet, theme, p, random, spot, veinCount, veinOpacity);

    const auto birth = isBirthPlanet(planet);
    const auto algorithm7 = planet.algoId == 7;
    auto resourceScale = planet.star->updateResourceCoef();
    if (birth) resourceScale *= (2.0f / 3.0f);
    constexpr float amountScale = 1.1f;
    const auto spacing = 2.1f / planet.radius;

    std::vector<Vector3> veinVectors;
    veinVectors.reserve(512);
    std::vector<int> veinTypes;
    veinTypes.reserve(512);

    Vector3 birthPoint;
    if (birth) {
        if (algorithm7) {
            birthPoint = terrain::predictLocalStarDirection(planet, 120.0) *
                         0.75f;
        } else {
            const auto points =
                terrain::generateBirthPoints(planet, data, birthSeed);
            birthPoint = points.birthPoint;
            birthPoint.normalize();
            birthPoint = birthPoint * 0.75f;
            veinVectors.emplace_back(points.resourcePoint0);
            veinTypes.emplace_back(static_cast<int>(EVeinType::Iron));
            veinVectors.emplace_back(points.resourcePoint1);
            veinTypes.emplace_back(static_cast<int>(EVeinType::Copper));
        }
    } else {
        birthPoint = {
            static_cast<float>(placementRandom.nextDouble()) * 2.0f - 1.0f,
            static_cast<float>(placementRandom.nextDouble()) - 0.5f,
            static_cast<float>(placementRandom.nextDouble()) * 2.0f - 1.0f};
        birthPoint.normalize();
        birthPoint = birthPoint *
                     static_cast<float>(placementRandom.nextDouble() * 0.4 + 0.2);
    }

    for (int type = 1; type < 15 && veinVectors.size() < 512; ++type) {
        auto attempts = spot[static_cast<std::size_t>(type)];
        if (attempts > 1) attempts += placementRandom.next(-1, 2);
        for (int cluster = 0; cluster < attempts && veinVectors.size() < 512; ++cluster) {
            Vector3 center;
            bool accepted = false;
            int retries = 0;
            while (++retries <= 200) {
                center = {
                    static_cast<float>(placementRandom.nextDouble()) * 2.0f - 1.0f,
                    static_cast<float>(placementRandom.nextDouble()) * 2.0f - 1.0f,
                    static_cast<float>(placementRandom.nextDouble()) * 2.0f - 1.0f};
                if (type != static_cast<int>(EVeinType::Oil)) center = center + birthPoint;
                center.normalize();
                const auto veinType = static_cast<EVeinType>(type);
                float height = 0.0f;
                if (planet.algoId != 7 || veinType == EVeinType::Bamboo)
                    height = data.queryHeight(center);
                if (!vein_detail::acceptsVeinCenter(
                        planet.algoId, veinType, height, planet.radius,
                        planet.realRadius()))
                    continue;
                auto minDistance = (type == static_cast<int>(EVeinType::Oil)) ? 100.0f : 196.0f;
                auto threshold = spacing * spacing * minDistance;
                auto tooClose = false;
                for (const auto &other: veinVectors) {
                    if ((other - center).sqrMagnitude() < threshold) {
                        tooClose = true;
                        break;
                    }
                }
                if (!tooClose) {
                    accepted = true;
                    break;
                }
            }
            if (accepted) {
                veinVectors.emplace_back(center);
                veinTypes.emplace_back(type);
            }
        }
    }

    constexpr Vector3 up{0.0f, 1.0f, 0.0f};
    constexpr Vector3 right{1.0f, 0.0f, 0.0f};
    constexpr Vector3 forward{0.0f, 0.0f, 1.0f};
    for (std::size_t group = 0; group < veinVectors.size(); ++group) {
        auto normalized = veinVectors[group].normalized();
        auto type = veinTypes[group];
        auto index = static_cast<std::size_t>(type);
        std::vector<Vector2> points;
        points.reserve(32);
        points.emplace_back(Vector2{});
        auto count = util::mathfRoundToInt(
            veinCount[index] * static_cast<float>(placementRandom.next(20, 25)));
        if (type == static_cast<int>(EVeinType::Oil)) count = 1;
        auto opacity = veinOpacity[index];
        if (birth && !algorithm7 && group < 2) {
            count = 6;
            opacity = 0.2f;
        }
        int rounds = 0;
        while (rounds++ < 20) {
            auto initialCount = points.size();
            for (std::size_t i = 0; i < initialCount && points.size() < static_cast<std::size_t>(std::max(count, 0)); ++i) {
                if (points[i].sqrMagnitude() > 36.0f) continue;
                auto angle = placementRandom.nextDouble() * 3.14159265358979323846 * 2.0;
                Vector2 direction{static_cast<float>(std::cos(angle)),
                                  static_cast<float>(std::sin(angle))};
                direction = direction + points[i] * 0.2f;
                direction = direction.normalized();
                auto point = points[i] + direction;
                bool duplicate = false;
                for (const auto &old: points) {
                    if ((old - point).sqrMagnitude() < 0.85f) {
                        duplicate = true;
                        break;
                    }
                }
                if (!duplicate) points.emplace_back(point);
            }
            if (points.size() >= static_cast<std::size_t>(std::max(count, 0))) break;
        }

        const auto rotation = terrain::fromToRotation(up, normalized);
        Vector3 basisRight = rotation * right;
        Vector3 basisForward = rotation * forward;
        auto amountScaleForType = type == static_cast<int>(EVeinType::Oil)
                                      ? util::mathfPow(resourceScale, 0.5f)
                                      : resourceScale;
        auto amount = util::mathfRoundToInt(opacity * 100000.0f * amountScaleForType);
        if (amount < 20) amount = 20;
        auto spread = amount < 16000 ? static_cast<int>(std::floor(static_cast<float>(amount) * 0.9375f)) : 15000;
        auto minValue = amount - spread;
        auto maxValue = amount + spread + 1;
        for (const auto &point: points) {
            auto offset = (basisRight * point.x + basisForward * point.y) * spacing;
            auto position = normalized + offset;
            const auto *veinProto = veinProtoSet.select(type);
            if (veinProto == nullptr || veinProto->modelCount <= 0) {
                throw std::runtime_error(
                    "missing or invalid vein prototype model range");
            }
            (void)placementRandom.next(
                veinProto->modelIndex,
                veinProto->modelIndex + veinProto->modelCount);
            auto nodeAmount = util::mathfRoundToInt(
                static_cast<float>(placementRandom.next(minValue, maxValue)) * amountScale);
            if (nodeAmount < 1) nodeAmount = 1;
            if (type == static_cast<int>(EVeinType::Oil))
                position = terrain::rawSnap(position, planet.radius);
            const auto height = data.queryHeight(position);
            data.eraseVegetableAtPoint(position);
            if (!vein_detail::acceptsVeinNode(
                    planet.algoId, planet.waterItemId, height, planet.radius))
                continue;
            ++summary.nodeCount[index];
            summary.baseAmount[index] += nodeAmount;
        }
    }
}

}

VeinAmountGenerationResult generateVeinAmountSummary(
    const Planet &planet, VeinAmountSummary &summary) {
    summary = {};
    if (planet.star == nullptr)
        return VeinAmountGenerationResult::InvalidPlanet;
    if (planet.type == EPlanetType::Gas || planet.algoId == 0)
        return VeinAmountGenerationResult::Generated;
    const auto *theme = themeProtoSet.select(planet.theme);
    if (theme == nullptr)
        return VeinAmountGenerationResult::MissingTheme;

    if (planet.algoId < 1 || planet.algoId > 14)
        return VeinAmountGenerationResult::UnsupportedAlgorithm;

    PlanetRawData raw(200);
    raw.calcVerts();
    terrain::PlanetAlgorithmContext terrainContext{raw, planet.seed, planet.radius};
    auto terrainResult = terrain::dispatchTerrain(planet.algoId, terrainContext,
                                                  planet.modX, planet.modY);
    if (terrainResult == terrain::TerrainGenerationResult::UnsupportedAlgorithm) {
        return VeinAmountGenerationResult::UnsupportedAlgorithm;
    }
    generateBaseSummary(planet, *theme, raw, summary);
    return VeinAmountGenerationResult::Generated;
}

}
