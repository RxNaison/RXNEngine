#include "rxnpch.h"
#include "ScriptEngine.h"

#include "ScriptInterop.h"
#include "RXNEngine/Scene/Entity.h"
#include "RXNEngine/Scene/Components.h"

#include <glm/glm.hpp>

#ifdef RXN_PLATFORM_WINDOWS
    #include <windows.h>
    #define STR(s) L ## s
    #define CHR(c) L ## c
    #define DIR_SEPARATOR L'\\'
    #define STRING_TYPE std::wstring
#else
    #include <dlfcn.h>
    #define STR(s) s
    #define CHR(c) c
    #define DIR_SEPARATOR '/'
    #define STRING_TYPE std::string
#endif

#include <coreclr_delegates.h>
#include <hostfxr.h>

#define LOAD_MANAGED_METHOD(ClassString, MethodName, FuncPtrVar) \
    { \
        int rc = m_Data->LoadAssemblyAndGetFunctionPointer( \
            hostDllPath.c_str(), \
            STR(ClassString), \
            STR(#MethodName), \
            UNMANAGEDCALLERSONLY_METHOD, \
            nullptr, \
            (void**)&(FuncPtrVar)); \
        if (rc != 0 || (FuncPtrVar) == nullptr) \
            RXN_CORE_ERROR("Failed to extract " #MethodName "! Error code: {0}", rc); \
    }

namespace RXNEngine {

#pragma region CoreCLR
    namespace {
        hostfxr_initialize_for_runtime_config_fn init_fptr;
        hostfxr_get_runtime_delegate_fn get_delegate_fptr;
        hostfxr_close_fn close_fptr;

        static void* LoadLibraryOS(const char_t* path)
        {
            #ifdef RXN_PLATFORM_WINDOWS
                return (void*)LoadLibraryW(path);
            #else
                return dlopen(path, RTLD_LAZY | RTLD_LOCAL);
            #endif
        }

        static void* GetExportOS(void* module, const char* name)
        {
            #ifdef RXN_PLATFORM_WINDOWS
                return (void*)GetProcAddress((HMODULE)module, name);
            #else
                return dlsym(module, name);
            #endif
        }

        static STRING_TYPE GetLatestHostFxrPath()
        {
            std::filesystem::path fxrRoot = "C:\\Program Files\\dotnet\\host\\fxr";

            if (!std::filesystem::exists(fxrRoot))
                return STR("");

            std::filesystem::path latestPath;
            int maxMajor = -1, maxMinor = -1, maxPatch = -1;

            for (const auto& entry : std::filesystem::directory_iterator(fxrRoot))
            {
                if (entry.is_directory())
                {
                    std::string folderName = entry.path().filename().string();
                    int major = 0, minor = 0, patch = 0;

                    if (std::sscanf(folderName.c_str(), "%d.%d.%d", &major, &minor, &patch) >= 2)
                    {
                        bool isNewer = false;
                        if (major > maxMajor) isNewer = true;
                        else if (major == maxMajor && minor > maxMinor) isNewer = true;
                        else if (major == maxMajor && minor == maxMinor && patch > maxPatch) isNewer = true;

                        if (isNewer)
                        {
                            maxMajor = major;
                            maxMinor = minor;
                            maxPatch = patch;
                            latestPath = entry.path() / "hostfxr.dll";
                        }
                    }
                }
            }

            #ifdef RXN_PLATFORM_WINDOWS
                return latestPath.wstring();
            #else
                return latestPath.string();
            #endif
        }

        static bool LoadHostFxr()
        {
            std::filesystem::path localPath = std::filesystem::current_path() / "dotnet" / "hostfxr.dll";
            STRING_TYPE hostfxrPath = localPath.wstring();

            void* lib = LoadLibraryOS(hostfxrPath.c_str());
            if (!lib)
            {
                hostfxrPath = GetLatestHostFxrPath();
                lib = LoadLibraryOS(hostfxrPath.c_str());
            }

            if (!lib)
            {
                RXN_CORE_CRITICAL("Failed to locate hostfxr.dll! Make sure the .NET SDK is installed.");
                return false;
            }

            init_fptr = (hostfxr_initialize_for_runtime_config_fn)GetExportOS(lib, "hostfxr_initialize_for_runtime_config");
            get_delegate_fptr = (hostfxr_get_runtime_delegate_fn)GetExportOS(lib, "hostfxr_get_runtime_delegate");
            close_fptr = (hostfxr_close_fn)GetExportOS(lib, "hostfxr_close");

            return (init_fptr && get_delegate_fptr && close_fptr);
        }
    }
#pragma endregion

#pragma region Engine Data State
    typedef void (CORECLR_DELEGATE_CALLTYPE* set_engine_time_fn)(float deltaTime);
    typedef void (CORECLR_DELEGATE_CALLTYPE* load_game_scripts_fn)(const char* corePath, const char* appPath);
    typedef void (CORECLR_DELEGATE_CALLTYPE* unload_game_scripts_fn)();
    typedef void (CORECLR_DELEGATE_CALLTYPE* register_internal_calls_fn)(void*);
    typedef void (CORECLR_DELEGATE_CALLTYPE* instantiate_script_fn)(uint64_t entityID, const char* className);
    typedef void (CORECLR_DELEGATE_CALLTYPE* invoke_on_create_fn)(uint64_t entityID);
    typedef void (CORECLR_DELEGATE_CALLTYPE* invoke_on_destroy_fn)(uint64_t entityID);
    typedef void (CORECLR_DELEGATE_CALLTYPE* invoke_on_update_fn)(uint64_t entityID, float deltaTime);
    typedef void (CORECLR_DELEGATE_CALLTYPE* invoke_on_fixed_update_fn)(uint64_t entityID, float deltaTime);
    typedef int32_t(CORECLR_DELEGATE_CALLTYPE* entity_class_exists_fn)(const char* className);
    typedef void (CORECLR_DELEGATE_CALLTYPE* reflect_class_fn)(const char* className);
    typedef void (CORECLR_DELEGATE_CALLTYPE* get_field_value_fn)(uint64_t entityID, const char* fieldName, void* outBuffer);
    typedef void (CORECLR_DELEGATE_CALLTYPE* set_field_value_fn)(uint64_t entityID, const char* fieldName, const void* inBuffer);
    typedef void (CORECLR_DELEGATE_CALLTYPE* on_collision_fn)(uint64_t entityID, uint64_t otherEntityID);
    typedef void (CORECLR_DELEGATE_CALLTYPE* clear_instances_fn)();


    struct ScriptEngineData
    {
        set_engine_time_fn SetEngineTime = nullptr;

        load_assembly_and_get_function_pointer_fn LoadAssemblyAndGetFunctionPointer = nullptr;
        load_game_scripts_fn LoadGameScripts = nullptr;
        unload_game_scripts_fn UnloadGameScripts = nullptr;
        register_internal_calls_fn RegisterInternalCalls = nullptr;

        instantiate_script_fn InstantiateScript = nullptr;
        invoke_on_create_fn InvokeOnCreate = nullptr;
        invoke_on_destroy_fn InvokeOnDestroy = nullptr;
        invoke_on_update_fn InvokeOnUpdate = nullptr;
		invoke_on_fixed_update_fn InvokeOnFixedUpdate = nullptr;

        entity_class_exists_fn CheckEntityClassExists = nullptr;
        reflect_class_fn ReflectClass = nullptr;
        get_field_value_fn GetFieldValue = nullptr;
        set_field_value_fn SetFieldValue = nullptr;

        on_collision_fn OnCollisionEnter = nullptr;
        on_collision_fn OnCollisionExit = nullptr;

        on_collision_fn OnTriggerEnter = nullptr;
        on_collision_fn OnTriggerExit = nullptr;

        clear_instances_fn ClearInstances = nullptr;

        Scene* SceneContext = nullptr;
        std::unordered_map<UUID, Ref<ScriptInstance>> EntityInstances;
        std::unordered_map<std::string, std::vector<ScriptField>> ScriptClassFields;

        std::filesystem::path CoreAssemblyPath;
        std::filesystem::file_time_type CoreAssemblyLastWriteTime;
        bool ReloadPending = false;
        float ReloadTimer = 0.0f;
    };
#pragma endregion

#pragma region Initialization & Shutdown
    void ScriptEngine::Init()
    {
        m_Data = new ScriptEngineData();

        if (!LoadHostFxr())
        {
            RXN_CORE_ASSERT(false, "Failed to initialize CoreCLR HostFxr!");
            return;
        }

        std::filesystem::path currentDir = std::filesystem::current_path();
        STRING_TYPE runtimeConfigPath = (currentDir/"res"/"scripts"/"RXNScriptHost.runtimeconfig.json").wstring();
        STRING_TYPE hostDllPath = (currentDir/"res"/"scripts"/"RXNScriptHost.dll").wstring();

        hostfxr_handle cxt = nullptr;
        int rc = init_fptr(runtimeConfigPath.c_str(), nullptr, &cxt);
        if (rc != 0 || cxt == nullptr)
        {
            RXN_CORE_CRITICAL("Init failed: {0}", rc);
            close_fptr(cxt);
            return;
        }

        void* load_assembly_and_get_function_pointer = nullptr;
        rc = get_delegate_fptr(cxt, hdt_load_assembly_and_get_function_pointer, &load_assembly_and_get_function_pointer);
        if (rc != 0 || load_assembly_and_get_function_pointer == nullptr)
            RXN_CORE_CRITICAL("Get delegate failed: {0}", rc);

        m_Data->LoadAssemblyAndGetFunctionPointer = (load_assembly_and_get_function_pointer_fn)load_assembly_and_get_function_pointer;
        close_fptr(cxt);

        LOAD_MANAGED_METHOD("RXNScriptHost.Host, RXNScriptHost", SetEngineTime, m_Data->SetEngineTime);
        LOAD_MANAGED_METHOD("RXNScriptHost.Host, RXNScriptHost", LoadGameScripts, m_Data->LoadGameScripts);
        LOAD_MANAGED_METHOD("RXNScriptHost.Host, RXNScriptHost", UnloadGameScripts, m_Data->UnloadGameScripts);
        LOAD_MANAGED_METHOD("RXNScriptHost.Interop, RXNScriptHost", RegisterInternalCalls, m_Data->RegisterInternalCalls);
        LOAD_MANAGED_METHOD("RXNScriptHost.Host, RXNScriptHost", InstantiateScript, m_Data->InstantiateScript);
        LOAD_MANAGED_METHOD("RXNScriptHost.Host, RXNScriptHost", InvokeOnCreate, m_Data->InvokeOnCreate);
        LOAD_MANAGED_METHOD("RXNScriptHost.Host, RXNScriptHost", InvokeOnDestroy, m_Data->InvokeOnDestroy);
        LOAD_MANAGED_METHOD("RXNScriptHost.Host, RXNScriptHost", InvokeOnUpdate, m_Data->InvokeOnUpdate);
        LOAD_MANAGED_METHOD("RXNScriptHost.Host, RXNScriptHost", InvokeOnFixedUpdate, m_Data->InvokeOnFixedUpdate);
        LOAD_MANAGED_METHOD("RXNScriptHost.Host, RXNScriptHost", EntityClassExists, m_Data->CheckEntityClassExists);
        LOAD_MANAGED_METHOD("RXNScriptHost.Host, RXNScriptHost", ReflectClass, m_Data->ReflectClass);
        LOAD_MANAGED_METHOD("RXNScriptHost.Host, RXNScriptHost", GetFieldValue, m_Data->GetFieldValue);
        LOAD_MANAGED_METHOD("RXNScriptHost.Host, RXNScriptHost", SetFieldValue, m_Data->SetFieldValue);
        LOAD_MANAGED_METHOD("RXNScriptHost.Host, RXNScriptHost", OnCollisionEnter, m_Data->OnCollisionEnter);
        LOAD_MANAGED_METHOD("RXNScriptHost.Host, RXNScriptHost", OnCollisionExit, m_Data->OnCollisionExit);
        LOAD_MANAGED_METHOD("RXNScriptHost.Host, RXNScriptHost", OnTriggerEnter, m_Data->OnTriggerEnter);
        LOAD_MANAGED_METHOD("RXNScriptHost.Host, RXNScriptHost", OnTriggerExit, m_Data->OnTriggerExit);
        LOAD_MANAGED_METHOD("RXNScriptHost.Host, RXNScriptHost", ClearInstances, m_Data->ClearInstances);

        InternalCalls nativeFunctions;
        ScriptInterop::RegisterFunctions(&nativeFunctions);

        if (m_Data->RegisterInternalCalls)
        {
            m_Data->RegisterInternalCalls(&nativeFunctions);
        }

        LoadAssembly("res/scripts/RXNScriptApp.dll");
        RXN_CORE_INFO("CoreCLR Runtime initialized successfully!");
    }

    void ScriptEngine::Update(float deltaTime)
    {
        ReloadIfModified(deltaTime);
    }

    void ScriptEngine::Shutdown()
    {
        if (m_Data && m_Data->UnloadGameScripts)
            m_Data->UnloadGameScripts();

        delete m_Data;
    }
#pragma endregion

#pragma region Assembly Loading & Hot Reloading
    void ScriptEngine::LoadAssembly(const std::string& appFilepath)
    {
        if (m_Data && m_Data->LoadGameScripts)
        {
            m_Data->UnloadGameScripts();

            std::filesystem::path appAbsolutePath = std::filesystem::absolute(appFilepath);
            std::filesystem::path coreAbsolutePath = std::filesystem::absolute("res/scripts/RXNScriptCore.dll");

            m_Data->CoreAssemblyPath = appAbsolutePath;
            m_Data->CoreAssemblyLastWriteTime = std::filesystem::last_write_time(appAbsolutePath);
            m_Data->ReloadPending = false;

            m_Data->LoadGameScripts(coreAbsolutePath.string().c_str(), appAbsolutePath.string().c_str());
        }
        else
        {
            RXN_CORE_ERROR("Cannot load assembly, C# Host functions are not bound!");
        }
    }

    void ScriptEngine::ReloadAssembly()
    {
        m_Data->ScriptClassFields.clear();

        LoadAssembly("res/scripts/RXNScriptApp.dll");
        RXN_CORE_INFO("ScriptEngine: Assembly Hot-Reloaded successfully!");
    }

    void ScriptEngine::ReloadIfModified(float deltaTime)
    {
        if (m_Data->ReloadPending)
        {
            m_Data->ReloadTimer -= deltaTime;
            if (m_Data->ReloadTimer <= 0.0f)
            {
                m_Data->ReloadPending = false;
                ReloadAssembly();
            }
            return;
        }

        std::error_code ec;
        auto currentWriteTime = std::filesystem::last_write_time(m_Data->CoreAssemblyPath, ec);

        if (!ec && currentWriteTime > m_Data->CoreAssemblyLastWriteTime)
        {
            m_Data->ReloadPending = true;
            m_Data->ReloadTimer = 0.5f;
            RXN_CORE_TRACE("ScriptEngine: Assembly modification detected. Waiting for compiler...");
        }
    }
#pragma endregion

#pragma region Runtime Context Management
    void ScriptEngine::OnRuntimeStart(Scene* scene)
    {
        m_Data->SceneContext = scene;
        RXN_CORE_INFO("ScriptEngine: Runtime started, Scene context set.");
    }

    void ScriptEngine::OnRuntimeStop()
    {
        m_Data->SceneContext = nullptr;
        m_Data->EntityInstances.clear();

        if (m_Data && m_Data->ClearInstances)
            m_Data->ClearInstances();

        RXN_CORE_INFO("ScriptEngine: Runtime stopped, Scene context cleared.");
    }

    void ScriptEngine::SetEngineTime(float deltaTime)
    {
		m_Data->SetEngineTime(deltaTime);
    }


    Scene* ScriptEngine::GetSceneContext()
    {
        return m_Data->SceneContext;
    }
#pragma endregion

#pragma region Entity Lifecycle
    void ScriptEngine::OnCreateEntity(Entity entity)
    {
        const auto& sc = entity.GetComponent<ScriptComponent>();

        if (sc.ClassName.empty())
            return;

        if (EntityClassExists(sc.ClassName))
        {
            UUID entityID = entity.GetUUID();

            Ref<ScriptInstance> instance = CreateRef<ScriptInstance>(this, entity, sc.ClassName);

            m_Data->EntityInstances[entityID] = instance;

            for (const auto& [name, fieldInstance] : sc.FieldInstances)
                instance->SetFieldValueInternal(name, fieldInstance.Data.data());

            instance->InvokeOnCreate();
        }
        else
        {
            RXN_CORE_ERROR("ScriptEngine: EntityClass '{0}' does not exist!", sc.ClassName);
        }
    }

    void ScriptEngine::OnDestroyEntity(Entity entity)
    {
        if (m_Data && m_Data->InvokeOnDestroy)
            m_Data->InvokeOnDestroy(entity.GetUUID());
    }

    void ScriptEngine::OnUpdateEntity(Entity entity, float deltaTime)
    {
        UUID entityID = entity.GetUUID();

        auto it = m_Data->EntityInstances.find(entityID);
        if (it != m_Data->EntityInstances.end())
            it->second->InvokeOnUpdate(deltaTime);
    }

    void ScriptEngine::OnFixedUpdateEntity(Entity entity, float deltaTime)
    {
        UUID entityID = entity.GetUUID();

        auto it = m_Data->EntityInstances.find(entityID);
        if (it != m_Data->EntityInstances.end())
            it->second->InvokeOnFixedUpdate(deltaTime);
    }
#pragma endregion

#pragma region Physics Events
    void ScriptEngine::OnCollisionEnter(uint64_t entityID, uint64_t otherID)
    {
        if (m_Data && m_Data->OnCollisionEnter)
            m_Data->OnCollisionEnter(entityID, otherID);
    }

    void ScriptEngine::OnCollisionExit(uint64_t entityID, uint64_t otherID)
    {
        if (m_Data && m_Data->OnCollisionExit)
            m_Data->OnCollisionExit(entityID, otherID);
    }

    void ScriptEngine::OnTriggerEnter(uint64_t entityID, uint64_t otherID)
    {
        if (m_Data && m_Data->OnTriggerEnter)
            m_Data->OnTriggerEnter(entityID, otherID);
    }
    void ScriptEngine::OnTriggerExit(uint64_t entityID, uint64_t otherID)
    {
        if (m_Data && m_Data->OnTriggerExit)
            m_Data->OnTriggerExit(entityID, otherID);
    }
#pragma endregion

#pragma region Reflection
    void ScriptEngine::RegisterField(const std::string& className, const std::string& fieldName, ScriptFieldType type)
    {
        m_Data->ScriptClassFields[className].push_back({ type, fieldName });
    }

    const std::vector<ScriptField>& ScriptEngine::GetClassFields(const std::string& className)
    {
        return m_Data->ScriptClassFields[className];
    }

    bool ScriptEngine::EntityClassExists(const std::string& fullClassName)
    {
        if (!m_Data || !m_Data->CheckEntityClassExists || fullClassName.empty())
            return false;

        int32_t exists = m_Data->CheckEntityClassExists(fullClassName.c_str());
        if (exists != 0 && m_Data->ScriptClassFields.find(fullClassName) == m_Data->ScriptClassFields.end())
            m_Data->ReflectClass(fullClassName.c_str());

        return exists != 0;
    }

    Ref<ScriptInstance> ScriptEngine::GetEntityScriptInstance(UUID uuid)
    {
        if (m_Data->EntityInstances.find(uuid) != m_Data->EntityInstances.end())
            return m_Data->EntityInstances[uuid];

        return nullptr;
    }
#pragma endregion

#pragma region ScriptInstance Implementation
    ScriptInstance::ScriptInstance(ScriptEngine* engine, Entity entity, const std::string& className)
        : m_Engine(engine), m_Entity(entity), m_ClassName(className)
    {
        auto data = m_Engine->GetData();
        if (data->InstantiateScript)
            data->InstantiateScript(m_Entity.GetUUID(), className.c_str());
    }

    void ScriptInstance::InvokeOnCreate()
    {
        auto data = m_Engine->GetData();

        if (data->InvokeOnCreate)
            data->InvokeOnCreate(m_Entity.GetUUID());
    }

    void ScriptInstance::InvokeOnUpdate(float deltaTime)
    {
        auto data = m_Engine->GetData();

        if (data->InvokeOnUpdate)
            data->InvokeOnUpdate(m_Entity.GetUUID(), deltaTime);
    }

    void ScriptInstance::InvokeOnFixedUpdate(float deltaTime)
    {
        auto data = m_Engine->GetData();

        if (data->InvokeOnFixedUpdate)
            data->InvokeOnFixedUpdate(m_Entity.GetUUID(), deltaTime);
    }

    bool ScriptInstance::GetFieldValueInternal(const std::string& name, void* outBuffer)
    {
        auto data = m_Engine->GetData();

        if (data->GetFieldValue)
        {
            data->GetFieldValue(m_Entity.GetUUID(), name.c_str(), outBuffer);
            return true;
        }
        return false;
    }

    void ScriptInstance::SetFieldValueInternal(const std::string& name, const void* inBuffer)
    {
        auto data = m_Engine->GetData();

        if (data->SetFieldValue)
            data->SetFieldValue(m_Entity.GetUUID(), name.c_str(), inBuffer);
    }
#pragma endregion

}
