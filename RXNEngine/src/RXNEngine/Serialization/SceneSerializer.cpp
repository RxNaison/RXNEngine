#include "rxnpch.h"
#include "SceneSerializer.h"

#include "RXNEngine/Scene/Entity.h"
#include "RXNEngine/Scene/Components.h"
#include "RXNEngine/Core/UUID.h"
#include "RXNEngine/Asset/AssetManager.h"

#include <fstream>

#include <yaml-cpp/yaml.h>

namespace YAML {

	template<>
	struct convert<glm::vec2>
	{
		static Node encode(const glm::vec2& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.SetStyle(EmitterStyle::Flow);
			return node;
		}

		static bool decode(const Node& node, glm::vec2& rhs)
		{
			if (!node.IsSequence() || node.size() != 2)
				return false;

			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			return true;
		}
	};

	template<>
	struct convert<glm::vec3>
	{
		static Node encode(const glm::vec3& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.push_back(rhs.z);
			node.SetStyle(EmitterStyle::Flow);
			return node;
		}

		static bool decode(const Node& node, glm::vec3& rhs)
		{
			if (!node.IsSequence() || node.size() != 3)
				return false;

			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			rhs.z = node[2].as<float>();
			return true;
		}
	};

	template<>
	struct convert<glm::vec4>
	{
		static Node encode(const glm::vec4& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.push_back(rhs.z);
			node.push_back(rhs.w);
			node.SetStyle(EmitterStyle::Flow);
			return node;
		}

		static bool decode(const Node& node, glm::vec4& rhs)
		{
			if (!node.IsSequence() || node.size() != 4)
				return false;

			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			rhs.z = node[2].as<float>();
			rhs.w = node[3].as<float>();
			return true;
		}
	};

	template<>
	struct convert<RXNEngine::UUID>
	{
		static Node encode(const RXNEngine::UUID& uuid)
		{
			Node node;
			node.push_back((uint64_t)uuid);
			return node;
		}

		static bool decode(const Node& node, RXNEngine::UUID& uuid)
		{
			uuid = node.as<uint64_t>();
			return true;
		}
	};

}

namespace RXNEngine {

	YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec2& v)
	{
		out << YAML::Flow;
		out << YAML::BeginSeq << v.x << v.y << YAML::EndSeq;
		return out;
	}

	YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec3& v)
	{
		out << YAML::Flow;
		out << YAML::BeginSeq << v.x << v.y << v.z << YAML::EndSeq;
		return out;
	}

	YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec4& v)
	{
		out << YAML::Flow;
		out << YAML::BeginSeq << v.x << v.y << v.z << v.w << YAML::EndSeq;
		return out;
	}

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

	static void SerializeEntity(YAML::Emitter& out, Entity entity)
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

			out << YAML::EndMap; // PointLightComponent
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
		// Not implemented
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
				uint64_t uuid = entity["Entity"].as<uint64_t>();

				std::string name;
				auto tagComponent = entity["TagComponent"];
				if (tagComponent)
					name = tagComponent["Tag"].as<std::string>();

				RXN_CORE_TRACE("Deserialized entity with ID = {0}, name = {1}", uuid, name);

				Entity deserializedEntity = m_Scene->CreateEntityWithUUID(uuid, name);

				auto transformComponent = entity["TransformComponent"];
				if (transformComponent)
				{
					// Entities always have transforms
					auto& tc = deserializedEntity.GetComponent<TransformComponent>();
					tc.Translation = transformComponent["Translation"].as<glm::vec3>();
					tc.Rotation = transformComponent["Rotation"].as<glm::vec3>();
					tc.Scale = transformComponent["Scale"].as<glm::vec3>();
				}

				auto relationshipComponent = entity["RelationshipComponent"];
				if (relationshipComponent)
				{
					auto& rc = deserializedEntity.GetComponent<RelationshipComponent>();
					rc.ParentHandle = relationshipComponent["ParentHandle"].as<uint64_t>();

					auto children = relationshipComponent["Children"];
					if (children)
					{
						for (auto child : children)
						{
							rc.Children.push_back(child.as<uint64_t>());
						}
					}
				}

				auto cameraComponent = entity["CameraComponent"];
				if (cameraComponent)
				{
					auto& cc = deserializedEntity.AddComponent<CameraComponent>();

					auto cameraProps = cameraComponent["Camera"];
					cc.Camera.SetProjectionType((SceneCamera::ProjectionMode)cameraProps["ProjectionType"].as<int>());

					cc.Camera.SetPerspectiveFOV(cameraProps["PerspectiveFOV"].as<float>());
					cc.Camera.SetPerspectiveNearClip(cameraProps["PerspectiveNear"].as<float>());
					cc.Camera.SetPerspectiveFarClip(cameraProps["PerspectiveFar"].as<float>());

					cc.Camera.SetOrthographicSize(cameraProps["OrthographicSize"].as<float>());
					cc.Camera.SetOrthographicNearClip(cameraProps["OrthographicNear"].as<float>());
					cc.Camera.SetOrthographicFarClip(cameraProps["OrthographicFar"].as<float>());

					cc.FixedAspectRatio = cameraComponent["FixedAspectRatio"].as<bool>();
				}

				auto staticMeshComponent = entity["StaticMeshComponent"];
				if (staticMeshComponent)
				{
					auto& mc = deserializedEntity.AddComponent<StaticMeshComponent>();

					std::string assetPath = staticMeshComponent["AssetPath"].as<std::string>();
					mc.AssetPath = assetPath;

					mc.Mesh = AssetManager::GetMesh(assetPath);

					if (staticMeshComponent["SubmeshIndex"])
						mc.SubmeshIndex = staticMeshComponent["SubmeshIndex"].as<uint32_t>();
					else
						mc.SubmeshIndex = 0;
				}

				auto directionalLightComponent = entity["DirectionalLightComponent"];
				if (directionalLightComponent)
				{
					auto& dlc = deserializedEntity.AddComponent<DirectionalLightComponent>();

					dlc.Color = directionalLightComponent["Color"].as<glm::vec3>();
					dlc.Intensity = directionalLightComponent["Intensity"].as<float>();
				}

				auto pointLightComponent = entity["PointLightComponent"];
				if (pointLightComponent)
				{
					auto& plc = deserializedEntity.AddComponent<PointLightComponent>();

					plc.Color = pointLightComponent["Color"].as<glm::vec3>();
					plc.Intensity = pointLightComponent["Intensity"].as<float>();
					plc.Radius = pointLightComponent["Radius"].as<float>();
					plc.Falloff = pointLightComponent["Falloff"].as<float>();
				}

				auto rigidbodyComponent = entity["RigidbodyComponent"];
				if (rigidbodyComponent)
				{
					auto& rb = deserializedEntity.AddComponent<RigidbodyComponent>();
					rb.Type = (RigidbodyComponent::BodyType)rigidbodyComponent["Type"].as<int>();
					rb.Mass = rigidbodyComponent["Mass"].as<float>();
					rb.LinearDrag = rigidbodyComponent["LinearDrag"].as<float>();
					rb.AngularDrag = rigidbodyComponent["AngularDrag"].as<float>();

					rb.FixedRotation = rigidbodyComponent["FixedRotation"].as<bool>();

					rb.UseCCD = rigidbodyComponent["UseCCD"].as<bool>();
					rb.CCDVelocityThreshold = rigidbodyComponent["CCDVelocityThreshold"].as<float>();
				}

				auto boxColliderComponent = entity["BoxColliderComponent"];
				if (boxColliderComponent)
				{
					auto& bc = deserializedEntity.AddComponent<BoxColliderComponent>();
					bc.HalfExtents = boxColliderComponent["HalfExtents"].as<glm::vec3>();
					bc.Offset = boxColliderComponent["Offset"].as<glm::vec3>();

					bc.StaticFriction = boxColliderComponent["StaticFriction"].as<float>();
					bc.DynamicFriction = boxColliderComponent["DynamicFriction"].as<float>();
					bc.Restitution = boxColliderComponent["Restitution"].as<float>();

					bc.IsTrigger = boxColliderComponent["IsTrigger"].as<bool>();
				}

				auto sphereColliderComponent = entity["SphereColliderComponent"];
				if (sphereColliderComponent)
				{
					auto& sc = deserializedEntity.AddComponent<SphereColliderComponent>();
					sc.Radius = sphereColliderComponent["Radius"].as<float>();
					sc.Offset = sphereColliderComponent["Offset"].as<glm::vec3>();

					sc.StaticFriction = sphereColliderComponent["StaticFriction"].as<float>();
					sc.DynamicFriction = sphereColliderComponent["DynamicFriction"].as<float>();
					sc.Restitution = sphereColliderComponent["Restitution"].as<float>();

					sc.IsTrigger = sphereColliderComponent["IsTrigger"].as<bool>();
				}

				auto capsuleColliderComponent = entity["CapsuleColliderComponent"];
				if (capsuleColliderComponent)
				{
					auto& cc = deserializedEntity.AddComponent<CapsuleColliderComponent>();
					cc.Radius = capsuleColliderComponent["Radius"].as<float>();
					cc.Height = capsuleColliderComponent["Height"].as<float>();
					cc.Offset = capsuleColliderComponent["Offset"].as<glm::vec3>();

					cc.StaticFriction = capsuleColliderComponent["StaticFriction"].as<float>();
					cc.DynamicFriction = capsuleColliderComponent["DynamicFriction"].as<float>();
					cc.Restitution = capsuleColliderComponent["Restitution"].as<float>();

					cc.IsTrigger = capsuleColliderComponent["IsTrigger"].as<bool>();
				}

				auto scriptComponent = entity["ScriptComponent"];
				if (scriptComponent)
				{
					auto& sc = deserializedEntity.AddComponent<ScriptComponent>();
					sc.ClassName = scriptComponent["ClassName"].as<std::string>();
				}

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
		// Not implemented
		RXN_CORE_ASSERT(false);
		return false;
	}

}