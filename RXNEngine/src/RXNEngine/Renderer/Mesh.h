#pragma once

#include "VertexArray.h"
#include "RXNEngine/Core/Math.h"

namespace RXNEngine {

    class Mesh
    {
    public:
        Mesh(const Ref<VertexArray>& vertexArray)
            : m_VertexArray(vertexArray) {
        }

        const Ref<VertexArray>& GetVertexArray() const { return m_VertexArray; }

        uint32_t GetIndexCount() const { return m_VertexArray->GetIndexBuffer()->GetCount(); }

        void SetAABB(const AABB& aabb) { m_BoundingBox = aabb; }
        const AABB& GetAABB() const { return m_BoundingBox; }
    private:
        Ref<VertexArray> m_VertexArray;
        AABB m_BoundingBox;
    };
}