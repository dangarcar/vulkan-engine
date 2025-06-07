cmake_minimum_required(VERSION 3.22)

include(FetchContent)

project(FlyEngineLibrary)

set(CMAKE_CXX_STANDARD 20)

find_package(Vulkan REQUIRED)

FetchContent_Declare(glfw GIT_REPOSITORY https://github.com/glfw/glfw.git GIT_TAG master)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(glm GIT_REPOSITORY https://github.com/g-truc/glm.git GIT_TAG master)
FetchContent_MakeAvailable(glm)

FetchContent_Declare(imgui_external GIT_REPOSITORY https://github.com/ocornut/imgui.git GIT_TAG master)
FetchContent_MakeAvailable(imgui_external)

#Thanks to u/Kakod123 on https://www.reddit.com/r/vulkan/comments/1h6ta10/include_ktx_lib_to_project/ for this snippet
message(NOTICE "Fetching LibKTX from https://github.com/KhronosGroup/KTX-Software ...")
set(KTX_FEATURE_STATIC_LIBRARY ON CACHE BOOL "Build KTX as a static library" FORCE)
FetchContent_Declare(
        fetch_ktx
        GIT_REPOSITORY https://github.com/KhronosGroup/KTX-Software
        GIT_TAG        v4.3.2
)
FetchContent_MakeAvailable(fetch_ktx)

add_library(imgui
	${imgui_external_SOURCE_DIR}/imgui.cpp
	${imgui_external_SOURCE_DIR}/imgui_draw.cpp
	${imgui_external_SOURCE_DIR}/imgui_tables.cpp
	${imgui_external_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_external_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_external_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
	${imgui_external_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)
target_link_libraries(imgui PUBLIC glfw Vulkan::Vulkan)
target_include_directories(imgui PUBLIC ${imgui_external_SOURCE_DIR})
	
include_directories(include)
include_directories(external/ktx/include)
include_directories(external/ktx/other_include)

file(GLOB_RECURSE sources *.cpp)
add_library(fly_engine STATIC ${sources})
target_link_libraries(fly_engine PUBLIC glfw ${GLFW_LIBRARIES} Vulkan::Vulkan glm imgui ktx)
target_include_directories(fly_engine PRIVATE ${fetch_ktx_SOURCE_DIR}/include)
target_include_directories(fly_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(MSVC)
	target_compile_options(fly_engine PUBLIC /W4 /WX)
else()
	target_compile_options(fly_engine PUBLIC -Wall -Wextra -Wpedantic -Werror)
endif()