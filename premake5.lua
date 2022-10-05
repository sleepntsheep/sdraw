-- premake5.lua
workspace "sdraw"
configurations { "Debug", "Release", "Mingw" }

newaction {
    trigger = "install",
    description = "install to path",
    execute = function ()
        os.copyfile("./Release/sdraw", "/usr/local/bin")
        os.copyfile("./sdraw.desktop", "/usr/local/share/applications/")
    end
}

project "sdraw"
kind "WindowedApp"
language "C"
targetdir "%{cfg.buildcfg}"

files { "*.h", "*.c" }

filter "configurations:Mingw"
system "Windows"
defines { "NDEBUG" }
optimize "On"
defines { "main=SDL_main" }
links { "mingw32", "SDL2main", "comdlg32", "ole32" }

filter "configurations:Debug"
defines { "DEBUG" }
symbols "On"

filter "configurations:Release"
defines { "NDEBUG" }
optimize "On"

filter "configurations:MinimalFont"

filter {}
defines { "_DEFAULT_SOURCE" }
buildoptions { "-std=c99", "-Wall", "-Wextra", "-pedantic", "-ggdb" }
links { "SDL2", "SDL2_ttf", "m", "fontconfig" }

