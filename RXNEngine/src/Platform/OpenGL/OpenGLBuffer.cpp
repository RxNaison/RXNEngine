#include "rxnpch.h"
#include "OpenGLBuffer.h"

#include "glad/glad.h"

namespace RXNEngine {

	// Vertex Buffer -------------------------------------------------------------------------------------

	OpenGLVertexBuffer::OpenGLVertexBuffer(uint32_t size)
		: m_Size(size)
	{
		glCreateBuffers(1, &m_RendererID);
		glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
		glBufferData(GL_ARRAY_BUFFER, size, nullptr, GL_DYNAMIC_DRAW);
	}

	OpenGLVertexBuffer::OpenGLVertexBuffer(float* vertices, uint32_t size)
		: m_Size(size)
	{
		glCreateBuffers(1, &m_RendererID);
		glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
		glBufferData(GL_ARRAY_BUFFER, size, vertices, GL_STATIC_DRAW);
	}

	OpenGLVertexBuffer::~OpenGLVertexBuffer()
	{
		glDeleteBuffers(1, &m_RendererID);
	}

	void OpenGLVertexBuffer::Bind() const
	{
		glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
	}

	void OpenGLVertexBuffer::Unbind() const
	{
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	void OpenGLVertexBuffer::SetData(const void* data, uint32_t size)
	{
		glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
		if (size > m_Size)
		{
			m_Size = size;
			glBufferData(GL_ARRAY_BUFFER, m_Size, data, GL_DYNAMIC_DRAW);
		}
		else
		{
			glBufferData(GL_ARRAY_BUFFER, m_Size, nullptr, GL_DYNAMIC_DRAW);
			glBufferSubData(GL_ARRAY_BUFFER, 0, size, data);
		}
	}

	void OpenGLVertexBuffer::BindBase(uint32_t bindingPoint) const
	{
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, bindingPoint, m_RendererID);
	}

	// Index Buffer --------------------------------------------------------------------------------------

	OpenGLIndexBuffer::OpenGLIndexBuffer(uint32_t* indeces, uint32_t count)
		: m_Count(count)
	{
		glCreateBuffers(1, &m_RendererID);
		glNamedBufferData(m_RendererID, count * sizeof(uint32_t), indeces, GL_STATIC_DRAW);
	}

	OpenGLIndexBuffer::~OpenGLIndexBuffer()
	{
		glDeleteBuffers(1, &m_RendererID);
	}

	void OpenGLIndexBuffer::Bind() const
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_RendererID);
	}

	void OpenGLIndexBuffer::Unbind() const
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}


	OpenGLShaderStorageBuffer::OpenGLShaderStorageBuffer(const void* data, uint32_t size)
	{
		glCreateBuffers(1, &m_RendererID);
		glNamedBufferData(m_RendererID, size, data, GL_STATIC_DRAW);
	}

	OpenGLShaderStorageBuffer::~OpenGLShaderStorageBuffer()
	{
		glDeleteBuffers(1, &m_RendererID);
	}

	void OpenGLShaderStorageBuffer::Bind(uint32_t bindingPoint) const
	{
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, bindingPoint, m_RendererID);
	}

	void OpenGLShaderStorageBuffer::Unbind() const
	{
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	}

	void OpenGLShaderStorageBuffer::SetData(const void* data, uint32_t size)
	{
		glNamedBufferSubData(m_RendererID, 0, size, data);
	}


	OpenGLStreamVertexBuffer::OpenGLStreamVertexBuffer(uint32_t size)
	{
		m_RegionSize = (size + 255u) & ~255u;
		m_Size = m_RegionSize * RegionCount;

		glCreateBuffers(1, &m_RendererID);

		glNamedBufferStorage(m_RendererID, m_Size, nullptr,
			GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
		m_Mapped = (uint8_t*)glMapNamedBufferRange(m_RendererID, 0, m_Size,
			GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);

		if (!m_Mapped)
		{
			RXN_CORE_WARN("StreamVertexBuffer: persistent mapping unavailable, falling back to buffer orphaning");
			glDeleteBuffers(1, &m_RendererID);
			glCreateBuffers(1, &m_RendererID);
			glNamedBufferData(m_RendererID, m_Size, nullptr, GL_DYNAMIC_DRAW);
		}
	}

	OpenGLStreamVertexBuffer::~OpenGLStreamVertexBuffer()
	{
		for (uint32_t i = 0; i < RegionCount; i++)
		{
			if (m_RegionFences[i])
				glDeleteSync((GLsync)m_RegionFences[i]);
		}

		if (m_Mapped)
			glUnmapNamedBuffer(m_RendererID);

		glDeleteBuffers(1, &m_RendererID);
	}

	uint32_t OpenGLStreamVertexBuffer::Push(const void* data, uint32_t size)
	{
		RXN_CORE_ASSERT(size <= m_RegionSize, "StreamVertexBuffer: allocation larger than a ring region!");

		if (!m_Mapped)
		{
			glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
			glBufferData(GL_ARRAY_BUFFER, m_Size, nullptr, GL_DYNAMIC_DRAW);
			glBufferSubData(GL_ARRAY_BUFFER, 0, size, data);
			return 0;
		}

		uint32_t alignedSize = (size + 63u) & ~63u;

		uint32_t offset = m_Cursor;
		uint32_t region = m_CurrentRegion;

		if (offset + alignedSize > (region + 1) * m_RegionSize)
		{
			region = (region + 1) % RegionCount;
			offset = region * m_RegionSize;

			FenceRegion(m_CurrentRegion);
			WaitForRegion(region);
			m_CurrentRegion = region;
		}

		memcpy(m_Mapped + offset, data, size);
		m_Cursor = offset + alignedSize;
		return offset;
	}

	void OpenGLStreamVertexBuffer::FenceRegion(uint32_t region)
	{
		if (m_RegionFences[region])
			glDeleteSync((GLsync)m_RegionFences[region]);

		m_RegionFences[region] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
	}

	void OpenGLStreamVertexBuffer::WaitForRegion(uint32_t region)
	{
		if (!m_RegionFences[region])
			return;

		RXN_PROFILE_SCOPE_NAMED("StreamVB Fence Wait");

		GLsync sync = (GLsync)m_RegionFences[region];
		GLenum result = glClientWaitSync(sync, GL_SYNC_FLUSH_COMMANDS_BIT, 0);
		while (result == GL_TIMEOUT_EXPIRED)
			result = glClientWaitSync(sync, 0, 1000000);

		glDeleteSync(sync);
		m_RegionFences[region] = nullptr;
	}
}
