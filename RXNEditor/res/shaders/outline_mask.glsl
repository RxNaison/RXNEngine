#type vertex
#version 450 core
layout(location = 0) in vec3 a_Position;

layout(location = 4) in vec4 a_ModelMatrixCol0;
layout(location = 5) in vec4 a_ModelMatrixCol1;
layout(location = 6) in vec4 a_ModelMatrixCol2;
layout(location = 7) in vec4 a_ModelMatrixCol3;

uniform mat4 u_ViewProjection;

void main()
{
    mat4 transform = mat4(a_ModelMatrixCol0, a_ModelMatrixCol1, a_ModelMatrixCol2, a_ModelMatrixCol3);
    gl_Position = u_ViewProjection * transform * vec4(a_Position, 1.0);
}

#type fragment
#version 450 core
layout(location = 0) out vec4 o_Color;

void main()
{
    o_Color = vec4(1.0); 
}