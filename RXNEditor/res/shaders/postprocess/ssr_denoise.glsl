#type vertex
#version 450 core
layout(location = 0) in vec2 a_Position;
layout(location = 1) in vec2 a_TexCoord;

out vec2 v_TexCoord;

void main()
{
    v_TexCoord = a_TexCoord;
    gl_Position = vec4(a_Position, 0.0, 1.0);
}

#type fragment
#version 450 core


layout(location = 0) out vec4 o_SSRBlur;

in vec2 v_TexCoord;

uniform sampler2D u_SSRTexture;
uniform sampler2D u_DepthTexture;
uniform mat4  u_InverseViewProjection;
uniform vec2  u_TexelSize;
uniform vec2  u_Direction;

float GetLinearDepth(vec2 uv, float depth)
{
    vec4 p = u_InverseViewProjection * vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    return abs(1.0 / p.w);
}

void main()
{
    vec4  center      = texture(u_SSRTexture, v_TexCoord);
    float centerDepth = texture(u_DepthTexture, v_TexCoord).r;

    if (centerDepth >= 0.9999)
    {
        o_SSRBlur = center;
        return;
    }

    float radius = clamp(center.a, 0.0, 1.0);
    float stepPx = radius * 3.0;

    float centerLinDepth = GetLinearDepth(v_TexCoord, centerDepth);

    vec3  sum  = center.rgb;
    float wsum = 1.0;

    const int RADIUS = 8;
    for (int i = 1; i <= RADIUS; ++i)
    {
        float spatial = exp(-float(i * i) * 0.12);
        for (int s = -1; s <= 1; s += 2)
        {
            vec2 off = u_Direction * u_TexelSize * (float(i) * stepPx * float(s));
            vec2 uv  = v_TexCoord + off;

            float sd = texture(u_DepthTexture, uv).r;
            if (sd >= 0.9999)
                continue;

            float sLin   = GetLinearDepth(uv, sd);
            float depthW = exp(-abs(sLin - centerLinDepth) * 6.0);

            float w = spatial * depthW;
            sum  += texture(u_SSRTexture, uv).rgb * w;
            wsum += w;
        }
    }

    o_SSRBlur = vec4(sum / max(wsum, 1e-4), radius);
}
