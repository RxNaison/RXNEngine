#pragma once

#include "RXNEngine/Renderer/GraphicsAPI/Shader.h"
#include "RXNEngine/Renderer/GraphicsAPI/Buffer.h"
#include "RXNEngine/Renderer/GraphicsAPI/UniformBuffer.h"

#include <glm/glm.hpp>

namespace RXNEngine {

    class LightCuller
    {
    public:
        static constexpr uint32_t CLUSTER_X = 16;
        static constexpr uint32_t CLUSTER_Y = 9;
        static constexpr uint32_t CLUSTER_Z = 24;
        static constexpr uint32_t NUM_CLUSTERS = CLUSTER_X * CLUSTER_Y * CLUSTER_Z;

        static constexpr uint32_t MAX_POINT_PER_CLUSTER = 64;
        static constexpr uint32_t MAX_SPOT_PER_CLUSTER = 32;

        void Init();

        void BuildAndCull(const glm::mat4& viewProjection,
                          const glm::mat4& viewMatrix,
                          uint32_t screenWidth,
                          uint32_t screenHeight,
                          const void* pointLights, uint32_t pointCount, uint32_t pointStride,
                          const void* spotLights, uint32_t spotCount, uint32_t spotStride);

        void BindForShading();

    private:
        struct ClusterDataGPU
        {
            glm::mat4  InvProjection;
            glm::mat4  ClusterView;
            glm::vec4  GridSizeTileX;
            glm::vec4  ScreenNearFar;
            glm::vec4  ScaleBiasTileY;
            glm::uvec4 LightCounts;
        };

        void EnsureLightCapacity(Ref<ShaderStorageBuffer>& buffer, uint32_t& capacityBytes, uint32_t requiredBytes);

        Ref<Shader> m_ClusterAABBShader;
        Ref<Shader> m_LightCullShader;

        Ref<ShaderStorageBuffer> m_ClusterAABBBuffer;
        Ref<ShaderStorageBuffer> m_PointLightBuffer;
        Ref<ShaderStorageBuffer> m_SpotLightBuffer;
        Ref<ShaderStorageBuffer> m_PointGridBuffer;
        Ref<ShaderStorageBuffer> m_SpotGridBuffer;
        Ref<ShaderStorageBuffer> m_PointIndexBuffer;
        Ref<ShaderStorageBuffer> m_SpotIndexBuffer;

        Ref<UniformBuffer> m_ClusterUBO;

        uint32_t m_PointCapacityBytes = 0;
        uint32_t m_SpotCapacityBytes = 0;
    };

}
