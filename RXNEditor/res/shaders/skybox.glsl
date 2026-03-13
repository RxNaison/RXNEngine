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

out vec4 FragColor;

in vec3 v_TexCoord;

uniform samplerCube u_Skybox;

void main()
{
    FragColor = texture(u_Skybox, v_TexCoord);
}