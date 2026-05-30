#type vertex
#version 450 core

layout(location = 0) in vec2 a_Position; 
layout(location = 1) in vec2 a_TexCoord;

out vec2 v_TexCoord;

void main()
{
    v_TexCoord = a_TexCoord;
    gl_Position = vec4(a_Position.x, a_Position.y, 0.0, 1.0); 
}

#type fragment
#version 450 core

layout(location = 0) out float o_AO;

in vec2 v_TexCoord;

uniform sampler2D u_DepthTexture;
uniform mat4 u_InverseViewProjection;
uniform vec2 u_TexelSize;

float GetDitherNoise(vec2 screenPos)
{
    return fract(52.9829189 * fract(dot(screenPos, vec2(0.06711056, 0.00583715))));
}

float GetLinearDepth(vec2 uv, float depth)
{
    vec4 ndc = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 wp = u_InverseViewProjection * ndc;
    return abs(1.0 / wp.w);
}

vec3 ReconstructWorldPos(vec2 uv, float depth)
{
    vec2 clampedUV = clamp(uv, vec2(0.0001), vec2(0.9999));
    vec4 ndc = vec4(clampedUV * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 wp = u_InverseViewProjection * ndc;
    return wp.xyz / wp.w;
}

vec3 ReconstructNormalFromDepth(vec2 uv, float depth)
{
    vec3 pos = ReconstructWorldPos(uv, depth);
    vec3 posRight = ReconstructWorldPos(uv + vec2(u_TexelSize.x, 0.0), texture(u_DepthTexture, uv + vec2(u_TexelSize.x, 0.0)).r);
    vec3 posTop   = ReconstructWorldPos(uv + vec2(0.0, u_TexelSize.y), texture(u_DepthTexture, uv + vec2(0.0, u_TexelSize.y)).r);
    
    vec3 tangent = posRight - pos;
    vec3 bitangent = posTop - pos;
    
    vec3 normal = cross(tangent, bitangent);
    float len = length(normal);
    return len > 0.0001 ? normal / len : vec3(0.0, 1.0, 0.0);
}

void main()
{
    float centerDepth = texture(u_DepthTexture, v_TexCoord).r;
    
    if (centerDepth >= 0.9999)
    {
        o_AO = 1.0;
        return;
    }

    float centerLinearDepth = GetLinearDepth(v_TexCoord, centerDepth);
    vec3 P = ReconstructWorldPos(v_TexCoord, centerDepth);
    vec3 N = ReconstructNormalFromDepth(v_TexCoord, centerDepth);

    const float PI = 3.14159265359;
    const float R = 0.8; // meters
    const float tangentBias = 0.15;
    const float intensity = 1.6;

    float screenRadius = R / (centerLinearDepth * u_TexelSize.y);
    screenRadius = clamp(screenRadius, 4.0, 64.0);

    float noise = GetDitherNoise(gl_FragCoord.xy);
    float totalOcclusion = 0.0;

    const int DIRS = 4;
    const int STEPS = 3;

    for (int d = 0; d < DIRS; ++d)
    {
        float angle = noise * 2.0 * PI + float(d) * (2.0 * PI / float(DIRS));
        vec2 dir = vec2(cos(angle), sin(angle));

        float maxOcclusion = 0.0;

        for (int s = 1; s <= STEPS; ++s)
        {
            float t = float(s) / float(STEPS);
            vec2 sampleUV = v_TexCoord + dir * (t * screenRadius * u_TexelSize);

            if (sampleUV.x < 0.0 || sampleUV.x > 1.0 || sampleUV.y < 0.0 || sampleUV.y > 1.0)
                continue;

            float sampleDepth = texture(u_DepthTexture, sampleUV).r;
            if (sampleDepth >= 0.9999)
                continue;

            vec3 Q = ReconstructWorldPos(sampleUV, sampleDepth);
            vec3 V = Q - P;
            float dist = length(V);

            if (dist < R && dist > 0.001)
            {
                float dotVal = dot(V / dist, N);
                float falloff = 1.0 - (dist * dist) / (R * R);
                float sampleOcclusion = max(0.0, dotVal - tangentBias) * falloff;
                
                maxOcclusion = max(maxOcclusion, sampleOcclusion);
            }
        }

        totalOcclusion += maxOcclusion;
    }

    float ao = 1.0 - (totalOcclusion / float(DIRS)) * intensity;
    o_AO = clamp(ao, 0.0, 1.0);
}
