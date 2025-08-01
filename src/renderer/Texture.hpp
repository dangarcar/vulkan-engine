#pragma once

#include <filesystem>

#include "vulkan/VulkanTypes.h"

class ktxTexture2;

namespace fly {

    enum class STB_Format: int {
        STBI_default = 0, // only used for desired_channels

        STBI_grey       = 1,
        STBI_grey_alpha = 2,
        STBI_rgb        = 3,
        STBI_rgb_alpha  = 4
    };

    struct TextureRef {
        uint32_t mipLevels;
        VkImage image;
        VkDeviceMemory imageMemory;
        VkImageView imageView;

        bool operator==(const TextureRef& tex) const {
            return mipLevels == tex.mipLevels && image == tex.image && imageView == tex.imageView && imageMemory == tex.imageMemory;
        }
    };

    class Texture {
    public:
        //Texture creator for generic purposes, like depth textures 
        Texture(
            const VulkanInstance& vk, 
            uint32_t width, uint32_t height, 
            VkFormat format, 
            VkSampleCountFlagBits numSamples,
            VkImageUsageFlags usage,
            VkImageAspectFlags aspectFlags
        );
        //Texture obtained from the path given in png, jpeg or bmp
        Texture(const VulkanInstance& vk, const VkCommandPool commandPool, std::filesystem::path path, STB_Format stbFormat, VkFormat format);
        //Ktx texture in bc7 with mipmaps included, they aren't generated
        Texture(const VulkanInstance& vk, const VkCommandPool commandPool, std::filesystem::path ktxPath);
        //Default 2x2 magenta and black texture ready to be sampled
        Texture(const VulkanInstance& vk, const VkCommandPool commandPool);
        

        ~Texture();
    
        uint32_t getMipLevels() const { return mipLevels; }
        VkImage getImage() const { return image; }
        VkImageView getImageView() const { return imageView; }
        bool isCubemap() const { return cubemap; }

        const TextureRef toRef() const {
            return { mipLevels, image, imageMemory, imageView };
        }

        std::unique_ptr<Texture> copyToFormat(VkFormat newFormat, VkImageUsageFlags usage, VkCommandBuffer commandBuffer) const;

    private:
        uint32_t mipLevels;
        VkImage image;
        VkDeviceMemory imageMemory;
        VkImageView imageView;
        uint32_t width, height;
        VkFormat format;

        const VulkanInstance& vk;
        bool cubemap = false;
        
    private:
        void _createTextureFromPixels(const VulkanInstance& vk, const VkCommandPool commandPool, void* pixels, VkDeviceSize imageSize);
        void _createTextureFromKtx2(const VulkanInstance& vk, const VkCommandPool commandPool, ktxTexture2* texture, const std::vector<VkBufferImageCopy>& regions);
        
    };


    class TextureSampler {
    public:
        enum class Filter { LINEAR, NEAREST };

        TextureSampler(const VulkanInstance& vk, uint32_t mipLevels, Filter filter = Filter::LINEAR);
        ~TextureSampler();

        VkSampler getSampler() const { return textureSampler; }

    private:
        VkSampler textureSampler;

        const VulkanInstance& vk;
    };

}

namespace std {
    template<> struct hash<fly::TextureRef> {
        size_t operator()(fly::TextureRef const& texture) const;
    };
}