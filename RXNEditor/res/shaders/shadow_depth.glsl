#type vertex
#version 450 core
layout (location = 0) in vec3 a_Position;

// INSTANCING
layout(location = 4) in vec4 a_ModelRow0;
layout(location = 5) in vec4 a_ModelRow1;
layout(location = 6) in vec4 a_ModelRow2;
layout(location = 7) in vec4 a_ModelRow3;

uniform mat4 u_LightSpaceMatrix;

void main()
{
    mat4 model = mat4(a_ModelRow0, a_ModelRow1, a_ModelRow2, a_ModelRow3);
    gl_Position = u_LightSpaceMatrix * model * vec4(a_Position, 1.0);
}

#type fragment
#version 450 core
void main()
{
}