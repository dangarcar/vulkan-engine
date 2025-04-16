cmake_minimum_required(VERSION 3.20)

include(FetchContent)

project(EngineLibrary)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_TYPE Debug)

find_package(Vulkan REQUIRED)

FetchContent_Declare(glfw GIT_REPOSITORY https://github.com/glfw/glfw.git GIT_TAG master)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(glm GIT_REPOSITORY https://github.com/g-truc/glm.git GIT_TAG master)
FetchContent_MakeAvailable(glm)

FetchContent_Declare(imgui_external GIT_REPOSITORY https://github.com/ocornut/imgui.git GIT_TAG master)
FetchContent_MakeAvailable(imgui_external)

add_library(imgui
	${imgui_external_SOURCE_DIR}/imgui.cpp
	${imgui_external_SOURCE_DIR}/imgui_draw.cpp
	${imgui_external_SOURCE_DIR}/imgui_tables.cpp
	${imgui_external_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_external_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_external_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
	${imgui_external_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)
target_link_libraries(imgui PRIVATE glfw Vulkan::Vulkan)
target_include_directories(imgui PUBLIC ${imgui_external_SOURCE_DIR})
	
include_directories(include)
file(GLOB sources src/*.cpp)
add_library(engine STATIC ${sources})
target_link_libraries(engine PUBLIC glfw ${GLFW_LIBRARIES} Vulkan::Vulkan glm imgui)
target_include_directories(engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Test executable
add_executable(EngineTest test/test.cpp)
target_link_libraries(EngineTest engine)