#include "rxnpch.h"
#include "OpenGLTexture.h"
#include "RXNEngine/Core/Application.h"
#include "RXNEngine/Core/VFSSystem.h"

#include <stb_image.h>
#include <fstream>

namespace RXNEngine {

	namespace Utils {

		static GLenum ImageFormatToGLDataFormat(ImageFormat format)
		{
			switch (format)
			{
				case ImageFormat::RGB8:  return GL_RGB;
				case ImageFormat::RGBA8: return GL_RGBA;
			}

			RXN_CORE_ASSERT(false);
			return 0;
		}

		static GLenum ImageFormatToGLInternalFormat(ImageFormat format)
		{
			switch (format)
			{
				case ImageFormat::RGB8:  return GL_RGB8;
				case ImageFormat::RGBA8: return GL_RGBA8;
			}

			RXN_CORE_ASSERT(false);
			return 0;
		}

	}

#ifndef GL_COMPRESSED_RGBA_S3TC_DXT1_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT 0x83F1
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT3_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT 0x83F2
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83F3
#endif
#ifndef GL_COMPRESSED_RED_RGTC1
#define GL_COMPRESSED_RED_RGTC1 0x8DBB
#endif
#ifndef GL_COMPRESSED_RG_RGTC2
#define GL_COMPRESSED_RG_RGTC2 0x8DBD
#endif

	struct DDS_PIXELFORMAT {
		uint32_t dwSize;
		uint32_t dwFlags;
		uint32_t dwFourCC;
		uint32_t dwRGBBitCount;
		uint32_t dwRBitMask;
		uint32_t dwGBitMask;
		uint32_t dwBBitMask;
		uint32_t dwABitMask;
	};

	struct DDS_HEADER {
		uint32_t dwSize;
		uint32_t dwFlags;
		uint32_t dwHeight;
		uint32_t dwWidth;
		uint32_t dwPitchOrLinearSize;
		uint32_t dwDepth;
		uint32_t dwMipMapCount;
		uint32_t dwReserved1[11];
		DDS_PIXELFORMAT ddspf;
		uint32_t dwCaps;
		uint32_t dwCaps2;
		uint32_t dwCaps3;
		uint32_t dwCaps4;
		uint32_t dwReserved2;
	};

	bool OpenGLTexture2D::LoadDDS(const uint8_t* data, size_t size)
	{
		if (size < 128)
		{
			RXN_CORE_ERROR("Failed to load DDS texture: file too small!");
			return false;
		}

		uint32_t magic = *(const uint32_t*)data;
		if (magic != 0x20534444) // "DDS "
		{
			RXN_CORE_ERROR("Failed to load DDS texture: invalid magic number!");
			return false;
		}

		const DDS_HEADER* header = (const DDS_HEADER*)(data + 4);
		if (header->dwSize != 124 || header->ddspf.dwSize != 32)
		{
			RXN_CORE_ERROR("Failed to load DDS texture: invalid header sizes!");
			return false;
		}

		m_Width = header->dwWidth;
		m_Height = header->dwHeight;
		uint32_t mipmapCount = header->dwMipMapCount > 0 ? header->dwMipMapCount : 1;

		GLenum internalFormat = 0;
		uint32_t blockSize = 16;

		if (header->ddspf.dwFlags & 0x00000004) // DDPF_FOURCC
		{
			switch (header->ddspf.dwFourCC)
			{
				case 0x31545844: // "DXT1"
					internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
					blockSize = 8;
					break;
				case 0x33545844: // "DXT3"
					internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
					blockSize = 16;
					break;
				case 0x35545844: // "DXT5"
					internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
					blockSize = 16;
					break;
				case 0x32495441: // "ATI2" (BC5)
					internalFormat = GL_COMPRESSED_RG_RGTC2;
					blockSize = 16;
					break;
				case 0x31495441: // "ATI1" (BC4)
					internalFormat = GL_COMPRESSED_RED_RGTC1;
					blockSize = 8;
					break;
				default:
					RXN_CORE_ERROR("Failed to load DDS texture: unsupported FourCC format 0x{0:x}!", header->ddspf.dwFourCC);
					return false;
			}
		}
		else
		{
			RXN_CORE_ERROR("Failed to load DDS texture: uncompressed or non-FourCC formats not supported!");
			return false;
		}

		m_InternalFormat = internalFormat;
		m_DataFormat = internalFormat;

		glCreateTextures(GL_TEXTURE_2D, 1, &m_RendererID);
		glTextureStorage2D(m_RendererID, mipmapCount, internalFormat, m_Width, m_Height);

		glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, mipmapCount > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
		glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, GL_REPEAT);

		GLfloat maxAnisotropy;
		glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAnisotropy);
		glTextureParameterf(m_RendererID, GL_TEXTURE_MAX_ANISOTROPY, maxAnisotropy);

		size_t offset = 128; // magic (4) + header (124)
		uint32_t mipWidth = m_Width;
		uint32_t mipHeight = m_Height;

		for (uint32_t level = 0; level < mipmapCount; ++level)
		{
			uint32_t mipSize = ((mipWidth + 3) / 4) * ((mipHeight + 3) / 4) * blockSize;
			if (offset + mipSize > size)
			{
				RXN_CORE_ERROR("Failed to load DDS texture: unexpected end of file data at mipmap level {0}!", level);
				glDeleteTextures(1, &m_RendererID);
				m_RendererID = 0;
				return false;
			}

			glCompressedTextureSubImage2D(m_RendererID, level, 0, 0, mipWidth, mipHeight, internalFormat, mipSize, data + offset);

			offset += mipSize;
			mipWidth = std::max(1u, mipWidth / 2);
			mipHeight = std::max(1u, mipHeight / 2);
		}

		m_IsLoaded = true;
		return true;
	}

	OpenGLTexture2D::OpenGLTexture2D(const TextureSpecification& specification)
		: m_Specification(specification), m_Width(m_Specification.Width), m_Height(m_Specification.Height)
	{
		m_InternalFormat = Utils::ImageFormatToGLInternalFormat(m_Specification.Format);
		m_DataFormat = Utils::ImageFormatToGLDataFormat(m_Specification.Format);

		glCreateTextures(GL_TEXTURE_2D, 1, &m_RendererID);
		glTextureStorage2D(m_RendererID, 1, m_InternalFormat, m_Width, m_Height);

		glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, GL_REPEAT);
	}

	OpenGLTexture2D::OpenGLTexture2D(const std::string& path)
		: m_Path(path)
	{
		int width, height, channels;
		stbi_set_flip_vertically_on_load(1);
		bool isDDS = path.ends_with(".dds") || path.ends_with(".DDS");

		auto vfs = Application::Get().GetSubsystem<VFSSystem>();
		if (vfs && vfs->FileExists(path))
		{
			auto fileData = vfs->ReadFile(path);
			if (!fileData.empty())
			{
				if (isDDS || (fileData.size() >= 4 && *(const uint32_t*)fileData.data() == 0x20534444))
				{
					if (LoadDDS((const uint8_t*)fileData.data(), fileData.size()))
						return;
				}

				stbi_set_flip_vertically_on_load(1);
				if (stbi_is_hdr_from_memory((const stbi_uc*)fileData.data(), (int)fileData.size()))
				{
					float* data = stbi_loadf_from_memory((const stbi_uc*)fileData.data(), (int)fileData.size(), &width, &height, &channels, 0);
					if (data)
					{
						m_IsLoaded = true;
						m_Width = width;
						m_Height = height;

						m_InternalFormat = GL_RGB16F;
						m_DataFormat = GL_RGB;

						glCreateTextures(GL_TEXTURE_2D, 1, &m_RendererID);
						glTextureStorage2D(m_RendererID, 1, m_InternalFormat, m_Width, m_Height);

						glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
						glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
						glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
						glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

						glTextureSubImage2D(m_RendererID, 0, 0, 0, m_Width, m_Height, m_DataFormat, GL_FLOAT, data);
						glGenerateTextureMipmap(m_RendererID);
						stbi_image_free(data);
					}
				}
				else
				{
					stbi_uc* data = stbi_load_from_memory((const stbi_uc*)fileData.data(), (int)fileData.size(), &width, &height, &channels, 4);
					if (data)
					{
						m_IsLoaded = true;
						m_Width = width;
						m_Height = height;

						GLenum internalFormat = GL_RGBA8, dataFormat = GL_RGBA;
						m_InternalFormat = internalFormat;
						m_DataFormat = dataFormat;

						glCreateTextures(GL_TEXTURE_2D, 1, &m_RendererID);
						glTextureStorage2D(m_RendererID, 1, internalFormat, m_Width, m_Height);

						glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
						glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
						glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, GL_REPEAT);
						glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, GL_REPEAT);

						GLfloat maxAnisotropy;
						glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAnisotropy);
						glTextureParameterf(m_RendererID, GL_TEXTURE_MAX_ANISOTROPY, maxAnisotropy);

						glTextureSubImage2D(m_RendererID, 0, 0, 0, m_Width, m_Height, m_DataFormat, GL_UNSIGNED_BYTE, data);
						glGenerateTextureMipmap(m_RendererID);
						stbi_image_free(data);
					}
				}
			}
		}
		else
		{
			if (isDDS)
			{
				std::ifstream stream(path, std::ios::binary | std::ios::ate);
				if (stream)
				{
					size_t size = stream.tellg();
					std::vector<uint8_t> fileData(size);
					stream.seekg(0, std::ios::beg);
					stream.read((char*)fileData.data(), size);
					if (LoadDDS(fileData.data(), size))
						return;
				}
			}

			if (stbi_is_hdr(path.c_str()))
			{
				float* data = stbi_loadf(path.c_str(), &width, &height, &channels, 0);
				if (data)
				{
					m_IsLoaded = true;
					m_Width = width;
					m_Height = height;

					m_InternalFormat = GL_RGB16F;
					m_DataFormat = GL_RGB;

					glCreateTextures(GL_TEXTURE_2D, 1, &m_RendererID);
					glTextureStorage2D(m_RendererID, 1, m_InternalFormat, m_Width, m_Height);

					glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
					glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
					glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
					glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

					glTextureSubImage2D(m_RendererID, 0, 0, 0, m_Width, m_Height, m_DataFormat, GL_FLOAT, data);

					glGenerateTextureMipmap(m_RendererID);

					stbi_image_free(data);
				}
			}
			else
			{
				stbi_uc* data = nullptr;
				data = stbi_load(path.c_str(), &width, &height, &channels, 4);

				if (data)
				{
					m_IsLoaded = true;

					m_Width = width;
					m_Height = height;

					GLenum internalFormat = GL_RGBA8, dataFormat = GL_RGBA;

					m_InternalFormat = internalFormat;
					m_DataFormat = dataFormat;

					glCreateTextures(GL_TEXTURE_2D, 1, &m_RendererID);
					glTextureStorage2D(m_RendererID, 1, internalFormat, m_Width, m_Height);

					glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
					glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
					glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, GL_REPEAT);
					glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, GL_REPEAT);

					GLfloat maxAnisotropy;
					glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAnisotropy);
					glTextureParameterf(m_RendererID, GL_TEXTURE_MAX_ANISOTROPY, maxAnisotropy);

					glTextureSubImage2D(m_RendererID, 0, 0, 0, m_Width, m_Height, m_DataFormat, GL_UNSIGNED_BYTE, data);
					glGenerateTextureMipmap(m_RendererID);

					stbi_image_free(data);
				}
			}
		}
	}

	OpenGLTexture2D::OpenGLTexture2D(const void* data, size_t size)
	{
		if (size >= 128 && *(const uint32_t*)data == 0x20534444)
		{
			if (LoadDDS((const uint8_t*)data, size))
				return;
		}

		int width, height, channels;
		stbi_set_flip_vertically_on_load(1);

		stbi_uc* imageData = stbi_load_from_memory((const stbi_uc*)data, (int)size, &width, &height, &channels, 0);

		if (imageData)
		{
			m_IsLoaded = true;
			m_Width = width;
			m_Height = height;

			GLenum internalFormat = 0, dataFormat = 0;
			if (channels == 4)
			{
				internalFormat = GL_RGBA8;
				dataFormat = GL_RGBA;
			}
			else if (channels == 3)
			{
				internalFormat = GL_RGB8;
				dataFormat = GL_RGB;
			}

			m_InternalFormat = internalFormat;
			m_DataFormat = dataFormat;

			glCreateTextures(GL_TEXTURE_2D, 1, &m_RendererID);
			glTextureStorage2D(m_RendererID, 1, internalFormat, m_Width, m_Height);

			glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, GL_REPEAT);

			glTextureSubImage2D(m_RendererID, 0, 0, 0, m_Width, m_Height, dataFormat, GL_UNSIGNED_BYTE, imageData);

			stbi_image_free(imageData);
		}
		else
		{
			RXN_CORE_ERROR("Failed to load texture from memory!");
		}
	}

	OpenGLTexture2D::~OpenGLTexture2D()
	{
		glDeleteTextures(1, &m_RendererID);
	}

	void OpenGLTexture2D::SetData(void* data, uint32_t size)
	{
		uint32_t bytesPerPixel = m_DataFormat == GL_RGBA ? 4 : 3;
		RXN_CORE_ASSERT(size == m_Width * m_Height * bytesPerPixel, "Data must be entire texture!");

		if (bytesPerPixel == 3)
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

		glTextureSubImage2D(m_RendererID, 0, 0, 0, m_Width, m_Height, m_DataFormat, GL_UNSIGNED_BYTE, data);

		if (bytesPerPixel == 3)
			glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

		if (m_Specification.GenerateMips)
			glGenerateTextureMipmap(m_RendererID);
	}

	void OpenGLTexture2D::Bind(uint32_t slot) const
	{
		glBindTextureUnit(slot, m_RendererID);
	}
}