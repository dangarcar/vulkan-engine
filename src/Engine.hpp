#pragma once

#include "Window.hpp"
#include "renderer/ui/UIRenderer.hpp"
#include "renderer/FilterPipeline.hpp"
#include "renderer/DeferredShader.hpp"


#include <map>
#include <future>

namespace fly {

    class Scene {
    public:
        virtual void init(VkCommandPool commandPool) = 0;
        virtual void run(double dt, uint32_t currentFrame, VkCommandPool commandPool) = 0;
        virtual ~Scene() {}

        //Should be static, but there is no static virtual 
        virtual std::unique_ptr<DeferredShader> getDeferredShader(std::shared_ptr<VulkanInstance> vk) = 0;
    };

    struct EngineCreateInfo {
        const char* name;
        bool fullscreen = true;
        int width = 0, height = 0;
    };

    class Engine {
    private:
        static inline const char* ENGINE_NAME = "Fly Engine";
        static inline constexpr uint32_t ENGINE_VERSION = VK_MAKE_VERSION(0, 1, 0);
    
        inline static Engine* instance = nullptr;
        inline static std::mutex instanceMtx = {};
    
    public:
        static Engine& get() {
            return *Engine::instance;
        }

        //Creates a fullscreen engine
        Engine(const char* name): name(name), window(name) { 
            std::unique_lock<std::mutex> lock(Engine::instanceMtx);
            
            FLY_ASSERT(Engine::instance == nullptr, "Engine already exists!");
            instance = this;
        }
        //Creates a windowed engine with given width and height
        Engine(const char* name, int width, int height): name(name), window(name, width, height) { 
            std::unique_lock<std::mutex> lock(Engine::instanceMtx);
            
            FLY_ASSERT(Engine::instance == nullptr, "Engine already exists!");
            instance = this;
        }
        
        ~Engine();

        void init();
        void run();
        
        template<typename T, typename ...Args>
        requires(std::is_base_of<Scene, T>::value)
        void setScene(Args&&... args) {
            this->nextScene = std::make_unique<T>(std::forward<Args>(args)...);
            startNextSceneLoading();
        }

        template<typename T>
        requires(std::is_base_of<IGraphicsPipeline, T>::value)
        T* addPipeline(bool background = false) {
            auto pip = std::make_unique<T>(this->vk);
            auto ptr = pip.get();
            pip->allocate(this->renderPass);
            if(background)
                nextGraphicsPipelines.insert(nextGraphicsPipelines.begin(), std::move(pip));
            else
                nextGraphicsPipelines.emplace_back(std::move(pip));
            return ptr;
        }
    
        template<typename T>
        requires(std::is_base_of<FilterPipeline, T>::value)
        uint64_t addFilter() {
            auto filter = std::make_unique<T>(vk);
            filter->allocate();
            filter->createResources();
            this->nextFilters.insert(std::make_pair(this->globalFilterId, std::move(filter)));
            return this->globalFilterId++;
        }

        template<typename T>
        requires(std::is_base_of<FilterPipeline, T>::value)
        T& getFilter(uint64_t filterId) {
            auto ptr = dynamic_cast<T*>(this->filters[filterId].get());
            if(ptr == nullptr) 
                throw std::runtime_error(std::format("There is no filter with id {}", filterId));
            return *ptr;
        }


        template<typename T>
        requires(std::is_base_of<DeferredShader, T>::value)
        T& getDeferredShader() {
            auto ptr = dynamic_cast<T*>(this->deferredShader.get());
            if(ptr == nullptr) 
                throw std::runtime_error(std::format("There is no deferred shader with the type given"));
            return *ptr;
        }

        Tonemapper& getTonemapper() { return *this->tonemapper; }
        void removeFilter(uint64_t filterId);
        void removeFilters();

        Window& getWindow() { return this->window; }
        const Window& getWindow() const { return this->window; }
        std::shared_ptr<VulkanInstance> getVulkanInstance() const { return this->vk; } 
        UIRenderer& getUIRenderer() { return *this->uiRenderer; }
        uint32_t getPickingValue() const { return *static_cast<uint32_t*>(pickingCPUBufferInfo.pMappedData); }

    private:
        const char* name;
        std::unique_ptr<Scene> scene, nextScene;
        std::future<VkResult> nextSceneReady;

        std::shared_ptr<VulkanInstance> vk;
        VkCommandPool drawCommandPool, transferCommandPool;

        Window window;
        std::unique_ptr<UIRenderer> uiRenderer;

        uint32_t currentFrame = 0;
        VkDebugUtilsMessengerEXT debugMessenger;

        VkRenderPass renderPass;
        
        std::vector<VkFramebuffer> swapChainFramebuffers;
        std::vector<VkCommandBuffer> commandBuffers;
        
        std::vector<std::unique_ptr<IGraphicsPipeline>> graphicPipelines, nextGraphicsPipelines;

        VkFormat hdrFormat = VK_FORMAT_R16G16B16A16_SFLOAT, pickingFormat = VK_FORMAT_R32_UINT;
        std::shared_ptr<Texture> hdrColorTexture, depthTexture, pickingTexture, albedoSpecTexture, positionsTexture, normalsTexture;
        VkBuffer pickingCPUBuffer;
        VmaAllocation pickingCPUAlloc;
        VmaAllocationInfo pickingCPUBufferInfo;

        std::vector<VkSemaphore> imageAvailableSemaphores, renderFinishedSemaphores;
        std::vector<VkFence> inFlightFences;

        std::unique_ptr<DeferredShader> deferredShader;
        std::unique_ptr<Tonemapper> tonemapper;
        
        struct FilterDetachInfo { 
            std::unique_ptr<FilterPipeline> pipeline; 
            uint32_t frame; 
        };
        std::map<uint64_t, std::unique_ptr<FilterPipeline>> filters, nextFilters;
        std::queue<FilterDetachInfo> filterDetachPending;
        uint64_t globalFilterId = 0;

    private:
        void drawFrame();
        void cleanup();

        void startNextSceneLoading();
        void switchScene();

        void recreateSwapChain();
        void cleanupSwapChain();
        void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

        void createInstance();
        void setupDebugMessenger();
        void pickPhysicalDevice();
        void createLogicalDevice();
        void createVmaAllocator();
        void createSwapChain();
        void createImageViews();
        void createRenderPass();
        void createAttachmentsAndBuffers();
        void createSyncObjects();
        
        void drawImguiEngineInfo(double frameTime);

        void applyFilters(VkCommandBuffer commandBuffer, VkImage swapchainImage);

        void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
        VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
        static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);
    };

}
    