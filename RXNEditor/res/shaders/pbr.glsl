#type vertex
#version 450 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;

layout(location = 4) in vec4 a_ModelRow0;
layout(location = 5) in vec4 a_ModelRow1;
layout(location = 6) in vec4 a_ModelRow2;
layout(location = 7) in vec4 a_ModelRow3;
layout(location = 8) in int a_EntityID;

uniform mat4 u_ViewProjection;
uniform float u_Tiling;

out vec2 v_TexCoord;
out vec3 v_WorldPos;
out vec3 v_Normal;
flat out int v_EntityID;

void main()
{
    mat4 model = mat4(a_ModelRow0, a_ModelRow1, a_ModelRow2, a_ModelRow3);
    vec4 worldPos = model * vec4(a_Position, 1.0);
    
    v_WorldPos = worldPos.xyz;
    v_TexCoord = a_TexCoord * u_Tiling;
    v_EntityID = a_EntityID;

    mat3 m3 = mat3(model);
    float det = determinant(m3);
    mat3 normalMatrix;
    if (abs(det) > 1e-12)
    {
        normalMatrix = transpose(inverse(m3));
    }
    else
    {
        normalMatrix = m3;
    }
    v_Normal = normalMatrix * a_Normal;
    if (length(v_Normal) < 1e-6)
    {
        v_Normal = a_Normal;
    }

    gl_Position = u_ViewProjection * worldPos;
}

#type fragment
#version 450 core

layout(location = 0) out vec4 o_Color;

in vec2 v_TexCoord;
in vec3 v_WorldPos;
in vec3 v_Normal;
flat in int v_EntityID;

uniform sampler2D u_AlbedoMap;
uniform sampler2D u_NormalMap;
uniform sampler2D u_MetalnessMap;
uniform sampler2D u_RoughnessMap;
uniform sampler2D u_AOMap;
uniform sampler2D u_EmissiveMap;

uniform vec4 u_AlbedoColor;
uniform vec3 u_EmissiveColor;
uniform float u_Metalness;
uniform float u_Roughness;
uniform float u_AO;

uniform int u_UseNormalMap;

uniform vec3 u_CameraPosition;

layout(binding = 8) uniform sampler2DArrayShadow u_ShadowMap; 
layout(binding = 9) uniform sampler2DArray u_ShadowMapRaw; 
layout (std140, binding = 2) uniform ShadowData
{
    mat4 u_LightSpaceMatrices[4];
    vec4 u_CascadePlaneDistances[4];
    float u_LightSize;
    float u_ContactThreshold;
    float u_ContactSharpness;
    float u_ContactSharpeningBias;
};
uniform mat4 u_View;

layout(binding = 10) uniform samplerCube u_IrradianceMap;
layout(binding = 11) uniform samplerCube u_PrefilterMap;
layout(binding = 12) uniform sampler2D   u_BRDFLUT;

layout(binding = 13) uniform sampler2DArrayShadow u_SpotShadowMap; 
layout(binding = 14) uniform samplerCubeArray u_PointShadowMap; 

layout(binding = 16) uniform sampler2D u_LightCookies[8];

layout(binding = 15) uniform sampler2D u_PrevFrameColor;
layout(binding = 24) uniform sampler2D u_PrevDepthTexture;
uniform mat4 u_ViewProjection;
uniform mat4 u_InverseViewProjection;
uniform mat4 u_PrevViewProjection;
uniform mat4 u_PrevInverseViewProjection;

struct AmbientVolume {
    mat4 InverseTransform;
    vec3 HalfExtents;
    float Intensity;
    vec3 TransitionMin;
    vec3 TransitionMax;
};
uniform AmbientVolume u_AmbientVolumes[8];
uniform int u_AmbientVolumeCount;

float GetLinearDepth(vec2 uv, float depth)
{
    vec4 ndc = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 wp = u_InverseViewProjection * ndc;
    return abs(1.0 / wp.w);
}

float GetLinearDepthPrev(vec2 uv, float depth)
{
    vec4 ndc = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 wp = u_PrevInverseViewProjection * ndc;
    return abs(1.0 / wp.w);
}

struct PointLight {
    vec4 PositionIntensity; 
    vec4 ColorRadius;       
    vec4 Falloff;           
    vec4 ExtraData;
};

struct SpotLight {
    vec4 PositionIntensity; 
    vec4 DirectionRadius;
    vec4 ColorCutoff;
    vec4 Falloff;
    mat4 LightSpaceMatrix;
    vec4 ExtraData;
};

layout(std140, binding = 1) uniform LightData {
    vec4 u_DirLightDirection; 
    vec4 u_DirLightColor;
    uint u_PointLightCount;
    uint u_SpotLightCount;
    float u_EnvironmentIntensity;
    uint u_Padding;
    PointLight u_PointLights[100];
    SpotLight  u_SpotLights[100];
};

const float PI = 3.14159265359;

vec3 SafeNormalize(vec3 v)
{
    float len = length(v);
    return len > 1e-6 ? v / len : vec3(0.0, 1.0, 0.0);
}

vec3 DisneyDiffuse(vec3 albedo, float roughness, float NdotV, float NdotL, float LdotH)
{
    float FD90 = 0.5 + 2.0 * roughness * LdotH * LdotH;
    float lightScatter = 1.0 + (FD90 - 1.0) * pow(max(1.0 - NdotL, 0.0), 5.0);
    float viewScatter  = 1.0 + (FD90 - 1.0) * pow(max(1.0 - NdotV, 0.0), 5.0);
    return (albedo / PI) * lightScatter * viewScatter;
}

float SoftenTerminator(float NdotL)
{
    return smoothstep(0.0, 0.1, NdotL);
}

float GetToksvigAdjustedRoughness(float roughness, vec3 normal)
{
    vec3 dX = dFdx(normal);
    vec3 dY = dFdy(normal);
    float variance = 0.5 * (dot(dX, dX) + dot(dY, dY));
    variance = min(variance, 0.1); 
    return sqrt(max(roughness * roughness + variance, 0.0));
}

float GetDitherNoise(vec2 screenPos)
{
    return fract(52.9829189 * fract(dot(screenPos, vec2(0.06711056, 0.00583715))));
}

vec4 StochasticSSR(vec3 N, vec3 V, float roughness, vec3 F0)
{
    vec3 R_world = reflect(-V, N);
    vec3 rayStartPos = v_WorldPos + N * 0.05; 
    vec4 rayStartNDC = u_PrevViewProjection * vec4(rayStartPos, 1.0);
    if (rayStartNDC.w <= 0.0001) return vec4(0.0);
        
    rayStartNDC /= rayStartNDC.w;
    vec3 rayStartUV = rayStartNDC.xyz * 0.5 + 0.5;

    const int maxSteps = 40; 
    const float maxDistance = 15.0; 
    const float stepSize = maxDistance / float(maxSteps);
    float dither = GetDitherNoise(gl_FragCoord.xy);
    
    vec3 currentWorldPos = rayStartPos;
    float currentDist = 0.05 + stepSize * dither * 0.5; 
    bool hit = false;
    vec2 hitUV = vec2(0.0);
    vec3 hitWorldPos = vec3(0.0);
    
    for (int i = 0; i < maxSteps; ++i)
    {
        currentDist += stepSize;
        currentWorldPos = rayStartPos + R_world * currentDist;
        vec4 rayNDC = u_PrevViewProjection * vec4(currentWorldPos, 1.0);
        if (rayNDC.w <= 0.0001) break;
            
        rayNDC /= rayNDC.w;
        vec3 rayUV = rayNDC.xyz * 0.5 + 0.5;
        
        if (rayUV.x < 0.001 || rayUV.x > 0.999 || rayUV.y < 0.001 || rayUV.y > 0.999 || rayUV.z < 0.0 || rayUV.z > 1.0)
            break;
            
        float sampledDepth = texture(u_PrevDepthTexture, rayUV.xy).r;
        float rayDepth = GetLinearDepthPrev(rayUV.xy, rayUV.z);
        float sampledDepthLinear = GetLinearDepthPrev(rayUV.xy, sampledDepth);
        float thickness = stepSize * (1.0 + currentDist * 0.05);
        
        if (rayDepth > sampledDepthLinear && (rayDepth - sampledDepthLinear) < thickness)
        {
            hit = true;
            hitUV = rayUV.xy;
            vec3 prevWorldPos = rayStartPos + R_world * (currentDist - stepSize);
            hitWorldPos = currentWorldPos;
            
            for (int j = 0; j < 10; ++j)
            {
                vec3 midWorldPos = (prevWorldPos + hitWorldPos) * 0.5;
                vec4 midNDC = u_PrevViewProjection * vec4(midWorldPos, 1.0);
                if (midNDC.w <= 0.0001) break;
                midNDC /= midNDC.w;
                vec3 midUV = midNDC.xyz * 0.5 + 0.5;
                
                float midSampledDepth = texture(u_PrevDepthTexture, midUV.xy).r;
                float midRayDepth = GetLinearDepthPrev(midUV.xy, midUV.z);
                float midSampledDepthLinear = GetLinearDepthPrev(midUV.xy, midSampledDepth);
                
                if (midRayDepth > midSampledDepthLinear)
                {
                    hitWorldPos = midWorldPos;
                    hitUV = midUV.xy;
                }
                else
                {
                    prevWorldPos = midWorldPos;
                }
            }
            break;
        }
    }
    
    if (hit)
    {
        vec2 prevUV = hitUV;
        vec2 edge = smoothstep(vec2(0.0), vec2(0.12), prevUV) * (1.0 - smoothstep(vec2(0.88), vec2(1.0), prevUV));
        float fade = edge.x * edge.y;
        fade *= (1.0 - clamp(dot(R_world, V), 0.0, 1.0));
        float lod = roughness * 5.0;
        vec3 reflectionColor = textureLod(u_PrevFrameColor, prevUV, lod).rgb;
        return vec4(reflectionColor, fade);
    }
    return vec4(0.0);
}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return nom / max(denom, 0.0000001);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    return nom / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 GetNormalFromMap(vec3 N)
{
    vec3 tangentNormal = texture(u_NormalMap, v_TexCoord).xyz * 2.0 - 1.0;
    
    // bypass derivative artifacts
    if (abs(tangentNormal.x) < 0.015 && abs(tangentNormal.y) < 0.015)
    {
        return N;
    }

    vec3 dp1 = dFdx(v_WorldPos);
    vec3 dp2 = dFdy(v_WorldPos);
    vec2 duv1 = dFdx(v_TexCoord);
    vec2 duv2 = dFdy(v_TexCoord);
    vec3 dp2perp = cross(dp2, N);
    vec3 dp1perp = cross(N, dp1);
    vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;
    float det = max(dot(T, T), dot(B, B));
    float invmax = (det == 0.0) ? 0.0 : inversesqrt(det);
    return SafeNormalize(T * tangentNormal.x * invmax + B * tangentNormal.y * invmax + N * tangentNormal.z);
}

const vec2 poissonDisk[16] = vec2[16](
    vec2(-0.94201624, -0.39906216), vec2(0.94558609, -0.76890725),
    vec2(-0.094184101, -0.92938870), vec2(0.34495938, 0.29387760),
    vec2(-0.91588583, 0.45771432), vec2(-0.81544232, -0.87912464),
    vec2(-0.38208752, 0.27676845), vec2(0.97484398, 0.75648379),
    vec2(0.44323325, -0.97511554), vec2(0.53742981, -0.47373420),
    vec2(-0.51209949, -0.89733621), vec2(0.28989186, -0.66877443),
    vec2(0.59620762, 0.22675971), vec2(-0.25883713, 0.52834376),
    vec2(0.18730415, 0.81231885), vec2(-0.43265215, -0.31238692)
);

// Standardized Direct PBR calculation helper
vec3 ComputePBRDirect(vec3 N, vec3 V, vec3 L, vec3 albedo, float roughness, float metallic, vec3 F0, vec3 radiance)
{
    vec3 H = SafeNormalize(V + L);
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    float LdotH = max(dot(L, H), 0.0);

    if (NdotL <= 0.0 || NdotV <= 0.0)
        return vec3(0.0);

    float NDF = DistributionGGX(N, H, roughness);   
    float G   = GeometrySmith(N, V, L, roughness);      
    vec3 F    = FresnelSchlick(max(dot(H, V), 0.0), F0);
       
    vec3 numerator    = NDF * G * F; 
    float denominator = 4.0 * NdotV * NdotL + 0.0001; 
    vec3 specular = numerator / denominator;
    
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;   

    float terminator = SoftenTerminator(NdotL);
    vec3 diffuseTerm = kD * DisneyDiffuse(albedo, roughness, NdotV, NdotL, LdotH);
    return (diffuseTerm + specular) * radiance * NdotL * terminator;
}

// Generates a mathematically ideal Vogel Disk pattern on the fly
vec2 GetVogelDiskSample(int sampleIndex, int sampleCount, float phi, float jitter)
{
    float GoldenRatio = 1.6180339887498948482;
    float r = sqrt(float(sampleIndex) + 0.5 + jitter * 0.4) / sqrt(float(sampleCount));
    float theta = float(sampleIndex) * GoldenRatio * 2.0 * PI + phi;
    return vec2(cos(theta), sin(theta)) * r;
}

float SamplePCSSShadow(vec3 unbiasedProjCoords, float biasWorld, int layer, float depthRange)
{
    if (unbiasedProjCoords.z > 1.0)
        return 1.0;

    float lightSize = u_LightSize;                         // Controls how wide/blurry the shadow gets at its softest point
                                                           
    float contactThreshold = u_ContactThreshold;           // Keep shadow 100% sharp for the first N meters of distance.
                                                           // -> INCREASE this (e.g., 1.5 or 2.0) to stretch the sharp zone further.
                                                           
    float contactSharpness = u_ContactSharpness;           // Exponents > 1.0 keep the near-contact region sharp longer,
                                                           // transitioning quickly to a soft blur afterwards.
                                        
    float contactSharpeningBias = u_ContactSharpeningBias; // Blends the closest blocker (1.0) with average blocker (0.0).
                                                           // Set to 0.85 to keep contact points (feet/pillars) razor-sharp.

    vec2 texelSize = 1.0 / vec2(textureSize(u_ShadowMap, 0));
    float cascadeWorldWidth = u_CascadePlaneDistances[layer].w; 

    // Generate a world-position-based rotation/jitter angle
    float worldNoise = fract(sin(dot(v_WorldPos.xyz, vec3(12.9898, 78.233, 45.164))) * 43758.5453);
    float phi = worldNoise * 2.0 * PI;
    float jitter = fract(worldNoise * 1.618);

    // Blocker Search (12 Vogel Samples)
    float sumBlockerDepth = 0.0;
    float numBlockers = 0.0;
    float maxBlockerDepth = 0.0; 
    
    float blockerSearchRadiusWS = 1.2; 
    float blockerSearchRadiusUV = blockerSearchRadiusWS / cascadeWorldWidth;
    blockerSearchRadiusUV = clamp(blockerSearchRadiusUV, texelSize.x * 1.5, 0.15);

    float blockerBiasWS = 0.0015; 
    float blockerBiasUVz = blockerBiasWS / depthRange;
    float blockerThresholdDepth = unbiasedProjCoords.z - blockerBiasUVz;

    for (int i = 0; i < 12; ++i)
    {
        vec2 offset = GetVogelDiskSample(i, 12, phi, jitter) * blockerSearchRadiusUV;
        float sampleDepth = texture(u_ShadowMapRaw, vec3(unbiasedProjCoords.xy + offset, float(layer))).r;
        
        if (sampleDepth < blockerThresholdDepth)
        {
            sumBlockerDepth += sampleDepth;
            numBlockers += 1.0;
            maxBlockerDepth = max(maxBlockerDepth, sampleDepth);
        }
    }

    if (numBlockers == 0.0) 
    {
        return 1.0; // Fully lit
    }

    float avgBlockerDepth = sumBlockerDepth / numBlockers;
    float finalBlockerDepth = mix(avgBlockerDepth, maxBlockerDepth, contactSharpeningBias);

    // World-Space Penumbra Evaluation
    float currentDepthWS = unbiasedProjCoords.z * depthRange;
    float finalBlockerDepthWS = finalBlockerDepth * depthRange;
    
    float blockerDistanceWS = max(currentDepthWS - finalBlockerDepthWS, 0.0);

    float activeDistance = max(blockerDistanceWS - contactThreshold, 0.0);
    float penumbraSizeWS = pow(activeDistance, contactSharpness) * lightSize;

    // PCF Filtering in World-Space Meters (16 Jittered Vogel Samples)
    float minBlurWS = 0.001; 
    float maxBlurWS = min(4.5, cascadeWorldWidth * 0.18); 

    float filterRadiusWS = clamp(penumbraSizeWS, minBlurWS, maxBlurWS);
    float filterRadiusUV = filterRadiusWS / cascadeWorldWidth;

    filterRadiusUV = clamp(filterRadiusUV, texelSize.x * 1.0, 0.15);

    float biasedDepth = unbiasedProjCoords.z - (biasWorld / depthRange);

    float visibility = 0.0;
    for (int i = 0; i < 16; ++i)
    {
        vec2 offset = GetVogelDiskSample(i, 16, phi, jitter) * filterRadiusUV;
        visibility += texture(u_ShadowMap, vec4(unbiasedProjCoords.xy + offset, float(layer), biasedDepth));
    }

    return visibility / 16.0;
}

float ShadowCalculation(vec3 fragPosWorld, vec3 shadingNormal)
{
    vec4 fragPosViewSpace = u_View * vec4(fragPosWorld, 1.0);
    float depthValue = abs(fragPosViewSpace.z);
    
    if (depthValue >= 150.0)
    {
        return 0.0;
    }

    int layer = 3;
    for (int i = 0; i < 4; ++i) {
        if (depthValue < u_CascadePlaneDistances[i].x) {
            layer = i;
            break;
        }
    }

    vec3 geoNormal = SafeNormalize(v_Normal); 
    vec3 lightDir = SafeNormalize(-u_DirLightDirection.xyz);

    float cosTheta = clamp(dot(geoNormal, lightDir), 0.0, 1.0);
    float sinTheta = sqrt(max(1.0 - cosTheta * cosTheta, 0.0));
    float slopeScale = sinTheta / max(cosTheta, 0.05);

    float worldTexelSize = u_CascadePlaneDistances[layer].y;
    float depthRange = max(u_CascadePlaneDistances[layer].z, 1.0);

    float normalOffsetScale = 1.35; // Adjust this to control global normal offset strength
    vec3 normalOffset = geoNormal * (worldTexelSize * normalOffsetScale * sinTheta);
    vec3 biasedFragPosWorld = fragPosWorld + normalOffset;

    vec4 rawFragPosLightSpace = u_LightSpaceMatrices[layer] * vec4(biasedFragPosWorld, 1.0);
    vec3 unbiasedProjCoords;
    unbiasedProjCoords.xy = (rawFragPosLightSpace.xy / max(rawFragPosLightSpace.w, 0.0001)) * 0.5 + 0.5;
    unbiasedProjCoords.z  = (rawFragPosLightSpace.z  / max(rawFragPosLightSpace.w, 0.0001)) * 0.5 + 0.5;

    float baseBiasWorld = 0.001; 
    float biasWorld = baseBiasWorld + worldTexelSize * 0.15 * slopeScale;
    biasWorld = min(biasWorld, 0.015); 

    float shadowResult = SamplePCSSShadow(unbiasedProjCoords, biasWorld, layer, depthRange);

    // DUAL TRANSITION BLENDING (Depth + Border Edge Fading)
    float blendRange = 0.70; 
    float currentMax = u_CascadePlaneDistances[layer].x;
    float currentMin = (layer > 0) ? u_CascadePlaneDistances[layer - 1].x : 0.0;
    float cascadeLength = currentMax - currentMin;
    float transitionStart = currentMin + (cascadeLength * blendRange);

    float depthBlendFactor = 0.0;
    if (depthValue > transitionStart)
    {
        depthBlendFactor = (depthValue - transitionStart) / (currentMax - transitionStart);
    }

    float borderFade = min(min(unbiasedProjCoords.x, 1.0 - unbiasedProjCoords.x), 
                           min(unbiasedProjCoords.y, 1.0 - unbiasedProjCoords.y));
    float borderThreshold = 0.18; 
    float borderBlendFactor = 1.0 - smoothstep(0.0, borderThreshold, clamp(borderFade, 0.0, borderThreshold));

    if (unbiasedProjCoords.x < 0.0 || unbiasedProjCoords.x > 1.0 || 
        unbiasedProjCoords.y < 0.0 || unbiasedProjCoords.y > 1.0)
    {
        borderBlendFactor = 1.0;
    }

    float blendFactor = max(depthBlendFactor, borderBlendFactor);

    if (blendFactor > 0.0 && layer < 3)
    {
        int nextLayer = layer + 1;
        float nextWorldTexelSize = u_CascadePlaneDistances[nextLayer].y;
        float nextDepthRange = max(u_CascadePlaneDistances[nextLayer].z, 1.0);

        vec3 nextNormalOffset = geoNormal * (nextWorldTexelSize * normalOffsetScale * sinTheta);
        vec3 nextBiasedFragPosWorld = fragPosWorld + nextNormalOffset;

        vec4 nextRawFragPosLightSpace = u_LightSpaceMatrices[nextLayer] * vec4(nextBiasedFragPosWorld, 1.0);
        vec3 nextUnbiasedProjCoords;
        nextUnbiasedProjCoords.xy = (nextRawFragPosLightSpace.xy / max(nextRawFragPosLightSpace.w, 0.0001)) * 0.5 + 0.5;
        nextUnbiasedProjCoords.z  = (nextRawFragPosLightSpace.z  / max(nextRawFragPosLightSpace.w, 0.0001)) * 0.5 + 0.5;

        float nextBiasWorld = baseBiasWorld + nextWorldTexelSize * 0.15 * slopeScale;
        nextBiasWorld = min(nextBiasWorld, 0.02);

        float nextShadowResult = SamplePCSSShadow(nextUnbiasedProjCoords, nextBiasWorld, nextLayer, nextDepthRange);

        shadowResult = mix(shadowResult, nextShadowResult, clamp(blendFactor, 0.0, 1.0));
    }

    float shadowFade = clamp((150.0 - depthValue) / (150.0 * 0.15), 0.0, 1.0);
    return mix(0.0, 1.0 - shadowResult, shadowFade);
}

float SpotShadowCalculation(vec3 fragPosWorld, SpotLight light, vec3 lightDir) {
    int shadowIndex = int(light.ExtraData.x);
    if (shadowIndex < 0)
        return 1.0;

    vec3 lightPos = light.PositionIntensity.xyz;
    float outerCutOffCos = light.Falloff.y;

    vec3 normal = SafeNormalize(v_Normal);
    float cosTheta = clamp(dot(normal, lightDir), 0.0, 1.0);
    float sinTheta = sqrt(max(1.0 - cosTheta * cosTheta, 0.0));
    float slopeScale = sinTheta / max(cosTheta, 0.001);

    float distanceToLight = length(lightPos - fragPosWorld);
    float sinOuter = sqrt(max(1.0 - outerCutOffCos * outerCutOffCos, 0.0));
    float tanOuter = sinOuter / max(outerCutOffCos, 0.001);
    float texelSize = distanceToLight * tanOuter / 1024.0; 
    float normalOffset = max(texelSize, 0.0025) * 1.5 * sinTheta;

    float depthBiasWorld = 0.02 + 0.025 * slopeScale;
    depthBiasWorld = min(depthBiasWorld, 0.08); 

    vec3 shadowPos = fragPosWorld + normal * normalOffset + lightDir * depthBiasWorld;

    vec4 fragPosLightSpace = light.LightSpaceMatrix * vec4(shadowPos, 1.0);
    vec3 projCoords = (fragPosLightSpace.xyz / max(fragPosLightSpace.w, 0.0001)) * 0.5 + 0.5;
    
    if(projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0) 
        return 1.0;

    float shadow = 0.0;
    vec2 texelSizeUV = 1.0 / vec2(textureSize(u_SpotShadowMap, 0));
    for(int x = -1; x <= 1; ++x) {
        for(int y = -1; y <= 1; ++y) {
            shadow += texture(u_SpotShadowMap, vec4(projCoords.xy + vec2(x, y) * texelSizeUV, float(shadowIndex), projCoords.z));
        }
    }
    return shadow / 9.0;
}

float PointShadowCalculation(vec3 fragPosWorld, PointLight light, vec3 normal) {
    int shadowIndex = int(light.ExtraData.x);
    if (shadowIndex < 0) 
        return 1.0;
    
    vec3 lightPos = light.PositionIntensity.xyz;
    float farPlane = max(light.ColorRadius.w, 0.2);
    
    vec3 geoNormal = SafeNormalize(v_Normal);
    vec3 fragToLightUnbiased = fragPosWorld - lightPos;
    vec3 L = SafeNormalize(-fragToLightUnbiased); 
    float cosTheta = clamp(dot(geoNormal, L), 0.0, 1.0);
    float sinTheta = sqrt(max(1.0 - cosTheta * cosTheta, 0.0));
    float slopeScale = sinTheta / max(cosTheta, 0.001);
    
    float distanceToLight = length(fragToLightUnbiased);
    float texelSize = distanceToLight / 1024.0; 
    float normalOffset = max(texelSize, 0.002) * 2.0 * sinTheta;
    
    float constantDepthBias = 0.005;
    float depthOffset = constantDepthBias * (1.0 + min(slopeScale, 3.0));
    
    vec3 shadowPos = fragPosWorld + geoNormal * normalOffset + L * depthOffset;
    vec3 fragToLight = shadowPos - lightPos;
    float trueDistance = length(fragToLight);
    
    if (trueDistance < 0.001) 
        return 1.0;
    
    vec3 lightDir = SafeNormalize(fragToLight);
    vec3 absSampleDir = abs(fragToLight);
    float sampleZEye = max(absSampleDir.x, max(absSampleDir.y, absSampleDir.z));
    
    vec3 up = abs(lightDir.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = SafeNormalize(cross(up, lightDir));
    vec3 bitangent = cross(lightDir, tangent);
    vec3 normalDirection = SafeNormalize(fragToLight);
    float stableValue = fract(sin(dot(normalDirection, vec3(12.9898, 78.233, 45.164))) * 43758.5453);
    float randomAngle = stableValue * 2.0 * PI;
    float c = cos(randomAngle), s = sin(randomAngle);

    float shadow = 0.0;
    float diskRadius = 0.005 + (0.002 * (trueDistance / farPlane));

    for(int i = 0; i < 8; ++i) {
        vec2 srcOffset = poissonDisk[i];
        vec3 sphereOffset = (tangent * (srcOffset.x * c - srcOffset.y * s) + bitangent * (srcOffset.x * s + srcOffset.y * c)) * diskRadius * trueDistance;
        vec3 sampleDir = fragToLight + sphereOffset;
        vec3 absDir = abs(sampleDir);
        float sZEye = max(max(absDir.x, max(absDir.y, absDir.z)), 0.001);
        
        float sampledDepth = texture(u_PointShadowMap, vec4(sampleDir, float(shadowIndex))).r;
        float sampledLinearDistance = (0.2 * farPlane) / ((farPlane + 0.1) - (2.0 * sampledDepth - 1.0) * (farPlane - 0.1));
        
        float linearBias = 0.004 + 0.008 * slopeScale;
        linearBias = min(linearBias, 0.02); 
        
        if (sZEye - linearBias > sampledLinearDistance)
            shadow += 1.0;
    }
    return 1.0 - (shadow / 8.0);
}

void main()
{
    vec2 texCoords = v_TexCoord;
    vec3 V = SafeNormalize(u_CameraPosition - v_WorldPos);
    vec4 albedoSample = texture(u_AlbedoMap, texCoords);
    vec3 albedo = pow(max(albedoSample.rgb, 0.0), vec3(2.2)) * u_AlbedoColor.rgb; 
    float alpha = albedoSample.a * u_AlbedoColor.a; 

    float metallic  = clamp(texture(u_MetalnessMap, texCoords).b * u_Metalness, 0.0, 1.0); 
    float roughness = clamp(texture(u_RoughnessMap, texCoords).g * u_Roughness, 0.001, 1.0);
    float ao        = clamp(texture(u_AOMap, texCoords).r * u_AO, 0.0, 1.0);
    vec3 emissive   = texture(u_EmissiveMap, texCoords).rgb * u_EmissiveColor;

    vec3 N = SafeNormalize(v_Normal);
    if (u_UseNormalMap > 0)
    {
        N = GetNormalFromMap(N);
    }
 
    vec3 R = reflect(-V, N); 
    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo, metallic);

    float roughnessSpec = GetToksvigAdjustedRoughness(roughness, N);

    vec3 Lo = vec3(0.0);

    // DIRECTIONAL LIGHT
    {
        vec3 L = SafeNormalize(-u_DirLightDirection.xyz);
        float NdotL = max(dot(N, L), 0.0);
        
        if (NdotL > 0.0)
        {
            float shadow = ShadowCalculation(v_WorldPos, N);
            vec3 radiance = u_DirLightColor.rgb * u_DirLightDirection.w * (1.0 - shadow);
            Lo += ComputePBRDirect(N, V, L, albedo, roughnessSpec, metallic, F0, radiance);
        }
    }

    // POINT LIGHTS
    for(int i = 0; i < int(u_PointLightCount); ++i)
    {
        vec3 lightPos = u_PointLights[i].PositionIntensity.xyz;
        float distance = length(lightPos - v_WorldPos);
        float radius = u_PointLights[i].ColorRadius.w;

        if (distance < radius)
        {
            vec3 L = SafeNormalize(lightPos - v_WorldPos);
            float NdotL = max(dot(N, L), 0.0);

            if (NdotL > 0.0)
            {
                float intensity = u_PointLights[i].PositionIntensity.w;
                float falloffExp = max(u_PointLights[i].Falloff.x, 0.001);
                float window = pow(clamp(1.0 - (distance / radius), 0.0, 1.0), falloffExp);
                float attenuation = window / (distance * distance + 0.001);

                if (attenuation > 0.0001)
                {
                    float pShadow = PointShadowCalculation(v_WorldPos, u_PointLights[i], N);
                    if (pShadow > 0.0)
                    {
                        vec3 radiance = u_PointLights[i].ColorRadius.rgb * intensity * attenuation * pShadow;
                        Lo += ComputePBRDirect(N, V, L, albedo, roughnessSpec, metallic, F0, radiance);
                    }
                }
            }
        }
    }

    // SPOT LIGHTS
    for(int i = 0; i < int(u_SpotLightCount); ++i)
    {
        vec3 lightPos = u_SpotLights[i].PositionIntensity.xyz;
        float distance = length(lightPos - v_WorldPos);
        float radius = u_SpotLights[i].DirectionRadius.w;

        if (distance < radius)
        {
            vec3 L = SafeNormalize(lightPos - v_WorldPos);
            float NdotL = max(dot(N, L), 0.0);

            if (NdotL > 0.0)
            {
                vec3 lightDir = SafeNormalize(u_SpotLights[i].DirectionRadius.xyz);
                float theta = dot(L, -lightDir); 
                float innerCutOff = u_SpotLights[i].ColorCutoff.w;
                float outerCutOff = u_SpotLights[i].Falloff.y;
                float epsilon = max(innerCutOff - outerCutOff, 0.0001);
                float intensityMultiplier = clamp((theta - outerCutOff) / epsilon, 0.0, 1.0);

                if (intensityMultiplier > 0.0)
                {
                    float intensity = u_SpotLights[i].PositionIntensity.w;
                    float falloffExp = max(u_SpotLights[i].Falloff.x, 0.001);
                    float window = pow(clamp(1.0 - (distance / radius), 0.0, 1.0), falloffExp);
                    float attenuation = window / (distance * distance + 0.001);

                    if (attenuation > 0.0001)
                    {
                        float sShadow = SpotShadowCalculation(v_WorldPos, u_SpotLights[i], L);
                        if (sShadow > 0.0)
                        {
                            vec3 cookieColor = vec3(1.0);
                            int cookieIndex = int(u_SpotLights[i].Falloff.z);
                            
                            if (cookieIndex >= 16 && cookieIndex < 24)
                            {
                                int arrayIndex = cookieIndex - 16;
                                vec4 lightSpacePos = u_SpotLights[i].LightSpaceMatrix * vec4(v_WorldPos, 1.0);
                                vec3 projCoords = lightSpacePos.xyz / max(lightSpacePos.w, 0.0001);
                                projCoords = projCoords * 0.5 + 0.5;
                                
                                float cookieSize = max(u_SpotLights[i].Falloff.w, 0.0001);
                                vec2 cookieUV = (projCoords.xy - 0.5) / cookieSize + 0.5;

                                if (lightSpacePos.w > 0.0 && 
                                    cookieUV.x >= 0.0 && cookieUV.x <= 1.0 && 
                                    cookieUV.y >= 0.0 && cookieUV.y <= 1.0) 
                                {
                                    cookieColor = texture(u_LightCookies[arrayIndex], cookieUV).rgb;
                                } 
                                else 
                                {
                                    cookieColor = vec3(0.0);
                                }
                            }

                            vec3 radiance = u_SpotLights[i].ColorCutoff.rgb * intensity * attenuation * intensityMultiplier * cookieColor * sShadow;
                            Lo += ComputePBRDirect(N, V, L, albedo, roughnessSpec, metallic, F0, radiance);
                        }
                    }
                }
            }
        }
    }

    // INDIRECT LIGHTING (IBL)
    float NdotV = clamp(dot(N, V), 0.0, 1.0);
    vec3 F_IBL = FresnelSchlickRoughness(NdotV, F0, roughness);
    vec3 kS_IBL = F_IBL;
    vec3 kD_IBL = 1.0 - kS_IBL;
    kD_IBL *= 1.0 - metallic;
    
    vec3 irradiance = texture(u_IrradianceMap, N).rgb;
    float specularOcclusion = clamp(pow(NdotV + ao, 2.0) - 1.0 + ao, 0.0, 1.0);

    float skyOcclusion = 0.0;
    for (int i = 0; i < u_AmbientVolumeCount; ++i)
    {
        vec4 localPos = u_AmbientVolumes[i].InverseTransform * vec4(v_WorldPos, 1.0);
        vec3 extents = u_AmbientVolumes[i].HalfExtents;
        vec3 tMin = max(u_AmbientVolumes[i].TransitionMin, vec3(0.001));
        vec3 tMax = max(u_AmbientVolumes[i].TransitionMax, vec3(0.001));
        
        float distLeft   = extents.x + localPos.x;
        float distRight  = extents.x - localPos.x;
        float distBottom = extents.y + localPos.y;
        float distTop    = extents.y - localPos.y;
        float distBack   = extents.z + localPos.z;
        float distFront  = extents.z - localPos.z;
        
        if (distLeft >= 0.0 && distRight >= 0.0 && 
            distBottom >= 0.0 && distTop >= 0.0 && 
            distBack >= 0.0 && distFront >= 0.0)
        {
            float factorLeft   = clamp(distLeft / tMin.x, 0.0, 1.0);
            float factorRight  = clamp(distRight / tMax.x, 0.0, 1.0);
            float factorBottom = clamp(distBottom / tMin.y, 0.0, 1.0);
            float factorTop    = clamp(distTop / tMax.y, 0.0, 1.0);
            float factorBack   = clamp(distBack / tMin.z, 0.0, 1.0);
            float factorFront  = clamp(distFront / tMax.z, 0.0, 1.0);
            
            float boxOcclusion = min(factorLeft, min(factorRight, 
                                 min(factorBottom, min(factorTop, 
                                 min(factorBack, factorFront)))));
                                 
            float smoothOcclusion = smoothstep(0.0, 1.0, boxOcclusion) * u_AmbientVolumes[i].Intensity;
            skyOcclusion = max(skyOcclusion, smoothOcclusion);
        }
    }
    float skyVisibility = 1.0 - skyOcclusion;

    vec3 diffuseIBL = irradiance * albedo * ao * skyVisibility;
    const float MAX_REFLECTION_LOD = 4.0;
    vec3 prefilteredColor = textureLod(u_PrefilterMap, R, roughness * MAX_REFLECTION_LOD).rgb;
    vec2 brdf  = texture(u_BRDFLUT, vec2(NdotV, roughness)).rg;
    
    vec3 reflectionColor = prefilteredColor * skyVisibility;
    if (roughness < 0.4)
    {
        vec4 ssrResult = StochasticSSR(N, V, roughness, F0);
        if (ssrResult.a > 0.001)
        {
            float ssrRoughnessFade = smoothstep(0.4, 0.15, roughness);
            reflectionColor = mix(prefilteredColor * skyVisibility, ssrResult.rgb, ssrResult.a * ssrRoughnessFade);
        }
    }
    vec3 specularIBL = reflectionColor * (F_IBL * brdf.x + brdf.y) * specularOcclusion;

    vec3 ambient = (kD_IBL * diffuseIBL + specularIBL) * u_EnvironmentIntensity;
    vec3 color = ambient + Lo + emissive;

    if (!(color.r >= 0.0 && color.r <= 65500.0)) color.r = 0.0;
    if (!(color.g >= 0.0 && color.g <= 65500.0)) color.g = 0.0;
    if (!(color.b >= 0.0 && color.b <= 65500.0)) color.b = 0.0;
    if (!(alpha >= 0.0 && alpha <= 1.0)) alpha = 1.0;

    o_Color = vec4(color, alpha);
}