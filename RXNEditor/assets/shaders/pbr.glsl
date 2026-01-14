#type vertex
#version 450 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;

// INSTANCING: Matrix is split into 4 vec4s
layout(location = 4) in vec4 a_ModelRow0;
layout(location = 5) in vec4 a_ModelRow1;
layout(location = 6) in vec4 a_ModelRow2;
layout(location = 7) in vec4 a_ModelRow3;

uniform mat4 u_ViewProjection;

out vec2 v_TexCoord;
out vec3 v_WorldPos;
out vec3 v_Normal;

void main()
{
    // Reconstruct Model Matrix from Instanced Attributes
    mat4 model = mat4(a_ModelRow0, a_ModelRow1, a_ModelRow2, a_ModelRow3);

    // Calculate World Position
    vec4 worldPos = model * vec4(a_Position, 1.0);
    v_WorldPos = worldPos.xyz;
    v_TexCoord = a_TexCoord;

    // Normal Matrix (Inverse Transpose to handle non-uniform scaling)
    // In a real AAA engine, you might pre-calculate this on CPU or send it as another attribute
    // but calculating it here is fine for now.
    v_Normal = mat3(transpose(inverse(model))) * a_Normal;

    gl_Position = u_ViewProjection * worldPos;
}

#type fragment
#version 450 core

out vec4 FragColor;

in vec2 v_TexCoord;
in vec3 v_WorldPos;
in vec3 v_Normal;

// MATERIAL DATA
uniform sampler2D u_AlbedoMap;
uniform sampler2D u_NormalMap;
uniform sampler2D u_MetallicMap;
uniform sampler2D u_RoughnessMap;
uniform sampler2D u_AOMap;
uniform vec4 u_AlbedoColor;

// Material Fallback/Tints
uniform float u_Metallic;
uniform float u_Roughness;
uniform float u_AO;

// Toggles
uniform int u_UseNormalMap;

// SCENE DATA
uniform vec3 u_CameraPosition;
uniform samplerCube u_EnvironmentMap;

layout(binding = 8) uniform sampler2DArray u_ShadowMap; 
layout (std140, binding = 2) uniform ShadowData
{
    mat4 u_LightSpaceMatrices[4];
    vec4 u_CascadePlaneDistances[4]; // x = distance
};
uniform mat4 u_View;

layout(binding = 10) uniform samplerCube u_IrradianceMap;
layout(binding = 11) uniform samplerCube u_PrefilterMap;
layout(binding = 12) uniform sampler2D   u_BRDFLUT;

// LIGHTING UBO (Must match C++ Struct Alignment!)
struct PointLight {
    vec4 PositionIntensity; // xyz = pos, w = intensity
    vec4 ColorRadius;       // xyz = color, w = radius
    vec4 Falloff;           // x = falloff
};

layout(std140, binding = 1) uniform LightData {
    vec4 u_DirLightDirection; // w = intensity
    vec4 u_DirLightColor;
    uint u_PointLightCount;
    // Padding ensures alignment
    PointLight u_PointLights[100];
};

const float PI = 3.14159265359;

// ----------------------------------------------------------------------------
// PBR MATH FUNCTIONS (Cook-Torrance)
// ----------------------------------------------------------------------------

// 1. Normal Distribution Function (GGX)
// Approximates the amount of microfacets aligned to the halfway vector
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / max(denom, 0.0000001); // Avoid divide by zero
}

// 2. Geometry Function (Schlick-GGX)
// Approximates microfacet self-shadowing
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

// 3. Fresnel Equation (Fresnel-Schlick)
// Calculates reflectance at grazing angles
vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Helper to calculate tangents on the fly (Perturb Normal)
// This saves us from sending Tangents/Bitangents from CPU!
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

    if (length(T) < 0.0001 || length(B) < 0.0001)
    {
        return N;
    }

    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
}

vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// ----------------------------------------------------------------------------
// MAIN
// ----------------------------------------------------------------------------

float ShadowCalculation(vec3 fragPosWorld)
{
    vec4 fragPosViewSpace = u_View * vec4(fragPosWorld, 1.0);
    float depthValue = abs(fragPosViewSpace.z);

    int layer = -1;
    for (int i = 0; i < 4; ++i)
    {
        if (depthValue < u_CascadePlaneDistances[i].x)
        {
            layer = i;
            break;
        }
    }
    if (layer == -1) layer = 3;

    vec4 fragPosLightSpace = u_LightSpaceMatrices[layer] * vec4(fragPosWorld, 1.0);
    
    // Perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    float currentDepth = projCoords.z;
    if (currentDepth > 1.0) return 0.0;

    // Bias to prevent shadow acne
    vec3 normal = normalize(v_Normal);
    vec3 lightDir = normalize(-u_DirLightDirection.xyz);
    float bias = max(0.005 * (1.0 - dot(normal, lightDir)), 0.0005);
    // bias *= 1.0 / (u_CascadePlaneDistances[layer].x * 0.5); // Optional: scale bias by cascade

    // PCF (Percentage-closer filtering)
    float shadow = 0.0;
    vec2 texelSize = 1.0 / vec2(textureSize(u_ShadowMap, 0));
    for(int x = -1; x <= 1; ++x)
    {
        for(int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(u_ShadowMap, vec3(projCoords.xy + vec2(x, y) * texelSize, layer)).r; 
            shadow += (currentDepth - bias) > pcfDepth ? 1.0 : 0.0;        
        }    
    }
    shadow /= 9.0;
    
    return shadow;
}

void main()
{
    // 1. Sample Material
    // Use texture values mixed with uniform factors (simplifies fallback logic)

    vec4 albedoSample = texture(u_AlbedoMap, v_TexCoord);
    vec3 albedo = pow(albedoSample.rgb, vec3(2.2)) * u_AlbedoColor.rgb; // Linearize gamma
    float alpha = albedoSample.a * u_AlbedoColor.a;

    float metallic = texture(u_MetallicMap, v_TexCoord).r * u_Metallic; 
    float roughness = texture(u_RoughnessMap, v_TexCoord).g * u_Roughness;
    float ao = texture(u_AOMap, v_TexCoord).b * u_AO;
     
    float shadow = ShadowCalculation(v_WorldPos);

    // 2. Calculate Normal
    vec3 N = normalize(v_Normal);
    if (u_UseNormalMap > 0) // Or just check if texture isn't the dummy blue one
    {
        N = GetNormalFromMap();
    }
    
    vec3 V = normalize(u_CameraPosition - v_WorldPos);

    // 3. Calculate Reflectance (F0)
    // Dielectrics (Plastic/Wood) = 0.04, Metals = Albedo color
    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo, metallic);

    // 4. Lighting Loop
    vec3 Lo = vec3(0.0);

    // --- DIRECTIONAL LIGHT ---
    {
        vec3 L = normalize(-u_DirLightDirection.xyz);
        vec3 H = normalize(V + L);
        float intensity = u_DirLightDirection.w;
        vec3 radiance = u_DirLightColor.rgb * intensity * (1.0 - shadow);

        // Cook-Torrance BRDF
        float NDF = DistributionGGX(N, H, roughness);   
        float G   = GeometrySmith(N, V, L, roughness);      
        vec3 F    = FresnelSchlick(max(dot(H, V), 0.0), F0);
           
        vec3 numerator    = NDF * G * F; 
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001; // + 0.0001 to prevent divide by zero
        vec3 specular = numerator / denominator;
        
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;	  

        float NdotL = max(dot(N, L), 0.0);        

        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    // --- POINT LIGHTS ---
    for(int i = 0; i < int(u_PointLightCount); ++i)
    {
        vec3 lightPos = u_PointLights[i].PositionIntensity.xyz;
        vec3 L = normalize(lightPos - v_WorldPos);
        vec3 H = normalize(V + L);
        
        float distance = length(lightPos - v_WorldPos);
        float radius = u_PointLights[i].ColorRadius.w;

        if (distance < radius)
        {
            float intensity = u_PointLights[i].PositionIntensity.w;
            
            // Attenuation (Physical falloff)
            float attenuation = 1.0 / (distance * distance);
            
            // Soft Falloff (Windowing)
            float falloff = u_PointLights[i].Falloff.x; // Not used in standard formula but available
            
            vec3 radiance = u_PointLights[i].ColorRadius.rgb * intensity * attenuation;

            // Cook-Torrance BRDF (Same as above)
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

    // 5. Ambient & AO

    vec3 F_IBL = FresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);

    // B. Diffuse Irradiance (Background light)
    vec3 kS_IBL = F_IBL;
    vec3 kD_IBL = 1.0 - kS_IBL;
    kD_IBL *= 1.0 - metallic; // Metals consume all diffuse light
    
    vec3 irradiance = texture(u_IrradianceMap, N).rgb;
    vec3 diffuseIBL = irradiance * albedo;

    // C. Specular IBL (Reflections)
    // 1. Get reflection vector
    vec3 R = reflect(-V, N); 
    
    // 2. Sample Prefilter Map based on Roughness
    // MAX_REFLECTION_LOD should match the mip levels in CreatePrefilterMap (we used 5)
    const float MAX_REFLECTION_LOD = 4.0;
    vec3 prefilteredColor = textureLod(u_PrefilterMap, R, roughness * MAX_REFLECTION_LOD).rgb;
    
    // 3. Sample BRDF LUT (Scale and Bias for Fresnel)
    vec2 brdf  = texture(u_BRDFLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;
    vec3 specularIBL = prefilteredColor * (F_IBL * brdf.x + brdf.y);

    // D. Combine
    vec3 ambient = (kD_IBL * diffuseIBL + specularIBL) * ao;
    
    // ------------------------------------------------------------------

    vec3 color = ambient + Lo;

    float fresnelAlpha = pow(1.0 - max(dot(N, V), 0.0), 2.0);
    alpha = clamp(alpha + fresnelAlpha * 0.5, 0.0, 1.0);

    FragColor = vec4(color, alpha);
}