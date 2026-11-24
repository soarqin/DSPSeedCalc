/*
 * Copyright (c) 2023 Soar Qin<soarchin@gmail.com>
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#include "filter.hh"

#include <fmt/format.h>

extern "C" {

static PluginAPIV2 *theAPI = nullptr;

__declspec(dllexport) const char *FILTERAPI init3(PluginAPIV2 *api, int *type) {
    theAPI = api;
    *type = 0;
    return "Search for fly star";
}

__declspec(dllexport) bool FILTERAPI describePlugin(std::uint32_t version,
                                                     PluginDescriptorV2 *descriptor) {
    if (version != 2) return false;
    descriptor->filterRequirements = NeedsPlanetTopology | NeedsPlanetTheme | NeedsVeinMetadata | NeedsGas;
    descriptor->capabilities = ThreadSafeCallbacks | OrderIndependent | SupportsBlockScheduling;
    return true;
}

__declspec(dllexport) bool FILTERAPI galaxyFilter(const dspugen::Galaxy *g) {
    const auto *birthStar = g->starById(g->birthStarId);

    for (const auto *star: g->stars) {
        auto sqrDist = (star->position - birthStar->position).sqrMagnitude();
        if (sqrDist > 16) continue;
        uint32_t flag = 0;
        float prod = 0.f;
        for (auto *planet: star->planets) {
            switch (planet->theme) {
            case 15: case 22:
                // 草原 or 热带草原
                flag |= 0x8000u;
                break;
            }
            if (planet->gasItems.size() > 0) {
                auto sz = planet->gasItems.size();
                for (size_t i = 0; i < sz; ++i) {
                    auto item = planet->gasItems[i];
                    switch (item) {
                    case 1120:
                        // 氢
                        flag |= 0x01u;
                        break;
                    case 1121:
                        // 重氢
                        flag |= 0x02u;
                        prod = planet->gasSpeeds[i];
                        break;
                    case 1011:
                        // 可燃冰:
                        flag |= 0x100u;
                        break;
                    }
                }
                continue;
            }
            switch (planet->waterItemId) {
            case 1000:
                // 水
                flag |= 0x04u;
                break;
            case 1116:
                // 硫酸
                flag |= 0x4000u;
                break;
            default:
                break;
            }
            // 3. Silicon veins (硅矿脉)
            // 4. Titanium veins (钛矿脉)
            // 5. Stone veins (石矿脉)
            // 6. Coal veins (煤矿脉)
            // 7. Crude oil seep (原油涌泉)
            // 8. Fire ice vein (可燃冰矿)
            // 9. Kimberlite vein (金伯利矿)
            // 10. Fractal silicon vein (分形硅矿)
            // 11. Organic crystal vein (有机晶体矿)
            // 12. Optical grating crystal vein (光栅石矿)
            // 13. Spiniform stalagmite crystal vein (刺笋矿脉)
            for (int i = 3; i < 14; ++i) {
                if (planet->veinSpot[i] > 1)
                    flag |= 1u << i;
            }
        }
        // if ((flag & 0xF9FDu) == 0xF9FDu) return true;
        if ((flag & 0xF9FFu) == 0xF9FFu) {
            uint32_t magCount = 0;
            for (const auto *star2: g->stars) {
                if (star2->type != dspugen::EStarType::NeutronStar && star2->type != dspugen::EStarType::BlackHole) continue;
                for (auto *planet2: star2->planets) {
                    magCount += planet2->veinSpot[14];
                }
            }
            fmt::print(stdout, "{},{},{:.2f},{:.3f},{}\n", g->seed, g->starCount, std::sqrt(sqrDist), prod, magCount);
            return true;
        }
    }
    return false;
}

}
