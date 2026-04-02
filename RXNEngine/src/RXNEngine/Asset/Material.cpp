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
            m_AlbedoMap->Bind(0);
        else
            Texture2D::WhiteTexture()->Bind(0);

        m_Shader->SetInt("u_AlbedoMap", 0);

        // Slot 1: Normal
        if (m_NormalMap)
            m_NormalMap->Bind(1);
        else
            Texture2D::BlueTexture()->Bind(1);

        m_Shader->SetInt("u_NormalMap", 1);

        // Slot 2: Metalness
        if (m_MetalnessMap)
            m_MetalnessMap->Bind(2);
        else
            Texture2D::WhiteTexture()->Bind(2);

        m_Shader->SetInt("u_MetalnessMap", 2);

        // Slot 3: Roughness
        if (m_RoughnessMap)
            m_RoughnessMap->Bind(3);
        else
            Texture2D::WhiteTexture()->Bind(3);

        m_Shader->SetInt("u_RoughnessMap", 3);

        // Slot 4: Ambient Occlusion (AO)
        if (m_AOMap)
            m_AOMap->Bind(4);
        else
            Texture2D::WhiteTexture()->Bind(4);

        m_Shader->SetInt("u_AOMap", 4);

        // Slot 5: Emissive
        if (m_EmissiveMap)
            m_EmissiveMap->Bind(5);
        else
            Texture2D::WhiteTexture()->Bind(5);

        m_Shader->SetInt("u_EmissiveMap", 5);
    }

	Ref<Material> Material::CreateDefault(const Ref<Shader>& shader)
	{
		return CreateRef<Material>(shader);
	}

    Ref<Material> Material::Copy(const Ref<Material>& other)
    {
        Ref<Material> mat = CreateRef<Material>(other->GetShader());

        mat->SetParameters(other->GetParameters());
        mat->SetTransparent(other->IsTransparent());

        mat->SetAlbedoMap(other->GetAlbedoMap(), other->GetAlbedoPath());
        mat->SetNormalMap(other->GetNormalMap(), other->GetNormalPath());
        mat->SetMetalnessMap(other->GetMetalnessMap(), other->GetMetalPath());
        mat->SetRoughnessMap(other->GetRoughnessMap(), other->GetRoughPath());
        mat->SetAOMap(other->GetAOMap(), other->GetAOPath());
        mat->SetEmissiveMap(other->GetEmissiveMap(), other->GetEmissivePath());

        return mat;
    }
}