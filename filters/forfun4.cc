/*
 * Copyright (c) 2024 Soar Qin<soarchin@gmail.com>
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#include "filter.hh"
#include <fmt/format.h>
#include <mutex>

extern "C" {

static PluginAPI *pluginAPI = nullptr;
static float highestValue[4] = {0.f, 0.f, 0.f, 0.f};
static int highestId[4] = {0, 0, 0, 0};
static int maxCount[22] = {0};
static int minCount[22] = {0x10000, 0x10000, 0x10000, 0x10000, 0x10000, 0x10000, 0x10000, 0x10000, 0x10000, 0x10000, 0x10000, 0x10000, 0x10000, 0x10000, 0x10000, 0x10000, 0x10000, 0x10000, 0x10000, 0x10000, 0x10000, 0x10000};
static std::vector<int> maxId[22];
static std::vector<int> minId[22];
constexpr int themeIdMap[26] = {-1, -1, 0, 0, 1, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21};
constexpr const char *themeNames[22] = {
    "气态巨星",
    "冰巨星",
    "干旱荒漠",
    "灰烬冻土",
    "海洋丛林",
    "熔岩",
    "冰原冻土",
    "贫瘠荒漠",
    "戈壁",
    "火山灰",
    "红石",
    "草原",
    "水世界",
    "黑石盐滩",
    "樱林海",
    "飓风石林",
    "猩红冰湖",
    "高产气巨",
    "热带草原",
    "橙晶荒漠",
    "极寒冻土",
    "潘多拉沼泽",
};

__declspec(dllexport) const char *FILTERAPI init(PluginAPI *api, int *type) {
    pluginAPI = api;
    *type = 0;
    return "Min/Max count of each theme";
}

__declspec(dllexport) void FILTERAPI uninit() {
    for (int i = 0; i < 22; i++) {
        std::string maxIds;
        for (int id: maxId[i]) {
            maxIds += fmt::format("{}, ", id);
        }
        std::string minIds;
        for (int id: minId[i]) {
            minIds += fmt::format("{}, ", id);
        }
        if (!maxIds.empty()) {
            maxIds.pop_back();
            maxIds.pop_back();
        }
        if (!minIds.empty()) {
            minIds.pop_back();
            minIds.pop_back();
        }
        fmt::println(stdout, "{}: 最大{}({}), 最小{}({})", themeNames[i], maxCount[i], maxIds, minCount[i], minIds);
    }
}

std::mutex mux;

__declspec(dllexport) bool FILTERAPI galaxyFilter(const dspugen::Galaxy *g) {
    float total[2] = {0.f, 0.f};
    int count[22] = {0};
    for (auto *star: g->stars) {
        for (const auto *p: star->planets) {
            int index = themeIdMap[p->theme];
            if (index < 0) {
                continue;
            }
            count[index]++;
            switch (p->theme) {
                case 2:
                case 3:
                case 21:
                    pluginAPI->GeneratePlanetGas(p);
                    total[0] += p->gasSpeeds[0];
                    total[1] += p->gasSpeeds[1];
                    if (p->gasSpeeds[0] > highestValue[0]) {
                        highestValue[0] = p->gasSpeeds[0];
                        highestId[0] = g->seed;
                    }
                    if (p->gasSpeeds[1] > highestValue[1]) {
                        highestValue[1] = p->gasSpeeds[1];
                        highestId[1] = g->seed;
                    }
                    break;
            }
        }
    }
    std::lock_guard lock(mux);
    if (total[0] > highestValue[2]) {
        highestValue[2] = total[0];
        highestId[2] = g->seed;
    }
    if (total[1] > highestValue[3]) {
        highestValue[3] = total[1];
        highestId[3] = g->seed;
    }
    for (int i = 0; i < 22; i++) {
        if (count[i] > maxCount[i]) {
            maxCount[i] = count[i];
            maxId[i] = {g->seed};
        } else if (count[i] == maxCount[i]) {
            if (maxId[i].size() < 10) maxId[i].push_back(g->seed);
        }
        if (count[i] < minCount[i]) {
            minCount[i] = count[i];
            minId[i] = {g->seed};
        } else if (count[i] == minCount[i]) {
            if (minId[i].size() < 10) minId[i].push_back(g->seed);
        }
    }
    return false;
}

}
