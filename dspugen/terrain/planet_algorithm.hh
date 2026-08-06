#pragma once

#include "planet_raw_data.hh"

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
};

TerrainGenerationResult generateTerrain(int algorithmId,
                                         PlanetAlgorithmContext &context,
                                         double modX,
                                         double modY);

inline TerrainGenerationResult dispatchTerrain(int algorithmId,
                                                PlanetAlgorithmContext &context,
                                                double modX,
                                                double modY) {
    return generateTerrain(algorithmId, context, modX, modY);
}

}
