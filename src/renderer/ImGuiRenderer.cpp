#include "ImGuiRenderer.hpp"

#include <cstddef>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

#include "vulkan/VulkanConstants.h"
#include "vulkan/VulkanHelpers.hpp"

#include <iostream>

namespace fly {

    ImGuiRenderer::ImGuiRenderer(GLFWwindow* window, const VulkanInstance& vk): vk{vk} {
        createDescriptorPool();
        createRenderPass();
        this->imguiCommandPool = createCommandPool(this->vk, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
        this->imguiCommandBuffers = createCommandBuffers(vk.device, MAX_FRAMES_IN_FLIGHT, this->imguiCommandPool);
        createFramebuffers();
        createSyncObjects();

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
        init_info.Instance = vk.instance;
        init_info.PhysicalDevice = vk.physicalDevice;
        init_info.Device = vk.device;
        init_info.QueueFamily = findQueueFamilies(vk.surface, vk.physicalDevice).graphicsAndComputeFamily.value();
        init_info.Queue = vk.graphicsQueue;
        init_info.PipelineCache = VK_NULL_HANDLE;
        init_info.DescriptorPool = this->imguiDescriptorPool;
        init_info.Allocator = nullptr;
        init_info.MinImageCount = MAX_FRAMES_IN_FLIGHT;
        init_info.ImageCount = MAX_FRAMES_IN_FLIGHT;
        init_info.RenderPass = this->imGuiRenderPass;
        init_info.CheckVkResultFn = [](VkResult err) {
            if(err != VK_SUCCESS) 
                std::cerr << "[vulkan] Error: VkResult = " << err << std::endl;
        };
        ImGui_ImplVulkan_Init(&init_info);
    }

    ImGuiRenderer::~ImGuiRenderer() {
        cleanupSwapchain();    

        vkDestroyRenderPass(vk.device, this->imGuiRenderPass, nullptr);

        vkFreeCommandBuffers(vk.device, this->imguiCommandPool, static_cast<uint32_t>(this->imguiCommandBuffers.size()), this->imguiCommandBuffers.data());
        vkDestroyCommandPool(vk.device, this->imguiCommandPool, nullptr);

        for (int i=0; i<MAX_FRAMES_IN_FLIGHT; ++i) {
            vkDestroySemaphore(vk.device, this->imguiRenderFinishedSemaphores[i], nullptr);
            vkDestroyFence(vk.device, this->imguiInFlightFences[i], nullptr);
        }

        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        
        vkDestroyDescriptorPool(vk.device, this->imguiDescriptorPool, nullptr);
    }

    void ImGuiRenderer::setupFrame() {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void ImGuiRenderer::cleanupSwapchain() {
        for (auto framebuffer : this->imguiFramebuffers) {
            vkDestroyFramebuffer(vk.device, framebuffer, nullptr);
        }
    }

    void ImGuiRenderer::recreateOnNewSwapChain() {
        createFramebuffers();
    }
    
    void ImGuiRenderer::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0;
        if(vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording ImGui command buffer!");
        }

        VkRenderPassBeginInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = this->imGuiRenderPass;
        renderPassInfo.framebuffer = this->imguiFramebuffers[imageIndex];
        
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = vk.swapChainExtent;
        
        VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;
        
        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);

        vkCmdEndRenderPass(commandBuffer);
        
        if(vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to record ImGui command buffer!");
        }
    }

    void ImGuiRenderer::createDescriptorPool() {
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
        
        if(vkCreateDescriptorPool(vk.device, &pool_info, nullptr, &this->imguiDescriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create ImGui descriptor pool");
        }
    }

    void ImGuiRenderer::createRenderPass() {
        VkAttachmentDescription attachment = {};
        attachment.format = vk.swapChainImageFormat;
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
        
        if (vkCreateRenderPass(vk.device, &info, nullptr, &this->imGuiRenderPass) != VK_SUCCESS) {
            throw std::runtime_error("could not create Dear ImGui's render pass");
        }
    }

    void ImGuiRenderer::createFramebuffers() {
        this->imguiFramebuffers.resize(vk.swapChainImageViews.size());

        for(size_t i=0; i<vk.swapChainImageViews.size(); i++) {
            VkImageView attachments[] = {
                vk.swapChainImageViews[i]
            };
        
            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = this->imGuiRenderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = attachments;
            framebufferInfo.width = vk.swapChainExtent.width;
            framebufferInfo.height = vk.swapChainExtent.height;
            framebufferInfo.layers = 1;
        
            if (vkCreateFramebuffer(vk.device, &framebufferInfo, nullptr, &this->imguiFramebuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create framebuffer!");
            }
        }
    }

    void ImGuiRenderer::createSyncObjects() {
        this->imguiRenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        this->imguiInFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    
        for(int i=0; i<MAX_FRAMES_IN_FLIGHT; ++i) {
            if (vkCreateSemaphore(vk.device, &semaphoreInfo, nullptr, &this->imguiRenderFinishedSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(vk.device, &fenceInfo, nullptr, &this->imguiInFlightFences[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create ImGui synchronization objects for a frame!");
            }
        }
    }


}