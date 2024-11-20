#version 430 core

in vec2 uv;
in vec4 color;

out vec4 FragColor;

uniform ivec2 u_resolution;
uniform vec2 u_mousePos;
uniform int u_mouseClicked;
uniform int u_baseRayCount;
uniform int u_rayCount;
uniform sampler2D u_canvasTexture;
uniform sampler2D u_distanceFieldTexture;
uniform sampler2D u_lastTexture;

#define TAU 6.2831853f
#define EPS (0.5/max(u_resolution.x, u_resolution.y)) 

float brushRadius = 0.5f / min(u_resolution.x, u_resolution.y);

float rand(vec2 co) {
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

float distSquared(vec2 a, vec2 b) {
    vec2 d = a - b;
    return dot(d, d);
}

bool outOfBounds(vec2 uv) {
    return uv.x < 0.0f || uv.x > 1.1f || uv.y < 0.0f || uv.y > 1.1f;
}

vec4 raymarch(int rayCount, float intervalStart, float intervalEnd, vec2 effectiveUv, bool isLastLayer) {
    int maxSteps = 64;
    vec4 radiance = vec4(0.0f);
    float oneOverRayCount = 1.0f / float(rayCount);
    float tauOverRayCount = TAU * oneOverRayCount;

    for (int i = 0; i < rayCount; i++) {
        float angle = tauOverRayCount * (float(i) + 0.5f);
        vec2 rayDirection = vec2(cos(angle), -sin(angle));
        
        vec2 sampleUv = effectiveUv + rayDirection * intervalStart;
        float traveled = intervalStart;

        for (int step = 1; step < maxSteps; step++) {
            float dist = texture(u_distanceFieldTexture, sampleUv).r;
            sampleUv += rayDirection * dist;
            
            if (outOfBounds(sampleUv)) break;

            
            if (dist < EPS) {
                vec4 sampleLight = texture(u_canvasTexture, sampleUv);
                radiance += vec4(pow(sampleLight.rgb, vec3(2.2f)), 1.0f);
                break;
            }

            traveled += dist;
            if (traveled >= intervalEnd) break;
        }
    }

    if (isLastLayer) {
        radiance += texture(u_lastTexture, effectiveUv);
    }

    return radiance * oneOverRayCount;
}

void main() {
    vec2 fixedUv = (uv + 1.0f) / 2.0f;

    vec2 coord = fixedUv * vec2(u_resolution);
    bool isLastLayer = u_rayCount == u_baseRayCount;
    vec2 effectiveUv = isLastLayer ? fixedUv : floor(coord / 2.0f) * 2.0f / vec2(u_resolution);

    float partial = 0.125f;
    float intervalStart = isLastLayer ? 0.0f : partial;
    float intervalEnd = isLastLayer ? partial : sqrt(2.0f);

    vec4 radiance = vec4(0.0f);

    vec4 canvasColor = texture(u_canvasTexture, fixedUv);
    if (canvasColor.a > 0.1f) radiance = vec4(pow(canvasColor.rgb, vec3(2.2f)), 1.0f);
    
    else radiance = raymarch(u_rayCount, intervalStart, intervalEnd, effectiveUv, isLastLayer);

    vec3 correctSRGB = pow(radiance.rgb, vec3(1.0f / 2.2f));
    FragColor = vec4(correctSRGB, 1.0f);
}
