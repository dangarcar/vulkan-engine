#pragma once

#include "Window.hpp"
#include "renderer/ui/UIRenderer.hpp"
#include "renderer/FilterPipeline.hpp"

#include <map>

namespace fly {

    class Texture;
    class TextureSampler;
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
        T* addPipeline(bool background = false) {
            static_assert(std::is_base_of<IGraphicsPipeline, T>::value);
            auto pip = std::make_unique<T>(this->vk);
            auto ptr = pip.get();
            pip->allocate(this->renderPass, this->msaaSamples);
            if(background)
                graphicPipelines.insert(graphicPipelines.begin(), std::move(pip));
            else
                graphicPipelines.emplace_back(std::move(pip));
            return ptr;
        }
    
        template<typename T>
        uint64_t addFilter() {
            static_assert(std::is_base_of<FilterPipeline, T>::value);
            auto filter = std::make_unique<T>(vk);
            filter->allocate();
            this->filters.insert(std::make_pair(this->globalFilterId, std::move(filter)));
            return this->globalFilterId++;
        }

        void removeFilter(uint64_t filterId);
        void removeFilters();

        Window& getWindow() { return this->window; }
        const Window& getWindow() const { return this->window; }
        const VulkanInstance& getVulkanInstance() const { return this->vk; } 
        VkCommandPool getCommandPool() const { return this->commandPool; }
        UIRenderer& getUIRenderer() { return *this->uiRenderer; }

    private:
        std::unique_ptr<Scene> scene;

        VulkanInstance vk;
        VkCommandPool commandPool;

        Window window;
        std::unique_ptr<UIRenderer> uiRenderer;

        uint32_t currentFrame = 0;
        
        VkDebugUtilsMessengerEXT debugMessenger;
        
        VkRenderPass renderPass;
        
        std::vector<VkFramebuffer> swapChainFramebuffers;
        std::vector<VkCommandBuffer> commandBuffers, computeCommandBuffers;
        
        std::vector<std::unique_ptr<IGraphicsPipeline>> graphicPipelines;

        VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;
        VkFormat hdrFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
        std::unique_ptr<Texture> msaaColorTexture, hdrColorTexture;
        std::unique_ptr<Texture> depthTexture;

        std::vector<VkSemaphore> imageAvailableSemaphores;
        std::vector<VkSemaphore> renderPassFinishedSemaphores;
        std::vector<VkSemaphore> computePassFinishedSemaphores;
        std::vector<VkFence> inFlightFences;

        std::unique_ptr<TonemapFilter> tonemapFilter;
        struct FilterDetachInfo { 
            std::unique_ptr<FilterPipeline> pipeline; 
            uint32_t frame; 
        };
        std::map<uint64_t, std::unique_ptr<FilterPipeline>> filters;
        std::queue<FilterDetachInfo> filterDetachPending;
        uint64_t globalFilterId = 0;

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
        
        void applyFilters(VkCommandBuffer commandBuffer, VkImage swapchainImage);

        void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
        VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
        static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);
    };

}
    