# Fly engine

This is a Vulkan game engine written in C++ initially made for the my game Fly.

## Requirements
- [CMake](https://cmake.org/) 
- [LunarG Vulkan SDK](https://vulkan.lunarg.com/) 

## How to use

1. In the project root folder, clone the engine repo.

2. Create a folder called `Game` in this directory.

3. Inside the `Game` folder, create a CMakeLists.txt with the following content: 
```
cmake_minimum_required(VERSION 3.20)

add_executable(game src/main.cpp)
target_link_libraries(game PRIVATE fly_engine)
```

4. In the project root folder create a CMakeLists.txt with the following content:
```
cmake_minimum_required(VERSION 3.20)

add_subdirectory(vulkan-engine)
add_subdirectory(Game)
```

5. Create folder `build` and run this commands:
```
cd build
cmake ..
cmake --build .
```

In the end you should have this project folder structure:
```
|
|--Engine
|  |- (The cloned repo)
|
|--Game
|  |--src (your game source code)
|  |- CMakeLists.txt
|
|- .gitignore
|- CMakeLists.txt
```
