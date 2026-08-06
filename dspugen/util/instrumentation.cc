#include "instrumentation.hh"

#ifdef DSP_ENABLE_INSTRUMENTATION

#include <fstream>
#include <algorithm>
#include <mutex>

namespace dspugen::util::instrumentation {

static thread_local Counters threadCounters;
static Counters totalCounters;
static std::mutex countersMutex;

Counters &local() {
    return threadCounters;
}

void resetThread() {
    threadCounters = {};
}

void mergeThread() {
    std::lock_guard lock(countersMutex);
#define ADD_COUNTER(name) totalCounters.name += threadCounters.name
    ADD_COUNTER(positionCandidates);
    ADD_COUNTER(unitSphereRejects);
    ADD_COUNTER(collisionRejects);
    ADD_COUNTER(collisionComparisons);
    ADD_COUNTER(acceptedPositions);
    ADD_COUNTER(poolFreshAllocations);
    ADD_COUNTER(poolReuseAllocations);
    totalCounters.poolFreeListHighWater =
        std::max(totalCounters.poolFreeListHighWater, threadCounters.poolFreeListHighWater);
    ADD_COUNTER(planetsCreated);
    ADD_COUNTER(gasEntriesGenerated);
    ADD_COUNTER(rareVeinRolls);
    ADD_COUNTER(terrainHeightQueries);
    ADD_COUNTER(terrainSparseBatches);
    ADD_COUNTER(terrainVertexEvaluations);
    ADD_COUNTER(themeRowsExamined);
    ADD_COUNTER(prototypeLookups);
    ADD_COUNTER(pluginCallbacks);
    ADD_COUNTER(pluginCallbackNanoseconds);
    ADD_COUNTER(rngConstructions);
    ADD_COUNTER(rngCalls);
    totalCounters.rngTraceHash ^= threadCounters.rngTraceHash;
#undef ADD_COUNTER
    threadCounters = {};
}

void writeReport(const std::string &filename) {
    std::lock_guard lock(countersMutex);
    std::ofstream output(filename);
    output << "{\n"
           << "  \"positionCandidates\": " << totalCounters.positionCandidates << ",\n"
           << "  \"unitSphereRejects\": " << totalCounters.unitSphereRejects << ",\n"
           << "  \"collisionRejects\": " << totalCounters.collisionRejects << ",\n"
           << "  \"collisionComparisons\": " << totalCounters.collisionComparisons << ",\n"
           << "  \"acceptedPositions\": " << totalCounters.acceptedPositions << ",\n"
           << "  \"poolFreshAllocations\": " << totalCounters.poolFreshAllocations << ",\n"
           << "  \"poolReuseAllocations\": " << totalCounters.poolReuseAllocations << ",\n"
           << "  \"poolFreeListHighWater\": " << totalCounters.poolFreeListHighWater << ",\n"
           << "  \"planetsCreated\": " << totalCounters.planetsCreated << ",\n"
           << "  \"gasEntriesGenerated\": " << totalCounters.gasEntriesGenerated << ",\n"
           << "  \"rareVeinRolls\": " << totalCounters.rareVeinRolls << ",\n"
           << "  \"terrainHeightQueries\": " << totalCounters.terrainHeightQueries << ",\n"
           << "  \"terrainSparseBatches\": " << totalCounters.terrainSparseBatches << ",\n"
           << "  \"terrainVertexEvaluations\": " << totalCounters.terrainVertexEvaluations << ",\n"
           << "  \"themeRowsExamined\": " << totalCounters.themeRowsExamined << ",\n"
           << "  \"prototypeLookups\": " << totalCounters.prototypeLookups << ",\n"
           << "  \"pluginCallbacks\": " << totalCounters.pluginCallbacks << ",\n"
           << "  \"pluginCallbackNanoseconds\": " << totalCounters.pluginCallbackNanoseconds << ",\n"
           << "  \"rngConstructions\": " << totalCounters.rngConstructions << ",\n"
           << "  \"rngCalls\": " << totalCounters.rngCalls << ",\n"
           << "  \"rngTraceHash\": " << totalCounters.rngTraceHash << "\n"
           << "}\n";
}

} // namespace dspugen::util::instrumentation

#endif
