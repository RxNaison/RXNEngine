#pragma once
#include "RXNEngine/Core/Base.h"

namespace RXNEngine {

    class Subsystem
    {
    public:
        virtual ~Subsystem() = default;

        virtual void Init() {}
        virtual void Update(float deltaTime) {}
        virtual void Shutdown() {}
    };

}