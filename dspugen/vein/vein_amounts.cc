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

namespace {

std::uint64_t fnv1a(const void *data, std::size_t length) {
    const auto *bytes = static_cast<const unsigned char *>(data);
    std::uint64_t hash = 14695981039346656037ULL;
    for (std::size_t i = 0; i < length; ++i) {
        hash = (hash ^ bytes[i]) * 1099511628211ULL;
    }
    return hash;
}

}

const char *veinAmountResultName(VeinAmountGenerationResult result) {
    switch (result) {
    case VeinAmountGenerationResult::Generated: return "generated";
    case VeinAmountGenerationResult::NotGenerated: return "not generated";
    case VeinAmountGenerationResult::UnsupportedAlgorithm:
        return "unsupported algorithm";
    case VeinAmountGenerationResult::MissingTheme: return "missing theme";
    case VeinAmountGenerationResult::InvalidPlanet: return "invalid planet";
    case VeinAmountGenerationResult::InvalidThemeData:
        return "invalid theme rare vein data";
    case VeinAmountGenerationResult::ServiceUnavailable:
        return "exact vein service unavailable";
    case VeinAmountGenerationResult::InternalError:
        return "internal error";
    }
    return "unknown";
}

namespace vein_detail {

int underwaterOilAllowance(EVeinType type, int spot) {
    if (type != EVeinType::Oil) return 0;
    // kUnderwaterOilPercentage = 15; the reference computes
    // (int)Math.Ceiling((double)(num11 * 15) / 100.0) in double.
    return static_cast<int>(
        std::ceil(static_cast<double>(spot) * 15.0 / 100.0));
}

bool acceptsVeinCenter(int algorithmId, EVeinType type, float height,
                       float radius, float realRadius,
                       int underwaterOilRemaining) {
    if (algorithmId == 7) {
        if (type == EVeinType::Bamboo)
            return !(height > realRadius - 4.0f);
        return true;
    }
    if (height < radius && type != EVeinType::Oil) return false;
    if (type == EVeinType::Oil && height < radius + 0.5f &&
        (height > radius - 1.0f || underwaterOilRemaining <= 0))
        return false;
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
                     float radius, EVeinType type) {
    if (algorithmId == 7) return true;
    // After the oil rework oil nodes are always added even underwater; the
    // reference appends `|| eVeinType2 == EVeinType.Oil` to the water check.
    return waterItemId == 0 || !(height < radius) || type == EVeinType::Oil;
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
    case EStarType::BlackHole:
        // The reference keeps these as two separate cases but uses the same
        // cluster probability in both; only the resource exponent differs
        // (4.5 versus 5.0), and that is handled by resourceExponent().
        ++spot[14];
        for (int i = 1; i < 12 && random.nextDouble() < 0.6499999761581421; ++i)
            ++spot[14];
        count[14] = 0.7f;
        opacity[14] = 0.3f;
        break;
    default:
        break;
    }
}

bool addRareVeins(const Planet &planet,
                  const ThemeProto &theme,
                  float p,
                  util::DotNet35Random &random,
                  std::array<int, VeinTypeCount> &spot,
                  std::array<float, VeinTypeCount> &count,
                  std::array<float, VeinTypeCount> &opacity) {
    for (std::size_t i = 0; i < theme.rareVeins.size(); ++i) {
        const auto baseOffset = i * 4;
        // The reference indexes RareSettings unconditionally and throws when it
        // is short.  Report that instead of dropping rolls: every later random
        // value in the planet depends on this loop consuming the right amount.
        if (baseOffset + 3 >= theme.rareSettings.size()) return false;
        const auto type = theme.rareVeins[i];
        auto probability = theme.rareSettings[baseOffset +
                                               (planet.star->index == 0 ? 0 : 1)];
        const auto clusterProbability = theme.rareSettings[baseOffset + 2];
        auto opacityValue = theme.rareSettings[baseOffset + 3];
        probability = 1.0f - util::mathfPow(1.0f - probability, p);
        opacityValue = 1.0f - util::mathfPow(1.0f - opacityValue, p);
        // The managed implementation uses the transformed opacity for both
        // count and amount opacity and performs one gate roll before adding a
        // rare vein family.
        if (random.nextDouble() >= static_cast<double>(probability)) continue;
        // The reference sizes its spot/count/opacity arrays by the vein
        // prototype count rather than by 15, so an id outside 1..14 writes to a
        // slot the placement loop never reads.  Skip only the writes: the gate
        // and cluster rolls are consumed either way.
        const auto tracked = type > 0 && type < static_cast<int>(VeinTypeCount);
        if (tracked) {
            const auto index = static_cast<std::size_t>(type);
            ++spot[index];
            count[index] = opacityValue;
            opacity[index] = opacityValue;
        }
        for (int j = 1; j < 12; ++j) {
            if (random.nextDouble() >= static_cast<double>(clusterProbability))
                break;
            if (tracked) ++spot[static_cast<std::size_t>(type)];
        }
    }
    return true;
}

VeinAmountGenerationResult generateBaseSummary(const Planet &planet,
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
    if (!addRareVeins(planet, theme, p, random, spot, veinCount, veinOpacity))
        return VeinAmountGenerationResult::InvalidThemeData;

    const auto birth = isBirthPlanet(planet);
    const auto algorithm7 = planet.algoId == 7;
    auto resourceScale = planet.star->updateResourceCoef();
    if (birth) resourceScale *= (2.0f / 3.0f);
    constexpr float amountScale = 1.1f;
    // Mono evaluates float32 expressions in double and rounds at the store,
    // so this is a double divide rounded once, not a float divide.
    const auto spacing = static_cast<float>(static_cast<double>(2.1f) /
                                            static_cast<double>(planet.radius));

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
        const auto veinType = static_cast<EVeinType>(type);
        // kUnderwaterOilPercentage: up to 15 % (ceil) of oil clusters may sit
        // at least 1 m below sea level; each accepted underwater oil cluster
        // consumes one allowance and adds one extra attempt to replace it.
        int underwaterOilRemaining =
            vein_detail::underwaterOilAllowance(veinType, attempts);
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
                float height = 0.0f;
                if (planet.algoId != 7 || veinType == EVeinType::Bamboo)
                    height = data.queryHeight(center);
                if (!vein_detail::acceptsVeinCenter(
                        planet.algoId, veinType, height, planet.radius,
                        planet.realRadius(), underwaterOilRemaining))
                    continue;
                auto minDistance = (type == static_cast<int>(EVeinType::Oil)) ? 100.0f : 196.0f;
                // The reference forms this product inline in the comparison, so
                // it stays in double and the comparison happens in double.
                const auto threshold = static_cast<double>(spacing) *
                                       static_cast<double>(spacing) *
                                       static_cast<double>(minDistance);
                auto tooClose = false;
                for (const auto &other: veinVectors) {
                    if (static_cast<double>((other - center).sqrMagnitude()) <
                        threshold) {
                        tooClose = true;
                        break;
                    }
                }
                if (!tooClose) {
                    // Only the shared base-class path (non-7 algorithms) tracks
                    // the underwater oil allowance; Algorithm7 overrides
                    // GenerateVeins without that bookkeeping.
                    if (planet.algoId != 7 && veinType == EVeinType::Oil &&
                        height <= planet.radius - 1.0f) {
                        --underwaterOilRemaining;
                        ++attempts;
                    }
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
        // GenerateVeins uses the Unity operator here, not Maths.QRotate.
        Vector3 basisRight = terrain::unityRotate(rotation, right);
        Vector3 basisForward = terrain::unityRotate(rotation, forward);
        // The reference reads PlanetModelingManager's per-id model tables, which
        // are zero-filled for any id without a prototype.  Mirror that rather
        // than failing: DotNet35Random::next() skips its sample when the range
        // is empty or unit, so a missing prototype must not consume one either.
        const auto *veinProto = veinProtoSet.select(type);
        const auto modelIndex = veinProto != nullptr ? veinProto->modelIndex : 0;
        const auto modelCount = veinProto != nullptr ? veinProto->modelCount : 0;
        auto amountScaleForType = type == static_cast<int>(EVeinType::Oil)
                                      ? util::mathfPow(resourceScale, 0.5f)
                                      : resourceScale;
        auto amount = util::mathfRoundToInt(static_cast<float>(
            static_cast<double>(opacity) * 100000.0 *
            static_cast<double>(amountScaleForType)));
        if (amount < 20) amount = 20;
        auto spread = amount < 16000
                          ? static_cast<int>(std::floor(static_cast<float>(
                                static_cast<double>(amount) *
                                static_cast<double>(0.9375f))))
                          : 15000;
        auto minValue = amount - spread;
        auto maxValue = amount + spread + 1;
        for (const auto &point: points) {
            auto offset = (basisRight * point.x + basisForward * point.y) * spacing;
            auto position = normalized + offset;
            (void)placementRandom.next(modelIndex, modelIndex + modelCount);
            auto nodeAmount = util::mathfRoundToInt(static_cast<float>(
                static_cast<double>(placementRandom.next(minValue, maxValue)) *
                static_cast<double>(amountScale)));
            if (nodeAmount < 1) nodeAmount = 1;
            if (type == static_cast<int>(EVeinType::Oil))
                position = terrain::rawSnap(position, planet.radius);
            data.eraseVegetableAtPoint(position);
            if (planet.algoId != 7 && planet.waterItemId != 0) {
                const auto height = data.queryHeight(position);
                if (!vein_detail::acceptsVeinNode(
                        planet.algoId, planet.waterItemId, height,
                        planet.radius, static_cast<EVeinType>(type)))
                    continue;
            }
            ++summary.nodeCount[index];
            summary.baseAmount[index] += nodeAmount;
        }
    }
    return VeinAmountGenerationResult::Generated;
}

}

VeinAmountGenerationResult generateVeinAmountSummary(
    const Planet &planet, VeinAmountSummary &summary, TerrainDigest &digest,
    bool includeTerrainDigest) {
    summary = {};
    digest = {};
    if (planet.star == nullptr)
        return VeinAmountGenerationResult::InvalidPlanet;
    if (planet.type == EPlanetType::Gas || planet.algoId == 0) {
        // The plugin ABI never requests a terrain digest, so a forced exact
        // request for a gas giant returns a ready all-zero summary before any
        // PlanetRawData scratch, terrain sampling or vein placement is entered.
        if (!includeTerrainDigest)
            return VeinAmountGenerationResult::Generated;
        // Gas planets carry no veins, but they do get terrain: PlanetGen gives
        // them precision 64 and PlanetAlgorithm0 fills heightData with
        // (ushort)(radius * 100.0) and biomoData with 0.  Reproducing that is
        // ~17k entries against ~162k for a real algorithm, so the digest is
        // computed instead of leaving a zero sentinel that has to be excluded
        // from every comparison.
        constexpr int gasPrecision = 64;
        constexpr auto gasLength = static_cast<std::size_t>(
            (gasPrecision + 1) * (gasPrecision + 1) * 4);
        static thread_local std::vector<std::uint16_t> gasHeight(gasLength);
        static thread_local const std::vector<std::uint8_t> gasBiomo(gasLength, 0);
        std::fill(gasHeight.begin(), gasHeight.end(),
                  util::narrowToUInt16(static_cast<double>(planet.radius) * 100.0));
        digest = {
            fnv1a(gasHeight.data(), gasHeight.size() * sizeof(std::uint16_t)),
            fnv1a(gasBiomo.data(), gasBiomo.size()),
        };
        return VeinAmountGenerationResult::Generated;
    }
    const auto *theme = themeProtoSet.select(planet.theme);
    if (theme == nullptr)
        return VeinAmountGenerationResult::MissingTheme;

    if (planet.algoId < 1 || planet.algoId > 14)
        return VeinAmountGenerationResult::UnsupportedAlgorithm;

    // PlanetGen gives every non-gas planet precision 200.  One buffer set is
    // ~2.6 MB and its vertex grid is precision-derived, so the instance is
    // reused per thread and only the per-planet height and biome data is
    // cleared between planets.
    static thread_local PlanetRawData raw = [] {
        PlanetRawData data(200);
        data.calcVerts();
        return data;
    }();
    if (includeTerrainDigest) {
        terrain::PlanetAlgorithmContext terrainContext{
            raw, planet.seed, planet.radius};
        auto terrainResult = terrain::dispatchTerrain(
            planet.algoId, terrainContext, planet.modX, planet.modY);
        if (terrainResult ==
            terrain::TerrainGenerationResult::UnsupportedAlgorithm) {
            return VeinAmountGenerationResult::UnsupportedAlgorithm;
        }
        digest = {
            fnv1a(raw.heightData().data(),
                  raw.heightData().size() * sizeof(std::uint16_t)),
            fnv1a(raw.biomoData().data(), raw.biomoData().size()),
        };
        return generateBaseSummary(planet, *theme, raw, summary);
    }

    raw.beginLazyTerrain(planet.algoId, planet.seed, planet.radius,
                         planet.modX, planet.modY);
    const auto result = generateBaseSummary(planet, *theme, raw, summary);
    raw.endLazyTerrain();
    return result;
}

}
