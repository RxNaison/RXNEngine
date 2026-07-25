#pragma once

#include "RXNEngine/Core/Base.h"

#include <glm/glm.hpp>
#include <cstdint>
#include <vector>

namespace RXNEngine {

	class HiZBuffer
	{
	public:
		virtual ~HiZBuffer() = default;

		virtual void Init(uint32_t width, uint32_t height) = 0;
		virtual void Resize(uint32_t width, uint32_t height) = 0;

		virtual void Build(uint32_t srcDepthTextureId, const glm::mat4& invViewProjection) = 0;

		virtual uint32_t GetTextureID() const = 0;
		virtual uint32_t GetMipLevels() const = 0;

		struct OcclusionGrid
		{
			uint32_t Width = 0;
			uint32_t Height = 0;
			std::vector<glm::vec2> Texels;
			glm::mat4 SourceInvViewProjection = glm::mat4(1.0f);
			bool Valid = false;
		};

		virtual const OcclusionGrid& GetOcclusionGrid() const = 0;

		static Ref<HiZBuffer> Create();
	};

}
