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


## How to prepare image for the textures


For this engine it is recommended to create 2d textures or cubemaps with this fly::Texture constructor:
``` cpp
fly::Texture(const fly::VulkanInstance& vk, const VkCommandPool commandPool, std::filesystem::path ktxPath);
```

This constructor is valid for 2d and cubemaps, but they need to be in ktx2 format with bc7 compression and with the mipmaps included in the image.
To create files like that, you need to use the [KTX CLI](https://github.com/KhronosGroup/KTX-Software/releases) and run the following commands:

1. Create the ktx2 with uastc (needs transcoding to be sent to the GPU)
- For 2d images:
``` bash
ktx create --assign-tf srgb --encode uastc --generate-mipmap --format R8G8B8A8_SRGB {YOUR_IMAGE}.png {YOUR_TEXTURE}.ktx2 
```
- For cubemaps
``` bash
ktx create --assign-tf srgb --cubemap --encode uastc --generate-mipmap --format R8G8B8A8_SRGB px.png nx.png py.png ny.png pz.png nz.png {YOUR_TEXTURE}.ktx2
``` 

2. Convert uastc to bc7 (doesn't need transcoding to be sent to the GPU)
``` bash
ktx transcode --target bc7 {YOUR_TEXTURE}.ktx2 {YOUR_TEXTURE}.ktx2
```
