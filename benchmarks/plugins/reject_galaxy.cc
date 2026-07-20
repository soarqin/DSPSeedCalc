#include "filter.hh"

extern "C" __declspec(dllexport) const char *FILTERAPI init(PluginAPI *, int *type) {
    *type = 0;
    return "benchmark constant reject";
}

extern "C" __declspec(dllexport) bool FILTERAPI galaxyFilter(const dspugen::Galaxy *, void *) {
    return false;
}

extern "C" __declspec(dllexport) bool FILTERAPI prePlanetFilter(const dspugen::Galaxy *, void *) {
    return false;
}

extern "C" __declspec(dllexport) bool FILTERAPI describePlugin(std::uint32_t, PluginDescriptorV1 *descriptor) {
    descriptor->prePlanetRequirements = NeedsStars;
    descriptor->capabilities = FilterIsPure | ThreadSafeCallbacks |
                               OrderIndependent | SupportsBlockScheduling;
    return true;
}
