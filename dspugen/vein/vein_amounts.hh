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
    InvalidThemeData,
    ServiceUnavailable,
    InternalError,
};

// Terrain fingerprint of the buffers the vein pass ran against.  Only the
// parity oracle reads it: it separates "the terrain differs" from "the terrain
// matches but placement differs" when comparing against a dump taken from the
// running game.
struct TerrainDigest {
    std::uint64_t height = 0;
    std::uint64_t biomo = 0;
};

// Stable, allocation-free description of a result.  Used to build host-side
// diagnostics without forcing every caller to duplicate the switch.
const char *veinAmountResultName(VeinAmountGenerationResult result);

// Outcome of a whole-galaxy pass.  The generator never throws: it is reachable
// from the plugin C ABI, and a C++ exception must not cross that boundary.
struct VeinAmountBatchResult {
    VeinAmountGenerationResult result = VeinAmountGenerationResult::Generated;
    int planetId = 0;
    int algoId = 0;

    [[nodiscard]] bool ok() const {
        return result == VeinAmountGenerationResult::Generated;
    }
};

class Planet;

// Generate the lazy, game-style vein summary for one planet.  This entry point
// intentionally reports the first amount written to VeinData (before the game
// description multipliers are applied); callers that only need the cheap vein
// spot estimate should not invoke it. includeTerrainDigest materializes the
// complete height and biome buffers for diagnostic parity checks.
VeinAmountGenerationResult generateVeinAmountSummary(
    const Planet &planet, VeinAmountSummary &summary, TerrainDigest &digest,
    bool includeTerrainDigest = false);

namespace vein_detail {

bool acceptsVeinCenter(int algorithmId, EVeinType type, float height,
                       float radius, float realRadius);
bool acceptsVeinNode(int algorithmId, int waterItemId, float height,
                     float radius);

}

}
