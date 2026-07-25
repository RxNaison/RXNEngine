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

// =============================================================================
// Ground-Truth Ambient Occlusion (GTAO) - view space, single frame, NO TAA.
//
// Robust at grazing angles because it uses the TRUE surface normal (written by
// the forward pass) together with a physically-based horizon-visibility
// integral, instead of depth-reconstructed normals + an NdotV bias hack.
//
// Noise is removed spatially (Interleaved Gradient Noise + a normal-aware
// bilateral pass), so there is zero temporal dependency / ghosting.
//
// Reference: Jimenez et al. 2016, "Practical Realtime Strategies for Accurate
// Indirect Occlusion" (Activision).
// =============================================================================

layout(location = 0) out float o_AO;

in vec2 v_TexCoord;

uniform sampler2D u_DepthTexture;    // full-res scene depth (non-linear)
uniform sampler2D u_NormalTexture;   // WORLD-space normal, encoded *0.5+0.5
uniform mat4  u_View;                 // world -> view
uniform mat4  u_Proj;                 // camera projection
uniform mat4  u_InvProj;              // inverse projection (NDC -> view)
uniform vec2  u_AOTexelSize;          // 1/aoW, 1/aoH
uniform vec2  u_Resolution;          // AO render resolution (pixels)
uniform float u_Radius;               // world-space sampling radius (meters)
uniform float u_Intensity;            // AO power curve exponent
uniform float u_FalloffMul;           // thickness / falloff multiplier

const float PI      = 3.14159265359;
const float HALF_PI = 1.57079632679;

vec3 GetViewPos(vec2 uv)
{
    float d = texture(u_DepthTexture, uv).r;
    vec4 ndc = vec4(uv * 2.0 - 1.0, d * 2.0 - 1.0, 1.0);
    vec4 v = u_InvProj * ndc;
    return v.xyz / v.w;
}

vec3 GetViewNormal(vec2 uv)
{
    vec3 nWorld = texture(u_NormalTexture, uv).xyz * 2.0 - 1.0;
    return normalize(mat3(u_View) * nWorld);
}

// Per-pixel hash -> DECORRELATED dither. This is the key to a clean no-TAA result:
// neighbouring pixels get independent sample sets, so the bilateral pass averages
// NOISE (which converges to the true value) instead of BANDING (which cannot be
// blurred away no matter how wide the kernel).
float Hash21(vec2 p)
{
    p = fract(p * vec2(123.34, 345.45));
    p += dot(p, p + 34.345);
    return fract(p.x * p.y);
}

void main()
{
    float centerDepth = texture(u_DepthTexture, v_TexCoord).r;
    if (centerDepth >= 0.9999) { o_AO = 1.0; return; }

    vec3 P = GetViewPos(v_TexCoord);     // view-space position (-Z forward)

    // WP18: distance fade. Far away, the whole world-space radius projects to
    // a couple of half-res texels, so the horizon march just resamples the
    // same depth values for zero visual gain. Fade AO out across a band and
    // skip the march entirely beyond it.
    const float kFadeStart = 80.0;   // meters: full-strength AO up to here
    const float kFadeEnd   = 120.0;  // meters: AO fully gone
    float distFade = 1.0 - smoothstep(kFadeStart, kFadeEnd, -P.z);
    if (distFade <= 0.0) { o_AO = 1.0; return; }

    vec3 N = GetViewNormal(v_TexCoord);  // view-space normal
    vec3 V = normalize(-P);              // toward the camera

    // World radius -> screen radius (pixels). Capped so the kernel can never
    // flood the screen at point-blank range.
    float pixelRadius = u_Radius * (u_Proj[1][1] * 0.5 * u_Resolution.y) / max(-P.z, 1e-4);
    pixelRadius = clamp(pixelRadius, 2.0, 0.30 * u_Resolution.y);

    const int SLICES = 3;    // angular slices (WP18: was 4 — the decorrelated
                             // dither + bilateral blur absorb the extra noise)
    const int STEPS  = 8;    // MAX horizon march steps per slice (WP18: was 10)

    // WP18: footprint-adaptive march. When the kernel projects to only a few
    // pixels there is nothing new to sample between steps — scale the step
    // count with the on-screen radius instead of always marching STEPS times.
    int steps = int(clamp(pixelRadius * 0.5, 3.0, float(STEPS)));

    vec2  fragPx      = floor(v_TexCoord * u_Resolution);
    float sliceJitter = Hash21(fragPx);            // rotates the whole slice fan per pixel
    float stepJitter  = Hash21(fragPx + 17.123);   // independent march offset

    float thickness  = max(u_Radius * u_FalloffMul, 1e-4);
    float visibility = 0.0;

    for (int slice = 0; slice < SLICES; ++slice)
    {
        float phi = (float(slice) + sliceJitter) * (PI / float(SLICES));
        vec2  dir = vec2(cos(phi), sin(phi));

        // Decorrelate the march offset per slice too (golden-ratio sequence),
        // so no two slices share a step pattern.
        float sj = fract(stepJitter + float(slice) * 0.61803398875);

        // View-space direction of this screen slice (used to build the plane).
        vec3 dirView = GetViewPos(v_TexCoord + dir * u_AOTexelSize * 2.0) - P;
        dirView = dirView - V * dot(dirView, V); // project perpendicular to V
        float dvl = length(dirView);
        if (dvl < 1e-5) continue;
        dirView /= dvl;

        vec3 planeNormal = normalize(cross(dirView, V));
        vec3 projN = N - planeNormal * dot(N, planeNormal);
        float projNLen = length(projN);
        if (projNLen < 1e-4) continue;
        projN /= projNLen;

        // Signed angle of projected normal from V (+ toward +dir side).
        float n = atan(dot(projN, dirView), dot(projN, V));

        // Horizon search both sides: track max cos(angle-from-V).
        float cosH1 = -1.0; // -dir side
        float cosH2 = -1.0; // +dir side

        for (int s = 1; s <= steps; ++s)
        {
            float t = (float(s) - sj) / float(steps);
            vec2 off = dir * (t * pixelRadius * u_AOTexelSize);

            // +dir
            {
                vec3 D = GetViewPos(v_TexCoord + off) - P;
                float len = length(D);
                float c = dot(D, V) / max(len, 1e-5);
                float falloff = clamp(1.0 - len / u_Radius, 0.0, 1.0);
                falloff *= falloff;            // smooth quadratic fade -> no distance rings
                cosH2 = max(cosH2, mix(-1.0, c, falloff));
            }
            // -dir
            {
                vec3 D = GetViewPos(v_TexCoord - off) - P;
                float len = length(D);
                float c = dot(D, V) / max(len, 1e-5);
                float falloff = clamp(1.0 - len / u_Radius, 0.0, 1.0);
                falloff *= falloff;            // smooth quadratic fade -> no distance rings
                cosH1 = max(cosH1, mix(-1.0, c, falloff));
            }
        }

        // Signed horizon angles relative to V, clamped to the hemisphere
        // around the projected normal.
        float h1 = -acos(clamp(cosH1, -1.0, 1.0));
        float h2 =  acos(clamp(cosH2, -1.0, 1.0));
        h1 = n + max(h1 - n, -HALF_PI);
        h2 = n + min(h2 - n,  HALF_PI);

        // Cosine-weighted inner integral (GTAO analytic form).
        float sinN = sin(n);
        float a = 0.25 * (-cos(2.0 * h1 - n) + cos(n) + 2.0 * h1 * sinN)
                + 0.25 * (-cos(2.0 * h2 - n) + cos(n) + 2.0 * h2 * sinN);

        visibility += projNLen * a;
    }

    visibility /= float(SLICES);
    visibility = clamp(visibility, 0.0, 1.0);
    visibility = pow(visibility, u_Intensity);
    visibility = mix(1.0, visibility, distFade); // WP18: distance fade

    o_AO = visibility;
}
