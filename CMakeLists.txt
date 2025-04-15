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

include_directories(include)
file(GLOB sources src/*.cpp)
add_library(engine STATIC ${sources})
target_link_libraries(engine PUBLIC glfw ${GLFW_LIBRARIES} Vulkan::Vulkan glm::glm)
target_include_directories(engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Test executable
add_executable(EngineTest test/test.cpp)
target_link_libraries(EngineTest engine)