workspace "RXNEngine"

   language "C++"
   architecture "x64"
   startproject "Sandbox"
   configurations { "Debug", "Release", "Dist" }

project "RXNEngine"
   location "RXNEngine"
   kind "SharedLib"
   language "C++"

   targetdir "bin/%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}/%{prj.name}"
   objdir "bin-int/%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}/%{prj.name}"

   files { "%{prj.name}/src/**.h", "%{prj.name}/src/**.cpp" }
   includedirs { "%{prj.name}/src", "%{prj.name}/vendor/spdlog/include" }

   filter "system:windows"
      cppdialect "C++latest"
      staticruntime "On"
      systemversion "latest"
      defines { "RXN_PLATFORM_WINDOWS", "RXN_BUILD_DLL" }

      postbuildcommands { "{COPYFILE} %{cfg.buildtarget.relpath} ../bin/%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}/Sandbox"}

      filter "configurations:Debug"
        defines "RXN_DEBUG"
        symbols "On"

      filter "configurations:Release"
        defines "RXN_DEBUG"
        optimize "On"

      filter "configurations:Dist"
        defines "RXN_DEBUG"
        optimize "On"

project "Sandbox"
 location "RXNEngine"
   kind "ConsoleApp"
   language "C++"

   targetdir "bin/%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}/%{prj.name}"
   objdir "bin-int/%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}/%{prj.name}"

   files { "%{prj.name}/src/**.h", "%{prj.name}/src/**.cpp" }
   includedirs {"RXNEngine/vendor/spdlog/include", "RXNEngine/src"}
   links "RXNEngine"

   filter "system:windows"
      cppdialect "C++latest"
      staticruntime "On"
      systemversion "latest"
      defines "RXN_PLATFORM_WINDOWS"

   filter "configurations:Debug"
      defines "RXN_DEBUG"
      symbols "On"

   filter "configurations:Release"
      defines "RXN_DEBUG"
      optimize "On"

   filter "configurations:Dist"
      defines "RXN_DEBUG"
      optimize "On"