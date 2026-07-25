#type compute
#version 450 core


layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

const uint MAX_POINT_PER_CLUSTER = 64u;
const uint MAX_SPOT_PER_CLUSTER  = 32u;

struct ClusterAABB
{
    vec4 MinPoint;
    vec4 MaxPoint;
};

struct PointLight
{
    vec4 PositionIntensity;
    vec4 ColorRadius;
    vec4 Falloff;
    vec4 ExtraData;
};

struct SpotLight
{
    vec4 PositionIntensity;
    vec4 DirectionRadius;
    vec4 ColorCutoff;
    vec4 Falloff;
    mat4 LightSpaceMatrix;
    vec4 ExtraData;
};

layout(std430, binding = 0) readonly  buffer ClusterAABBBuffer { ClusterAABB b_Clusters[]; };
layout(std430, binding = 1) readonly  buffer PointLightBuffer  { PointLight  b_PointLights[]; };
layout(std430, binding = 2) readonly  buffer SpotLightBuffer   { SpotLight   b_SpotLights[]; };
layout(std430, binding = 3) writeonly buffer PointGridBuffer   { uvec2 b_PointGrid[]; };
layout(std430, binding = 4) writeonly buffer SpotGridBuffer    { uvec2 b_SpotGrid[]; };
layout(std430, binding = 5) writeonly buffer PointIndexBuffer  { uint  b_PointIndices[]; };
layout(std430, binding = 6) writeonly buffer SpotIndexBuffer   { uint  b_SpotIndices[]; };

layout(std140, binding = 4) uniform ClusterData
{
    mat4  u_InvProjection;
    mat4  u_ClusterView;
    vec4  u_GridSizeTileX;
    vec4  u_ScreenNearFar;
    vec4  u_ScaleBiasTileY;
    uvec4 u_LightCounts;
};

bool SphereIntersectsAABB(vec3 center, float radius, ClusterAABB box)
{
    float sqDist = 0.0;
    for (int i = 0; i < 3; ++i)
    {
        float v = center[i];
        if (v < box.MinPoint[i]) { float d = box.MinPoint[i] - v; sqDist += d * d; }
        if (v > box.MaxPoint[i]) { float d = v - box.MaxPoint[i]; sqDist += d * d; }
    }
    return sqDist <= radius * radius;
}

void main()
{
    uint gridX = uint(u_GridSizeTileX.x);
    uint gridY = uint(u_GridSizeTileX.y);
    uint gridZ = uint(u_GridSizeTileX.z);

    uvec3 c = gl_GlobalInvocationID;
    if (c.x >= gridX || c.y >= gridY || c.z >= gridZ)
        return;

    uint clusterIndex = c.x + c.y * gridX + c.z * gridX * gridY;
    ClusterAABB box = b_Clusters[clusterIndex];

    uint pointCount = u_LightCounts.x;
    uint spotCount  = u_LightCounts.y;

    uint pOffset = clusterIndex * MAX_POINT_PER_CLUSTER;
    uint pVisible = 0u;
    for (uint i = 0u; i < pointCount && pVisible < MAX_POINT_PER_CLUSTER; ++i)
    {
        vec3 posVS = (u_ClusterView * vec4(b_PointLights[i].PositionIntensity.xyz, 1.0)).xyz;
        float radius = b_PointLights[i].ColorRadius.w;
        if (SphereIntersectsAABB(posVS, radius, box))
        {
            b_PointIndices[pOffset + pVisible] = i;
            pVisible++;
        }
    }
    b_PointGrid[clusterIndex] = uvec2(pOffset, pVisible);

    uint sOffset = clusterIndex * MAX_SPOT_PER_CLUSTER;
    uint sVisible = 0u;
    for (uint i = 0u; i < spotCount && sVisible < MAX_SPOT_PER_CLUSTER; ++i)
    {
        vec3 posVS = (u_ClusterView * vec4(b_SpotLights[i].PositionIntensity.xyz, 1.0)).xyz;
        float radius = b_SpotLights[i].DirectionRadius.w;
        if (SphereIntersectsAABB(posVS, radius, box))
        {
            b_SpotIndices[sOffset + sVisible] = i;
            sVisible++;
        }
    }
    b_SpotGrid[clusterIndex] = uvec2(sOffset, sVisible);
}
