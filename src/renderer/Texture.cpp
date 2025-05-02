#include "Texture.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include "vulkan/VulkanHelpers.hpp"

namespace fly {

    Texture::Texture(
        const VulkanInstance& vk, 
        uint32_t width, uint32_t height, 
        VkFormat format, 
        VkSampleCountFlagBits numSamples,
        VkImageUsageFlags usage,
        VkImageAspectFlags aspectFlags
    ): mipLevels{1}, vk{vk} 
    {
        createImage(
            this->vk,
            width, 
            height, 
            1, 
            numSamples, 
            format, 
            VK_IMAGE_TILING_OPTIMAL, 
            usage, 
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
            this->image, 
            this->imageMemory
        );
        
        this->imageView = createImageView(this->vk, this->image, format, aspectFlags, 1);
    }

    Texture::Texture(const VulkanInstance& vk, const VkCommandPool commandPool, std::filesystem::path path, STB_Format stbFormat, VkFormat format):
        vk{vk}
    {
        int texWidth, texHeight, texChannels;
        stbi_uc* pixels = stbi_load(path.string().c_str(), &texWidth, &texHeight, &texChannels, static_cast<int>(stbFormat));
        VkDeviceSize imageSize = texWidth * texHeight * 4;
    
        if (!pixels) {
            throw std::runtime_error("failed to load texture image!");
        }
    
        this->mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;
    
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
    
        createBuffer(
            vk,
            imageSize, 
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
            stagingBuffer, 
            stagingBufferMemory
        );
    
        void* data;
        vkMapMemory(vk.device, stagingBufferMemory, 0, imageSize, 0, &data);
        memcpy(data, pixels, static_cast<size_t>(imageSize));
        vkUnmapMemory(vk.device, stagingBufferMemory);
    
        stbi_image_free(pixels);
    
        createImage(
            vk,    
            texWidth, 
            texHeight, 
            this->mipLevels,
            VK_SAMPLE_COUNT_1_BIT,
            format, 
            VK_IMAGE_TILING_OPTIMAL, 
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, 
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
            this->image, 
            this->imageMemory
        );
    
        transitionImageLayout(
            vk, commandPool,
            this->image, 
            VK_IMAGE_LAYOUT_UNDEFINED, 
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            this->mipLevels
        );
        
        copyBufferToImage(
            vk, commandPool,    
            stagingBuffer, 
            this->image, 
            static_cast<uint32_t>(texWidth), 
            static_cast<uint32_t>(texHeight)
        );
    
        generateMipmaps(
            vk, commandPool,    
            this->image, 
            format, 
            texWidth, texHeight, 
            this->mipLevels
        );
        
        vkDestroyBuffer(vk.device, stagingBuffer, nullptr);
        vkFreeMemory(vk.device, stagingBufferMemory, nullptr);

        this->imageView = createImageView(vk, this->image, format, VK_IMAGE_ASPECT_COLOR_BIT, this->mipLevels);
    }

    Texture::~Texture() {
        vkDestroyImageView(vk.device, this->imageView, nullptr);
        vkDestroyImage(vk.device, this->image, nullptr);
        vkFreeMemory(vk.device, this->imageMemory, nullptr);
    }


    TextureSampler::TextureSampler(const VulkanInstance& vk, uint32_t mipLevels): vk{vk} {
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;

        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(vk.physicalDevice, &properties);
        
        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.minLod = 0.0f; 
        samplerInfo.maxLod = static_cast<float>(mipLevels);
        samplerInfo.mipLodBias = 0.0f; // Optional

        if(vkCreateSampler(vk.device, &samplerInfo, nullptr, &this->textureSampler) != VK_SUCCESS) {
            throw std::runtime_error("failed to create texture sampler!");
        }
    }

    TextureSampler::~TextureSampler() {
        vkDestroySampler(vk.device, this->textureSampler, nullptr);
    }


}