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

vec3 ACESFilm(vec3 x)
{
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main()
{
    vec3 hdrColor = texture(u_ScreenTexture, v_TexCoord).rgb;
    vec3 bloomColor = texture(u_BloomTexture, v_TexCoord).rgb;

    hdrColor += bloomColor * u_BloomIntensity;

    vec3 mapped = hdrColor * u_Exposure;
    mapped = ACESFilm(mapped);
    mapped = pow(mapped, vec3(1.0 / u_Gamma));

    float centerMask = texture(u_OutlineTexture, v_TexCoord).r;
    
    float a = texture(u_OutlineTexture, v_TexCoord + vec2(u_TexelSize.x, 0.0)).r;
    float b = texture(u_OutlineTexture, v_TexCoord + vec2(-u_TexelSize.x, 0.0)).r;
    float c = texture(u_OutlineTexture, v_TexCoord + vec2(0.0, u_TexelSize.y)).r;
    float d = texture(u_OutlineTexture, v_TexCoord + vec2(0.0, -u_TexelSize.y)).r;
    
    float e = texture(u_OutlineTexture, v_TexCoord + vec2(u_TexelSize.x, u_TexelSize.y)).r;
    float f = texture(u_OutlineTexture, v_TexCoord + vec2(-u_TexelSize.x, -u_TexelSize.y)).r;
    float g = texture(u_OutlineTexture, v_TexCoord + vec2(u_TexelSize.x, -u_TexelSize.y)).r;
    float h = texture(u_OutlineTexture, v_TexCoord + vec2(-u_TexelSize.x, u_TexelSize.y)).r;

    float maxNeighbor = max(max(max(a, b), max(c, d)), max(max(e, f), max(g, h)));
    
    float outline = max(maxNeighbor - centerMask, 0.0);

    if (outline > 0.0)
    {
        mapped = mix(mapped, vec3(1.0, 0.5, 0.0), outline);
    }

    FragColor = vec4(mapped, 1.0);
}