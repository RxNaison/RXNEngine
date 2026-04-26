#pragma once

#include "RXNEngine/Core/Base.h"

namespace RXNEngine {

    class ShadowMap
    {
    public:
        virtual ~ShadowMap() = default;

        virtual void Init(uint32_t size, uint32_t layers = 4, bool isCubemap = false) = 0;
        virtual void BindWriteLayer(uint32_t layer, uint32_t face = 0) = 0;
        virtual void BindRead(uint32_t slot) = 0;

        virtual uint32_t GetSize() const = 0;
        virtual uint32_t GetRendererID() const = 0;

        static Ref<ShadowMap> Create();
    };

}