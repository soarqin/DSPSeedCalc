/*
 * Copyright (c) 2022 Soar Qin<soarchin@gmail.com>
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#include "protoset.hh"
#include "util/instrumentation.hh"

#include <nlohmann/json.hpp>
#include <fstream>

namespace dspugen {

ThemeProtoSet themeProtoSet;
ItemProtoSet itemProtoSet;
VeinProtoSet veinProtoSet;

void ThemeProtoSet::onLoaded() {
    ProtoSet<ThemeProto>::onLoaded();
    denseIndices.clear();
    sparseIndices.clear();
    birthThemes.clear();
    desertThemes.clear();
    for (auto &indices: birthDefaultByType) indices.clear();
    for (auto &indices: interstellarByType) indices.clear();

    int maxId = -1;
    for (const auto &theme: dataArray)
        if (theme.id >= 0 && theme.id > maxId) maxId = theme.id;
    denseIndices.resize(static_cast<std::size_t>(maxId + 1), -1);
    for (std::size_t i = 0; i < dataArray.size(); ++i) {
        const auto &theme = dataArray[i];
        if (theme.id >= 0)
            denseIndices[static_cast<std::size_t>(theme.id)] = static_cast<int>(i);
        else
            sparseIndices[theme.id] = static_cast<int>(i);
        if (theme.distribute == EThemeDistribute::Birth) birthThemes.emplace_back(static_cast<int>(i));
        if (theme.planetType >= 0 && theme.planetType < static_cast<int>(birthDefaultByType.size())) {
            if (theme.distribute == EThemeDistribute::Default)
                birthDefaultByType[static_cast<std::size_t>(theme.planetType)].emplace_back(static_cast<int>(i));
            if (theme.distribute == EThemeDistribute::Default ||
                theme.distribute == EThemeDistribute::Interstellar)
                interstellarByType[static_cast<std::size_t>(theme.planetType)].emplace_back(static_cast<int>(i));
        }
        if (theme.planetType == 3) desertThemes.emplace_back(static_cast<int>(i));
    }
}

const ThemeProto *ThemeProtoSet::select(int id) const {
    DSP_INSTRUMENT(++util::instrumentation::local().prototypeLookups);
    if (id < 0) {
        auto found = sparseIndices.find(id);
        return found == sparseIndices.end() ? nullptr : &dataArray[found->second];
    }
    if (static_cast<std::size_t>(id) >= denseIndices.size()) return nullptr;
    auto index = denseIndices[static_cast<std::size_t>(id)];
    return index < 0 ? nullptr : &dataArray[static_cast<std::size_t>(index)];
}

const std::vector<int> &ThemeProtoSet::candidates(bool birthStar, int planetType) const {
    static const std::vector<int> empty;
    if (planetType < 0 || planetType >= static_cast<int>(birthDefaultByType.size())) return empty;
    return birthStar ? birthDefaultByType[static_cast<std::size_t>(planetType)]
                     : interstellarByType[static_cast<std::size_t>(planetType)];
}

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
}

}
