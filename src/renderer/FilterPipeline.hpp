#pragma once

#include "TUniformBuffer.hpp"
#include "vulkan/VulkanTypes.h"
#include "vulkan/VulkanConstants.h"
#include <memory>
#include <array>
#include <map>
#include <glm/glm.hpp>

namespace fly {

    class Texture;
    class TextureSampler;

    class FilterPipeline {
    public:
        FilterPipeline(const VulkanInstance& vk): vk{vk} {}
        virtual ~FilterPipeline();

        virtual void allocate(); // Has default implementation, but can be overriden
        
        virtual void createResources() = 0;
        virtual void applyFilter(VkCommandBuffer commandBuffer, VkImage inputImage, VkImage outputImage, uint32_t currentFrame) = 0;

    protected:
        virtual std::vector<char> getShaderCode() = 0;
        virtual VkDescriptorPool createDescriptorPool() = 0;
        virtual VkDescriptorSetLayout createDescriptorSetLayout() = 0;
        
        const VulkanInstance& vk;
        std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> descriptorSets;
        VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        VkPipeline pipeline = VK_NULL_HANDLE;
    
        static std::pair<VkPipeline, VkPipelineLayout> createComputePipeline(const VulkanInstance& vk, VkDescriptorSetLayout descriptorSetLayout, const std::vector<char>& shaderCode, size_t pushConstantSize);
        static std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> allocateDescriptorSets(const VulkanInstance& vk, VkDescriptorSetLayout descriptorSetLayout, VkDescriptorPool descriptorPool);
    
    };

    class GrayscaleFilter: public FilterPipeline {
    private:
        static constexpr const char* GRAYSCALE_SHADER_SRC = "vulkan-engine/shaders/filters/grayscale.spv";
    public:
        GrayscaleFilter(const VulkanInstance& vk): FilterPipeline(vk) {}

        void createResources() override;
        void applyFilter(VkCommandBuffer commandBuffer, VkImage inputImage, VkImage outputImage, uint32_t currentFrame) override;

    private:
        std::unique_ptr<Texture> computeInputImage, computeOutputImage;  

    protected:
        std::vector<char> getShaderCode() override;
        VkDescriptorPool createDescriptorPool() override;
        VkDescriptorSetLayout createDescriptorSetLayout() override;

    };

    class TonemapFilter: public FilterPipeline {
    private:
        static constexpr const char* TONEMAP_SHADER_SRC = "vulkan-engine/shaders/filters/tonemap.spv";
    public:
        struct UBO {
            float exposure;
        };

        TonemapFilter(const VulkanInstance& vk): FilterPipeline(vk) {}

        void createResources() override;
        void applyFilter(VkCommandBuffer commandBuffer, VkImage inputImage, VkImage outputImage, uint32_t currentFrame) override;
        void setUbo(UBO ubo, uint32_t currentFrame);

    private:
        std::unique_ptr<Texture> computeInputImage, computeOutputImage;
        std::unique_ptr<TUniformBuffer<UBO>> uniformBuffer;
        UBO ubo;

    protected:
        std::vector<char> getShaderCode() override;
        VkDescriptorPool createDescriptorPool() override;
        VkDescriptorSetLayout createDescriptorSetLayout() override;

    };

    class BloomFilter: public FilterPipeline {
    private:
        static constexpr const char* BLOOM_DOWNSAMPLE_SHADER_SRC = "vulkan-engine/shaders/filters/bloom_downsample.spv";
        static constexpr const char* BLOOM_UPSAMPLE_SHADER_SRC = "vulkan-engine/shaders/filters/bloom_upsample.spv";
        static constexpr int BLOOM_LEVELS = 5;
    public:
        struct UpsamplePush { glm::vec2 invNormCurrResolution, filterRadius; float bloomIntensity; };
        struct DownsamplePush { glm::vec2 srcTexelSize, invNormCurrResolution; };

        BloomFilter(const VulkanInstance& vk): FilterPipeline(vk) {}
        ~BloomFilter() override;

        void allocate() override;
        void createResources() override;
        void applyFilter(VkCommandBuffer commandBuffer, VkImage inputImage, VkImage outputImage, uint32_t currentFrame) override;
        void setFilterRadius(glm::vec2 radius) { this->filterRadius = radius; }
        void settBloomIntensity(float bloom) { this->bloomIntensity = bloom; }

    private:
        std::array<std::unique_ptr<fly::TextureSampler>, BLOOM_LEVELS> computeSamplers;
        std::array<std::unique_ptr<Texture>, BLOOM_LEVELS> computeImages;
        std::array<glm::vec2, BLOOM_LEVELS> computeImageSizes;
        VkPipelineLayout upsamplePipelineLayout = VK_NULL_HANDLE;
        VkPipeline upsamplePipeline = VK_NULL_HANDLE;
        
        std::map<std::pair<int,int>, std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT>> descriptorSetMap;
        glm::vec2 filterRadius;
        float bloomIntensity;

    protected:
        std::vector<char> getShaderCode() override;
        VkDescriptorPool createDescriptorPool() override;
        VkDescriptorSetLayout createDescriptorSetLayout() override;
        
    private:
        std::vector<char> getUpsampleShaderCode();
        void updateDescriptorSet(int inputLevel, int outputLevel);    

    };

}
