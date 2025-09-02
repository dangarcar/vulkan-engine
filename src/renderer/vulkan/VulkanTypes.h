#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <optional>

namespace fly {

    enum class QueueType: int {
        GRAPHICS = 0, COMPUTE, PRESENT, TRANSFER
    };

    struct VulkanInstance {
        VkInstance instance;
        VkSurfaceKHR surface;

        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkDevice device;

        VkQueue graphicsQueue, computeQueue, presentQueue, transferQueue;

        VkSwapchainKHR swapChain;
        VkFormat swapChainImageFormat;
        VkExtent2D swapChainExtent;
        std::vector<VkImage> swapChainImages;
        std::vector<VkImageView> swapChainImageViews;
    };

    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily, computeFamily, presentFamily, transferFamily;
    
        bool isComplete() {
            return graphicsFamily.has_value() && computeFamily.has_value() && presentFamily.has_value() && transferFamily.has_value();
        }
    };
    
    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    struct DescriptorSetLayout {
        VkDescriptorSetLayout layout;
        std::vector<VkDescriptorPoolSize> poolSizes;
        uint32_t descriptorCount;
    };

}
