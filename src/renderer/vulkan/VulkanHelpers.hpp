#pragma once 

#include <vulkan/vulkan.h>
#include <optional>
#include <vector>

namespace fly {

    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsAndComputeFamily;
        std::optional<uint32_t> presentFamily;
    
        bool isComplete() {
            return graphicsAndComputeFamily.has_value() && presentFamily.has_value();
        }
    };
    
    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    //FUNCTION DECLARATIONS
    void createBuffer(
        const VkPhysicalDevice physicalDevice,
        const VkDevice device, 
        
        VkDeviceSize size, 
        VkBufferUsageFlags usage, 
        VkMemoryPropertyFlags properties, 
        VkBuffer& buffer, 
        VkDeviceMemory& bufferMemory
    );

    uint32_t findMemoryType(
        const VkPhysicalDevice physicalDevice, 
        
        uint32_t typeFilter, 
        VkMemoryPropertyFlags properties
    );

    VkCommandBuffer beginSingleTimeCommands(
        const VkDevice device, 
        const VkCommandPool commandPool
    );

    void endSingleTimeCommands(
        const VkDevice device, 
        const VkQueue graphicsQueue, 
        const VkCommandPool commandPool, 
        
        VkCommandBuffer commandBuffer
    );

    void copyBuffer(
        const VkDevice device, 
        const VkQueue graphicsQueue, 
        const VkCommandPool commandPool, 

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

    VkCommandPool createCommandPool(
        const VkPhysicalDevice physicalDevice,
        const VkDevice device,
        const VkSurfaceKHR surface,

        VkCommandPoolCreateFlags flags
    );

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
        const VkPhysicalDevice physicalDevice,
        const VkDevice device, 
        const VkQueue graphicsQueue, 
        const VkCommandPool commandPool,

        VkImage image, 
        VkFormat imageFormat, 
        int32_t texWidth, 
        int32_t texHeight, 
        uint32_t mipLevels
    );

    void transitionImageLayout(
        const VkDevice device, 
        const VkQueue graphicsQueue, 
        const VkCommandPool commandPool,

        VkImage image, 
        VkFormat format, 
        VkImageLayout oldLayout, 
        VkImageLayout newLayout, 
        uint32_t mipLevels
    );

    void copyBufferToImage(
        const VkDevice device, 
        const VkQueue graphicsQueue, 
        const VkCommandPool commandPool,

        VkBuffer buffer, 
        VkImage image, 
        uint32_t width, 
        uint32_t height
    );

    VkImageView createImageView(
        const VkDevice device,

        VkImage image, 
        VkFormat format, 
        VkImageAspectFlags aspectFlags, 
        uint32_t mipLevels
    );

    void createImage(
        const VkPhysicalDevice physicalDevice,
        const VkDevice device,

        uint32_t width, 
        uint32_t height, 
        uint32_t mipLevels, 
        VkSampleCountFlagBits numSamples, 
        VkFormat format, 
        VkImageTiling tiling, 
        VkImageUsageFlags usage, 
        VkMemoryPropertyFlags properties, 
        VkImage& image, 
        VkDeviceMemory& imageMemory
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
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return availableFormat;
            }
        }

        return availableFormats[0];
    }


}