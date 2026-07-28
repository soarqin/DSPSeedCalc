#pragma once

#include "render_resources.hh"

#include <string>

namespace viewer {

std::string ViewerAssetPath(const char *name);
std::string ViewerRuntimePath(const char *name);
bool LoadRenderResources(RenderResources &resources, const std::string &fontPath);
void UnloadRenderResources(RenderResources &resources);

}
