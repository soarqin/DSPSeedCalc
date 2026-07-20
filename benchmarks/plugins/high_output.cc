#include "filter.hh"

#include <cstdio>

static FILE *outputFile;

extern "C" __declspec(dllexport) const char *FILTERAPI init(PluginAPI *, int *type) {
    *type = 1;
    outputFile = std::fopen("benchmark-output.csv", "wb");
    return "benchmark high output";
}

extern "C" __declspec(dllexport) void FILTERAPI output(const dspugen::Galaxy *galaxy) {
    std::fprintf(outputFile, "%d,%d\n", galaxy->seed, galaxy->starCount);
}

extern "C" __declspec(dllexport) void FILTERAPI uninit() {
    if (outputFile) std::fclose(outputFile);
}

extern "C" __declspec(dllexport) bool FILTERAPI describePlugin(std::uint32_t, PluginDescriptorV1 *descriptor) {
    descriptor->outputRequirements = NeedsStars;
    descriptor->capabilities = ThreadSafeCallbacks | OrderIndependent |
                               SupportsBlockScheduling;
    return true;
}
