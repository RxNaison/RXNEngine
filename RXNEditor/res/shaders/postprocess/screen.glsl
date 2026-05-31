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

out vec4 FragColor;
in vec2 v_TexCoord;

uniform sampler2D u_ScreenTexture;
uniform float u_Exposure; // Default to 1.0
uniform float u_Gamma;    // Default to 2.2

uniform sampler2D u_BloomTexture;
uniform float u_BloomIntensity; // Default 0.04

uniform sampler2D u_OutlineTexture;
uniform vec2 u_TexelSize;

uniform sampler2D u_DepthTexture;
uniform mat4 u_InverseViewProjection;

vec3 ACESFilm(vec3 x)
{
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

float GetDitherNoise(vec2 screenPos)
{
    return fract(52.9829189 * fract(dot(screenPos, vec2(0.06711056, 0.00583715))));
}

vec3 GetChromaticAberrationColor(vec2 uv)
{
    float dist = length(uv - vec2(0.5));
    float caMask = smoothstep(0.2, 0.75, dist);
    
    if (caMask <= 0.0)
    {
        return texture(u_ScreenTexture, uv).rgb;
    }
    
    vec2 dir = normalize(uv - vec2(0.5));
    vec2 offset = dir * u_TexelSize * caMask * 1.8;
    
    float r = texture(u_ScreenTexture, uv - offset).r;
    float g = texture(u_ScreenTexture, uv).g;
    float b = texture(u_ScreenTexture, uv + offset).b;
    
    return vec3(r, g, b);
}

vec3 ComputeLensFlare(vec2 uv)
{
    vec2 centerVec = vec2(0.5) - uv;
    vec3 flare = vec3(0.0);
    
    vec2 ghost1 = uv + centerVec * 0.4;
    vec2 ghost2 = uv + centerVec * 0.8;
    vec2 ghost3 = uv + centerVec * 1.2;
    
    vec3 c1 = texture(u_BloomTexture, ghost1).rgb * 0.5;
    vec3 c2 = texture(u_BloomTexture, ghost2).rgb * 0.35;
    vec3 c3 = texture(u_BloomTexture, ghost3).rgb * 0.25;
    
    vec2 haloUV = uv + normalize(centerVec) * 0.35;
    vec3 c4 = texture(u_BloomTexture, haloUV).rgb * 0.15;
    
    flare = (c1 + c2 + c3 + c4) * 0.05;
    return flare;
}

float GetLinearDepth(vec2 uv, float depth);

uniform sampler2D u_AOTexture;

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

float BilateralUpscaleAO(vec2 uv, float centerDepth, vec3 centerNormal)
{
    vec2 texelSizeLow = u_TexelSize * 4.0;
    
    vec2 centerCoord = uv / texelSizeLow - 0.5;
    vec2 baseUV = (floor(centerCoord) + 0.5) * texelSizeLow;
    
    float totalAO = 0.0;
    float totalWeight = 0.0;
    
    float centerLinearDepth = GetLinearDepth(uv, centerDepth);
    
    vec3 N_full = centerNormal;
    if (length(N_full) < 0.01)
    {
        N_full = ReconstructNormalFromDepth(uv, centerDepth);
    }
    
    vec2 f = fract(centerCoord);
    float bilinearWeights[4] = float[](
        (1.0 - f.x) * (1.0 - f.y),
        f.x * (1.0 - f.y),
        (1.0 - f.x) * f.y,
        f.x * f.y
    );
    
    vec2 offsets[4] = vec2[](
        vec2(0.0, 0.0),
        vec2(1.0, 0.0),
        vec2(0.0, 1.0),
        vec2(1.0, 1.0)
    );
    
    for (int i = 0; i < 4; ++i)
    {
        vec2 sampleUV = baseUV + offsets[i] * texelSizeLow;
        
        float lowResDepth = texture(u_DepthTexture, sampleUV).r;
        float lowResLinearDepth = GetLinearDepth(sampleUV, lowResDepth);
        
        vec3 N_quarter = ReconstructNormalFromDepth(sampleUV, lowResDepth);
        
        float lowResAO = texture(u_AOTexture, sampleUV).r;
        
        float depthDiff = abs(lowResLinearDepth - centerLinearDepth);
        float depthWeight = exp(-depthDiff * 8.0);
        
        float normalDiff = max(dot(N_full, N_quarter), 0.0);
        float normalWeight = pow(normalDiff, 4.0);
        
        float weight = bilinearWeights[i] * depthWeight * normalWeight;
        
        totalAO += lowResAO * weight;
        totalWeight += weight;
    }
    
    if (totalWeight > 0.0001)
    {
        return totalAO / totalWeight;
    }
    
    return texture(u_AOTexture, uv).r;
}

float GetLinearDepth(vec2 uv, float depth)
{
    vec4 ndc = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 wp = u_InverseViewProjection * ndc;
    return abs(1.0 / wp.w);
}

void main()
{
    vec3 hdrColor = GetChromaticAberrationColor(v_TexCoord);
    
    vec3 bloomColor = texture(u_BloomTexture, v_TexCoord).rgb;
    vec3 flareColor = ComputeLensFlare(v_TexCoord);
    hdrColor += (bloomColor + flareColor) * u_BloomIntensity;
    
    float centerDepth = texture(u_DepthTexture, v_TexCoord).r;

    if (centerDepth < 0.9999)
    {
        float ao = BilateralUpscaleAO(v_TexCoord, centerDepth, vec3(0.0));
        hdrColor *= ao;
    }

    vec3 mapped = hdrColor * u_Exposure;

    mapped = ACESFilm(mapped);
    
    mapped = pow(mapped, vec3(1.0 / u_Gamma));

    float noise = GetDitherNoise(gl_FragCoord.xy);
    float ditherValue = (noise - 0.5) / 255.0; 
    mapped += vec3(ditherValue);

    float grainNoise = GetDitherNoise(gl_FragCoord.xy * 2.0 + vec2(ditherValue * 17.3));
    float luminance = dot(mapped, vec3(0.2126, 0.7152, 0.0722));
    float grainStrength = 0.012 * smoothstep(0.0, 0.5, luminance) * smoothstep(1.0, 0.5, luminance);
    mapped += vec3((grainNoise - 0.5) * grainStrength);

    float centerMask = texture(u_OutlineTexture, v_TexCoord).r;
    float outline = 0.0;
    
    if (centerMask < 0.5)
    {
        float a = texture(u_OutlineTexture, v_TexCoord + vec2(u_TexelSize.x, 0.0)).r;
        float b = texture(u_OutlineTexture, v_TexCoord + vec2(-u_TexelSize.x, 0.0)).r;
        float c = texture(u_OutlineTexture, v_TexCoord + vec2(0.0, u_TexelSize.y)).r;
        float d = texture(u_OutlineTexture, v_TexCoord + vec2(0.0, -u_TexelSize.y)).r;
        
        float maxNeighbor = max(max(a, b), max(c, d));
        outline = max(maxNeighbor - centerMask, 0.0);
    }

    if (outline > 0.0)
    {
        mapped = mix(mapped, vec3(1.0, 0.5, 0.0), outline);
    }

    FragColor = vec4(mapped, 1.0);
}