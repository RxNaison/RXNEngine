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
layout(location = 1) out vec4 o_SSR; // R2 Step 3: isolated SSR target (unused until 3b)
layout(location = 2) out vec4 o_Normal; // GTAO: world-space shading normal (encoded *0.5+0.5)

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
uniform int u_NormalMapIsBC5; // WP17: BC5/RGTC2 stores only X/Y; reconstruct Z
uniform int u_IsTransparent; // 1 while drawing the transparent pass: suppress normal/SSR G-buffer writes so GTAO keeps the opaque surface behind

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
    float u_SoftShadows;
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
layout(binding = 25) uniform sampler2D u_BlueNoiseTex; // static screen-space blue noise (RG)
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

struct ReflectionProbe {
    mat4 Transform;
    mat4 InverseTransform;
    vec3 HalfExtents;
    float BlendDistance;
    float Intensity;
};
uniform ReflectionProbe u_ReflectionProbes[8];
uniform int u_ReflectionProbeCount;
layout(binding = 26) uniform samplerCube u_ReflectionProbeCubes[4];

// R2 Step 2: Hi-Z hierarchical min-depth pyramid of the previous frame's depth.
// Level 0 holds linearized prev depth (abs(clip.w)); coarser levels hold the min
// over 2x2 children so the SSR tracer can skip large empty regions in one step.
layout(binding = 30) uniform sampler2D u_HiZBuffer;

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
};

// ===================== Clustered Forward+ =============================
// Lights now live in SSBOs and are culled per-froxel by the light_culling
// compute pass. These constants MUST match LightCuller.h / light_culling.glsl.
const uint MAX_POINT_PER_CLUSTER = 64u;
const uint MAX_SPOT_PER_CLUSTER  = 32u;

layout(std430, binding = 1) readonly buffer PointLightBuffer { PointLight b_PointLights[]; };
layout(std430, binding = 2) readonly buffer SpotLightBuffer  { SpotLight  b_SpotLights[]; };
layout(std430, binding = 3) readonly buffer PointGridBuffer  { uvec2 b_PointGrid[]; };
layout(std430, binding = 4) readonly buffer SpotGridBuffer   { uvec2 b_SpotGrid[]; };
layout(std430, binding = 5) readonly buffer PointIndexBuffer { uint  b_PointIndices[]; };
layout(std430, binding = 6) readonly buffer SpotIndexBuffer  { uint  b_SpotIndices[]; };

layout(std140, binding = 4) uniform ClusterData {
    mat4  u_InvProjection;
    mat4  u_ClusterView;   // world -> view
    vec4  u_GridSizeTileX;  // x=gridX y=gridY z=gridZ w=tilePxX
    vec4  u_ScreenNearFar;  // x=screenW y=screenH z=zNear w=zFar
    vec4  u_ScaleBiasTileY; // x=scale y=bias z=tilePxY w=numClusters
    uvec4 u_LightCounts;    // x=pointCount y=spotCount
};

uniform int u_ClusteredEnabled;
uniform float u_PointShadowResolution; // actual point-shadow cube face resolution (texels per side)

// Maps the current fragment to its froxel index in the cluster grid.
uint ComputeClusterIndex()
{
    uint gridX = uint(u_GridSizeTileX.x);
    uint gridY = uint(u_GridSizeTileX.y);
    uint gridZ = uint(u_GridSizeTileX.z);

    float zNear = u_ScreenNearFar.z;
    float viewZ = (u_ClusterView * vec4(v_WorldPos, 1.0)).z;
    float depthVS = max(-viewZ, zNear);

    float scale = u_ScaleBiasTileY.x;
    float bias  = u_ScaleBiasTileY.y;
    uint zSlice = uint(clamp(floor(log(depthVS) * scale + bias), 0.0, float(gridZ - 1u)));

    uint tileX = min(uint(gl_FragCoord.x / u_GridSizeTileX.w),  gridX - 1u);
    uint tileY = min(uint(gl_FragCoord.y / u_ScaleBiasTileY.z), gridY - 1u);

    return tileX + tileY * gridX + zSlice * gridX * gridY;
}

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

#define SSR_MAX_RAYS 1

// GGX importance sampling: build a microfacet half-vector around N for a given
// roughness. Xi is a 2D low-discrepancy / blue-noise sample in the 0..1 range.
vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness)
{
    float a = roughness * roughness;
    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    float sinTheta = sqrt(max(1.0 - cosTheta * cosTheta, 0.0));

    vec3 H = vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);

    vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);
    return normalize(tangent * H.x + bitangent * H.y + N * H.z);
}

vec2 SampleBlueNoise(vec2 fragCoord);

vec4 TraceSSRRay(vec3 rayOrigin, vec3 rayDir, vec3 V, float roughness, float startJitter)
{
    const int   maxSteps    = 28;
    const float maxDistance = 18.0;
    const float baseStep    = maxDistance / float(maxSteps);

    int maxLevel    = max(textureQueryLevels(u_HiZBuffer) - 1, 0);
    int coarseLevel = clamp(maxLevel - 1, 0, maxLevel);

    float dWdt  = (u_PrevViewProjection * vec4(rayDir, 0.0)).w;
    float absDW = abs(dWdt);
    float invDW = (absDW > 1e-5) ? (1.0 / absDW) : 0.0;

    float currentDist  = baseStep * (0.5 + 0.5 * startJitter);
    vec3  prevWorldPos = rayOrigin;

    for (int i = 0; i < maxSteps; ++i)
    {
        vec3 pos  = rayOrigin + rayDir * currentDist;
        vec4 clip = u_PrevViewProjection * vec4(pos, 1.0);
        if (clip.w <= 0.0001) break;                         // behind the prev camera

        vec2 uv = (clip.xy / clip.w) * 0.5 + 0.5;
        if (uv.x < 0.001 || uv.x > 0.999 || uv.y < 0.001 || uv.y > 0.999)
            break;                                            // left the screen

        float rayZ    = abs(clip.w);                          // ray depth metric
        float cellMin = textureLod(u_HiZBuffer, uv, float(coarseLevel)).r;
        float gap     = cellMin - rayZ;                       // >0 => in front of all geometry in cell

        if (gap > 0.02)
        {
            // Empty-space skip: jump roughly to the cell's nearest surface depth.
            float skip = clamp(gap * invDW, baseStep, maxDistance * 0.5);
            prevWorldPos = pos;
            currentDist += skip;
            if (currentDist > maxDistance) break;
            continue;
        }

        // Potential intersection: test against the precise (level 0) depth.
        float surfZ = textureLod(u_HiZBuffer, uv, 0.0).r;
        float delta = rayZ - surfZ;                           // >0 => ray passed behind surface

        if (delta > 0.0)
        {
            // Thickness-aware acceptance: reject crossings of thin foreground
            // occluders (convert a world-space slab into depth-metric units).
            float worldStep  = max(currentDist - length(prevWorldPos - rayOrigin), baseStep);
            float thickness  = clamp(worldStep * (1.0 + currentDist * 0.08), 0.05, 2.0);
            float thicknessZ = thickness * absDW + 0.05;

            if (delta < thicknessZ)
            {
                // Binary refinement of the crossing between prevWorldPos and pos.
                vec3 lo = prevWorldPos;
                vec3 hi = pos;
                vec2 hitUV = uv;
                for (int j = 0; j < 6; ++j) // perf: 8 -> 6 binary-refinement steps (sub-pixel crossing accuracy already saturates)
                {
                    vec3 mid = (lo + hi) * 0.5;
                    vec4 mc  = u_PrevViewProjection * vec4(mid, 1.0);
                    if (mc.w <= 0.0001) break;
                    vec2 muv    = (mc.xy / mc.w) * 0.5 + 0.5;
                    float mRayZ = abs(mc.w);
                    float mSurf = textureLod(u_HiZBuffer, muv, 0.0).r;
                    hitUV = muv;
                    if (mRayZ - mSurf > 0.0) hi = mid; else lo = mid;
                }

                // Confidence: fade at screen borders, when the ray points back
                // toward the camera, and with ray length.
                vec2 edge = smoothstep(vec2(0.0), vec2(0.12), hitUV) * (1.0 - smoothstep(vec2(0.88), vec2(1.0), hitUV));
                float confidence = edge.x * edge.y;
                confidence *= (1.0 - clamp(dot(rayDir, V), 0.0, 1.0));
                confidence *= 1.0 - clamp(currentDist / maxDistance, 0.0, 1.0) * 0.5;

                float lod = roughness * 4.0;
                vec3 reflectionColor = textureLod(u_PrevFrameColor, hitUV, lod).rgb;
                return vec4(reflectionColor, clamp(confidence, 0.0, 1.0));
            }
            // Thin occluder: keep marching past it.
        }

        prevWorldPos = pos;
        currentDist += baseStep;
        if (currentDist > maxDistance) break;
    }

    return vec4(0.0);
}

// Stochastic screen-space reflections (temporal-free). GGX-importance-sampled,
// blue-noise jittered rays; a few rays for glossy surfaces are averaged inline
// (the separable bilateral denoise pass in R2 Step 3 lets this drop to 1 ray).
vec4 StochasticSSR(vec3 N, vec3 V, float roughness, vec3 F0)
{
    vec3 rayOrigin = v_WorldPos + N * 0.05;
    vec4 originNDC = u_PrevViewProjection * vec4(rayOrigin, 1.0);
    if (originNDC.w <= 0.0001) return vec4(0.0);

    vec2 bn = SampleBlueNoise(gl_FragCoord.xy);
    int numRays = (roughness < 0.08) ? 1 : SSR_MAX_RAYS;

    vec3  accumColor = vec3(0.0);
    float accumConf  = 0.0;

    for (int r = 0; r < SSR_MAX_RAYS; ++r)
    {
        if (r >= numRays) break;

        // Decorrelate the blue-noise sample per ray (golden-ratio rotation).
        vec2 Xi = fract(bn + float(r) * vec2(0.61803399, 0.32471796));

        // Mirror-ish surfaces use the exact reflection; glossy ones perturb the
        // half-vector via GGX importance sampling scaled by roughness.
        vec3 H = (roughness < 0.02) ? N : ImportanceSampleGGX(Xi, N, roughness);
        vec3 rayDir = reflect(-V, H);

        // Reject rays that fall below the surface horizon.
        if (dot(rayDir, N) <= 0.0)
            continue;

        vec4 rayResult = TraceSSRRay(rayOrigin, rayDir, V, roughness, Xi.y);
        accumColor += rayResult.rgb * rayResult.a;
        accumConf  += rayResult.a;
    }

    if (accumConf <= 0.0001)
        return vec4(0.0);

    vec3 color = accumColor / accumConf;            // confidence-weighted color
    float confidence = accumConf / float(numRays);  // misses lower confidence
    return vec4(color, clamp(confidence, 0.0, 1.0));
}

// R1: Local parallax-corrected reflection probes.
// Step 2 samples each probe's own captured + prefiltered specular cubemap,
// parallax-corrected through the probe's box volume. Blended by influence weight.
vec3 SampleReflectionProbes(vec3 R, vec3 worldPos, float roughness, vec3 fallback, out float weightOut)
{
    const float MAX_REFLECTION_LOD = 4.0;
    vec3 accumColor = vec3(0.0);
    float accumWeight = 0.0;

    for (int i = 0; i < u_ReflectionProbeCount && i < 4; ++i)
    {
        vec3 ext = u_ReflectionProbes[i].HalfExtents;
        vec3 localPos = (u_ReflectionProbes[i].InverseTransform * vec4(worldPos, 1.0)).xyz;
        vec3 inside = ext - abs(localPos);
        if (inside.x < 0.0 || inside.y < 0.0 || inside.z < 0.0)
            continue;

        vec3 localR = normalize(mat3(u_ReflectionProbes[i].InverseTransform) * R);
        vec3 firstPlane  = (-ext - localPos) / localR;
        vec3 secondPlane = ( ext - localPos) / localR;
        vec3 furthest = max(firstPlane, secondPlane);
        float dist = min(min(furthest.x, furthest.y), furthest.z);
        vec3 localHit = localPos + localR * dist;

        vec3 worldDir = normalize(mat3(u_ReflectionProbes[i].Transform) * localHit);
        vec3 probeColor = textureLod(u_ReflectionProbeCubes[i], worldDir, roughness * MAX_REFLECTION_LOD).rgb;

        float edge = min(min(inside.x, inside.y), inside.z);
        float w = clamp(edge / max(u_ReflectionProbes[i].BlendDistance, 0.001), 0.0, 1.0) * u_ReflectionProbes[i].Intensity;

        accumColor += probeColor * w;
        accumWeight += w;
    }

    weightOut = clamp(accumWeight, 0.0, 1.0);
    if (accumWeight > 0.0)
        accumColor /= accumWeight;
    return mix(fallback, accumColor, weightOut);
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

    // WP17: BC5 normal maps carry only X/Y (sampled B is 0 -> Z would be -1).
    // Rebuild Z on the unit hemisphere.
    if (u_NormalMapIsBC5 == 1)
        tangentNormal.z = sqrt(clamp(1.0 - dot(tangentNormal.xy, tangentNormal.xy), 0.0, 1.0));
    
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

// Static screen-space blue noise (RG channels). texelFetch => nearest, no
// filtering or mips, with manual toroidal wrap so it tiles seamlessly. Static
// (not animated per frame) on purpose: this engine ships without TAA, so a
// fixed screen-space pattern stays stable instead of crawling frame to frame.
vec2 SampleBlueNoise(vec2 fragCoord)
{
    ivec2 sz = textureSize(u_BlueNoiseTex, 0);
    ivec2 uv = ivec2(fragCoord) % sz;
    return texelFetch(u_BlueNoiseTex, uv, 0).rg;
}

// Penumbra-adaptive PCF bounds (inline, MSAA-correct directional shadows).
#define PCSS_MIN_PCF_SAMPLES 12   // razor-sharp contacts: cheap
#define PCSS_MAX_PCF_SAMPLES 32   // widest penumbra: smoothest
#define PCSS_GAUSSIAN_FALLOFF 2.2 // higher = tighter center weighting
#define PCSS_BLOCKER_SAMPLES  24  // blocker search taps: more taps = stabler penumbra estimate (less grain on small casters)

float SamplePCSSShadow(vec3 unbiasedProjCoords, float biasWorld, int layer, float depthRange)
{
    if (unbiasedProjCoords.z > 1.0)
        return 1.0;

    vec2 texelSize = 1.0 / vec2(textureSize(u_ShadowMap, 0));
    float cascadeWorldWidth = u_CascadePlaneDistances[layer].w; 

    // Screen-space blue-noise rotation + jitter (decorrelated R/G channels)
    vec2 bn = SampleBlueNoise(gl_FragCoord.xy);
    float phi = bn.r * 2.0 * PI;
    float jitter = bn.g;

    float biasedDepth = unbiasedProjCoords.z - (biasWorld / depthRange);

    if (u_SoftShadows < 0.5)
    {
        // Low-cost 4-sample PCF filtering for high performance
        float filterRadiusUV = texelSize.x * 1.5;
        float visibility = 0.0;
        for (int i = 0; i < 4; ++i)
        {
            vec2 offset = GetVogelDiskSample(i, 4, phi, jitter) * filterRadiusUV;
            visibility += texture(u_ShadowMap, vec4(unbiasedProjCoords.xy + offset, float(layer), biasedDepth));
        }
        return visibility / 4.0;
    }

    float lightSize = u_LightSize;                         // Controls how wide/blurry the shadow gets at its softest point
                                                           
    float contactThreshold = u_ContactThreshold;           // Keep shadow 100% sharp for the first N meters of distance.
                                                           // -> INCREASE this (e.g., 1.5 or 2.0) to stretch the sharp zone further.
                                                           
    float contactSharpness = u_ContactSharpness;           // Exponents > 1.0 keep the near-contact region sharp longer,
                                                           // transitioning quickly to a soft blur afterwards.
                                        
    float contactSharpeningBias = u_ContactSharpeningBias; // Blends the closest blocker (1.0) with average blocker (0.0).
                                                           // Set to 0.85 to keep contact points (feet/pillars) razor-sharp.

    // Blocker Search (light-size-scaled radius, dense Vogel samples)
    float sumBlockerDepth = 0.0;
    float numBlockers = 0.0;
    float maxBlockerDepth = 0.0; 
    
    // Search radius scaled by light size (proper PCSS) instead of a fixed 1.2 m.
    // The fixed radius was far wider than small (< ~1.5 m) casters, so only a few
    // of the per-pixel rotated samples hit the occluder and the blocker statistics
    // jittered pixel-to-pixel -> random penumbra estimate -> salt-and-pepper grain.
    // Scaling with lightSize keeps the search proportional to the real penumbra and
    // the samples dense, killing the noise without changing the overall softness.
    float blockerSearchRadiusWS = clamp(lightSize * 3.0, 0.06, 1.2);
    float blockerSearchRadiusUV = blockerSearchRadiusWS / cascadeWorldWidth;
    blockerSearchRadiusUV = clamp(blockerSearchRadiusUV, texelSize.x * 1.5, 0.08);

    float blockerBiasWS = 0.0015; 
    float blockerBiasUVz = blockerBiasWS / depthRange;
    float blockerThresholdDepth = unbiasedProjCoords.z - blockerBiasUVz;

    for (int i = 0; i < PCSS_BLOCKER_SAMPLES; ++i)
    {
        vec2 offset = GetVogelDiskSample(i, PCSS_BLOCKER_SAMPLES, phi, jitter) * blockerSearchRadiusUV;
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

    // PCF Filtering in World-Space Meters (penumbra-adaptive Vogel samples)
    float minBlurWS = 0.001; 
    float maxBlurWS = min(4.5, cascadeWorldWidth * 0.18); 

    float filterRadiusWS = clamp(penumbraSizeWS, minBlurWS, maxBlurWS);
    float filterRadiusUV = clamp(filterRadiusWS / cascadeWorldWidth, texelSize.x * 1.0, 0.15);

    // Penumbra-adaptive tap count: razor-sharp contacts stay cheap (few taps),
    // wide penumbrae get more taps to kill residual grain. Keeps the average
    // cost near the old fixed 16 while making the soft regions visibly smoother.
    float penumbraNorm = clamp((filterRadiusWS - minBlurWS) / max(maxBlurWS - minBlurWS, 1e-4), 0.0, 1.0);
    int sampleCount = int(mix(float(PCSS_MIN_PCF_SAMPLES), float(PCSS_MAX_PCF_SAMPLES), penumbraNorm) + 0.5);
    sampleCount = clamp(sampleCount, PCSS_MIN_PCF_SAMPLES, PCSS_MAX_PCF_SAMPLES);

    // Gaussian-weighted accumulation (center taps weigh more) for a smoother,
    // more natural penumbra falloff than a flat disk average. normR2 matches the
    // Vogel radial distribution (r^2 grows linearly with sample index).
    float visibility = 0.0;
    float weightSum = 0.0;
    for (int i = 0; i < sampleCount; ++i)
    {
        float normR2 = (float(i) + 0.5) / float(sampleCount);
        float w = exp(-PCSS_GAUSSIAN_FALLOFF * normR2);
        vec2 offset = GetVogelDiskSample(i, sampleCount, phi, jitter) * filterRadiusUV;
        visibility += w * texture(u_ShadowMap, vec4(unbiasedProjCoords.xy + offset, float(layer), biasedDepth));
        weightSum += w;
    }

    return visibility / max(weightSum, 1e-4);
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
    
    float shadowRes = max(u_PointShadowResolution, 1.0);
    float distanceToLight = length(fragToLightUnbiased);
    // World size of one shadow-cube texel at the receiver. A cube face is a 90-deg
    // FOV frustum, so the face spans 2*distance across 'shadowRes' texels.
    float texelSize = (2.0 * distanceToLight) / shadowRes;
    // Normal-offset bias. The shadow texel's footprint ALONG the surface grows as
    // texelSize * tan(incidence) = texelSize * slopeScale. A point light embedded in
    // a wall makes that wall extremely grazing, so the offset must scale by slopeScale
    // (not just sinTheta, which saturates at 1 and was far too weak -> residual acne).
    float normalOffset = texelSize * (1.0 + 2.0 * min(slopeScale, 6.0));
    normalOffset = max(normalOffset, 0.003);
    // Light-direction offset mops up residual self-shadow on near-facing surfaces.
    float depthOffset = texelSize * (0.5 + min(slopeScale, 3.0));
    
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
    // Screen-space blue-noise rotation (replaces world-space white-noise hash)
    float randomAngle = SampleBlueNoise(gl_FragCoord.xy).r * 2.0 * PI;
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
        
        // Distance-scaled slope bias. One shadow texel covers ~ (2 * sZEye / res)
        // in world units and required bias grows with surface slope; the old fixed
        // 0.02 cap was far too small for distant texels, so far / grazing surfaces
        // self-shadowed (acne). Scaling with the texel footprint fixes it at all ranges.
        float worldTexelSize = (2.0 * sZEye) / shadowRes;
        float linearBias = worldTexelSize * (1.5 + 2.0 * min(slopeScale, 6.0));
        linearBias = clamp(linearBias, 0.0015, farPlane * 0.02);
        
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

    // WP19: silhouette fix. On smooth-shaded low-poly geometry the
    // interpolated (and normal-mapped) normal can face AWAY from the camera
    // near silhouettes (dot(N,V) < 0). ComputePBRDirect hard-rejects those
    // pixels (NdotV <= 0 -> vec3(0)) and specularOcclusion zeroes the IBL
    // specular, leaving a near-black patch that slides with the view angle.
    // Bend N the minimal amount needed to sit on the view horizon instead;
    // pixels with dot(N,V) >= kHorizonEps are completely untouched.
    {
        const float kHorizonEps = 0.02;
        float NoV = dot(N, V);
        if (NoV < kHorizonEps)
            N = SafeNormalize(N + V * (kHorizonEps - NoV));
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

    // POINT LIGHTS (Clustered Forward+)
    uint clusterIndex = (u_ClusteredEnabled == 1) ? ComputeClusterIndex() : 0u;

    uint pointOffset = 0u;
    uint pointCount  = u_PointLightCount;
    if (u_ClusteredEnabled == 1)
    {
        uvec2 pg = b_PointGrid[clusterIndex];
        pointOffset = pg.x;
        pointCount  = pg.y;
    }

    for (uint pi = 0u; pi < pointCount; ++pi)
    {
        uint i = (u_ClusteredEnabled == 1) ? b_PointIndices[pointOffset + pi] : pi;

        vec3 lightPos = b_PointLights[i].PositionIntensity.xyz;
        float distance = length(lightPos - v_WorldPos);
        float radius = b_PointLights[i].ColorRadius.w;

        if (distance < radius)
        {
            vec3 L = SafeNormalize(lightPos - v_WorldPos);
            float NdotL = max(dot(N, L), 0.0);

            if (NdotL > 0.0)
            {
                float intensity = b_PointLights[i].PositionIntensity.w;
                float falloffExp = max(b_PointLights[i].Falloff.x, 0.001);
                float window = pow(clamp(1.0 - (distance / radius), 0.0, 1.0), falloffExp);
                float attenuation = window / (distance * distance + 0.001);

                if (attenuation > 0.0001)
                {
                    float pShadow = PointShadowCalculation(v_WorldPos, b_PointLights[i], N);
                    if (pShadow > 0.0)
                    {
                        vec3 radiance = b_PointLights[i].ColorRadius.rgb * intensity * attenuation * pShadow;
                        Lo += ComputePBRDirect(N, V, L, albedo, roughnessSpec, metallic, F0, radiance);
                    }
                }
            }
        }
    }

    // SPOT LIGHTS (Clustered Forward+)
    uint spotOffset = 0u;
    uint spotCount  = u_SpotLightCount;
    if (u_ClusteredEnabled == 1)
    {
        uvec2 sg = b_SpotGrid[clusterIndex];
        spotOffset = sg.x;
        spotCount  = sg.y;
    }

    for (uint si = 0u; si < spotCount; ++si)
    {
        uint i = (u_ClusteredEnabled == 1) ? b_SpotIndices[spotOffset + si] : si;

        vec3 lightPos = b_SpotLights[i].PositionIntensity.xyz;
        float distance = length(lightPos - v_WorldPos);
        float radius = b_SpotLights[i].DirectionRadius.w;

        if (distance < radius)
        {
            vec3 L = SafeNormalize(lightPos - v_WorldPos);
            float NdotL = max(dot(N, L), 0.0);

            if (NdotL > 0.0)
            {
                vec3 lightDir = SafeNormalize(b_SpotLights[i].DirectionRadius.xyz);
                float theta = dot(L, -lightDir); 
                float innerCutOff = b_SpotLights[i].ColorCutoff.w;
                float outerCutOff = b_SpotLights[i].Falloff.y;
                float epsilon = max(innerCutOff - outerCutOff, 0.0001);
                float intensityMultiplier = clamp((theta - outerCutOff) / epsilon, 0.0, 1.0);

                if (intensityMultiplier > 0.0)
                {
                    float intensity = b_SpotLights[i].PositionIntensity.w;
                    float falloffExp = max(b_SpotLights[i].Falloff.x, 0.001);
                    float window = pow(clamp(1.0 - (distance / radius), 0.0, 1.0), falloffExp);
                    float attenuation = window / (distance * distance + 0.001);

                    if (attenuation > 0.0001)
                    {
                        float sShadow = SpotShadowCalculation(v_WorldPos, b_SpotLights[i], L);
                        if (sShadow > 0.0)
                        {
                            vec3 cookieColor = vec3(1.0);
                            int cookieIndex = int(b_SpotLights[i].Falloff.z);
                            
                            if (cookieIndex >= 16 && cookieIndex < 24)
                            {
                                int arrayIndex = cookieIndex - 16;
                                vec4 lightSpacePos = b_SpotLights[i].LightSpaceMatrix * vec4(v_WorldPos, 1.0);
                                vec3 projCoords = lightSpacePos.xyz / max(lightSpacePos.w, 0.0001);
                                projCoords = projCoords * 0.5 + 0.5;
                                
                                float cookieSize = max(b_SpotLights[i].Falloff.w, 0.0001);
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

                            vec3 radiance = b_SpotLights[i].ColorCutoff.rgb * intensity * attenuation * intensityMultiplier * cookieColor * sShadow;
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
    
    float probeWeight = 0.0;
    vec3 localReflection = SampleReflectionProbes(R, v_WorldPos, roughness, prefilteredColor, probeWeight);

    // R2 Step 3b: o_Color now carries ONLY the probe/global reflection. The SSR
    // contribution is exported as a *delta* into o_SSR, denoised in a separable
    // bilateral pass, then re-added in screen.glsl. With an identity denoise this
    // is mathematically identical to the previous inline blend.
    vec3 reflectionColor = localReflection * skyVisibility;
    vec4 ssrOut = vec4(0.0);
    if (roughness < 0.6)
    {
        vec4 ssrResult = StochasticSSR(N, V, roughness, F0);
        if (ssrResult.a > 0.001)
        {
            float ssrRoughnessFade = smoothstep(0.6, 0.2, roughness);
            float ssrWeight = clamp(ssrResult.a, 0.0, 1.0) * ssrRoughnessFade;
            // delta = ssrWeight * (ssrRadiance - baseReflection), pre-multiplied by
            // the same specular BRDF / occlusion / env-intensity terms o_Color uses,
            // so the composite in screen.glsl is a plain additive blend.
            vec3 ssrDelta = ssrWeight * (ssrResult.rgb - localReflection * skyVisibility);
            ssrOut.rgb = ssrDelta * (F_IBL * brdf.x + brdf.y) * specularOcclusion * u_EnvironmentIntensity;
            // Per-pixel blur radius: 0 = mirror-sharp, 1 = widest glossy blur.
            ssrOut.a = clamp(roughness / 0.6, 0.0, 1.0);
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

    float gbufAlpha = (u_IsTransparent != 0) ? 0.0 : 1.0;
    o_SSR = vec4(ssrOut.rgb, (u_IsTransparent != 0) ? 0.0 : ssrOut.a);
    o_Normal = vec4(normalize(N) * 0.5 + 0.5, gbufAlpha); 
}