#include "DeferredShader.hpp"

#include "vulkan/VulkanHelpers.hpp"


namespace fly {

    DeferredShader::~DeferredShader() {
        vkDestroyDescriptorPool(vk->device, this->descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(vk->device, this->descriptorSetLayout.layout, nullptr);
        vkDestroyPipeline(vk->device, this->pipeline, nullptr);
        vkDestroyPipelineLayout(vk->device, this->pipelineLayout, nullptr);
    }

    void DeferredShader::run(VkCommandBuffer commandBuffer, uint32_t currentFrame) {
        //Output image from undef to general
        transitionImageLayout(
            commandBuffer, outputTexture->getImage(),
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0,
            VK_ACCESS_SHADER_WRITE_BIT,
            1, false
        );

        //Dispatch shader
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, this->pipeline);
        vkCmdBindDescriptorSets(
            commandBuffer, 
            VK_PIPELINE_BIND_POINT_COMPUTE, 
            this->pipelineLayout, 
            0, 
            1, 
            &this->descriptorSets[currentFrame], 
            0, 
            nullptr
        );
        uint32_t groupCountX = (vk->swapChainExtent.width + 15) / 16;
        uint32_t groupCountY = (vk->swapChainExtent.height + 15) / 16;
        vkCmdDispatch(commandBuffer, groupCountX, groupCountY, 1);


        //Output image from general to transfer dst (the format that the filters expect)
        transitionImageLayout(
            commandBuffer, outputTexture->getImage(),
            VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            1, false
        );
    }

}