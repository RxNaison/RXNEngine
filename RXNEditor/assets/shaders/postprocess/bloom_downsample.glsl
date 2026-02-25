#type vertex
#version 450 core
layout(location = 0) in vec2 a_Position; 
layout(location = 1) in vec2 a_TexCoord;
out vec2 v_TexCoord;

void main() {
    v_TexCoord = a_TexCoord;
    gl_Position = vec4(a_Position.x, a_Position.y, 0.0, 1.0); 
}

#type fragment
#version 450 core
out vec4 FragColor;
in vec2 v_TexCoord;

uniform sampler2D u_Texture;
uniform vec2 u_TexelSize;
uniform int u_MipLevel;
uniform vec4 u_Threshold; // x = threshold, y = threshold - knee, z = 2.0 * knee, w = 0.25 / knee

vec3 Sample(vec2 uv) {
    return texture(u_Texture, uv).rgb;
}

void main() {
    vec2 texelSize = u_TexelSize;
    float x = texelSize.x;
    float y = texelSize.y;

    // 13-Tap Kawase Downsample
    vec3 a = Sample(vec2(v_TexCoord.x - 2.0*x, v_TexCoord.y + 2.0*y));
    vec3 b = Sample(vec2(v_TexCoord.x,           v_TexCoord.y + 2.0*y));
    vec3 c = Sample(vec2(v_TexCoord.x + 2.0*x, v_TexCoord.y + 2.0*y));

    vec3 d = Sample(vec2(v_TexCoord.x - 2.0*x, v_TexCoord.y));
    vec3 e = Sample(vec2(v_TexCoord.x,           v_TexCoord.y));
    vec3 f = Sample(vec2(v_TexCoord.x + 2.0*x, v_TexCoord.y));

    vec3 g = Sample(vec2(v_TexCoord.x - 2.0*x, v_TexCoord.y - 2.0*y));
    vec3 h = Sample(vec2(v_TexCoord.x,           v_TexCoord.y - 2.0*y));
    vec3 i = Sample(vec2(v_TexCoord.x + 2.0*x, v_TexCoord.y - 2.0*y));

    vec3 j = Sample(vec2(v_TexCoord.x - x, v_TexCoord.y + y));
    vec3 k = Sample(vec2(v_TexCoord.x + x, v_TexCoord.y + y));
    vec3 l = Sample(vec2(v_TexCoord.x - x, v_TexCoord.y - y));
    vec3 m = Sample(vec2(v_TexCoord.x + x, v_TexCoord.y - y));

    vec3 color = e*0.125;
    color += (a+c+g+i)*0.03125;
    color += (b+d+f+h)*0.0625;
    color += (j+k+l+m)*0.125;

    // Prefilter on the first mip
    if (u_MipLevel == 0) {
        float brightness = max(color.r, max(color.g, color.b));
        // Soft knee threshold calculation
        float rq = clamp(brightness - u_Threshold.y, 0.0, u_Threshold.z);
        rq = u_Threshold.w * rq * rq;
        color *= max(rq, brightness - u_Threshold.x) / max(brightness, 0.00001);
    }

    FragColor = vec4(color, 1.0);
}