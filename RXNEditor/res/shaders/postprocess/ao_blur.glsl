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

uniform sampler2D u_AOTexture;      // Quarter-res raw HBAO+ input
uniform sampler2D u_DepthTexture;   // Full-res depth buffer
uniform mat4 u_InverseViewProjection;
uniform vec2 u_TexelSize;           // Quarter-res texel size (1 / 270p or 1 / 480p)

float GetLinearDepth(vec2 uv, float depth)
{
    vec4 ndc = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 wp = u_InverseViewProjection * ndc;
    return abs(1.0 / wp.w);
}

void main()
{
    float centerAO = texture(u_AOTexture, v_TexCoord).r;
    float centerDepth = texture(u_DepthTexture, v_TexCoord).r;

    if (centerDepth >= 0.9999)
    {
        o_BlurredAO = 1.0;
        return;
    }

    float centerLinearDepth = GetLinearDepth(v_TexCoord, centerDepth);

    float totalAO = 0.0;
    float totalWeight = 0.0;

    const int blurSize = 2;
    const float depthThreshold = 1.0;

    for (int x = -blurSize; x <= blurSize; ++x)
    {
        for (int y = -blurSize; y <= blurSize; ++y)
        {
            vec2 offset = vec2(x, y) * u_TexelSize;
            vec2 sampleUV = v_TexCoord + offset;

            if (sampleUV.x < 0.0 || sampleUV.x > 1.0 || sampleUV.y < 0.0 || sampleUV.y > 1.0)
                continue;

            float sampleAO = texture(u_AOTexture, sampleUV).r;
            float sampleDepth = texture(u_DepthTexture, sampleUV).r;
            float sampleLinearDepth = GetLinearDepth(sampleUV, sampleDepth);

            float spatialDistSqr = float(x * x + y * y);
            float spatialWeight = exp(-spatialDistSqr * 0.15);

            float depthDiff = abs(sampleLinearDepth - centerLinearDepth);
            float rangeWeight = exp(-depthDiff * 4.0); // sharp cut-off to preserve edges

            float weight = spatialWeight * rangeWeight;

            totalAO += sampleAO * weight;
            totalWeight += weight;
        }
    }

    if (totalWeight > 0.0001)
    {
        o_BlurredAO = totalAO / totalWeight;
    }
    else
    {
        o_BlurredAO = centerAO;
    }
}
