#pragma once

#include "Utils.hpp"
#include "vulkan/VulkanTypes.h"
#include "vulkan/VulkanConstants.h"

#include <glm/glm.hpp>
#include <memory>
#include <array>
#include <map>

namespace fly {

    class Texture;
    class TextureSampler;


    class FilterPipeline {
    public:
        FilterPipeline(std::shared_ptr<VulkanInstance> vk): vk{vk} {}
        virtual ~FilterPipeline();

        virtual void allocate(); // Has default implementation, but can be overriden
        virtual void createResources() = 0;
        
        virtual void applyFilter(VkCommandBuffer commandBuffer, VkImage image, uint32_t currentFrame) = 0;

    protected:
        virtual std::vector<char> getShaderCode() = 0;
        virtual DescriptorSetLayout createDescriptorSetLayout() = 0;
        
        std::shared_ptr<VulkanInstance> vk;
        std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> descriptorSets;
        DescriptorSetLayout descriptorSetLayout;
        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        VkPipeline pipeline = VK_NULL_HANDLE;
        size_t pushConstantSize = 0;

    };



    class GrayscaleFilter: public FilterPipeline {
    private:
        static constexpr const char* GRAYSCALE_SHADER_SRC = "vulkan-engine/shaders/filters/bin/grayscale.comp.spv";
    public:
        GrayscaleFilter(std::shared_ptr<VulkanInstance> vk): FilterPipeline(vk) {}

        void createResources() override;
        void applyFilter(VkCommandBuffer commandBuffer, VkImage image, uint32_t currentFrame) override;

    private:
        std::unique_ptr<Texture> computeInputImage, computeOutputImage;  

    protected:
        std::vector<char> getShaderCode() override;
        DescriptorSetLayout createDescriptorSetLayout() override;

    };



    class BloomFilter: public FilterPipeline {
    private:
        static constexpr const char* BLOOM_DOWNSAMPLE_SHADER_SRC = "vulkan-engine/shaders/filters/bin/bloom_downsample.comp.spv";
        static constexpr const char* BLOOM_UPSAMPLE_SHADER_SRC = "vulkan-engine/shaders/filters/bin/bloom_upsample.comp.spv";
        static constexpr int BLOOM_LEVELS = 6;
    public:
        struct UpsamplePush { glm::vec2 invNormCurrResolution, filterRadius; float bloomIntensity; };
        struct DownsamplePush { glm::vec2 srcTexelSize, invNormCurrResolution; float bloomThreshold; int srcIndex; };

        BloomFilter(std::shared_ptr<VulkanInstance> vk): FilterPipeline(vk) {}
        ~BloomFilter() override;

        void allocate() override;
        void createResources() override;
        void applyFilter(VkCommandBuffer commandBuffer, VkImage image, uint32_t currentFrame) override;
        void setFilterRadius(glm::vec2 radius) { this->filterRadius = radius; }
        void settBloomIntensity(float bloom) { this->bloomIntensity = bloom; }
        void settBloomThreshold(float threshold) { this->bloomThreshold = threshold; }

    private:
        std::array<std::unique_ptr<fly::TextureSampler>, BLOOM_LEVELS> computeSamplers;
        std::array<std::unique_ptr<Texture>, BLOOM_LEVELS> computeImages;
        std::array<glm::vec2, BLOOM_LEVELS> computeImageSizes;
        VkPipelineLayout upsamplePipelineLayout = VK_NULL_HANDLE;
        VkPipeline upsamplePipeline = VK_NULL_HANDLE;
        
        std::map<std::pair<int,int>, std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT>> descriptorSetMap;
        glm::vec2 filterRadius;
        float bloomIntensity, bloomThreshold = 0;

    protected:
        std::vector<char> getShaderCode() override;
        DescriptorSetLayout createDescriptorSetLayout() override;
        
    private:
        std::vector<char> getUpsampleShaderCode();
        void updateDescriptorSet(int inputLevel, int outputLevel);    

    };



    /*
    This class doesn't need to follow the interface as it is controlled by the engine, but I wanted it to inherit the FilterPipeline behaviour
    */
    class Tonemapper: public FilterPipeline {
    private:
        static constexpr const char* TONEMAP_SHADER_SRC = "vulkan-engine/shaders/filters/bin/tonemap.comp.spv";
    public:
        struct TonemapPush {
            float exposure, invGamma;
        };

        Tonemapper(std::shared_ptr<VulkanInstance> vk): FilterPipeline(vk) {
            this->pushConstantSize = sizeof(TonemapPush);
        }

        void createResources(std::shared_ptr<Texture> hdrColorTexture);
        void applyFilter(VkCommandBuffer commandBuffer, VkImage swapchainImage, uint32_t currentFrame) override;
        void setExposure(float exposure) { this->exposure = exposure; }
        void setGamma(float gamma) { this->gamma = gamma; }
        
        private:
        std::shared_ptr<Texture> hdrColorTexture;
        std::unique_ptr<Texture> computeOutputImage;
        float exposure = 1.0, gamma = 2.2;

        void createResources() override { FLY_ASSERT(0, "YOU MUST NOT USE THIS METHOD!!"); }

    protected:
        std::vector<char> getShaderCode() override { return readFile(TONEMAP_SHADER_SRC); }
        DescriptorSetLayout createDescriptorSetLayout() override;

    };

}
