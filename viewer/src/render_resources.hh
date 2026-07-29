#pragma once

#include <raylib.h>

#include <initializer_list>
#include <string>
#include <unordered_map>

namespace viewer {

struct RenderResources {
    Font font{};
    bool hasCustomFont = false;
    Shader starSurfaceShader{};
    Shader starSpotShader{};
    Shader starBlackShader{};
    Shader planetThumbShader{};
    Shader galaxyStarPointShader{};
    Shader galaxyLineShader{};
    Shader galaxyGridShader{};
    Shader galaxyFrameShader{};
    Shader galaxyBackgroundStarShader{};
    Shader galaxyMarkerShader{};
    Shader starmapOrbitShader{};
    Shader starmapDiskShader{};
    Shader bloomPrefilterShader{};
    Shader bloomDownsampleFirstShader{};
    Shader bloomDownsampleShader{};
    Shader bloomUpsampleShader{};
    Shader bloomCompositeShader{};
    Shader galaxyFxaaShader{};
    Model starMassModel{};
    Model starBlackModel{};
    Model starSpotModel{};
    Material planetThumbMaterial{};
    Material galaxyStarPointMaterial{};
    Material galaxyGridMaterial{};
    Material galaxyFrameMaterial{};
    Material galaxyBackgroundStarMaterial{};
    Material galaxyMarkerMaterial{};
    Material starmapDiskMaterial{};
    Texture2D galaxyStarPointTexture{};
    Texture2D galaxyLineTexture{};
    Texture2D galaxyGridDotTexture{};
    Texture2D galaxyGridLineTexture{};
    Texture2D galaxyGridFrameTexture{};
    Texture2D galaxyBackgroundStarNoise{};
    Texture2D galaxyStarRingTexture{};
    Texture2D starmapOrbitDiskTexture{};
    Texture2D bloomLensDirtTexture{};
    Texture2D galaxyColorGradingLut{};
    // Every official thumb material leaves `_HeightMap` unassigned, so the shader property falls
    // back to Unity's built in black cubemap. The viewer has no terrain to sample, so it binds a
    // 1x1 black cubemap and keeps the same code path rather than inventing a height texture.
    unsigned int planetHeightCubemapId = 0;
    bool hasStarSurfaceShader = false;
    bool hasStarSpotShader = false;
    bool hasStarBlackShader = false;
    bool hasPlanetThumbShader = false;
    bool hasGalaxyStarPointShader = false;
    bool hasGalaxyLineShader = false;
    bool hasGalaxyGridShaders = false;
    bool hasGalaxyBackgroundStarShader = false;
    bool hasGalaxyMarkerShader = false;
    bool hasStarmapOrbitShaders = false;
    bool hasBloomShaders = false;
    bool hasGalaxyFxaaShader = false;
    bool hasStarModels = false;
    bool hasPlanetThumbModel = false;
    bool hasGalaxyStarPointModel = false;
    std::unordered_map<std::string, int> uniformLocations;

    int Uniform(Shader shader, const char *name) const;
    void SetVector3(Shader shader, const char *name, Vector3 value) const;
    void SetFloat(Shader shader, const char *name, float value) const;
    void SetVector4(Shader shader, const char *name, Vector4 value) const;
    void SetMatrix(Shader shader, const char *name, Matrix value) const;
    void SetSourceTexelSize(Shader shader, const Texture2D &texture, float scale = 1.0f) const;
};

void CacheShaderUniforms(RenderResources &resources, Shader shader, std::initializer_list<const char *> names);
void ConfigureStarShader(Shader &shader, bool hasNormal, bool hasTexcoord);
void ConfigurePlanetThumbShader(Shader &shader);
void ConfigureGalaxyStarPointShader(Shader &shader);
void ConfigureGalaxyLineShader(Shader &shader);
void ConfigureGalaxyGridShader(Shader &shader, bool hasTexcoord);
void ConfigureScreenShader(Shader &shader);

}
