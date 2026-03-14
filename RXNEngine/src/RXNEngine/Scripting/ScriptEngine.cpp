#include "rxnpch.h"
#include "ScriptEngine.h"

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

namespace RXNEngine {

    hostfxr_initialize_for_runtime_config_fn init_fptr;
    hostfxr_get_runtime_delegate_fn get_delegate_fptr;
    hostfxr_close_fn close_fptr;

    typedef void (CORECLR_DELEGATE_CALLTYPE* load_game_scripts_fn)(const char* assemblyPath);
    typedef void (CORECLR_DELEGATE_CALLTYPE* unload_game_scripts_fn)();
    typedef void (CORECLR_DELEGATE_CALLTYPE* register_internal_calls_fn)(void*);
    typedef void (CORECLR_DELEGATE_CALLTYPE* instantiate_script_fn)(uint64_t entityID, const char* className);
    typedef void (CORECLR_DELEGATE_CALLTYPE* invoke_on_create_fn)(uint64_t entityID);
    typedef void (CORECLR_DELEGATE_CALLTYPE* invoke_on_update_fn)(uint64_t entityID, float ts);
    typedef int32_t(CORECLR_DELEGATE_CALLTYPE* entity_class_exists_fn)(const char* className);


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

    struct InternalCalls
    {
        void* LogMessage = nullptr;
        void* Entity_GetTranslation = nullptr;
        void* Entity_SetTranslation = nullptr;
    };

    static bool LoadHostFxr()
    {
        // use nethost.lib's get_hostfxr_path() 
        STRING_TYPE hostfxrPath = STR("C:\\Program Files\\dotnet\\host\\fxr\\8.0.3\\hostfxr.dll");

        void* lib = LoadLibraryOS(hostfxrPath.c_str());
        if (!lib)
        {
            RXN_CORE_CRITICAL("Failed to load hostfxr.dll! Make sure .NET 8 SDK is installed.");
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

        Scene* SceneContext = nullptr;
    };

    static ScriptEngineData* s_Data = nullptr;

    extern "C" void CORECLR_DELEGATE_CALLTYPE NativeLogMessage(const char* message)
    {
        RXN_CORE_WARN("C# SAYS: {0}", message);
    }
    static glm::vec3 s_DummyTranslation = { 1.0f, 2.0f, 3.0f };

    extern "C" void CORECLR_DELEGATE_CALLTYPE NativeEntity_GetTranslation(uint64_t entityID, glm::vec3 * outTranslation)
    {
        Scene* scene = s_Data->SceneContext;
        RXN_CORE_ASSERT(scene, "No active scene during script execution!");

        Entity entity = scene->GetEntityByUUID(entityID);
        *outTranslation = entity.GetComponent<TransformComponent>().Translation;
    }

    extern "C" void CORECLR_DELEGATE_CALLTYPE NativeEntity_SetTranslation(uint64_t entityID, glm::vec3 * inTranslation)
    {
        Scene* scene = s_Data->SceneContext;
        RXN_CORE_ASSERT(scene, "No active scene during script execution!");

        Entity entity = scene->GetEntityByUUID(entityID);
        entity.GetComponent<TransformComponent>().Translation = *inTranslation;
    }


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

        int rc1 = s_Data->LoadAssemblyAndGetFunctionPointer(
            hostDllPath.c_str(),
            STR("RXNScriptHost.Host, RXNScriptHost"),
            STR("LoadGameScripts"),
            UNMANAGEDCALLERSONLY_METHOD,
            nullptr,
            (void**)&s_Data->LoadGameScripts);

        if (rc1 != 0 || s_Data->LoadGameScripts == nullptr)
            RXN_CORE_ERROR("Failed to extract LoadGameScripts! Error code: {0}", rc1);

        int rc2 = s_Data->LoadAssemblyAndGetFunctionPointer(
            hostDllPath.c_str(),
            STR("RXNScriptHost.Host, RXNScriptHost"),
            STR("UnloadGameScripts"),
            UNMANAGEDCALLERSONLY_METHOD,
            nullptr,
            (void**)&s_Data->UnloadGameScripts);

        if (rc2 != 0 || s_Data->UnloadGameScripts == nullptr)
            RXN_CORE_ERROR("Failed to extract UnloadGameScripts! Error code: {0}", rc2);

        int rc3 = s_Data->LoadAssemblyAndGetFunctionPointer(
            hostDllPath.c_str(),
            STR("RXNScriptHost.Host, RXNScriptHost"),
            STR("RegisterInternalCalls"),
            UNMANAGEDCALLERSONLY_METHOD,
            nullptr,
            (void**)&s_Data->RegisterInternalCalls);

        if (rc3 != 0 || s_Data->RegisterInternalCalls == nullptr)
            RXN_CORE_ERROR("Failed to extract RegisterInternalCalls! Error code: {0}", rc3);

        int rc4 = s_Data->LoadAssemblyAndGetFunctionPointer(
            hostDllPath.c_str(),
            STR("RXNScriptHost.Host, RXNScriptHost"),
            STR("InstantiateScript"),
            UNMANAGEDCALLERSONLY_METHOD,
            nullptr,
            (void**)&s_Data->InstantiateScript);

        if (rc4 != 0 || s_Data->InstantiateScript == nullptr)
            RXN_CORE_ERROR("Failed to extract InstantiateScript! Error code: {0}", rc4);

        int rc5 = s_Data->LoadAssemblyAndGetFunctionPointer(
            hostDllPath.c_str(),
            STR("RXNScriptHost.Host, RXNScriptHost"),
            STR("InvokeOnCreate"),
            UNMANAGEDCALLERSONLY_METHOD,
            nullptr,
            (void**)&s_Data->InvokeOnCreate);

        if (rc5 != 0 || s_Data->InvokeOnCreate == nullptr)
            RXN_CORE_ERROR("Failed to extract InvokeOnCreate! Error code: {0}", rc5);

        int rc6 = s_Data->LoadAssemblyAndGetFunctionPointer(
            hostDllPath.c_str(),
            STR("RXNScriptHost.Host, RXNScriptHost"),
            STR("InvokeOnUpdate"),
            UNMANAGEDCALLERSONLY_METHOD,
            nullptr,
            (void**)&s_Data->InvokeOnUpdate);

        if (rc6 != 0 || s_Data->InvokeOnUpdate == nullptr)
            RXN_CORE_ERROR("Failed to extract InvokeOnUpdate! Error code: {0}", rc6);

        int rc7 = s_Data->LoadAssemblyAndGetFunctionPointer(
            hostDllPath.c_str(),
            STR("RXNScriptHost.Host, RXNScriptHost"),
            STR("EntityClassExists"), // Our new C# method!
            UNMANAGEDCALLERSONLY_METHOD,
            nullptr,
            (void**)&s_Data->CheckEntityClassExists);

        if (rc7 != 0 || s_Data->CheckEntityClassExists == nullptr)
            RXN_CORE_ERROR("Failed to extract EntityClassExists! Error code: {0}", rc7);

        InternalCalls nativeFunctions;
        nativeFunctions.LogMessage = (void*)NativeLogMessage;
        nativeFunctions.Entity_GetTranslation = (void*)NativeEntity_GetTranslation;
        nativeFunctions.Entity_SetTranslation = (void*)NativeEntity_SetTranslation;

        if (s_Data->RegisterInternalCalls)
        {
            s_Data->RegisterInternalCalls(&nativeFunctions);
        }

        LoadAssembly("res/scripts/RXNScriptCore.dll");
        RXN_CORE_INFO("CoreCLR Runtime initialized successfully!");
    }

    void ScriptEngine::Shutdown()
    {
        if (s_Data && s_Data->UnloadGameScripts)
            s_Data->UnloadGameScripts();

        delete s_Data;
    }

    void ScriptEngine::LoadAssembly(const std::string& filepath)
    {
        if (s_Data && s_Data->LoadGameScripts)
        {
            s_Data->UnloadGameScripts();

            std::filesystem::path absolutePath = std::filesystem::absolute(filepath);
            std::string pathStr = absolutePath.string();

            s_Data->LoadGameScripts(pathStr.c_str());
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
        RXN_CORE_INFO("ScriptEngine: Runtime stopped, Scene context cleared.");
    }

    void ScriptEngine::OnCreateEntity(Entity entity)
    {
        const auto& sc = entity.GetComponent<ScriptComponent>();
        if (sc.ClassName.empty())
            return;

        UUID entityUUID = entity.GetUUID();

        if (s_Data->InstantiateScript)
            s_Data->InstantiateScript(entityUUID, sc.ClassName.c_str());

        if (s_Data->InvokeOnCreate)
            s_Data->InvokeOnCreate(entityUUID);
    }

    void ScriptEngine::OnUpdateEntity(Entity entity, float ts)
    {
        UUID entityUUID = entity.GetUUID();

        if (s_Data->InvokeOnUpdate)
            s_Data->InvokeOnUpdate(entityUUID, ts);
    }

    bool ScriptEngine::EntityClassExists(const std::string& fullClassName)
    {
        if (!s_Data || !s_Data->CheckEntityClassExists || fullClassName.empty())
            return false;

        int32_t exists = s_Data->CheckEntityClassExists(fullClassName.c_str());
        return exists != 0;
    }
}
