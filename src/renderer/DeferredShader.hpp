#pragma once

#include "Texture.hpp"
#include "vulkan/VulkanConstants.h"


namespace fly {

    //Deferred shader abstract class with virtual methods
    class DeferredShader {
    public:
        virtual void updateShader(
            std::shared_ptr<Texture> hdrColorTexture,
            std::shared_ptr<Texture> albedoSpecTexture, 
            std::shared_ptr<Texture> positionsTexture, 
            std::shared_ptr<Texture> normalsTexture,
            std::shared_ptr<Texture> pickingTexture
        ) = 0;

        virtual void run(VkCommandBuffer commandBuffer, uint32_t currentFrame);
        virtual ~DeferredShader();
    
    protected:
        DeferredShader(std::shared_ptr<VulkanInstance> vk): vk{vk} {}
        
        std::shared_ptr<VulkanInstance> vk;
        std::shared_ptr<Texture> outputTexture;

        std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> descriptorSets;
        DescriptorSetLayout descriptorSetLayout;
        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        VkPipeline pipeline = VK_NULL_HANDLE;
    };

}