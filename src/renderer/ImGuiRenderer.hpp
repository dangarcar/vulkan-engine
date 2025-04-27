#pragma once

#include "vulkan/VulkanTypes.h"
#include <cstdint>

class GLFWwindow;

namespace fly {

    class ImGuiRenderer {
    public:

        ImGuiRenderer(GLFWwindow* window, const VulkanInstance& vk);
        ~ImGuiRenderer();
        
        void cleanupSwapchain();
        void recreateOnNewSwapChain();

        void setupFrame();

        void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

        VkFence getFrameFence(uint32_t currentFrame) const { return imguiInFlightFences[currentFrame]; }
        VkCommandBuffer getCommandBuffer(uint32_t currentFrame) const { return imguiCommandBuffers[currentFrame]; }
        VkSemaphore getRenderFinishedSemaphore(uint32_t currentFrame) const { return imguiRenderFinishedSemaphores[currentFrame]; }

    private:
        VkRenderPass imGuiRenderPass;
        VkDescriptorPool imguiDescriptorPool;
        std::vector<VkFramebuffer> imguiFramebuffers;
        VkCommandPool imguiCommandPool;
        std::vector<VkCommandBuffer> imguiCommandBuffers;
        std::vector<VkSemaphore> imguiRenderFinishedSemaphores;
        std::vector<VkFence> imguiInFlightFences;

        const VulkanInstance& vk;

    private:    
        void createDescriptorPool();
        void createRenderPass();
        void createFramebuffers();
        void createSyncObjects();

    };
    
}