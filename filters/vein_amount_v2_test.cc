#include "filter.hh"

extern "C" {

static PluginAPIV2 *theAPI = nullptr;

__declspec(dllexport) const char *FILTERAPI init3(PluginAPIV2 *api, int *type) {
    theAPI = api;
    *type = 0;
    return "Vein amount V2 smoke filter";
}

__declspec(dllexport) bool FILTERAPI describePlugin(std::uint32_t version,
                                                     PluginDescriptorV2 *descriptor) {
    if (version != 2) return false;
    descriptor->prePlanetRequirements = NeedsVeinAmounts;
    descriptor->filterRequirements = NeedsVeinAmounts;
    descriptor->capabilities = FilterIsPure | ThreadSafeCallbacks |
                               OrderIndependent | SupportsBlockScheduling;
    return true;
}

__declspec(dllexport) bool FILTERAPI planetFilter(const dspugen::Planet *planet, void *) {
    const auto *summary = theAPI->GetPlanetVeinAmountSummary(planet);
    return summary && summary->baseAmount[1] != 0;
}

}
