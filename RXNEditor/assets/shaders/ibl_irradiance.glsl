#type vertex
#version 450 core
layout (location = 0) in vec3 a_Pos;

out vec3 v_WorldPos;

uniform mat4 u_View;
uniform mat4 u_Projection;

void main()
{
    v_WorldPos = a_Pos;
    gl_Position = u_Projection * u_View * vec4(v_WorldPos, 1.0);
}

#type fragment
#version 450 core
out vec4 FragColor;
in vec3 v_WorldPos;

uniform samplerCube u_EnvironmentMap;

const float PI = 3.14159265359;

void main()
{		
    vec3 N = normalize(v_WorldPos);
    vec3 irradiance = vec3(0.0);   
    
    // Tangent space calculation
    vec3 up    = vec3(0.0, 1.0, 0.0);
    vec3 right = normalize(cross(up, N));
    up         = normalize(cross(N, right));
       
    float sampleDelta = 0.025;
    float nrSamples = 0.0;

    // Convolution loop
    for(float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta)
    {
        for(float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta)
        {
            // Spherical to Cartesian (in tangent space)
            vec3 tangentSample = vec3(sin(theta) * cos(phi),  sin(theta) * sin(phi), cos(theta));
            
            // Tangent space to World
            vec3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * N; 

            irradiance += texture(u_EnvironmentMap, sampleVec).rgb * cos(theta) * sin(theta);
            nrSamples++;
        }
    }
    irradiance = PI * irradiance * (1.0 / float(nrSamples));
    
    FragColor = vec4(irradiance, 1.0);
}