#pragma once

#include "planet_raw_data.hh"

#include <cstddef>

namespace dspugen::terrain {

// The first terrain pass only covers algorithms whose managed implementation
// has been ported.  Unsupported IDs must be reported to the caller rather than
// leaving an apparently valid, zero-filled terrain buffer behind.
enum class TerrainGenerationResult {
    Generated,
    UnsupportedAlgorithm,
};

struct PlanetAlgorithmContext {
    PlanetRawData &data;
    int seed;
    float radius;
    // Exact vein summaries only read heightData.  The full terrain oracle keeps
    // this enabled so heightData and biomoData can still be compared byte for
    // byte with the game.
    bool generateBiomes = true;
    // Null generates the complete precision-derived grid.  A non-null list is
    // used by the lazy vein path to evaluate only vertices queryHeight needs.
    const int *indices = nullptr;
    std::size_t indexCount = 0;
};

TerrainGenerationResult dispatchTerrain(int algorithmId,
                                        PlanetAlgorithmContext &context,
                                        double modX,
                                        double modY);

}
