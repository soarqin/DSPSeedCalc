#include "post_process.hh"
#include "render_resources.hh"

#include <rlgl.h>

#if defined(__EMSCRIPTEN__)
#include <GLES3/gl3.h>
#else
#include <external/glad.h>
#endif

#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace viewer {
namespace {

float GammaToLinearSpace(float value) {
    if (value <= 0.04045f) return value / 12.92f;
    return std::pow((value + 0.055f) / 1.055f, 2.4f);
}

Color LinearBackground(Color color) {
    auto convert = [](unsigned char value) {
        float srgb = static_cast<float>(value) / 255.0f;
        float linear = srgb <= 0.04045f ? srgb / 12.92f : std::pow((srgb + 0.055f) / 1.055f, 2.4f);
        return static_cast<unsigned char>(std::round(linear * 255.0f));
    };
    return Color {convert(color.r), convert(color.g), convert(color.b), color.a};
}

RenderTexture2D LoadHalfFloatRenderTexture(int width, int height, bool withDepth) {
    RenderTexture2D target{};
    target.id = rlLoadFramebuffer();
    if (target.id == 0) return {};
    rlEnableFramebuffer(target.id);
    target.texture.id = rlLoadTexture(nullptr, width, height, RL_PIXELFORMAT_UNCOMPRESSED_R16G16B16A16, 1);
    target.texture.width = width;
    target.texture.height = height;
    target.texture.mipmaps = 1;
    target.texture.format = PIXELFORMAT_UNCOMPRESSED_R16G16B16A16;
    rlFramebufferAttach(target.id, target.texture.id, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D, 0);
    if (withDepth) {
        target.depth.id = rlLoadTextureDepth(width, height, true);
        target.depth.width = width;
        target.depth.height = height;
        target.depth.mipmaps = 1;
        target.depth.format = 19;
        rlFramebufferAttach(target.id, target.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_RENDERBUFFER, 0);
    }
    bool complete = rlFramebufferComplete(target.id);
    rlDisableFramebuffer();
    if (!complete || !IsTextureValid(target.texture)) {
        UnloadRenderTexture(target);
        return {};
    }
    SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureWrap(target.texture, TEXTURE_WRAP_CLAMP);
    return target;
}

RenderTexture2D LoadMultisampleHalfFloatRenderTexture(int width, int height, int requestedSamples) {
    RenderTexture2D target{};
    GLint maxSamples = 0;
    glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
    int samples = 0;
    for (int candidate: {requestedSamples, 4, 2, 1}) {
        if (candidate > maxSamples) continue;
        samples = candidate;
        GLenum previousError = glGetError();
        if (previousError != GL_NO_ERROR) {
            TraceLog(LOG_WARNING, "FBO: OpenGL error before multisample target creation: 0x%04X", previousError);
            return {};
        }
        target.id = rlLoadFramebuffer();
        if (target.id == 0) return {};
        glBindFramebuffer(GL_FRAMEBUFFER, target.id);
        glGenRenderbuffers(1, &target.texture.id);
        glBindRenderbuffer(GL_RENDERBUFFER, target.texture.id);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_RGBA16F, width, height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, target.texture.id);
        glGenRenderbuffers(1, &target.depth.id);
        glBindRenderbuffer(GL_RENDERBUFFER, target.depth.id);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH_COMPONENT24, width, height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, target.depth.id);
        bool complete = glGetError() == GL_NO_ERROR &&
                        glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        if (complete) break;
        if (target.texture.id != 0) glDeleteRenderbuffers(1, &target.texture.id);
        if (target.depth.id != 0) glDeleteRenderbuffers(1, &target.depth.id);
        GLuint framebuffer = target.id;
        glDeleteFramebuffers(1, &framebuffer);
        target = {};
    }
    if (target.id == 0) return {};
    target.texture.width = width;
    target.texture.height = height;
    target.texture.mipmaps = 1;
    target.texture.format = PIXELFORMAT_UNCOMPRESSED_R16G16B16A16;
    target.depth.width = width;
    target.depth.height = height;
    target.depth.mipmaps = 1;
    target.depth.format = 19;
    if (samples < requestedSamples)
        TraceLog(LOG_WARNING, "FBO: Requested %i multisample HDR samples, using %i", requestedSamples, samples);
    TraceLog(LOG_INFO, "FBO: [ID %i] Multisample HDR target loaded successfully (%ix%i | %i samples)",
             target.id, width, height, samples);
    return target;
}

void UnloadMultisampleRenderTexture(RenderTexture2D &target) {
    if (target.texture.id != 0) glDeleteRenderbuffers(1, &target.texture.id);
    if (target.depth.id != 0) glDeleteRenderbuffers(1, &target.depth.id);
    if (target.id != 0) {
        GLuint framebuffer = target.id;
        glDeleteFramebuffers(1, &framebuffer);
    }
    target = {};
}

}

void PostProcess::Unload() {
    UnloadMultisampleRenderTexture(multisampleTarget);
    if (sceneTarget.id != 0) UnloadRenderTexture(sceneTarget);
    if (fxaaTarget.id != 0) UnloadRenderTexture(fxaaTarget);
    if (prefilterTarget.id != 0) UnloadRenderTexture(prefilterTarget);
    for (auto &level: downLevels) {
        if (level.id != 0) UnloadRenderTexture(level);
        level = {};
    }
    for (auto &level: upLevels) {
        if (level.id != 0) UnloadRenderTexture(level);
        level = {};
    }
    sceneTarget = {};
    fxaaTarget = {};
    prefilterTarget = {};
    targetWidth = 0;
    targetHeight = 0;
    iterationCount = 0;
    sampleScale = 1.0f;
}

bool PostProcess::EnsureTargets(int width, int height) {
    if (targetWidth == width && targetHeight == height && multisampleTarget.id != 0 && sceneTarget.id != 0 &&
        fxaaTarget.id != 0 && prefilterTarget.id != 0 && downLevels[0].id != 0) return true;
    if (failedWidth == width && failedHeight == height) return false;
    Unload();
    multisampleTarget = LoadMultisampleHalfFloatRenderTexture(width, height, 8);
    sceneTarget = LoadHalfFloatRenderTexture(width, height, true);
    fxaaTarget = LoadHalfFloatRenderTexture(width, height, false);
    int levelWidth = std::max(1, width / 2);
    int levelHeight = std::max(1, height / 2);
    prefilterTarget = LoadHalfFloatRenderTexture(levelWidth, levelHeight, false);
    float pyramid = std::log2(static_cast<float>(levelHeight)) - 3.0f;
    int pyramidInteger = static_cast<int>(pyramid);
    iterationCount = std::clamp(pyramidInteger, 1, kMaxBloomIterations);
    sampleScale = 0.5f + pyramid - static_cast<float>(pyramidInteger);
    for (int i = 0; i < iterationCount; ++i) {
        levelWidth = std::max(1, levelWidth / 2);
        levelHeight = std::max(1, levelHeight / 2);
        downLevels[i] = LoadHalfFloatRenderTexture(levelWidth, levelHeight, false);
    }
    for (int i = iterationCount - 2; i >= 0; --i)
        upLevels[i] = LoadHalfFloatRenderTexture(downLevels[i].texture.width, downLevels[i].texture.height, false);
    bool complete = multisampleTarget.id != 0 && sceneTarget.id != 0 && fxaaTarget.id != 0 && prefilterTarget.id != 0;
    for (int i = 0; i < iterationCount; ++i) complete = complete && downLevels[i].id != 0;
    for (int i = 0; i < iterationCount - 1; ++i) complete = complete && upLevels[i].id != 0;
    if (!complete) {
        Unload();
        failedWidth = width;
        failedHeight = height;
        return false;
    }
    targetWidth = width;
    targetHeight = height;
    failedWidth = 0;
    failedHeight = 0;
    return true;
}

bool PostProcess::Draw(const Camera3D &camera, const RenderResources &resources,
                       const void *context, void (*drawScene)(const void *, float)) {
    float time = static_cast<float>(GetTime());
    if (!EnsureTargets(GetScreenWidth(), GetScreenHeight())) {
        BeginMode3D(camera);
        drawScene(context, time);
        EndMode3D();
        return false;
    }

    BeginMultisampleTextureMode(multisampleTarget);
    Color linearBackground = LinearBackground(Color {5, 21, 26, 255});
    rlClearColor(linearBackground.r, linearBackground.g, linearBackground.b, 255);
    rlClearScreenBuffers();
    BeginMode3D(camera);
    drawScene(context, time);
    EndMode3D();
    ResolveMultisampleTexture(multisampleTarget, sceneTarget);

    BeginTextureMode(prefilterTarget);
    BeginOpaqueBlit();
    BeginShaderMode(resources.bloomPrefilterShader);
    resources.SetSourceTexelSize(resources.bloomPrefilterShader, sceneTarget.texture);
    const float threshold = GammaToLinearSpace(1.2f);
    const float knee = threshold * 0.3f + 1.0e-5f;
    resources.SetFloat(resources.bloomPrefilterShader, "prefilterOffset", -0.5f);
    resources.SetFloat(resources.bloomPrefilterShader, "threshold", threshold);
    resources.SetVector3(resources.bloomPrefilterShader, "curve",
                         Vector3 {threshold - knee, knee * 2.0f, 0.25f / knee});
    DrawFlippedTexture(sceneTarget.texture, prefilterTarget.texture.width, prefilterTarget.texture.height);
    EndShaderMode();
    EndOpaqueBlit();
    EndTextureMode();

    Texture2D bloomSource = prefilterTarget.texture;
    for (int i = 0; i < iterationCount; ++i) {
        auto &level = downLevels[i];
        BeginTextureMode(level);
        BeginOpaqueBlit();
        Shader shader = i == 0 ? resources.bloomDownsampleFirstShader : resources.bloomDownsampleShader;
        BeginShaderMode(shader);
        resources.SetSourceTexelSize(shader, bloomSource);
        DrawFlippedTexture(bloomSource, level.texture.width, level.texture.height);
        EndShaderMode();
        EndOpaqueBlit();
        EndTextureMode();
        bloomSource = level.texture;
    }

    for (int i = iterationCount - 2; i >= 0; --i) {
        auto &level = upLevels[i];
        BeginTextureMode(level);
        BeginOpaqueBlit();
        BeginShaderMode(resources.bloomUpsampleShader);
        resources.SetSourceTexelSize(resources.bloomUpsampleShader, bloomSource, sampleScale);
        int baseLocation = resources.Uniform(resources.bloomUpsampleShader, "baseTexture");
        if (baseLocation >= 0)
            SetShaderValueTexture(resources.bloomUpsampleShader, baseLocation, downLevels[i].texture);
        DrawFlippedTexture(bloomSource, level.texture.width, level.texture.height);
        EndShaderMode();
        EndOpaqueBlit();
        EndTextureMode();
        bloomSource = level.texture;
    }

    BeginTextureMode(fxaaTarget);
    BeginOpaqueBlit();
    BeginShaderMode(resources.bloomCompositeShader);
    int bloomLocation = resources.Uniform(resources.bloomCompositeShader, "bloomTexture");
    if (bloomLocation >= 0) SetShaderValueTexture(resources.bloomCompositeShader, bloomLocation, bloomSource);
    int dirtLocation = resources.Uniform(resources.bloomCompositeShader, "lensDirtTexture");
    if (dirtLocation >= 0)
        SetShaderValueTexture(resources.bloomCompositeShader, dirtLocation, resources.bloomLensDirtTexture);
    int lutLocation = resources.Uniform(resources.bloomCompositeShader, "gradingLutTexture");
    if (lutLocation >= 0)
        SetShaderValueTexture(resources.bloomCompositeShader, lutLocation, resources.galaxyColorGradingLut);
    resources.SetSourceTexelSize(resources.bloomCompositeShader, bloomSource, sampleScale);
    resources.SetFloat(resources.bloomCompositeShader, "intensity", 0.3f);
    resources.SetFloat(resources.bloomCompositeShader, "dirtIntensity", 3.0f);
    DrawFlippedTexture(sceneTarget.texture, fxaaTarget.texture.width, fxaaTarget.texture.height);
    EndShaderMode();
    EndOpaqueBlit();
    EndTextureMode();

    BeginOpaqueBlit();
    BeginShaderMode(resources.galaxyFxaaShader);
    resources.SetSourceTexelSize(resources.galaxyFxaaShader, fxaaTarget.texture);
    resources.SetFloat(resources.galaxyFxaaShader, "qualitySubpix", 0.75f);
    resources.SetFloat(resources.galaxyFxaaShader, "qualityEdgeThreshold", 0.165999994f);
    resources.SetFloat(resources.galaxyFxaaShader, "qualityEdgeThresholdMin", 0.0833000019f);
    DrawFlippedTexture(fxaaTarget.texture, GetScreenWidth(), GetScreenHeight());
    EndShaderMode();
    EndOpaqueBlit();
    return true;
}

void BeginMultisampleTextureMode(const RenderTexture2D &target) {
    rlDrawRenderBatchActive();
    rlEnableFramebuffer(target.id);
    rlViewport(0, 0, target.texture.width, target.texture.height);
    rlSetFramebufferWidth(target.texture.width);
    rlSetFramebufferHeight(target.texture.height);
    rlMatrixMode(RL_PROJECTION);
    rlLoadIdentity();
    rlOrtho(0, target.texture.width, target.texture.height, 0, 0.0f, 1.0f);
    rlMatrixMode(RL_MODELVIEW);
    rlLoadIdentity();
}

void ResolveMultisampleTexture(const RenderTexture2D &source, const RenderTexture2D &destination) {
    rlDrawRenderBatchActive();
    rlBindFramebuffer(RL_READ_FRAMEBUFFER, source.id);
    rlBindFramebuffer(RL_DRAW_FRAMEBUFFER, destination.id);
    rlBlitFramebuffer(0, 0, source.texture.width, source.texture.height,
                      0, 0, destination.texture.width, destination.texture.height, GL_COLOR_BUFFER_BIT);
    rlDisableFramebuffer();
    rlViewport(0, 0, GetRenderWidth(), GetRenderHeight());
    rlSetFramebufferWidth(GetRenderWidth());
    rlSetFramebufferHeight(GetRenderHeight());
    rlMatrixMode(RL_PROJECTION);
    rlLoadIdentity();
    rlOrtho(0, GetRenderWidth(), GetRenderHeight(), 0, 0.0f, 1.0f);
    rlMatrixMode(RL_MODELVIEW);
    rlLoadIdentity();
}

void BeginOpaqueBlit() {
    rlDrawRenderBatchActive();
    rlDisableDepthTest();
    rlDisableDepthMask();
    rlDisableBackfaceCulling();
    rlDisableColorBlend();
}

void EndOpaqueBlit() {
    rlDrawRenderBatchActive();
    rlEnableColorBlend();
    rlEnableBackfaceCulling();
    rlEnableDepthMask();
    rlEnableDepthTest();
}

void DrawFlippedTexture(Texture2D texture, int width, int height) {
    DrawTexturePro(texture,
                   Rectangle {0.0f, 0.0f, static_cast<float>(texture.width), -static_cast<float>(texture.height)},
                   Rectangle {0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)},
                   Vector2 {}, 0.0f, WHITE);
}

}
