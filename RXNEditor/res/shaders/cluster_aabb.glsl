#type compute
#version 450 core


layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

struct ClusterAABB
{
    vec4 MinPoint;
    vec4 MaxPoint;
};

layout(std430, binding = 0) writeonly buffer ClusterAABBBuffer
{
    ClusterAABB b_Clusters[];
};

layout(std140, binding = 4) uniform ClusterData
{
    mat4  u_InvProjection;
    mat4  u_ClusterView;
    vec4  u_GridSizeTileX;
    vec4  u_ScreenNearFar;
    vec4  u_ScaleBiasTileY;
    uvec4 u_LightCounts;
};

vec3 ScreenToView(vec2 screenPx)
{
    vec2 uv = screenPx / u_ScreenNearFar.xy;
    vec4 clip = vec4(uv * 2.0 - 1.0, -1.0, 1.0);
    vec4 view = u_InvProjection * clip;
    return view.xyz / view.w;
}

vec3 LineIntersectToZ(vec3 dir, float zDist)
{
    float t = zDist / dir.z;
    return dir * t;
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

    float tilePxX = u_GridSizeTileX.w;
    float tilePxY = u_ScaleBiasTileY.z;

    vec2 minSS = vec2(float(c.x)      * tilePxX, float(c.y)      * tilePxY);
    vec2 maxSS = vec2(float(c.x + 1u) * tilePxX, float(c.y + 1u) * tilePxY);

    vec3 minView = ScreenToView(minSS);
    vec3 maxView = ScreenToView(maxSS);

    float zNear = u_ScreenNearFar.z;
    float zFar  = u_ScreenNearFar.w;

    float tileNear = -zNear * pow(zFar / zNear, float(c.z)      / float(gridZ));
    float tileFar  = -zNear * pow(zFar / zNear, float(c.z + 1u) / float(gridZ));

    vec3 minNear = LineIntersectToZ(minView, tileNear);
    vec3 minFar  = LineIntersectToZ(minView, tileFar);
    vec3 maxNear = LineIntersectToZ(maxView, tileNear);
    vec3 maxFar  = LineIntersectToZ(maxView, tileFar);

    vec3 aabbMin = min(min(minNear, minFar), min(maxNear, maxFar));
    vec3 aabbMax = max(max(minNear, minFar), max(maxNear, maxFar));

    b_Clusters[clusterIndex].MinPoint = vec4(aabbMin, 0.0);
    b_Clusters[clusterIndex].MaxPoint = vec4(aabbMax, 0.0);
}
