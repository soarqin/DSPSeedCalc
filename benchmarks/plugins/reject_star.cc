#include "filter.hh"

extern "C" __declspec(dllexport) const char *FILTERAPI init(PluginAPI *, int *type) {
    *type = 0;
    return "benchmark star reject";
}

extern "C" __declspec(dllexport) bool FILTERAPI starFilter(const dspugen::Star *, void *) {
    return false;
}

extern "C" __declspec(dllexport) bool FILTERAPI prePlanetFilter(const dspugen::Galaxy *galaxy, void *) {
    for (const auto *star: galaxy->stars)
        if (starFilter(star, nullptr)) return true;
    return false;
}

extern "C" __declspec(dllexport) bool FILTERAPI describePlugin(std::uint32_t, PluginDescriptorV1 *descriptor) {
    descriptor->prePlanetRequirements = NeedsStars;
    descriptor->capabilities = FilterIsPure | ThreadSafeCallbacks |
                               OrderIndependent | SupportsBlockScheduling;
    return true;
}
