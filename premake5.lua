workspace "StuntFetcher"
    configurations { "Debug", "Release" }
	
	filter "action:vs*"
		buildoptions { "/Zc:__cplusplus" }
		platforms { "Win64" }
		startproject "StuntFetcher"
		
	filter "action:gmake*"
		platforms { "Linux64" }
	
	filter "Debug"
        defines { "DEBUG" }
        symbols "On"

    filter "Release"
        defines { "NDEBUG" }
        optimize "Speed"
		
	filter "platforms:*64"
		architecture "x86_64"
		
	filter "platforms:Win64" 
		system "windows"
		defines { "OS_WINDOWS" }
		links { "steam_api64" }
		
	filter "platforms:Linux64"
		system "linux"
		defines { "OS_UNIX" }
		links { "steam_api" }
		linkgroups "On"
	
	filter { "files:**.ixx" }
		compileas "Module"
		buildaction "ClCompile" -- thanks Premake for 30 minutes wasted
		
project "StuntFetcher"
    kind "ConsoleApp"
    language "C++"
	cppdialect "C++20"
    targetdir "bin/%{cfg.buildcfg}"
	includedirs { "libs", "libs/websocketpp", "libs/asio/asio/include", "libs/spdlog/include", "libs/steamworks" }
	libdirs { "libs" }
	characterset "Unicode"
	defines { "ASIO_STANDALONE", "SPDLOG_USE_STD_FORMAT" }

    files { "src/**.hpp", "src/**.h", "src/**.cpp", "src/**.ixx" }