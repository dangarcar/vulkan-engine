#pragma once

#include "vulkan/VulkanTypes.h"
#include "vulkan/VulkanConstants.h"
#include <memory>
#include <array>

namespace fly {

    class Texture;

    class FilterPipeline {
    public:
        FilterPipeline(const VulkanInstance& vk): vk{vk} {}
        virtual ~FilterPipeline();

        void allocate();
        
        virtual void createResources() = 0;
        virtual void applyFilter(VkCommandBuffer commandBuffer, VkImage inputImage, VkImage outputImage, uint32_t currentFrame) = 0;

    protected:
        virtual std::vector<char> getShaderCode() = 0;
        virtual VkDescriptorPool createDescriptorPool() = 0;
        virtual VkDescriptorSetLayout createDescriptorSetLayout() = 0;
        
        const VulkanInstance& vk;
        std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> descriptorSets;
        VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        VkPipeline pipeline = VK_NULL_HANDLE;
        VkDescriptorPool descriptorPool;
    
    private:
        void createComputePipeline();
        static std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> allocateDescriptorSets(const VulkanInstance& vk, VkDescriptorSetLayout descriptorSetLayout, VkDescriptorPool descriptorPool);
    
    };

    class GrayscaleFilter: public FilterPipeline {
    private:
        static constexpr const char* GRAYSCALE_SHADER_SRC = "vulkan-engine/shaders/grayscale.spv";
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
        static constexpr const char* TONEMAP_SHADER_SRC = "vulkan-engine/shaders/tonemap.spv";
    public:
        TonemapFilter(const VulkanInstance& vk): FilterPipeline(vk) {}

        void createResources() override;
        void applyFilter(VkCommandBuffer commandBuffer, VkImage inputImage, VkImage outputImage, uint32_t currentFrame) override;

    private:
        std::unique_ptr<Texture> computeInputImage, computeOutputImage;  

    protected:
        std::vector<char> getShaderCode() override;
        VkDescriptorPool createDescriptorPool() override;
        VkDescriptorSetLayout createDescriptorSetLayout() override;

    };

}
