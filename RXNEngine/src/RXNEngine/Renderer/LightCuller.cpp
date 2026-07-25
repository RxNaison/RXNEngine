#include "rxnpch.h"
#include "LightCuller.h"
#include "RXNEngine/Renderer/RenderCommand.h"

#include <algorithm>
#include <cmath>

namespace RXNEngine {

    void LightCuller::Init()
    {
        m_ClusterAABBShader = Shader::Create("res/shaders/cluster_aabb.glsl");
        m_LightCullShader   = Shader::Create("res/shaders/light_culling.glsl");

        m_ClusterAABBBuffer = ShaderStorageBuffer::Create(nullptr, NUM_CLUSTERS * 2u * (uint32_t)sizeof(glm::vec4));

        m_PointGridBuffer = ShaderStorageBuffer::Create(nullptr, NUM_CLUSTERS * 2u * (uint32_t)sizeof(uint32_t));
        m_SpotGridBuffer  = ShaderStorageBuffer::Create(nullptr, NUM_CLUSTERS * 2u * (uint32_t)sizeof(uint32_t));

        m_PointIndexBuffer = ShaderStorageBuffer::Create(nullptr, NUM_CLUSTERS * MAX_POINT_PER_CLUSTER * (uint32_t)sizeof(uint32_t));
        m_SpotIndexBuffer  = ShaderStorageBuffer::Create(nullptr, NUM_CLUSTERS * MAX_SPOT_PER_CLUSTER  * (uint32_t)sizeof(uint32_t));

        m_ClusterUBO = UniformBuffer::Create((uint32_t)sizeof(ClusterDataGPU), 4);
    }

    void LightCuller::EnsureLightCapacity(Ref<ShaderStorageBuffer>& buffer, uint32_t& capacityBytes, uint32_t requiredBytes)
    {
        requiredBytes = std::max(requiredBytes, 256u);
        if (!buffer || capacityBytes < requiredBytes)
        {
            capacityBytes = requiredBytes + requiredBytes / 2u;
            buffer = ShaderStorageBuffer::Create(nullptr, capacityBytes);
        }
    }

    void LightCuller::BuildAndCull(const glm::mat4& viewProjection,
                                   const glm::mat4& viewMatrix,
                                   uint32_t screenWidth,
                                   uint32_t screenHeight,
                                   const void* pointLights, uint32_t pointCount, uint32_t pointStride,
                                   const void* spotLights, uint32_t spotCount, uint32_t spotStride)
    {
        if (screenWidth == 0 || screenHeight == 0)
            return;

        glm::mat4 projection = viewProjection * glm::inverse(viewMatrix);
        glm::mat4 invProjection = glm::inverse(projection);

        float A = projection[2][2];
        float B = projection[3][2];
        float zNear = B / (A - 1.0f);
        float zFar  = B / (A + 1.0f);
        zNear = std::abs(zNear);
        zFar  = std::abs(zFar);
        if (!(zFar > zNear) || zNear < 1e-4f)
        {
            zNear = 0.1f;
            zFar  = 1000.0f;
        }

        EnsureLightCapacity(m_PointLightBuffer, m_PointCapacityBytes, pointCount * pointStride);
        EnsureLightCapacity(m_SpotLightBuffer,  m_SpotCapacityBytes,  spotCount  * spotStride);
        if (pointCount > 0 && pointLights)
            m_PointLightBuffer->SetData(pointLights, pointCount * pointStride);
        if (spotCount > 0 && spotLights)
            m_SpotLightBuffer->SetData(spotLights, spotCount * spotStride);

        float tilePxX = (float)screenWidth  / (float)CLUSTER_X;
        float tilePxY = (float)screenHeight / (float)CLUSTER_Y;
        float logFarNear = std::log(zFar / zNear);
        float scale = (float)CLUSTER_Z / logFarNear;
        float bias  = -((float)CLUSTER_Z * std::log(zNear) / logFarNear);

        ClusterDataGPU data;
        data.InvProjection  = invProjection;
        data.ClusterView    = viewMatrix;
        data.GridSizeTileX  = glm::vec4((float)CLUSTER_X, (float)CLUSTER_Y, (float)CLUSTER_Z, tilePxX);
        data.ScreenNearFar  = glm::vec4((float)screenWidth, (float)screenHeight, zNear, zFar);
        data.ScaleBiasTileY = glm::vec4(scale, bias, tilePxY, (float)NUM_CLUSTERS);
        data.LightCounts    = glm::uvec4(pointCount, spotCount, 0u, 0u);
        m_ClusterUBO->SetData(&data, (uint32_t)sizeof(ClusterDataGPU));
        m_ClusterUBO->Bind();

        m_ClusterAABBShader->Bind();
        m_ClusterAABBBuffer->Bind(0);
        RenderCommand::DispatchCompute(CLUSTER_X, CLUSTER_Y, CLUSTER_Z);
        RenderCommand::MemoryBarrier(RendererAPI::BarrierType::ShaderStorage);

        m_LightCullShader->Bind();
        m_ClusterAABBBuffer->Bind(0);
        m_PointLightBuffer->Bind(1);
        m_SpotLightBuffer->Bind(2);
        m_PointGridBuffer->Bind(3);
        m_SpotGridBuffer->Bind(4);
        m_PointIndexBuffer->Bind(5);
        m_SpotIndexBuffer->Bind(6);
        RenderCommand::DispatchCompute(CLUSTER_X, CLUSTER_Y, CLUSTER_Z);
        RenderCommand::MemoryBarrier(RendererAPI::BarrierType::ShaderStorage);
    }

    void LightCuller::BindForShading()
    {
        m_ClusterUBO->Bind();
        m_PointLightBuffer->Bind(1);
        m_SpotLightBuffer->Bind(2);
        m_PointGridBuffer->Bind(3);
        m_SpotGridBuffer->Bind(4);
        m_PointIndexBuffer->Bind(5);
        m_SpotIndexBuffer->Bind(6);
    }

}
