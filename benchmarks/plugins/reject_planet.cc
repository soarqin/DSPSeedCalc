#include "filter.hh"

extern "C" __declspec(dllexport) const char *FILTERAPI init(PluginAPI *, int *type) {
    *type = 0;
    return "benchmark planet reject";
}

extern "C" __declspec(dllexport) bool FILTERAPI planetFilter(const dspugen::Planet *, void *) {
    return false;
}

extern "C" __declspec(dllexport) bool FILTERAPI describePlugin(std::uint32_t, PluginDescriptorV1 *descriptor) {
    descriptor->filterRequirements = NeedsPlanetTopology;
    descriptor->capabilities = FilterIsPure | ThreadSafeCallbacks |
                               OrderIndependent | SupportsBlockScheduling;
    return true;
}
