#type vertex
#version 450 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;

layout(location = 4) in vec4 a_ModelRow0;
layout(location = 5) in vec4 a_ModelRow1;
layout(location = 6) in vec4 a_ModelRow2;
layout(location = 7) in vec4 a_ModelRow3;

uniform mat4 u_ViewProjection;
uniform float u_Tiling;

out vec2 v_TexCoord;
out vec3 v_WorldPos;
out vec3 v_Normal;

void main()
{
    mat4 model = mat4(a_ModelRow0, a_ModelRow1, a_ModelRow2, a_ModelRow3);
    vec4 worldPos = model * vec4(a_Position, 1.0);
    
    v_WorldPos = worldPos.xyz;
    
    // Apply Tiling here (Cheaper than in Fragment Shader)
    v_TexCoord = a_TexCoord * u_Tiling; 

    v_Normal = mat3(transpose(inverse(model))) * a_Normal;

    gl_Position = u_ViewProjection * worldPos;
}

#type fragment
#version 450 core

layout(location = 0) out vec4 o_Color;

in vec2 v_TexCoord;
in vec3 v_WorldPos;
in vec3 v_Normal;
flat in int v_EntityID;

// --- MATERIAL UNIFORMS (Must match C++ Material::Bind) ---
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

// --- SCENE DATA ---
uniform vec3 u_CameraPosition;

// Shadows
layout(binding = 8) uniform sampler2DArrayShadow u_ShadowMap; 
layout (std140, binding = 2) uniform ShadowData
{
    mat4 u_LightSpaceMatrices[4];
    vec4 u_CascadePlaneDistances[4];
};
uniform mat4 u_View;

// IBL
layout(binding = 10) uniform samplerCube u_IrradianceMap;
layout(binding = 11) uniform samplerCube u_PrefilterMap;
layout(binding = 12) uniform sampler2D   u_BRDFLUT;

layout(binding = 13) uniform sampler2DArrayShadow u_SpotShadowMap; 
layout(binding = 14) uniform samplerCubeArray u_PointShadowMap; 

layout(binding = 16) uniform sampler2D u_LightCookies[8];

// Lighting UBO
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

// PBR MATH 

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

// Tangent Space Normal Calculation
vec3 GetNormalFromMap()
{
    vec3 tangentNormal = texture(u_NormalMap, v_TexCoord).xyz * 2.0 - 1.0;

    vec3 Q1 = dFdx(v_WorldPos);
    vec3 Q2 = dFdy(v_WorldPos);
    vec2 st1 = dFdx(v_TexCoord);
    vec2 st2 = dFdy(v_TexCoord);

    vec3 N = normalize(v_Normal);
    
    vec3 T_unnorm = Q1 * st2.t - Q2 * st1.t;
    if (length(T_unnorm) < 0.0001) return N; 
    
    vec3 T = normalize(T_unnorm);
    vec3 B = -normalize(cross(N, T));

    mat3 TBN = mat3(T, B, N);
    return normalize(TBN * tangentNormal);
}

// SHADOWS
float ShadowCalculation(vec3 fragPosWorld)
{
    vec4 fragPosViewSpace = u_View * vec4(fragPosWorld, 1.0);
    float depthValue = abs(fragPosViewSpace.z);

    int layer = -1;
    for (int i = 0; i < 4; ++i) {
        if (depthValue < u_CascadePlaneDistances[i].x) {
            layer = i;
            break;
        }
    }
    if (layer == -1) layer = 3;

    vec3 normal = normalize(v_Normal);
    vec3 lightDir = normalize(-u_DirLightDirection.xyz);

    float normalOffsetScale = 0.01 * float(layer + 1);
    
    vec3 shadowPos = fragPosWorld + normal * normalOffsetScale * max(1.0 - dot(normal, lightDir), 0.00);

    vec4 fragPosLightSpace = u_LightSpaceMatrices[layer] * vec4(shadowPos, 1.0);
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.z > 1.0) return 0.0;

    float minBias = 0.00003;
    float maxBias = 0.0003;
    float cascadeMultiplier = float(layer + 1);
    float bias = max(maxBias * (1.0 - dot(normal, lightDir)), minBias) * cascadeMultiplier;

    float visibility = 0.0;
    vec2 texelSize = 1.0 / vec2(textureSize(u_ShadowMap, 0));
    
    // automatically gives 16-tap smoothing due to hardware bilinear interpolation
    vec2 offsets[4] = vec2[](
        vec2(-0.5, -0.5), vec2(0.5, -0.5),
        vec2(-0.5,  0.5), vec2(0.5,  0.5)
    );

    for(int i = 0; i < 4; ++i) {
        visibility += texture(u_ShadowMap, vec4(projCoords.xy + offsets[i] * texelSize, float(layer), projCoords.z - bias));
    }
    
    visibility /= 4.0;
    return 1.0 - visibility;
}

float SpotShadowCalculation(vec3 fragPosWorld, mat4 lightSpaceMatrix, int shadowIndex) {
    if (shadowIndex < 0)
        return 1.0;
    
    vec4 fragPosLightSpace = lightSpaceMatrix * vec4(fragPosWorld, 1.0);
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    
    if(projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0)
        return 1.0;
    
    float shadow = 0.0;
    vec2 texelSize = 1.0 / vec2(textureSize(u_SpotShadowMap, 0));
    vec2 offsets[4] = vec2[]( vec2(-0.5,-0.5), vec2(0.5,-0.5), vec2(-0.5,0.5), vec2(0.5,0.5) );
    
    for(int i = 0; i < 4; ++i)
        shadow += texture(u_SpotShadowMap, vec4(projCoords.xy + offsets[i] * texelSize, float(shadowIndex), projCoords.z - 0.0005));

    return shadow / 4.0;
}

float PointShadowCalculation(vec3 fragPosWorld, vec3 lightPos, float farPlane, int shadowIndex) {
    if (shadowIndex < 0)
        return 1.0;
    
    vec3 fragToLight = fragPosWorld - lightPos;
    
    vec3 absVec = abs(fragToLight);
    float z_eye = max(absVec.x, max(absVec.y, absVec.z));
    
    if (z_eye < 0.001) return 1.0;
    
    float n = 0.1;
    float f = farPlane;
    
    float z_ndc = (f + n) / (f - n) - (2.0 * f * n) / ((f - n) * z_eye);
    
    float currentDepth = z_ndc * 0.5 + 0.5;
    
    float closestDepth = texture(u_PointShadowMap, vec4(fragToLight, float(shadowIndex))).r;
    
    float dz_dzeye = (2.0 * f * n) / ((f - n) * z_eye * z_eye);
    float bias = clamp(0.15 * dz_dzeye, 0.00005, 0.005);
    
    return (currentDepth - bias > closestDepth) ? 0.0 : 1.0;
}

// ----------------------------------------------------------------------------
// MAIN
// ----------------------------------------------------------------------------
void main()
{
    // GATHER MATERIAL DATA
    vec4 albedoSample = texture(u_AlbedoMap, v_TexCoord);
    vec3 albedo = pow(albedoSample.rgb, vec3(2.2)) * u_AlbedoColor.rgb; 
    
    float alpha = albedoSample.a * u_AlbedoColor.a; 

    // Combine Texture * Uniform for Modulated Control
    float metallic  = texture(u_MetalnessMap, v_TexCoord).b * u_Metalness; 
    float roughness = texture(u_RoughnessMap, v_TexCoord).g * u_Roughness;
    float ao        = texture(u_AOMap, v_TexCoord).r * u_AO;

    vec3 emissive   = texture(u_EmissiveMap, v_TexCoord).rgb * u_EmissiveColor;

    // PREPARE VECTORS
    vec3 N = normalize(v_Normal);
    if (u_UseNormalMap > 0) N = GetNormalFromMap();
    
    vec3 V = normalize(u_CameraPosition - v_WorldPos);
    vec3 R = reflect(-V, N); 

    // Dielectrics F0 = 0.04, Metals F0 = Albedo
    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo, metallic);

    // LIGHTING LOOP
    vec3 Lo = vec3(0.0);
    float shadow = ShadowCalculation(v_WorldPos);

    // Directional Light
    {
        vec3 L = normalize(-u_DirLightDirection.xyz);
        vec3 H = normalize(V + L);
        vec3 radiance = u_DirLightColor.rgb * u_DirLightDirection.w * (1.0 - shadow);

        float NDF = DistributionGGX(N, H, roughness);   
        float G   = GeometrySmith(N, V, L, roughness);      
        vec3 F    = FresnelSchlick(max(dot(H, V), 0.0), F0);
           
        vec3 numerator    = NDF * G * F; 
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular = numerator / denominator;
        
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;   

        float NdotL = max(dot(N, L), 0.0);        
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    // Point Lights
    for(int i = 0; i < int(u_PointLightCount); ++i)
    {
        vec3 lightPos = u_PointLights[i].PositionIntensity.xyz;
        float distance = length(lightPos - v_WorldPos);
        float radius = u_PointLights[i].ColorRadius.w;

        if (distance < radius)
        {
            vec3 L = normalize(lightPos - v_WorldPos);
            vec3 H = normalize(V + L);
            float intensity = u_PointLights[i].PositionIntensity.w;

            float falloffExp = max(u_PointLights[i].Falloff.x, 0.001);
            float window = pow(clamp(1.0 - (distance / radius), 0.0, 1.0), falloffExp);
            float attenuation = window / (distance * distance + 0.001);

            int shadowIdx = int(u_PointLights[i].ExtraData.x);
            float pShadow = PointShadowCalculation(v_WorldPos, lightPos, radius, shadowIdx);
            vec3 radiance = u_PointLights[i].ColorRadius.rgb * intensity * attenuation * pShadow;

            float NDF = DistributionGGX(N, H, roughness);   
            float G   = GeometrySmith(N, V, L, roughness);      
            vec3 F    = FresnelSchlick(max(dot(H, V), 0.0), F0);
                
            vec3 numerator    = NDF * G * F; 
            float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001; 
            vec3 specular = numerator / denominator;
            
            vec3 kS = F;
            vec3 kD = vec3(1.0) - kS;
            kD *= 1.0 - metallic;   

            float NdotL = max(dot(N, L), 0.0);        
            Lo += (kD * albedo / PI + specular) * radiance * NdotL;
        }
    }

// Spot Lights
    for(int i = 0; i < int(u_SpotLightCount); ++i)
    {
        vec3 lightPos = u_SpotLights[i].PositionIntensity.xyz;
        float distance = length(lightPos - v_WorldPos);
        float radius = u_SpotLights[i].DirectionRadius.w;

        if (distance < radius)
        {
            vec3 L = normalize(lightPos - v_WorldPos);
            vec3 lightDir = normalize(u_SpotLights[i].DirectionRadius.xyz);
            
            float theta = dot(L, -lightDir); 
            float innerCutOff = u_SpotLights[i].ColorCutoff.w;
            float outerCutOff = u_SpotLights[i].Falloff.y;
            float epsilon = innerCutOff - outerCutOff;
            float intensityMultiplier = clamp((theta - outerCutOff) / epsilon, 0.0, 1.0);

            if (intensityMultiplier > 0.0)
            {
                vec3 cookieColor = vec3(1.0);
                int cookieIndex = int(u_SpotLights[i].Falloff.z);
                
                if (cookieIndex >= 16 && cookieIndex < 24)
                {
                    int arrayIndex = cookieIndex - 16;
                    
                    vec4 lightSpacePos = u_SpotLights[i].LightSpaceMatrix * vec4(v_WorldPos, 1.0);
                    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
                    projCoords = projCoords * 0.5 + 0.5;
                    
                    float cookieSize = u_SpotLights[i].Falloff.w;
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

                vec3 H = normalize(V + L);
                float intensity = u_SpotLights[i].PositionIntensity.w;

                float falloffExp = max(u_SpotLights[i].Falloff.x, 0.001);
                float window = pow(clamp(1.0 - (distance / radius), 0.0, 1.0), falloffExp);
                float attenuation = window / (distance * distance + 0.001);

                int shadowIdx = int(u_SpotLights[i].ExtraData.x);
                float sShadow = SpotShadowCalculation(v_WorldPos, u_SpotLights[i].LightSpaceMatrix, shadowIdx);
                vec3 radiance = u_SpotLights[i].ColorCutoff.rgb * intensity * attenuation * intensityMultiplier * cookieColor * sShadow;

                float NDF = DistributionGGX(N, H, roughness);   
                float G   = GeometrySmith(N, V, L, roughness);      
                vec3 F    = FresnelSchlick(max(dot(H, V), 0.0), F0);
                    
                vec3 numerator    = NDF * G * F; 
                float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001; 
                vec3 specular = numerator / denominator;
                
                vec3 kS = F;
                vec3 kD = vec3(1.0) - kS;
                kD *= 1.0 - metallic;   

                float NdotL = max(dot(N, L), 0.0);        
                Lo += (kD * albedo / PI + specular) * radiance * NdotL;
            }
        }
    }

    // IBL (AMBIENT)
    vec3 F_IBL = FresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
    vec3 kS_IBL = F_IBL;
    vec3 kD_IBL = 1.0 - kS_IBL;
    kD_IBL *= 1.0 - metallic;
    
    vec3 irradiance = texture(u_IrradianceMap, N).rgb;
    vec3 diffuseIBL = irradiance * albedo;

    const float MAX_REFLECTION_LOD = 4.0;
    vec3 prefilteredColor = textureLod(u_PrefilterMap, R, roughness * MAX_REFLECTION_LOD).rgb;
    vec2 brdf  = texture(u_BRDFLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;
    vec3 specularIBL = prefilteredColor * (F_IBL * brdf.x + brdf.y);

    vec3 ambient = (kD_IBL * diffuseIBL + specularIBL) * ao * u_EnvironmentIntensity;
    
    // COMBINE
    vec3 color = ambient + Lo + emissive;
    color = min(color, vec3(1000.0));

    o_Color = vec4(color, alpha);
}