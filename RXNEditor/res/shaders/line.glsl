#type vertex
#version 450 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Color;

uniform mat4 u_ViewProjection;

out vec4 v_Color;

void main()
{
    gl_Position = u_ViewProjection * vec4(a_Position, 1.0);
    v_Color = a_Color;
}

#type fragment
#version 450 core

in vec4 v_Color;
layout(location = 0) out vec4 o_Color;
layout(location = 1) out vec4 o_SSR;
layout(location = 2) out vec4 o_Normal;

void main()
{
    o_Color = v_Color;
    o_SSR = vec4(0.0);
    o_Normal = vec4(0.5, 0.5, 1.0, 1.0);
}