#pragma once

#include "post_process.hh"
#include "render_resources.hh"
#include "view_model.hh"

#include <cstddef>
#include <vector>

namespace viewer {

class SceneRenderer {
public:
    SceneRenderer(const RenderResources &resources, const PostProcess &postProcess);

    float StarViewRadius(const dspugen::Star *star);
    float StarVisualScale(const StarViewData &star, const Camera3D &camera);
    void DrawGalaxy(const std::vector<GalaxyLineData> &lines,
                    const std::vector<StarViewData> &stars,
                    const Camera3D &camera, float time, std::size_t markedStar,
                    std::size_t invalidIndex);
    void DrawSystem(const SystemViewCache &system, const Camera3D &camera,
                    float time, std::size_t selectedPlanet, std::size_t invalidIndex);

private:
    void DrawGalaxyLineQuad(Vector3 start, Vector3 end, const Camera3D &camera, float width);
    float RawStarSpotLevel(const StarViewData &star, const Camera3D &camera);
    float StarSpotLevel(const StarViewData &star, const Camera3D &camera);
    Matrix ViewProjection();
    Matrix StarViewProjection();
    void SetStarMatrices(Shader shader, Matrix transform);
    Matrix StarTransform(Vector3 position, float scale);
    void SetGalaxyPointUniforms(float pointGlow, float multiplier, float alphaMultiplier);
    void DrawStarMass(const StarViewData &star, const Camera3D &camera, float time);
    void DrawStarBlack(const StarViewData &star, const Camera3D &camera);
    void DrawStarSpot(const StarViewData &star, const Camera3D &camera, float visualScale);
    void DrawGalaxyStarPoint(const StarViewData &star, const Camera3D &camera);
    void DrawGalaxyLines(const std::vector<GalaxyLineData> &lines,
                         const std::vector<StarViewData> &stars, const Camera3D &camera);
    float SceneLinear() const;
    void DrawGalaxyGridPlane(const Texture2D &texture, float size, float height, Vector4 tint,
                             float multiplier, float alphaMultiplier, float tiling,
                             float areaMin, float areaMax, float nearFade);
    void DrawGalaxyFrame(float size, float height, Vector4 color, float multiplier);
    void DrawGalaxyGrid();
    void DrawGalaxyBackgroundStars(float time);
    void DrawGalaxyStarMarker(const Texture2D &texture, Vector3 position, float size, Vector4 tint,
                              float multiplier, float alphaMultiplier);
    void DrawBackgroundStars(const Camera3D &camera, bool bloomVisible = true);
    void DrawCachedLines(const std::vector<Line3DCache> &lines, Color color);
    void DrawOrbit(Vector3 center, const PlanetViewData &planet);
    void DrawPlanet(const PlanetViewData &planet, const Camera3D &camera, float time, bool selected);

    const RenderResources &resources;
    const PostProcess &postProcess;
};

}
