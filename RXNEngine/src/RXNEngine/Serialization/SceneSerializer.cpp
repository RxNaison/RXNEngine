#include "rxnpch.h"
#include "SceneSerializer.h"

#include "RXNEngine/Scene/Entity.h"
#include "RXNEngine/Scene/Components.h"
#include "RXNEngine/Core/UUID.h"
#include "RXNEngine/Asset/AssetManager.h"
#include "RXNEngine/Serialization/YamlHelpers.h"

#include <fstream>

namespace RXNEngine {

	static std::string GetRelativePath(const std::string& path)
	{
		std::filesystem::path filePath(path);
		std::filesystem::path currentPath = std::filesystem::current_path();

		if (filePath.is_absolute())
			return std::filesystem::relative(filePath, currentPath).generic_string();

		return filePath.generic_string();
	}

	SceneSerializer::SceneSerializer(const Ref<Scene>& scene)
		: m_Scene(scene)
	{
	}

	void SceneSerializer::SerializeEntity(YAML::Emitter& out, Entity entity)
	{
		RXN_CORE_ASSERT(entity.HasComponent<IDComponent>());

		out << YAML::BeginMap; // Entity
		out << YAML::Key << "Entity" << YAML::Value << entity.GetUUID();

		if (entity.HasComponent<TagComponent>())
		{
			out << YAML::Key << "TagComponent";
			out << YAML::BeginMap; // TagComponent

			auto& tag = entity.GetComponent<TagComponent>().Tag;
			out << YAML::Key << "Tag" << YAML::Value << tag;

			out << YAML::EndMap; // TagComponent
		}

		if (entity.HasComponent<TransformComponent>())
		{
			out << YAML::Key << "TransformComponent";
			out << YAML::BeginMap; // TransformComponent

			auto& tc = entity.GetComponent<TransformComponent>();
			out << YAML::Key << "Translation" << YAML::Value << tc.Translation;
			out << YAML::Key << "Rotation" << YAML::Value << tc.Rotation;
			out << YAML::Key << "Scale" << YAML::Value << tc.Scale;

			out << YAML::EndMap; // TransformComponent
		}

		if (entity.HasComponent<RelationshipComponent>())
		{
			out << YAML::Key << "RelationshipComponent";
			out << YAML::BeginMap; // RelationshipComponent

			auto& rc = entity.GetComponent<RelationshipComponent>();
			out << YAML::Key << "ParentHandle" << YAML::Value << rc.ParentHandle;

			out << YAML::Key << "Children" << YAML::Value << YAML::BeginSeq;
			for (auto child : rc.Children)
			{
				out << child;
			}
			out << YAML::EndSeq;

			out << YAML::EndMap; // RelationshipComponent
		}

		if (entity.HasComponent<CameraComponent>())
		{
			out << YAML::Key << "CameraComponent";
			out << YAML::BeginMap; // CameraComponent

			auto& cameraComponent = entity.GetComponent<CameraComponent>();
			auto& camera = cameraComponent.Camera;

			out << YAML::Key << "Camera" << YAML::Value;
			out << YAML::BeginMap; // Camera
			out << YAML::Key << "ProjectionType" << YAML::Value << (int)camera.GetProjectionType();
			out << YAML::Key << "PerspectiveFOV" << YAML::Value << camera.GetPerspectiveFOV();
			out << YAML::Key << "PerspectiveNear" << YAML::Value << camera.GetPerspectiveNearClip();
			out << YAML::Key << "PerspectiveFar" << YAML::Value << camera.GetPerspectiveFarClip();
			out << YAML::Key << "OrthographicSize" << YAML::Value << camera.GetOrthographicSize();
			out << YAML::Key << "OrthographicNear" << YAML::Value << camera.GetOrthographicNearClip();
			out << YAML::Key << "OrthographicFar" << YAML::Value << camera.GetOrthographicFarClip();
			out << YAML::EndMap; // Camera

			out << YAML::Key << "FixedAspectRatio" << YAML::Value << cameraComponent.FixedAspectRatio;

			out << YAML::EndMap; // CameraComponent
		}

		if (entity.HasComponent<StaticMeshComponent>())
		{
			out << YAML::Key << "StaticMeshComponent";
			out << YAML::BeginMap;

			auto& mc = entity.GetComponent<StaticMeshComponent>();

			out << YAML::Key << "AssetPath" << YAML::Value << GetRelativePath(mc.AssetPath);
			out << YAML::Key << "SubmeshIndex" << YAML::Value << mc.SubmeshIndex;
			out << YAML::Key << "MaterialAssetPath" << YAML::Value << GetRelativePath(mc.MaterialAssetPath);

			out << YAML::EndMap;
		}

		if (entity.HasComponent<DirectionalLightComponent>())
		{
			out << YAML::Key << "DirectionalLightComponent";
			out << YAML::BeginMap; // DirectionalLightComponent

			auto& dl = entity.GetComponent<DirectionalLightComponent>();
			out << YAML::Key << "Color" << YAML::Value << dl.Color;
			out << YAML::Key << "Intensity" << YAML::Value << dl.Intensity;

			out << YAML::EndMap; // DirectionalLightComponent
		}

		if (entity.HasComponent<PointLightComponent>())
		{
			out << YAML::Key << "PointLightComponent";
			out << YAML::BeginMap; // PointLightComponent

			auto& pl = entity.GetComponent<PointLightComponent>();
			out << YAML::Key << "Color" << YAML::Value << pl.Color;
			out << YAML::Key << "Intensity" << YAML::Value << pl.Intensity;
			out << YAML::Key << "Radius" << YAML::Value << pl.Radius;
			out << YAML::Key << "Falloff" << YAML::Value << pl.Falloff;
			out << YAML::Key << "CastsShadows" << YAML::Value << pl.CastsShadows;

			out << YAML::EndMap; // PointLightComponent
		}

		if (entity.HasComponent<SpotLightComponent>())
		{
			out << YAML::Key << "SpotLightComponent";
			out << YAML::BeginMap;

			auto& sl = entity.GetComponent<SpotLightComponent>();
			out << YAML::Key << "Color" << YAML::Value << sl.Color;
			out << YAML::Key << "Intensity" << YAML::Value << sl.Intensity;
			out << YAML::Key << "Radius" << YAML::Value << sl.Radius;
			out << YAML::Key << "Falloff" << YAML::Value << sl.Falloff;
			out << YAML::Key << "InnerAngle" << YAML::Value << sl.InnerAngle;
			out << YAML::Key << "OuterAngle" << YAML::Value << sl.OuterAngle;
			out << YAML::Key << "CastsShadows" << YAML::Value << sl.CastsShadows;

			out << YAML::Key << "CookieAssetPath" << YAML::Value << sl.CookieAssetPath;
			out << YAML::Key << "IsVideo" << YAML::Value << sl.IsVideo;
			out << YAML::Key << "CookieSize" << YAML::Value << sl.CookieSize;

			out << YAML::EndMap;
		}

		if (entity.HasComponent<RigidbodyComponent>())
		{
			out << YAML::Key << "RigidbodyComponent";
			out << YAML::BeginMap; // RigidbodyComponent

			auto& rb = entity.GetComponent<RigidbodyComponent>();
			out << YAML::Key << "Type" << YAML::Value << (int)rb.Type;
			out << YAML::Key << "Mass" << YAML::Value << rb.Mass;
			out << YAML::Key << "LinearDrag" << YAML::Value << rb.LinearDrag;
			out << YAML::Key << "AngularDrag" << YAML::Value << rb.AngularDrag;

			out << YAML::Key << "FixedRotation" << YAML::Value << rb.FixedRotation;

			out << YAML::Key << "UseCCD" << YAML::Value << rb.UseCCD;
			out << YAML::Key << "CCDVelocityThreshold" << YAML::Value << rb.CCDVelocityThreshold;

			out << YAML::EndMap; // RigidbodyComponent
		}

		if (entity.HasComponent<BoxColliderComponent>())
		{
			out << YAML::Key << "BoxColliderComponent";
			out << YAML::BeginMap; // BoxColliderComponent

			auto& bc = entity.GetComponent<BoxColliderComponent>();
			out << YAML::Key << "HalfExtents" << YAML::Value << bc.HalfExtents;
			out << YAML::Key << "Offset" << YAML::Value << bc.Offset;
			out << YAML::Key << "StaticFriction" << YAML::Value << bc.StaticFriction;
			out << YAML::Key << "DynamicFriction" << YAML::Value << bc.DynamicFriction;
			out << YAML::Key << "Restitution" << YAML::Value << bc.Restitution;

			out << YAML::Key << "IsTrigger" << YAML::Value << bc.IsTrigger;

			out << YAML::EndMap; // BoxColliderComponent
		}

		if (entity.HasComponent<SphereColliderComponent>())
		{
			out << YAML::Key << "SphereColliderComponent";
			out << YAML::BeginMap; // SphereColliderComponent

			auto& sc = entity.GetComponent<SphereColliderComponent>();
			out << YAML::Key << "Radius" << YAML::Value << sc.Radius;
			out << YAML::Key << "Offset" << YAML::Value << sc.Offset;
			out << YAML::Key << "StaticFriction" << YAML::Value << sc.StaticFriction;
			out << YAML::Key << "DynamicFriction" << YAML::Value << sc.DynamicFriction;
			out << YAML::Key << "Restitution" << YAML::Value << sc.Restitution;

			out << YAML::Key << "IsTrigger" << YAML::Value << sc.IsTrigger;

			out << YAML::EndMap; // SphereColliderComponent
		}

		if (entity.HasComponent<CapsuleColliderComponent>())
		{
			out << YAML::Key << "CapsuleColliderComponent";
			out << YAML::BeginMap; // CapsuleColliderComponent

			auto& cc = entity.GetComponent<CapsuleColliderComponent>();
			out << YAML::Key << "Radius" << YAML::Value << cc.Radius;
			out << YAML::Key << "Height" << YAML::Value << cc.Height;
			out << YAML::Key << "Offset" << YAML::Value << cc.Offset;
			out << YAML::Key << "StaticFriction" << YAML::Value << cc.StaticFriction;
			out << YAML::Key << "DynamicFriction" << YAML::Value << cc.DynamicFriction;
			out << YAML::Key << "Restitution" << YAML::Value << cc.Restitution;

			out << YAML::Key << "IsTrigger" << YAML::Value << cc.IsTrigger;

			out << YAML::EndMap; // CapsuleColliderComponent
		}

		if (entity.HasComponent<MeshColliderComponent>())
		{
			out << YAML::Key << "MeshColliderComponent";
			out << YAML::BeginMap; // MeshColliderComponent

			auto& mc = entity.GetComponent<MeshColliderComponent>();
			out << YAML::Key << "IsConvex" << YAML::Value << mc.IsConvex;
			out << YAML::Key << "OverrideAssetPath" << YAML::Value << mc.OverrideAssetPath;
			out << YAML::Key << "StaticFriction" << YAML::Value << mc.StaticFriction;
			out << YAML::Key << "DynamicFriction" << YAML::Value << mc.DynamicFriction;
			out << YAML::Key << "Restitution" << YAML::Value << mc.Restitution;
			out << YAML::Key << "IsTrigger" << YAML::Value << mc.IsTrigger;

			out << YAML::EndMap; // MeshColliderComponent
		}

		if (entity.HasComponent<CharacterControllerComponent>())
		{
			out << YAML::Key << "CharacterControllerComponent";
			out << YAML::BeginMap;

			auto& cct = entity.GetComponent<CharacterControllerComponent>();
			out << YAML::Key << "SlopeLimitDegrees" << YAML::Value << cct.SlopeLimitDegrees;
			out << YAML::Key << "StepOffset" << YAML::Value << cct.StepOffset;
			out << YAML::Key << "Radius" << YAML::Value << cct.Radius;
			out << YAML::Key << "Height" << YAML::Value << cct.Height;

			out << YAML::EndMap;
		}

		if (entity.HasComponent<ScriptComponent>())
		{
			out << YAML::Key << "ScriptComponent";
			out << YAML::BeginMap; // ScriptComponent

			auto& sc = entity.GetComponent<ScriptComponent>();
			out << YAML::Key << "ClassName" << YAML::Value << sc.ClassName;

			out << YAML::EndMap; // ScriptComponent
		}

		out << YAML::EndMap; // Entity
	}

	void SceneSerializer::DeserializeComponentsToEntity(YAML::Node& entity, Entity deserializedEntity)
	{
		auto transformComponent = entity["TransformComponent"];
		if (transformComponent)
		{
			auto& tc = deserializedEntity.GetComponent<TransformComponent>();

			if (transformComponent["Translation"])
				tc.Translation = transformComponent["Translation"].as<glm::vec3>();

			if (transformComponent["Rotation"])
				tc.Rotation = transformComponent["Rotation"].as<glm::vec3>();

			if (transformComponent["Scale"])
				tc.Scale = transformComponent["Scale"].as<glm::vec3>();
		}

		auto relationshipComponent = entity["RelationshipComponent"];
		if (relationshipComponent)
		{
			auto& rc = deserializedEntity.GetComponent<RelationshipComponent>();

			if (relationshipComponent["ParentHandle"])
				rc.ParentHandle = relationshipComponent["ParentHandle"].as<uint64_t>();

			if (auto children = relationshipComponent["Children"])
			{
				for (auto child : children)
					rc.Children.push_back(child.as<uint64_t>());
			}
		}

		auto cameraComponent = entity["CameraComponent"];
		if (cameraComponent)
		{
			auto& cc = deserializedEntity.AddComponent<CameraComponent>();

			if (auto cameraProps = cameraComponent["Camera"])
			{
				if (cameraProps["ProjectionType"])
					cc.Camera.SetProjectionType((SceneCamera::ProjectionMode)cameraProps["ProjectionType"].as<int>());
				if (cameraProps["PerspectiveFOV"])
					cc.Camera.SetPerspectiveFOV(cameraProps["PerspectiveFOV"].as<float>());
				if (cameraProps["PerspectiveNear"])
					cc.Camera.SetPerspectiveNearClip(cameraProps["PerspectiveNear"].as<float>());
				if (cameraProps["PerspectiveFar"])
					cc.Camera.SetPerspectiveFarClip(cameraProps["PerspectiveFar"].as<float>());

				if (cameraProps["OrthographicSize"])
					cc.Camera.SetOrthographicSize(cameraProps["OrthographicSize"].as<float>());
				if (cameraProps["OrthographicNear"])
					cc.Camera.SetOrthographicNearClip(cameraProps["OrthographicNear"].as<float>());
				if (cameraProps["OrthographicFar"])
					cc.Camera.SetOrthographicFarClip(cameraProps["OrthographicFar"].as<float>());
			}

			if (cameraComponent["FixedAspectRatio"]) cc.FixedAspectRatio = cameraComponent["FixedAspectRatio"].as<bool>();
		}

		auto staticMeshComponent = entity["StaticMeshComponent"];
		if (staticMeshComponent)
		{
			auto& mc = deserializedEntity.AddComponent<StaticMeshComponent>();

			if (staticMeshComponent["AssetPath"])
			{
				std::string assetPath = staticMeshComponent["AssetPath"].as<std::string>();
				mc.AssetPath = assetPath;
				mc.Mesh = Application::Get().GetSubsystem<AssetManager>()->GetMesh(assetPath);
			}

			if (staticMeshComponent["SubmeshIndex"])
				mc.SubmeshIndex = staticMeshComponent["SubmeshIndex"].as<uint32_t>();

			if (staticMeshComponent["MaterialAssetPath"])
			{
				std::string matPath = staticMeshComponent["MaterialAssetPath"].as<std::string>();
				if (!matPath.empty())
				{
					mc.MaterialAssetPath = matPath;
					mc.MaterialTableOverride = Application::Get().GetSubsystem<AssetManager>()->GetMaterial(matPath);
				}
			}
		}

		auto directionalLightComponent = entity["DirectionalLightComponent"];
		if (directionalLightComponent)
		{
			auto& dlc = deserializedEntity.AddComponent<DirectionalLightComponent>();

			if (directionalLightComponent["Color"])
				dlc.Color = directionalLightComponent["Color"].as<glm::vec3>();
			if (directionalLightComponent["Intensity"])
				dlc.Intensity = directionalLightComponent["Intensity"].as<float>();
		}

		auto pointLightComponent = entity["PointLightComponent"];
		if (pointLightComponent)
		{
			auto& plc = deserializedEntity.AddComponent<PointLightComponent>();

			if (pointLightComponent["Color"])
				plc.Color = pointLightComponent["Color"].as<glm::vec3>();
			if (pointLightComponent["Intensity"])
				plc.Intensity = pointLightComponent["Intensity"].as<float>();
			if (pointLightComponent["Radius"])
				plc.Radius = pointLightComponent["Radius"].as<float>();
			if (pointLightComponent["Falloff"])
				plc.Falloff = pointLightComponent["Falloff"].as<float>();
			if (pointLightComponent["CastsShadows"])
				plc.CastsShadows = pointLightComponent["CastsShadows"].as<bool>();
		}

		auto spotLightComponent = entity["SpotLightComponent"];
		if (spotLightComponent)
		{
			auto& sl = deserializedEntity.AddComponent<SpotLightComponent>();

			if (spotLightComponent["Color"])
				sl.Color = spotLightComponent["Color"].as<glm::vec3>();
			if (spotLightComponent["Intensity"])
				sl.Intensity = spotLightComponent["Intensity"].as<float>();
			if (spotLightComponent["Radius"])
				sl.Radius = spotLightComponent["Radius"].as<float>();
			if (spotLightComponent["Falloff"])
				sl.Falloff = spotLightComponent["Falloff"].as<float>();
			if (spotLightComponent["InnerAngle"])
				sl.InnerAngle = spotLightComponent["InnerAngle"].as<float>();
			if (spotLightComponent["OuterAngle"])
				sl.OuterAngle = spotLightComponent["OuterAngle"].as<float>();
			if (spotLightComponent["CastsShadows"])
				sl.CastsShadows = spotLightComponent["CastsShadows"].as<bool>();

			if (spotLightComponent["IsVideo"])
				sl.IsVideo = spotLightComponent["IsVideo"].as<bool>();

			if (spotLightComponent["CookieAssetPath"])
			{
				std::string path = spotLightComponent["CookieAssetPath"].as<std::string>();
				if (!path.empty())
				{
					sl.CookieAssetPath = path;

					if (sl.IsVideo)
						sl.CookieVideo = CreateRef<VideoTexture>(path);
					else
						sl.CookieTexture = Application::Get().GetSubsystem<AssetManager>()->GetTexture(path);
				}
			}

			if (spotLightComponent["CookieSize"])
				sl.CookieSize = spotLightComponent["CookieSize"].as<float>();
		}

		auto rigidbodyComponent = entity["RigidbodyComponent"];
		if (rigidbodyComponent)
		{
			auto& rb = deserializedEntity.AddComponent<RigidbodyComponent>();

			if (rigidbodyComponent["Type"])
				rb.Type = (RigidbodyComponent::BodyType)rigidbodyComponent["Type"].as<int>();
			if (rigidbodyComponent["Mass"])
				rb.Mass = rigidbodyComponent["Mass"].as<float>();
			if (rigidbodyComponent["LinearDrag"])
				rb.LinearDrag = rigidbodyComponent["LinearDrag"].as<float>();
			if (rigidbodyComponent["AngularDrag"])
				rb.AngularDrag = rigidbodyComponent["AngularDrag"].as<float>();
			if (rigidbodyComponent["FixedRotation"])
				rb.FixedRotation = rigidbodyComponent["FixedRotation"].as<bool>();
			if (rigidbodyComponent["UseCCD"])
				rb.UseCCD = rigidbodyComponent["UseCCD"].as<bool>();
			if (rigidbodyComponent["CCDVelocityThreshold"])
				rb.CCDVelocityThreshold = rigidbodyComponent["CCDVelocityThreshold"].as<float>();
		}

		auto boxColliderComponent = entity["BoxColliderComponent"];
		if (boxColliderComponent)
		{
			auto& bc = deserializedEntity.AddComponent<BoxColliderComponent>();

			if (boxColliderComponent["HalfExtents"])
				bc.HalfExtents = boxColliderComponent["HalfExtents"].as<glm::vec3>();
			if (boxColliderComponent["Offset"])
				bc.Offset = boxColliderComponent["Offset"].as<glm::vec3>();
			if (boxColliderComponent["StaticFriction"])
				bc.StaticFriction = boxColliderComponent["StaticFriction"].as<float>();
			if (boxColliderComponent["DynamicFriction"])
				bc.DynamicFriction = boxColliderComponent["DynamicFriction"].as<float>();
			if (boxColliderComponent["Restitution"])
				bc.Restitution = boxColliderComponent["Restitution"].as<float>();
			if (boxColliderComponent["IsTrigger"])
				bc.IsTrigger = boxColliderComponent["IsTrigger"].as<bool>();
		}

		auto sphereColliderComponent = entity["SphereColliderComponent"];
		if (sphereColliderComponent)
		{
			auto& sc = deserializedEntity.AddComponent<SphereColliderComponent>();

			if (sphereColliderComponent["Radius"])
				sc.Radius = sphereColliderComponent["Radius"].as<float>();
			if (sphereColliderComponent["Offset"])
				sc.Offset = sphereColliderComponent["Offset"].as<glm::vec3>();
			if (sphereColliderComponent["StaticFriction"])
				sc.StaticFriction = sphereColliderComponent["StaticFriction"].as<float>();
			if (sphereColliderComponent["DynamicFriction"])
				sc.DynamicFriction = sphereColliderComponent["DynamicFriction"].as<float>();
			if (sphereColliderComponent["Restitution"])
				sc.Restitution = sphereColliderComponent["Restitution"].as<float>();
			if (sphereColliderComponent["IsTrigger"])
				sc.IsTrigger = sphereColliderComponent["IsTrigger"].as<bool>();
		}

		auto capsuleColliderComponent = entity["CapsuleColliderComponent"];
		if (capsuleColliderComponent)
		{
			auto& cc = deserializedEntity.AddComponent<CapsuleColliderComponent>();

			if (capsuleColliderComponent["Radius"])
				cc.Radius = capsuleColliderComponent["Radius"].as<float>();
			if (capsuleColliderComponent["Height"])
				cc.Height = capsuleColliderComponent["Height"].as<float>();
			if (capsuleColliderComponent["Offset"])
				cc.Offset = capsuleColliderComponent["Offset"].as<glm::vec3>();
			if (capsuleColliderComponent["StaticFriction"])
				cc.StaticFriction = capsuleColliderComponent["StaticFriction"].as<float>();
			if (capsuleColliderComponent["DynamicFriction"])
				cc.DynamicFriction = capsuleColliderComponent["DynamicFriction"].as<float>();
			if (capsuleColliderComponent["Restitution"])
				cc.Restitution = capsuleColliderComponent["Restitution"].as<float>();
			if (capsuleColliderComponent["IsTrigger"])
				cc.IsTrigger = capsuleColliderComponent["IsTrigger"].as<bool>();
		}

		auto meshColliderComponent = entity["MeshColliderComponent"];
		if (meshColliderComponent)
		{
			auto& mc = deserializedEntity.AddComponent<MeshColliderComponent>();
			if (meshColliderComponent["IsConvex"])
				mc.IsConvex = meshColliderComponent["IsConvex"].as<bool>();
			if (meshColliderComponent["OverrideAssetPath"])
				mc.OverrideAssetPath = meshColliderComponent["OverrideAssetPath"].as<std::string>();
			if (meshColliderComponent["StaticFriction"])
				mc.StaticFriction = meshColliderComponent["StaticFriction"].as<float>();
			if (meshColliderComponent["DynamicFriction"])
				mc.DynamicFriction = meshColliderComponent["DynamicFriction"].as<float>();
			if (meshColliderComponent["Restitution"])
				mc.Restitution = meshColliderComponent["Restitution"].as<float>();
			if (meshColliderComponent["IsTrigger"])
				mc.IsTrigger = meshColliderComponent["IsTrigger"].as<bool>();
		}

		auto characterControllerComponent = entity["CharacterControllerComponent"];
		if (characterControllerComponent)
		{
			auto& cct = deserializedEntity.AddComponent<CharacterControllerComponent>();
			if (characterControllerComponent["SlopeLimitDegrees"])
				cct.SlopeLimitDegrees = characterControllerComponent["SlopeLimitDegrees"].as<float>();
			if (characterControllerComponent["StepOffset"])
				cct.StepOffset = characterControllerComponent["StepOffset"].as<float>();
			if (characterControllerComponent["Radius"])
				cct.Radius = characterControllerComponent["Radius"].as<float>();
			if (characterControllerComponent["Height"])
				cct.Height = characterControllerComponent["Height"].as<float>();
		}

		auto scriptComponent = entity["ScriptComponent"];
		if (scriptComponent)
		{
			auto& sc = deserializedEntity.AddComponent<ScriptComponent>();
			if (scriptComponent["ClassName"]) sc.ClassName = scriptComponent["ClassName"].as<std::string>();
		}

	}

	void SceneSerializer::Serialize(const std::string& filepath)
	{
		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Scene" << YAML::Value << "Untitled";

		Entity primaryCam = m_Scene->GetPrimaryCameraEntity();
		if (primaryCam)
			out << YAML::Key << "PrimaryCameraID" << YAML::Value << primaryCam.GetUUID();

		if (m_Scene->m_Skybox)
		{
			out << YAML::Key << "Skybox";
			out << YAML::BeginMap;

			out << YAML::Key << "TexturePath" << YAML::Value << GetRelativePath(m_Scene->m_Skybox->GetPath());

			out << YAML::Key << "Intensity" << YAML::Value << m_Scene->m_SkyboxIntensity;

			out << YAML::EndMap;
		}

		out << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;
		m_Scene->GetRaw().view<entt::entity>().each([&](auto entityID)
			{
				Entity entity = { entityID, m_Scene.get() };
				if (!entity)
					return;

				SerializeEntity(out, entity);
			});
		out << YAML::EndSeq;
		out << YAML::EndMap;

		std::ofstream fout(filepath);
		fout << out.c_str();
	}

	void SceneSerializer::SerializeRuntime(const std::string& filepath)
	{
		RXN_CORE_ASSERT(false);
	}

	bool SceneSerializer::Deserialize(const std::string& filepath)
	{
		YAML::Node data;
		try
		{
			data = YAML::LoadFile(filepath);
		}
		catch (YAML::ParserException e)
		{
			RXN_CORE_ERROR("Failed to load .rxn file '{0}'\n     {1}", filepath, e.what());
			return false;
		}

		if (!data["Scene"])
			return false;

		std::string sceneName = data["Scene"].as<std::string>();
		RXN_CORE_TRACE("Deserializing scene '{0}'", sceneName);

		UUID primaryCameraID = UUID::Null;
		if (data["PrimaryCameraID"])
			primaryCameraID = data["PrimaryCameraID"].as<UUID>();

		auto skyboxData = data["Skybox"];
		if (skyboxData)
		{
			if (skyboxData["TexturePath"])
				m_Scene->m_Skybox = Cubemap::Create(skyboxData["TexturePath"].as<std::string>());

			if (skyboxData["Intensity"])
				m_Scene->m_SkyboxIntensity = skyboxData["Intensity"].as<float>();
		}

		auto entities = data["Entities"];
		if (entities)
		{
			for (auto entity : entities)
			{
				if (!entity["Entity"])
					continue;

				uint64_t uuid = entity["Entity"].as<uint64_t>();

				std::string name;
				auto tagComponent = entity["TagComponent"];
				if (tagComponent && tagComponent["Tag"])
					name = tagComponent["Tag"].as<std::string>();

				RXN_CORE_TRACE("Deserialized entity with ID = {0}, name = {1}", uuid, name);
				Entity deserializedEntity = m_Scene->CreateEntityWithUUID(uuid, name);

				DeserializeComponentsToEntity(entity, deserializedEntity);

				if (primaryCameraID != UUID::Null && uuid == (uint64_t)primaryCameraID)
				{
					m_Scene->SetPrimaryCameraEntity(deserializedEntity);
				}
			}
		}

		return true;
	}

	bool SceneSerializer::DeserializeRuntime(const std::string& filepath)
	{
		RXN_CORE_ASSERT(false);
		return false;
	}

}