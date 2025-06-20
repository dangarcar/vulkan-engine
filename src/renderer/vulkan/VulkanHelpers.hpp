#pragma once 

#include "VulkanTypes.h"

namespace fly {

    //FUNCTION DECLARATIONS
    void createBuffer(
        const VulkanInstance& vk, 
        VkDeviceSize size, 
        VkBufferUsageFlags usage, 
        VkMemoryPropertyFlags properties, 
        VkBuffer& buffer, 
        VkDeviceMemory& bufferMemory
    );

    uint32_t findMemoryType(
        const VulkanInstance& vk, 
        uint32_t typeFilter, 
        VkMemoryPropertyFlags properties
    );

    VkCommandBuffer beginSingleTimeCommands(const VulkanInstance& vk, const VkCommandPool commandPool);

    void endSingleTimeCommands(const VulkanInstance& vk, const VkCommandPool commandPool, VkCommandBuffer commandBuffer);

    void copyBuffer(
        VkCommandBuffer commandBuffer,
        VkBuffer srcBuffer, 
        VkBuffer dstBuffer, 
        VkDeviceSize size
    );

    VkFormat findSupportedFormat(
        const VkPhysicalDevice physicalDevice,

        const std::vector<VkFormat>& candidates, 
        VkImageTiling tiling, 
        VkFormatFeatureFlags features
    );

    VkCommandPool createCommandPool(const VulkanInstance& vk, VkCommandPoolCreateFlags flags);

    QueueFamilyIndices findQueueFamilies(const VkSurfaceKHR surface, VkPhysicalDevice physicalDevice);

    bool checkDeviceExtensionSupport(VkPhysicalDevice device);
    
    SwapChainSupportDetails querySwapChainSupport(const VkSurfaceKHR surface, VkPhysicalDevice device);
    
    bool isDeviceSuitable(const VkSurfaceKHR surface, VkPhysicalDevice device);

    bool checkValidationLayerSupport();

    VkShaderModule createShaderModule(const VkDevice device, const std::vector<char>& code);

    std::vector<VkCommandBuffer> createCommandBuffers(
        const VkDevice device, 
        uint32_t commandBufferCount, 
        VkCommandPool &commandPool
    );

    VkSampleCountFlagBits getMaxUsableSampleCount(const VkPhysicalDevice physicalDevice);


    //IMAGE
    void generateMipmaps(
        const VulkanInstance& vk,
        VkCommandBuffer commandBuffer,
        VkImage image, 
        VkFormat imageFormat, 
        int32_t texWidth, 
        int32_t texHeight, 
        uint32_t mipLevels,
        bool cubemap
    );

    void transitionImageLayout(
        VkCommandBuffer commandBuffer,
        VkImage image, 
        VkImageLayout oldLayout, 
        VkImageLayout newLayout,
        VkPipelineStageFlags srcStageMask,
        VkPipelineStageFlags dstStageMask,
        VkAccessFlags srcAccessMask,
        VkAccessFlags dstAccessMask,
        uint32_t mipLevels,
        bool cubemap
    );

    void copyBufferToImage(
        VkCommandBuffer commandBuffer,
        VkBuffer buffer, 
        VkImage image, 
        uint32_t width, 
        uint32_t height,
        bool cubemap
    );

    VkImageView createImageView(
        const VulkanInstance& vk,
        VkImage image, 
        VkFormat format, 
        VkImageAspectFlags aspectFlags, 
        uint32_t mipLevels,
        bool cubemap
    );

    void createImage(
        const VulkanInstance& vk,
        uint32_t width, 
        uint32_t height, 
        uint32_t mipLevels, 
        VkSampleCountFlagBits numSamples, 
        VkFormat format, 
        VkImageTiling tiling, 
        VkImageUsageFlags usage, 
        VkMemoryPropertyFlags properties, 
        VkImage& image, 
        VkDeviceMemory& imageMemory,
        bool cubemap
    );


    //INLINE DEFINITIONS
    inline VkFormat findDepthFormat(const VkPhysicalDevice physicalDevice) {
        return findSupportedFormat(
            physicalDevice,
            {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
        );
    }

    inline bool hasStencilComponent(VkFormat format) {
        return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
    }

    inline VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
        for (const auto& availablePresentMode : availablePresentModes) {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return availablePresentMode;
            }
        }
    
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    inline VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
        for (const auto& availableFormat : availableFormats) {
            if (availableFormat.format == VK_FORMAT_R8G8B8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return availableFormat;
            }
        }

        return availableFormats[0];
    }


}