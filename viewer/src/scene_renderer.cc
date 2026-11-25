#include "scene_renderer.hh"

#include "material_catalog.hh"
#include "render_resources.hh"

#include <raymath.h>
#include <rlgl.h>

#if defined(__EMSCRIPTEN__)
#include <GLES3/gl3.h>
#else
#include <external/glad.h>
#endif

#include <algorithm>
#include <cmath>

namespace viewer {

namespace {
// `star-point`, `birth-star-point` and `selected-star-point` each carry a `Billboard`
// component (globalgamemanagers.assets MonoScript 2473) with `lookCamera` null and
// `vertical` false, so its `LateUpdate` assigns the camera's own rotation to the quad. The
// quad is screen aligned; the identity rotation stored in the scene is what Billboard
// overwrites, so it must not be read as evidence that the quad is fixed in a world plane.
Matrix CameraBillboardRotation(const Camera3D &camera) {
    Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    Vector3 up = Vector3Normalize(camera.up);
    if (std::abs(Vector3DotProduct(forward, up)) > 0.999999f) up = Vector3 {0.0f, 0.0f, 1.0f};
    Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, up));
    up = Vector3CrossProduct(right, forward);
    return Matrix {right.x, up.x, forward.x, 0.0f,
                   right.y, up.y, forward.y, 0.0f,
                   right.z, up.z, forward.z, 0.0f,
                   0.0f, 0.0f, 0.0f, 1.0f};
}
}

SceneRenderer::SceneRenderer(const RenderResources &resources, const PostProcess &postProcess)
    : resources(resources), postProcess(postProcess) {}

void SceneRenderer::SetOrbitWidthFactor(float factor) {
    // Material 189 serialises `_TileScale = 7`.
    orbitTileScale = 7.0f / std::clamp(factor, 0.05f, 4.0f);
}

void SceneRenderer::DrawGalaxyLineQuad(Vector3 start, Vector3 end, const Camera3D &camera, float width,
                                       float texcoordStart, float texcoordEnd) {
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
    rlTexCoord2f(texcoordStart, 1.0f);
    rlVertex3f(start.x - side.x, start.y - side.y, start.z - side.z);
    rlTexCoord2f(texcoordEnd, 1.0f);
    rlVertex3f(end.x - side.x, end.y - side.y, end.z - side.z);
    rlTexCoord2f(texcoordEnd, 0.0f);
    rlVertex3f(end.x + side.x, end.y + side.y, end.z + side.z);
    rlTexCoord2f(texcoordStart, 0.0f);
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
    // `UIStarmapStarObject._OnLateUpdate` overwrites the blended material's `_Multiplier` with the
    // constant `4 * bodyLevel` every frame, so the per spectral type value is not used here. It
    // differs for O type stars, neutron stars, white dwarfs and black holes.
    resources.SetVector4(resources.starSurfaceShader, "_Multiplier_and_NoiseTile_and_NoiseSpeed_and_ChaosTile",
                     Vector4 {4.0f * bodyLevel, material.noiseTile, material.noiseSpeed, material.chaosTile});
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
    // Same story as the mass sphere: the runtime sets `_Multiplier = 0.3 * spotLevel` outright.
    resources.SetVector4(resources.starSpotShader, "_Multiplier",
                         Vector4 {0.3f * spotLevel, 0.0f, 0.0f, 0.0f});
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
    Matrix transform = MatrixMultiply(MatrixScale(star.galaxyScale, star.galaxyScale, star.galaxyScale),
                                      CameraBillboardRotation(camera));
    transform = MatrixMultiply(transform,
                               MatrixTranslate(star.position.x, star.position.y, star.position.z));
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

float SceneRenderer::SceneLinear() const {
    return (postProcess.SceneTarget().id != 0 && rlGetActiveFramebuffer() == postProcess.SceneTarget().id) ||
                   (postProcess.MultisampleTarget().id != 0 &&
                    rlGetActiveFramebuffer() == postProcess.MultisampleTarget().id)
               ? 1.0f
               : 0.0f;
}

// The official grid planes are unit quads laid into the XZ plane by a +90 degree
// rotation about X, then scaled and lifted, so the quad spans size world units.
static Matrix GalaxyGroundQuad(float size, float height) {
    Matrix transform = MatrixMultiply(MatrixScale(size, size, 1.0f), MatrixRotateX(PI * 0.5f));
    return MatrixMultiply(transform, MatrixTranslate(0.0f, height, 0.0f));
}

void SceneRenderer::DrawGalaxyGridPlane(const Texture2D &texture, float size, float height, Vector4 tint,
                                        float multiplier, float alphaMultiplier, float tiling,
                                        float areaMin, float areaMax, float nearFade) {
    Matrix transform = GalaxyGroundQuad(size, height);
    rlDrawRenderBatchActive();
    resources.SetMatrix(resources.galaxyGridShader, "matViewProjection", SceneRenderer::ViewProjection());
    resources.SetVector4(resources.galaxyGridShader, "_TintColor", tint);
    resources.SetFloat(resources.galaxyGridShader, "_Multiplier", multiplier);
    resources.SetFloat(resources.galaxyGridShader, "_AlphaMultiplier", alphaMultiplier);
    resources.SetFloat(resources.galaxyGridShader, "_Tiling", tiling);
    resources.SetFloat(resources.galaxyGridShader, "_AreaMin", areaMin);
    resources.SetFloat(resources.galaxyGridShader, "_AreaMax", areaMax);
    resources.SetFloat(resources.galaxyGridShader, "_NearFade", nearFade);
    resources.SetFloat(resources.galaxyGridShader, "_SceneLinear", SceneRenderer::SceneLinear());
    Material material = resources.galaxyGridMaterial;
    material.maps[MATERIAL_MAP_DIFFUSE].texture = texture;
    rlDisableDepthMask();
    rlDisableDepthTest();
    rlDisableBackfaceCulling();
    rlColorMask(true, true, true, false);
    rlSetBlendMode(RL_BLEND_ALPHA);
    DrawMesh(resources.starSpotModel.meshes[0], material, transform);
    rlDrawRenderBatchActive();
    rlColorMask(true, true, true, true);
    rlEnableBackfaceCulling();
    rlEnableDepthTest();
    rlEnableDepthMask();
}

void SceneRenderer::DrawGalaxyFrame(float size, float height, Vector4 color, float multiplier) {
    Matrix transform = GalaxyGroundQuad(size, height);
    rlDrawRenderBatchActive();
    resources.SetMatrix(resources.galaxyFrameShader, "matViewProjection", SceneRenderer::ViewProjection());
    resources.SetVector4(resources.galaxyFrameShader, "_Color", color);
    resources.SetFloat(resources.galaxyFrameShader, "_Multiplier", multiplier);
    resources.SetFloat(resources.galaxyFrameShader, "_SceneLinear", SceneRenderer::SceneLinear());
    Material material = resources.galaxyFrameMaterial;
    material.maps[MATERIAL_MAP_DIFFUSE].texture = resources.galaxyGridFrameTexture;
    rlEnableDepthMask();
    rlEnableDepthTest();
    rlDisableBackfaceCulling();
    rlSetBlendMode(RL_BLEND_ALPHA);
    DrawMesh(resources.starSpotModel.meshes[0], material, transform);
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
}

// The official background is a 5000 unit cube of six outward facing quads centred on
// the origin; the shader places one star per noise texel on each face.
void SceneRenderer::DrawGalaxyBackgroundStars(float time) {
    if (!resources.hasGalaxyBackgroundStarShader || !resources.hasStarModels) return;
    static const struct {
        Quaternion rotation;
        Vector3 position;
    } faces[] = {
        {{0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 2500.0f}},
        {{0.0f, -0.7071068f, 0.0f, 0.7071068f}, {-2500.0f, 0.0f, 0.0f}},
        {{0.0f, 0.7071068f, 0.0f, 0.7071068f}, {2500.0f, 0.0f, 0.0f}},
        {{0.0f, 1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -2500.0f}},
        {{0.7071068f, 0.0f, 0.0f, 0.7071068f}, {0.0f, -2500.0f, 0.0f}},
        {{-0.7071068f, 0.0f, 0.0f, 0.7071068f}, {0.0f, 2500.0f, 0.0f}},
    };
    rlDrawRenderBatchActive();
    Shader shader = resources.galaxyBackgroundStarShader;
    resources.SetMatrix(shader, "matViewProjection", SceneRenderer::ViewProjection());
    resources.SetVector4(shader, "_StarSetting", Vector4 {0.200000003f, 0.200000003f, 1.10000002f, 0.400000006f});
    resources.SetVector4(shader, "_RefValueA", GalaxyGridColor(Vector4 {0.274509804f, 0.490196078f, 0.360784314f, 1.0f}));
    resources.SetVector4(shader, "_RefValueB", GalaxyGridColor(Vector4 {0.603921569f, 0.270588235f, 0.317400008f, 1.0f}));
    resources.SetVector4(shader, "_StarColor", GalaxyGridColor(Vector4 {0.655700028f, 0.747900009f, 1.0f, 1.0f}));
    // _StarColorChange is declared as a Vector, so it is not colour space converted.
    resources.SetVector4(shader, "_StarColorChange", Vector4 {0.5f, 0.699999988f, 0.300000012f, 0.0f});
    resources.SetVector4(shader, "_Time", Vector4 {time / 20.0f, time, time * 2.0f, time * 3.0f});
    resources.SetFloat(shader, "_SceneLinear", SceneRenderer::SceneLinear());
    int texelLocation = resources.Uniform(shader, "_NoiseMapTexelSize");
    if (texelLocation >= 0) {
        Vector2 texelSize {1.0f / 128.0f, 1.0f / 128.0f};
        SetShaderValue(shader, texelLocation, &texelSize, SHADER_UNIFORM_VEC2);
    }
    rlDisableDepthMask();
    rlEnableDepthTest();
    rlDisableBackfaceCulling();
    rlSetBlendMode(RL_BLEND_ALPHA);
    for (const auto &face: faces) {
        Matrix transform = MatrixMultiply(MatrixScale(5000.0f, 5000.0f, 5000.0f),
                                          QuaternionToMatrix(face.rotation));
        transform = MatrixMultiply(transform,
                                   MatrixTranslate(face.position.x, face.position.y, face.position.z));
        DrawMesh(resources.starSpotModel.meshes[0], resources.galaxyBackgroundStarMaterial, transform);
    }
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
    rlEnableDepthMask();
}

void SceneRenderer::DrawGalaxyStarMarker(const Texture2D &texture, Vector3 position, float size,
                                         Vector4 tint, float multiplier, float alphaMultiplier,
                                         const Camera3D &camera) {
    if (!resources.hasGalaxyMarkerShader || !resources.hasStarModels) return;
    Matrix transform = MatrixMultiply(MatrixScale(size, size, size), CameraBillboardRotation(camera));
    transform = MatrixMultiply(transform, MatrixTranslate(position.x, position.y, position.z));
    rlDrawRenderBatchActive();
    resources.SetMatrix(resources.galaxyMarkerShader, "matViewProjection", SceneRenderer::ViewProjection());
    resources.SetVector4(resources.galaxyMarkerShader, "_TintColor", tint);
    resources.SetFloat(resources.galaxyMarkerShader, "_Multiplier", multiplier);
    resources.SetFloat(resources.galaxyMarkerShader, "_AlphaMultiplier", alphaMultiplier);
    resources.SetFloat(resources.galaxyMarkerShader, "_SceneLinear", SceneRenderer::SceneLinear());
    Material material = resources.galaxyMarkerMaterial;
    material.maps[MATERIAL_MAP_DIFFUSE].texture = texture;
    rlDisableDepthMask();
    rlDisableDepthTest();
    rlDisableBackfaceCulling();
    rlColorMask(true, true, true, false);
    rlSetBlendMode(RL_BLEND_ALPHA);
    DrawMesh(resources.starSpotModel.meshes[0], material, transform);
    rlDrawRenderBatchActive();
    rlColorMask(true, true, true, true);
    rlEnableBackfaceCulling();
    rlEnableDepthTest();
    rlEnableDepthMask();
}

void SceneRenderer::DrawGalaxyGrid() {
    if (!resources.hasGalaxyGridShaders || !resources.hasStarModels) return;
    // Material constants come from galaxy-select-grid-1, -2, -4 and -5 in the reference build;
    // the tints are the linear form of the serialised sRGB colours.
    SceneRenderer::DrawGalaxyGridPlane(resources.galaxyGridDotTexture, 500.0f, -15.0f,
                                       GalaxyGridColor(Vector4 {0.551886797f, 0.691747725f, 1.0f, 1.0f}),
                                       1.0f, 0.0299999993f, 0.200000003f, 60.0f, 250.0f, 35.0f);
    SceneRenderer::DrawGalaxyGridPlane(resources.galaxyGridLineTexture, 500.0f, -15.0f,
                                       GalaxyGridColor(Vector4 {1.0f, 1.0f, 1.0f, 1.0f}),
                                       1.0f, 0.0299999993f, 0.400000006f, 60.0f, 250.0f, 35.0f);
    SceneRenderer::DrawGalaxyGridPlane(resources.galaxyGridDotTexture, 67.0f, -10.0f,
                                       GalaxyGridColor(Vector4 {0.570754707f, 0.662471890f, 1.0f, 1.0f}),
                                       1.0f, 0.100000001f, 0.200000003f, 50.0f, 250.0f, -10.0f);
    SceneRenderer::DrawGalaxyGridPlane(resources.galaxyGridLineTexture, 67.0f, -10.0f,
                                       GalaxyGridColor(Vector4 {0.636792421f, 0.739024401f, 1.0f, 1.0f}),
                                       1.0f, 0.0500000007f, 0.400000006f, 50.0f, 250.0f, -10.0f);
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
    resources.SetFloat(resources.galaxyLineShader, "_SceneLinear", SceneRenderer::SceneLinear());
    resources.SetVector4(resources.galaxyLineShader, "_ScreenParams",
                     Vector4 {static_cast<float>(rlGetFramebufferWidth()),
                              static_cast<float>(rlGetFramebufferHeight()), 0.0f, 0.0f});
    rlDisableDepthMask();
    rlDisableBackfaceCulling();
    rlSetBlendFactors(RL_SRC_ALPHA, RL_ONE, RL_FUNC_ADD);
    rlSetBlendMode(RL_BLEND_CUSTOM);
    BeginShaderMode(resources.galaxyLineShader);
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
        // rlDrawRenderBatchActive() below resets every batch draw slot back to the default 1x1
        // white texture, so the profile texture has to be re-bound for every line.
        rlSetTexture(resources.galaxyLineTexture.id);
        SceneRenderer::DrawGalaxyLineQuad(start, end, camera, 1.0f);
        rlDrawRenderBatchActive();
    }
    rlSetTexture(0);
    EndShaderMode();
    rlSetBlendMode(RL_BLEND_ALPHA);
    rlEnableBackfaceCulling();
    rlEnableDepthMask();
}

// `Game Starmap` carries two skybox shells, `bg-stars-near` and `bg-stars-far`. Both are six quads
// of the same material as the Galaxy Select background, only with different constants, and
// `StarmapCamera.LateUpdate` walks them after the camera at 0.93 and 0.96 so they parallax against
// each other. The near shell is scaled 0.7 and turned 45 degrees about X then Y, which decorrelates
// its star field from the far one.
void SceneRenderer::DrawStarmapBackgroundStars(const Camera3D &camera, float time) {
    if (!resources.hasGalaxyBackgroundStarShader || !resources.hasStarModels) return;
    static const struct {
        Quaternion rotation;
        Vector3 position;
        Vector3 scale;
    } faces[] = {
        {{0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 50000.0f}, {100000.0f, 100000.0f, 100000.0f}},
        {{0.0f, -0.707106829f, 0.0f, 0.707106829f}, {-50000.0039f, 0.0f, 0.0f},
         {100000.0625f, 100000.0f, 100000.0625f}},
        {{0.0f, 0.707106829f, 0.0f, 0.707106829f}, {50000.0039f, 0.0f, 0.0f},
         {100000.0625f, 100000.0f, 100000.0625f}},
        {{0.0f, 1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -50000.0f}, {100000.0f, 100000.0f, 100000.0f}},
        {{0.707106829f, 0.0f, 0.0f, 0.707106829f}, {0.0f, -50000.0039f, 0.0f},
         {100000.0f, 100000.0625f, 100000.0625f}},
        {{-0.707106829f, 0.0f, 0.0f, 0.707106829f}, {0.0f, 50000.0039f, 0.0f},
         {100000.0f, 100000.0625f, 100000.0625f}},
    };
    static const struct {
        Quaternion rotation;
        float scale;
        float follow;
    } shells[] = {
        {{0.353553385f, 0.353553385f, -0.146446600f, 0.853553474f}, 0.699999988f, 0.930000007f},
        {{0.0f, 0.0f, 0.0f, 1.0f}, 1.0f, 0.959999979f},
    };
    rlDrawRenderBatchActive();
    Shader shader = resources.galaxyBackgroundStarShader;
    resources.SetMatrix(shader, "matViewProjection", SceneRenderer::ViewProjection());
    // Material 103 `bgstars-game-starmap`; `_StarColor` and `_StarColorChange` match Galaxy Select.
    resources.SetVector4(shader, "_StarSetting", Vector4 {0.270000011f, 0.319999993f, 1.02999997f, 0.25f});
    resources.SetVector4(shader, "_RefValueA", GalaxyGridColor(Vector4 {0.654901981f, 0.668705046f, 1.0f, 1.0f}));
    resources.SetVector4(shader, "_RefValueB", GalaxyGridColor(Vector4 {0.654901981f, 0.834364772f, 1.0f, 1.0f}));
    resources.SetVector4(shader, "_StarColor", GalaxyGridColor(Vector4 {0.655700028f, 0.747900009f, 1.0f, 1.0f}));
    resources.SetVector4(shader, "_StarColorChange", Vector4 {0.5f, 0.699999988f, 0.300000012f, 0.0f});
    resources.SetVector4(shader, "_Time", Vector4 {time / 20.0f, time, time * 2.0f, time * 3.0f});
    resources.SetFloat(shader, "_SceneLinear", SceneRenderer::SceneLinear());
    int texelLocation = resources.Uniform(shader, "_NoiseMapTexelSize");
    if (texelLocation >= 0) {
        Vector2 texelSize {1.0f / 128.0f, 1.0f / 128.0f};
        SetShaderValue(shader, texelLocation, &texelSize, SHADER_UNIFORM_VEC2);
    }
    rlDisableDepthMask();
    rlEnableDepthTest();
    rlDisableBackfaceCulling();
    rlSetBlendMode(RL_BLEND_ALPHA);
    for (const auto &shell: shells) {
        Matrix shellTransform = MatrixMultiply(MatrixScale(shell.scale, shell.scale, shell.scale),
                                               QuaternionToMatrix(shell.rotation));
        Vector3 follow = camera.position * shell.follow;
        shellTransform = MatrixMultiply(shellTransform, MatrixTranslate(follow.x, follow.y, follow.z));
        for (const auto &face: faces) {
            Matrix transform = MatrixMultiply(MatrixScale(face.scale.x, face.scale.y, face.scale.z),
                                              QuaternionToMatrix(face.rotation));
            transform = MatrixMultiply(transform,
                                       MatrixTranslate(face.position.x, face.position.y, face.position.z));
            transform = MatrixMultiply(transform, shellTransform);
            DrawMesh(resources.starSpotModel.meshes[0], resources.galaxyBackgroundStarMaterial, transform);
        }
    }
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
    rlEnableDepthMask();
}

void SceneRenderer::DrawCachedLines(const std::vector<Line3DCache> &lines, Color color) {
    for (const auto &line: lines) DrawLine3D(line.start, line.end, color);
}

// The orbit frame the shader wants is `runtimeOrbitRotation * Quaternion.Euler(0, -phase, 0)`,
// which puts the circle's zero angle on the planet so the `u^5` colour ramp trails behind it. The
// viewer's `orbitRotation` already holds the inclination and longitude, so undoing the current
// phase about Y produces the same frame.
static Matrix StarmapOrbitFrame(const PlanetViewData &planet) {
    return MatrixMultiply(MatrixRotateY(-planet.orbitAngle), planet.orbitRotation);
}

void SceneRenderer::DrawStarmapOrbit(const PlanetViewData &planet, Vector3 center, const Camera3D &camera) {
    if (!resources.hasStarmapOrbitShaders || !IsTextureValid(resources.galaxyLineTexture)) return;
    if (planet.orbitPoints.size() < 3) return;
    rlDrawRenderBatchActive();
    Shader shader = resources.starmapOrbitShader;
    resources.SetMatrix(shader, "matViewProjection", SceneRenderer::ViewProjection());
    resources.SetMatrix(shader, "matOrbitRotation", StarmapOrbitFrame(planet));
    resources.SetVector3(shader, "_WorldSpaceCameraPos", camera.position);
    resources.SetVector4(shader, "_Position", Vector4 {center.x, center.y, center.z, 1.0f});
    // Material 189 `planet-orbit`; both line colours are `Color` properties.
    resources.SetVector4(shader, "_LineColorA",
                         GalaxyGridColor(Vector4 {0.0168209318f, 0.148948908f, 0.169811279f, 1.0f}));
    resources.SetVector4(shader, "_LineColorB",
                         GalaxyGridColor(Vector4 {0.425373763f, 0.707084894f, 0.745283008f, 1.0f}));
    resources.SetFloat(shader, "_Multiplier", 1.0f);
    resources.SetFloat(shader, "_TileScale", orbitTileScale);
    resources.SetFloat(shader, "_DistScale", 0.0500000007f);
    resources.SetFloat(shader, "_DistPower", 1.0f);
    resources.SetFloat(shader, "_MinDistScale", 0.00100000005f);
    resources.SetFloat(shader, "_MaxDistScale", 100.0f);
    resources.SetFloat(shader, "_Radius", planet.orbitRadius);
    resources.SetFloat(shader, "_SceneLinear", SceneRenderer::SceneLinear());
    rlDisableDepthMask();
    rlDisableDepthTest();
    rlDisableBackfaceCulling();
    rlSetBlendFactors(RL_ONE, RL_ONE, RL_FUNC_ADD);
    rlSetBlendMode(RL_BLEND_CUSTOM);
    // Blend One One over a ribbon that overlaps itself at every joint of the loop would add the
    // analytic line twice; the official pass keeps each pixel to a single contribution with a
    // NotEqual stencil test against its own per planet reference and a Replace on pass.
    glEnable(GL_STENCIL_TEST);
    glStencilMask(0xFF);
    glStencilFunc(GL_NOTEQUAL, planet.orbitStencilRef, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    BeginShaderMode(shader);
    // Every segment shares the same uniforms, so one texture bind covers the whole loop; the batch
    // is only flushed once at the end.
    rlSetTexture(resources.galaxyLineTexture.id);
    std::size_t count = planet.orbitPoints.size();
    float step = 1.0f / static_cast<float>(count);
    for (std::size_t i = 0; i < count; ++i) {
        Vector3 start = center + planet.orbitPoints[i];
        Vector3 end = center + planet.orbitPoints[(i + 1) % count];
        SceneRenderer::DrawGalaxyLineQuad(start, end, camera, planet.orbitWidth,
                                          static_cast<float>(i) * step,
                                          static_cast<float>(i + 1) * step);
    }
    rlDrawRenderBatchActive();
    rlSetTexture(0);
    EndShaderMode();
    glDisable(GL_STENCIL_TEST);
    rlSetBlendMode(RL_BLEND_ALPHA);
    rlEnableBackfaceCulling();
    rlEnableDepthTest();
    rlEnableDepthMask();
}

// `orbit-disk` is a child of the orbit LineRenderer, so its 2.052 local scale multiplies the orbit
// radius: a quad 1.026 orbit diameters across, lying in the orbital plane.
void SceneRenderer::DrawStarmapOrbitDisk(const PlanetViewData &planet, Vector3 center) {
    if (!resources.hasStarmapOrbitShaders || !resources.hasStarModels) return;
    float size = planet.orbitRadius * 2.052f;
    Matrix transform = MatrixMultiply(MatrixScale(size, size, 1.0f), MatrixRotateX(PI * 0.5f));
    transform = MatrixMultiply(transform, StarmapOrbitFrame(planet));
    transform = MatrixMultiply(transform, MatrixTranslate(center.x, center.y, center.z));
    rlDrawRenderBatchActive();
    Shader shader = resources.starmapDiskShader;
    resources.SetMatrix(shader, "matViewProjection", SceneRenderer::ViewProjection());
    resources.SetVector4(shader, "_MainTex_ST", Vector4 {1.0f, 1.0f, 0.0f, 0.0f});
    // Material 188 `orbit-disk`, shader `YC Shaders/Particle/Add`.
    resources.SetVector4(shader, "_TintColor",
                         GalaxyGridColor(Vector4 {0.221698105f, 0.868282259f, 1.0f, 1.0f}));
    resources.SetFloat(shader, "_Multiplier", 0.0149999997f);
    resources.SetFloat(shader, "_SceneLinear", SceneRenderer::SceneLinear());
    rlDisableDepthMask();
    rlEnableDepthTest();
    rlDisableBackfaceCulling();
    rlColorMask(true, true, true, false);
    rlSetBlendFactors(RL_SRC_ALPHA, RL_ONE, RL_FUNC_ADD);
    rlSetBlendMode(RL_BLEND_CUSTOM);
    DrawMesh(resources.starSpotModel.meshes[0], resources.starmapDiskMaterial, transform);
    rlDrawRenderBatchActive();
    rlColorMask(true, true, true, true);
    rlSetBlendMode(RL_BLEND_ALPHA);
    rlEnableBackfaceCulling();
    rlEnableDepthMask();
}

void SceneRenderer::DrawPlanet(const PlanetViewData &planet, const Camera3D &camera, float time) {
    if (!resources.hasPlanetThumbShader || !resources.hasPlanetThumbModel) {
        DrawSphere(planet.position, planet.radius, planet.color);
        return;
    }
    // `planetRenderer.transform.localScale = realRadius * 2 * 0.00025` against the unit diameter
    // GeoSphere, and `_Rotation` is the same quaternion the transform carries.
    float scale = planet.radius * 2.0f;
    float rotationAngle = time * 0.035f + planet.flickerPhase;
    // The transform rotation and `_Rotation` have to be the same rotation: the fragment program
    // brings the object space normal into world space with `_Rotation` alone, so building the model
    // matrix from the quaternion keeps the two from drifting apart.
    Quaternion rotation = QuaternionFromAxisAngle(Vector3 {0.0f, -1.0f, 0.0f}, rotationAngle);
    Matrix transform = MatrixMultiply(MatrixScale(scale, scale, scale), QuaternionToMatrix(rotation));
    transform = MatrixMultiply(transform,
                               MatrixTranslate(planet.position.x, planet.position.y, planet.position.z));
    // `planetMat.SetVector("_SunDir", ((star.uPosition - uPosition) * 0.00025).normalized)`; the
    // viewer keeps the star at the origin, so that is the direction back to the origin.
    Vector3 sunDirection = Vector3Normalize(Vector3Scale(planet.position, -1.0f));
    const auto &material = planet.thumbMaterial;
    Shader shader = resources.planetThumbShader;
    rlDrawRenderBatchActive();
    resources.SetMatrix(shader, "matModel", transform);
    resources.SetMatrix(shader, "matViewProjection", SceneRenderer::ViewProjection());
    resources.SetVector3(shader, "_SunDir", sunDirection);
    resources.SetVector4(shader, "_Rotation", Vector4 {rotation.x, rotation.y, rotation.z, rotation.w});
    resources.SetVector3(shader, "_WorldSpaceCameraPos", camera.position);
    resources.SetVector4(shader, "_Color", material.color);
    resources.SetVector4(shader, "_ShoreLineColor", material.shoreLineColor);
    resources.SetVector4(shader, "_RimColor", material.rimColor);
    resources.SetVector4(shader, "_ColorBio0", material.colorBio0);
    resources.SetVector4(shader, "_ColorBio1", material.colorBio1);
    resources.SetVector4(shader, "_ColorBio2", material.colorBio2);
    resources.SetVector4(shader, "_HeightSettings", material.heightSettings);
    resources.SetFloat(shader, "_ShoreInvThick", material.shoreInvThick);
    resources.SetFloat(shader, "_ShoreHeight", material.shoreHeight);
    resources.SetFloat(shader, "_WireIntens", material.wireIntensity);
    resources.SetFloat(shader, "_BioStrength", material.bioStrength);
    resources.SetFloat(shader, "_FarHeight", material.farHeight);
    resources.SetFloat(shader, "_Diameter", material.diameter);
    resources.SetFloat(shader, "_SceneLinear", SceneRenderer::SceneLinear());
    // The pass is `RenderType Opaque` with `Blend One Zero`, ZWrite on, ZTest LEqual and Cull Back.
    rlEnableDepthMask();
    rlEnableDepthTest();
    rlEnableBackfaceCulling();
    rlSetCullFace(RL_CULL_FACE_BACK);
    rlDisableColorBlend();
    DrawMesh(resources.starMassModel.meshes[0], resources.planetThumbMaterial, transform);
    rlDrawRenderBatchActive();
    rlEnableColorBlend();
    rlSetBlendMode(RL_BLEND_ALPHA);
}

void SceneRenderer::DrawGalaxy(const std::vector<GalaxyLineData> &lines,
                               const std::vector<StarViewData> &stars,
                               const Camera3D &camera, float time, std::size_t markedStar,
                               std::size_t invalidIndex) {
    // Official render queues: background 1000, frame 2499, connection lines 2999,
    // grid planes and star points 3000 with the grid planes sorting behind the points.
    DrawGalaxyBackgroundStars(time);
    DrawGalaxyFrame(67.0f, -10.0f, Vector4 {1.0f, 1.0f, 1.0f, 1.0f}, 1.0f);
    DrawGalaxyLines(lines, stars, camera);
    DrawGalaxyGrid();
    for (const auto &star: stars) DrawGalaxyStarPoint(star, camera);
    // `starPointBirth` follows stars[0] with the plain gradient colour and a fixed 1.4 scale;
    // `starPointSelection` uses the star type colour overrides and a scale of
    // `typeScale * 0.6 + 0.6`. The reference build drives it from the ray hit because its starmap
    // has no click selection; the viewer drives it from the clicked star instead.
    if (!stars.empty()) {
        SceneRenderer::DrawGalaxyStarMarker(resources.galaxyStarRingTexture, stars[0].position, 1.4f,
                                            GalaxyGridColor(GalaxyStarGradient(stars[0].data->color)),
                                            2.5f, 1.0f, camera);
    }
    if (markedStar != invalidIndex && markedStar < stars.size()) {
        const auto &star = stars[markedStar];
        SceneRenderer::DrawGalaxyStarMarker(resources.galaxyStarRingTexture, star.position,
                                            star.galaxyScale * 0.6f + 0.6f,
                                            GalaxyGridColor(star.galaxyColor), 2.5f, 1.0f, camera);
    }
}

void SceneRenderer::DrawSystem(const SystemViewCache &system, const Camera3D &camera, float time) {
    auto orbitCenter = [&system](const PlanetViewData &planet) {
        return planet.parentIndex >= 0
                   ? system.planets[static_cast<std::size_t>(planet.parentIndex)].position
                   : Vector3 {};
    };
    // Official render queues in `Game Starmap`: background stars at Background 1000, planet spheres
    // in the opaque pass at 2000, then the star objects. `UIStarmapStarObject` puts the view star's
    // spot at 2981 and every other star's at 2979, around the shared black mask at 2980; the mass
    // spheres are all at 2982. The orbit disks are 2999 and the orbit lines Transparent+2 = 3002.
    DrawStarmapBackgroundStars(camera, time);
    for (const auto &planet: system.planets) DrawPlanet(planet, camera, time);
    for (const auto &star: system.otherStars) DrawStarSpot(star, camera, StarVisualScale(star, camera));
    DrawStarBlack(system.star, camera);
    for (const auto &star: system.otherStars) DrawStarBlack(star, camera);
    DrawStarSpot(system.star, camera, StarVisualScale(system.star, camera));
    DrawStarMass(system.star, camera, time);
    for (const auto &star: system.otherStars) DrawStarMass(star, camera, time);
    for (const auto &planet: system.planets) DrawStarmapOrbitDisk(planet, orbitCenter(planet));
    for (const auto &planet: system.planets) DrawStarmapOrbit(planet, orbitCenter(planet), camera);
}

}
