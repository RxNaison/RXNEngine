#type vertex
#version 450 core

layout(location = 0) in vec2 a_Position;
layout(location = 1) in vec2 a_TexCoord;

out vec2 v_TexCoord;

void main()
{
    v_TexCoord = a_TexCoord;
    gl_Position = vec4(a_Position.x, a_Position.y, 0.0, 1.0);
}

#type fragment
#version 450 core


layout(location = 0) out float o_BlurredAO;

in vec2 v_TexCoord;

uniform sampler2D u_AOTexture;
uniform sampler2D u_DepthTexture;
uniform sampler2D u_NormalTexture;
uniform mat4  u_InvProj;
uniform vec2  u_TexelSize;

float ViewZ(vec2 uv)
{
    float d = texture(u_DepthTexture, uv).r;
    vec4 ndc = vec4(uv * 2.0 - 1.0, d * 2.0 - 1.0, 1.0);
    vec4 v = u_InvProj * ndc;
    return v.z / v.w;
}

vec3 GetN(vec2 uv)
{
    return normalize(texture(u_NormalTexture, uv).xyz * 2.0 - 1.0);
}

void main()
{
    float centerDepth = texture(u_DepthTexture, v_TexCoord).r;
    if (centerDepth >= 0.9999) { o_BlurredAO = 1.0; return; }

    float centerZ = ViewZ(v_TexCoord);
    vec3  centerN = GetN(v_TexCoord);

    float totalAO = 0.0;
    float totalW  = 0.0;

    const int R = 5;
    for (int x = -R; x <= R; ++x)
    for (int y = -R; y <= R; ++y)
    {
        vec2 uv = v_TexCoord + vec2(x, y) * u_TexelSize;
        if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) continue;

        float ao = texture(u_AOTexture, uv).r;

        float ws = exp(-float(x * x + y * y) * 0.07);
        float dz = abs(ViewZ(uv) - centerZ) / max(abs(centerZ), 1e-3);
        float wd = exp(-dz * dz * 80.0);
        float nd = max(dot(GetN(uv), centerN), 0.0);
        float wn = pow(nd, 4.0);

        float w = ws * wd * wn;
        totalAO += ao * w;
        totalW  += w;
    }

    o_BlurredAO = (totalW > 1e-4) ? (totalAO / totalW) : texture(u_AOTexture, v_TexCoord).r;
}
