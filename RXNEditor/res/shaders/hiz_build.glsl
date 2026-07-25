#type vertex
#version 450 core

layout(location = 0) in vec2 a_Position;

out vec2 v_UV;

void main()
{
    v_UV = a_Position * 0.5 + 0.5;
    gl_Position = vec4(a_Position, 0.0, 1.0);
}

#type fragment
#version 450 core

layout(location = 0) out vec2 o_Depth;

in vec2 v_UV;

layout(binding = 0) uniform sampler2D u_SrcTexture;

uniform int  u_Mode;
uniform mat4 u_InvViewProjection;
uniform vec2 u_SrcTexelSize;

void main()
{
    if (u_Mode == 0)
    {
        float d = texture(u_SrcTexture, v_UV).r;
        vec4 ndc = vec4(v_UV * 2.0 - 1.0, d * 2.0 - 1.0, 1.0);
        vec4 wp = u_InvViewProjection * ndc;
        float lin = abs(1.0 / wp.w);
        o_Depth = vec2(lin, lin);
    }
    else
    {
        vec2 o = u_SrcTexelSize * 0.5;
        vec2 d0 = textureLod(u_SrcTexture, v_UV + vec2(-o.x, -o.y), 0.0).rg;
        vec2 d1 = textureLod(u_SrcTexture, v_UV + vec2( o.x, -o.y), 0.0).rg;
        vec2 d2 = textureLod(u_SrcTexture, v_UV + vec2(-o.x,  o.y), 0.0).rg;
        vec2 d3 = textureLod(u_SrcTexture, v_UV + vec2( o.x,  o.y), 0.0).rg;
        o_Depth = vec2(
            min(min(d0.r, d1.r), min(d2.r, d3.r)),
            max(max(d0.g, d1.g), max(d2.g, d3.g)));
    }
}
