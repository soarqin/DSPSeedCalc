#include "scene_renderer.hh"

#include "material_catalog.hh"
#include "render_resources.hh"

#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <cmath>

namespace viewer {

namespace {
float Hash01(int value) {
    unsigned int hash = static_cast<unsigned int>(value);
    hash ^= hash >> 16;
    hash *= 0x7feb352dU;
    hash ^= hash >> 15;
    hash *= 0x846ca68bU;
    hash ^= hash >> 16;
    return static_cast<float>(hash & 0x00ffffffU) / 16777216.0f;
}
}

SceneRenderer::SceneRenderer(const RenderResources &resources, const PostProcess &postProcess)
    : resources(resources), postProcess(postProcess) {}

void SceneRenderer::DrawGalaxyLineQuad(Vector3 start, Vector3 end, const Camera3D &camera, float width) {
    Vector3 forward = Vector3Normalize(camera.target - camera.position);
    Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, camera.up));
    Vector3 up = Vector3CrossProduct(right, forward);
    Vector3 segment = end - start;
    Vector3 endRay = end - camera.position;
    Vector3 rayView {
        Vector3DotProduct(endRay, right),
        Vector3DotProduct(endRay, up),
        -Vector3DotProduct(endRay, forward),
    };
    Vector3 segmentView {
        Vector3DotProduct(segment, right),
        Vector3DotProduct(segment, up),
        -Vector3DotProduct(segment, forward),
    };
    auto clampRayComponent = [](float value) {
        return std::copysign(std::max(std::abs(value), 0.01f), value);
    };
    rayView.x = clampRayComponent(rayView.x);
    rayView.y = clampRayComponent(rayView.y);
    rayView.z = clampRayComponent(rayView.z);
    float sideX = rayView.z * segmentView.y - rayView.y * segmentView.z;
    float sideY = rayView.x * segmentView.z - rayView.z * segmentView.x;
    float sideLengthSquared = sideX * sideX + sideY * sideY;
    if (sideLengthSquared < 1.0e-8f) return;
    float sideScale = width * 0.5f / std::sqrt(sideLengthSquared);
    Vector3 side = right * (sideX * sideScale) + up * (sideY * sideScale);
    rlBegin(RL_QUADS);
    rlTexCoord2f(0.0f, 1.0f);
    rlVertex3f(start.x - side.x, start.y - side.y, start.z - side.z);
    rlTexCoord2f(1.0f, 1.0f);
    rlVertex3f(end.x - side.x, end.y - side.y, end.z - side.z);
    rlTexCoord2f(1.0f, 0.0f);
    rlVertex3f(end.x + side.x, end.y + side.y, end.z + side.z);
    rlTexCoord2f(0.0f, 0.0f);
    rlVertex3f(start.x + side.x, start.y + side.y, start.z + side.z);
    rlEnd();
}

float SceneRenderer::StarViewRadius(const dspugen::Star *star) {
    float viewRadius = star->radius * 800.0f;
    return static_cast<float>(static_cast<double>(viewRadius) * 0.00025);
}

float SceneRenderer::RawStarSpotLevel(const StarViewData &star, const Camera3D &camera) {
    float ratio = Vector3Distance(star.position, camera.position) / std::max(0.0001f, SceneRenderer::StarViewRadius(star.data));
    return std::max(0.0f, (ratio - 15.0f) * 0.05f);
}

float SceneRenderer::StarSpotLevel(const StarViewData &star, const Camera3D &camera) {
    float level = SceneRenderer::RawStarSpotLevel(star, camera);
    if (level > 1.0f) {
        level = std::log(level) + 1.0f;
        level = std::log(level) + 1.0f;
    }
    return std::min(level, 2.0f);
}

float SceneRenderer::StarVisualScale(const StarViewData &star, const Camera3D &camera) {
    float vradius = SceneRenderer::StarViewRadius(star.data);
    float vdist = Vector3Distance(star.position, camera.position);
    float factor = static_cast<float>(std::pow(static_cast<double>(std::max(1.0f, star.data->radius)),
                                               0.15000000596046448));
    float minimumScaleRatio = (vdist / std::max(0.0001f, vradius)) / (160.0f / factor);
    float minimumScale = std::max(1.0f, static_cast<float>(std::pow(static_cast<double>(minimumScaleRatio), 0.7)));
    return minimumScale * vradius * 2.0f;
}

Matrix SceneRenderer::ViewProjection() {
    return MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());
}

Matrix SceneRenderer::StarViewProjection() {
    Matrix result = SceneRenderer::ViewProjection();
    result.m2 = (result.m2 + result.m3) * 0.5f;
    result.m6 = (result.m6 + result.m7) * 0.5f;
    result.m10 = (result.m10 + result.m11) * 0.5f;
    result.m14 = (result.m14 + result.m15) * 0.5f;
    return result;
}

void SceneRenderer::SetStarMatrices(Shader shader, Matrix transform) {
    resources.SetMatrix(shader, "matModel", transform);
    resources.SetMatrix(shader, "matViewProjection", SceneRenderer::StarViewProjection());
}

Matrix SceneRenderer::StarTransform(Vector3 position, float scale) {
    return MatrixMultiply(MatrixScale(scale, scale, scale), MatrixTranslate(position.x, position.y, position.z));
}

void SceneRenderer::SetGalaxyPointUniforms(float pointGlow, float multiplier, float alphaMultiplier) {
    int glowLocation = resources.Uniform(resources.galaxyStarPointShader, "_PointGlow");
    int multiplierLocation = resources.Uniform(resources.galaxyStarPointShader, "_Multiplier");
    int alphaLocation = resources.Uniform(resources.galaxyStarPointShader, "_AlphaMultiplier");
    if (glowLocation >= 0) SetShaderValue(resources.galaxyStarPointShader, glowLocation, &pointGlow, SHADER_UNIFORM_FLOAT);
    if (multiplierLocation >= 0) SetShaderValue(resources.galaxyStarPointShader, multiplierLocation, &multiplier, SHADER_UNIFORM_FLOAT);
    if (alphaLocation >= 0) SetShaderValue(resources.galaxyStarPointShader, alphaLocation, &alphaMultiplier, SHADER_UNIFORM_FLOAT);
}

void SceneRenderer::DrawStarMass(const StarViewData &star, const Camera3D &camera, float time) {
    float rawSpotLevel = SceneRenderer::RawStarSpotLevel(star, camera);
    float bodyLevel = std::sqrt(std::clamp(1.0f - rawSpotLevel, 0.0f, 1.0f));
    if (bodyLevel <= 0.0f || !resources.hasStarSurfaceShader || !resources.hasStarModels) return;
    float visualScale = SceneRenderer::StarVisualScale(star, camera);
    Matrix transform = SceneRenderer::StarTransform(star.position, visualScale);
    const auto &material = star.massMaterial;
    rlDrawRenderBatchActive();
    resources.SetVector3(resources.starSurfaceShader, "_Color0_xyz", Vector3 {material.color0.x, material.color0.y, material.color0.z});
    resources.SetVector3(resources.starSurfaceShader, "_Color1_xyz", Vector3 {material.color1.x, material.color1.y, material.color1.z});
    resources.SetVector3(resources.starSurfaceShader, "_Color2_xyz", Vector3 {material.color2.x, material.color2.y, material.color2.z});
    resources.SetVector3(resources.starSurfaceShader, "_Color3_xyz", Vector3 {material.color3.x, material.color3.y, material.color3.z});
    resources.SetVector4(resources.starSurfaceShader, "_Multiplier_and_NoiseTile_and_NoiseSpeed_and_ChaosTile",
                     Vector4 {material.multiplier * bodyLevel, material.noiseTile, material.noiseSpeed, material.chaosTile});
    resources.SetVector4(resources.starSurfaceShader, "_ChaosDistort_and_ChaosOverlay_and_SpotIntens_and_RimPower",
                     Vector4 {material.chaosDistort, material.chaosOverlay, material.spotIntensity, material.rimPower});
    resources.SetVector4(resources.starSurfaceShader, "_RotSpeed", Vector4 {material.rotSpeed, 0.0f, 0.0f, 0.0f});
    resources.SetVector4(resources.starSurfaceShader, "_Time", Vector4 {time / 20.0f, time, time * 2.0f, time * 3.0f});
    resources.SetVector3(resources.starSurfaceShader, "_WorldSpaceCameraPos", camera.position);
    SceneRenderer::SetStarMatrices(resources.starSurfaceShader, transform);
    rlEnableDepthMask();
    rlEnableBackfaceCulling();
    rlSetCullFace(RL_CULL_FACE_BACK);
    rlSetBlendMode(RL_BLEND_ADD_COLORS);
    DrawMesh(resources.starMassModel.meshes[0], resources.starMassModel.materials[0], transform);
    rlSetBlendMode(RL_BLEND_ALPHA);
}

void SceneRenderer::DrawStarBlack(const StarViewData &star, const Camera3D &camera) {
    if (SceneRenderer::RawStarSpotLevel(star, camera) >= 1.0f || !resources.hasStarBlackShader || !resources.hasStarModels) return;
    float visualScale = SceneRenderer::StarVisualScale(star, camera);
    Matrix transform = SceneRenderer::StarTransform(star.position, visualScale * 1.0049999952316284f);
    rlDrawRenderBatchActive();
    resources.SetVector4(resources.starBlackShader, "_TintColor", Vector4 {0.0f, 0.0f, 0.0f, 1.0f});
    SceneRenderer::SetStarMatrices(resources.starBlackShader, transform);
    rlSetBlendMode(RL_BLEND_ALPHA);
    rlDisableDepthMask();
    rlDisableBackfaceCulling();
    DrawMesh(resources.starBlackModel.meshes[0], resources.starBlackModel.materials[0], transform);
    rlEnableBackfaceCulling();
    rlEnableDepthMask();
}

void SceneRenderer::DrawStarSpot(const StarViewData &star, const Camera3D &camera, float visualScale) {
    if (!resources.hasStarSpotShader || !resources.hasStarModels) return;
    float spotLevel = SceneRenderer::StarSpotLevel(star, camera);
    if (spotLevel <= 0.0f) return;
    const auto &material = star.spotMaterial;
    rlDrawRenderBatchActive();
    resources.SetVector4(resources.starSpotShader, "_Multiplier",
                         Vector4 {material.multiplier * 0.3f * spotLevel, 0.0f, 0.0f, 0.0f});
    resources.SetVector3(resources.starSpotShader, "_SunGlowColor_xyz", Vector3 {material.glowColor.x, material.glowColor.y, material.glowColor.z});
    resources.SetVector4(resources.starSpotShader, "_SunGlowPower", Vector4 {material.glowPower, 0.0f, 0.0f, 0.0f});
    resources.SetVector3(resources.starSpotShader, "_SunCenterColor_xyz", Vector3 {material.centerColor.x, material.centerColor.y, material.centerColor.z});
    resources.SetVector4(resources.starSpotShader, "_SunCenterMultiplier_and_SunCenterSize_and_SunCenterBlur_and_SunCenterBlurPower",
                     Vector4 {material.centerMultiplier, material.centerSize, material.centerBlur, material.centerBlurPower});
    resources.SetVector4(resources.starSpotShader, "_Global_WhiteMode0", Vector4 {});
    Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    Vector3 worldUp {0.0f, 1.0f, 0.0f};
    if (std::abs(Vector3DotProduct(forward, worldUp)) > 0.999999f) worldUp = Vector3 {0.0f, 0.0f, 1.0f};
    Vector3 right = Vector3Normalize(Vector3CrossProduct(worldUp, forward));
    Vector3 up = Vector3CrossProduct(forward, right);
    Matrix rotation {right.x, up.x, forward.x, 0.0f,
                     right.y, up.y, forward.y, 0.0f,
                     right.z, up.z, forward.z, 0.0f,
                     0.0f, 0.0f, 0.0f, 1.0f};
    Matrix transform = MatrixMultiply(MatrixScale(visualScale * 3.700000047683716f,
                                                   visualScale * 3.700000047683716f,
                                                   visualScale * 3.700000047683716f), rotation);
    transform = MatrixMultiply(transform, MatrixTranslate(star.position.x, star.position.y, star.position.z));
    SceneRenderer::SetStarMatrices(resources.starSpotShader, transform);
    rlSetBlendMode(RL_BLEND_ADD_COLORS);
    rlDisableDepthMask();
    rlDisableBackfaceCulling();
    DrawMesh(resources.starSpotModel.meshes[0], resources.starSpotModel.materials[0], transform);
    rlEnableBackfaceCulling();
    rlEnableDepthMask();
    rlSetBlendMode(RL_BLEND_ALPHA);
}

void SceneRenderer::DrawGalaxyStarPoint(const StarViewData &star, const Camera3D &camera) {
    if (!resources.hasGalaxyStarPointShader || !resources.hasGalaxyStarPointModel) return;
    Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    Vector3 up = Vector3Normalize(camera.up);
    if (std::abs(Vector3DotProduct(forward, up)) > 0.999999f) up = Vector3 {0.0f, 0.0f, 1.0f};
    Vector3 right = Vector3Normalize(Vector3CrossProduct(up, forward));
    up = Vector3CrossProduct(forward, right);
    Matrix rotation {right.x, up.x, forward.x, 0.0f,
                     right.y, up.y, forward.y, 0.0f,
                     right.z, up.z, forward.z, 0.0f,
                     0.0f, 0.0f, 0.0f, 1.0f};
    Matrix transform = MatrixMultiply(MatrixScale(star.galaxyScale, star.galaxyScale, star.galaxyScale), rotation);
    transform = MatrixMultiply(transform, MatrixTranslate(star.position.x, star.position.y, star.position.z));
    rlDrawRenderBatchActive();
    resources.SetMatrix(resources.galaxyStarPointShader, "matViewProjection", SceneRenderer::ViewProjection());
    resources.SetVector4(resources.galaxyStarPointShader, "_MainTex_ST", Vector4 {1.0f, 1.0f, 0.0f, 0.0f});
    resources.SetVector4(resources.galaxyStarPointShader, "_TintColor", star.galaxyColor);
    int framebufferWidth = rlGetFramebufferWidth();
    int framebufferHeight = rlGetFramebufferHeight();
    resources.SetVector4(resources.galaxyStarPointShader, "_ScreenParams",
                     Vector4 {static_cast<float>(framebufferWidth), static_cast<float>(framebufferHeight), 0.0f, 0.0f});
    float fadeRadius = 0.0f;
    int fadeLocation = resources.Uniform(resources.galaxyStarPointShader, "_Global_GalaxyFadeRadius");
    if (fadeLocation >= 0) SetShaderValue(resources.galaxyStarPointShader, fadeLocation, &fadeRadius, SHADER_UNIFORM_FLOAT);
    float sceneLinear = (postProcess.SceneTarget().id != 0 && rlGetActiveFramebuffer() == postProcess.SceneTarget().id) ||
                        (postProcess.MultisampleTarget().id != 0 &&
                         rlGetActiveFramebuffer() == postProcess.MultisampleTarget().id) ? 1.0f : 0.0f;
    int sceneLinearLocation = resources.Uniform(resources.galaxyStarPointShader, "_SceneLinear");
    if (sceneLinearLocation >= 0)
        SetShaderValue(resources.galaxyStarPointShader, sceneLinearLocation, &sceneLinear, SHADER_UNIFORM_FLOAT);
    Vector4 white {1.0f, 1.0f, 1.0f, 1.0f};
    int colorLocation = resources.Uniform(resources.galaxyStarPointShader, "colDiffuse");
    if (colorLocation >= 0) SetShaderValue(resources.galaxyStarPointShader, colorLocation, &white, SHADER_UNIFORM_VEC4);
    auto &material = resources.galaxyStarPointMaterial;
    int savedColorLocation = material.shader.locs[SHADER_LOC_COLOR_DIFFUSE];
    material.shader.locs[SHADER_LOC_COLOR_DIFFUSE] = -1;
    rlDisableDepthMask();
    rlDisableDepthTest();
    rlDisableBackfaceCulling();
    rlColorMask(true, true, true, false);
    SceneRenderer::SetGalaxyPointUniforms(0.0f, 2.5f, 1.0f);
    rlSetBlendFactors(RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD);
    rlSetBlendMode(RL_BLEND_CUSTOM);
    DrawMesh(resources.starSpotModel.meshes[0], material, transform);
    material.shader.locs[SHADER_LOC_COLOR_DIFFUSE] = savedColorLocation;
    rlColorMask(true, true, true, true);
    rlSetBlendMode(RL_BLEND_ALPHA);
    rlEnableBackfaceCulling();
    rlEnableDepthTest();
    rlEnableDepthMask();
}

void SceneRenderer::DrawGalaxyLines(const std::vector<GalaxyLineData> &lines,
                     const std::vector<StarViewData> &stars, const Camera3D &camera) {
    if (!resources.hasGalaxyLineShader || !IsTextureValid(resources.galaxyLineTexture)) return;
    rlDrawRenderBatchActive();
    resources.SetMatrix(resources.galaxyLineShader, "matViewProjection", SceneRenderer::ViewProjection());
    resources.SetVector3(resources.galaxyLineShader, "_WorldSpaceCameraPos", camera.position);
    resources.SetFloat(resources.galaxyLineShader, "_Multiplier", 0.349999994f);
    resources.SetFloat(resources.galaxyLineShader, "_TileScale", 6.0f);
    resources.SetFloat(resources.galaxyLineShader, "_DistScale", 0.0299999993f);
    resources.SetFloat(resources.galaxyLineShader, "_DistPower", 0.899999976f);
    resources.SetFloat(resources.galaxyLineShader, "_MinDistScale", 0.100000001f);
    resources.SetFloat(resources.galaxyLineShader, "_MaxDistScale", 10.0f);
    resources.SetFloat(resources.galaxyLineShader, "_Global_GalaxyFadeRadius", 0.0f);
    float sceneLinear = (postProcess.SceneTarget().id != 0 && rlGetActiveFramebuffer() == postProcess.SceneTarget().id) ||
                        (postProcess.MultisampleTarget().id != 0 &&
                         rlGetActiveFramebuffer() == postProcess.MultisampleTarget().id) ? 1.0f : 0.0f;
    resources.SetFloat(resources.galaxyLineShader, "_SceneLinear", sceneLinear);
    resources.SetVector4(resources.galaxyLineShader, "_ScreenParams",
                     Vector4 {static_cast<float>(rlGetFramebufferWidth()),
                              static_cast<float>(rlGetFramebufferHeight()), 0.0f, 0.0f});
    rlDisableDepthMask();
    rlDisableBackfaceCulling();
    rlSetBlendFactors(RL_SRC_ALPHA, RL_ONE, RL_FUNC_ADD);
    rlSetBlendMode(RL_BLEND_CUSTOM);
    BeginShaderMode(resources.galaxyLineShader);
    rlSetTexture(resources.galaxyLineTexture.id);
    for (const auto &line: lines) {
        const auto &startStar = stars[line.start];
        const auto &endStar = stars[line.end];
        Vector3 span = endStar.position - startStar.position;
        float distance = Vector3Length(span);
        if (distance <= 0.0f) continue;
        Vector3 start = startStar.position + span * ((startStar.galaxyScale * 0.25f + 0.35f) / distance);
        Vector3 end = startStar.position + span * (1.0f - (endStar.galaxyScale * 0.25f + 0.35f) / distance);
        resources.SetVector4(resources.galaxyLineShader, "_LineColorA", GalaxyLineColor(startStar.galaxyColor));
        resources.SetVector4(resources.galaxyLineShader, "_LineColorB", GalaxyLineColor(endStar.galaxyColor));
        SceneRenderer::DrawGalaxyLineQuad(start, end, camera, 1.0f);
        rlDrawRenderBatchActive();
    }
    rlSetTexture(0);
    EndShaderMode();
    rlSetBlendMode(RL_BLEND_ALPHA);
    rlEnableBackfaceCulling();
    rlEnableDepthMask();
}

void SceneRenderer::DrawBackgroundStars(const Camera3D &camera, bool bloomVisible) {
    if (!bloomVisible) return;
    for (int layer = 0; layer < 2; ++layer) {
        float follow = layer == 0 ? 0.93f : 0.96f;
        float shellRadius = layer == 0 ? 105.0f : 180.0f;
        int count = layer == 0 ? 90 : 150;
        Vector3 center = camera.position * follow;
        for (int i = 0; i < count; ++i) {
            float y = 1.0f - 2.0f * (static_cast<float>(i) + 0.5f) / static_cast<float>(count);
            float ring = std::sqrt(std::max(0.0f, 1.0f - y * y));
            float angle = static_cast<float>(i) * 2.39996323f + Hash01(i + layer * 997) * 0.35f;
            Vector3 point = center + Vector3 {std::cos(angle) * ring, y, std::sin(angle) * ring} * shellRadius;
            float size = (layer == 0 ? 0.025f : 0.035f) * (0.65f + Hash01(i * 17 + layer) * 0.9f);
            Color color = Hash01(i * 31 + layer) > 0.82f ? Color {132, 186, 231, 180} : Color {194, 211, 224, 145};
            DrawSphere(point, size, color);
        }
    }
}

void SceneRenderer::DrawCachedLines(const std::vector<Line3DCache> &lines, Color color) {
    for (const auto &line: lines) DrawLine3D(line.start, line.end, color);
}

void SceneRenderer::DrawOrbit(Vector3 center, const PlanetViewData &planet) {
    if (planet.orbitPoints.empty()) return;
    Vector3 previous = center + planet.orbitPoints.front();
    for (std::size_t i = 1; i < planet.orbitPoints.size(); ++i) {
        Vector3 next = center + planet.orbitPoints[i];
        DrawLine3D(previous, next, Color {67, 119, 150, 122});
        previous = next;
    }
}

void SceneRenderer::DrawPlanet(const PlanetViewData &planet, const Camera3D &camera, float time, bool selected) {
    if (resources.hasPlanetThumbShader && resources.hasPlanetThumbModel) {
        float scale = planet.radius * 2.0f;
        float rotationAngle = time * 0.035f + planet.flickerPhase;
        Quaternion rotation = QuaternionFromAxisAngle(Vector3 {0.0f, -1.0f, 0.0f}, rotationAngle);
        Matrix transform = MatrixMultiply(MatrixScale(scale, scale, scale), MatrixRotateY(-rotationAngle));
        transform = MatrixMultiply(transform,
                                   MatrixTranslate(planet.position.x, planet.position.y, planet.position.z));
        Vector3 sunDirection = Vector3Normalize(Vector3Scale(planet.position, -1.0f));
        const auto &material = planet.thumbMaterial;
        rlDrawRenderBatchActive();
        resources.SetMatrix(resources.planetThumbShader, "matModel", transform);
        resources.SetMatrix(resources.planetThumbShader, "matViewProjection", SceneRenderer::ViewProjection());
        resources.SetVector3(resources.planetThumbShader, "_SunDir", sunDirection);
        resources.SetVector4(resources.planetThumbShader, "_Rotation", Vector4 {rotation.x, rotation.y, rotation.z, rotation.w});
        resources.SetVector3(resources.planetThumbShader, "_WorldSpaceCameraPos", camera.position);
        resources.SetVector4(resources.planetThumbShader, "_Color", material.color);
        resources.SetVector4(resources.planetThumbShader, "_RimColor", material.rimColor);
        rlEnableDepthMask();
        rlEnableDepthTest();
        rlEnableBackfaceCulling();
        rlSetCullFace(RL_CULL_FACE_BACK);
        rlSetBlendMode(RL_BLEND_ALPHA);
        DrawMesh(resources.starMassModel.meshes[0], resources.planetThumbMaterial, transform);
    } else {
        DrawSphere(planet.position, planet.radius, planet.color);
    }
    if (selected) {
        DrawSphereWires(planet.position, planet.selectionRadius, 12, 8, WHITE);
    }
}

void SceneRenderer::DrawGalaxy(const std::vector<Line3DCache> &gridLines,
                               const std::vector<GalaxyLineData> &lines,
                               const std::vector<StarViewData> &stars,
                               const Camera3D &camera, float time) {
    (void)time;
    DrawBackgroundStars(camera, false);
    float gridAlpha = std::clamp((Vector3Distance(camera.position, camera.target) - 22.0f) / 34.0f,
                                 0.0f, 1.0f) * 0.3f;
    Color gridColor {105, 153, 181,
                     static_cast<unsigned char>(std::clamp(gridAlpha, 0.0f, 1.0f) * 255.0f)};
    DrawCachedLines(gridLines, gridColor);
    DrawGalaxyLines(lines, stars, camera);
    for (const auto &star: stars) DrawGalaxyStarPoint(star, camera);
}

void SceneRenderer::DrawSystem(const SystemViewCache &system, const Camera3D &camera,
                               float time, std::size_t selectedPlanet, std::size_t invalidIndex) {
    DrawBackgroundStars(camera);
    DrawStarBlack(system.star, camera);
    DrawStarMass(system.star, camera, time);
    DrawStarSpot(system.star, camera, StarVisualScale(system.star, camera));
    if (selectedPlanet == invalidIndex) DrawCubeWiresV(system.star.position, system.star.selectionSize, WHITE);
    for (std::size_t planetIndex = 0; planetIndex < system.planets.size(); ++planetIndex) {
        const auto &planet = system.planets[planetIndex];
        Vector3 center {};
        if (planet.parentIndex >= 0)
            center = system.planets[static_cast<std::size_t>(planet.parentIndex)].position;
        DrawOrbit(center, planet);
        DrawPlanet(planet, camera, time, planetIndex == selectedPlanet);
    }
}

}
