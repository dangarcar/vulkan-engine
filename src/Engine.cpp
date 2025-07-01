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
        createInstance();
        setupDebugMessenger();

        vk.surface = this->window.createSurface(vk.instance);

        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImageViews();

        createRenderPass();
        
        this->commandPool = createCommandPool(this->vk, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
        
        createColorAndDepthTextures();
        createFramebuffers();
        
        this->commandBuffers = createCommandBuffers(vk.device, MAX_FRAMES_IN_FLIGHT, this->commandPool);
        this->computeCommandBuffers = createCommandBuffers(vk.device, MAX_FRAMES_IN_FLIGHT, this->commandPool);
        createSyncObjects();

        uiRenderer = std::make_unique<UIRenderer>(this->window.getGlfwWindow(), this->vk);

        this->tonemapFilter = std::make_unique<TonemapFilter>(vk);
        this->tonemapFilter->allocate();
    }

    void Engine::run() {
        auto time = std::chrono::system_clock::now();
        while (!window.shouldClose()) {
            auto lastTime = time;
            time = std::chrono::system_clock::now();

            window.handleInput();
            if(window.keyJustPressed(GLFW_KEY_F11))
                window.toggleFullscreen();

            if(window.isFramebufferResized())
                uiRenderer->resize(window.getWidth(), window.getHeight()); 

            while(!this->filterDetachPending.empty()) {
                FilterDetachInfo& info = this->filterDetachPending.front();
                if(info.frame != currentFrame) 
                    break;
                info.pipeline.reset();
                this->filterDetachPending.pop();
            }
            
            for(auto& pipeline: this->graphicPipelines)
                pipeline->update(this->currentFrame);

            uiRenderer->setupFrame();
            ImGui::Begin("Debug");

            this->scene->run(
                std::chrono::duration<double, std::chrono::seconds::period>(time - lastTime).count(), 
                this->currentFrame,
                *this
            );

            this->uiRenderer->render(this->currentFrame);
            ImGui::End();
            ImGui::Render();
            drawFrame();
        }
    }

    Engine::~Engine() {
        vkDeviceWaitIdle(vk.device);

        scene.reset();
        uiRenderer.reset();
        tonemapFilter.reset();
        cleanup();
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
        vkWaitForFences(vk.device, 1, &this->inFlightFences[this->currentFrame], VK_TRUE, UINT64_MAX);
        
        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(vk.device, vk.swapChain, UINT64_MAX, this->imageAvailableSemaphores[this->currentFrame], VK_NULL_HANDLE, &imageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapChain();
            return;
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }
        
        // Only reset the fence if we are submitting work
        vkResetFences(vk.device, 1, &this->inFlightFences[this->currentFrame]);

        vkResetCommandBuffer(this->commandBuffers[this->currentFrame], 0);
        this->recordCommandBuffer(this->commandBuffers[this->currentFrame], imageIndex);
        
        vkResetCommandBuffer(this->computeCommandBuffers[this->currentFrame], 0);
        applyFilters(this->computeCommandBuffers[this->currentFrame], vk.swapChainImages[imageIndex]);
        
        vkResetCommandBuffer(uiRenderer->getCommandBuffer(this->currentFrame), 0);
        uiRenderer->recordCommandBuffer(imageIndex, this->currentFrame);
        
        
        std::array<VkSubmitInfo, 3> submitInfos = {};
        // Graphics pass
        VkPipelineStageFlags graphicsWaitStage = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfos[0].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfos[0].waitSemaphoreCount = 1;
        submitInfos[0].pWaitSemaphores = &this->imageAvailableSemaphores[this->currentFrame];
        submitInfos[0].pWaitDstStageMask = &graphicsWaitStage; 
        submitInfos[0].commandBufferCount = 1;
        submitInfos[0].pCommandBuffers = &this->commandBuffers[this->currentFrame];
        submitInfos[0].signalSemaphoreCount = 1;
        submitInfos[0].pSignalSemaphores = &this->renderPassFinishedSemaphores[this->currentFrame];

        // Compute pass
        VkPipelineStageFlags computeWaitStage = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfos[1].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfos[1].waitSemaphoreCount = 1;
        submitInfos[1].pWaitSemaphores = &this->renderPassFinishedSemaphores[this->currentFrame];
        submitInfos[1].pWaitDstStageMask = &computeWaitStage; 
        submitInfos[1].commandBufferCount = 1;
        submitInfos[1].pCommandBuffers = &this->computeCommandBuffers[this->currentFrame];
        submitInfos[1].signalSemaphoreCount = 1;
        submitInfos[1].pSignalSemaphores = &this->computePassFinishedSemaphores[this->currentFrame];

        // UI pass
        VkPipelineStageFlags uiWaitStage = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfos[2].sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfos[2].waitSemaphoreCount = 1;
        submitInfos[2].pWaitSemaphores = &this->computePassFinishedSemaphores[this->currentFrame];
        submitInfos[2].pWaitDstStageMask = &uiWaitStage; 
        submitInfos[2].commandBufferCount = 1;
        auto uiBuffer = uiRenderer->getCommandBuffer(this->currentFrame);
        submitInfos[2].pCommandBuffers = &uiBuffer;
        submitInfos[2].signalSemaphoreCount = 1;
        auto uiSemaphore = uiRenderer->getRenderFinishedSemaphore(this->currentFrame);
        submitInfos[2].pSignalSemaphores = &uiSemaphore;

        if(vkQueueSubmit(vk.graphicsQueue, submitInfos.size(), submitInfos.data(), this->inFlightFences[this->currentFrame]) != VK_SUCCESS) {
            throw std::runtime_error("failed to submit draw command buffer!");
        }

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        presentInfo.waitSemaphoreCount = 1;
        auto imguiRenderFinishedSemaphore = uiRenderer->getRenderFinishedSemaphore(this->currentFrame);
        presentInfo.pWaitSemaphores = &imguiRenderFinishedSemaphore;
        
        VkSwapchainKHR swapChains[] = {vk.swapChain};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;

        result = vkQueuePresentKHR(vk.presentQueue, &presentInfo);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || this->window.isFramebufferResized()) {
            this->window.resizeFramebuffer();
            recreateSwapChain();
        } else if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image!");
        }

        this->currentFrame = (this->currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    void Engine::recreateSwapChain() {
        vkDeviceWaitIdle(vk.device);
    
        uiRenderer->cleanupSwapchain();
        cleanupSwapChain();

        createSwapChain();
        createImageViews();
        createColorAndDepthTextures();
        createFramebuffers();
        uiRenderer->recreateOnNewSwapChain();

        tonemapFilter->createResources();
        for(auto& [id, f]: filters)
            f->createResources();
    }

    void Engine::cleanupSwapChain() {
        msaaColorTexture.reset();
        hdrColorTexture.reset();
        depthTexture.reset();

        for(auto framebuffer : this->swapChainFramebuffers) {
            vkDestroyFramebuffer(vk.device, framebuffer, nullptr);
        }

        for(auto imageView : vk.swapChainImageViews) {
            vkDestroyImageView(vk.device, imageView, nullptr);
        }

        vkDestroySwapchainKHR(vk.device, vk.swapChain, nullptr);
    }

    void Engine::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0; // Optional
        beginInfo.pInheritanceInfo = nullptr; // Optional

        if(vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording command buffer!");
        }

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = this->renderPass;
        renderPassInfo.framebuffer = this->swapChainFramebuffers[imageIndex];

        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = vk.swapChainExtent;

        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        clearValues[1].depthStencil = {1.0f, 0};
        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(vk.swapChainExtent.width);
        viewport.height = static_cast<float>(vk.swapChainExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = vk.swapChainExtent;
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
        
        for(auto& pipeline: this->graphicPipelines)
            pipeline->recordOnCommandBuffer(commandBuffer, this->currentFrame);

        vkCmdEndRenderPass(commandBuffer);

        if(vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer!");
        }
    }

    void Engine::cleanup() {
        cleanupSwapChain();

        graphicPipelines.clear();
        filters.clear();

        vkDestroyRenderPass(vk.device, this->renderPass, nullptr);

        for (int i=0; i<MAX_FRAMES_IN_FLIGHT; ++i) {
            vkDestroySemaphore(vk.device, this->renderPassFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(vk.device, this->computePassFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(vk.device, this->imageAvailableSemaphores[i], nullptr);
            vkDestroyFence(vk.device, this->inFlightFences[i], nullptr);
        }
        
        vkDestroyCommandPool(vk.device, this->commandPool, nullptr);

        vkDestroyDevice(vk.device, nullptr);

        if (enableValidationLayers) {
            DestroyDebugUtilsMessengerEXT(vk.instance, this->debugMessenger, nullptr);
        }

        vkDestroySurfaceKHR(vk.instance, vk.surface, nullptr);
        vkDestroyInstance(vk.instance, nullptr);
    }


    void Engine::createInstance() {
        if (enableValidationLayers && !checkValidationLayerSupport()) {
            throw std::runtime_error("validation layers requested, but not available!");
        }

        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Hello Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        auto extensions = this->window.getRequiredExtensions();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();

            populateDebugMessengerCreateInfo(debugCreateInfo);
            createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
        } else {
            createInfo.enabledLayerCount = 0;

            createInfo.pNext = nullptr;
        }

        if (vkCreateInstance(&createInfo, nullptr, &vk.instance) != VK_SUCCESS) {
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

        if (CreateDebugUtilsMessengerEXT(vk.instance, &createInfo, nullptr, &this->debugMessenger) != VK_SUCCESS) {
            throw std::runtime_error("failed to set up debug messenger!");
        }
    }

    void Engine::pickPhysicalDevice() {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(vk.instance, &deviceCount, nullptr);

        if (deviceCount == 0) {
            throw std::runtime_error("failed to find GPUs with Vulkan support!");
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(vk.instance, &deviceCount, devices.data());

        for (const auto& device : devices) {
            if (isDeviceSuitable(vk.surface, device)) {
                vk.physicalDevice = device;
                this->msaaSamples = getMaxUsableSampleCount(device);
                break;
            }
        }

        if (vk.physicalDevice == VK_NULL_HANDLE) {
            throw std::runtime_error("failed to find a suitable GPU!");
        }
    }

    void Engine::createLogicalDevice() {
        QueueFamilyIndices indices = findQueueFamilies(vk.surface, vk.physicalDevice);

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsAndComputeFamily.value(), indices.presentFamily.value()};

        float queuePriority = 1.0f;
        for (uint32_t queueFamily : uniqueQueueFamilies) {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        VkPhysicalDeviceFeatures deviceFeatures{};
        deviceFeatures.samplerAnisotropy = VK_TRUE;

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();

        createInfo.pEnabledFeatures = &deviceFeatures;

        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();

        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        } else {
            createInfo.enabledLayerCount = 0;
        }

        if (vkCreateDevice(vk.physicalDevice, &createInfo, nullptr, &vk.device) != VK_SUCCESS) {
            throw std::runtime_error("failed to create logical device!");
        }

        vkGetDeviceQueue(vk.device, indices.graphicsAndComputeFamily.value(), 0, &vk.graphicsQueue);
        vkGetDeviceQueue(vk.device, indices.presentFamily.value(), 0, &vk.presentQueue);
        vkGetDeviceQueue(vk.device, indices.graphicsAndComputeFamily.value(), 0, &vk.computeQueue);
    }

    void Engine::createSwapChain() {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(vk.surface, vk.physicalDevice);

        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
        VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
        if(swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
            imageCount = swapChainSupport.capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = vk.surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        QueueFamilyIndices indices = findQueueFamilies(vk.surface, vk.physicalDevice);
        uint32_t queueFamilyIndices[] = {indices.graphicsAndComputeFamily.value(), indices.presentFamily.value()};

        if (indices.graphicsAndComputeFamily != indices.presentFamily) {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        } else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0; // Optional
            createInfo.pQueueFamilyIndices = nullptr; // Optional
        }

        createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        if (vkCreateSwapchainKHR(vk.device, &createInfo, nullptr, &vk.swapChain) != VK_SUCCESS) {
            throw std::runtime_error("failed to create swap chain!");
        }

        vkGetSwapchainImagesKHR(vk.device, vk.swapChain, &imageCount, nullptr);
        vk.swapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(vk.device, vk.swapChain, &imageCount, vk.swapChainImages.data());

        vk.swapChainImageFormat = surfaceFormat.format;
        vk.swapChainExtent = extent;
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
        vk.swapChainImageViews.resize(vk.swapChainImages.size());
        
        for (size_t i = 0; i < vk.swapChainImages.size(); i++) {
            vk.swapChainImageViews[i] = createImageView(this->vk, vk.swapChainImages[i], vk.swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1, false);
        }
    }

    void Engine::createRenderPass() {
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = hdrFormat;
        colorAttachment.samples = this->msaaSamples;
        
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentDescription colorAttachmentResolve{};
        colorAttachmentResolve.format = hdrFormat;
        colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorAttachmentResolveRef{};
        colorAttachmentResolveRef.attachment = 2;
        colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;


        VkAttachmentDescription depthAttachment{};
        depthAttachment.format = findDepthFormat(vk.physicalDevice);
        depthAttachment.samples = this->msaaSamples;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depthAttachmentRef{};
        depthAttachmentRef.attachment = 1;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;


        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;
        subpass.pDepthStencilAttachment = &depthAttachmentRef;
        subpass.pResolveAttachments = &colorAttachmentResolveRef;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        std::array<VkAttachmentDescription, 3> attachments = {colorAttachment, depthAttachment, colorAttachmentResolve};
        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        if (vkCreateRenderPass(vk.device, &renderPassInfo, nullptr, &this->renderPass) != VK_SUCCESS) {
            throw std::runtime_error("failed to create render pass!");
        }
    }

    void Engine::createColorAndDepthTextures() {
        this->msaaColorTexture = std::make_unique<Texture>(
            this->vk, 
            vk.swapChainExtent.width, vk.swapChainExtent.height, 
            this->hdrFormat, 
            this->msaaSamples,
            VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT
        );
        
        this->hdrColorTexture = std::make_unique<Texture>(
            this->vk, 
            vk.swapChainExtent.width, vk.swapChainExtent.height, 
            this->hdrFormat, 
            VK_SAMPLE_COUNT_1_BIT,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT
        );

        auto depthFormat = findDepthFormat(vk.physicalDevice);
        this->depthTexture = std::make_unique<Texture>(
            this->vk,
            vk.swapChainExtent.width, vk.swapChainExtent.height, 
            depthFormat,
            this->msaaSamples,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            VK_IMAGE_ASPECT_DEPTH_BIT
        );
    }

    void Engine::createFramebuffers() {
        this->swapChainFramebuffers.resize(vk.swapChainImageViews.size());

        for(size_t i=0; i<vk.swapChainImageViews.size(); i++) {
            std::array<VkImageView, 3> attachments = {
                this->msaaColorTexture->getImageView(),
                this->depthTexture->getImageView(),
                this->hdrColorTexture->getImageView()
            };
        
            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = this->renderPass;
            framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebufferInfo.pAttachments = attachments.data();
            framebufferInfo.width = vk.swapChainExtent.width;
            framebufferInfo.height = vk.swapChainExtent.height;
            framebufferInfo.layers = 1;
        
            if (vkCreateFramebuffer(vk.device, &framebufferInfo, nullptr, &this->swapChainFramebuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create framebuffer!");
            }
        }

    }

    void Engine::createSyncObjects() {
        this->imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        this->renderPassFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        this->computePassFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        this->inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    
        for(int i=0; i<MAX_FRAMES_IN_FLIGHT; ++i) {
            if (vkCreateSemaphore(vk.device, &semaphoreInfo, nullptr, &this->imageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateSemaphore(vk.device, &semaphoreInfo, nullptr, &this->renderPassFinishedSemaphores[i]) != VK_SUCCESS ||
                vkCreateSemaphore(vk.device, &semaphoreInfo, nullptr, &this->computePassFinishedSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(vk.device, &fenceInfo, nullptr, &this->inFlightFences[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create synchronization objects for a frame!");
            }
        }
    }


    void Engine::applyFilters(VkCommandBuffer commandBuffer, VkImage swapchainImage) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        
        if(vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
            throw std::runtime_error("failed to begin recording command buffer!");


        auto colorImage = this->hdrColorTexture->getImage();
        for(auto& [id, f]: filters)
            f->applyFilter(commandBuffer, colorImage, colorImage, this->currentFrame);


        this->tonemapFilter->applyFilter(commandBuffer, colorImage, swapchainImage, this->currentFrame);

        if(vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
            throw std::runtime_error("failed to record compute command buffer!");
    }

}
