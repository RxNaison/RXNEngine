workspace "RXNEngine"

   language "C++"
   architecture "x64"
   startproject "Sandbox"
   configurations { "Debug", "Release", "Dist" }

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

IncludeDir = {}
IncludeDir["GLFW"] = "RXNEngine/vendor/GLFW/include"

include "RXNEngine/vendor/GLFW"


project "RXNEngine"
   location "RXNEngine"
   kind "SharedLib"
   language "C++"
   staticruntime "On"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

   pchheader "rxnpch.h"
   pchsource "RXNEngine/src/rxnpch.cpp"

   files { "%{prj.name}/src/**.h", "%{prj.name}/src/**.cpp" }
   includedirs { "%{prj.name}/src", "%{prj.name}/vendor/spdlog/include", "%{IncludeDir.GLFW}" }
   links { "GLFW", "opengl32.lib", "dwmapi.lib" }

   filter "system:windows"
      cppdialect "C++latest"
      systemversion "latest"
      defines { "RXN_PLATFORM_WINDOWS", "RXN_BUILD_DLL" }

      postbuildcommands { "{COPYFILE} %{cfg.buildtarget.relpath} ../bin/" .. outputdir .. "/Sandbox" }

      filter "configurations:Debug"
        defines "RXN_DEBUG"
        runtime "Debug"
        symbols "On"

      filter "configurations:Release"
        defines "RXN_DEBUG"
        runtime "Release"
        optimize "On"

      filter "configurations:Dist"
        defines "RXN_DEBUG"
        runtime "Release"
        optimize "On"

project "Sandbox"
 location "RXNEngine"
   kind "ConsoleApp"
   language "C++"
   staticruntime "Off"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

   files { "%{prj.name}/src/**.h", "%{prj.name}/src/**.cpp" }
   includedirs {"RXNEngine/vendor/spdlog/include", "RXNEngine/src"}
   links "RXNEngine"

   filter "system:windows"
      cppdialect "C++latest"
      systemversion "latest"
      defines "RXN_PLATFORM_WINDOWS"

   filter "configurations:Debug"
      defines "RXN_DEBUG"
      runtime "Debug"
      symbols "On"

   filter "configurations:Release"
      defines "RXN_DEBUG"
      runtime "Release"
      optimize "On"

   filter "configurations:Dist"
      defines "RXN_DEBUG"
      runtime "Release"
      optimize "On"