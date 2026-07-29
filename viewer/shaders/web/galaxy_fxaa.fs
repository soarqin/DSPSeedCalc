#version 300 es
precision highp float;
precision highp int;

in highp vec2 fragTexCoord;

uniform highp sampler2D texture0;
uniform highp vec2 sourceTexelSize;
uniform highp float qualitySubpix;
uniform highp float qualityEdgeThreshold;
uniform highp float qualityEdgeThresholdMin;
uniform highp float fxaaEnabled;

out highp vec4 finalColor;

highp vec3 LinearToSrgb(highp vec3 value)
{
    value = max(value, vec3(0.0));
    highp vec3 low = value * 12.92;
    highp vec3 high = 1.055 * pow(value, vec3(1.0 / 2.4)) - 0.055;
    return mix(low, high, step(vec3(0.0031308), value));
}

highp float Luma(highp vec2 uv)
{
    return textureLod(texture0, uv, 0.0).g;
}

highp vec3 ApplyFxaa(highp vec2 uv)
{
    highp vec4 center = textureLod(texture0, uv, 0.0);
    highp float lumaM = center.g;
    highp float lumaS = Luma(uv + vec2(0.0, sourceTexelSize.y));
    highp float lumaE = Luma(uv + vec2(sourceTexelSize.x, 0.0));
    highp float lumaN = Luma(uv - vec2(0.0, sourceTexelSize.y));
    highp float lumaW = Luma(uv - vec2(sourceTexelSize.x, 0.0));
    highp float rangeMax = max(max(lumaN, lumaW), max(lumaM, max(lumaS, lumaE)));
    highp float rangeMin = min(min(lumaN, lumaW), min(lumaM, min(lumaS, lumaE)));
    highp float range = rangeMax - rangeMin;
    if (range < max(qualityEdgeThresholdMin, rangeMax * qualityEdgeThreshold)) return center.rgb;

    highp float lumaNW = Luma(uv - sourceTexelSize);
    highp float lumaSE = Luma(uv + sourceTexelSize);
    highp float lumaNE = Luma(uv + vec2(sourceTexelSize.x, -sourceTexelSize.y));
    highp float lumaSW = Luma(uv + vec2(-sourceTexelSize.x, sourceTexelSize.y));
    highp float lumaNS = lumaN + lumaS;
    highp float lumaWE = lumaW + lumaE;
    highp float subpixNSWE = lumaNS + lumaWE;
    highp float edgeHorz1 = -2.0 * lumaM + lumaNS;
    highp float edgeVert1 = -2.0 * lumaM + lumaWE;
    highp float lumaNESE = lumaNE + lumaSE;
    highp float lumaNWNE = lumaNW + lumaNE;
    highp float lumaNWSW = lumaNW + lumaSW;
    highp float lumaSWSE = lumaSW + lumaSE;
    highp float edgeHorz = abs(-2.0 * lumaW + lumaNWSW) +
                           abs(edgeHorz1) * 2.0 + abs(-2.0 * lumaE + lumaNESE);
    highp float edgeVert = abs(-2.0 * lumaS + lumaSWSE) +
                           abs(edgeVert1) * 2.0 + abs(-2.0 * lumaN + lumaNWNE);
    bool horzSpan = edgeHorz >= edgeVert;
    highp float lengthSign = horzSpan ? sourceTexelSize.y : sourceTexelSize.x;
    if (!horzSpan) {
        lumaN = lumaW;
        lumaS = lumaE;
    }

    highp float gradientN = lumaN - lumaM;
    highp float gradientS = lumaS - lumaM;
    bool pairN = abs(gradientN) >= abs(gradientS);
    highp float gradient = max(abs(gradientN), abs(gradientS));
    if (pairN) lengthSign = -lengthSign;
    highp float lumaNN = pairN ? lumaN + lumaM : lumaS + lumaM;
    highp float subpixC = clamp(abs((subpixNSWE * 2.0 + lumaNWSW + lumaNESE) / 12.0 - lumaM) / range,
                                0.0, 1.0);
    highp float subpixF = (-2.0 * subpixC + 3.0) * subpixC * subpixC;

    highp vec2 posB = uv;
    highp vec2 offNP = horzSpan ? vec2(sourceTexelSize.x, 0.0) : vec2(0.0, sourceTexelSize.y);
    if (horzSpan) posB.y += lengthSign * 0.5;
    else posB.x += lengthSign * 0.5;
    highp vec2 posN = posB - offNP;
    highp vec2 posP = posB + offNP;
    highp float edgeAverage = lumaNN * 0.5;
    highp float lumaEndN = Luma(posN) - edgeAverage;
    highp float lumaEndP = Luma(posP) - edgeAverage;
    highp float gradientScaled = gradient * 0.25;
    const highp float steps[11] = float[11](1.0, 1.0, 1.0, 1.0, 1.5,
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

    highp float dstN = horzSpan ? uv.x - posN.x : uv.y - posN.y;
    highp float dstP = horzSpan ? posP.x - uv.x : posP.y - uv.y;
    bool lumaMLTZero = lumaM - edgeAverage < 0.0;
    bool goodSpanN = (lumaEndN < 0.0) != lumaMLTZero;
    bool goodSpanP = (lumaEndP < 0.0) != lumaMLTZero;
    bool directionN = dstN < dstP;
    highp float spanLength = dstP + dstN;
    highp float pixelOffset = 0.5 - min(dstN, dstP) / spanLength;
    highp float pixelOffsetGood = (directionN ? goodSpanN : goodSpanP) ? pixelOffset : 0.0;
    highp float pixelOffsetSubpix = max(pixelOffsetGood, subpixF * subpixF * qualitySubpix);
    if (horzSpan) uv.y += pixelOffsetSubpix * lengthSign;
    else uv.x += pixelOffsetSubpix * lengthSign;
    return textureLod(texture0, uv, 0.0).rgb;
}

void main()
{
    // PostEffectController drives antialiasing.enabled from the FXAA option, and the game still
    // writes the graded result straight to the display when it is off.
    highp vec3 color = fxaaEnabled > 0.5 ? ApplyFxaa(fragTexCoord)
                                         : textureLod(texture0, fragTexCoord, 0.0).rgb;
    finalColor = vec4(LinearToSrgb(clamp(color, 0.0, 1.0)), 1.0);
}
