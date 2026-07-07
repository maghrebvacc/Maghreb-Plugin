cmake_minimum_required(VERSION 4.2)
project(Maghreb_Plugin)

set(CMAKE_CXX_STANDARD 14)

include_directories(include)
include_directories(include/nlohmann)
include_directories(src)

add_executable(Maghreb_Plugin
        .idea/inspectionProfiles/Project_Default.xml
        .idea/.gitignore
        .idea/discord.xml
        .idea/Maghreb-Plugin.iml
        .idea/modules.xml
        .idea/vcs.xml
        .idea/workspace.xml
        build/image.png
        build/MagPlugin.dll
        build/MagPlugin.exp
        build/MagPlugin.lib
        DATA/routes/GMMN.json
        DATA/stars/GCFV.json
        DATA/stars/GCLA.json
        DATA/stars/GCLP.json
        DATA/stars/GCRR.json
        DATA/stars/GCTS.json
        DATA/stars/GCXO.json
        DATA/stars/GMAD.json
        DATA/stars/GMFO.json
        DATA/stars/GMME.json
        DATA/stars/GMMN.json
        DATA/stars/GMMX.json
        DATA/stars/GMTT.json
        include/nlohmann/json.hpp
        include/curl.h
        include/EuroScopePlugIn.h
        include/TopskyFunctions.h
        src/IndraApcInterop.h
        src/plugin.cpp
        src/S-Mode/S-Mode.cpp
        src/S-Mode/S-Mode.h
        compile.bat
        EuroScope.ttf
        LICENSE
        Plugin.obj
        README.md
        src/AIRACChecker/Version.cpp
        src/AIRACChecker/Version.cpp)
