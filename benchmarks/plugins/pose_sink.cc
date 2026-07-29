#include "filter.hh"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>

// Order-independent bit-level digest of every generated coordinate. Reporting it at
// unload turns this benchmark sink into a broad position-parity check: two builds that
// agree on the digest over a large seed range agree on every coordinate bit pattern,
// which covers far more ground than the two fixture seeds in tests/oracle.cc.
static std::atomic<std::uint64_t> checksum;
static std::atomic<std::uint64_t> pointCount;

static inline std::uint64_t mix(std::uint64_t value, double coordinate) {
    std::uint64_t bits;
    std::memcpy(&bits, &coordinate, sizeof(bits));
    return value ^ (bits + 0x9e3779b97f4a7c15ULL + (value << 6U) + (value >> 2U));
}

extern "C" __declspec(dllexport) const char *FILTERAPI init(PluginAPI *, int *type) {
    *type = 2;
    return "benchmark pose sink";
}

extern "C" __declspec(dllexport) void FILTERAPI uninit() {
    std::fprintf(stderr, "pose_sink checksum=%016llx points=%llu\n",
                 static_cast<unsigned long long>(checksum.load(std::memory_order_relaxed)),
                 static_cast<unsigned long long>(pointCount.load(std::memory_order_relaxed)));
}

extern "C" __declspec(dllexport) void FILTERAPI pose(int seed, int,
                                 const std::vector<dspugen::VectorLF3> &poses) {
    std::uint64_t value = static_cast<std::uint32_t>(seed);
    for (const auto &point: poses) {
        value = mix(value, point.x);
        value = mix(value, point.y);
        value = mix(value, point.z);
    }
    checksum.fetch_xor(value, std::memory_order_relaxed);
    pointCount.fetch_add(poses.size(), std::memory_order_relaxed);
}

extern "C" __declspec(dllexport) bool FILTERAPI describePlugin(std::uint32_t, PluginDescriptorV1 *descriptor) {
    descriptor->filterRequirements = NeedsPositions;
    descriptor->capabilities = FilterIsPure | ThreadSafeCallbacks |
                               OrderIndependent | SupportsBlockScheduling;
    return true;
}
