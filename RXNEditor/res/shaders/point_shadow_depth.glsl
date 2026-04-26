#type vertex
#version 450 core
layout(location = 0) in vec3 a_Position;
layout(location = 4) in vec4 a_ModelRow0;
layout(location = 5) in vec4 a_ModelRow1;
layout(location = 6) in vec4 a_ModelRow2;
layout(location = 7) in vec4 a_ModelRow3;

uniform mat4 u_LightSpaceMatrix;
out vec4 v_WorldPos;

void main()
{
    mat4 model = mat4(a_ModelRow0, a_ModelRow1, a_ModelRow2, a_ModelRow3);
    v_WorldPos = model * vec4(a_Position, 1.0);
    gl_Position = u_LightSpaceMatrix * v_WorldPos;
}

#type fragment
#version 450 core
in vec4 v_WorldPos;
uniform vec3 u_LightPos;
uniform float u_FarPlane;

void main()
{
    float lightDistance = length(v_WorldPos.xyz - u_LightPos);
    gl_FragDepth = lightDistance / u_FarPlane;
}