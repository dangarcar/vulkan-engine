#include "DeferredShader.hpp"

#include "Utils.hpp"

#include "vulkan/Descriptors.hpp"
#include "vulkan/VulkanHelpers.hpp"

namespace fly {


    //DEFERRED SHADER COMMON METHODS
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



    //DEFAULT DEFERRED SHADER IMPLEMENTATION
    DefaultDeferredShader::DefaultDeferredShader(std::shared_ptr<VulkanInstance> vk): DeferredShader(vk) {
        this->uniformBuffer = std::make_unique<fly::TBuffer<DeferredUBO>>(vk, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        
        //DESCRIPTOR LAYOUT CREATION
        this->descriptorSetLayout = newDescriptorSetBuild(MAX_FRAMES_IN_FLIGHT, {
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT}
        }).build(vk);;
        this->descriptorPool = createDescriptorPoolWithLayout(this->descriptorSetLayout, this->vk);
        
        //PIPELINE AND DESCRIPTOR SET CREATION
        auto [pip, lay] = createComputePipeline(vk, this->descriptorSetLayout.layout, readFile(DEFERRED_SHADER_SRC), 0);
        this->pipeline = pip;
        this->pipelineLayout = lay;
        this->descriptorSets = allocateDescriptorSets(vk, this->descriptorSetLayout.layout, this->descriptorPool);
    }


    void DefaultDeferredShader::updateShader(
        std::shared_ptr<Texture> hdrColorTexture,
        std::shared_ptr<Texture> albedoSpecTexture, 
        std::shared_ptr<Texture> positionsTexture, 
        std::shared_ptr<Texture> normalsTexture,
        [[maybe_unused]] std::shared_ptr<Texture> pickingTexture
    ) {
        this->outputTexture = hdrColorTexture;

        //DESCRIPTOR BINDING
        for(int i=0; i<MAX_FRAMES_IN_FLIGHT; ++i) {
            VkDescriptorImageInfo outputImageInfo{};
            outputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            outputImageInfo.imageView = this->outputTexture->getImageView();

            VkDescriptorImageInfo albedoImageInfo{};
            albedoImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            albedoImageInfo.imageView = albedoSpecTexture->getImageView();

            VkDescriptorImageInfo posImageInfo{};
            posImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            posImageInfo.imageView = positionsTexture->getImageView();

            VkDescriptorImageInfo normalsImageInfo{};
            normalsImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            normalsImageInfo.imageView = normalsTexture->getImageView();

            VkDescriptorBufferInfo uboInfo{};
            uboInfo.buffer = uniformBuffer->getBuffer(i);
            uboInfo.offset = 0;
            uboInfo.range = uniformBuffer->getSize();

            std::array<VkWriteDescriptorSet, 5> descriptorWrites{};
            descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[0].dstSet = this->descriptorSets[i];
            descriptorWrites[0].dstBinding = 0;
            descriptorWrites[0].dstArrayElement = 0;
            descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].pImageInfo = &outputImageInfo;

            descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[1].dstSet = this->descriptorSets[i];
            descriptorWrites[1].dstBinding = 1;
            descriptorWrites[1].dstArrayElement = 0;
            descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            descriptorWrites[1].descriptorCount = 1;
            descriptorWrites[1].pImageInfo = &albedoImageInfo;

            descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[2].dstSet = this->descriptorSets[i];
            descriptorWrites[2].dstBinding = 2;
            descriptorWrites[2].dstArrayElement = 0;
            descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            descriptorWrites[2].descriptorCount = 1;
            descriptorWrites[2].pImageInfo = &posImageInfo;

            descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[3].dstSet = this->descriptorSets[i];
            descriptorWrites[3].dstBinding = 3;
            descriptorWrites[3].dstArrayElement = 0;
            descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            descriptorWrites[3].descriptorCount = 1;
            descriptorWrites[3].pImageInfo = &normalsImageInfo;

            descriptorWrites[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[4].dstSet = this->descriptorSets[i];
            descriptorWrites[4].dstBinding = 4;
            descriptorWrites[4].dstArrayElement = 0;
            descriptorWrites[4].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrites[4].descriptorCount = 1;
            descriptorWrites[4].pBufferInfo = &uboInfo;

            vkUpdateDescriptorSets(vk->device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
        }
    }
}