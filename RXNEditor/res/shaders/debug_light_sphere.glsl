#type vertex
#version 450 core
layout(location = 0) in vec3 a_Position;

uniform mat4 u_ViewProjection;
uniform mat4 u_Model;

out vec4 v_ScreenPos;
out vec3 v_LocalPos;

void main()
{
    v_LocalPos = a_Position;
    vec4 clipPos = u_ViewProjection * u_Model * vec4(a_Position, 1.0);
    v_ScreenPos = clipPos;
    gl_Position = clipPos;
}

#type fragment
#version 450 core
layout(location = 0) out vec4 o_Color;

in vec4 v_ScreenPos;
in vec3 v_LocalPos;

uniform sampler2D u_SceneDepth;
uniform mat4 u_InverseViewProjection;
uniform vec3 u_LightPos;
uniform float u_Radius;
uniform vec4 u_Color;

void main()
{
    vec2 ndc = (v_ScreenPos.xy / v_ScreenPos.w) * 0.5 + 0.5;

    float depth = texture(u_SceneDepth, ndc).r;

    vec4 clipSpacePos = vec4(ndc * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 worldPos = u_InverseViewProjection * clipSpacePos;
    worldPos /= worldPos.w;

    float distToLight = length(worldPos.xyz - u_LightPos);

    float edgeThickness = 0.05 * u_Radius;
    float alpha = smoothstep(u_Radius - edgeThickness, u_Radius, distToLight);
    alpha *= 1.0 - smoothstep(u_Radius, u_Radius + edgeThickness, distToLight);

    if (alpha <= 0.01)
        discard;

    o_Color = vec4(u_Color.rgb, u_Color.a * alpha * 2.0);
}