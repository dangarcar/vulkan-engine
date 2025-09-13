#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <optional>
#include <mutex>

namespace fly {

    struct VulkanInstance {
        VkInstance instance;
        VkSurfaceKHR surface;

        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkDevice device;

        VkQueue generalQueue, presentQueue;
        std::mutex submitMtx;

        VkSwapchainKHR swapChain;
        VkFormat swapChainImageFormat;
        VkExtent2D swapChainExtent;
        std::vector<VkImage> swapChainImages;
        std::vector<VkImageView> swapChainImageViews;
    };

    struct QueueFamilyIndices {
        std::optional<uint32_t> generalFamily, presentFamily;
    
        bool isComplete() {
            return generalFamily.has_value() && presentFamily.has_value();
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
