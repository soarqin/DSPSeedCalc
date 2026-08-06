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
#include <cstddef>
#include <cstdint>
#include <vector>

namespace dspugen {

Settings settings;

static thread_local util::MemPool<Galaxy> *gpool;

constexpr double MIN_DIST = 2.0;
constexpr double MIN_STEP = 2.0;
constexpr double MAX_STEP = 3.2;
constexpr double FLATTEN = 0.18;

// MIN_DIST is the only cell size the broad phase ever uses, and 1.0 / 2.0 is exact,
// so multiplying is bit-identical to dividing and keeps the same floor boundaries.
constexpr double CELL_SCALE = 1.0 / MIN_DIST;

// Upper bound on the points one walk can accept, independent of maxCount: one origin,
// at most 7 first-stage points (num4 = int(r * 2 + 6) with r in [0, 1)), then at most
// 256 rounds that each move every drunk point at most once. 1 + 7 + 256 * 7 = 1800.
// The bound also covers a nonsensical non-positive star count, where the size_t
// comparison against maxCount never stops the walk.
constexpr std::size_t MAX_WALK_POINTS = 2048;

// Open-addressed spatial hash, reused across seeds. Slots carry exact cell
// coordinates, so two distinct cells can never be confused; the hash only chooses the
// probe order. Points sharing a cell are chained through nextPoint, indexed by point
// index, so no per-cell container is ever allocated. A per-seed stamp retires the whole
// table in constant time instead of freeing and reallocating buckets.
struct GridSlot {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    std::int32_t head = -1;
    std::uint32_t stamp = 0;
};

struct PositionScratch {
    std::vector<VectorLF3> poses;
    std::vector<VectorLF3> drunk;
    std::vector<GridSlot> grid;
    std::vector<std::int32_t> nextPoint;
    std::uint32_t stamp = 0;
    std::uint32_t mask = 0;
};

static thread_local PositionScratch positionScratch;

void Galaxy::initThread() {
    gpool = new util::MemPool<Galaxy>();
}

void Galaxy::releaseThread() {
    delete gpool;
}

// Cell coordinates stay tiny: the walk cannot move a point further than
// MAX_STEP * 257 from the origin, so |coordinate| < 826 and |cell| < 414. int32 leaves
// six orders of magnitude of headroom.
static inline std::int32_t cellCoord(double value) {
    return static_cast<std::int32_t>(std::floor(value * CELL_SCALE));
}

static inline std::uint32_t cellHash(std::int32_t x, std::int32_t y, std::int32_t z) {
    auto hash = static_cast<std::uint64_t>(static_cast<std::uint32_t>(x)) * 0x9E3779B97F4A7C15ULL;
    hash ^= static_cast<std::uint64_t>(static_cast<std::uint32_t>(y)) * 0xC2B2AE3D27D4EB4FULL;
    hash ^= static_cast<std::uint64_t>(static_cast<std::uint32_t>(z)) * 0x165667B19E3779F9ULL;
    hash ^= hash >> 29U;
    hash *= 0xBF58476D1CE4E5B9ULL;
    return static_cast<std::uint32_t>(hash >> 32U);
}

static void ResetGrid(PositionScratch &scratch, std::size_t maxPoints) {
    // Half load factor guarantees every probe run ends on a retired slot, which is what
    // lets a lookup stop as soon as it sees one.
    auto slots = scratch.grid.size() < 64U ? std::size_t(64) : scratch.grid.size();
    while (slots < (maxPoints + 1) * 2) slots <<= 1U;
    if (slots != scratch.grid.size()) {
        scratch.grid.assign(slots, GridSlot{});
        scratch.stamp = 0;
    }
    scratch.mask = static_cast<std::uint32_t>(slots - 1);
    if (++scratch.stamp == 0) {
        for (auto &slot: scratch.grid) slot.stamp = 0;
        scratch.stamp = 1;
    }
    scratch.nextPoint.resize(maxPoints + 1);
}

static void AddToGrid(PositionScratch &scratch, const VectorLF3 &point, std::int32_t index) {
    auto cx = cellCoord(point.x);
    auto cy = cellCoord(point.y);
    auto cz = cellCoord(point.z);
    auto slot = cellHash(cx, cy, cz) & scratch.mask;
    while (true) {
        auto &entry = scratch.grid[slot];
        if (entry.stamp != scratch.stamp) {
            entry.stamp = scratch.stamp;
            entry.x = cx;
            entry.y = cy;
            entry.z = cz;
            entry.head = index;
            scratch.nextPoint[static_cast<std::size_t>(index)] = -1;
            return;
        }
        if (entry.x == cx && entry.y == cy && entry.z == cz) {
            scratch.nextPoint[static_cast<std::size_t>(index)] = entry.head;
            entry.head = index;
            return;
        }
        slot = (slot + 1) & scratch.mask;
    }
}

// The result is an existential test: whether any already accepted point lies closer
// than minDist. That answer does not depend on the order the neighbours are visited,
// so the indices are tested in place instead of being gathered and sorted first, and
// the walk returns as soon as one point is close enough. The exact predicate is
// unchanged, so the accepted position sequence is bit-identical; only the
// collisionComparisons counter now reports actual comparisons rather than the size of
// the candidate's neighbourhood.
static bool CheckCollision(const std::vector<VectorLF3> &pts, const VectorLF3 &pt,
                           double minDist, const PositionScratch &scratch) {
    double sqrDist = minDist * minDist;
    auto bx = cellCoord(pt.x);
    auto by = cellCoord(pt.y);
    auto bz = cellCoord(pt.z);
    for (std::int32_t x = bx - 1; x <= bx + 1; ++x)
        for (std::int32_t y = by - 1; y <= by + 1; ++y)
            for (std::int32_t z = bz - 1; z <= bz + 1; ++z) {
                auto slot = cellHash(x, y, z) & scratch.mask;
                while (true) {
                    const auto &entry = scratch.grid[slot];
                    if (entry.stamp != scratch.stamp) break;
                    if (entry.x == x && entry.y == y && entry.z == z) {
                        for (auto index = entry.head; index >= 0;
                             index = scratch.nextPoint[static_cast<std::size_t>(index)]) {
                            DSP_INSTRUMENT(++util::instrumentation::local().collisionComparisons);
                            const auto &pt2 = pts[static_cast<std::size_t>(index)];
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
                        break;
                    }
                    slot = (slot + 1) & scratch.mask;
                }
            }
    return false;
}

static void RandomPoses(PositionScratch &scratch, int seed, int maxCount) {
    auto &tmpPoses = scratch.poses;
    auto &tmpDrunk = scratch.drunk;
    util::DotNet35Random dotNet35Random(seed);
    double num = dotNet35Random.nextDouble();
    tmpPoses.emplace_back();
    AddToGrid(scratch, tmpPoses.back(), 0);
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
                AddToGrid(scratch, vectorLf, static_cast<std::int32_t>(tmpPoses.size() - 1));
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
                    AddToGrid(scratch, vectorLf2, static_cast<std::int32_t>(tmpPoses.size() - 1));
                    if (tmpPoses.size() < maxCount) break;
                    return;
                }
            }
        }
}

static int GenerateTempPoses(PositionScratch &scratch, int seed, int targetCount) {
    scratch.poses.clear();
    scratch.drunk.clear();
    auto maxPoints = targetCount > 0
                     ? std::min(static_cast<std::size_t>(targetCount) * 4, MAX_WALK_POINTS)
                     : MAX_WALK_POINTS;
    ResetGrid(scratch, maxPoints);
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

bool Galaxy::hasStarName(const std::string &name) const {
    auto hash = std::hash<std::string>{}(name);
    auto count = starNameHashes.size();
    for (std::size_t i = 0; i < count; ++i) {
        if (starNameHashes[i] != hash) continue;
        const auto *star = stars[i];
        if (star != nullptr && star->name == name) return true;
    }
    return false;
}

void Galaxy::addStarName(const std::string &name) {
    starNameHashes.emplace_back(std::hash<std::string>{}(name));
}

void Galaxy::reset() {
    for (auto *star: stars) {
        if (star) star->release();
    }
    stars.clear();
    starNameHashes.clear();
    birthPlanetId = 0;
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

VeinAmountBatchResult Galaxy::ensureAllVeinAmounts(bool includeTerrainDigest) {
    ensureAllPlanets();
    for (auto *star: stars) {
        for (auto *planet: star->planets) {
            const auto result =
                planet->ensureVeinAmounts(includeTerrainDigest);
            if (result == VeinAmountGenerationResult::Generated) continue;
            return {result, planet->id, planet->algoId};
        }
    }
    return {};
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
