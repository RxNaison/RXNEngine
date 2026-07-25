#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

namespace RXNEngine {

	class SoftwareOcclusionBuffer
	{
	public:
		static constexpr uint32_t kWidth = 256;
		static constexpr uint32_t kHeight = 144;

		void BeginFrame(const glm::mat4& viewProjection);

		void RasterizeOccluder(const glm::mat4& transform,
			const void* positions, size_t stride,
			const uint32_t* indices, uint32_t indexCount);

		bool IsRectOccluded(const glm::vec2& uvMin, const glm::vec2& uvMax, float nearestDepth) const;

		bool IsValid() const { return m_Valid; }
		uint32_t GetRasterizedTriangles() const { return m_TriangleCount; }

	private:
		void RasterizeClippedTriangle(const glm::vec4& c0, const glm::vec4& c1, const glm::vec4& c2);

		std::vector<float> m_Depth;
		glm::mat4 m_ViewProjection = glm::mat4(1.0f);
		uint32_t m_TriangleCount = 0;
		bool m_Valid = false;
	};

}
