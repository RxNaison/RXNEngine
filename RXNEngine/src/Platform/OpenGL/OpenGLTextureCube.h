#pragma once
#include "RXNEngine/Renderer/Texture.h"
#include "RXNEngine/Renderer/Shader.h"

#include <vector>

namespace RXNEngine {
	class OpenGLTextureCube : public TextureCube
	{
	public:
		OpenGLTextureCube(const std::vector<std::string>& paths);
		OpenGLTextureCube(const std::string& path);
		virtual ~OpenGLTextureCube();

		virtual uint32_t GetWidth() const override { return m_Specification.Width; }
		virtual uint32_t GetHeight() const override { return m_Specification.Height; }
		virtual uint32_t GetRendererID() const override { return m_RendererID; }
		virtual const std::string& GetPath() const override { return m_Path; }
		virtual const TextureSpecification& GetSpecification() const override { return m_Specification; }

		virtual void SetData(void* data, uint32_t size) override {}
		virtual void Bind(uint32_t slot = 0) const override;

		uint32_t GetIrradianceRendererID() const { return m_IrradianceMapID; }
		uint32_t GetPrefilterRendererID() const { return m_PrefilterMapID; }
		uint32_t GetBRDFLUTRendererID() const { return m_BRDFLUTMapID; }

		virtual bool IsLoaded() const override { return m_IsLoaded; }

		virtual bool operator==(const Texture& other) const override
		{
			return m_RendererID == ((OpenGLTextureCube&)other).m_RendererID;
		}
	private:
		void CreateIrradianceMap();
		void CreatePrefilterMap();
		void CreateBRDFLUT();
	private:
		uint32_t m_RendererID;
		uint32_t m_IrradianceMapID = 0;
		uint32_t m_PrefilterMapID = 0;
		uint32_t m_BRDFLUTMapID = 0;

		std::string m_Path;
		bool m_IsLoaded = false;
		TextureSpecification m_Specification;

		Ref<Shader> m_EquirectShader;
	};
}