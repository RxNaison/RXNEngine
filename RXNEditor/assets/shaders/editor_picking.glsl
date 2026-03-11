#type vertex
#version 450 core

layout(location = 0) in vec3 a_Position;

layout(location = 4) in vec4 a_ModelMatrixCol0;
layout(location = 5) in vec4 a_ModelMatrixCol1;
layout(location = 6) in vec4 a_ModelMatrixCol2;
layout(location = 7) in vec4 a_ModelMatrixCol3;
layout(location = 8) in int a_EntityID;

uniform mat4 u_ViewProjection;

flat out int v_EntityID;

void main()
{
    mat4 modelMatrix = mat4(
        a_ModelMatrixCol0, 
        a_ModelMatrixCol1, 
        a_ModelMatrixCol2, 
        a_ModelMatrixCol3
    );
    
    gl_Position = u_ViewProjection * modelMatrix * vec4(a_Position, 1.0);
    
    v_EntityID = a_EntityID;
}


#type fragment
#version 450 core

layout(location = 0) out int o_EntityID;

flat in int v_EntityID;

void main()
{
    o_EntityID = v_EntityID;
}