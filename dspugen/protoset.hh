/*
 * Copyright (c) 2022 Soar Qin<soarchin@gmail.com>
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#pragma once

#include "vectors.hh"

#include <unordered_map>
#include <vector>
#include <string>
#include <cstdint>

namespace dspugen {

namespace EThemeDistribute {

enum : int {
    Default,
    Birth,
    Interstellar,
    Rare
};

}

template<typename T>
class ProtoSet {
private:
    std::unordered_map<int, int> dataIndices;
    std::unordered_map<std::string, int> nameIndices;

public:
    std::vector<T> dataArray;

public:
    inline const T *operator[](int index) const {
        return dataArray[index];
    }

    inline void init(int length) {
        dataArray.resize(length);
    }

    inline const T *select(int id) const {
        auto ite = dataIndices.find(id);
        if (ite == dataIndices.end()) { return nullptr; }
        return &dataArray[ite->second];
    }

    inline const T *select(const std::string &name) const {
        auto ite = nameIndices.find(name);
        if (ite == nameIndices.end()) { return nullptr; }
        return &dataArray[ite->second];
    }

    inline void onLoaded() {
        auto count = static_cast<int>(dataArray.size());
        for (int i = 0; i < count; ++i) {
            dataIndices[dataArray[i].id] = i;
            nameIndices[dataArray[i].name] = i;
        }
    }
};

struct Proto {
    int id;
    std::string name;
};

struct ThemeProto : Proto {
    std::vector<int> algos;
    std::string displayName;
    int distribute = 0;
    std::vector<int> gasItems;
    std::vector<float> gasSpeeds;
    int iceFlag = 0;
    float ionHeight = 0.0f;
    Vector2 modX;
    Vector2 modY;
    int planetType = 0;
    std::vector<float> rareSettings;
    std::vector<int> rareVeins;
    float temperature = 0.0f;
    bool useHeightForBuild = false;
    std::vector<int> vegetables0;
    std::vector<int> vegetables1;
    std::vector<int> vegetables2;
    std::vector<int> vegetables3;
    std::vector<int> vegetables4;
    std::vector<int> vegetables5;
    std::vector<float> veinCount;
    std::vector<float> veinOpacity;
    std::vector<int> veinSpot;
    float waterHeight = 0.0f;
    int waterItemId = 0;
    float wind = 0.0f;
};

struct ItemProto : Proto {
    int64_t heatValue = 0;
};

struct VeinProto : Proto {
    int miningItem = 0;
    int modelCount = 0;
    int modelIndex = 0;
};

struct StringProto : Proto {
    std::string name;
    std::string zhcn;
    std::string enus;
    std::string frfr;
};

extern void loadProtoSets();

using ThemeProtoSet = ProtoSet<ThemeProto>;
extern ThemeProtoSet themeProtoSet;
using ItemProtoSet = ProtoSet<ItemProto>;
extern ItemProtoSet itemProtoSet;
using VeinProtoSet = ProtoSet<VeinProto>;
extern VeinProtoSet veinProtoSet;
using StringProtoSet = ProtoSet<StringProto>;
extern StringProtoSet stringProtoSet;

/* for type: 0-ZHCN 1-ENUS 2-FRFR */
extern const std::string &translate(const std::string &name, int type = 0);

}
