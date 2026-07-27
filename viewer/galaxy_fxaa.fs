#version 330

in vec2 fragTexCoord;

uniform sampler2D texture0;
uniform vec2 sourceTexelSize;
uniform float qualitySubpix;
uniform float qualityEdgeThreshold;
uniform float qualityEdgeThresholdMin;

out vec4 finalColor;

vec3 LinearToSrgb(vec3 value)
{
    value = max(value, vec3(0.0));
    vec3 low = value * 12.92;
    vec3 high = 1.055 * pow(value, vec3(1.0 / 2.4)) - 0.055;
    return mix(low, high, step(vec3(0.0031308), value));
}

float Luma(vec2 uv)
{
    return textureLod(texture0, uv, 0.0).g;
}

vec3 ApplyFxaa(vec2 uv)
{
    vec4 center = textureLod(texture0, uv, 0.0);
    float lumaM = center.g;
    float lumaS = Luma(uv + vec2(0.0, sourceTexelSize.y));
    float lumaE = Luma(uv + vec2(sourceTexelSize.x, 0.0));
    float lumaN = Luma(uv - vec2(0.0, sourceTexelSize.y));
    float lumaW = Luma(uv - vec2(sourceTexelSize.x, 0.0));
    float rangeMax = max(max(lumaN, lumaW), max(lumaM, max(lumaS, lumaE)));
    float rangeMin = min(min(lumaN, lumaW), min(lumaM, min(lumaS, lumaE)));
    float range = rangeMax - rangeMin;
    if (range < max(qualityEdgeThresholdMin, rangeMax * qualityEdgeThreshold)) return center.rgb;

    float lumaNW = Luma(uv - sourceTexelSize);
    float lumaSE = Luma(uv + sourceTexelSize);
    float lumaNE = Luma(uv + vec2(sourceTexelSize.x, -sourceTexelSize.y));
    float lumaSW = Luma(uv + vec2(-sourceTexelSize.x, sourceTexelSize.y));
    float lumaNS = lumaN + lumaS;
    float lumaWE = lumaW + lumaE;
    float subpixNSWE = lumaNS + lumaWE;
    float edgeHorz1 = -2.0 * lumaM + lumaNS;
    float edgeVert1 = -2.0 * lumaM + lumaWE;
    float lumaNESE = lumaNE + lumaSE;
    float lumaNWNE = lumaNW + lumaNE;
    float lumaNWSW = lumaNW + lumaSW;
    float lumaSWSE = lumaSW + lumaSE;
    float edgeHorz = abs(-2.0 * lumaW + lumaNWSW) +
                     abs(edgeHorz1) * 2.0 + abs(-2.0 * lumaE + lumaNESE);
    float edgeVert = abs(-2.0 * lumaS + lumaSWSE) +
                     abs(edgeVert1) * 2.0 + abs(-2.0 * lumaN + lumaNWNE);
    bool horzSpan = edgeHorz >= edgeVert;
    float lengthSign = horzSpan ? sourceTexelSize.y : sourceTexelSize.x;
    if (!horzSpan) {
        lumaN = lumaW;
        lumaS = lumaE;
    }

    float gradientN = lumaN - lumaM;
    float gradientS = lumaS - lumaM;
    bool pairN = abs(gradientN) >= abs(gradientS);
    float gradient = max(abs(gradientN), abs(gradientS));
    if (pairN) lengthSign = -lengthSign;
    float lumaNN = pairN ? lumaN + lumaM : lumaS + lumaM;
    float subpixC = clamp(abs((subpixNSWE * 2.0 + lumaNWSW + lumaNESE) / 12.0 - lumaM) / range,
                          0.0, 1.0);
    float subpixF = (-2.0 * subpixC + 3.0) * subpixC * subpixC;

    vec2 posB = uv;
    vec2 offNP = horzSpan ? vec2(sourceTexelSize.x, 0.0) : vec2(0.0, sourceTexelSize.y);
    if (horzSpan) posB.y += lengthSign * 0.5;
    else posB.x += lengthSign * 0.5;
    vec2 posN = posB - offNP;
    vec2 posP = posB + offNP;
    float edgeAverage = lumaNN * 0.5;
    float lumaEndN = Luma(posN) - edgeAverage;
    float lumaEndP = Luma(posP) - edgeAverage;
    float gradientScaled = gradient * 0.25;
    const float steps[11] = float[11](1.0, 1.0, 1.0, 1.0, 1.5,
                                      2.0, 2.0, 2.0, 2.0, 4.0, 8.0);
    bool doneN = false;
    bool doneP = false;
    for (int i = 0; i < 11; ++i) {
        doneN = abs(lumaEndN) >= gradientScaled;
        doneP = abs(lumaEndP) >= gradientScaled;
        if (doneN && doneP) break;
        if (!doneN) posN -= offNP * steps[i];
        if (!doneP) posP += offNP * steps[i];
        if (i < 10) {
            if (!doneN) lumaEndN = Luma(posN) - edgeAverage;
            if (!doneP) lumaEndP = Luma(posP) - edgeAverage;
        }
    }

    float dstN = horzSpan ? uv.x - posN.x : uv.y - posN.y;
    float dstP = horzSpan ? posP.x - uv.x : posP.y - uv.y;
    bool lumaMLTZero = lumaM - edgeAverage < 0.0;
    bool goodSpanN = (lumaEndN < 0.0) != lumaMLTZero;
    bool goodSpanP = (lumaEndP < 0.0) != lumaMLTZero;
    bool directionN = dstN < dstP;
    float spanLength = dstP + dstN;
    float pixelOffset = 0.5 - min(dstN, dstP) / spanLength;
    float pixelOffsetGood = (directionN ? goodSpanN : goodSpanP) ? pixelOffset : 0.0;
    float pixelOffsetSubpix = max(pixelOffsetGood, subpixF * subpixF * qualitySubpix);
    if (horzSpan) uv.y += pixelOffsetSubpix * lengthSign;
    else uv.x += pixelOffsetSubpix * lengthSign;
    return textureLod(texture0, uv, 0.0).rgb;
}

void main()
{
    finalColor = vec4(LinearToSrgb(clamp(ApplyFxaa(fragTexCoord), 0.0, 1.0)), 1.0);
}
