#type vertex
#version 450 core

layout(location = 0) in vec3 a_Position;

out vec3 v_TexCoord;

uniform mat4 u_ViewProjection;

void main()
{
    v_TexCoord = a_Position;
    
    vec4 pos = u_ViewProjection * vec4(a_Position, 1.0);
    
    gl_Position = pos.xyww;
}

#type fragment
#version 450 core

layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec4 o_SSR;
layout(location = 2) out vec4 o_Normal;

in vec3 v_TexCoord;

uniform samplerCube u_Skybox;

void main()
{
    FragColor = min(texture(u_Skybox, v_TexCoord), vec4(1000.0));
    o_SSR = vec4(0.0);
    o_Normal = vec4(0.5, 0.5, 1.0, 1.0);
}