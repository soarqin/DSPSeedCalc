#include "galaxy.hh"
#include "planet.hh"
#include "protoset.hh"
#include "settings.hh"

#include <cassert>

int main() {
    dspugen::loadProtoSets();
    dspugen::settings = {};
    dspugen::settings.hasPlanets = true;
    dspugen::Galaxy::initThread();
    dspugen::Star::initThread();
    dspugen::Planet::initThread();
    auto *galaxy = dspugen::Galaxy::create(dspugen::DefaultAlgoVersion, 0, 64);
    assert(galaxy != nullptr);
    assert(galaxy->stars[0]->planets[0]->veinAmountSummary() == nullptr);
    galaxy->ensureAllVeinAmounts();
    auto *summary = galaxy->stars[0]->planets[0]->veinAmountSummary();
    assert(summary != nullptr);
    assert(summary->nodeCount[0] == 0);
    galaxy->release();
    dspugen::Planet::releaseThread();
    dspugen::Star::releaseThread();
    dspugen::Galaxy::releaseThread();
}
