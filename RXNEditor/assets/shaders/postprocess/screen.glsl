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

    FragColor = vec4(mapped, 1.0);
}