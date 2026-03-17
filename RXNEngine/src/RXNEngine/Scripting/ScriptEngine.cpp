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
        int rc = s_Data->LoadAssemblyAndGetFunctionPointer( \
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

    hostfxr_initialize_for_runtime_config_fn init_fptr;
    hostfxr_get_runtime_delegate_fn get_delegate_fptr;
    hostfxr_close_fn close_fptr;

    typedef void (CORECLR_DELEGATE_CALLTYPE* load_game_scripts_fn)(const char* corePath, const char* appPath);
    typedef void (CORECLR_DELEGATE_CALLTYPE* unload_game_scripts_fn)();
    typedef void (CORECLR_DELEGATE_CALLTYPE* register_internal_calls_fn)(void*);
    typedef void (CORECLR_DELEGATE_CALLTYPE* instantiate_script_fn)(uint64_t entityID, const char* className);
    typedef void (CORECLR_DELEGATE_CALLTYPE* invoke_on_create_fn)(uint64_t entityID);
    typedef void (CORECLR_DELEGATE_CALLTYPE* invoke_on_update_fn)(uint64_t entityID, float deltaTime);
    typedef int32_t(CORECLR_DELEGATE_CALLTYPE* entity_class_exists_fn)(const char* className);
    typedef void (CORECLR_DELEGATE_CALLTYPE* reflect_class_fn)(const char* className);
    typedef void (CORECLR_DELEGATE_CALLTYPE* get_field_value_fn)(uint64_t entityID, const char* fieldName, void* outBuffer);
    typedef void (CORECLR_DELEGATE_CALLTYPE* set_field_value_fn)(uint64_t entityID, const char* fieldName, const void* inBuffer);


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


    struct ScriptEngineData
    {
        load_assembly_and_get_function_pointer_fn LoadAssemblyAndGetFunctionPointer = nullptr;

        load_game_scripts_fn LoadGameScripts = nullptr;
        unload_game_scripts_fn UnloadGameScripts = nullptr;

        register_internal_calls_fn RegisterInternalCalls = nullptr;

        instantiate_script_fn InstantiateScript = nullptr;
        invoke_on_create_fn InvokeOnCreate = nullptr;
        invoke_on_update_fn InvokeOnUpdate = nullptr;

        entity_class_exists_fn CheckEntityClassExists = nullptr;

        reflect_class_fn ReflectClass = nullptr;
        get_field_value_fn GetFieldValue = nullptr;
        set_field_value_fn SetFieldValue = nullptr;

        Scene* SceneContext = nullptr;

        std::unordered_map<UUID, Ref<ScriptInstance>> EntityInstances;
        std::unordered_map<std::string, std::vector<ScriptField>> ScriptClassFields;

        std::filesystem::path CoreAssemblyPath;
        std::filesystem::file_time_type CoreAssemblyLastWriteTime;
        bool ReloadPending = false;
        float ReloadTimer = 0.0f;
    };

    static ScriptEngineData* s_Data = nullptr;

    void ScriptEngine::Init()
    {
        s_Data = new ScriptEngineData();

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

        s_Data->LoadAssemblyAndGetFunctionPointer = (load_assembly_and_get_function_pointer_fn)load_assembly_and_get_function_pointer;
        close_fptr(cxt);

        LOAD_MANAGED_METHOD("RXNScriptHost.Host, RXNScriptHost", LoadGameScripts, s_Data->LoadGameScripts);
        LOAD_MANAGED_METHOD("RXNScriptHost.Host, RXNScriptHost", UnloadGameScripts, s_Data->UnloadGameScripts);
        LOAD_MANAGED_METHOD("RXNScriptHost.Interop, RXNScriptHost", RegisterInternalCalls, s_Data->RegisterInternalCalls);
        LOAD_MANAGED_METHOD("RXNScriptHost.Host, RXNScriptHost", InstantiateScript, s_Data->InstantiateScript);
        LOAD_MANAGED_METHOD("RXNScriptHost.Host, RXNScriptHost", InvokeOnCreate, s_Data->InvokeOnCreate);
        LOAD_MANAGED_METHOD("RXNScriptHost.Host, RXNScriptHost", InvokeOnUpdate, s_Data->InvokeOnUpdate);
        LOAD_MANAGED_METHOD("RXNScriptHost.Host, RXNScriptHost", EntityClassExists, s_Data->CheckEntityClassExists);
        LOAD_MANAGED_METHOD("RXNScriptHost.Host, RXNScriptHost", ReflectClass, s_Data->ReflectClass);
        LOAD_MANAGED_METHOD("RXNScriptHost.Host, RXNScriptHost", GetFieldValue, s_Data->GetFieldValue);
        LOAD_MANAGED_METHOD("RXNScriptHost.Host, RXNScriptHost", SetFieldValue, s_Data->SetFieldValue);

        InternalCalls nativeFunctions;
        ScriptInterop::RegisterFunctions(&nativeFunctions);

        if (s_Data->RegisterInternalCalls)
        {
            s_Data->RegisterInternalCalls(&nativeFunctions);
        }

        LoadAssembly("res/scripts/RXNScriptApp.dll");
        RXN_CORE_INFO("CoreCLR Runtime initialized successfully!");
    }

    void ScriptEngine::Shutdown()
    {
        if (s_Data && s_Data->UnloadGameScripts)
            s_Data->UnloadGameScripts();

        delete s_Data;
    }

    void ScriptEngine::LoadAssembly(const std::string& appFilepath)
    {
        if (s_Data && s_Data->LoadGameScripts)
        {
            s_Data->UnloadGameScripts();

            std::filesystem::path appAbsolutePath = std::filesystem::absolute(appFilepath);
            std::filesystem::path coreAbsolutePath = std::filesystem::absolute("res/scripts/RXNScriptCore.dll");

            s_Data->CoreAssemblyPath = appAbsolutePath;
            s_Data->CoreAssemblyLastWriteTime = std::filesystem::last_write_time(appAbsolutePath);
            s_Data->ReloadPending = false;

            s_Data->LoadGameScripts(coreAbsolutePath.string().c_str(), appAbsolutePath.string().c_str());
        }
        else
        {
            RXN_CORE_ERROR("Cannot load assembly, C# Host functions are not bound!");
        }
    }

    void ScriptEngine::OnRuntimeStart(Scene* scene)
    {
        s_Data->SceneContext = scene;
        RXN_CORE_INFO("ScriptEngine: Runtime started, Scene context set.");
    }

    void ScriptEngine::OnRuntimeStop()
    {
        s_Data->SceneContext = nullptr;
        s_Data->EntityInstances.clear();
        RXN_CORE_INFO("ScriptEngine: Runtime stopped, Scene context cleared.");
    }

    void ScriptEngine::OnCreateEntity(Entity entity)
    {
        const auto& sc = entity.GetComponent<ScriptComponent>();
        if (sc.ClassName.empty()) return;

        if (EntityClassExists(sc.ClassName))
        {
            UUID entityID = entity.GetUUID();

            Ref<ScriptInstance> instance = CreateRef<ScriptInstance>(entity, sc.ClassName);

            s_Data->EntityInstances[entityID] = instance;

            instance->InvokeOnCreate();
        }
        else
        {
            RXN_CORE_ERROR("ScriptEngine: EntityClass '{0}' does not exist!", sc.ClassName);
        }
    }

    void ScriptEngine::OnUpdateEntity(Entity entity, float deltaTime)
    {
        UUID entityID = entity.GetUUID();

        if (s_Data->EntityInstances.find(entityID) != s_Data->EntityInstances.end())
            s_Data->EntityInstances[entityID]->InvokeOnUpdate(deltaTime);
    }

    Scene* ScriptEngine::GetSceneContext()
    {
        return s_Data->SceneContext;
    }

    Ref<ScriptInstance> ScriptEngine::GetEntityScriptInstance(UUID uuid)
    {
        if (s_Data->EntityInstances.find(uuid) != s_Data->EntityInstances.end())
            return s_Data->EntityInstances[uuid];
        
		return nullptr;
    }

    void ScriptEngine::ReloadAssembly()
    {
        s_Data->ScriptClassFields.clear();

        LoadAssembly("res/scripts/RXNScriptApp.dll");
        RXN_CORE_INFO("ScriptEngine: Assembly Hot-Reloaded successfully!");
    }

    void ScriptEngine::ReloadIfModified(float deltaTime)
    {
        if (s_Data->ReloadPending)
        {
            s_Data->ReloadTimer -= deltaTime;
            if (s_Data->ReloadTimer <= 0.0f)
            {
                s_Data->ReloadPending = false;
                ReloadAssembly();
            }
            return;
        }

        std::error_code ec;
        auto currentWriteTime = std::filesystem::last_write_time(s_Data->CoreAssemblyPath, ec);

        if (!ec && currentWriteTime > s_Data->CoreAssemblyLastWriteTime)
        {
            s_Data->ReloadPending = true;
            s_Data->ReloadTimer = 0.5f;
            RXN_CORE_TRACE("ScriptEngine: Assembly modification detected. Waiting for compiler...");
        }
    }

    void ScriptEngine::RegisterField(const std::string& className, const std::string& fieldName, ScriptFieldType type)
    {
        s_Data->ScriptClassFields[className].push_back({ type, fieldName });
    }

    const std::vector<ScriptField>& ScriptEngine::GetClassFields(const std::string& className)
    {
        return s_Data->ScriptClassFields[className];
    }

    bool ScriptEngine::EntityClassExists(const std::string& fullClassName)
    {
        if (!s_Data || !s_Data->CheckEntityClassExists || fullClassName.empty())
            return false;

        int32_t exists = s_Data->CheckEntityClassExists(fullClassName.c_str());
        if (exists != 0 && s_Data->ScriptClassFields.find(fullClassName) == s_Data->ScriptClassFields.end())
            s_Data->ReflectClass(fullClassName.c_str());

        return exists != 0;
    }




    ScriptInstance::ScriptInstance(Entity entity, const std::string& className)
        : m_Entity(entity), m_ClassName(className)
    {
        if (s_Data->InstantiateScript)
            s_Data->InstantiateScript(m_Entity.GetUUID(), className.c_str());
    }

    void ScriptInstance::InvokeOnCreate()
    {
        if (s_Data->InvokeOnCreate)
            s_Data->InvokeOnCreate(m_Entity.GetUUID());
    }

    void ScriptInstance::InvokeOnUpdate(float deltaTime)
    {
        if (s_Data->InvokeOnUpdate)
            s_Data->InvokeOnUpdate(m_Entity.GetUUID(), deltaTime);
    }

    bool ScriptInstance::GetFieldValueInternal(const std::string& name, void* outBuffer)
    {
        if (s_Data->GetFieldValue)
        {
            s_Data->GetFieldValue(m_Entity.GetUUID(), name.c_str(), outBuffer);
            return true;
        }
        return false;
    }

    void ScriptInstance::SetFieldValueInternal(const std::string& name, const void* inBuffer)
    {
        if (s_Data->SetFieldValue)
            s_Data->SetFieldValue(m_Entity.GetUUID(), name.c_str(), inBuffer);
    }
}
