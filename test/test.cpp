#include "renderer/DefaultPipeline.hpp"
#include <Engine.hpp>
#include <cstdint>
#include <iostream>
#include <imgui.h>
#include <memory>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

static const char* const IMAGE_SRC = "test/res/texture.jpg";
static const char* const VIKING_MODEL_PATH = "test/res/viking_room.obj";
static const char* const PLANE_MODEL_PATH = "test/res/plane.obj";

static const char* const PLANE_TEXTURE_PATH = "test/res/plane.jpg";
static const char* const VIKING_TEXTURE_PATH = "test/res/viking_room.png";


class Game: public fly::Scene {
public:
    Game() = default;
    ~Game() = default;
    
    void init(fly::Engine& engine) override {
        this->defaultPipeline = engine.addPipeline<fly::DefaultPipeline>();

        this->planeTexture = std::make_unique<fly::Texture>(
            engine.getVulkanInstance(), engine.getCommandPool(), std::filesystem::path(PLANE_TEXTURE_PATH), 
            fly::STB_Format::STBI_rgb_alpha, VK_FORMAT_R8G8B8A8_SRGB
        );
        this->planeSampler = std::make_unique<fly::TextureSampler>(engine.getVulkanInstance(), planeTexture->getMipLevels());
        
        this->vikingTexture = std::make_unique<fly::Texture>(
            engine.getVulkanInstance(), engine.getCommandPool(), std::filesystem::path(VIKING_TEXTURE_PATH), 
            fly::STB_Format::STBI_rgb_alpha, VK_FORMAT_R8G8B8A8_SRGB
        );
        this->vikingSampler = std::make_unique<fly::TextureSampler>(engine.getVulkanInstance(), vikingTexture->getMipLevels());

        this->uniformBuffer = std::make_unique<fly::TUniformBuffer<fly::DefaultUBO>>(engine.getVulkanInstance());

        auto planeIdx = defaultPipeline->attachModel(
            loadModel(engine.getVulkanInstance(), engine.getCommandPool(), std::filesystem::path(PLANE_MODEL_PATH))
        );
        this->defaultPipeline->updateDescriptorSet(
            planeIdx, 
            *uniformBuffer, 
            *planeTexture, 
            *planeSampler
        );
        
        auto vikingIdx = defaultPipeline->attachModel(
            loadModel(engine.getVulkanInstance(), engine.getCommandPool(), std::filesystem::path(VIKING_MODEL_PATH))
        );
        this->defaultPipeline->updateDescriptorSet(
            vikingIdx, 
            *uniformBuffer, 
            *vikingTexture, 
            *vikingSampler
        );
    }


    void run(double dt, uint32_t currentFrame, const fly::VulkanInstance& vk, const fly::Window& window) override {
        {
            ImGui::Begin("Debug");
            ImGui::ColorEdit4("Color", &myColor[0]);
            ImGui::SliderFloat("Gamma", &gamma, 0, 3);
            ImGui::End();
        }
    
        totalTime += dt;
        fly::DefaultUBO ubo{};
        ubo.model = glm::rotate(glm::mat4(1.0f), (float)totalTime * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.model = glm::scale(ubo.model, glm::vec3(0.5));
        ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.proj = glm::perspective(glm::radians(45.0f), vk.swapChainExtent.width / (float) vk.swapChainExtent.height, 0.1f, 10.0f);
    
        ubo.proj[1][1] *= -1; //GLM was designed for opengl, where the Y is inverted
    
        ubo.gamma = gamma;
    
        uniformBuffer->updateUBO(ubo, currentFrame);
    }
    
    

private:
    fly::DefaultPipeline* defaultPipeline = nullptr;

    std::unique_ptr<fly::TextureSampler> planeSampler, vikingSampler;
    std::unique_ptr<fly::Texture> planeTexture, vikingTexture;
    std::unique_ptr<fly::TUniformBuffer<fly::DefaultUBO>> uniformBuffer;

    float gamma = 1.0f;
    glm::vec4 myColor;
    double totalTime = 0;

};

int main() {
    fly::Engine engine(1280, 720);

    try {
        engine.init();

        engine.setScene<Game>();

        engine.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}