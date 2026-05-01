#type vertex
#version 450 core
layout(location = 0) in vec3 a_Position;

uniform mat4 u_ViewProjection;
uniform mat4 u_Model;

out vec4 v_ScreenPos;

void main()
{
    vec4 clipPos = u_ViewProjection * u_Model * vec4(a_Position, 1.0);
    v_ScreenPos = clipPos;
    gl_Position = clipPos;
}

#type fragment
#version 450 core
layout(location = 0) out vec4 o_Color;

in vec4 v_ScreenPos;

uniform sampler2D u_SceneDepth;
uniform mat4 u_InverseViewProjection;
uniform vec3 u_LightPos;
uniform vec3 u_LightDir;
uniform float u_Radius;
uniform float u_CutoffCos;
uniform vec4 u_Color;

void main()
{
    vec2 ndc = (v_ScreenPos.xy / v_ScreenPos.w) * 0.5 + 0.5;
    float depth = texture(u_SceneDepth, ndc).r;

    vec4 clipSpacePos = vec4(ndc * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 worldPos = u_InverseViewProjection * clipSpacePos;
    worldPos /= worldPos.w;

    vec3 dirToSurface = worldPos.xyz - u_LightPos;
    float distToSurface = length(dirToSurface);
    dirToSurface = normalize(dirToSurface);

    float cosAngle = dot(dirToSurface, u_LightDir);
    
    if (distToSurface > u_Radius || cosAngle < u_CutoffCos)
        discard;

    float edgeDistThickness = 0.05 * u_Radius;
    float alphaDist = smoothstep(u_Radius - edgeDistThickness, u_Radius, distToSurface);

    float edgeAngleThickness = 0.005; 
    float alphaAngle = smoothstep(u_CutoffCos, u_CutoffCos + edgeAngleThickness, cosAngle);
    alphaAngle = 1.0 - alphaAngle;

    float finalAlpha = max(alphaDist, alphaAngle);

    if (finalAlpha <= 0.01)
        discard;

    o_Color = vec4(u_Color.rgb, u_Color.a * finalAlpha * 2.0);
}