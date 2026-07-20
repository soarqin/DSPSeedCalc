#include "filter.hh"

#include <atomic>
#include <cstdint>
#include <cstring>

static std::atomic<std::uint64_t> checksum;

extern "C" __declspec(dllexport) const char *FILTERAPI init(PluginAPI *, int *type) {
    *type = 2;
    return "benchmark pose sink";
}

extern "C" __declspec(dllexport) void FILTERAPI pose(int seed, int,
                                 const std::vector<dspugen::VectorLF3> &poses) {
    std::uint64_t value = static_cast<std::uint32_t>(seed);
    for (const auto &point: poses) {
        std::uint64_t bits;
        std::memcpy(&bits, &point.x, sizeof(bits));
        value ^= bits + 0x9e3779b97f4a7c15ULL + (value << 6U) + (value >> 2U);
    }
    checksum.fetch_xor(value, std::memory_order_relaxed);
}

extern "C" __declspec(dllexport) bool FILTERAPI describePlugin(std::uint32_t, PluginDescriptorV1 *descriptor) {
    descriptor->filterRequirements = NeedsPositions;
    descriptor->capabilities = FilterIsPure | ThreadSafeCallbacks |
                               OrderIndependent | SupportsBlockScheduling;
    return true;
}
