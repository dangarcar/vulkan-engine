#pragma once

#include <filesystem>

#include "vulkan/VulkanTypes.h"

namespace fly {

    enum class STB_Format: int {
        STBI_default = 0, // only used for desired_channels

        STBI_grey       = 1,
        STBI_grey_alpha = 2,
        STBI_rgb        = 3,
        STBI_rgb_alpha  = 4
    };

    class Texture {
    public:
        Texture(
            const VulkanInstance& vk, 
            uint32_t width, uint32_t height, 
            VkFormat format, 
            VkSampleCountFlagBits numSamples,
            VkImageUsageFlags usage,
            VkImageAspectFlags aspectFlags
        );
        Texture(const VulkanInstance& vk, const VkCommandPool commandPool, std::filesystem::path path, STB_Format stbFormat, VkFormat format);
    
        ~Texture();
    
        uint32_t getMipLevels() const { return mipLevels; }
        VkImage getImage() const { return image; }
        VkImageView getImageView() const { return imageView; }
    
    private:
        uint32_t mipLevels;
        VkImage image;
        VkDeviceMemory imageMemory;
        VkImageView imageView;
    
        const VulkanInstance& vk;

    };

    class TextureSampler {
    public:
        TextureSampler(const VulkanInstance& vk, uint32_t mipLevels);
        ~TextureSampler();

        VkSampler getSampler() const { return textureSampler; }

    private:
        VkSampler textureSampler;

        const VulkanInstance& vk;
    };

}
