/*
 * Copyright (c) 2022 Soar Qin<soarchin@gmail.com>
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#include "galaxy.hh"

#include "settings.hh"
#include "util/dotnet35random.hh"
#include "util/mempool.hh"
#include "util/instrumentation.hh"
#include "vectors.hh"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace dspugen {

Settings settings;

static thread_local util::MemPool<Galaxy> *gpool;

struct GridCell {
    std::int64_t x;
    std::int64_t y;
    std::int64_t z;

    bool operator==(const GridCell &other) const {
        return x == other.x && y == other.y && z == other.z;
    }
};

struct GridCellHash {
    std::size_t operator()(const GridCell &cell) const {
        auto hash = static_cast<std::size_t>(cell.x);
        hash ^= static_cast<std::size_t>(cell.y) + 0x9e3779b9U + (hash << 6U) + (hash >> 2U);
        hash ^= static_cast<std::size_t>(cell.z) + 0x9e3779b9U + (hash << 6U) + (hash >> 2U);
        return hash;
    }
};

struct PositionScratch {
    std::vector<VectorLF3> poses;
    std::vector<VectorLF3> drunk;
    std::unordered_map<GridCell, std::vector<std::size_t>, GridCellHash> grid;
    std::vector<std::size_t> nearby;
};

static thread_local PositionScratch positionScratch;

void Galaxy::initThread() {
    gpool = new util::MemPool<Galaxy>();
}

void Galaxy::releaseThread() {
    delete gpool;
}

static GridCell getGridCell(const VectorLF3 &point, double cellSize) {
    return {
        static_cast<std::int64_t>(std::floor(point.x / cellSize)),
        static_cast<std::int64_t>(std::floor(point.y / cellSize)),
        static_cast<std::int64_t>(std::floor(point.z / cellSize)),
    };
}

static void AddToGrid(PositionScratch &scratch, const VectorLF3 &point, std::size_t index,
                      double cellSize) {
    scratch.grid[getGridCell(point, cellSize)].emplace_back(index);
}

static bool CheckCollision(const std::vector<VectorLF3> &pts, const VectorLF3 &pt,
                           double minDist, PositionScratch &scratch) {
    double sqrDist = minDist * minDist;
    auto cell = getGridCell(pt, minDist);
    scratch.nearby.clear();
    for (std::int64_t x = cell.x - 1; x <= cell.x + 1; ++x)
        for (std::int64_t y = cell.y - 1; y <= cell.y + 1; ++y)
            for (std::int64_t z = cell.z - 1; z <= cell.z + 1; ++z) {
                auto found = scratch.grid.find({x, y, z});
                if (found != scratch.grid.end()) {
                    scratch.nearby.insert(scratch.nearby.end(), found->second.begin(), found->second.end());
                }
            }
    std::sort(scratch.nearby.begin(), scratch.nearby.end());
    for (auto index: scratch.nearby) {
        DSP_INSTRUMENT(++util::instrumentation::local().collisionComparisons);
        const auto &pt2 = pts[index];
        double dx = pt.x - pt2.x;
        if (std::abs(dx) >= minDist) continue;
        double dy = pt.y - pt2.y;
        if (std::abs(dy) >= minDist) continue;
        double dz = pt.z - pt2.z;
        if (std::abs(dz) >= minDist) continue;
        if (dx * dx + dy * dy + dz * dz < sqrDist) {
            DSP_INSTRUMENT(++util::instrumentation::local().collisionRejects);
            return true;
        }
    }
    return false;
}

static void RandomPoses(PositionScratch &scratch, int seed, int maxCount) {
    constexpr double MIN_DIST = 2.0;
    constexpr double MIN_STEP = 2.0;
    constexpr double MAX_STEP = 3.2;
    constexpr double FLATTEN = 0.18;
    auto &tmpPoses = scratch.poses;
    auto &tmpDrunk = scratch.drunk;
    util::DotNet35Random dotNet35Random(seed);
    double num = dotNet35Random.nextDouble();
    tmpPoses.emplace_back();
    AddToGrid(scratch, tmpPoses.back(), 0, MIN_DIST);
    int num2 = 6;
    int num3 = 8;
    int num4 = static_cast<int>(num * (num3 - num2) + num2);
    for (int i = 0; i < num4; i++) {
        int num5 = 0;
        while (num5++ < 256) {
            DSP_INSTRUMENT(++util::instrumentation::local().positionCandidates);
            double num6 = dotNet35Random.nextDouble() * 2.0 - 1.0;
            double num7 = (dotNet35Random.nextDouble() * 2.0 - 1.0) * FLATTEN;
            double num8 = dotNet35Random.nextDouble() * 2.0 - 1.0;
            double num9 = dotNet35Random.nextDouble();
            double num10 = num6 * num6 + num7 * num7 + num8 * num8;
            if (num10 > 1.0 || num10 < 1E-08) {
                DSP_INSTRUMENT(++util::instrumentation::local().unitSphereRejects);
                continue;
            }
            double num11 = std::sqrt(num10);
            num9 = (num9 * (MAX_STEP - MIN_STEP) + MIN_STEP) / num11;
            VectorLF3 vectorLf { num6 * num9, num7 * num9, num8 * num9 };
            if (!CheckCollision(tmpPoses, vectorLf, MIN_DIST, scratch)) {
                tmpDrunk.emplace_back(vectorLf);
                tmpPoses.emplace_back(vectorLf);
                DSP_INSTRUMENT(++util::instrumentation::local().acceptedPositions);
                AddToGrid(scratch, vectorLf, tmpPoses.size() - 1, MIN_DIST);
                if (tmpPoses.size() < maxCount) break;
                return;
            }
        }
    }

    int num12 = 0;
    while (num12++ < 256)
        for (auto &drunk: tmpDrunk) {
            if (dotNet35Random.nextDouble() > 0.7) continue;
            int num13 = 0;
            while (num13++ < 256) {
                DSP_INSTRUMENT(++util::instrumentation::local().positionCandidates);
                double num14 = dotNet35Random.nextDouble() * 2.0 - 1.0;
                double num15 = (dotNet35Random.nextDouble() * 2.0 - 1.0) * FLATTEN;
                double num16 = dotNet35Random.nextDouble() * 2.0 - 1.0;
                double num17 = dotNet35Random.nextDouble();
                double num18 = num14 * num14 + num15 * num15 + num16 * num16;
                if (num18 > 1.0 || num18 < 1E-08) {
                    DSP_INSTRUMENT(++util::instrumentation::local().unitSphereRejects);
                    continue;
                }
                double num19 = std::sqrt(num18);
                num17 = (num17 * (MAX_STEP - MIN_STEP) + MIN_STEP) / num19;
                VectorLF3 vectorLf2 {
                    drunk.x + num14 * num17, drunk.y + num15 * num17,
                    drunk.z + num16 * num17
                };
                if (!CheckCollision(tmpPoses, vectorLf2, MIN_DIST, scratch)) {
                    drunk = vectorLf2;
                    tmpPoses.emplace_back(vectorLf2);
                    DSP_INSTRUMENT(++util::instrumentation::local().acceptedPositions);
                    AddToGrid(scratch, vectorLf2, tmpPoses.size() - 1, MIN_DIST);
                    if (tmpPoses.size() < maxCount) break;
                    return;
                }
            }
        }
}

static int GenerateTempPoses(PositionScratch &scratch, int seed, int targetCount) {
    scratch.poses.clear();
    scratch.drunk.clear();
    scratch.grid.clear();
    scratch.nearby.clear();
    RandomPoses(scratch, seed, targetCount * 4);
    auto &tmpPoses = scratch.poses;
    auto resultingSize = tmpPoses.size();
    auto firstRemoved = tmpPoses.size();
    for (int num = static_cast<int>(tmpPoses.size()) - 1; num >= 0; num--) {
        if ((num % 4) == 0) continue;
        firstRemoved = static_cast<std::size_t>(num);
        --resultingSize;
        if (resultingSize <= static_cast<std::size_t>(targetCount)) break;
    }
    if (firstRemoved < tmpPoses.size()) {
        auto write = firstRemoved;
        for (auto read = firstRemoved; read < tmpPoses.size(); ++read) {
            if ((read % 4) != 0) continue;
            tmpPoses[write++] = tmpPoses[read];
        }
        tmpPoses.resize(resultingSize);
    }
    return static_cast<int>(tmpPoses.size());
}

Galaxy::~Galaxy() {
    for (auto *s: stars) {
        if (s) s->release();
    }
}

void Galaxy::release() {
    gpool->release(this);
}

void Galaxy::reset() {
    for (auto *star: stars) {
        if (star) star->release();
    }
    stars.clear();
    birthStarId = 0;
    HabitableCount = 0;
    seed = 0;
    starCount = 0;
    habitableTarget = 0.0f;
}

void Galaxy::ensureAllPlanets() {
    if (habitableTarget == 0.0f) {
        habitableTarget = std::ceil(starCount * 0.29f);
        if (habitableTarget < 11.0f) habitableTarget = 11.0f;
    }
    bool foundEmpty = false;
    for (auto *star: stars) {
        if (star->planets.empty()) foundEmpty = true;
        else if (foundEmpty) {
            // Out-of-order partial generation cannot preserve HabitableCount.
            return;
        }
    }
    if (!foundEmpty) return;
    for (auto *star: stars)
        if (star->planets.empty()) star->createStarPlanets();
}

void Galaxy::ensureAllGas() {
    ensureAllPlanets();
    for (auto *star: stars)
        for (auto *planet: star->planets)
            planet->generateGas();
}

Galaxy *Galaxy::create(int algoVersion, int galaxySeed, int starCount) {
    util::DotNet35Random dotNet35Random(galaxySeed);
    if (settings.noPosition) {
        static const VectorLF3 temp;
        dotNet35Random.next();
        auto *galaxy = gpool->alloc();
        galaxy->seed = galaxySeed;
        galaxy->starCount = starCount;
        galaxy->stars.resize(settings.birthOnly ? 1 : starCount);

        auto starCountf = float(starCount);
        auto num = float(dotNet35Random.nextDouble());
        auto num2 = float(dotNet35Random.nextDouble());
        auto num3 = float(dotNet35Random.nextDouble());
        auto num4 = float(dotNet35Random.nextDouble());
        auto num5 = int(std::ceil(0.01f * starCountf + num * 0.3f));
        auto num6 = int(std::ceil(0.01f * starCountf + num2 * 0.3f));
        auto num7 = int(std::ceil(0.016f * starCountf + num3 * 0.4f));
        auto num8 = int(std::ceil(0.013f * starCountf + num4 * 1.4f));
        auto num9 = starCount - num5;
        auto num10 = num9 - num6;
        auto num11 = num10 - num7;
        auto num12 = (num11 - 1) / num8;
        auto num13 = num12 / 2;

        for (int i = 0; i < starCount; i++) {
            auto seed = dotNet35Random.next();
            if (i == 0) {
                galaxy->stars[i] = Star::createBirthStar(galaxy, seed);
                galaxy->birthStarId = galaxy->stars[i]->id;
                if (settings.birthOnly) break;
                continue;
            }

            auto needSpectr = ESpectrType::X;
            if (i == 3)
                needSpectr = ESpectrType::M;
            else if (i == num11 - 1) needSpectr = ESpectrType::O;
            auto needtype = EStarType::MainSeqStar;
            if (i % num12 == num13) needtype = EStarType::GiantStar;
            if (i >= num9)
                needtype = EStarType::BlackHole;
            else if (i >= num10)
                needtype = EStarType::NeutronStar;
            else if (i >= num11) needtype = EStarType::WhiteDwarf;
            galaxy->stars[i] = Star::createStar(galaxy, temp, i + 1, seed, needtype, needSpectr);
        }
        if (settings.hasPlanets) {
            galaxy->ensureAllPlanets();
        }
        return galaxy;
    }
    starCount = GenerateTempPoses(positionScratch, dotNet35Random.next(), starCount);
    auto &tmpPoses = positionScratch.poses;
    if (starCount <= 0) { return nullptr; }

    auto *galaxy = gpool->alloc();
    galaxy->seed = galaxySeed;
    galaxy->starCount = starCount;
    galaxy->stars.resize(settings.birthOnly ? 1 : starCount);

    auto starCountf = float(starCount);
    auto num = float(dotNet35Random.nextDouble());
    auto num2 = float(dotNet35Random.nextDouble());
    auto num3 = float(dotNet35Random.nextDouble());
    auto num4 = float(dotNet35Random.nextDouble());
    auto num5 = int(std::ceil(0.01f * starCountf + num * 0.3f));
    auto num6 = int(std::ceil(0.01f * starCountf + num2 * 0.3f));
    auto num7 = int(std::ceil(0.016f * starCountf + num3 * 0.4f));
    auto num8 = int(std::ceil(0.013f * starCountf + num4 * 1.4f));
    auto num9 = starCount - num5;
    auto num10 = num9 - num6;
    auto num11 = num10 - num7;
    auto num12 = (num11 - 1) / num8;
    auto num13 = num12 / 2;

    for (int i = 0; i < starCount; i++) {
        auto seed = dotNet35Random.next();
        if (i == 0) {
            galaxy->stars[i] = Star::createBirthStar(galaxy, seed);
            galaxy->birthStarId = galaxy->stars[i]->id;
            if (settings.birthOnly) break;
            continue;
        }

        auto needSpectr = ESpectrType::X;
        if (i == 3)
            needSpectr = ESpectrType::M;
        else if (i == num11 - 1) needSpectr = ESpectrType::O;
        auto needtype = EStarType::MainSeqStar;
        if (i % num12 == num13) needtype = EStarType::GiantStar;
        if (i >= num9)
            needtype = EStarType::BlackHole;
        else if (i >= num10)
            needtype = EStarType::NeutronStar;
        else if (i >= num11) needtype = EStarType::WhiteDwarf;
        galaxy->stars[i] = Star::createStar(galaxy, tmpPoses[i], i + 1, seed, needtype, needSpectr);
    }
    if (settings.hasPlanets) {
        galaxy->ensureAllPlanets();
    }
    return galaxy;
}

int Galaxy::GeneratePoses(int algoVersion, int galaxySeed, int starCount, std::vector<VectorLF3> &poses) {
    util::DotNet35Random dotNet35Random(galaxySeed);
    auto result = GenerateTempPoses(positionScratch, dotNet35Random.next(), starCount);
    poses.assign(positionScratch.poses.begin(), positionScratch.poses.end());
    return result;
}

}
