#type vertex
#version 450 core
layout(location = 0) in vec3 a_Position;

uniform mat4 u_ViewProjection;
uniform vec3 u_CameraPos;

out vec3 v_FragPos;

void main()
{
    vec3 pos = a_Position * 1000.0 + vec3(u_CameraPos.x, 0.0, u_CameraPos.z);
    v_FragPos = pos;
    gl_Position = u_ViewProjection * vec4(pos, 1.0);
}

#type fragment
#version 450 core
layout(location = 0) out vec4 o_Color;

in vec3 v_FragPos;

uniform vec3      u_CameraPos;
uniform sampler2D u_SceneDepth;
uniform vec2      u_ScreenSize;

float gridCoverage(vec2 ws, float cell, float lineWidthPx)
{
    vec2 uv    = ws / cell;
    vec2 deriv = fwidth(uv);
    vec2 dist  = abs(fract(uv - 0.5) - 0.5) / max(deriv, vec2(1e-8));
    float d    = min(dist.x, dist.y);
    return 1.0 - smoothstep(0.0, lineWidthPx, d);
}

float axisCoverage(float worldCoord, float worldPerPixel, float widthPx)
{
    return 1.0 - smoothstep(0.0, widthPx * worldPerPixel, abs(worldCoord));
}

void main()
{
    vec2 screenUV = gl_FragCoord.xy / u_ScreenSize;
    if (texture(u_SceneDepth, screenUV).r < gl_FragCoord.z)
        discard;

    vec2  ws         = v_FragPos.xz;
    vec2  deriv      = fwidth(ws);
    float worldPerPx = max(deriv.x, deriv.y);

    float minor = gridCoverage(ws,  1.0, 1.0);
    float major = gridCoverage(ws, 10.0, 1.5);

    float minorFade = 1.0 - smoothstep(0.30, 1.20, worldPerPx);
    float majorFade = 1.0 - smoothstep(3.00, 12.0, worldPerPx);
    minor *= minorFade;
    major *= majorFade;

    float dist     = length(v_FragPos - u_CameraPos);
    float distFade = 1.0 - smoothstep(120.0, 320.0, dist);

    vec3  minorColor   = vec3(0.8);
    vec3  majorColor   = vec3(0.85);
    float minorOpacity = 0.4;
    float majorOpacity = 0.6;

    vec3  color = minorColor;
    float alpha = minor * minorOpacity;
    color = mix(color, majorColor, major);
    alpha = max(alpha, major * majorOpacity);

    float axisX = axisCoverage(v_FragPos.z, deriv.y, 1.6) * majorFade;
    float axisZ = axisCoverage(v_FragPos.x, deriv.x, 1.6) * majorFade;
    color = mix(color, vec3(0.85, 0.27, 0.27), axisX);
    alpha = max(alpha, axisX * 0.65);
    color = mix(color, vec3(0.30, 0.47, 0.88), axisZ);
    alpha = max(alpha, axisZ * 0.65);

    alpha *= distFade;
    if (alpha <= 0.001)
        discard;

    o_Color = vec4(color, alpha);
}