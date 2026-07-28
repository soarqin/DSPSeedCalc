#pragma once

#include "view_model.hh"

#include <raylib.h>

#include <string>
#include <vector>

namespace viewer {

std::vector<int> ReplayInfoSeeds(const dspugen::Star *star);
OrbitParameters CreateOrbitParameters(const dspugen::Star *star, const dspugen::Planet *planet, int infoSeed);
Matrix PlanetOrbitRotation(float inclination, float longitude);
Vector3 OrbitPoint(Vector3 center, float radius, float angle, Matrix rotation);
Color PlanetColor(dspugen::EPlanetType type);
std::string PlanetName(const dspugen::Planet *planet);
PanelData CreateStarPanel(const dspugen::Star *star, int galaxySeed);
PanelData CreatePlanetPanel(const dspugen::Planet *planet, const std::string &name);

}
