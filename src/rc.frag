#version 430 core

#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

in vec2 uv;
in vec4 color;

out vec4 FragColor;

uniform ivec2   u_resolution;
uniform vec2    u_mousePos;
uniform int     u_mouseClicked;
uniform int     u_baseRayCount;
uniform int     u_cascadeIndex;
uniform int     u_cascadeCount;

uniform sampler2D u_canvasTexture;
uniform sampler2D u_distanceFieldTexture;
uniform sampler2D u_lastTexture;

#define PI 3.1415926f
#define TAU 2.0f * PI
#define srgb 1.0f // make 2.2 to enable correct srgb (will reveal artifacts)

float brushRadius = 0.25f / min(u_resolution.x, u_resolution.y);

float rand(vec2 co) {
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

float distSquared(vec2 a, vec2 b) {
    vec2 d = a - b;
    return dot(d, d);
}

bool outOfBounds(vec2 uv) {
    return min(uv.x, uv.y) < 0.0f || max(uv.x, uv.y) > 1.0f;
}

vec4 raymarch() {
    int  maxSteps           = 16;
    vec4 radiance           = vec4(0.0f);

    vec2  fixedUv           = (uv + 1.0f) / 2.0f;
    vec2  coord             = floor(fixedUv * u_resolution);
    float rayCount          = pow(u_baseRayCount, u_cascadeIndex + 1);
    float sqrtBase          = sqrt(float(u_baseRayCount));
    float spacing           = pow(sqrtBase, u_cascadeIndex);
    vec2  size              = floor(u_resolution / spacing);
    
    vec2  rayPos            = floor(coord / size);
    float baseIndex         = float(u_baseRayCount) * (rayPos.x + (spacing * rayPos.y));
    float angleStepSize     = TAU / float(rayCount);
    float minStepSize       = (0.5f/max(u_resolution.x, u_resolution.y));
    
    vec2  probeRelativePos  = mod(coord, size);
    vec2  probeCenter       = (probeRelativePos + 0.5f) * spacing;

    float shortestSide      = min(u_resolution.x, u_resolution.y);
    vec2  scale             = shortestSide / u_resolution;

    float intervalStart     = u_cascadeIndex == 0 ? 0.0f : pow(u_baseRayCount, u_cascadeIndex - 1.0f) / shortestSide * 5;
    float intervalLength    = pow(u_baseRayCount, u_cascadeIndex) / shortestSide * 5;

    for (int i = 0; i < u_baseRayCount; i++) {
        float index         = baseIndex + float(i);
        float angleStep     = index + 0.5f;
        float angle         = angleStepSize * angleStep;
        vec2  rayDirection  = vec2(cos(angle), -sin(angle));
        
        vec2  sampleUv      = (probeCenter / u_resolution) + rayDirection * intervalStart * scale;
        float traveled      = 0.0f;
        vec4  radDelta      = vec4(0.0f);
        bool  dontStart     = outOfBounds(sampleUv);

        for (int step = 1; step < maxSteps && !dontStart; step++) {
            float dist = texture(u_distanceFieldTexture, sampleUv).x;
            sampleUv += rayDirection * dist * scale;
            
            if (outOfBounds(sampleUv)) break;
            
            if (dist <= minStepSize) {
                vec4 sampleLight = texture(u_canvasTexture, sampleUv);
                radDelta += vec4(pow(sampleLight.rgb, vec3(srgb)), sampleLight.a);
                break;
            }

            traveled += dist;
            if (traveled >= intervalLength) break;
        }

        if ((u_cascadeIndex < (u_cascadeCount - 1)) && (radDelta.a == 0.0f)) {
            float upperSpacing  = pow(sqrtBase, u_cascadeIndex + 1.0f);
            vec2  upperSize     = floor(u_resolution / upperSpacing);
            vec2  upperPosition = vec2(mod(index, upperSpacing), floor(index / upperSpacing)) * upperSize;
            vec2  offset        = (probeRelativePos + 0.5f) / sqrtBase;
            vec2  clampedOffset = clamp(offset, vec2(0.5f), upperSize - 0.5f);
            vec2  upperUv       = (upperPosition + clampedOffset) / u_resolution;
            radDelta += texture(u_lastTexture, upperUv);
        }
        
        radiance += radDelta;
    }

    return vec4(radiance.rgb / float(u_baseRayCount), 1.0);
}

void main() {
    vec4 radiance       = vec4(0.0f);
    if (u_cascadeIndex == 0 && distSquared(u_mousePos, uv) < brushRadius) {
        vec2 fixedMousePos = (u_mousePos + 1.0f) / 2.0f;
        radiance = vec4(fixedMousePos, 1.0f, 1.0f);
    }
    else {
        radiance = raymarch();
    }
    FragColor = vec4((u_cascadeIndex > 0) ? radiance.rgb : pow(radiance.rgb, vec3(1.0 / srgb)), 1.0);
}