#type vertex
#version 450 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Color;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in float a_TexIndex;

uniform mat4 u_ViewProjection;
uniform vec2 u_TexSizes[32];

struct VertexOutput
{
	vec4 Color;
	vec2 TexCoord;
};

layout (location = 0) out VertexOutput Output;
layout (location = 2) flat out float v_TexIndex;
layout (location = 3) flat out vec2 v_UnitRange;

void main()
{
	Output.Color = a_Color;
	Output.TexCoord = a_TexCoord;
	v_TexIndex = a_TexIndex;
    
    v_UnitRange = vec2(2.0) / u_TexSizes[int(a_TexIndex)];

	gl_Position = u_ViewProjection * vec4(a_Position, 1.0);
}

#type fragment
#version 450 core

layout(location = 0) out vec4 color;

struct VertexOutput
{
	vec4 Color;
	vec2 TexCoord;
};

layout (location = 0) in VertexOutput Input;
layout (location = 2) flat in float v_TexIndex;
layout (location = 3) flat in vec2 v_UnitRange;

uniform sampler2D u_Textures[32];

float median(float r, float g, float b)
{
    return max(min(r, g), min(max(r, g), b));
}

void main()
{
	vec3 msd = vec3(0.0);

	switch(int(v_TexIndex))
	{
		case 0: msd = texture(u_Textures[0], Input.TexCoord).rgb; break;
		case 1: msd = texture(u_Textures[1], Input.TexCoord).rgb; break;
		case 2: msd = texture(u_Textures[2], Input.TexCoord).rgb; break;
		case 3: msd = texture(u_Textures[3], Input.TexCoord).rgb; break;
		case 4: msd = texture(u_Textures[4], Input.TexCoord).rgb; break;
		case 5: msd = texture(u_Textures[5], Input.TexCoord).rgb; break;
		case 6: msd = texture(u_Textures[6], Input.TexCoord).rgb; break;
		case 7: msd = texture(u_Textures[7], Input.TexCoord).rgb; break;
		case 8: msd = texture(u_Textures[8], Input.TexCoord).rgb; break;
		case 9: msd = texture(u_Textures[9], Input.TexCoord).rgb; break;
		case 10: msd = texture(u_Textures[10], Input.TexCoord).rgb; break;
		case 11: msd = texture(u_Textures[11], Input.TexCoord).rgb; break;
		case 12: msd = texture(u_Textures[12], Input.TexCoord).rgb; break;
		case 13: msd = texture(u_Textures[13], Input.TexCoord).rgb; break;
		case 14: msd = texture(u_Textures[14], Input.TexCoord).rgb; break;
		case 15: msd = texture(u_Textures[15], Input.TexCoord).rgb; break;
		case 16: msd = texture(u_Textures[16], Input.TexCoord).rgb; break;
		case 17: msd = texture(u_Textures[17], Input.TexCoord).rgb; break;
		case 18: msd = texture(u_Textures[18], Input.TexCoord).rgb; break;
		case 19: msd = texture(u_Textures[19], Input.TexCoord).rgb; break;
		case 20: msd = texture(u_Textures[20], Input.TexCoord).rgb; break;
		case 21: msd = texture(u_Textures[21], Input.TexCoord).rgb; break;
		case 22: msd = texture(u_Textures[22], Input.TexCoord).rgb; break;
		case 23: msd = texture(u_Textures[23], Input.TexCoord).rgb; break;
		case 24: msd = texture(u_Textures[24], Input.TexCoord).rgb; break;
		case 25: msd = texture(u_Textures[25], Input.TexCoord).rgb; break;
		case 26: msd = texture(u_Textures[26], Input.TexCoord).rgb; break;
		case 27: msd = texture(u_Textures[27], Input.TexCoord).rgb; break;
		case 28: msd = texture(u_Textures[28], Input.TexCoord).rgb; break;
		case 29: msd = texture(u_Textures[29], Input.TexCoord).rgb; break;
		case 30: msd = texture(u_Textures[30], Input.TexCoord).rgb; break;
		case 31: msd = texture(u_Textures[31], Input.TexCoord).rgb; break;
	}

    float sd = median(msd.r, msd.g, msd.b) - 0.5;

    vec2 dx = dFdx(Input.TexCoord);
    vec2 dy = dFdy(Input.TexCoord);
    float toPixels = 1.0 / length(vec2(length(dx), length(dy)));
    
    float screenPxDistance = max(0.5 * dot(v_UnitRange, vec2(toPixels)), 1.0) * sd;

    float opacity = smoothstep(-0.5, 0.5, screenPxDistance);

    color = vec4(Input.Color.rgb, Input.Color.a * opacity);
    
    if (color.a < 0.001)
        discard;
}