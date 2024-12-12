cmake_minimum_required(VERSION 3.27)
project(PostureBarMod)

set(CMAKE_CXX_STANDARD 23)

# Define the path to the folder where dll's go
set(DLL_DIR "O:/SteamLibrary/steamapps/common/ELDEN RING/Game/mods")

file(COPY ${CMAKE_SOURCE_DIR}/Resources DESTINATION ${CMAKE_BINARY_DIR})

add_library(PostureBarMod SHARED
        Source/Common.cpp
        Source/dllmain.cpp
        Source/PostureBarMod.cpp
        Source/Main/D3DRenderer.cpp
        Source/Main/Hooking.cpp
        Source/Main/Logger.cpp
        Source/Main/PostureBarUI.cpp
)
add_subdirectory(Source/Minhook)
add_subdirectory(Source/ImGui)

target_link_libraries(PostureBarMod PRIVATE minhook imgui imgui_backends d3d12 dxgi dxguid)

add_custom_command(TARGET PostureBarMod POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:PostureBarMod>" "${DLL_DIR}/"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Resources" "${DLL_DIR}/PostureBarResources"
        COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_BINARY_DIR}/posture_bar_mod_resources.stamp"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Config" "${DLL_DIR}/PostureBarResources"
        COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_BINARY_DIR}/posture_bar_configs.stamp"
)

add_custom_target(copy_resources ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Resources" "${DLL_DIR}/PostureBarResources"
        DEPENDS "${CMAKE_BINARY_DIR}/posture_bar_mod_resources.stamp"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Config" "${DLL_DIR}/PostureBarResources"
        DEPENDS "${CMAKE_BINARY_DIR}/posture_bar_configs.stamp"
)

#set_source_files_properties(resources.rc PROPERTIES LANGUAGE RC)
