#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace dspugen {

enum class EVeinType;

inline constexpr std::size_t VeinTypeCount = 15;

struct VeinAmountSummary {
    std::array<std::uint32_t, VeinTypeCount> nodeCount{};
    std::array<std::int64_t, VeinTypeCount> baseAmount{};
};

enum class VeinAmountGenerationResult : std::uint8_t {
    NotGenerated,
    Generated,
    UnsupportedAlgorithm,
    MissingTheme,
    InvalidPlanet,
};

class Planet;

// Generate the lazy, game-style vein summary for one planet.  This entry point
// intentionally reports the first amount written to VeinData (before the game
// description multipliers are applied); callers that only need the cheap vein
// spot estimate should not invoke it.
VeinAmountGenerationResult generateVeinAmountSummary(
    const Planet &planet, VeinAmountSummary &summary);

namespace vein_detail {

bool acceptsVeinCenter(int algorithmId, EVeinType type, float height,
                       float radius, float realRadius);
bool acceptsVeinNode(int algorithmId, int waterItemId, float height,
                     float radius);

}

}
