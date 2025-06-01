#include "Texture.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include "vulkan/VulkanHelpers.hpp"

namespace std {

    size_t hash<fly::TextureRef>::operator()(fly::TextureRef const& texture) const {
        return ((hash<glm::vec<3, uint64_t>>()({
            reinterpret_cast<uint64_t>(texture.image),
            reinterpret_cast<uint64_t>(texture.imageView),
            texture.mipLevels
        })));
    }

}

namespace fly {

    //TEXTURE
    Texture::Texture(
        const VulkanInstance& vk, 
        uint32_t width, uint32_t height, 
        VkFormat format, 
        VkSampleCountFlagBits numSamples,
        VkImageUsageFlags usage,
        VkImageAspectFlags aspectFlags
    ): mipLevels{1}, vk{vk}, cubemap{false}
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
            this->imageMemory,
            false
        );
        
        this->imageView = createImageView(this->vk, this->image, format, aspectFlags, 1, false);
    }

    Texture::Texture(const VulkanInstance& vk, const VkCommandPool commandPool, std::filesystem::path path, STB_Format stbFormat, VkFormat format):
        vk{vk}, cubemap{false}
    {
        int texWidth, texHeight, texChannels;
        stbi_uc* pixels = stbi_load(path.string().c_str(), &texWidth, &texHeight, &texChannels, static_cast<int>(stbFormat));
        VkDeviceSize imageSize;
        if(stbFormat == STB_Format::STBI_rgb_alpha)
            imageSize = texHeight * texWidth * 4;
        else
            imageSize = texHeight * texWidth * texChannels;

        if (!pixels) {
            throw std::runtime_error("failed to load texture image!");
        }

        _createTextureFromPixels(vk, commandPool, texWidth, texHeight, pixels, imageSize, format);

        stbi_image_free(pixels);
    }

    Texture::Texture(const VulkanInstance& vk, const VkCommandPool commandPool): vk{vk}, cubemap{false} {
        uint32_t pixels[4] = { 0xFFFF00FFu, 0xFF000000u, 0xFF000000u, 0xFFFF00FFu };
        _createTextureFromPixels(vk, commandPool, 2, 2, pixels, sizeof(pixels), VK_FORMAT_R8G8B8A8_SRGB);
    }

    Texture::Texture(const VulkanInstance& vk, const VkCommandPool commandPool, std::array<std::filesystem::path, 6> path, STB_Format stbFormat, VkFormat format):
        vk{vk}, cubemap{true}
    {
        int texWidth, texHeight, texChannels;
        std::vector<stbi_uc*> pixelArray;
        for(auto& p: path) {
            pixelArray.push_back(stbi_load(p.string().c_str(), &texWidth, &texHeight, &texChannels, static_cast<int>(stbFormat)));
            if(!pixelArray.back())
                throw std::runtime_error("failed to load texture image in cubemap!");
        }
        VkDeviceSize imageSize;
        if(stbFormat == STB_Format::STBI_rgb_alpha)
            imageSize = texHeight * texWidth * 4 * 6;
        else
            imageSize = texHeight * texWidth * texChannels * 6;
        auto layerSize = imageSize / 6;        

        //Unsafe C-like code that is fine in this situation because of its short livespan
        {
            void* pixels = malloc(imageSize);
            for(int i=0; i<6; ++i) {
                memcpy((uint8_t*)pixels + layerSize*i, pixelArray[i], layerSize);
            }

            _createTextureFromPixels(vk, commandPool, texWidth, texHeight, pixels, imageSize, format);

            free(pixels);
        }

        for(auto p: pixelArray) {
            stbi_image_free(p);
        }
    }

    void Texture::_createTextureFromPixels(const VulkanInstance& vk, const VkCommandPool commandPool, int width, int height, void* pixels, VkDeviceSize imageSize, VkFormat format) {        
        this->mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
    
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
    
        createImage(
            vk,    
            width, 
            height, 
            this->mipLevels,
            VK_SAMPLE_COUNT_1_BIT,
            format, 
            VK_IMAGE_TILING_OPTIMAL, 
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, 
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
            this->image, 
            this->imageMemory,
            this->cubemap
        );
    
        transitionImageLayout(
            vk, commandPool,
            this->image, 
            VK_IMAGE_LAYOUT_UNDEFINED, 
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            this->mipLevels,
            this->cubemap
        );
        
        copyBufferToImage(
            vk, commandPool,    
            stagingBuffer, 
            this->image, 
            static_cast<uint32_t>(width), 
            static_cast<uint32_t>(height),
            this->cubemap
        );
    
        generateMipmaps(
            vk, commandPool,    
            this->image, 
            format, 
            width, height, 
            this->mipLevels,
            this->cubemap
        );
        
        vkDestroyBuffer(vk.device, stagingBuffer, nullptr);
        vkFreeMemory(vk.device, stagingBufferMemory, nullptr);

        this->imageView = createImageView(vk, this->image, format, VK_IMAGE_ASPECT_COLOR_BIT, this->mipLevels, this->cubemap);
    }

    Texture::~Texture() {
        vkDestroyImageView(vk.device, this->imageView, nullptr);
        vkDestroyImage(vk.device, this->image, nullptr);
        vkFreeMemory(vk.device, this->imageMemory, nullptr);
    }



    //TEXTURE SAMPLER
    TextureSampler::TextureSampler(const VulkanInstance& vk, uint32_t mipLevels, Filter filter): vk{vk} {
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

        if(filter == Filter::LINEAR) {
            samplerInfo.magFilter = VK_FILTER_LINEAR;
            samplerInfo.minFilter = VK_FILTER_LINEAR;
        } else if(filter == Filter::NEAREST) {
            samplerInfo.magFilter = VK_FILTER_NEAREST;
            samplerInfo.minFilter = VK_FILTER_NEAREST;
        }
            
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