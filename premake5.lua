workspace "RXNEngine"

    language "C++"
    architecture "x64"
    startproject "RXNEditor"
    configurations { "Debug", "Release", "Dist" }

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

IncludeDir = {}
IncludeDir["SDL"] = "RXNEngine/vendor/SDL/include"
IncludeDir["Glad"] = "RXNEngine/vendor/Glad/include"
IncludeDir["Imgui"] = "RXNEngine/vendor/imgui"
IncludeDir["glm"] = "RXNEngine/vendor/glm"
IncludeDir["stb_image"] = "RXNEngine/vendor/stb_image"
IncludeDir["entt"] = "RXNEngine/vendor/entt/include"
IncludeDir["assimp"] = "RXNEngine/vendor/assimp/include"
IncludeDir["yaml_cpp"] = "RXNEngine/vendor/yaml-cpp/include"
IncludeDir["ImGuizmo"] = "RXNEngine/vendor/ImGuizmo"
IncludeDir["PhysX"] = "RXNEngine/vendor/PhysX/physx/include"
IncludeDir["PxShared"] = "RXNEngine/vendor/PhysX/pxshared/include"
IncludeDir["Tracy"] = "RXNEngine/vendor/tracy/public"
IncludeDir["CoreCLR"] = "RXNEngine/vendor/coreclr/include"
IncludeDir["pl_mpeg"] = "RXNEngine/vendor/pl_mpeg/include"
IncludeDir["CoACD"] = "RXNEngine/vendor/CoACD/public"
IncludeDir["miniaudio"] = "RXNEngine/vendor/miniaudio/include"
IncludeDir["FMODCore"] = "RXNEngine/vendor/fmod/api/core/inc"
IncludeDir["FMODStudio"] = "RXNEngine/vendor/fmod/api/studio/inc"

PhysXBinDir = "RXNEngine/vendor/PhysX/physx/bin/win.x86_64.vc143.md"

group "Dependencies"
    include "RXNEngine/vendor/Glad"
    include "RXNEngine/vendor/imgui"

group ""

project "RXNEngine"
    location "RXNEngine"
    kind "StaticLib"
    language "C++"
    cppdialect "C++latest"
    staticruntime "off"
   
    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")
   
    pchheader "rxnpch.h"
    pchsource "RXNEngine/src/rxnpch.cpp"
   
    files
    {
        "%{prj.name}/src/**.h",
        "%{prj.name}/src/**.cpp",
        "%{prj.name}/vendor/glm/glm/**.hpp",
        "%{prj.name}/vendor/glm/glm/**.inl",
        "%{prj.name}/vendor/glm/glm/**.h",
        "%{prj.name}/vendor/stb_image/**.h",
        "%{prj.name}/vendor/stb_image/**.cpp",
        "%{prj.name}/vendor/entt/include/**.hpp",
        "%{prj.name}/vendor/ImGuizmo/**.h",
        "%{prj.name}/vendor/ImGuizmo/*.cpp",
        "%{prj.name}/vendor/tracy/public/tracy/**.h",
        "%{prj.name}/vendor/tracy/public/TracyClient.cpp"
    }
   
    defines
    {
	    "_CRT_SECURE_NO_WARNINGS",
        "GLFW_INCLUDE_NONE",
        "_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING",
        "YAML_CPP_STATIC_DEFINE"
    }
   
    includedirs
    { 
        "%{prj.name}/src",
        "%{prj.name}/vendor/spdlog/include",
        "%{IncludeDir.SDL}",
        "%{IncludeDir.Glad}",
        "%{IncludeDir.Imgui}",
        "%{IncludeDir.glm}",
        "%{IncludeDir.stb_image}",
        "%{IncludeDir.entt}",
        "%{IncludeDir.assimp}",
        "%{prj.name}/vendor/assimp/build/include",
        "%{IncludeDir.yaml_cpp}",
        "%{IncludeDir.ImGuizmo}",
        "%{IncludeDir.PhysX}",
        "%{IncludeDir.PxShared}",
        "%{IncludeDir.Tracy}",
        "%{IncludeDir.CoreCLR}",
        "%{IncludeDir.pl_mpeg}",
        "%{IncludeDir.CoACD}",
        "%{IncludeDir.miniaudio}",
        "%{IncludeDir.FMODCore}",
        "%{IncludeDir.FMODStudio}"
    }

    links { "Glad", "Imgui", "opengl32.lib", "ws2_32.lib", "dbghelp.lib" }

    removefiles
    {
        "%{prj.name}/vendor/ImGuizmo/example/**"
    }

    filter "files:RXNEngine/vendor/ImGuizmo/*.cpp"
    flags { "NoPCH" }
    filter {}

    filter "files:RXNEngine/vendor/tracy/public/TracyClient.cpp"
        flags { "NoPCH" }
    filter {}

    filter "system:windows"
    systemversion "latest"
    defines { "GLFW_INCLUDE_NONE" }

    filter "configurations:Debug"
      defines "RXN_DEBUG"
      runtime "Debug"
      symbols "on"
      defines { "RXN_ENABLE_ASSERTS", "TRACY_ENABLE" }
      links 
      { 
          "RXNEngine/vendor/SDL/build/Debug/SDL3.lib",
          "RXNEngine/vendor/assimp/build/lib/Debug/assimp-vc143-mtd.lib",
          "RXNEngine/vendor/assimp/build/contrib/zlib/Debug/zlibstaticd.lib",
          "RXNEngine/vendor/yaml-cpp/build/Debug/yaml-cppd.lib",
          PhysXBinDir .. "/debug/PhysX_64.lib",
          PhysXBinDir .. "/debug/PhysXFoundation_64.lib",
          PhysXBinDir .. "/debug/PhysXCommon_64.lib",
          PhysXBinDir .. "/debug/PhysXExtensions_static_64.lib",
          PhysXBinDir .. "/debug/PhysXPvdSDK_static_64.lib",
          PhysXBinDir .. "/debug/PhysXCharacterKinematic_static_64.lib",
          PhysXBinDir .. "/debug/PhysXCooking_64.lib",
          "RXNEngine/vendor/CoACD/build/Debug/_coacd.lib",
          "RXNEngine/vendor/fmod/api/core/lib/x64/fmodL_vc.lib",
          "RXNEngine/vendor/fmod/api/studio/lib/x64/fmodstudioL_vc.lib"
      }
    filter "configurations:Release"
      defines { "RXN_RELEASE", "NDEBUG", "TRACY_ENABLE" }
      runtime "Release"
      optimize "on"
      links 
      { 
          "RXNEngine/vendor/SDL/build/Release/SDL3.lib",
          "RXNEngine/vendor/assimp/build/lib/Release/assimp-vc143-mt.lib",
          "RXNEngine/vendor/assimp/build/contrib/zlib/Release/zlibstatic.lib",
          "RXNEngine/vendor/yaml-cpp/build/Release/yaml-cpp.lib",
          PhysXBinDir .. "/release/PhysX_64.lib",
          PhysXBinDir .. "/release/PhysXFoundation_64.lib",
          PhysXBinDir .. "/release/PhysXCommon_64.lib",
          PhysXBinDir .. "/release/PhysXExtensions_static_64.lib",
          PhysXBinDir .. "/release/PhysXPvdSDK_static_64.lib",
          PhysXBinDir .. "/release/PhysXCharacterKinematic_static_64.lib",
          PhysXBinDir .. "/release/PhysXCooking_64.lib",
          "RXNEngine/vendor/CoACD/build/Release/_coacd.lib",
          "RXNEngine/vendor/fmod/api/core/lib/x64/fmod_vc.lib",
          "RXNEngine/vendor/fmod/api/studio/lib/x64/fmodstudio_vc.lib"
      }
    filter "configurations:Dist"
      defines { "RXN_DIST", "NDEBUG" }
      runtime "Release"
      optimize "on"
      links 
      { 
          "RXNEngine/vendor/SDL/build/Release/SDL3.lib",
          "RXNEngine/vendor/assimp/build/lib/Release/assimp-vc143-mt.lib",
          "RXNEngine/vendor/assimp/build/contrib/zlib/Release/zlibstatic.lib",
          "RXNEngine/vendor/yaml-cpp/build/Release/yaml-cpp.lib",
          PhysXBinDir .. "/release/PhysX_64.lib",
          PhysXBinDir .. "/release/PhysXFoundation_64.lib",
          PhysXBinDir .. "/release/PhysXCommon_64.lib",
          PhysXBinDir .. "/release/PhysXExtensions_static_64.lib",
          PhysXBinDir .. "/release/PhysXPvdSDK_static_64.lib",
          PhysXBinDir .. "/release/PhysXCharacterKinematic_static_64.lib",
          PhysXBinDir .. "/release/PhysXCooking_64.lib",
          "RXNEngine/vendor/CoACD/build/Release/_coacd.lib",
          "RXNEngine/vendor/fmod/api/core/lib/x64/fmod_vc.lib",
          "RXNEngine/vendor/fmod/api/studio/lib/x64/fmodstudio_vc.lib"
      }



project "RXNEditor"
    location "RXNEditor"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++latest"
    staticruntime "off"
    
    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")
    
    files
    { 
        "%{prj.name}/src/**.h",
        "%{prj.name}/src/**.cpp",
        "%{prj.name}/vendor/entt/include/**.hpp"
    }
    
    defines
    {
         "_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING"
    }
    
    includedirs
    {
        "RXNEngine/vendor/spdlog/include",
        "RXNEngine/src",
        "%{IncludeDir.glm}",
        "%{IncludeDir.entt}",
        "%{IncludeDir.Imgui}",
        "%{IncludeDir.assimp}",
        "RXNEngine/vendor/assimp/build/include",
        "%{IncludeDir.yaml_cpp}",
        "%{IncludeDir.ImGuizmo}",
        "%{IncludeDir.PhysX}",
        "%{IncludeDir.PxShared}",
        "%{IncludeDir.Tracy}"
    }
    links "RXNEngine"
    
    filter "system:windows"
       systemversion "latest"
    
    filter "configurations:Debug"
       defines { "RXN_DEBUG", "TRACY_ENABLE" }
       runtime "Debug"
       symbols "on"

       prebuildcommands
       {
           "dotnet build \"%{wks.location}/RXNScriptHost/RXNScriptHost.csproj\" -c Debug -o \"%{wks.location}/RXNEditor/res/scripts\"",
           "dotnet build \"%{wks.location}/RXNScriptCore/RXNScriptCore.csproj\" -c Debug -o \"%{wks.location}/RXNEditor/res/scripts\""
       }

       postbuildcommands
       {
           "{COPY} \"%{wks.location}/" .. PhysXBinDir .. "/debug/*.dll\" \"%{cfg.targetdir}\"",
           "{COPY} \"%{wks.location}/RXNEngine/vendor/SDL/build/Debug/*.dll\" \"%{cfg.targetdir}\"",
           "{COPY} \"%{wks.location}/RXNEngine/vendor/CoACD/build/Debug/lib_coacd.dll\" \"%{cfg.targetdir}\"",
           "{COPY} \"%{wks.location}/RXNEngine/vendor/fmod/api/core/lib/x64/fmodL.dll\" \"%{cfg.targetdir}\"",
           "{COPY} \"%{wks.location}/RXNEngine/vendor/fmod/api/studio/lib/x64/fmodstudioL.dll\" \"%{cfg.targetdir}\""
       }

    filter "configurations:Release"
       defines { "RXN_RELEASE", "TRACY_ENABLE" }
       runtime "Release"
       optimize "on"

       prebuildcommands
       {
           "dotnet build \"%{wks.location}/RXNScriptHost/RXNScriptHost.csproj\" -c Release -o \"%{wks.location}/RXNEditor/res/scripts\"",
           "dotnet build \"%{wks.location}/RXNScriptCore/RXNScriptCore.csproj\" -c Release -o \"%{wks.location}/RXNEditor/res/scripts\""
       }

       postbuildcommands
       {
           "{COPY} \"%{wks.location}/" .. PhysXBinDir .. "/release/*.dll\" \"%{cfg.targetdir}\"",
           "{COPY} \"%{wks.location}/RXNEngine/vendor/SDL/build/Release/*.dll\" \"%{cfg.targetdir}\"",
           "{COPY} \"%{wks.location}/RXNEngine/vendor/CoACD/build/Release/lib_coacd.dll\" \"%{cfg.targetdir}\"",
           "{COPY} \"%{wks.location}/RXNEngine/vendor/fmod/api/core/lib/x64/fmod.dll\" \"%{cfg.targetdir}\"",
           "{COPY} \"%{wks.location}/RXNEngine/vendor/fmod/api/studio/lib/x64/fmodstudio.dll\" \"%{cfg.targetdir}\""
       }

    filter "configurations:Dist"
       defines { "RXN_DIST" }
       runtime "Release"
       optimize "on"

       prebuildcommands
       {
           "dotnet build \"%{wks.location}/RXNScriptHost/RXNScriptHost.csproj\" -c Release -o \"%{wks.location}/RXNEditor/res/scripts\"",
           "dotnet build \"%{wks.location}/RXNScriptCore/RXNScriptCore.csproj\" -c Release -o \"%{wks.location}/RXNEditor/res/scripts\""
       }

       postbuildcommands
       {
           "{COPY} \"%{wks.location}/" .. PhysXBinDir .. "/release/*.dll\" \"%{cfg.targetdir}\"",
           "{COPY} \"%{wks.location}/RXNEngine/vendor/SDL/build/Release/*.dll\" \"%{cfg.targetdir}\"",
           "{COPY} \"%{wks.location}/RXNEngine/vendor/CoACD/build/Release/lib_coacd.dll\" \"%{cfg.targetdir}\"",
           "{COPY} \"%{wks.location}/RXNEngine/vendor/fmod/api/core/lib/x64/fmod.dll\" \"%{cfg.targetdir}\"",
           "{COPY} \"%{wks.location}/RXNEngine/vendor/fmod/api/studio/lib/x64/fmodstudio.dll\" \"%{cfg.targetdir}\""
       }

group "Scripting"
    externalproject "RXNScriptHost"
        location "RXNScriptHost"
        kind "SharedLib"
        language "C#"
        targetdir ("%{wks.location}/RXNEditor/res/scripts")

    externalproject "RXNScriptCore"
        location "RXNScriptCore"
        kind "SharedLib"
        language "C#"
        targetdir ("%{wks.location}/RXNEditor/res/scripts")

    externalproject "RXNScriptApp"
        location "RXNScriptApp"
        kind "SharedLib"
        language "C#"
        targetdir ("%{wks.location}/RXNEditor/res/scripts")
group ""