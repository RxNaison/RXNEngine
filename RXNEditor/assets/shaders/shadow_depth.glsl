#type vertex
#version 450 core
layout (location = 0) in vec3 a_Position;

// INSTANCING
layout(location = 4) in vec4 a_ModelRow0;
layout(location = 5) in vec4 a_ModelRow1;
layout(location = 6) in vec4 a_ModelRow2;
layout(location = 7) in vec4 a_ModelRow3;

void main()
{
    mat4 model = mat4(a_ModelRow0, a_ModelRow1, a_ModelRow2, a_ModelRow3);
    gl_Position = model * vec4(a_Position, 1.0);
}

#type geometry
#version 450 core
layout(triangles) in;
layout(triangle_strip, max_vertices = 12) out; // 3 verts * 4 cascades

uniform mat4 u_LightSpaceMatrices[4];

void main()
{
    for(int layer = 0; layer < 4; ++layer)
    {
        gl_Layer = layer; // Built-in variable to select Texture Array Layer
        for(int i = 0; i < 3; ++i)
        {
            gl_Position = u_LightSpaceMatrices[layer] * gl_in[i].gl_Position;
            EmitVertex();
        }
        EndPrimitive();
    }
}

#type fragment
#version 450 core
void main()
{
}