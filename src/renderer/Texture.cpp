#include "Texture.hpp"
#include "Utils.hpp"
#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <stdexcept>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include "vulkan/VulkanHelpers.hpp"

#include <ktx.h>
#include <ktxvulkan.h>

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

    //TEXTURE IN DEPTH
    Texture::Texture(
        const VulkanInstance& vk, 
        uint32_t width, uint32_t height, 
        VkFormat format, 
        VkSampleCountFlagBits numSamples,
        VkImageUsageFlags usage,
        VkImageAspectFlags aspectFlags
    ): mipLevels{1}, width{width}, height{height}, format{format}, vk{vk}, cubemap{false}
    {
        createImage(
            this->vk,
            this->width, 
            this->height, 
            1, 
            numSamples, 
            this->format, 
            VK_IMAGE_TILING_OPTIMAL, 
            usage, 
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
            this->image, 
            this->imageMemory,
            false
        );
        
        this->imageView = createImageView(this->vk, this->image, this->format, aspectFlags, 1, false);
    }

    //PNG OR JPEG WITH MIPMAP GENERATION
    Texture::Texture(const VulkanInstance& vk, const VkCommandPool commandPool, std::filesystem::path path, STB_Format stbFormat, VkFormat format):
        format{format}, vk{vk}, cubemap{false}
    {
        ScopeTimer t(std::format("Texture load {}", path.string())); //TODO: remove timer

        int texWidth, texHeight, texChannels;
        stbi_uc* pixels = stbi_load(path.string().c_str(), &texWidth, &texHeight, &texChannels, static_cast<int>(stbFormat));
        this->width = texWidth; 
        this->height = texHeight;
        VkDeviceSize imageSize;
        if(stbFormat == STB_Format::STBI_rgb_alpha)
            imageSize = this->width * this->height * 4;
        else
            imageSize = this->width * this->height * texChannels;

        if (!pixels) {
            throw std::runtime_error("failed to load texture image!");
        }

        _createTextureFromPixels(vk, commandPool, pixels, imageSize);

        stbi_image_free(pixels);
    }

    //DEFAULT TEXTURE
    Texture::Texture(const VulkanInstance& vk, const VkCommandPool commandPool): 
        width{2}, height{2}, format{VK_FORMAT_R8G8B8A8_SRGB}, vk{vk}, cubemap{false}
    {
        uint32_t pixels[4] = { 0xFFFF00FFu, 0xFF000000u, 0xFF000000u, 0xFFFF00FFu };
        _createTextureFromPixels(vk, commandPool, pixels, sizeof(pixels));
    }

    //KTX TEXTURE WITH MIPMAPS INCLUDED IN BC7
    Texture::Texture(const VulkanInstance& vk, const VkCommandPool commandPool, std::filesystem::path ktxPath):
        vk{vk}
    {
        ScopeTimer t(std::format("KTX Texture {}", ktxPath.string()));

        ktxTexture2* texture;
        auto result = ktxTexture2_CreateFromNamedFile(
            ktxPath.string().c_str(),
            KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
            &texture
        );

        if(result != KTX_SUCCESS)
           throw std::runtime_error(std::format("Failed to load KTX2 file: {}", ktxErrorString(result)));

        if(ktxTexture2_NeedsTranscoding(texture)) {
            std::cout << "Needs transcoding\n";
            ktx_transcode_fmt_e format = KTX_TTF_BC7_RGBA; // or ETC2, ASTC, etc.
            result = ktxTexture2_TranscodeBasis(texture, format, 0);
            if(result != KTX_SUCCESS) {
                ktxTexture_Destroy(reinterpret_cast<ktxTexture*>(texture));
                throw std::runtime_error(std::format("Transcoding failed: {}", ktxErrorString(result)));
            }
        }

        this->mipLevels = texture->numLevels;
        this->cubemap = texture->isCubemap;
        this->width  = texture->baseWidth;
        this->height = texture->baseHeight;
        this->format = ktxTexture2_GetVkFormat(texture);
        
        std::vector<VkBufferImageCopy> regions;
        for(uint32_t mip = 0; mip < this->mipLevels; ++mip) {
            uint32_t mipWidth  = std::max(1u, this->width >> mip);
            uint32_t mipHeight = std::max(1u, this->height >> mip);
        
            for(uint32_t face = 0; face < texture->numFaces; ++face) {
                ktx_size_t offset;
                ktxTexture_GetImageOffset(reinterpret_cast<ktxTexture*>(texture), mip, 0, face, &offset);

                VkBufferImageCopy region{};
                region.bufferOffset = offset;
                region.bufferRowLength = 0;
                region.bufferImageHeight = 0;
            
                region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                region.imageSubresource.mipLevel = mip;
                region.imageSubresource.baseArrayLayer = face;
                region.imageSubresource.layerCount = 1;
            
                region.imageOffset = {0, 0, 0};
                region.imageExtent = {mipWidth, mipHeight, 1};
            
                regions.push_back(region);
            }
        }

        _createTextureFromKtx2(vk, commandPool, texture, regions);

        ktxTexture_Destroy(reinterpret_cast<ktxTexture*>(texture));
    }

    void Texture::_createTextureFromPixels(const VulkanInstance& vk, const VkCommandPool commandPool, void* pixels, VkDeviceSize imageSize) {        
        this->mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(this->width, this->height)))) + 1;
        
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
            this->width, 
            this->height, 
            this->mipLevels,
            VK_SAMPLE_COUNT_1_BIT,
            this->format, 
            VK_IMAGE_TILING_OPTIMAL, 
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, 
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
            this->image, 
            this->imageMemory,
            this->cubemap
        );
        
        {
            auto commandBuffer = beginSingleTimeCommands(vk, commandPool);
            
            transitionImageLayout(
                commandBuffer,
                this->image, 
                VK_IMAGE_LAYOUT_UNDEFINED, 
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                0,
                VK_ACCESS_TRANSFER_WRITE_BIT,
                this->mipLevels,
                this->cubemap
            );
            
            copyBufferToImage(
                commandBuffer,    
                stagingBuffer, 
                this->image, 
                static_cast<uint32_t>(this->width), 
                static_cast<uint32_t>(this->height),
                this->cubemap
            );
            
            generateMipmaps(
                vk, commandBuffer,    
                this->image, 
                this->format, 
                this->width, this->height, 
                this->mipLevels,
                this->cubemap
            );

            endSingleTimeCommands(vk, commandPool, commandBuffer);
        }
        
        vkDestroyBuffer(vk.device, stagingBuffer, nullptr);
        vkFreeMemory(vk.device, stagingBufferMemory, nullptr);

        this->imageView = createImageView(vk, this->image, this->format, VK_IMAGE_ASPECT_COLOR_BIT, this->mipLevels, this->cubemap);
    }

    void Texture::_createTextureFromKtx2(const VulkanInstance& vk, const VkCommandPool commandPool, ktxTexture2* texture, const std::vector<VkBufferImageCopy>& regions) {
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        
        createBuffer(
            vk,
            texture->dataSize, 
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
            stagingBuffer, 
            stagingBufferMemory
        );
    
        void* data;
        vkMapMemory(vk.device, stagingBufferMemory, 0, texture->dataSize, 0, &data);
        memcpy(data, texture->pData, static_cast<size_t>(texture->dataSize));
        vkUnmapMemory(vk.device, stagingBufferMemory);
    
        createImage(
            vk,    
            this->width, 
            this->height, 
            this->mipLevels,
            VK_SAMPLE_COUNT_1_BIT,
            this->format, 
            VK_IMAGE_TILING_OPTIMAL, 
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, 
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
            this->image, 
            this->imageMemory,
            this->cubemap
        );
    
        {
            auto commandBuffer = beginSingleTimeCommands(vk, commandPool);
            
            transitionImageLayout(
                commandBuffer,
                this->image, 
                VK_IMAGE_LAYOUT_UNDEFINED, 
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                0,
                VK_ACCESS_TRANSFER_WRITE_BIT,
                this->mipLevels,
                this->cubemap
            );

            vkCmdCopyBufferToImage(
                commandBuffer,
                stagingBuffer,
                image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                regions.size(),
                regions.data()
            );

            transitionImageLayout(
                commandBuffer,
                this->image, 
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_ACCESS_SHADER_READ_BIT,
                this->mipLevels,
                this->cubemap
            );

            endSingleTimeCommands(vk, commandPool, commandBuffer);
        }

        vkDestroyBuffer(vk.device, stagingBuffer, nullptr);
        vkFreeMemory(vk.device, stagingBufferMemory, nullptr);

        this->imageView = createImageView(vk, this->image, this->format, VK_IMAGE_ASPECT_COLOR_BIT, this->mipLevels, this->cubemap);
    }


    Texture::~Texture() {
        vkDestroyImageView(vk.device, this->imageView, nullptr);
        vkDestroyImage(vk.device, this->image, nullptr);
        vkFreeMemory(vk.device, this->imageMemory, nullptr);
    }

    std::unique_ptr<Texture> Texture::copyToFormat(VkFormat newFormat, VkImageUsageFlags usage, VkCommandBuffer commandBuffer) const {
        auto newTexture = std::make_unique<fly::Texture>(
            vk, this->width, this->height, 
            newFormat, 
            VK_SAMPLE_COUNT_1_BIT,
            usage,
            VK_IMAGE_ASPECT_COLOR_BIT
        );
        
        fly::transitionImageLayout(
            commandBuffer, 
            this->image, 
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_SHADER_READ_BIT, 
            VK_ACCESS_TRANSFER_READ_BIT, 
            this->mipLevels,
            false
        );

        fly::transitionImageLayout(
            commandBuffer, 
            newTexture->image, 
            VK_IMAGE_LAYOUT_UNDEFINED, 
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, 
            VK_ACCESS_TRANSFER_WRITE_BIT, 
            newTexture->mipLevels, 
            false
        );

        VkImageCopy copyRegion{};
        copyRegion.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copyRegion.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copyRegion.extent = {uint32_t(this->width), uint32_t(this->height), 1};
        vkCmdCopyImage(
            commandBuffer,
            this->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            newTexture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &copyRegion
        );

        fly::transitionImageLayout(
            commandBuffer, 
            newTexture->image, 
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT, 
            VK_ACCESS_SHADER_READ_BIT, 
            newTexture->mipLevels, 
            false
        );

        return newTexture;
    }
    


    //TEXTURE SAMPLER
    TextureSampler::TextureSampler(const VulkanInstance& vk, uint32_t mipLevels, Filter filter): vk{vk} {
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;            
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(vk.physicalDevice, &properties);
        
        if(filter == Filter::LINEAR) {
            samplerInfo.magFilter = VK_FILTER_LINEAR;
            samplerInfo.minFilter = VK_FILTER_LINEAR;
            samplerInfo.anisotropyEnable = VK_TRUE;
            samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
        } else if(filter == Filter::NEAREST) {
            samplerInfo.magFilter = VK_FILTER_NEAREST;
            samplerInfo.minFilter = VK_FILTER_NEAREST;
            samplerInfo.anisotropyEnable = VK_FALSE;
        }

        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

        if(filter == Filter::LINEAR)
            samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        else if(filter == Filter::NEAREST)
            samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
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