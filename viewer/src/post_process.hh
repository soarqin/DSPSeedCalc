#pragma once

#include <raylib.h>

namespace viewer {

struct RenderResources;

class PostProcess {
public:
    bool EnsureTargets(int width, int height);
    void Unload();

    RenderTexture2D &SceneTarget() { return sceneTarget; }
    const RenderTexture2D &SceneTarget() const { return sceneTarget; }
    RenderTexture2D &MultisampleTarget() { return multisampleTarget; }
    const RenderTexture2D &MultisampleTarget() const { return multisampleTarget; }
    RenderTexture2D &FxaaTarget() { return fxaaTarget; }
    const RenderTexture2D &FxaaTarget() const { return fxaaTarget; }
    RenderTexture2D &PrefilterTarget() { return prefilterTarget; }
    const RenderTexture2D &PrefilterTarget() const { return prefilterTarget; }
    RenderTexture2D *DownLevels() { return downLevels; }
    RenderTexture2D *UpLevels() { return upLevels; }
    int IterationCount() const { return iterationCount; }
    float SampleScale() const { return sampleScale; }
    bool Draw(const Camera3D &camera, const RenderResources &resources,
              const void *context, void (*drawScene)(const void *, float));

private:
    static constexpr int kMaxBloomIterations = 16;
    RenderTexture2D sceneTarget{};
    RenderTexture2D multisampleTarget{};
    RenderTexture2D fxaaTarget{};
    RenderTexture2D prefilterTarget{};
    RenderTexture2D downLevels[kMaxBloomIterations]{};
    RenderTexture2D upLevels[kMaxBloomIterations]{};
    int targetWidth = 0;
    int targetHeight = 0;
    int failedWidth = 0;
    int failedHeight = 0;
    int iterationCount = 0;
    float sampleScale = 1.0f;
};

void BeginMultisampleTextureMode(const RenderTexture2D &target);
void ResolveMultisampleTexture(const RenderTexture2D &source, const RenderTexture2D &destination);
void BeginOpaqueBlit();
void EndOpaqueBlit();
void DrawFlippedTexture(Texture2D texture, int width, int height);

}
