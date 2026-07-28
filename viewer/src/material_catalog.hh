#pragma once

#include "view_model.hh"

namespace viewer {

float GammaToLinearSpace(float value);
Vector4 GalaxyStarColor(const dspugen::Star *star);
Vector4 GalaxyLineColor(Vector4 color);
Vector4 GalaxyGridColor(Vector4 color);
Vector4 GalaxyStarGradient(float color);
float GalaxyStarScale(const dspugen::Star *star);
StarMassMaterialData StarMassMaterial(const dspugen::Star *star);
StarSpotMaterialData StarSpotMaterial(const dspugen::Star *star);
PlanetThumbMaterialData PlanetThumbMaterial(const dspugen::Planet *planet);

}
