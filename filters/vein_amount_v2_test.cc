#include "filter.hh"

#include <cstdlib>

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
    descriptor->filterRequirements = NeedsPlanetTopology | NeedsPlanetTheme |
                                     NeedsVeinMetadata;
    if (!std::getenv("DSP_TEST_UNDECLARED_VEIN_AMOUNTS"))
        descriptor->filterRequirements |= NeedsVeinAmounts;
    descriptor->capabilities = ThreadSafeCallbacks | OrderIndependent |
                               SupportsBlockScheduling;
    return true;
}

__declspec(dllexport) bool FILTERAPI planetFilter(const dspugen::Planet *planet, void *) {
    // This is the cheap pass.  Only the birth planet reaches exact terrain and
    // vein placement; every other planet remains without a summary.
    if (!planet->galaxy || planet->id != planet->galaxy->birthPlanetId)
        return false;
    int generatedBefore = 0;
    for (const auto *star: planet->galaxy->stars)
        for (const auto *candidate: star->planets)
            if (theAPI->GetPlanetVeinAmountSummary(candidate)) ++generatedBefore;
    if (generatedBefore != 0) return false;
    if (theAPI->GeneratePlanetVeinAmounts(planet) !=
        dspugen::VeinAmountGenerationResult::Generated)
        return false;
    const auto *summary = theAPI->GetPlanetVeinAmountSummary(planet);
    if (!summary || summary->baseAmount[1] == 0) return false;
    int generatedAfter = 0;
    for (const auto *star: planet->galaxy->stars)
        for (const auto *candidate: star->planets)
            if (theAPI->GetPlanetVeinAmountSummary(candidate)) ++generatedAfter;
    return generatedAfter == 1;
}

}
