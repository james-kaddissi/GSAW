cmake_minimum_required(VERSION 3.20)
project(GSAW LANGUAGES CXX)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(GS_ENGINE_ROOT "C:/Users/james/GSHub/gs-engine-shared")

add_subdirectory(${GS_ENGINE_ROOT} ${CMAKE_BINARY_DIR}/gs-engine-build)

add_executable(GSAW
    src/main.cpp
    src/components/BrowserPanel.cpp
    src/components/BrowserItem.cpp
    src/components/MixerView.cpp
    src/components/ChannelRackView.cpp
    src/components/EffectStrip.cpp
    src/components/StereoMeterElement.cpp
    src/audio/AudioEngine.cpp 
    src/audio/AudioLayer.cpp 
    src/audio/Channel.cpp 
    src/audio/ChannelRack.cpp 
    src/audio/EffectBinding.cpp 
    src/audio/Pattern.cpp 
    src/audio/PitchUtils.cpp 
    src/audio/library/effects/Distortion.cpp 
    src/audio/library/effects/GainPan.cpp 
    src/audio/library/generators/BagpipeSynth.cpp 
    src/audio/library/generators/SubtractiveSynth.cpp 
)
target_include_directories(GSAW PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${GS_ENGINE_ROOT}/include
)

target_link_libraries(GSAW PRIVATE
    GSEngine::GSEngine
)

if(WIN32)
    add_custom_target(copy_mydaw_runtime_deps ALL
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:GSAW>
            $<TARGET_FILE_DIR:GSAW>
        COMMAND_EXPAND_LISTS
    )
    add_dependencies(copy_mydaw_runtime_deps GSAW)
endif()