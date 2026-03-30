#include "rxnpch.h"
#include "Material.h"

namespace RXNEngine {

	Material::Material(const Ref<Shader>& shader)
		: m_Shader(shader)
	{
	}

    void Material::Bind()
    {
        m_Shader->Bind();

        m_Shader->SetFloat4("u_AlbedoColor", m_Parameters.AlbedoColor);
        m_Shader->SetFloat3("u_EmissiveColor", m_Parameters.EmissiveColor);
        m_Shader->SetFloat("u_Roughness", m_Parameters.Roughness);
        m_Shader->SetFloat("u_Metalness", m_Parameters.Metalness);
        m_Shader->SetFloat("u_AO", m_Parameters.AO);
        m_Shader->SetFloat("u_Tiling", m_Parameters.Tiling);

        // Slot 0: Albedo
        if (m_AlbedoMap)
        {
            m_AlbedoMap->Bind(0);
            m_Shader->SetInt("u_UseAlbedoMap", 1);
        }
        else
        {
            Texture2D::WhiteTexture()->Bind(0);
            m_Shader->SetInt("u_UseAlbedoMap", 0);
        }
        m_Shader->SetInt("u_AlbedoMap", 0);

        // Slot 1: Normal
        if (m_NormalMap)
        {
            m_NormalMap->Bind(1);
            m_Shader->SetInt("u_UseNormalMap", 1);
        }
        else
        {
            Texture2D::BlueTexture()->Bind(1);
            m_Shader->SetInt("u_UseNormalMap", 0);
        }
        m_Shader->SetInt("u_NormalMap", 1);

        // Slot 2: Metal/Rough (Often packed)
        if (m_MetalnessRoughnessMap) 
        { 
            m_MetalnessRoughnessMap->Bind(2);
            m_Shader->SetInt("u_MetallicMap", 2);
            m_Shader->SetInt("u_RoughnessMap", 2);
        }
        else
        { 
            Texture2D::WhiteTexture()->Bind(2);
            m_Shader->SetInt("u_MetallicMap", 2);
            m_Shader->SetInt("u_RoughnessMap", 2);
        }
        m_Shader->SetInt("u_MetalnessRoughnessMap", 2);

        // Slot 3: Ambient Occlusion (AO)
        if (m_AOMap)
        { 
            m_AOMap->Bind(3);
            m_Shader->SetInt("u_UseAOMap", 1);
        }
        else
        { 
            Texture2D::WhiteTexture()->Bind(3);
            m_Shader->SetInt("u_UseAOMap", 0);
        }
        m_Shader->SetInt("u_AOMap", 3);

        // Slot 4: Emissive
        if (m_EmissiveMap)
        { 
            m_EmissiveMap->Bind(4);
            m_Shader->SetInt("u_UseEmissiveMap", 1);
        }
        else
        { 
            Texture2D::WhiteTexture()->Bind(4);
            m_Shader->SetInt("u_UseEmissiveMap", 0);
        }
        m_Shader->SetInt("u_EmissiveMap", 4);
    }

	Ref<Material> Material::CreateDefault(const Ref<Shader>& shader)
	{
		return CreateRef<Material>(shader);
	}
}