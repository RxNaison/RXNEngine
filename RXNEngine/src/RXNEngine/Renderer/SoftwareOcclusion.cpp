#include "rxnpch.h"
#include "SoftwareOcclusion.h"

#include <cfloat>
#include <cstring>

namespace RXNEngine {

	namespace {
		constexpr float kNearW = 0.1f;
	}

	void SoftwareOcclusionBuffer::BeginFrame(const glm::mat4& viewProjection)
	{
		if (m_Depth.size() != (size_t)kWidth * kHeight)
			m_Depth.resize((size_t)kWidth * kHeight);

		std::fill(m_Depth.begin(), m_Depth.end(), FLT_MAX);
		m_ViewProjection = viewProjection;
		m_TriangleCount = 0;
		m_Valid = true;
	}

	void SoftwareOcclusionBuffer::RasterizeOccluder(const glm::mat4& transform,
		const void* positions, size_t stride,
		const uint32_t* indices, uint32_t indexCount)
	{
		if (!m_Valid || !positions || !indices || indexCount < 3)
			return;

		const glm::mat4 mvp = m_ViewProjection * transform;
		const uint8_t* base = (const uint8_t*)positions;

		auto fetchClip = [&](uint32_t index) -> glm::vec4
			{
				glm::vec3 p;
				std::memcpy(&p, base + (size_t)index * stride, sizeof(glm::vec3));
				return mvp * glm::vec4(p, 1.0f);
			};

		for (uint32_t i = 0; i + 2 < indexCount; i += 3)
		{
			glm::vec4 c[3] = { fetchClip(indices[i]), fetchClip(indices[i + 1]), fetchClip(indices[i + 2]) };

			if (c[0].w <= kNearW && c[1].w <= kNearW && c[2].w <= kNearW)
				continue;

			if (c[0].w > kNearW && c[1].w > kNearW && c[2].w > kNearW)
			{
				RasterizeClippedTriangle(c[0], c[1], c[2]);
				continue;
			}

			glm::vec4 poly[4];
			int n = 0;
			for (int v = 0; v < 3; v++)
			{
				const glm::vec4& a = c[v];
				const glm::vec4& b = c[(v + 1) % 3];
				bool aIn = a.w > kNearW;
				bool bIn = b.w > kNearW;

				if (aIn)
					poly[n++] = a;

				if (aIn != bIn)
				{
					float t = (kNearW - a.w) / (b.w - a.w);
					poly[n++] = a + (b - a) * t;
				}
			}

			for (int k = 2; k < n; k++)
				RasterizeClippedTriangle(poly[0], poly[k - 1], poly[k]);
		}
	}

	void SoftwareOcclusionBuffer::RasterizeClippedTriangle(const glm::vec4& c0, const glm::vec4& c1, const glm::vec4& c2)
	{
		glm::vec3 p[3];
		const glm::vec4* c[3] = { &c0, &c1, &c2 };
		for (int i = 0; i < 3; i++)
		{
			float invW = 1.0f / c[i]->w;
			p[i].x = (c[i]->x * invW * 0.5f + 0.5f) * (float)kWidth;
			p[i].y = (c[i]->y * invW * 0.5f + 0.5f) * (float)kHeight;
			p[i].z = invW;
		}

		float area = (p[1].x - p[0].x) * (p[2].y - p[0].y) - (p[1].y - p[0].y) * (p[2].x - p[0].x);
		if (glm::abs(area) < 1e-8f)
			return;

		if (area < 0.0f)
		{
			std::swap(p[1], p[2]);
			area = -area;
		}

		int minX = glm::max((int)glm::floor(glm::min(p[0].x, glm::min(p[1].x, p[2].x))), 0);
		int maxX = glm::min((int)glm::ceil(glm::max(p[0].x, glm::max(p[1].x, p[2].x))), (int)kWidth - 1);
		int minY = glm::max((int)glm::floor(glm::min(p[0].y, glm::min(p[1].y, p[2].y))), 0);
		int maxY = glm::min((int)glm::ceil(glm::max(p[0].y, glm::max(p[1].y, p[2].y))), (int)kHeight - 1);

		if (minX > maxX || minY > maxY)
			return;

		const float invArea = 1.0f / area;

		const float e0dx = p[1].y - p[2].y, e0dy = p[2].x - p[1].x;
		const float e1dx = p[2].y - p[0].y, e1dy = p[0].x - p[2].x;
		const float e2dx = p[0].y - p[1].y, e2dy = p[1].x - p[0].x;

		float px = (float)minX + 0.5f;
		float py = (float)minY + 0.5f;

		float row0 = (px - p[1].x) * e0dx + (py - p[1].y) * e0dy;
		float row1 = (px - p[2].x) * e1dx + (py - p[2].y) * e1dy;
		float row2 = (px - p[0].x) * e2dx + (py - p[0].y) * e2dy;

		for (int y = minY; y <= maxY; y++)
		{
			float w0 = row0, w1 = row1, w2 = row2;
			float* depthRow = &m_Depth[(size_t)y * kWidth];

			for (int x = minX; x <= maxX; x++)
			{
				if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f)
				{
					float invW = (w0 * p[0].z + w1 * p[1].z + w2 * p[2].z) * invArea;
					float depth = 1.0f / glm::max(invW, 1e-9f);
					if (depth < depthRow[x])
						depthRow[x] = depth;
				}
				w0 += e0dx; w1 += e1dx; w2 += e2dx;
			}
			row0 += e0dy; row1 += e1dy; row2 += e2dy;
		}

		m_TriangleCount++;
	}

	bool SoftwareOcclusionBuffer::IsRectOccluded(const glm::vec2& uvMin, const glm::vec2& uvMax, float nearestDepth) const
	{
		if (!m_Valid)
			return false;

		const int guard = 1;
		int x0 = glm::max((int)(uvMin.x * (float)(kWidth - 1)) - guard, 0);
		int x1 = glm::min((int)(uvMax.x * (float)(kWidth - 1) + 0.5f) + guard, (int)kWidth - 1);
		int y0 = glm::max((int)(uvMin.y * (float)(kHeight - 1)) - guard, 0);
		int y1 = glm::min((int)(uvMax.y * (float)(kHeight - 1) + 0.5f) + guard, (int)kHeight - 1);

		float farthestOccluder = 0.0f;
		for (int y = y0; y <= y1; y++)
		{
			const float* depthRow = &m_Depth[(size_t)y * kWidth];
			for (int x = x0; x <= x1; x++)
			{
				float d = depthRow[x];
				if (d == FLT_MAX)
					return false;
				farthestOccluder = glm::max(farthestOccluder, d);
			}
		}

		float margin = 0.05f + nearestDepth * 0.005f;
		return nearestDepth > farthestOccluder + margin;
	}

}
