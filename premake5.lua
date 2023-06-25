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
		debugdir "bin/Debug"

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
		
project "StuntFetcher"
    kind "ConsoleApp"
    language "C++"
	cppdialect "C++17"
    targetdir "bin/%{cfg.buildcfg}"
	includedirs { "libs", "libs/websocketpp", "libs/asio/asio/include", "libs/spdlog/include", "libs/steamworks/sdk/public", "libs/tomlplusplus/include" }
	libdirs { "libs" }
	characterset "Unicode"
	defines { "ASIO_STANDALONE" }

    files { "src/**.hpp", "src/**.h", "src/**.cpp", "src/**.ixx" }