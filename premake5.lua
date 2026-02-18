workspace "RXNEngine"

    language "C++"
    architecture "x64"
    startproject "RXNEditor"
    configurations { "Debug", "Release", "Dist" }

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

IncludeDir = {}
IncludeDir["GLFW"] = "RXNEngine/vendor/GLFW/include"
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

PhysXBinDir = "RXNEngine/vendor/PhysX/physx/bin/win.x86_64.vc143.md"

group "Dependencies"
    include "RXNEngine/vendor/GLFW"
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
        "%{prj.name}/vendor/ImGuizmo/*.cpp"
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
        "%{IncludeDir.GLFW}",
        "%{IncludeDir.Glad}",
        "%{IncludeDir.Imgui}",
        "%{IncludeDir.glm}",
        "%{IncludeDir.stb_image}",
        "%{IncludeDir.entt}",
        "%{IncludeDir.assimp}",
        "%{IncludeDir.yaml_cpp}",
        "%{IncludeDir.ImGuizmo}",
        "%{IncludeDir.PhysX}",
        "%{IncludeDir.PxShared}"
    }


    links { "GLFW", "Glad", "Imgui", "opengl32.lib" }

    removefiles
    {
        "RXNEngine/vendor/ImGuizmo/example/**"
    }

    filter "files:RXNEngine/vendor/ImGuizmo/*.cpp"
    flags { "NoPCH" }
    filter {}

    filter "system:windows"
    systemversion "latest"
    defines { "GLFW_INCLUDE_NONE" }

    filter "configurations:Debug"
      defines "RXN_DEBUG"
      runtime "Debug"
      symbols "on"
      defines { "RXN_ENABLE_ASSERTS" }
      links 
      { 
          "RXNEngine/vendor/assimp/build/lib/Debug/assimp-vc143-mtd.lib",
          "RXNEngine/vendor/assimp/build/contrib/zlib/Debug/zlibstaticd.lib",
          "RXNEngine/vendor/yaml-cpp/build/Debug/yaml-cppd.lib",
          PhysXBinDir .. "/debug/PhysX_64.lib",
          PhysXBinDir .. "/debug/PhysXFoundation_64.lib",
          PhysXBinDir .. "/debug/PhysXCommon_64.lib",
          PhysXBinDir .. "/debug/PhysXExtensions_static_64.lib",
          PhysXBinDir .. "/debug/PhysXPvdSDK_static_64.lib",
          PhysXBinDir .. "/debug/PhysXCharacterKinematic_static_64.lib"
      }
    filter "configurations:Release"
      defines "RXN_RELEASE"
      runtime "Release"
      optimize "on"
      links 
      { 
          "RXNEngine/vendor/assimp/build/lib/Release/assimp-vc143-mt.lib",
          "RXNEngine/vendor/assimp/build/contrib/zlib/Release/zlibstatic.lib",
          "RXNEngine/vendor/yaml-cpp/build/Release/yaml-cpp.lib",
          PhysXBinDir .. "/release/PhysX_64.lib",
          PhysXBinDir .. "/release/PhysXFoundation_64.lib",
          PhysXBinDir .. "/release/PhysXCommon_64.lib",
          PhysXBinDir .. "/release/PhysXExtensions_static_64.lib",
          PhysXBinDir .. "/release/PhysXPvdSDK_static_64.lib",
          PhysXBinDir .. "/release/PhysXCharacterKinematic_static_64.lib"
      }
    filter "configurations:Dist"
      defines "RXN_DIST"
      runtime "Release"
      optimize "on"
      links 
      { 
          "RXNEngine/vendor/assimp/build/lib/Release/assimp-vc143-mt.lib",
          "RXNEngine/vendor/assimp/build/contrib/zlib/Release/zlibstatic.lib",
          "RXNEngine/vendor/yaml-cpp/build/Release/yaml-cpp.lib",
          PhysXBinDir .. "/release/PhysX_64.lib",
          PhysXBinDir .. "/release/PhysXFoundation_64.lib",
          PhysXBinDir .. "/release/PhysXCommon_64.lib",
          PhysXBinDir .. "/release/PhysXExtensions_static_64.lib",
          PhysXBinDir .. "/release/PhysXPvdSDK_static_64.lib",
          PhysXBinDir .. "/release/PhysXCharacterKinematic_static_64.lib"
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
        "%{IncludeDir.yaml_cpp}",
        "%{IncludeDir.ImGuizmo}",
        "%{IncludeDir.PhysX}",
        "%{IncludeDir.PxShared}"
    }
    links "RXNEngine"
    
    filter "system:windows"
       systemversion "latest"
    
    filter "configurations:Debug"
       defines "RXN_DEBUG"
       runtime "Debug"
       symbols "on"
       postbuildcommands
       {
           "{COPY} \"%{wks.location}/" .. PhysXBinDir .. "/debug/*.dll\" \"%{cfg.targetdir}\""
       }

    filter "configurations:Release or Dist"
       defines "RXN_RELEASE"
       runtime "Release"
       optimize "on"
       postbuildcommands
       {
           "{COPY} \"%{wks.location}/" .. PhysXBinDir .. "/release/*.dll\" \"%{cfg.targetdir}\""
       }