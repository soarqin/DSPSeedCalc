#pragma once

#include <cstdint>
#include <string>

namespace dspugen::util::instrumentation {

struct Counters {
    std::uint64_t positionCandidates = 0;
    std::uint64_t unitSphereRejects = 0;
    std::uint64_t collisionRejects = 0;
    std::uint64_t collisionComparisons = 0;
    std::uint64_t acceptedPositions = 0;
    std::uint64_t poolFreshAllocations = 0;
    std::uint64_t poolReuseAllocations = 0;
    std::uint64_t poolFreeListHighWater = 0;
    std::uint64_t planetsCreated = 0;
    std::uint64_t gasEntriesGenerated = 0;
    std::uint64_t themeRowsExamined = 0;
    std::uint64_t prototypeLookups = 0;
    std::uint64_t pluginCallbacks = 0;
    std::uint64_t pluginCallbackNanoseconds = 0;
    std::uint64_t rngConstructions = 0;
    std::uint64_t rngCalls = 0;
    std::uint64_t rngTraceHash = 1469598103934665603ULL;
};

#ifdef DSP_ENABLE_INSTRUMENTATION
Counters &local();
void resetThread();
void mergeThread();
void writeReport(const std::string &filename);
#else
inline Counters &local() {
    static thread_local Counters counters;
    return counters;
}
inline void resetThread() {}
inline void mergeThread() {}
inline void writeReport(const std::string &) {}
#endif

} // namespace dspugen::util::instrumentation

#ifdef DSP_ENABLE_INSTRUMENTATION
#define DSP_INSTRUMENT(statement) do { statement; } while (false)
#else
#define DSP_INSTRUMENT(statement) do {} while (false)
#endif
