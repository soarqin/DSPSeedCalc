#include "filter.hh"

#include <cstdint>
#include <cstdio>
#include <string>

// Exercises the owned-output path: formatOutput hands records to the host, which forwards
// them to the writer thread and splits them per channel. No shipped filter implements
// formatOutput, so without this plugin that host code has no coverage at all.
extern "C" __declspec(dllexport) const char *FILTERAPI init(PluginAPI *, int *type) {
    *type = 1;
    return "benchmark owned output";
}

extern "C" __declspec(dllexport) void FILTERAPI formatOutput(const dspugen::Galaxy *galaxy,
                                                            OutputSinkV1 *sink) {
    auto header = std::to_string(galaxy->seed) + "," + std::to_string(galaxy->starCount) + "\n";
    sink->append(sink->context, 0, header.data(), header.size());
    // Second channel with a different record size, so a channel mix-up shows up as a
    // length mismatch rather than only as reordered bytes.
    std::string stars;
    for (const auto *star: galaxy->stars) {
        if (star == nullptr) continue;
        stars += std::to_string(galaxy->seed);
        stars += ',';
        stars += std::to_string(star->id);
        stars += ',';
        stars += std::to_string(static_cast<int>(star->spectr));
        stars += '\n';
    }
    if (!stars.empty()) sink->append(sink->context, 1, stars.data(), stars.size());
}

extern "C" __declspec(dllexport) bool FILTERAPI describePlugin(std::uint32_t,
                                                              PluginDescriptorV1 *descriptor) {
    descriptor->outputRequirements = NeedsStars;
    descriptor->capabilities = ThreadSafeCallbacks | OrderIndependent |
                               SupportsBlockScheduling | SupportsOwnedOutput;
    return true;
}
