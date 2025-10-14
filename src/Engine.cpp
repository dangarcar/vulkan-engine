#include "Engine.hpp"

#include "renderer/vulkan/VulkanConstants.h"
#include "renderer/vulkan/VulkanHelpers.hpp"


#include <GLFW/glfw3.h>
#include <imgui.h>

#include <set>

static VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

namespace fly {

    void Engine::init() {
        ScopeTimer t("Engine loading time");

        this->vk = std::make_shared<VulkanInstance>();
        createInstance();
        setupDebugMessenger();

        vk->surface = this->window.createSurface(vk->instance);

        pickPhysicalDevice();
        createLogicalDevice();
        createVmaAllocator();
        createSwapChain();
        createImageViews();

        createRenderPass();
        
        this->drawCommandPool = createCommandPool(this->vk, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
        this->transferCommandPool = createCommandPool(this->vk, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
        
        createAttachmentsAndBuffers();
        
        this->commandBuffers = createCommandBuffers(vk->device, MAX_FRAMES_IN_FLIGHT, this->drawCommandPool);
        createSyncObjects();

        uiManager = std::make_unique<UIManager>(this->window.getGlfwWindow(), this->vk);

        this->tonemapper = std::make_unique<Tonemapper>(vk);
        this->tonemapper->allocate();
        this->tonemapper->createResources(this->hdrColorTexture);
    }

    void Engine::run() {
        auto time = std::chrono::system_clock::now();
        double perfTime = 0.0, frameTime = 1.0;
        int frames = 0;

        while(!window.shouldClose()) {
            if(scene == nullptr  ||  (nextScene != nullptr && nextSceneReady.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready))
                switchScene();

            
            auto lastTime = time;
            time = std::chrono::system_clock::now();
            auto dt = std::chrono::duration<double, std::chrono::seconds::period>(time - lastTime).count();
            
            frames++;
            perfTime += dt;
            if(perfTime >= 0.5) {
                frameTime = perfTime / frames;
                frames = 0;
                perfTime = 0;
            }


            window.handleInput();
            if(window.keyJustPressed(GLFW_KEY_F11))
                window.toggleFullscreen();

            if(window.isFramebufferResized())
                uiManager->resize(window.getWidth(), window.getHeight()); 

            while(!this->filterDetachPending.empty()) {
                FilterDetachInfo& info = this->filterDetachPending.front();
                if(info.frame != currentFrame) 
                    break;
                info.pipeline.reset();
                this->filterDetachPending.pop();
            }
            
            for(auto& pipeline: this->graphicPipelines)
                pipeline->update(this->currentFrame);


            uiManager->setupFrame();
            ImGui::Begin("Debug");
            drawImguiEngineInfo(frameTime);
            ImGui::Text("- Scene info");
            ImGui::Indent();
            
            this->scene->run(dt, this->currentFrame, transferCommandPool);
            
            this->uiManager->render(this->currentFrame);
            
            ImGui::End();
            ImGui::Render();
            drawFrame();
        }
    }

    Engine::~Engine() {
        vkDeviceWaitIdle(vk->device);

        scene.reset();
        uiManager.reset();
        tonemapper.reset();
        cleanup();

        Engine::instance = nullptr;
        std::cout << "Engine finished successfully!\n";
    }


    void Engine::removeFilter(uint64_t filterId) {
        FilterDetachInfo info;
        info.pipeline = std::move( this->filters.extract(filterId).mapped() );
        info.frame = this->currentFrame;

        this->filterDetachPending.push(std::move(info));
    }

    void Engine::removeFilters() {
        std::vector<uint64_t> keys;
        for(auto& [k, v]: this->filters) //Idk if this is UB but I'm doing it just in case it is
            keys.push_back(k);

        for(auto k: keys)
            removeFilter(k);
    }

    void Engine::drawFrame() {
        vkWaitForFences(vk->device, 1, &this->inFlightFences[this->currentFrame], VK_TRUE, UINT64_MAX);
        
        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(vk->device, vk->swapChain, UINT64_MAX, this->imageAvailableSemaphores[this->currentFrame], VK_NULL_HANDLE, &imageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapChain();
            return;
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }
        
        // Only reset the fence if we are submitting work
        vkResetFences(vk->device, 1, &this->inFlightFences[this->currentFrame]);

        vkResetCommandBuffer(this->commandBuffers[this->currentFrame], 0);
        this->recordCommandBuffer(this->commandBuffers[this->currentFrame], imageIndex);
        
        vkResetCommandBuffer(uiManager->getCommandBuffer(this->currentFrame), 0);
        uiManager->recordCommandBuffer(imageIndex, this->currentFrame); //WARNING: this uses the queue without synchronization!!!!

        
        std::array<VkSubmitInfo, 2> submitInfos = {};
        // Graphics pass
        VkPipelineStageFlags graphicsWaitStage = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfos[0].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfos[0].waitSemaphoreCount = 1;
        submitInfos[0].pWaitSemaphores = &this->imageAvailableSemaphores[this->currentFrame];
        submitInfos[0].pWaitDstStageMask = &graphicsWaitStage; 
        submitInfos[0].commandBufferCount = 1;
        submitInfos[0].pCommandBuffers = &this->commandBuffers[this->currentFrame];
        submitInfos[0].signalSemaphoreCount = 1;
        submitInfos[0].pSignalSemaphores = &this->renderFinishedSemaphores[this->currentFrame];

        // UI pass
        VkPipelineStageFlags uiWaitStage = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfos[1].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfos[1].waitSemaphoreCount = 1;
        submitInfos[1].pWaitSemaphores = &this->renderFinishedSemaphores[this->currentFrame];
        submitInfos[1].pWaitDstStageMask = &uiWaitStage; 
        submitInfos[1].commandBufferCount = 1;
        auto uiBuffer = uiManager->getCommandBuffer(this->currentFrame);
        submitInfos[1].pCommandBuffers = &uiBuffer;
        submitInfos[1].signalSemaphoreCount = 1;
        auto uiSemaphore = uiManager->getRenderFinishedSemaphore(this->currentFrame);
        submitInfos[1].pSignalSemaphores = &uiSemaphore;

        {
            std::unique_lock<std::mutex> lock(vk->submitMtx);
            if(vkQueueSubmit(vk->generalQueue, submitInfos.size(), submitInfos.data(), this->inFlightFences[this->currentFrame]) != VK_SUCCESS) 
                throw std::runtime_error("failed to submit draw queue!");
        }

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        presentInfo.waitSemaphoreCount = 1;
        auto imguiRenderFinishedSemaphore = uiManager->getRenderFinishedSemaphore(this->currentFrame);
        presentInfo.pWaitSemaphores = &imguiRenderFinishedSemaphore;
        
        VkSwapchainKHR swapChains[] = {vk->swapChain};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;

        {
            std::unique_lock<std::mutex> lock(vk->submitMtx);
            result = vkQueuePresentKHR(vk->presentQueue, &presentInfo);
        }
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || this->window.isFramebufferResized()) {
            this->window.resizeFramebuffer();
            recreateSwapChain();
        } else if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image!");
        }

        this->currentFrame = (this->currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    void Engine::recreateSwapChain() {
        vkDeviceWaitIdle(vk->device);
    
        uiManager->cleanupSwapchain();
        cleanupSwapChain();

        createSwapChain();
        createImageViews();
        createAttachmentsAndBuffers();
        uiManager->recreateOnNewSwapChain();
        
        deferredShader->updateShader(hdrColorTexture, albedoSpecTexture, positionsTexture, normalsTexture, pickingTexture);
        
        tonemapper->createResources(hdrColorTexture);
        for(auto& [id, f]: filters)
            f->createResources();
    }

    void Engine::cleanupSwapChain() {
        this->pickingTexture.reset();
        this->hdrColorTexture.reset();
        this->depthTexture.reset();
        this->albedoSpecTexture.reset();
        this->positionsTexture.reset();
        this->normalsTexture.reset();

        //TODO: this should not be necessary but it is included in the same function, so :|
        vmaDestroyBuffer(vk->allocator, this->pickingCPUBuffer, this->pickingCPUAlloc);

        for(auto framebuffer : this->swapChainFramebuffers)
            vkDestroyFramebuffer(vk->device, framebuffer, nullptr);

        for(auto imageView : vk->swapChainImageViews)
            vkDestroyImageView(vk->device, imageView, nullptr);

        vkDestroySwapchainKHR(vk->device, vk->swapChain, nullptr);
    }

    void Engine::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0; // Optional
        beginInfo.pInheritanceInfo = nullptr; // Optional

        if(vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
            throw std::runtime_error("failed to begin recording command buffer!");


        //RENDER PASS
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = this->renderPass;
        renderPassInfo.framebuffer = this->swapChainFramebuffers[imageIndex];

        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = vk->swapChainExtent;

        std::array<VkClearValue, 5> clearValues{};
        clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        clearValues[1].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        clearValues[2].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        clearValues[3].depthStencil = {1.0f, 0};
        clearValues[4].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        renderPassInfo.clearValueCount = clearValues.size();
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(vk->swapChainExtent.width);
        viewport.height = static_cast<float>(vk->swapChainExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = vk->swapChainExtent;
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
        
        for(auto& pipeline: this->graphicPipelines)
            pipeline->recordOnCommandBuffer(commandBuffer, this->currentFrame);

        vkCmdEndRenderPass(commandBuffer);


        //DO THE DEFERRED SHADING
        deferredShader->run(commandBuffer, this->currentFrame);


        //FILTERS
        for(auto& [id, f]: filters)
            f->applyFilter(commandBuffer, this->hdrColorTexture->getImage(), this->currentFrame);

        
        //TONEMAPPING (from rgb16 to rgb8)
        tonemapper->applyFilter(commandBuffer, vk->swapChainImages[imageIndex], this->currentFrame);


        //RETRIEVE PICKING BUFFER DATA
        glm::ivec2 mousePos = this->window.getMousePos();
        if(1 <= mousePos.x && mousePos.x < window.getWidth()-1
        && 1 <= mousePos.y && mousePos.y < window.getHeight()-1) {
            transitionImageLayout(
                commandBuffer, 
                this->pickingTexture->getImage(), 
                VK_IMAGE_LAYOUT_GENERAL, 
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 
                VK_PIPELINE_STAGE_TRANSFER_BIT, 
                VK_ACCESS_SHADER_READ_BIT, 
                VK_ACCESS_TRANSFER_READ_BIT, 
                this->pickingTexture->getMipLevels(), 
                false
            );
            copyImageToBuffer(
                commandBuffer, 
                this->pickingTexture->getImage(), 
                {mousePos.x-1, mousePos.y-1, 0}, 
                {3, 3, 1}, 
                false, 
                this->pickingCPUBuffer
            );
        }

        
        if(vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
            throw std::runtime_error("failed to record command buffer!");
    }

    void Engine::cleanup() {
        cleanupSwapChain();

        graphicPipelines.clear();
        filters.clear();
        deferredShader.reset();

        vkDestroyRenderPass(vk->device, this->renderPass, nullptr);

        for(int i=0; i<MAX_FRAMES_IN_FLIGHT; ++i) {
            vkDestroySemaphore(vk->device, this->renderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(vk->device, this->imageAvailableSemaphores[i], nullptr);
            vkDestroyFence(vk->device, this->inFlightFences[i], nullptr);
        }
        
        vkDestroyCommandPool(vk->device, this->transferCommandPool, nullptr);
        vkDestroyCommandPool(vk->device, this->drawCommandPool, nullptr);

        vmaDestroyAllocator(vk->allocator);

        vkDestroyDevice(vk->device, nullptr);

        if(enableValidationLayers)
            DestroyDebugUtilsMessengerEXT(vk->instance, this->debugMessenger, nullptr);

        vkDestroySurfaceKHR(vk->instance, vk->surface, nullptr);
        vkDestroyInstance(vk->instance, nullptr);
    }

    void Engine::switchScene() {
        FLY_ASSERT(nextSceneReady.get() == VK_SUCCESS, "The scene loading was not successfully made");

        ScopeTimer t("Scene switching time");
        vkWaitForFences(vk->device, MAX_FRAMES_IN_FLIGHT, this->inFlightFences.data(), VK_TRUE, UINT64_MAX);
        
        this->graphicPipelines = std::move(nextGraphicsPipelines);
        nextGraphicsPipelines.clear();
        
        this->filters = std::move(this->nextFilters);
        this->nextFilters.clear();

        this->deferredShader = this->nextScene->getDeferredShader(vk); //FIXME: THIS DOES NOT WORK !!!!! AAAAAA
        deferredShader->updateShader(hdrColorTexture, albedoSpecTexture, positionsTexture, normalsTexture, pickingTexture);

        this->scene = std::move(this->nextScene);
        this->nextScene = nullptr;
    }

    void Engine::startNextSceneLoading() {
        auto& nextScene = this->nextScene; //FIXME: idk is this is UB
        auto vk = this->vk;
        this->nextSceneReady = std::async(std::launch::async, [vk, &nextScene] {
            FLY_ASSERT(nextScene != nullptr, "Next scene is null");
            
            auto status = VK_SUCCESS;
            auto newSceneCommandPool = createCommandPool(vk, 0);

            try {
                nextScene->init(newSceneCommandPool);
            } catch(const std::exception& e) {
                std::cerr << "ERROR: " << e.what() << std::endl;
                status = VK_ERROR_INITIALIZATION_FAILED;
            }
            
            vkDestroyCommandPool(vk->device, newSceneCommandPool, nullptr); 
            return status;
        });
    }



    void Engine::createInstance() {
        if (enableValidationLayers && !checkValidationLayerSupport()) {
            throw std::runtime_error("validation layers requested, but not available!");
        }

        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = this->name;
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = ENGINE_NAME;
        appInfo.engineVersion = ENGINE_VERSION;
        appInfo.apiVersion = VK_API_VERSION_1_4;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        auto extensions = this->window.getRequiredExtensions();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        if(enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();

            populateDebugMessengerCreateInfo(debugCreateInfo);
            createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
        } else {
            createInfo.enabledLayerCount = 0;

            createInfo.pNext = nullptr;
        }

        if(vkCreateInstance(&createInfo, nullptr, &vk->instance) != VK_SUCCESS) {
            throw std::runtime_error("failed to create instance!");
        }
    }

    void Engine::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
        createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = debugCallback;
    }

    VKAPI_ATTR VkBool32 VKAPI_CALL Engine::debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
        //This is because I've turned on -Werror=unused-parameter, so this is the only use I can give it
        (void) messageSeverity; (void) messageType; (void) pCallbackData; (void) pUserData;

        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

        return VK_FALSE;
    }

    void Engine::setupDebugMessenger() {
        if (!enableValidationLayers) return;

        VkDebugUtilsMessengerCreateInfoEXT createInfo;
        populateDebugMessengerCreateInfo(createInfo);

        if (CreateDebugUtilsMessengerEXT(vk->instance, &createInfo, nullptr, &this->debugMessenger) != VK_SUCCESS) {
            throw std::runtime_error("failed to set up debug messenger!");
        }
    }

    void Engine::pickPhysicalDevice() {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(vk->instance, &deviceCount, nullptr);

        if(deviceCount == 0) {
            throw std::runtime_error("failed to find GPUs with Vulkan support!");
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(vk->instance, &deviceCount, devices.data());

        for(const auto& device : devices) {
            if(isDeviceSuitable(vk->surface, device)) {
                vk->physicalDevice = device;
                break;
            }
        }

        if(vk->physicalDevice == VK_NULL_HANDLE) {
            throw std::runtime_error("failed to find a suitable GPU!");
        }
    }

    void Engine::createLogicalDevice() {
        QueueFamilyIndices indices = findQueueFamilies(vk->surface, vk->physicalDevice);

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = {
            indices.generalFamily.value(), 
            indices.presentFamily.value()        
        };

        float queuePriorities[] = {1.0f, 1.0f, 1.0f, 1.0f};
        for(uint32_t queueFamily : uniqueQueueFamilies) {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = queuePriorities;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        VkPhysicalDeviceFeatures deviceFeatures{};
        deviceFeatures.samplerAnisotropy = VK_TRUE;
        deviceFeatures.independentBlend = VK_TRUE;
        deviceFeatures.fragmentStoresAndAtomics = VK_TRUE;

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();

        createInfo.pEnabledFeatures = &deviceFeatures;

        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();

        if(enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        } else {
            createInfo.enabledLayerCount = 0;
        }

        if(vkCreateDevice(vk->physicalDevice, &createInfo, nullptr, &vk->device) != VK_SUCCESS) {
            throw std::runtime_error("failed to create logical device!");
        }

        vkGetDeviceQueue(vk->device, indices.generalFamily.value(), 0, &vk->generalQueue);
        vkGetDeviceQueue(vk->device, indices.presentFamily.value(), 0, &vk->presentQueue);
    }

    void Engine::createVmaAllocator() {
        VmaAllocatorCreateInfo allocatorCreateInfo = {};
        allocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
        allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_4;
        allocatorCreateInfo.physicalDevice = vk->physicalDevice;
        allocatorCreateInfo.device = vk->device;
        allocatorCreateInfo.instance = vk->instance;

        if(vmaCreateAllocator(&allocatorCreateInfo, &vk->allocator) != VK_SUCCESS)
            throw std::runtime_error("Failed to create vma allocator!"); 
    }

    void Engine::createSwapChain() {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(vk->surface, vk->physicalDevice);

        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
        VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
        if(swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
            imageCount = swapChainSupport.capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = vk->surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        if(vkCreateSwapchainKHR(vk->device, &createInfo, nullptr, &vk->swapChain) != VK_SUCCESS) {
            throw std::runtime_error("failed to create swap chain!");
        }

        vkGetSwapchainImagesKHR(vk->device, vk->swapChain, &imageCount, nullptr);
        vk->swapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(vk->device, vk->swapChain, &imageCount, vk->swapChainImages.data());

        vk->swapChainImageFormat = surfaceFormat.format;
        vk->swapChainExtent = extent;
    }

    VkExtent2D Engine::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        } 

        VkExtent2D actualExtent = {
            static_cast<uint32_t>(window.getWidth()),
            static_cast<uint32_t>(window.getHeight())
        };

        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actualExtent;
    } 

    void Engine::createImageViews() {
        vk->swapChainImageViews.resize(vk->swapChainImages.size());
        
        for (size_t i = 0; i < vk->swapChainImages.size(); i++) {
            vk->swapChainImageViews[i] = createImageView(this->vk, vk->swapChainImages[i], vk->swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1, false);
        }
    }

    void Engine::createRenderPass() {
        VkAttachmentDescription albedoAttachment{};
        albedoAttachment.format = VK_FORMAT_R16G16B16A16_SFLOAT;
        albedoAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        albedoAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        albedoAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        albedoAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        albedoAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        albedoAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        albedoAttachment.finalLayout = VK_IMAGE_LAYOUT_GENERAL;

        VkAttachmentReference albedoAttachmentRef{};
        albedoAttachmentRef.attachment = 0;
        albedoAttachmentRef.layout = VK_IMAGE_LAYOUT_GENERAL;

        auto positionsAttachment = albedoAttachment;
        auto positionsAttachmentRef = albedoAttachmentRef;
        positionsAttachmentRef.attachment = 1;

        auto normalsAttachment = albedoAttachment;
        auto normalsAttachmentRef = albedoAttachmentRef;
        normalsAttachmentRef.attachment = 2;

        VkAttachmentDescription depthAttachment{};
        depthAttachment.format = findDepthFormat(vk->physicalDevice);
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depthAttachmentRef{};
        depthAttachmentRef.attachment = 3;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentDescription pickingAttachment{};
        pickingAttachment.format = this->pickingFormat;
        pickingAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        pickingAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        pickingAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        pickingAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        pickingAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        pickingAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        pickingAttachment.finalLayout = VK_IMAGE_LAYOUT_GENERAL;

        VkAttachmentReference pickingAttachmentRef{};
        pickingAttachmentRef.attachment = 4;
        pickingAttachmentRef.layout = VK_IMAGE_LAYOUT_GENERAL;


        std::array<VkAttachmentReference, 4> colorAttachs = {albedoAttachmentRef, positionsAttachmentRef, normalsAttachmentRef, pickingAttachmentRef};
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = colorAttachs.size();
        subpass.pColorAttachments = colorAttachs.data();
        subpass.pDepthStencilAttachment = &depthAttachmentRef;


        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        std::array<VkAttachmentDescription, 5> attachments = {albedoAttachment, positionsAttachment, normalsAttachment, depthAttachment, pickingAttachment};
        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = attachments.size();
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        if(vkCreateRenderPass(vk->device, &renderPassInfo, nullptr, &this->renderPass) != VK_SUCCESS)
            throw std::runtime_error("failed to create render pass!");
    }

    void Engine::createAttachmentsAndBuffers() {        
        //CREATE ATTACHMENT TEXTURES
        this->albedoSpecTexture = std::make_shared<Texture>(
            this->vk, 
            vk->swapChainExtent.width, vk->swapChainExtent.height, 
            VK_FORMAT_R16G16B16A16_SFLOAT, 
            VK_SAMPLE_COUNT_1_BIT,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT
        );

        this->positionsTexture = std::make_shared<Texture>(
            this->vk, 
            vk->swapChainExtent.width, vk->swapChainExtent.height, 
            VK_FORMAT_R16G16B16A16_SFLOAT, 
            VK_SAMPLE_COUNT_1_BIT,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT
        );

        this->normalsTexture = std::make_shared<Texture>(
            this->vk, 
            vk->swapChainExtent.width, vk->swapChainExtent.height, 
            VK_FORMAT_R16G16B16A16_SFLOAT, 
            VK_SAMPLE_COUNT_1_BIT,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT
        );

        auto depthFormat = findDepthFormat(vk->physicalDevice);
        this->depthTexture = std::make_shared<Texture>(
            this->vk,
            vk->swapChainExtent.width, vk->swapChainExtent.height, 
            depthFormat,
            VK_SAMPLE_COUNT_1_BIT,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            VK_IMAGE_ASPECT_DEPTH_BIT
        );

        this->pickingTexture = std::make_shared<Texture>(
            this->vk, 
            vk->swapChainExtent.width, vk->swapChainExtent.height, 
            this->pickingFormat, 
            VK_SAMPLE_COUNT_1_BIT,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT
        );

        this->hdrColorTexture = std::make_shared<Texture>(
            this->vk, 
            vk->swapChainExtent.width, vk->swapChainExtent.height, 
            this->hdrFormat, 
            VK_SAMPLE_COUNT_1_BIT,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT
        );

        //CREATE PICKING BUFFER (CPU SIDE)
        VkBufferCreateInfo bufferCreateInfo{};
        bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferCreateInfo.size = 9*sizeof(uint32_t);
        bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        VmaAllocationCreateInfo bufferCreateAllocInfo = {};
        bufferCreateAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        bufferCreateAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
                
        vmaCreateBuffer(
            vk->allocator, 
            &bufferCreateInfo, 
            &bufferCreateAllocInfo, 
            &this->pickingCPUBuffer, 
            &this->pickingCPUAlloc, 
            &this->pickingCPUBufferInfo
        );
    

        //CREATE FRAMEBUFFERS
        this->swapChainFramebuffers.resize(vk->swapChainImageViews.size());
        for(size_t i=0; i<vk->swapChainImageViews.size(); i++) {
            std::array<VkImageView, 5> attachments = {
                this->albedoSpecTexture->getImageView(),
                this->positionsTexture->getImageView(),
                this->normalsTexture->getImageView(),
                this->depthTexture->getImageView(),
                this->pickingTexture->getImageView()
            };
        
            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = this->renderPass;
            framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebufferInfo.pAttachments = attachments.data();
            framebufferInfo.width = vk->swapChainExtent.width;
            framebufferInfo.height = vk->swapChainExtent.height;
            framebufferInfo.layers = 1;
        
            if(vkCreateFramebuffer(vk->device, &framebufferInfo, nullptr, &this->swapChainFramebuffers[i]) != VK_SUCCESS)
                throw std::runtime_error("failed to create framebuffer!");
        }
    }

    void Engine::createSyncObjects() {
        this->imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        this->renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        this->inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    
        for(int i=0; i<MAX_FRAMES_IN_FLIGHT; ++i) {
            if (vkCreateSemaphore(vk->device, &semaphoreInfo, nullptr, &this->imageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateSemaphore(vk->device, &semaphoreInfo, nullptr, &this->renderFinishedSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(vk->device, &fenceInfo, nullptr, &this->inFlightFences[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create synchronization objects for a frame!");
            }
        }
    }


    void Engine::drawImguiEngineInfo(double frameTime) {
        ImGui::Text("- Engine info");
        ImGui::Indent();
        ImGui::LabelText("FPS", "%.03f", 1/frameTime);
        ImGui::LabelText("Frame time", "%.03fms", frameTime * 1000);
        ImGui::LabelText("Frame size", "%dpx x %dpx", window.getWidth(), window.getHeight());
        ImGui::LabelText("Mouse pos", "%d %d", (int)window.getMousePos().x, (int)window.getMousePos().y);
        ImGui::LabelText("Mouse pos", "%d %d", (int)window.getMousePos().x, (int)window.getMousePos().y);

        std::array<VmaBudget, VK_MAX_MEMORY_HEAPS> budgets;
        vmaGetHeapBudgets(vk->allocator, budgets.data());
        double deviceUsage = 0, deviceBudget = 0;
        VkPhysicalDeviceMemoryProperties prop;
        vkGetPhysicalDeviceMemoryProperties(vk->physicalDevice, &prop);
        for(size_t i=0; i<VK_MAX_MEMORY_HEAPS; ++i) {
            if(prop.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
                deviceUsage += budgets[i].usage;
                deviceBudget += budgets[i].budget;
            }
        }
        
        auto deviceRatio = deviceUsage / deviceBudget;
        ImGui::LabelText("VRAM", "%.1lf / %.1lf MB", deviceUsage / (1 << 20), deviceBudget / (1 << 20));
        
        ImVec4 color = {1, 0, 1, 1};
        if(deviceRatio < 0.5) {
            auto b = glm::mix(glm::vec4{0,1,0,1}, {1,1,0,1}, deviceRatio*2);
            color = ImVec4(b.x, b.y, b.z, b.w);
        } else {
            auto b = glm::mix(glm::vec4{1,1,0,1}, {1,0,0,1}, deviceRatio*2 - 1);
            color = ImVec4(b.x, b.y, b.z, b.w);
        }
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color);
        ImGui::ProgressBar(deviceRatio, ImVec2(0,0));
        ImGui::PopStyleColor();

        ImGui::Unindent();
        ImGui::Dummy(ImVec2(0.0f, 5.0f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 5.0f));
    }

}
