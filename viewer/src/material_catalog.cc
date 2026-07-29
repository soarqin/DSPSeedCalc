#include "material_catalog.hh"
#include "galaxy.hh"

#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <iterator>

namespace viewer {

float GammaToLinearSpace(float value) {
    if (value <= 0.04045f) return value / 12.92f;
    return std::pow((value + 0.055f) / 1.055f, 2.4f);
}

namespace {
struct GradientKey {
    float position;
    Vector4 color;
};
constexpr GradientKey kStarGradient[] = {
    {0.0f, {1.0f, 0.229039833f, 0.174528301f, 1.0f}},
        {0.06999313f, {1.0f, 0.437254906f, 0.176470578f, 1.0f}},
        {0.31999695f, {1.0f, 0.951527596f, 0.155660391f, 1.0f}},
        {0.50000763f, {0.987870634f, 1.0f, 0.617924571f, 1.0f}},
        {0.723537f, {0.976415098f, 1.0f, 0.97796154f, 1.0f}},
        {0.8200046f, {0.580000043f, 0.896296144f, 1.0f, 1.0f}},
        {0.919996977f, {0.117647052f, 0.654785156f, 1.0f, 1.0f}},
        {1.0f, {0.0990566015f, 0.340047508f, 1.0f, 1.0f}},
};
}

Vector4 GalaxyStarGradient(float color) {
    float value = std::clamp(color, 0.0f, 1.0f);
    for (std::size_t i = 1; i < std::size(kStarGradient); ++i) {
        if (value > kStarGradient[i].position) continue;
        float amount = (value - kStarGradient[i - 1].position) /
                       (kStarGradient[i].position - kStarGradient[i - 1].position);
        return Vector4Lerp(kStarGradient[i - 1].color, kStarGradient[i].color, amount);
    }
    return kStarGradient[std::size(kStarGradient) - 1].color;
}

Vector4 GalaxyStarColor(const dspugen::Star *star) {
    if (star->type == dspugen::EStarType::NeutronStar)
        return Vector4 {0.502143919f, 0.382075489f, 1.0f, 1.0f};
    if (star->type == dspugen::EStarType::WhiteDwarf)
        return Vector4 {0.46226418f, 0.46226418f, 0.46226418f, 1.0f};
    if (star->type == dspugen::EStarType::BlackHole) return Vector4 {0.0f, 0.0f, 0.0f, 1.0f};
    return GalaxyStarGradient(star->color);
}

Vector4 GalaxyLineColor(Vector4 color) {
    Vector4 result = Vector4Lerp(color, Vector4 {1.0f, 1.0f, 1.0f, 1.0f}, 0.65f);
    result.x = GammaToLinearSpace(result.x);
    result.y = GammaToLinearSpace(result.y);
    result.z = GammaToLinearSpace(result.z);
    return result;
}

Vector4 GalaxyGridColor(Vector4 color) {
    return Vector4 {GammaToLinearSpace(color.x), GammaToLinearSpace(color.y), GammaToLinearSpace(color.z),
                    color.w};
}

float GalaxyStarScale(const dspugen::Star *star) {
    switch (star->type) {
        case dspugen::EStarType::GiantStar: return 3.0f;
        case dspugen::EStarType::WhiteDwarf:
        case dspugen::EStarType::NeutronStar: return 0.6f;
        case dspugen::EStarType::BlackHole: return 0.8f;
        default: return 1.2f;
    }
}

Vector4 LerpColor(Vector4 left, Vector4 right, float t) {
    return Vector4Lerp(left, right, t);
}

constexpr float kSpectralParams[] = {-3.5999999046325684f, -3.0f, -2.0f, -1.0f, -0.30000001192092896f,
                                     0.0f, 0.25f, 1.0f, 1.600000023841858f};

struct SpectralBlend {
    std::size_t material = 0;
    std::size_t target = 0;
    float amount = 0.0f;
    bool hasTarget = false;
};

SpectralBlend FindSpectralBlend(float classFactor) {
    SpectralBlend result;
    for (std::size_t i = 0; i + 1 < std::size(kSpectralParams); ++i) {
        if (classFactor >= kSpectralParams[i] && classFactor <= kSpectralParams[i + 1]) {
            result.material = i;
            result.target = i + 1;
            result.amount = std::clamp((classFactor - kSpectralParams[i]) /
                                       (kSpectralParams[i + 1] - kSpectralParams[i]), 0.0f, 1.0f);
            result.hasTarget = true;
        }
    }
    if (classFactor <= kSpectralParams[0]) result.material = 0;
    if (classFactor >= kSpectralParams[std::size(kSpectralParams) - 1])
        result.material = std::size(kSpectralParams) - 1;
    return result;
}

float LerpFloat(float left, float right, float amount) {
    return left + (right - left) * amount;
}

StarMassMaterialData StarMassMaterial(const dspugen::Star *star) {
    static const StarMassMaterialData mainMaterials[] = {
        {{1.0f, 0.629063308f, 0.36470592f, 1.0f}, {0.528301895f, 0.255579025f, 0.0f, 1.0f}, {0.264150918f, 0.0138839362f, 0.00373797794f, 1.0f}, {1.0f, 0.282301843f, 0.0784313679f, 1.0f}, 4.0f, 200.0f, 0.300000012f, 1.29999995f, 0.180000007f, 0.75f, 12.0f, 3.0f, 0.0399999991f},
        {{1.0f, 0.813083529f, 0.363207519f, 1.0f}, {0.632075429f, 0.372084856f, 0.0f, 1.0f}, {0.301886797f, 0.0137221254f, 0.0f, 1.0f}, {1.0f, 0.432450503f, 0.0801886916f, 1.0f}, 4.0f, 200.0f, 0.300000012f, 1.29999995f, 0.180000007f, 0.75f, 12.0f, 3.0f, 0.0399999991f},
        {{0.990566015f, 0.933518291f, 0.406505883f, 1.0f}, {0.575471699f, 0.473466545f, 0.0407173298f, 1.0f}, {0.41568628f, 0.226671472f, 0.0f, 1.0f}, {1.0f, 0.56016314f, 0.26886791f, 1.0f}, 4.0f, 200.0f, 0.300000012f, 1.29999995f, 0.180000007f, 0.800000012f, 12.0f, 4.0f, 0.0399999991f},
        {{0.801886797f, 0.783705056f, 0.624109983f, 1.0f}, {0.556603789f, 0.502054989f, 0.312433243f, 1.0f}, {0.386792481f, 0.253650099f, 0.133187965f, 1.0f}, {0.726999998f, 0.643031478f, 0.391125977f, 1.0f}, 4.0f, 250.0f, 0.300000012f, 1.20000005f, 0.180000007f, 0.699999988f, 12.0f, 3.5f, 0.0399999991f},
        {{0.894999981f, 0.878270984f, 0.815537393f, 1.0f}, {0.490196079f, 0.46526733f, 0.36470589f, 1.0f}, {0.427450985f, 0.422490686f, 0.407843143f, 1.0f}, {0.796999991f, 0.796999991f, 0.665292382f, 1.0f}, 4.0f, 250.0f, 0.300000012f, 1.20000005f, 0.180000007f, 0.899999976f, 12.0f, 5.0f, 0.0399999991f},
        {{0.897672892f, 0.950999975f, 0.866565406f, 1.0f}, {0.506150007f, 0.573000014f, 0.487050027f, 1.0f}, {0.405999988f, 0.405999988f, 0.405999988f, 1.0f}, {0.911330044f, 1.0f, 0.985221684f, 1.0f}, 4.0f, 250.0f, 0.300000012f, 1.20000005f, 0.180000007f, 0.800000012f, 12.0f, 7.0f, 0.0399999991f},
        {{0.78047061f, 0.828764975f, 0.839215696f, 1.0f}, {0.656894147f, 0.737478256f, 0.776470602f, 1.0f}, {0.36470589f, 0.386221647f, 0.392156869f, 1.0f}, {0.74125427f, 0.854135633f, 0.888000011f, 1.0f}, 4.0f, 250.0f, 0.300000012f, 1.20000005f, 0.180000007f, 0.899999976f, 12.0f, 5.0f, 0.0399999991f},
        {{0.630784988f, 0.92203629f, 0.990566015f, 1.0f}, {0.313725501f, 0.39416796f, 0.556862772f, 1.0f}, {0.0813011751f, 0.205825761f, 0.273584902f, 1.0f}, {0.384700954f, 0.570500672f, 0.858490586f, 1.0f}, 4.0f, 280.0f, 0.300000012f, 1.20000005f, 0.180000007f, 0.699999988f, 12.0f, 4.0f, 0.0399999991f},
        {{0.458823562f, 0.737789989f, 1.0f, 1.0f}, {0.164705873f, 0.39788875f, 0.784313738f, 1.0f}, {0.0117647108f, 0.161571428f, 0.301960796f, 1.0f}, {0.286274582f, 0.401634216f, 0.992156863f, 1.0f}, 5.0f, 300.0f, 0.300000012f, 1.0f, 0.180000007f, 0.699999988f, 12.0f, 3.5f, 0.0399999991f},
    };
    static const StarMassMaterialData specialMaterials[] = {
        {{0.816037714f, 0.850495577f, 1.0f, 1.0f}, {0.411764711f, 0.499932766f, 0.549019635f, 1.0f}, {0.129505023f, 0.121568628f, 0.235294119f, 1.0f}, {0.57558459f, 0.372549057f, 1.0f, 1.0f}, 6.0f, 70.0f, 8.0f, 0.899999976f, 0.25f, 0.949999988f, 5.0f, 2.0f, 0.300000012f},
        {{1.0f, 1.0f, 1.0f, 1.0f}, {0.740101993f, 0.752798617f, 0.772549033f, 1.0f}, {0.122821964f, 0.161190778f, 0.216999978f, 1.0f}, {0.552910268f, 0.595265687f, 0.669811308f, 1.0f}, 4.0f, 60.0f, 0.0500000007f, 1.5f, 0.300000012f, 0.850000024f, 5.0f, 2.79999995f, 1.5f},
        {{0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 1.0f}, {0.264150918f, 0.264150918f, 0.264150918f, 1.0f}, 6.0f, 6.0f, 0.400000006f, 0.899999976f, 0.25f, 0.850000024f, 0.0f, 4.0f, 0.300000012f},
    };

    if (star->type == dspugen::EStarType::NeutronStar) return specialMaterials[0];
    if (star->type == dspugen::EStarType::WhiteDwarf) return specialMaterials[1];
    if (star->type == dspugen::EStarType::BlackHole) return specialMaterials[2];
    auto blend = FindSpectralBlend(star->classFactor);
    StarMassMaterialData result = mainMaterials[blend.material];
    if (blend.hasTarget) {
        const auto &target = mainMaterials[blend.target];
        result.color0 = LerpColor(result.color0, target.color0, blend.amount);
        result.color1 = LerpColor(result.color1, target.color1, blend.amount);
        result.color2 = LerpColor(result.color2, target.color2, blend.amount);
        result.color3 = LerpColor(result.color3, target.color3, blend.amount);
        result.multiplier = LerpFloat(result.multiplier, target.multiplier, blend.amount);
        result.noiseTile = LerpFloat(result.noiseTile, target.noiseTile, blend.amount);
        result.noiseSpeed = LerpFloat(result.noiseSpeed, target.noiseSpeed, blend.amount);
        result.chaosTile = LerpFloat(result.chaosTile, target.chaosTile, blend.amount);
        result.chaosDistort = LerpFloat(result.chaosDistort, target.chaosDistort, blend.amount);
        result.chaosOverlay = LerpFloat(result.chaosOverlay, target.chaosOverlay, blend.amount);
        result.spotIntensity = LerpFloat(result.spotIntensity, target.spotIntensity, blend.amount);
        result.rimPower = LerpFloat(result.rimPower, target.rimPower, blend.amount);
        result.rotSpeed = LerpFloat(result.rotSpeed, target.rotSpeed, blend.amount);
    }
    if (star->type == dspugen::EStarType::GiantStar) {
        result.noiseTile *= 3.0f;
        result.chaosTile /= 3.0f;
    }
    return result;
}

StarSpotMaterialData StarSpotMaterial(const dspugen::Star *star) {
    static const StarSpotMaterialData mainMaterials[] = {
        {{1.0f, 0.466861874f, 0.258823514f, 1.0f}, {1.0f, 0.0f, 0.15857029f, 1.0f}, 1.0f, 5.0f, 5.0f, 0.300000012f, 0.0500000007f, 2.0f},
        {{1.0f, 0.571663201f, 0.334905684f, 1.0f}, {1.0f, 0.426606327f, 0.0f, 1.0f}, 1.0f, 5.0f, 5.0f, 0.300000012f, 0.0500000007f, 2.0f},
        {{1.0f, 0.789915204f, 0.334905684f, 1.0f}, {1.0f, 0.591718853f, 0.0f, 1.0f}, 1.0f, 5.0f, 5.0f, 0.300000012f, 0.0500000007f, 2.0f},
        {{1.0f, 0.726871908f, 0.353773594f, 1.0f}, {1.0f, 0.876956701f, 0.627358496f, 1.0f}, 1.0f, 5.0f, 5.0f, 0.300000012f, 0.0500000007f, 2.0f},
        {{0.930000007f, 0.99667722f, 1.0f, 1.0f}, {0.908999979f, 0.98862499f, 1.0f, 1.0f}, 1.0f, 5.0f, 5.0f, 0.300000012f, 0.0500000007f, 2.0f},
        {{0.957547188f, 1.0f, 0.984421909f, 1.0f}, {0.99000001f, 1.0f, 0.993749976f, 1.0f}, 1.0f, 5.0f, 5.0f, 0.300000012f, 0.0500000007f, 2.0f},
        {{1.0f, 0.99902153f, 0.908999979f, 1.0f}, {1.0f, 0.995625019f, 0.930000007f, 1.0f}, 1.0f, 5.0f, 5.0f, 0.300000012f, 0.0500000007f, 2.0f},
        {{0.391509414f, 0.82864356f, 1.0f, 1.0f}, {0.419811308f, 0.640218198f, 1.0f, 1.0f}, 1.0f, 5.0f, 5.0f, 0.300000012f, 0.0500000007f, 2.0f},
        {{0.221698105f, 0.583651304f, 1.0f, 1.0f}, {0.221698105f, 0.391599625f, 1.0f, 1.0f}, 1.0f, 5.0f, 5.0f, 0.300000012f, 0.0500000007f, 2.0f},
    };
    static const StarSpotMaterialData specialMaterials[] = {
        {{0.433218956f, 0.380392134f, 1.0f, 1.0f}, {0.617746413f, 0.258823484f, 1.0f, 1.0f}, 0.600000024f, 5.0f, 12.0f, 0.300000012f, 0.0500000007f, 2.0f},
        {{0.929245293f, 0.941550434f, 1.0f, 1.0f}, {0.909803927f, 0.911484003f, 1.0f, 1.0f}, 1.0f, 5.0f, 5.0f, 0.300000012f, 0.0500000007f, 2.0f},
        {{0.837492347f, 0.580188632f, 1.0f, 1.0f}, {1.0f, 0.333333343f, 0.760824859f, 1.0f}, 0.600000024f, 5.0f, 5.0f, 0.319999993f, 0.0500000007f, 10.0f},
    };

    if (star->type == dspugen::EStarType::NeutronStar) return specialMaterials[0];
    if (star->type == dspugen::EStarType::WhiteDwarf) return specialMaterials[1];
    if (star->type == dspugen::EStarType::BlackHole) return specialMaterials[2];
    auto blend = FindSpectralBlend(star->classFactor);
    StarSpotMaterialData result = mainMaterials[blend.material];
    if (!blend.hasTarget) return result;
    const auto &target = mainMaterials[blend.target];
    result.centerColor = LerpColor(result.centerColor, target.centerColor, blend.amount);
    result.glowColor = LerpColor(result.glowColor, target.glowColor, blend.amount);
    result.multiplier = LerpFloat(result.multiplier, target.multiplier, blend.amount);
    result.glowPower = LerpFloat(result.glowPower, target.glowPower, blend.amount);
    result.centerMultiplier = LerpFloat(result.centerMultiplier, target.centerMultiplier, blend.amount);
    result.centerBlurPower = LerpFloat(result.centerBlurPower, target.centerBlurPower, blend.amount);
    return result;
}

PlanetThumbMaterialData PlanetThumbMaterial(const dspugen::Planet *planet) {
    static const PlanetThumbMaterialData materials[] = {
    // theme 0 never reaches here: PlanetGen emits 1 through 25. Neutral values.
    {{0.63f, 0.63f, 0.68f, 1.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, {0.85f, 0.85f, 0.9f, 1.0f},
     {0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f},
     {-0.1f, 0.3f, 0.1f, 0.1f}, 13.0f, 0.0f, 2.0f, 0.0f, 0.5f, 0.100000001f},
    // material 1298
    {{0.31764707f, 0.974079907f, 1.0f, 1.0f}, {0.267265826f, 0.363986194f, 0.396226317f, 1.0f}, {0.70014894f, 1.0f, 0.533018768f, 1.0f},
     {0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f},
     {-1.0f, 0.0f, 0.100000001f, 0.300000012f}, 6.0f, 0.0f, 2.0f, 0.0f, 0.5f, 0.100000001f},
    // material 1237
    {{1.0f, 0.737077832f, 0.683962166f, 1.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 0.63740623f, 0.391509354f, 1.0f},
     {1.0f, 0.688800216f, 0.608490586f, 1.0f}, {0.801886797f, 0.34582296f, 0.291251212f, 1.0f}, {1.0f, 0.677347004f, 0.627358496f, 1.0f},
     {-0.100000001f, 0.300000012f, 0.100000001f, 0.100000001f}, 13.0f, 0.0f, 2.0f, 0.349999994f, 0.5f, 0.400000006f},
    // material 1245
    {{1.0f, 0.893302858f, 0.68235296f, 1.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 0.793441594f, 0.39215681f, 1.0f},
     {1.0f, 0.813174486f, 0.607843161f, 1.0f}, {0.800000012f, 0.548543751f, 0.290196121f, 1.0f}, {1.0f, 0.850248575f, 0.627451062f, 1.0f},
     {-0.100000001f, 0.300000012f, 0.100000001f, 0.100000001f}, 13.0f, 0.0f, 2.0f, 0.349999994f, 0.5f, 0.400000006f},
    // material 1251
    {{0.68235296f, 1.0f, 0.86861521f, 1.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, {0.39215681f, 1.0f, 0.898458004f, 1.0f},
     {0.76886797f, 1.0f, 0.913477361f, 1.0f}, {0.290196121f, 0.800000012f, 0.562593877f, 1.0f}, {0.627451062f, 1.0f, 0.863983214f, 1.0f},
     {-0.100000001f, 0.300000012f, 0.100000001f, 0.100000001f}, 13.0f, 0.0f, 2.0f, 0.349999994f, 0.5f, 0.400000006f},
    // material 1259
    {{0.21226415f, 0.421354353f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, {0.21226418f, 0.421592534f, 1.0f, 1.0f},
     {0.627358496f, 0.794822335f, 1.0f, 1.0f}, {0.290196121f, 0.430219144f, 0.800000012f, 1.0f}, {0.627451062f, 0.7820068f, 1.0f, 1.0f},
     {-0.100000001f, 0.300000012f, 0.100000001f, 0.100000001f}, 13.0f, 0.0f, 2.0f, 0.349999994f, 0.5f, 0.400000006f},
    // material 1177
    {{1.0f, 0.613482177f, 0.533333302f, 1.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 0.657547176f, 0.429245234f, 1.0f},
     {1.0f, 0.649861515f, 0.561320722f, 1.0f}, {0.801886797f, 0.34582299f, 0.291251242f, 1.0f}, {1.0f, 0.677347004f, 0.627358496f, 1.0f},
     {-0.100000001f, 0.300000012f, 0.100000001f, 0.100000001f}, 13.0f, 0.0f, 2.0f, 0.349999994f, 0.5f, 0.100000001f},
    // material 1196
    {{0.594784617f, 0.68418479f, 0.773584902f, 1.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, {0.623576045f, 0.716571808f, 0.764150977f, 1.0f},
     {0.126156956f, 0.209185243f, 0.254716963f, 1.0f}, {0.438768208f, 0.551876485f, 0.641509414f, 1.0f}, {0.601949155f, 0.712541282f, 0.764150977f, 1.0f},
     {-0.100000001f, 0.300000012f, 0.0500000007f, 0.0500000007f}, 6.0f, 0.0f, 2.0f, 0.600000024f, 0.5f, 0.100000001f},
    // material 1305
    {{0.364705861f, 1.0f, 0.942396462f, 1.0f}, {0.267265797f, 0.363986164f, 0.396226287f, 1.0f}, {0.94343096f, 1.0f, 0.533333302f, 1.0f},
     {0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f},
     {-1.0f, 0.0f, 0.100000001f, 0.300000012f}, 4.0f, 0.0f, 2.0f, 0.0f, 0.5f, 0.100000001f},
    // material 1286
    {{1.0f, 0.390114635f, 0.117647029f, 0.552941203f}, {0.556862772f, 0.407498002f, 0.249474466f, 1.0f}, {0.55660373f, 0.437134475f, 0.412201762f, 1.0f},
     {0.235294044f, 0.164580345f, 0.159529358f, 0.0f}, {0.349999934f, 0.23440358f, 0.23440358f, 0.0f}, {0.377999932f, 0.3225137f, 0.364128381f, 0.0196078438f},
     {-3.0f, 0.0f, 1.0f, 0.0f}, 0.5f, -1.5f, 2.0f, 2.0f, 0.5f, 0.100000001f},
    // material 1279
    {{0.17359373f, 0.390347809f, 0.783018827f, 1.0f}, {0.436276138f, 0.499739945f, 0.716981173f, 0.0f}, {0.215601474f, 0.3932859f, 0.896226406f, 1.0f},
     {0.47726053f, 0.546652317f, 0.613207579f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.929245293f, 0.94619149f, 1.0f, 0.588235319f},
     {1.5f, 2.29999995f, 0.159999996f, 0.0500000007f}, 6.0f, 0.0f, 2.0f, 0.589999974f, 1.0f, 0.100000001f},
    // material 1200
    {{0.678431392f, 0.678431392f, 0.678431392f, 1.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, {0.850980401f, 0.850980401f, 0.850980401f, 1.0f},
     {0.349019587f, 0.349019587f, 0.349019587f, 1.0f}, {0.576470554f, 0.576470554f, 0.576470554f, 1.0f}, {0.839215696f, 0.839215696f, 0.839215696f, 1.0f},
     {-0.100000001f, 0.300000012f, 0.0500000007f, 0.0500000007f}, 6.0f, 0.0f, 2.0f, 0.600000024f, 0.5f, 0.100000001f},
    // material 1204
    {{0.754716933f, 0.716123521f, 0.566037774f, 1.0f}, {0.358490348f, 0.267963886f, 0.130206227f, 0.0f}, {0.0f, 0.905660391f, 0.677003562f, 1.0f},
     {0.992156804f, 0.863234758f, 0.552941144f, 1.0f}, {0.796078444f, 0.562193632f, 0.211756766f, 1.0f}, {0.286274433f, 0.143856794f, 0.00784314144f, 1.0f},
     {-0.100000001f, 0.300000012f, 0.0500000007f, 0.0500000007f}, 6.0f, 0.0f, 2.0f, 0.600000024f, 0.5f, 0.100000001f},
    // material 1340
    {{0.39930892f, 0.45395124f, 0.5169999f, 1.0f}, {0.528301895f, 0.454897463f, 0.346386582f, 0.0f}, {0.726999998f, 0.877150118f, 1.0f, 1.0f},
     {0.816666245f, 0.796548963f, 0.923000038f, 1.0f}, {0.223529309f, 0.0941175446f, 0.356862664f, 1.0f}, {0.411764681f, 0.0230588224f, 0.321630001f, 1.0f},
     {-0.100000001f, 0.300000012f, 0.0500000007f, 0.0500000007f}, 6.0f, 0.0f, 2.0f, 0.600000024f, 0.5f, 0.100000001f},
    // material 1312
    {{0.916000009f, 0.558636725f, 0.550421536f, 1.0f}, {0.678431392f, 0.461224735f, 0.166215658f, 1.0f}, {1.0f, 0.608490586f, 0.764849603f, 1.0f},
     {0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 1.0f},
     {-1.0f, 0.0f, 0.100000001f, 0.300000012f}, 4.0f, 0.0f, 2.0f, 0.0f, 0.5f, 0.100000001f},
    // material 1319
    {{0.488182306f, 1.0f, 0.36470592f, 1.0f}, {0.267265826f, 0.363986194f, 0.396226317f, 1.0f}, {0.70014894f, 1.0f, 0.533018768f, 1.0f},
     {0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f},
     {-1.0f, 0.0f, 0.100000001f, 0.300000012f}, 4.0f, 0.0f, 2.0f, 0.0f, 0.5f, 0.100000001f},
    // material 1326
    {{0.31764707f, 0.974079907f, 1.0f, 1.0f}, {0.267265826f, 0.363986194f, 0.396226317f, 1.0f}, {0.70014894f, 1.0f, 0.533018768f, 1.0f},
     {0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 0.0f},
     {-1.0f, 0.0f, 0.100000001f, 0.300000012f}, 6.0f, 0.0f, 2.0f, 0.0f, 0.5f, 0.100000001f},
    // material 1208
    {{0.925490201f, 0.861179173f, 0.823529422f, 1.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 0.657547176f, 0.429245234f, 1.0f},
     {1.0f, 0.948292017f, 0.872641504f, 1.0f}, {0.699000001f, 0.178944007f, 0.0f, 1.0f}, {0.0188679099f, 0.0188679099f, 0.0188679099f, 1.0f},
     {-0.100000001f, 0.300000012f, 0.100000001f, 0.100000001f}, 13.0f, 0.0f, 2.0f, 0.600000024f, 0.5f, 0.100000001f},
    // material 1333
    {{0.947095871f, 0.848789632f, 0.96226418f, 1.0f}, {0.843049169f, 0.62637943f, 0.867924511f, 1.0f}, {0.589556694f, 0.0f, 1.0f, 1.0f},
     {0.773584902f, 0.550996721f, 0.550996721f, 0.0f}, {1.0f, 1.0f, 1.0f, 0.0f}, {0.227216065f, 0.262433529f, 0.433962256f, 0.0f},
     {-1.0f, 0.0f, 0.100000001f, 0.300000012f}, 2.0f, 0.0f, 1.07000005f, 0.0f, 0.5f, 0.100000001f},
    // material 1212
    {{0.501960814f, 0.451389819f, 0.431372553f, 1.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, {0.165094316f, 1.0f, 0.797282696f, 1.0f},
     {1.0f, 0.948292017f, 0.872641504f, 1.0f}, {0.622641504f, 0.117105193f, 0.0910466313f, 1.0f}, {0.0188679099f, 0.0188679099f, 0.0188679099f, 1.0f},
     {-0.100000001f, 0.300000012f, 0.100000001f, 0.100000001f}, 13.0f, 0.0f, 2.0f, 0.600000024f, 0.5f, 0.100000001f},
    // material 1219
    {{0.839622617f, 0.693084717f, 0.704714715f, 1.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, {0.0705882236f, 0.651593029f, 0.858823538f, 1.0f},
     {1.0f, 1.0f, 1.0f, 1.0f}, {0.51886791f, 0.111226812f, 0.0758721903f, 1.0f}, {0.726415098f, 0.449429244f, 0.250133485f, 1.0f},
     {-0.100000001f, 0.300000012f, 0.100000001f, 0.100000001f}, 15.0299997f, 0.0f, 2.0f, 0.600000024f, 0.5f, 0.100000001f},
    // material 1262
    {{0.279369891f, 0.708091974f, 0.811320782f, 1.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, {0.211764693f, 0.74371165f, 1.0f, 1.0f},
     {0.52558738f, 0.797712207f, 0.877358496f, 1.0f}, {0.290196151f, 0.570994675f, 0.800000012f, 1.0f}, {0.627451062f, 0.7820068f, 1.0f, 1.0f},
     {-0.100000001f, 0.300000012f, 0.100000001f, 0.100000001f}, 13.0f, 0.0f, 2.0f, 0.349999994f, 0.5f, 0.400000006f},
    // material 1226
    {{0.422582775f, 0.549019635f, 0.368627459f, 1.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0879764557f, 0.773584902f, 1.0f},
     {0.658441246f, 0.734000027f, 0.0f, 1.0f}, {0.930000007f, 0.744000018f, 0.313647032f, 1.0f}, {0.796999991f, 0.49462074f, 0.29568699f, 1.0f},
     {-0.100000001f, 0.300000012f, 0.100000001f, 0.100000001f}, 15.0f, 0.0f, 2.0f, 0.800000012f, 0.5f, 0.100000001f},
    // material 1230
    {{0.0920705944f, 0.220235169f, 0.337254912f, 1.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, {0.438679218f, 0.701035619f, 1.0f, 1.0f},
     {0.866999984f, 0.54524219f, 0.182069972f, 1.0f}, {0.043903999f, 0.251197159f, 0.39200002f, 1.0f}, {0.144395992f, 0.390213042f, 0.573000014f, 1.0f},
     {-0.100000001f, 0.300000012f, 0.100000001f, 0.100000001f}, 13.0f, 0.0f, 2.0f, 0.800000012f, 0.5f, 0.100000001f},
    // material 1183
    {{0.197579205f, 0.208793163f, 0.226415098f, 1.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.708162427f, 0.896226406f, 1.0f},
     {0.0206924137f, 0.0811009109f, 0.141509414f, 1.0f}, {0.235294133f, 0.325490206f, 0.388235331f, 1.0f}, {0.982905984f, 0.995726466f, 1.0f, 1.0f},
     {-0.100000001f, 0.300000012f, 0.100000001f, 0.100000001f}, 15.0f, 0.0f, 2.5999999f, 0.800000012f, 0.5f, 0.100000001f},
    // material 1192
    {{0.109803915f, 0.133629814f, 0.321568638f, 1.0f}, {0.0121484501f, 0.0316117853f, 0.122641504f, 1.0f}, {0.0f, 0.55506897f, 1.0f, 1.0f},
     {0.0283908956f, 0.0864956453f, 0.207547188f, 1.0f}, {0.119259514f, 0.377358496f, 0.264820606f, 1.0f}, {0.195309713f, 0.655848801f, 0.726415098f, 1.0f},
     {-0.100000001f, 0.300000012f, 0.100000001f, 0.100000001f}, -10.0f, 0.0f, 2.0f, 1.0f, 0.5f, 0.100000001f},
    // material 1236
    {{1.0f, 0.715175867f, 0.655660391f, 1.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 0.514081478f, 0.392156839f, 1.0f},
     {1.0f, 0.57891506f, 0.466981113f, 1.0f}, {0.801886797f, 0.34582296f, 0.291251212f, 1.0f}, {1.0f, 0.677347004f, 0.627358496f, 1.0f},
     {-0.100000001f, 0.300000012f, 0.100000001f, 0.100000001f}, 13.0f, 0.0f, 2.0f, 0.349999994f, 0.5f, 0.400000006f},
    // material 1244
    {{0.670588255f, 0.490196079f, 0.534301162f, 1.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 0.392156839f, 0.636116445f, 1.0f},
     {0.915094316f, 0.591358125f, 0.690125048f, 1.0f}, {0.800000012f, 0.290196151f, 0.717332423f, 1.0f}, {1.0f, 0.627451003f, 0.947680831f, 1.0f},
     {-0.100000001f, 0.300000012f, 0.100000001f, 0.100000001f}, 13.0f, 0.0f, 2.0f, 0.349999994f, 0.5f, 0.400000006f},
    // material 1250
    {{0.39537558f, 0.880999982f, 0.562980533f, 1.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, {0.39215681f, 1.0f, 0.898458004f, 1.0f},
     {0.76886797f, 1.0f, 0.913477361f, 1.0f}, {0.290196121f, 0.800000012f, 0.562593877f, 1.0f}, {0.627451062f, 1.0f, 0.863983214f, 1.0f},
     {-0.100000001f, 0.300000012f, 0.100000001f, 0.100000001f}, 13.0f, 0.0f, 2.0f, 0.349999994f, 0.5f, 0.400000006f},
    // material 1258
    {{0.634163976f, 0.438679218f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 0.0f}, {0.64533478f, 0.400943398f, 1.0f, 1.0f},
     {0.872978389f, 0.816037714f, 1.0f, 1.0f}, {0.290196121f, 0.430219144f, 0.800000012f, 1.0f}, {0.627451062f, 0.7820068f, 1.0f, 1.0f},
     {-0.100000001f, 0.300000012f, 0.100000001f, 0.100000001f}, 13.0f, 0.0f, 2.0f, 0.349999994f, 0.5f, 0.400000006f},
    // material 1277
    {{0.73243922f, 0.771671414f, 0.831372559f, 1.0f}, {0.285777867f, 0.301936239f, 0.358490586f, 0.0f}, {0.716447115f, 0.755622327f, 0.867924511f, 1.0f},
     {0.0f, 0.204634547f, 0.321568638f, 1.0f}, {0.905660391f, 0.905660391f, 0.905660391f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f},
     {1.5f, 2.29999995f, 0.159999996f, 0.0500000007f}, 6.0f, 0.0f, 2.0f, 0.589999974f, 1.0f, 0.100000001f},
    // material 1278
    {{0.778301895f, 0.831293106f, 1.0f, 1.0f}, {0.5f, 0.372558355f, 0.285377353f, 0.0f}, {0.466981113f, 0.593418181f, 1.0f, 1.0f},
     {0.650943398f, 0.237080842f, 0.168876842f, 0.956862748f}, {0.891509414f, 0.945806205f, 1.0f, 0.921568632f}, {0.882075489f, 0.936407387f, 1.0f, 0.937254906f},
     {1.5f, 2.29999995f, 0.159999996f, 0.0500000007f}, 6.0f, 0.0f, 2.0f, 0.589999974f, 1.0f, 0.100000001f},
    // Material 1291 is the one thumb material no theme in `ThemeProtoSet.json` reaches.
    };
    constexpr int kThemeRowCount = 26;
    int theme = std::clamp(planet->theme, 1, kThemeRowCount - 1);
    int style = planet->themeSeed % 60;
    // `UIStarmapPlanet._OnInit` picks `themeProto.thumbMat[planet.style % thumbMat.Length]`.
    // Themes 2 through 5 carry two entries and theme 10 carries three; those extra variants are
    // appended after the per theme rows in the same order the asset serialises them.
    int index = theme;
    if (theme >= 2 && theme <= 5 && style % 2 == 1) index = kThemeRowCount + theme - 2;
    if (theme == 10 && style % 3 == 1) index = kThemeRowCount + 4;
    if (theme == 10 && style % 3 == 2) index = kThemeRowCount + 5;
    PlanetThumbMaterialData result = materials[index];
    // The six colours are `Color` shader properties, so Unity uploads them linear.
    result.color = GalaxyGridColor(result.color);
    result.shoreLineColor = GalaxyGridColor(result.shoreLineColor);
    result.rimColor = GalaxyGridColor(result.rimColor);
    result.colorBio0 = GalaxyGridColor(result.colorBio0);
    result.colorBio1 = GalaxyGridColor(result.colorBio1);
    result.colorBio2 = GalaxyGridColor(result.colorBio2);
    return result;
}

}
