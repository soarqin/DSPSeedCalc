#include "app_resources.hh"

#include <raylib.h>
#include <rlgl.h>

#include <algorithm>
#include <cstring>
#include <string>

namespace viewer {

#ifndef DSP_VIEWER_DEFAULT_FONT
#define DSP_VIEWER_DEFAULT_FONT ""
#endif

constexpr int kViewerFontSize = 64;

// rlTextureParameters() already guards the anisotropic extension and clamps to the device limit on
// both desktop GL and WebGL, so route through it rather than touching GL_TEXTURE_MAX_ANISOTROPY.
void SetTextureAnisotropy(const Texture2D &texture, int level) {
    if (texture.id == 0) return;
    rlTextureParameters(texture.id, RL_TEXTURE_FILTER_ANISOTROPIC, level);
}

Texture2D LoadGalaxyLineTexture(const char *path) {
    int size = 0;
    unsigned char *data = LoadFileData(path, &size);
    constexpr int expectedSize = 699056;
    if (data == nullptr || size != expectedSize) {
        if (data != nullptr) UnloadFileData(data);
        return {};
    }
    Texture2D texture;
    texture.id = rlLoadTexture(data, 256, 128, RL_PIXELFORMAT_UNCOMPRESSED_R32G32B32A32, 9);
    texture.width = 256;
    texture.height = 128;
    texture.mipmaps = 9;
    texture.format = PIXELFORMAT_UNCOMPRESSED_R32G32B32A32;
    UnloadFileData(data);
    return texture;
}

Texture2D LoadAlphaMipTexture(const char *path, int width, int height, int mipmaps) {
    int expectedSize = 0;
    for (int level = 0, w = width, h = height; level < mipmaps; ++level) {
        expectedSize += w * h;
        w = w > 1 ? w / 2 : 1;
        h = h > 1 ? h / 2 : 1;
    }
    int size = 0;
    unsigned char *data = LoadFileData(path, &size);
    if (data == nullptr || size != expectedSize) {
        if (data != nullptr) UnloadFileData(data);
        return {};
    }
    Texture2D texture{};
    texture.id = rlLoadTexture(data, width, height, RL_PIXELFORMAT_UNCOMPRESSED_GRAYSCALE, mipmaps);
    texture.width = width;
    texture.height = height;
    texture.mipmaps = mipmaps;
    texture.format = PIXELFORMAT_UNCOMPRESSED_GRAYSCALE;
    UnloadFileData(data);
    return texture;
}

std::string ViewerAssetPath(const char *name) {
#if defined(__EMSCRIPTEN__)
    return std::string("/assets/") + name;
#else
    return std::string(GetApplicationDirectory()) + "assets/" + name;
#endif
}

std::string ViewerRuntimePath(const char *name) {
#if defined(__EMSCRIPTEN__)
    return std::string("/") + name;
#else
    return std::string(GetApplicationDirectory()) + name;
#endif
}

template<typename T>
T *LoadMeshArray(const char *path, int expectedCount) {
    int size = 0;
    unsigned char *data = LoadFileData(path, &size);
    if (data == nullptr || size != expectedCount * static_cast<int>(sizeof(T))) {
        if (data != nullptr) UnloadFileData(data);
        return nullptr;
    }
    T *result = static_cast<T *>(MemAlloc(static_cast<unsigned int>(size)));
    if (result == nullptr) {
        UnloadFileData(data);
        return nullptr;
    }
    std::memcpy(result, data, static_cast<std::size_t>(size));
    UnloadFileData(data);
    return result;
}

Model LoadStarModel(const char *verticesPath, int vertexCount, const char *indicesPath, int triangleCount,
                    const char *normalsPath = nullptr, const char *texcoordsPath = nullptr) {
    Mesh mesh{};
    mesh.vertexCount = vertexCount;
    mesh.triangleCount = triangleCount;
    mesh.vertices = LoadMeshArray<float>(verticesPath, vertexCount * 3);
    mesh.indices = LoadMeshArray<unsigned short>(indicesPath, triangleCount * 3);
    if (normalsPath != nullptr) mesh.normals = LoadMeshArray<float>(normalsPath, vertexCount * 3);
    if (texcoordsPath != nullptr) mesh.texcoords = LoadMeshArray<float>(texcoordsPath, vertexCount * 2);
    if (mesh.vertices == nullptr || mesh.indices == nullptr || (normalsPath != nullptr && mesh.normals == nullptr) ||
        (texcoordsPath != nullptr && mesh.texcoords == nullptr)) {
        UnloadMesh(mesh);
        return {};
    }
    UploadMesh(&mesh, false);
    return LoadModelFromMesh(mesh);
}

void SetModelShader(Model &model, Shader shader) {
    model.materials[0].shader = shader;
    for (int i = 0; i <= MATERIAL_MAP_BRDF; ++i) model.materials[0].maps[i].texture = {};
}

bool StarModelReady(const Model &model) {
    return IsModelValid(model) && model.meshCount == 1 && model.materialCount == 1;
}


bool LoadRenderResources(RenderResources &resources, const std::string &fontPath) {
resources.font = GetFontDefault();
const auto surfaceVertex = ViewerAssetPath("shaders/star_surface.vs");
const auto surfaceFragment = ViewerAssetPath("shaders/star_surface.fs");
const auto spotVertex = ViewerAssetPath("shaders/star_spot.vs");
const auto spotFragment = ViewerAssetPath("shaders/star_spot.fs");
const auto blackVertex = ViewerAssetPath("shaders/star_black.vs");
const auto blackFragment = ViewerAssetPath("shaders/star_black.fs");
const auto planetThumbVertex = ViewerAssetPath("shaders/planet_thumb.vs");
const auto planetThumbFragment = ViewerAssetPath("shaders/planet_thumb.fs");
const auto galaxyPointVertex = ViewerAssetPath("shaders/galaxy_star_point.vs");
const auto galaxyPointFragment = ViewerAssetPath("shaders/galaxy_star_point.fs");
const auto galaxyLineVertex = ViewerAssetPath("shaders/galaxy_line.vs");
const auto galaxyLineFragment = ViewerAssetPath("shaders/galaxy_line.fs");
const auto galaxyGridVertex = ViewerAssetPath("shaders/galaxy_grid.vs");
const auto galaxyGridFragment = ViewerAssetPath("shaders/galaxy_grid.fs");
const auto galaxyFrameVertex = ViewerAssetPath("shaders/galaxy_frame.vs");
const auto galaxyFrameFragment = ViewerAssetPath("shaders/galaxy_frame.fs");
const auto galaxyBackgroundStarVertex = ViewerAssetPath("shaders/galaxy_bgstars.vs");
const auto galaxyBackgroundStarFragment = ViewerAssetPath("shaders/galaxy_bgstars.fs");
const auto galaxyMarkerVertex = ViewerAssetPath("shaders/galaxy_marker.vs");
const auto galaxyMarkerFragment = ViewerAssetPath("shaders/galaxy_marker.fs");
const auto starmapOrbitVertex = ViewerAssetPath("shaders/starmap_orbit.vs");
const auto starmapOrbitFragment = ViewerAssetPath("shaders/starmap_orbit.fs");
const auto starmapDiskVertex = ViewerAssetPath("shaders/starmap_disk.vs");
const auto starmapDiskFragment = ViewerAssetPath("shaders/starmap_disk.fs");
const auto bloomVertex = ViewerAssetPath("shaders/bloom.vs");
const auto bloomPrefilterFragment = ViewerAssetPath("shaders/bloom_prefilter.fs");
const auto bloomDownsampleFirstFragment = ViewerAssetPath("shaders/bloom_downsample_first.fs");
const auto bloomDownsampleFragment = ViewerAssetPath("shaders/bloom_downsample.fs");
const auto bloomUpsampleFragment = ViewerAssetPath("shaders/bloom_upsample.fs");
const auto bloomCompositeFragment = ViewerAssetPath("shaders/bloom_composite.fs");
const auto galaxyFxaaFragment = ViewerAssetPath("shaders/galaxy_fxaa.fs");
resources.starSurfaceShader = LoadShader(surfaceVertex.c_str(), surfaceFragment.c_str());
resources.starSpotShader = LoadShader(spotVertex.c_str(), spotFragment.c_str());
resources.starBlackShader = LoadShader(blackVertex.c_str(), blackFragment.c_str());
resources.planetThumbShader = LoadShader(planetThumbVertex.c_str(), planetThumbFragment.c_str());
resources.galaxyStarPointShader = LoadShader(galaxyPointVertex.c_str(), galaxyPointFragment.c_str());
resources.galaxyLineShader = LoadShader(galaxyLineVertex.c_str(), galaxyLineFragment.c_str());
resources.galaxyGridShader = LoadShader(galaxyGridVertex.c_str(), galaxyGridFragment.c_str());
resources.galaxyFrameShader = LoadShader(galaxyFrameVertex.c_str(), galaxyFrameFragment.c_str());
resources.galaxyBackgroundStarShader =
    LoadShader(galaxyBackgroundStarVertex.c_str(), galaxyBackgroundStarFragment.c_str());
resources.galaxyMarkerShader = LoadShader(galaxyMarkerVertex.c_str(), galaxyMarkerFragment.c_str());
resources.starmapOrbitShader = LoadShader(starmapOrbitVertex.c_str(), starmapOrbitFragment.c_str());
resources.starmapDiskShader = LoadShader(starmapDiskVertex.c_str(), starmapDiskFragment.c_str());
resources.bloomPrefilterShader = LoadShader(bloomVertex.c_str(), bloomPrefilterFragment.c_str());
resources.bloomDownsampleFirstShader = LoadShader(bloomVertex.c_str(), bloomDownsampleFirstFragment.c_str());
resources.bloomDownsampleShader = LoadShader(bloomVertex.c_str(), bloomDownsampleFragment.c_str());
resources.bloomUpsampleShader = LoadShader(bloomVertex.c_str(), bloomUpsampleFragment.c_str());
resources.bloomCompositeShader = LoadShader(bloomVertex.c_str(), bloomCompositeFragment.c_str());
resources.galaxyFxaaShader = LoadShader(bloomVertex.c_str(), galaxyFxaaFragment.c_str());
resources.hasStarSurfaceShader = IsShaderValid(resources.starSurfaceShader) && resources.starSurfaceShader.id != rlGetShaderIdDefault();
resources.hasStarSpotShader = IsShaderValid(resources.starSpotShader) && resources.starSpotShader.id != rlGetShaderIdDefault();
resources.hasStarBlackShader = IsShaderValid(resources.starBlackShader) && resources.starBlackShader.id != rlGetShaderIdDefault();
resources.hasPlanetThumbShader = IsShaderValid(resources.planetThumbShader) && resources.planetThumbShader.id != rlGetShaderIdDefault();
resources.hasGalaxyStarPointShader = IsShaderValid(resources.galaxyStarPointShader) &&
                           resources.galaxyStarPointShader.id != rlGetShaderIdDefault();
resources.hasGalaxyLineShader = IsShaderValid(resources.galaxyLineShader) && resources.galaxyLineShader.id != rlGetShaderIdDefault();
resources.hasGalaxyGridShaders = IsShaderValid(resources.galaxyGridShader) &&
                       resources.galaxyGridShader.id != rlGetShaderIdDefault() &&
                       IsShaderValid(resources.galaxyFrameShader) &&
                       resources.galaxyFrameShader.id != rlGetShaderIdDefault();
resources.hasGalaxyBackgroundStarShader = IsShaderValid(resources.galaxyBackgroundStarShader) &&
                       resources.galaxyBackgroundStarShader.id != rlGetShaderIdDefault();
resources.hasGalaxyMarkerShader = IsShaderValid(resources.galaxyMarkerShader) &&
                       resources.galaxyMarkerShader.id != rlGetShaderIdDefault();
resources.hasStarmapOrbitShaders = IsShaderValid(resources.starmapOrbitShader) &&
                       resources.starmapOrbitShader.id != rlGetShaderIdDefault() &&
                       IsShaderValid(resources.starmapDiskShader) &&
                       resources.starmapDiskShader.id != rlGetShaderIdDefault();
resources.hasBloomShaders = IsShaderValid(resources.bloomPrefilterShader) && IsShaderValid(resources.bloomDownsampleFirstShader) &&
                  IsShaderValid(resources.bloomDownsampleShader) && IsShaderValid(resources.bloomUpsampleShader) &&
                  IsShaderValid(resources.bloomCompositeShader) &&
                  resources.bloomPrefilterShader.id != rlGetShaderIdDefault() &&
                  resources.bloomDownsampleFirstShader.id != rlGetShaderIdDefault() &&
                  resources.bloomDownsampleShader.id != rlGetShaderIdDefault() &&
                  resources.bloomUpsampleShader.id != rlGetShaderIdDefault() &&
                  resources.bloomCompositeShader.id != rlGetShaderIdDefault();
resources.hasGalaxyFxaaShader = IsShaderValid(resources.galaxyFxaaShader) && resources.galaxyFxaaShader.id != rlGetShaderIdDefault();
if (resources.hasStarSurfaceShader) ConfigureStarShader(resources.starSurfaceShader, true, false);
if (resources.hasStarSpotShader) ConfigureStarShader(resources.starSpotShader, false, true);
if (resources.hasStarBlackShader) ConfigureStarShader(resources.starBlackShader, false, false);
if (resources.hasPlanetThumbShader) ConfigurePlanetThumbShader(resources.planetThumbShader);
if (resources.hasGalaxyStarPointShader) ConfigureGalaxyStarPointShader(resources.galaxyStarPointShader);
if (resources.hasGalaxyLineShader) ConfigureGalaxyLineShader(resources.galaxyLineShader);
if (resources.hasGalaxyGridShaders) {
    ConfigureGalaxyGridShader(resources.galaxyGridShader, false);
    ConfigureGalaxyGridShader(resources.galaxyFrameShader, true);
}
if (resources.hasGalaxyBackgroundStarShader)
    ConfigureGalaxyGridShader(resources.galaxyBackgroundStarShader, true);
if (resources.hasGalaxyMarkerShader) ConfigureGalaxyGridShader(resources.galaxyMarkerShader, true);
if (resources.hasStarmapOrbitShaders) {
    // The orbit ribbon is generated in world space like the connection lines, so it wants the same
    // attribute and matrix layout; the disk is an ordinary transformed quad like the grid planes.
    ConfigureGalaxyLineShader(resources.starmapOrbitShader);
    ConfigureGalaxyGridShader(resources.starmapDiskShader, true);
}
if (resources.hasBloomShaders) {
    ConfigureScreenShader(resources.bloomPrefilterShader);
    ConfigureScreenShader(resources.bloomDownsampleFirstShader);
    ConfigureScreenShader(resources.bloomDownsampleShader);
    ConfigureScreenShader(resources.bloomUpsampleShader);
    ConfigureScreenShader(resources.bloomCompositeShader);
}
if (resources.hasGalaxyFxaaShader) ConfigureScreenShader(resources.galaxyFxaaShader);
CacheShaderUniforms(resources, resources.starSurfaceShader,
    {"_Color0_xyz", "_Color1_xyz", "_Color2_xyz", "_Color3_xyz",
     "_Multiplier_and_NoiseTile_and_NoiseSpeed_and_ChaosTile",
     "_ChaosDistort_and_ChaosOverlay_and_SpotIntens_and_RimPower", "_RotSpeed", "_Time",
     "_WorldSpaceCameraPos", "matModel", "matViewProjection"});
CacheShaderUniforms(resources, resources.starSpotShader,
    {"_Multiplier", "_SunGlowColor_xyz", "_SunGlowPower", "_SunCenterColor_xyz",
     "_SunCenterMultiplier_and_SunCenterSize_and_SunCenterBlur_and_SunCenterBlurPower",
     "_Global_WhiteMode0", "matModel", "matViewProjection"});
CacheShaderUniforms(resources, resources.starBlackShader, {"_TintColor", "matModel", "matViewProjection"});
CacheShaderUniforms(resources, resources.planetThumbShader,
    {"matModel", "matViewProjection", "_SunDir", "_Rotation", "_WorldSpaceCameraPos", "_Color",
     "_ShoreLineColor", "_RimColor", "_ColorBio0", "_ColorBio1", "_ColorBio2", "_HeightSettings",
     "_ShoreInvThick", "_ShoreHeight", "_WireIntens", "_BioStrength", "_FarHeight", "_Diameter",
     "_SceneLinear"});
CacheShaderUniforms(resources, resources.starmapOrbitShader,
    {"matViewProjection", "matOrbitRotation", "_WorldSpaceCameraPos", "_Position", "_LineColorA",
     "_LineColorB", "_Multiplier", "_TileScale", "_DistScale", "_DistPower", "_MinDistScale",
     "_MaxDistScale", "_Radius", "_SceneLinear"});
CacheShaderUniforms(resources, resources.starmapDiskShader,
    {"matModel", "matViewProjection", "_MainTex_ST", "_TintColor", "_Multiplier", "_SceneLinear"});
CacheShaderUniforms(resources, resources.galaxyStarPointShader,
    {"matViewProjection", "_MainTex_ST", "_TintColor", "_ScreenParams", "_Global_GalaxyFadeRadius",
     "_SceneLinear", "colDiffuse", "_PointGlow", "_Multiplier", "_AlphaMultiplier"});
CacheShaderUniforms(resources, resources.galaxyLineShader,
    {"matViewProjection", "_WorldSpaceCameraPos", "_Multiplier", "_TileScale", "_DistScale",
     "_DistPower", "_MinDistScale", "_MaxDistScale", "_Global_GalaxyFadeRadius", "_SceneLinear",
     "_ScreenParams", "_LineColorA", "_LineColorB"});
CacheShaderUniforms(resources, resources.galaxyGridShader,
    {"matModel", "matViewProjection", "_TintColor", "_Multiplier", "_AlphaMultiplier",
     "_Tiling", "_AreaMin", "_AreaMax", "_NearFade", "_SceneLinear"});
CacheShaderUniforms(resources, resources.galaxyFrameShader,
    {"matModel", "matViewProjection", "_Color", "_Multiplier", "_SceneLinear"});
CacheShaderUniforms(resources, resources.galaxyMarkerShader,
    {"matModel", "matViewProjection", "_TintColor", "_Multiplier", "_AlphaMultiplier", "_SceneLinear"});
CacheShaderUniforms(resources, resources.galaxyBackgroundStarShader,
    {"matModel", "matViewProjection", "_NoiseMapTexelSize", "_StarSetting", "_RefValueA",
     "_RefValueB", "_StarColor", "_StarColorChange", "_Time", "_SceneLinear"});
CacheShaderUniforms(resources, resources.bloomPrefilterShader, {"sourceTexelSize", "prefilterOffset", "threshold", "curve"});
CacheShaderUniforms(resources, resources.bloomDownsampleFirstShader, {"sourceTexelSize"});
CacheShaderUniforms(resources, resources.bloomDownsampleShader, {"sourceTexelSize"});
CacheShaderUniforms(resources, resources.bloomUpsampleShader, {"sourceTexelSize", "baseTexture"});
CacheShaderUniforms(resources, resources.bloomCompositeShader,
    {"sourceTexelSize", "bloomTexture", "lensDirtTexture", "gradingLutTexture", "intensity", "dirtIntensity"});
CacheShaderUniforms(resources, resources.galaxyFxaaShader,
    {"sourceTexelSize", "qualitySubpix", "qualityEdgeThreshold", "qualityEdgeThresholdMin"});
const auto massVertices = ViewerAssetPath("meshes/star_mass_vertices.bin");
const auto massNormals = ViewerAssetPath("meshes/star_mass_normals.bin");
const auto massIndices = ViewerAssetPath("meshes/star_mass_indices.bin");
const auto blackVertices = ViewerAssetPath("meshes/star_black_vertices.bin");
const auto blackIndices = ViewerAssetPath("meshes/star_black_indices.bin");
const auto quadVertices = ViewerAssetPath("meshes/screen_quad_vertices.bin");
const auto quadTexcoords = ViewerAssetPath("meshes/screen_quad_uv.bin");
const auto quadIndices = ViewerAssetPath("meshes/screen_quad_indices.bin");
const auto galaxyPointTexturePath = ViewerAssetPath("textures/galaxy_star_point.png");
const auto galaxyLineTexturePath = ViewerAssetPath("textures/galaxy_line_texture.bin");
const auto galaxyGridDotPath = ViewerAssetPath("textures/galaxy_grid_dot.bin");
const auto galaxyGridLinePath = ViewerAssetPath("textures/galaxy_grid_line.bin");
const auto galaxyGridFramePath = ViewerAssetPath("textures/galaxy_grid_frame.bin");
const auto galaxyBackgroundStarNoisePath = ViewerAssetPath("textures/galaxy_bgstars_noise.bin");
const auto galaxyStarRingPath = ViewerAssetPath("textures/galaxy_star_ring.bin");
const auto starmapOrbitDiskPath = ViewerAssetPath("textures/starmap_orbit_disk.bin");
const auto bloomLensDirtPath = ViewerAssetPath("textures/LensDirt00.png");
const auto galaxyColorGradingLutPath = ViewerAssetPath("textures/galaxy_select_lut.bin");
resources.starMassModel = LoadStarModel(massVertices.c_str(), 2617, massIndices.c_str(), 5120, massNormals.c_str());
resources.starBlackModel = LoadStarModel(blackVertices.c_str(), 515, blackIndices.c_str(), 768);
resources.starSpotModel = LoadStarModel(quadVertices.c_str(), 4, quadIndices.c_str(), 2, nullptr, quadTexcoords.c_str());
resources.galaxyStarPointTexture = LoadTexture(galaxyPointTexturePath.c_str());
resources.galaxyLineTexture = LoadGalaxyLineTexture(galaxyLineTexturePath.c_str());
resources.galaxyGridDotTexture = LoadAlphaMipTexture(galaxyGridDotPath.c_str(), 256, 256, 9);
resources.galaxyGridLineTexture = LoadAlphaMipTexture(galaxyGridLinePath.c_str(), 256, 256, 9);
resources.galaxyGridFrameTexture = LoadAlphaMipTexture(galaxyGridFramePath.c_str(), 1024, 1024, 11);
resources.galaxyStarRingTexture = LoadAlphaMipTexture(galaxyStarRingPath.c_str(), 256, 256, 9);
// `orbit-disk` is greyscale with alpha 1, so only its red channel is stored; the single channel
// loader is the same one the alpha only textures use.
resources.starmapOrbitDiskTexture = LoadAlphaMipTexture(starmapOrbitDiskPath.c_str(), 512, 512, 10);
const unsigned char blackCubemapFace[24] = {};
resources.planetHeightCubemapId =
    rlLoadTextureCubemap(blackCubemapFace, 1, RL_PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 1);
Image noiseImage = LoadImageRaw(galaxyBackgroundStarNoisePath.c_str(), 128, 128,
                                PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 0);
if (IsImageValid(noiseImage)) {
    resources.galaxyBackgroundStarNoise = LoadTextureFromImage(noiseImage);
    UnloadImage(noiseImage);
}
resources.bloomLensDirtTexture = LoadTexture(bloomLensDirtPath.c_str());
Image gradingLutImage = LoadImageRaw(galaxyColorGradingLutPath.c_str(), 1024, 32,
                                     PIXELFORMAT_UNCOMPRESSED_R32G32B32A32, 0);
if (IsImageValid(gradingLutImage)) {
    resources.galaxyColorGradingLut = LoadTextureFromImage(gradingLutImage);
    UnloadImage(gradingLutImage);
}
resources.hasStarModels = StarModelReady(resources.starMassModel) && StarModelReady(resources.starBlackModel) && StarModelReady(resources.starSpotModel) &&
                resources.hasStarSurfaceShader && resources.hasStarSpotShader && resources.hasStarBlackShader;
resources.hasPlanetThumbModel = StarModelReady(resources.starMassModel) && resources.hasPlanetThumbShader;
resources.hasGalaxyStarPointModel = StarModelReady(resources.starSpotModel) && IsTextureValid(resources.galaxyStarPointTexture) &&
                          resources.hasGalaxyStarPointShader;
if (!resources.hasStarModels || !resources.hasGalaxyStarPointModel || !resources.hasGalaxyLineShader || !resources.hasBloomShaders ||
    !resources.hasGalaxyFxaaShader || !resources.hasGalaxyGridShaders ||
    !resources.hasGalaxyBackgroundStarShader || !IsTextureValid(resources.galaxyBackgroundStarNoise) ||
    !resources.hasGalaxyMarkerShader || !IsTextureValid(resources.galaxyStarRingTexture) ||
    !resources.hasStarmapOrbitShaders || !IsTextureValid(resources.starmapOrbitDiskTexture) ||
    resources.planetHeightCubemapId == 0 ||
    !IsTextureValid(resources.galaxyLineTexture) ||
    !IsTextureValid(resources.galaxyGridDotTexture) || !IsTextureValid(resources.galaxyGridLineTexture) ||
    !IsTextureValid(resources.galaxyGridFrameTexture) ||
    !IsTextureValid(resources.bloomLensDirtTexture) || !IsTextureValid(resources.galaxyColorGradingLut)) {
    return false;
}
SetModelShader(resources.starMassModel, resources.starSurfaceShader);
if (resources.hasPlanetThumbModel) {
    resources.planetThumbMaterial = LoadMaterialDefault();
    resources.planetThumbMaterial.shader = resources.planetThumbShader;
    for (int i = 0; i <= MATERIAL_MAP_BRDF; ++i) resources.planetThumbMaterial.maps[i].texture = {};
    resources.planetThumbMaterial.maps[MATERIAL_MAP_CUBEMAP].texture.id = resources.planetHeightCubemapId;
    resources.planetThumbMaterial.maps[MATERIAL_MAP_CUBEMAP].texture.width = 1;
    resources.planetThumbMaterial.maps[MATERIAL_MAP_CUBEMAP].texture.height = 1;
    resources.planetThumbMaterial.maps[MATERIAL_MAP_CUBEMAP].texture.mipmaps = 1;
    resources.planetThumbMaterial.maps[MATERIAL_MAP_CUBEMAP].texture.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
}
SetModelShader(resources.starBlackModel, resources.starBlackShader);
SetModelShader(resources.starSpotModel, resources.starSpotShader);
resources.galaxyStarPointMaterial = LoadMaterialDefault();
resources.galaxyStarPointMaterial.shader = resources.galaxyStarPointShader;
resources.galaxyStarPointMaterial.maps[MATERIAL_MAP_DIFFUSE].texture = resources.galaxyStarPointTexture;
resources.galaxyGridMaterial = LoadMaterialDefault();
resources.galaxyGridMaterial.shader = resources.galaxyGridShader;
resources.galaxyFrameMaterial = LoadMaterialDefault();
resources.galaxyFrameMaterial.shader = resources.galaxyFrameShader;
resources.galaxyBackgroundStarMaterial = LoadMaterialDefault();
resources.galaxyBackgroundStarMaterial.shader = resources.galaxyBackgroundStarShader;
resources.galaxyBackgroundStarMaterial.maps[MATERIAL_MAP_DIFFUSE].texture =
    resources.galaxyBackgroundStarNoise;
resources.galaxyMarkerMaterial = LoadMaterialDefault();
resources.galaxyMarkerMaterial.shader = resources.galaxyMarkerShader;
resources.galaxyMarkerMaterial.maps[MATERIAL_MAP_DIFFUSE].texture = resources.galaxyStarRingTexture;
resources.starmapDiskMaterial = LoadMaterialDefault();
resources.starmapDiskMaterial.shader = resources.starmapDiskShader;
resources.starmapDiskMaterial.maps[MATERIAL_MAP_DIFFUSE].texture = resources.starmapOrbitDiskTexture;
SetTextureFilter(resources.galaxyStarPointTexture, TEXTURE_FILTER_BILINEAR);
SetTextureWrap(resources.galaxyStarPointTexture, TEXTURE_WRAP_CLAMP);
SetTextureFilter(resources.galaxyLineTexture, TEXTURE_FILTER_TRILINEAR);
SetTextureWrap(resources.galaxyLineTexture, TEXTURE_WRAP_CLAMP);
SetTextureFilter(resources.galaxyStarRingTexture, TEXTURE_FILTER_BILINEAR);
SetTextureWrap(resources.galaxyStarRingTexture, TEXTURE_WRAP_CLAMP);
// `orbit-disk` imports with filter mode 2 (trilinear), aniso 1 and clamp on both axes.
SetTextureFilter(resources.starmapOrbitDiskTexture, TEXTURE_FILTER_TRILINEAR);
SetTextureWrap(resources.starmapOrbitDiskTexture, TEXTURE_WRAP_CLAMP);
SetTextureFilter(resources.galaxyBackgroundStarNoise, TEXTURE_FILTER_POINT);
SetTextureWrap(resources.galaxyBackgroundStarNoise, TEXTURE_WRAP_REPEAT);
SetTextureFilter(resources.galaxyGridDotTexture, TEXTURE_FILTER_TRILINEAR);
SetTextureWrap(resources.galaxyGridDotTexture, TEXTURE_WRAP_REPEAT);
SetTextureFilter(resources.galaxyGridLineTexture, TEXTURE_FILTER_TRILINEAR);
SetTextureWrap(resources.galaxyGridLineTexture, TEXTURE_WRAP_REPEAT);
SetTextureFilter(resources.galaxyGridFrameTexture, TEXTURE_FILTER_TRILINEAR);
SetTextureWrap(resources.galaxyGridFrameTexture, TEXTURE_WRAP_REPEAT);
SetTextureFilter(resources.bloomLensDirtTexture, TEXTURE_FILTER_BILINEAR);
SetTextureWrap(resources.bloomLensDirtTexture, TEXTURE_WRAP_CLAMP);
SetTextureFilter(resources.galaxyColorGradingLut, TEXTURE_FILTER_BILINEAR);
SetTextureWrap(resources.galaxyColorGradingLut, TEXTURE_WRAP_CLAMP);
// The game runs with QualitySettings.anisotropicTextures = ForceEnable, and each source texture
// carries its own m_Aniso. Only the two grid textures and the frame ask for more than one sample;
// star-line is m_Aniso = 1, so the connection lines must stay isotropic.
SetTextureAnisotropy(resources.galaxyGridDotTexture, 5);
SetTextureAnisotropy(resources.galaxyGridLineTexture, 5);
SetTextureAnisotropy(resources.galaxyGridFrameTexture, 2);
std::string resolvedFontPath = fontPath;
if (resolvedFontPath == DSP_VIEWER_DEFAULT_FONT && !resolvedFontPath.empty())
    resolvedFontPath = ViewerRuntimePath(resolvedFontPath.c_str());
if (!resolvedFontPath.empty() && FileExists(resolvedFontPath.c_str())) {
    Font loadedFont = LoadFontEx(resolvedFontPath.c_str(), kViewerFontSize, nullptr, 0);
    if (loadedFont.glyphCount > 0) {
        resources.font = loadedFont;
        resources.hasCustomFont = true;
        SetTextureFilter(resources.font.texture, TEXTURE_FILTER_BILINEAR);
    }
}
    return true;
}


void UnloadRenderResources(RenderResources &resources) {
if (resources.hasCustomFont) {
    UnloadFont(resources.font);
    resources.hasCustomFont = false;
}
if (resources.starMassModel.meshCount != 0) {
    resources.starMassModel.materials[0].shader.id = rlGetShaderIdDefault();
    resources.starMassModel.materials[0].shader.locs = rlGetShaderLocsDefault();
    UnloadModel(resources.starMassModel);
}
if (resources.starBlackModel.meshCount != 0) {
    resources.starBlackModel.materials[0].shader.id = rlGetShaderIdDefault();
    resources.starBlackModel.materials[0].shader.locs = rlGetShaderLocsDefault();
    UnloadModel(resources.starBlackModel);
}
if (resources.starSpotModel.meshCount != 0) {
    resources.starSpotModel.materials[0].shader.id = rlGetShaderIdDefault();
    resources.starSpotModel.materials[0].shader.locs = rlGetShaderLocsDefault();
    UnloadModel(resources.starSpotModel);
}
if (resources.planetThumbMaterial.maps != nullptr) {
    resources.planetThumbMaterial.shader.id = rlGetShaderIdDefault();
    resources.planetThumbMaterial.shader.locs = rlGetShaderLocsDefault();
    // Detach the shared black cubemap first: UnloadMaterial() deletes every map texture it holds.
    resources.planetThumbMaterial.maps[MATERIAL_MAP_CUBEMAP].texture = {};
    UnloadMaterial(resources.planetThumbMaterial);
    resources.planetThumbMaterial = {};
}
if (resources.planetHeightCubemapId != 0) {
    rlUnloadTexture(resources.planetHeightCubemapId);
    resources.planetHeightCubemapId = 0;
}
if (resources.galaxyStarPointMaterial.maps != nullptr) {
    resources.galaxyStarPointMaterial.shader.id = rlGetShaderIdDefault();
    resources.galaxyStarPointMaterial.shader.locs = rlGetShaderLocsDefault();
    resources.galaxyStarPointMaterial.maps[MATERIAL_MAP_DIFFUSE].texture = {};
    UnloadMaterial(resources.galaxyStarPointMaterial);
    resources.galaxyStarPointMaterial = {};
}
for (Material *material: {&resources.galaxyGridMaterial, &resources.galaxyFrameMaterial,
                          &resources.galaxyBackgroundStarMaterial,
                          &resources.galaxyMarkerMaterial, &resources.starmapDiskMaterial}) {
    if (material->maps == nullptr) continue;
    material->shader.id = rlGetShaderIdDefault();
    material->shader.locs = rlGetShaderLocsDefault();
    material->maps[MATERIAL_MAP_DIFFUSE].texture = {};
    UnloadMaterial(*material);
    *material = {};
}
if (IsTextureValid(resources.galaxyStarPointTexture)) UnloadTexture(resources.galaxyStarPointTexture);
if (IsTextureValid(resources.galaxyLineTexture)) UnloadTexture(resources.galaxyLineTexture);
if (IsTextureValid(resources.galaxyGridDotTexture)) UnloadTexture(resources.galaxyGridDotTexture);
if (IsTextureValid(resources.galaxyGridLineTexture)) UnloadTexture(resources.galaxyGridLineTexture);
if (IsTextureValid(resources.galaxyGridFrameTexture)) UnloadTexture(resources.galaxyGridFrameTexture);
if (IsTextureValid(resources.galaxyBackgroundStarNoise)) UnloadTexture(resources.galaxyBackgroundStarNoise);
if (IsTextureValid(resources.galaxyStarRingTexture)) UnloadTexture(resources.galaxyStarRingTexture);
if (IsTextureValid(resources.starmapOrbitDiskTexture)) UnloadTexture(resources.starmapOrbitDiskTexture);
resources.starmapOrbitDiskTexture = {};
resources.galaxyStarRingTexture = {};
resources.galaxyBackgroundStarNoise = {};
resources.galaxyGridDotTexture = {};
resources.galaxyGridLineTexture = {};
resources.galaxyGridFrameTexture = {};
if (IsTextureValid(resources.galaxyColorGradingLut)) UnloadTexture(resources.galaxyColorGradingLut);
if (IsTextureValid(resources.bloomLensDirtTexture)) UnloadTexture(resources.bloomLensDirtTexture);
resources.galaxyColorGradingLut = {};
resources.bloomLensDirtTexture = {};
auto unloadCustomShader = [](Shader &shader) {
    if (IsShaderValid(shader) && shader.id != rlGetShaderIdDefault()) UnloadShader(shader);
    shader = {};
};
unloadCustomShader(resources.galaxyFxaaShader);
unloadCustomShader(resources.bloomCompositeShader);
unloadCustomShader(resources.bloomUpsampleShader);
unloadCustomShader(resources.bloomDownsampleShader);
unloadCustomShader(resources.bloomDownsampleFirstShader);
unloadCustomShader(resources.bloomPrefilterShader);
unloadCustomShader(resources.starmapDiskShader);
unloadCustomShader(resources.starmapOrbitShader);
unloadCustomShader(resources.galaxyMarkerShader);
unloadCustomShader(resources.galaxyBackgroundStarShader);
unloadCustomShader(resources.galaxyFrameShader);
unloadCustomShader(resources.galaxyGridShader);
unloadCustomShader(resources.galaxyLineShader);
unloadCustomShader(resources.galaxyStarPointShader);
unloadCustomShader(resources.starBlackShader);
unloadCustomShader(resources.planetThumbShader);
unloadCustomShader(resources.starSpotShader);
unloadCustomShader(resources.starSurfaceShader);
resources.uniformLocations.clear();
}

}
