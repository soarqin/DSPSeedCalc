/*
 * Copyright (c) 2022 Soar Qin<soarchin@gmail.com>
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#include "protoset.hh"

#include <nlohmann/json.hpp>
#include <fstream>

namespace dspugen {

ThemeProtoSet themeProtoSet;
ItemProtoSet itemProtoSet;
VeinProtoSet veinProtoSet;
StringProtoSet stringProtoSet;

#define JL(a, b) j.at(#a).get_to(p.b)
void from_json(const nlohmann::json &j, Vector2 &p) {
    JL(x, x);
    JL(y, y);
}

void from_json(const nlohmann::json &j, ThemeProto &p) {
    JL(ID, id);
    JL(Name, name);
    JL(DisplayName, displayName);
    JL(PlanetType, planetType);
    JL(Algos, algos);
    JL(Distribute, distribute);
    JL(Temperature, temperature);
    JL(ModX, modX);
    JL(ModY, modY);
    JL(Vegetables0, vegetables0);
    JL(Vegetables1, vegetables1);
    JL(Vegetables2, vegetables2);
    JL(Vegetables3, vegetables3);
    JL(Vegetables4, vegetables4);
    JL(Vegetables5, vegetables5);
    JL(VeinSpot, veinSpot);
    JL(VeinCount, veinCount);
    JL(VeinOpacity, veinOpacity);
    JL(RareVeins, rareVeins);
    JL(RareSettings, rareSettings);
    JL(GasItems, gasItems);
    JL(GasSpeeds, gasSpeeds);
    JL(UseHeightForBuild, useHeightForBuild);
    JL(IonHeight, ionHeight);
    JL(WaterHeight, waterHeight);
    JL(WaterItemId, waterItemId);
    JL(IceFlag, iceFlag);
    JL(Wind, wind);
}

void from_json(const nlohmann::json &j, ItemProto &p) {
    JL(ID, id);
    JL(Name, name);
    JL(HeatValue, heatValue);
}

void from_json(const nlohmann::json &j, VeinProto &p) {
    JL(ID, id);
    JL(Name, name);
    JL(MiningItem, miningItem);
    JL(ModelCount, modelCount);
    JL(ModelIndex, modelIndex);
}

void from_json(const nlohmann::json &j, StringProto &p) {
    JL(Name, name);
    JL(ZHCN, zhcn);
    JL(ENUS, enus);
    JL(FRFR, frfr);
}

#undef JL

void loadProtoSets() {
    {
        nlohmann::json j;
        std::ifstream ifs("Prototypes/ThemeProtoSet.json");
        ifs >> j;
        j["dataArray"].get_to(themeProtoSet.dataArray);
        themeProtoSet.onLoaded();
    }
    {
        nlohmann::json j;
        std::ifstream ifs("Prototypes/ItemProtoSet.json");
        ifs >> j;
        j["dataArray"].get_to(itemProtoSet.dataArray);
        itemProtoSet.onLoaded();
    }
    {
        nlohmann::json j;
        std::ifstream ifs("Prototypes/VeinProtoSet.json");
        ifs >> j;
        j["dataArray"].get_to(veinProtoSet.dataArray);
        veinProtoSet.onLoaded();
    }
    {
        nlohmann::json j;
        std::ifstream ifs("Prototypes/StringProtoSet.json");
        ifs >> j;
        j["dataArray"].get_to(stringProtoSet.dataArray);
        stringProtoSet.onLoaded();
    }
}

const std::string &translate(const std::string &name, int type) {
    const auto *val = stringProtoSet.select(name);
    if (val) {
        switch (type) {
            case 1:
                return val->enus;
            case 2:
                return val->frfr;
            default:
                return val->zhcn;
        }
    }
    static std::string empty;
    return empty;
}

}
