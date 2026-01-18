#type vertex
#version 450 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;

// INSTANCING
layout(location = 4) in vec4 a_ModelRow0;
layout(location = 5) in vec4 a_ModelRow1;
layout(location = 6) in vec4 a_ModelRow2;
layout(location = 7) in vec4 a_ModelRow3;

uniform mat4 u_ViewProjection;
uniform float u_Tiling; // <--- NEW: Texture Tiling Factor

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

out vec4 FragColor;

in vec2 v_TexCoord;
in vec3 v_WorldPos;
in vec3 v_Normal;

// --- MATERIAL UNIFORMS (Must match C++ Material::Bind) ---
uniform sampler2D u_AlbedoMap;
uniform sampler2D u_NormalMap;
uniform sampler2D u_MetallicMap;
uniform sampler2D u_RoughnessMap;
uniform sampler2D u_AOMap;
uniform sampler2D u_EmissiveMap;

uniform vec4 u_AlbedoColor;
uniform vec3 u_EmissiveColor;
uniform float u_Metalness;
uniform float u_Roughness;
uniform float u_AO; // Strength multiplier

uniform int u_UseNormalMap;

// --- SCENE DATA ---
uniform vec3 u_CameraPosition;

// Shadows
layout(binding = 8) uniform sampler2DArray u_ShadowMap; 
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

// Lighting UBO
struct PointLight {
    vec4 PositionIntensity; 
    vec4 ColorRadius;       
    vec4 Falloff;           
};

layout(std140, binding = 1) uniform LightData {
    vec4 u_DirLightDirection; 
    vec4 u_DirLightColor;
    uint u_PointLightCount;
    PointLight u_PointLights[100];
};

const float PI = 3.14159265359;

// ----------------------------------------------------------------------------
// PBR MATH (Standard Cook-Torrance)
// ----------------------------------------------------------------------------

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
    vec3 T = normalize(Q1 * st2.t - Q2 * st1.t);
    vec3 B = -normalize(cross(N, T));

    if (length(T) < 0.0001 || length(B) < 0.0001) return N;

    mat3 TBN = mat3(T, B, N);
    return normalize(TBN * tangentNormal);
}

// ----------------------------------------------------------------------------
// SHADOWS
// ----------------------------------------------------------------------------
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

    vec4 fragPosLightSpace = u_LightSpaceMatrices[layer] * vec4(fragPosWorld, 1.0);
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.z > 1.0) return 0.0;

    vec3 normal = normalize(v_Normal);
    vec3 lightDir = normalize(-u_DirLightDirection.xyz);
    float bias = max(0.005 * (1.0 - dot(normal, lightDir)), 0.0005);
    
    // PCF
    float shadow = 0.0;
    vec2 texelSize = 1.0 / vec2(textureSize(u_ShadowMap, 0));
    for(int x = -1; x <= 1; ++x) {
        for(int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(u_ShadowMap, vec3(projCoords.xy + vec2(x, y) * texelSize, layer)).r; 
            shadow += (projCoords.z - bias) > pcfDepth ? 1.0 : 0.0;        
        }    
    }
    shadow /= 9.0;
    return shadow;
}

// ----------------------------------------------------------------------------
// MAIN
// ----------------------------------------------------------------------------
void main()
{
    // 1. GATHER MATERIAL DATA
    // -----------------------
    vec4 albedoSample = texture(u_AlbedoMap, v_TexCoord);
    vec3 albedo = pow(albedoSample.rgb, vec3(2.2)) * u_AlbedoColor.rgb; 
    
    // Note: We use the alpha from the Texture (useful for fences/leaves)
    float alpha = albedoSample.a * u_AlbedoColor.a; 

    // Combine Texture * Uniform for Modulated Control
    float metallic  = texture(u_MetallicMap, v_TexCoord).b * u_Metalness; 
    float roughness = texture(u_RoughnessMap, v_TexCoord).g * u_Roughness;
    float ao        = texture(u_AOMap, v_TexCoord).r * u_AO; // GLTF uses Red channel for AO usually

    // New Emissive Calculation
    vec3 emissive   = texture(u_EmissiveMap, v_TexCoord).rgb * u_EmissiveColor;

    // 2. PREPARE VECTORS
    // ------------------
    vec3 N = normalize(v_Normal);
    if (u_UseNormalMap > 0) N = GetNormalFromMap();
    
    vec3 V = normalize(u_CameraPosition - v_WorldPos);
    vec3 R = reflect(-V, N); 

    // Dielectrics F0 = 0.04, Metals F0 = Albedo
    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo, metallic);

    // 3. LIGHTING LOOP
    // ----------------
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
            float attenuation = 1.0 / (distance * distance);
            vec3 radiance = u_PointLights[i].ColorRadius.rgb * intensity * attenuation;

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

    // 4. IBL (AMBIENT)
    // ----------------
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

    vec3 ambient = (kD_IBL * diffuseIBL + specularIBL) * ao;
    
    // 5. COMBINE
    // ----------
    vec3 color = ambient + Lo + emissive;

    FragColor = vec4(color, alpha);
}