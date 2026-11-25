#include "view_model_builder.hh"

#include "util/dotnet35random.hh"

#include <raymath.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>

namespace viewer {
namespace {

const char *SpectrName(dspugen::ESpectrType spectr) {
    switch (spectr) {
        case dspugen::ESpectrType::M: return "M";
        case dspugen::ESpectrType::K: return "K";
        case dspugen::ESpectrType::G: return "G";
        case dspugen::ESpectrType::F: return "F";
        case dspugen::ESpectrType::A: return "A";
        case dspugen::ESpectrType::B: return "B";
        case dspugen::ESpectrType::O: return "O";
        default: return "X";
    }
}

const char *PlanetTypeName(dspugen::EPlanetType type) {
    switch (type) {
        case dspugen::EPlanetType::Vocano: return "Volcanic";
        case dspugen::EPlanetType::Ocean: return "Ocean";
        case dspugen::EPlanetType::Desert: return "Desert";
        case dspugen::EPlanetType::Ice: return "Ice";
        case dspugen::EPlanetType::Gas: return "Gas giant";
        default: return "Unknown";
    }
}

std::string ToRoman(int value) {
    static constexpr struct {
        int value;
        const char *text;
    } numerals[] = {
        {100, "C"}, {90, "XC"}, {50, "L"}, {40, "XL"},
        {10, "X"}, {9, "IX"}, {5, "V"}, {4, "IV"}, {1, "I"},
    };
    std::string result;
    for (const auto &numeral: numerals) {
        while (value >= numeral.value) {
            result += numeral.text;
            value -= numeral.value;
        }
    }
    return result;
}

std::string SingularityName(int singularity) {
    std::string result;
    auto append = [&result](const char *name) {
        if (!result.empty()) result += ", ";
        result += name;
    };
    if ((singularity & dspugen::EPlanetSingularity::TidalLocked) != 0) append("tidal locked");
    if ((singularity & dspugen::EPlanetSingularity::TidalLocked2) != 0) append("2:1 resonance");
    if ((singularity & dspugen::EPlanetSingularity::TidalLocked4) != 0) append("4:1 resonance");
    if ((singularity & dspugen::EPlanetSingularity::LaySide) != 0) append("lay side");
    if ((singularity & dspugen::EPlanetSingularity::ClockwiseRotate) != 0) append("clockwise");
    if ((singularity & dspugen::EPlanetSingularity::MultipleSatellites) != 0) append("multiple satellites");
    return result.empty() ? "none" : result;
}

const char *ThemeName(int theme) {
    static const char *names[] = {
        "Unknown", "Mediterranean", "Gas Giant", "Gas Giant", "Ice Giant", "Ice Giant",
        "Arid Desert", "Ashen Gelisol", "Oceanic Jungle", "Lava", "Ice Field Gelisol",
        "Barren Desert", "Gobi", "Volcanic Ash", "Red Stone", "Prairie", "Waterworld",
        "Rocky Salt Lake", "Sakura Ocean", "Hurricane Stone Forest", "Scarlet Ice Lake",
        "Gas Giant", "Savanna", "Crystal Desert", "Frozen Tundra", "Pandora Swamp",
    };
    return theme > 0 && theme < static_cast<int>(sizeof(names) / sizeof(names[0])) ? names[theme] : names[0];
}

const char *VeinName(int veinType) {
    static const char *names[] = {
        "", "Iron", "Copper", "Silicon", "Titanium", "Stone",
        "Coal", "Crude oil", "Fire ice", "Kimberlite", "Fractal silicon",
        "Organic crystal", "Optical grating", "Spiniform crystal",
        "Unipolar magnet",
    };
    return veinType > 0 && veinType < static_cast<int>(sizeof(names) / sizeof(names[0])) ? names[veinType] : "Unknown";
}

std::string FormatInteger(std::int64_t value) {
    std::string digits = std::to_string(value);
    const bool negative = !digits.empty() && digits.front() == '-';
    const std::size_t firstDigit = negative ? 1 : 0;
    for (std::size_t position = digits.size(); position > firstDigit + 3;) {
        position -= 3;
        digits.insert(position, ",");
    }
    return digits;
}

template<typename... Args>
std::string FormatString(const char *format, Args... args) {
    int length = std::snprintf(nullptr, 0, format, args...);
    if (length <= 0) return {};
    std::string result(static_cast<std::size_t>(length), '\0');
    std::snprintf(result.data(), result.size() + 1, format, args...);
    return result;
}

struct MineralTotals {
    std::array<std::uint64_t, dspugen::VeinTypeCount> pileCount{};
    std::array<std::int64_t, dspugen::VeinTypeCount> exactAmount{};
    bool complete = true;
    dspugen::VeinAmountGenerationResult firstFailure =
        dspugen::VeinAmountGenerationResult::Generated;
};

void AddPlanetMinerals(const dspugen::Planet *planet, MineralTotals &totals) {
    const auto result = planet->veinAmountGenerationResult();
    const auto *summary = planet->veinAmountSummary();
    if (result != dspugen::VeinAmountGenerationResult::Generated || summary == nullptr) {
        if (totals.complete) {
            totals.firstFailure = result == dspugen::VeinAmountGenerationResult::Generated
                                      ? dspugen::VeinAmountGenerationResult::InternalError
                                      : result;
        }
        totals.complete = false;
        return;
    }
    for (std::size_t i = 1; i < dspugen::VeinTypeCount; ++i) {
        totals.pileCount[i] += summary->nodeCount[i];
        totals.exactAmount[i] += summary->baseAmount[i];
    }
}

std::vector<MineralPanelRow> CreateMineralRows(const MineralTotals &totals) {
    std::vector<MineralPanelRow> rows;
    for (std::size_t i = 1; i < dspugen::VeinTypeCount; ++i) {
        if (totals.pileCount[i] == 0 && totals.exactAmount[i] == 0) continue;
        rows.push_back({VeinName(static_cast<int>(i)),
                        FormatInteger(static_cast<std::int64_t>(totals.pileCount[i])),
                        FormatInteger(totals.exactAmount[i])});
    }
    return rows;
}

std::string StarSubtitle(const dspugen::Star *star) {
    if (star->type == dspugen::EStarType::MainSeqStar)
        return std::string(SpectrName(star->spectr)) + "-type main sequence star";
    return star->typeName();
}

std::string StarTypeShort(const dspugen::Star *star) {
    switch (star->type) {
        case dspugen::EStarType::GiantStar: return std::string("Giant ") + SpectrName(star->spectr);
        case dspugen::EStarType::WhiteDwarf: return "White dwarf";
        case dspugen::EStarType::NeutronStar: return "Neutron";
        case dspugen::EStarType::BlackHole: return "Black hole";
        default: return SpectrName(star->spectr);
    }
}

Color StarSwatchColor(const dspugen::Star *star) {
    switch (star->type) {
        case dspugen::EStarType::WhiteDwarf: return Color {226, 236, 255, 255};
        case dspugen::EStarType::NeutronStar: return Color {120, 230, 255, 255};
        case dspugen::EStarType::BlackHole: return Color {170, 120, 255, 255};
        default: break;
    }
    switch (star->spectr) {
        case dspugen::ESpectrType::M: return Color {255, 112, 76, 255};
        case dspugen::ESpectrType::K: return Color {255, 168, 84, 255};
        case dspugen::ESpectrType::G: return Color {255, 220, 110, 255};
        case dspugen::ESpectrType::F: return Color {255, 244, 200, 255};
        case dspugen::ESpectrType::A: return Color {236, 244, 255, 255};
        case dspugen::ESpectrType::B: return Color {150, 200, 255, 255};
        case dspugen::ESpectrType::O: return Color {100, 160, 255, 255};
        default: return Color {200, 200, 210, 255};
    }
}

const char *WaterName(int itemId) {
    switch (itemId) {
        case 0: return "none";
        case 1000: return "Water";
        case 1116: return "Sulfuric acid";
        default: return "other";
    }
}

const char *GasRowName(int itemId) {
    switch (itemId) {
        case 1120: return "hydrogen rate";
        case 1121: return "deuterium rate";
        case 1011: return "fire ice rate";
        default: return "gas rate";
    }
}

void SetMineralStatus(PanelData &panel, const MineralTotals &totals) {
    panel.mineralTitle = "MINERALS";
    if (!totals.complete) {
        panel.mineralStatus =
            std::string("Exact calculation unavailable: ") +
            dspugen::veinAmountResultName(totals.firstFailure);
    }
}

}

Color PlanetColor(dspugen::EPlanetType type) {
    switch (type) {
        case dspugen::EPlanetType::Vocano: return Color {224, 78, 46, 255};
        case dspugen::EPlanetType::Ocean: return Color {45, 152, 224, 255};
        case dspugen::EPlanetType::Desert: return Color {214, 155, 75, 255};
        case dspugen::EPlanetType::Ice: return Color {156, 214, 232, 255};
        case dspugen::EPlanetType::Gas: return Color {218, 132, 202, 255};
        default: return Color {160, 160, 175, 255};
    }
}

std::string PlanetName(const dspugen::Planet *planet) {
    int displayNumber = planet->index + 1;
    return planet->star->name + " " +
           (planet->star->planets.size() > 20 ? std::to_string(displayNumber) : ToRoman(displayNumber));
}

PanelData CreateStarPanel(const dspugen::Star *star, int galaxySeed) {
    PanelData panel;
    panel.title = "STAR PARAMETERS";
    panel.name = star->name;
    panel.subtitle = StarSubtitle(star);
    panel.rows = {
        {"galaxy seed", std::to_string(galaxySeed)}, {"star ID", std::to_string(star->id)},
        {"generation seed", std::to_string(star->seed)}, {"spectrum", SpectrName(star->spectr)},
        {"position X", FormatString("%.3f", star->position.x)}, {"position Y", FormatString("%.3f", star->position.y)},
        {"position Z", FormatString("%.3f", star->position.z)}, {"mass", FormatString("%.4f solar", star->mass)},
        {"temperature", FormatString("%.0f K", star->temperature)},
        {"luminosity", FormatString("%.4f raw", star->luminosity)},
        {"radius", FormatString("%.4f solar", star->radius)},
        {"habitable radius", FormatString("%.4f AU", star->habitableRadius)},
        {"orbit scaler", FormatString("%.4f", star->orbitScaler)},
        {"dyson radius", FormatString("%.4f AU", star->dysonRadius)},
        {"planets", std::to_string(star->planets.size())},
    };
    MineralTotals minerals;
    for (const auto *planet: star->planets) AddPlanetMinerals(planet, minerals);
    SetMineralStatus(panel, minerals);
    panel.mineralTitle = "MINERALS - ALL PLANETS";
    panel.mineralRows = CreateMineralRows(minerals);
    return panel;
}

PanelData CreatePlanetPanel(const dspugen::Planet *planet, const std::string &name) {
    PanelData panel;
    panel.title = "PLANET PARAMETERS";
    panel.name = name;
    panel.subtitle = PlanetTypeName(planet->type);
    panel.rows = {
        {"star", planet->star->name},
        {"planet ID", std::to_string(planet->id)},
        {"generation seed", std::to_string(planet->seed)},
        {"orbit parent", planet->orbitAroundPlanet == nullptr ? "star" : PlanetName(planet->orbitAroundPlanet)},
        {"orbit radius", FormatString("%.4f AU", planet->orbitRadius)},
        {"sun distance", FormatString("%.4f AU", planet->sunDistance)},
        {"real radius", FormatString("%.1f km", planet->realRadius())},
        {"luminosity", FormatString("%.2f", planet->luminosity)}, {"theme", ThemeName(planet->theme)},
    };
    if (planet->type == dspugen::EPlanetType::Gas) {
        for (std::size_t i = 0; i < planet->gasItems.size() && i < planet->gasSpeeds.size(); ++i)
            panel.rows.push_back({GasRowName(planet->gasItems[i]), FormatString("%.4f", planet->gasSpeeds[i])});
    } else {
        panel.rows.push_back({"ocean", WaterName(planet->waterItemId)});
    }
    panel.rows.push_back({"singularities", SingularityName(planet->singularity)});
    MineralTotals minerals;
    AddPlanetMinerals(planet, minerals);
    SetMineralStatus(panel, minerals);
    panel.mineralRows = CreateMineralRows(minerals);
    return panel;
}

BodyListEntry CreateStarListEntry(const dspugen::Star *star) {
    BodyListEntry entry;
    entry.label = star->name;
    entry.detail = StarTypeShort(star) + "  " + std::to_string(star->planets.size()) + "p";
    if (star->index == 0) entry.detail = "birth  " + entry.detail;
    entry.swatch = StarSwatchColor(star);
    return entry;
}

BodyListEntry CreatePlanetListEntry(const dspugen::Planet *planet, const std::string &name) {
    BodyListEntry entry;
    entry.label = name;
    entry.detail = ThemeName(planet->theme);
    entry.swatch = PlanetColor(planet->type);
    entry.indent = planet->orbitAroundPlanet == nullptr ? 0 : 1;
    return entry;
}

std::vector<int> ReplayInfoSeeds(const dspugen::Star *star) {
    dspugen::util::DotNet35Random rng(star->seed);
    rng.next(); rng.next(); rng.next();
    dspugen::util::DotNet35Random planetRng(rng.next());
    for (int i = 0; i < 7; ++i) planetRng.nextDouble();
    std::vector<int> seeds;
    seeds.reserve(star->planets.size());
    if (star->type != dspugen::EStarType::MainSeqStar) {
        for (std::size_t i = 0; i < star->planets.size(); ++i) {
            seeds.push_back(planetRng.next());
            planetRng.next();
        }
        return seeds;
    }
    int orbitIndex = 1;
    for (std::size_t i = 0; i < star->planets.size(); ++i) {
        seeds.push_back(planetRng.next());
        planetRng.next(); planetRng.nextDouble(); planetRng.nextDouble();
        const auto *planet = star->planets[i];
        if (planet->orbitAround == 0) {
            bool gasGiant = planet->type == dspugen::EPlanetType::Gas && i + 1 < star->planets.size();
            if (star->index != 0 && gasGiant && orbitIndex < 3) orbitIndex = 3;
            while (true) {
                if (star->index == 0 && orbitIndex == 3) break;
                int remaining = static_cast<int>(star->planets.size() - i);
                int slots = 9 - orbitIndex;
                if (slots <= remaining) break;
                float chance = static_cast<float>(remaining) / static_cast<float>(slots);
                chance = orbitIndex <= 3 ? chance + (1.0f - chance) * 0.15f + 0.01f
                                         : chance + (1.0f - chance) * 0.45f + 0.01f;
                if (planetRng.nextDouble() < chance) break;
                ++orbitIndex;
            }
        }
        ++orbitIndex;
    }
    return seeds;
}

OrbitParameters CreateOrbitParameters(const dspugen::Star *star, const dspugen::Planet *planet, int infoSeed) {
    dspugen::util::DotNet35Random rng(infoSeed);
    double num2 = rng.nextDouble();
    double num3 = rng.nextDouble();
    double num4 = rng.nextDouble();
    double num5 = rng.nextDouble();
    double num6 = rng.nextDouble();
    OrbitParameters result;
    result.inclination = static_cast<float>(num4 * 16.0 - 8.0);
    if (planet->orbitAround > 0) result.inclination *= 2.2f;
    result.longitude = static_cast<float>(num5 * 360.0);
    if (star->type >= dspugen::EStarType::NeutronStar)
        result.inclination += result.inclination > 0.0f ? 3.0f : -3.0f;
    result.phase = static_cast<float>(num6 * 360.0);
    double radius = planet->orbitRadius;
    result.period = std::sqrt(39.478417604357432 * radius * radius * radius /
                              (planet->orbitAroundPlanet == nullptr
                                   ? 1.3538551990520382E-06 * star->mass
                                   : 1.0830842106853677E-08));
    (void)num2;
    (void)num3;
    return result;
}

Matrix PlanetOrbitRotation(float inclination, float longitude) {
    return MatrixMultiply(MatrixRotateY(longitude), MatrixRotateZ(inclination));
}

Vector3 OrbitPoint(Vector3 center, float radius, float angle, Matrix rotation) {
    Vector3 point {std::cos(angle) * radius, 0.0f, std::sin(angle) * radius};
    return center + Vector3Transform(point, rotation);
}

}
