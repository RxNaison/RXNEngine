#pragma once
#include "RXNEngine/Renderer/GraphicsAPI/Buffer.h"

namespace RXNEngine {

	class OpenGLVertexBuffer : public VertexBuffer
	{
	public:
		OpenGLVertexBuffer(uint32_t size);
		OpenGLVertexBuffer(float* vertices, uint32_t size);
		virtual ~OpenGLVertexBuffer();

		virtual void Bind() const override;
		virtual void Unbind() const override;

		virtual void SetData(const void* data, uint32_t size) override;
		virtual uint32_t GetRendererID() const override { return m_RendererID; }

		inline virtual const BufferLayout& GetLayout() const override { return m_Layout; }
		inline virtual void SetLayout(const BufferLayout& layout) override { m_Layout = layout; }

		virtual void BindBase(uint32_t bindingPoint) const override;
	private:
		uint32_t m_RendererID;
		BufferLayout m_Layout;
		uint32_t m_Size = 0;
	};


	class OpenGLIndexBuffer : public IndexBuffer
	{
	public:
		OpenGLIndexBuffer(uint32_t* vertices, uint32_t count);
		virtual ~OpenGLIndexBuffer();

		virtual void Bind() const override;
		virtual void Unbind() const override;

		uint32_t GetCount() const { return m_Count; }
	private:
		uint32_t m_RendererID;
		uint32_t m_Count;
	};

	class OpenGLShaderStorageBuffer : public ShaderStorageBuffer
	{
	public:
		OpenGLShaderStorageBuffer(const void* data, uint32_t size);
		virtual ~OpenGLShaderStorageBuffer();

		virtual void Bind(uint32_t bindingPoint) const override;
		virtual void Unbind() const override;

		virtual void SetData(const void* data, uint32_t size) override;
		virtual uint32_t GetRendererID() const override { return m_RendererID; }
	private:
		uint32_t m_RendererID = 0;
	};

	class OpenGLStreamVertexBuffer : public StreamVertexBuffer
	{
	public:
		OpenGLStreamVertexBuffer(uint32_t size);
		virtual ~OpenGLStreamVertexBuffer();

		virtual uint32_t Push(const void* data, uint32_t size) override;
		virtual uint32_t GetRendererID() const override { return m_RendererID; }

		inline virtual const BufferLayout& GetLayout() const override { return m_Layout; }
		inline virtual void SetLayout(const BufferLayout& layout) override { m_Layout = layout; }
	private:
		void FenceRegion(uint32_t region);
		void WaitForRegion(uint32_t region);

		static constexpr uint32_t RegionCount = 8;

		uint32_t m_RendererID = 0;
		BufferLayout m_Layout;
		uint8_t* m_Mapped = nullptr;
		uint32_t m_Size = 0;
		uint32_t m_RegionSize = 0;
		uint32_t m_Cursor = 0;
		uint32_t m_CurrentRegion = 0;
		void* m_RegionFences[RegionCount] = {};
	};

}