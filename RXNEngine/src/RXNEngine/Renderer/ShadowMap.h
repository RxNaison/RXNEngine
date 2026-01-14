#pragma once

#include "RXNEngine/Core/Base.h"

namespace RXNEngine {

	class ShadowMap
	{
	public:
		virtual ~ShadowMap() = default;

		virtual void Init(uint32_t size) = 0;
		virtual void BindWrite() = 0;
		virtual void BindRead(uint32_t slot) = 0;

		virtual uint32_t GetSize() const = 0;
		virtual uint32_t GetRendererID() const = 0;

		static Ref<ShadowMap> Create(uint32_t size);
	};

}