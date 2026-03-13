#type vertex
#version 450 core
layout(location = 0) in vec3 a_Position;

uniform mat4 u_ViewProjection;
uniform vec3 u_CameraPos;

out vec3 v_FragPos;

void main()
{
    vec3 pos = a_Position * 100.0 + vec3(u_CameraPos.x, 0.0, u_CameraPos.z);
    
    v_FragPos = pos;
    gl_Position = u_ViewProjection * vec4(pos, 1.0);
}

#type fragment
#version 450 core
layout(location = 0) out vec4 o_Color;

in vec3 v_FragPos;
uniform vec3 u_CameraPos;

void main()
{
    vec2 coord = v_FragPos.xz;
    
    vec2 derivative = fwidth(coord);
    vec2 grid = abs(fract(coord - 0.5) - 0.5) / derivative;
    float line = min(grid.x, grid.y);
    
    float alpha = 1.0 - min(line, 1.0);
    
    float dist = length(v_FragPos - u_CameraPos);
    float fade = max(0.0, 1.0 - (dist / 50.0));
    
    o_Color = vec4(0.4, 0.4, 0.4, alpha * fade * 0.5);
}