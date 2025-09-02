#include "UIRenderer.hpp"

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

#include <renderer/vulkan/VulkanConstants.h>
#include <renderer/vulkan/VulkanHelpers.hpp>

namespace fly {

    UIRenderer::UIRenderer(GLFWwindow* window, std::shared_ptr<VulkanInstance> vk): vk{vk}, renderer2d(vk), textRenderer(vk) {
        createDescriptorPool();
        createRenderPass();
        this->uiCommandPool = createCommandPool(this->vk, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
        this->uiCommandBuffers = createCommandBuffers(vk->device, MAX_FRAMES_IN_FLIGHT, this->uiCommandPool);
        createFramebuffers();
        createSyncObjects();

        initImgui(window);

        auto pipeline2d = std::make_unique<GPipeline2D>(vk);
        pipeline2d->allocate(uiRenderPass, VK_SAMPLE_COUNT_1_BIT);
        this->renderer2d.init(std::move(pipeline2d), this->uiCommandPool);

        auto textPipeline = std::make_unique<TextPipeline>(vk);
        textPipeline->allocate(uiRenderPass, VK_SAMPLE_COUNT_1_BIT);
        this->textRenderer.init(std::move(textPipeline));
    }

    UIRenderer::~UIRenderer() {
        cleanupSwapchain();

        vkDestroyRenderPass(vk->device, this->uiRenderPass, nullptr);

        vkFreeCommandBuffers(vk->device, this->uiCommandPool, static_cast<uint32_t>(this->uiCommandBuffers.size()), this->uiCommandBuffers.data());
        vkDestroyCommandPool(vk->device, this->uiCommandPool, nullptr);

        for (int i=0; i<MAX_FRAMES_IN_FLIGHT; ++i) {
            vkDestroySemaphore(vk->device, this->uiRenderFinishedSemaphores[i], nullptr);
        }

        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        
        vkDestroyDescriptorPool(vk->device, this->uiDescriptorPool, nullptr);
    }

    void UIRenderer::initImgui(GLFWwindow* window) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

        // Setup Dear ImGui style
        ImGui::StyleColorsDark();
        //ImGui::StyleColorsLight();

        // Setup Platform/Renderer bindings
        ImGui_ImplGlfw_InitForVulkan(window, true);
        ImGui_ImplVulkan_InitInfo init_info{};
        init_info.Instance = vk->instance;
        init_info.PhysicalDevice = vk->physicalDevice;
        init_info.Device = vk->device;
        init_info.QueueFamily = findQueueFamilies(vk->surface, vk->physicalDevice).graphicsFamily.value();
        init_info.Queue = vk->graphicsQueue;
        init_info.PipelineCache = VK_NULL_HANDLE;
        init_info.DescriptorPool = this->uiDescriptorPool;
        init_info.Allocator = nullptr;
        init_info.MinImageCount = MAX_FRAMES_IN_FLIGHT;
        init_info.ImageCount = MAX_FRAMES_IN_FLIGHT;
        init_info.RenderPass = this->uiRenderPass;
        init_info.CheckVkResultFn = [](VkResult err) {
            if(err != VK_SUCCESS) 
                std::cerr << "[vulkan] Error: VkResult = " << err << std::endl;
        };
        ImGui_ImplVulkan_Init(&init_info);
    }

    void UIRenderer::resize(int width, int height) {
        renderer2d.resize(width, height);
        textRenderer.resize(width, height);
    }

    void UIRenderer::render(uint32_t frame) {
        this->renderer2d.getPipeline()->update(frame);
        this->renderer2d.render(frame, this->uiCommandPool);

        this->textRenderer.getPipeline()->update(frame);
        this->textRenderer.render(frame);
    }

    void UIRenderer::setupFrame() {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void UIRenderer::cleanupSwapchain() {
        for (auto framebuffer : this->uiFramebuffers) {
            vkDestroyFramebuffer(vk->device, framebuffer, nullptr);
        }
    }

    void UIRenderer::recreateOnNewSwapChain() {
        createFramebuffers();
    }
    
    void UIRenderer::recordCommandBuffer(uint32_t imageIndex, uint32_t currentFrame) {
        auto commandBuffer = this->uiCommandBuffers[currentFrame];

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0;
        if(vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording UI command buffer!");
        }

        VkRenderPassBeginInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = this->uiRenderPass;
        renderPassInfo.framebuffer = this->uiFramebuffers[imageIndex];
        
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = vk->swapChainExtent;
        
        VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;
        
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

        this->renderer2d.getPipeline()->recordOnCommandBuffer(commandBuffer, currentFrame);
        this->textRenderer.getPipeline()->recordOnCommandBuffer(commandBuffer, currentFrame);
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);

        vkCmdEndRenderPass(commandBuffer);
        
        if(vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to record UI command buffer!");
        }
    }

    void UIRenderer::createDescriptorPool() {
        VkDescriptorPoolSize pool_sizes[] = {
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE },
        };
        
        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 0;
        
        for (VkDescriptorPoolSize& pool_size : pool_sizes)
            pool_info.maxSets += pool_size.descriptorCount;
        
        pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
        pool_info.pPoolSizes = pool_sizes;
        
        if(vkCreateDescriptorPool(vk->device, &pool_info, nullptr, &this->uiDescriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create UI descriptor pool");
        }
    }

    void UIRenderer::createRenderPass() {
        VkAttachmentDescription attachment = {};
        attachment.format = vk->swapChainImageFormat;
        attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        
        attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        
        attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        
        attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        
        VkAttachmentReference color_attachment = {};
        color_attachment.attachment = 0;
        color_attachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        
        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_attachment;

        VkSubpassDependency dependency = {};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        info.attachmentCount = 1;
        info.pAttachments = &attachment;
        info.subpassCount = 1;
        info.pSubpasses = &subpass;
        info.dependencyCount = 1;
        info.pDependencies = &dependency;
        
        if (vkCreateRenderPass(vk->device, &info, nullptr, &this->uiRenderPass) != VK_SUCCESS) {
            throw std::runtime_error("could not create UI render pass");
        }
    }

    void UIRenderer::createFramebuffers() {
        this->uiFramebuffers.resize(vk->swapChainImageViews.size());

        for(size_t i=0; i<vk->swapChainImageViews.size(); i++) {
            VkImageView attachments[] = {
                vk->swapChainImageViews[i]
            };
        
            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = this->uiRenderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = attachments;
            framebufferInfo.width = vk->swapChainExtent.width;
            framebufferInfo.height = vk->swapChainExtent.height;
            framebufferInfo.layers = 1;
        
            if (vkCreateFramebuffer(vk->device, &framebufferInfo, nullptr, &this->uiFramebuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create UI framebuffer!");
            }
        }
    }

    void UIRenderer::createSyncObjects() {
        this->uiRenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    
        for(int i=0; i<MAX_FRAMES_IN_FLIGHT; ++i) {
            if (vkCreateSemaphore(vk->device, &semaphoreInfo, nullptr, &this->uiRenderFinishedSemaphores[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create UI semaphore objects for a frame!");
            }
        }
    }


}