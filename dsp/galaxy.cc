/*
 * Copyright (c) 2022 Soar Qin<soarchin@gmail.com>
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#include "galaxy.hh"

#include "util/dotnet35random.hh"
#include "util/mempool.hh"
#include "vectors.hh"
#include <vector>

static thread_local MemPool<Galaxy> gpool;

bool CheckCollision(std::vector<VectorLF3> &pts, VectorLF3 pt, double minDist) {
    double num = minDist * minDist;
    for (auto &pt2: pts) {
        double num2 = pt.x - pt2.x;
        double num3 = pt.y - pt2.y;
        double num4 = pt.z - pt2.z;
        if (num2 * num2 + num3 * num3 + num4 * num4 < num) return true;
    }
    return false;
}

static void RandomPoses(std::vector<VectorLF3> &tmpPoses, std::vector<VectorLF3> &tmpDrunk, int seed, int maxCount,
                        double minDist, double minStepLen, double maxStepLen,
                        double flatten) {
    DotNet35Random dotNet35Random(seed);
    double num = dotNet35Random.nextDouble();
    tmpPoses.emplace_back();
    int num2 = 6;
    int num3 = 8;
    int num4 = (int)(num * (num3 - num2) + num2);
    for (int i = 0; i < num4; i++) {
        int num5 = 0;
        while (num5++ < 256) {
            double num6 = dotNet35Random.nextDouble() * 2.0 - 1.0;
            double num7 = (dotNet35Random.nextDouble() * 2.0 - 1.0) * flatten;
            double num8 = dotNet35Random.nextDouble() * 2.0 - 1.0;
            double num9 = dotNet35Random.nextDouble();
            double num10 = num6 * num6 + num7 * num7 + num8 * num8;
            if (num10 > 1.0 || num10 < 1E-08) continue;
            double num11 = std::sqrt(num10);
            num9 = (num9 * (maxStepLen - minStepLen) + minDist) / num11;
            VectorLF3 vectorLf{num6 * num9, num7 * num9, num8 * num9};
            if (!CheckCollision(tmpPoses, vectorLf, minDist)) {
                tmpDrunk.emplace_back(vectorLf);
                tmpPoses.emplace_back(vectorLf);
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
                double num14 = dotNet35Random.nextDouble() * 2.0 - 1.0;
                double num15 = (dotNet35Random.nextDouble() * 2.0 - 1.0) * flatten;
                double num16 = dotNet35Random.nextDouble() * 2.0 - 1.0;
                double num17 = dotNet35Random.nextDouble();
                double num18 = num14 * num14 + num15 * num15 + num16 * num16;
                if (num18 > 1.0 || num18 < 1E-08) continue;
                double num19 = std::sqrt(num18);
                num17 = (num17 * (maxStepLen - minStepLen) + minStepLen) / num19;
                VectorLF3 vectorLf2{drunk.x + num14 * num17, drunk.y + num15 * num17,
                                    drunk.z + num16 * num17};
                if (!CheckCollision(tmpPoses, vectorLf2, minDist)) {
                    drunk = vectorLf2;
                    tmpPoses.emplace_back(vectorLf2);
                    if (tmpPoses.size() < maxCount) break;
                    return;
                }
            }
        }
}

static int GenerateTempPoses(std::vector<VectorLF3> &tmpPoses, std::vector<VectorLF3> &tmpDrunk, int seed,
                             int targetCount, int iterCount, double minDist,
                             double minStepLen, double maxStepLen, double flatten) {
    if (iterCount < 1)
        iterCount = 1;
    else if (iterCount > 16) iterCount = 16;
    RandomPoses(tmpPoses, tmpDrunk, seed, targetCount * iterCount, minDist, minStepLen, maxStepLen, flatten);
    for (int num = int(tmpPoses.size()) - 1; num >= 0; num--) {
        if ((num % iterCount) == 0) { continue; }
        tmpPoses.erase(tmpPoses.begin() + num);
        if (tmpPoses.size() <= targetCount) break;
    }
    return int(tmpPoses.size());
}

Galaxy::~Galaxy() {
    for (auto *s: stars) {
        s->release();
    }
}

void Galaxy::release() {
    gpool.release(this);
}

Galaxy *Galaxy::create(int algoVersion, int galaxySeed, int starCount, bool genName, bool hasPlanets) {
    DotNet35Random dotNet35Random(galaxySeed);
    std::vector<VectorLF3> tmpPoses, tmpDrunk;
    tmpPoses.reserve(256);
    tmpDrunk.reserve(256);
    const double MIN_DIST = 2.0;
    const double MIN_STEP = 2.0;
    const double MAX_STEP = 3.2;
    starCount = GenerateTempPoses(tmpPoses, tmpDrunk, dotNet35Random.next(), starCount, 4, MIN_DIST, MIN_STEP, MAX_STEP, 0.18);
    if (starCount <= 0) { return nullptr; }

    auto *galaxy = gpool.alloc();
    galaxy->seed = galaxySeed;
    galaxy->starCount = starCount;
    galaxy->stars.resize(starCount);

    auto num = (float)dotNet35Random.nextDouble();
    auto num2 = (float)dotNet35Random.nextDouble();
    auto num3 = (float)dotNet35Random.nextDouble();
    auto num4 = (float)dotNet35Random.nextDouble();
    auto num5 = (int)std::ceil(0.01f * starCount + num * 0.3f);
    auto num6 = (int)std::ceil(0.01f * starCount + num2 * 0.3f);
    auto num7 = (int)std::ceil(0.016f * starCount + num3 * 0.4f);
    auto num8 = (int)std::ceil(0.013f * starCount + num4 * 1.4f);
    auto num9 = starCount - num5;
    auto num10 = num9 - num6;
    auto num11 = num10 - num7;
    auto num12 = (num11 - 1) / num8;
    auto num13 = num12 / 2;

    for (int i = 0; i < starCount; i++) {
        auto seed = dotNet35Random.next();
        if (i == 0) {
            galaxy->stars[i] = Star::createBirthStar(galaxy, seed, genName);
            galaxy->birthStarId = galaxy->stars[i]->id;
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
        galaxy->stars[i] = Star::createStar(galaxy, tmpPoses[i], i + 1, seed, needtype, needSpectr, genName);
    }
    if (hasPlanets) {
        for (auto &star: galaxy->stars) {
            star->createStarPlanets();
        }
    }
    return galaxy;
}
