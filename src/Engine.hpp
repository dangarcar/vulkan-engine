#pragma once

#include "renderer/TGraphicsPipeline.hpp"
#include "renderer/TextRenderer.hpp"
#include "renderer/vulkan/VulkanTypes.h"
#include "renderer/ImGuiRenderer.hpp"
#include "renderer/Renderer.hpp"

#include "Window.hpp"
#include <algorithm>


namespace fly {
 
    class Texture;
    class TextureSampler;
    class ImGuiRenderer;
    class Engine;
    
    class Scene {
    public:
        virtual void init(Engine& engine) = 0;
        virtual void run(double dt, uint32_t currentFrame, Engine& engine) = 0;
        virtual ~Scene() {}
    };

    class Engine {
    public:
        Engine(int width, int height): window(width, height) {}
        ~Engine();
        
        void init();
        void run();
        
        template<typename T, typename ...Args>
        void setScene(Args&&... args) {
            static_assert(std::is_base_of<Scene, T>::value);
            this->scene = std::make_unique<T>(std::forward(args)...);
            this->scene->init(*this);
        }

        template<typename T>
        T* addPipeline(int priority) {
            static_assert(std::is_base_of<IGraphicsPipeline, T>::value);
            auto pip = std::make_unique<T>(this->vk);
            auto ptr = pip.get();
            pip->allocate(this->renderPass, this->msaaSamples);
            graphicPipelines.emplace_back(std::make_pair(priority, std::move(pip)));
            std::sort(graphicPipelines.begin(), graphicPipelines.end());
            return ptr;
        }
    
        Window& getWindow() { return this->window; }
        const Window& getWindow() const { return this->window; }
        const VulkanInstance& getVulkanInstance() const { return this->vk; } 
        VkCommandPool getCommandPool() const { return this->commandPool; }
        Renderer& getRenderer() { return *this->renderer; }
        TextRenderer& getTextRenderer() { return *this->textRenderer; }

    private:
        std::unique_ptr<Scene> scene;

        VulkanInstance vk;
        VkCommandPool commandPool;

        Window window;
        std::unique_ptr<ImGuiRenderer> imguiRenderer;
        std::unique_ptr<Renderer> renderer;
        std::unique_ptr<TextRenderer> textRenderer;

        uint32_t currentFrame = 0;
        
        VkDebugUtilsMessengerEXT debugMessenger;
        
        VkRenderPass renderPass;
        
        std::vector<VkFramebuffer> swapChainFramebuffers;
        std::vector<VkCommandBuffer> commandBuffers;
        
        std::vector<std::pair<int, std::unique_ptr<IGraphicsPipeline>>> graphicPipelines;


        VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;
        std::unique_ptr<Texture> colorTexture;
        std::unique_ptr<Texture> depthTexture;

        std::vector<VkSemaphore> imageAvailableSemaphores;
        std::vector<VkSemaphore> renderFinishedSemaphores;
        std::vector<VkFence> inFlightFences;
    
    private:
        void drawFrame();
        void cleanup();

        void recreateSwapChain();
        void cleanupSwapChain();
        void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

        void createInstance();
        void setupDebugMessenger();
        void pickPhysicalDevice();
        void createLogicalDevice();
        void createSwapChain();
        void createImageViews();
        void createRenderPass();
        void createColorAndDepthTextures();
        void createFramebuffers();
        void createSyncObjects();

        void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
        VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
        static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);
    };

}
    