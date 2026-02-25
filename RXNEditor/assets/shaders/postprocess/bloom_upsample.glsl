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
uniform float u_FilterRadius; // Default ~0.005

void main() {
    float x = u_FilterRadius;
    float y = u_FilterRadius;

    // 9-Tap Tent Filter
    vec3 a = texture(u_Texture, vec2(v_TexCoord.x - x, v_TexCoord.y + y)).rgb;
    vec3 b = texture(u_Texture, vec2(v_TexCoord.x,     v_TexCoord.y + y)).rgb;
    vec3 c = texture(u_Texture, vec2(v_TexCoord.x + x, v_TexCoord.y + y)).rgb;

    vec3 d = texture(u_Texture, vec2(v_TexCoord.x - x, v_TexCoord.y)).rgb;
    vec3 e = texture(u_Texture, vec2(v_TexCoord.x,     v_TexCoord.y)).rgb;
    vec3 f = texture(u_Texture, vec2(v_TexCoord.x + x, v_TexCoord.y)).rgb;

    vec3 g = texture(u_Texture, vec2(v_TexCoord.x - x, v_TexCoord.y - y)).rgb;
    vec3 h = texture(u_Texture, vec2(v_TexCoord.x,     v_TexCoord.y - y)).rgb;
    vec3 i = texture(u_Texture, vec2(v_TexCoord.x + x, v_TexCoord.y - y)).rgb;

    vec3 color = e*0.25;
    color += (b+d+f+h)*0.125;
    color += (a+c+g+i)*0.0625;

    FragColor = vec4(color, 1.0);
}