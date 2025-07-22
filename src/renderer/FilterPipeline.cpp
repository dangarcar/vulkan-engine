#include "FilterPipeline.hpp"

#include <Utils.hpp>
#include <cassert>

#include "vulkan/VulkanHelpers.hpp"
#include "vulkan/Descriptors.hpp"
#include "Texture.hpp"

#include <vulkan/vulkan_core.h>

namespace fly {
    
    FilterPipeline::~FilterPipeline() {
        vkDestroyDescriptorPool(vk.device, this->descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(vk.device, this->descriptorSetLayout.layout, nullptr);
        vkDestroyPipeline(vk.device, this->pipeline, nullptr);
        vkDestroyPipelineLayout(vk.device, this->pipelineLayout, nullptr);
    }

    void FilterPipeline::allocate() {
        this->descriptorSetLayout = createDescriptorSetLayout();
        this->descriptorPool = createDescriptorPoolWithLayout(this->descriptorSetLayout, this->vk);
        
        auto [pip, lay] = createComputePipeline(vk, this->descriptorSetLayout.layout, getShaderCode(), 0);
        this->pipeline = pip;
        this->pipelineLayout = lay;
        this->descriptorSets = allocateDescriptorSets(vk, this->descriptorSetLayout.layout, this->descriptorPool);

        createResources();
    }


    //GRAYSCALE FILTER IMPLEMENTATION
    std::vector<char> GrayscaleFilter::getShaderCode() {
        return readFile(GRAYSCALE_SHADER_SRC);
    }

    DescriptorSetLayout GrayscaleFilter::createDescriptorSetLayout() {
        return newDescriptorSetBuild(MAX_FRAMES_IN_FLIGHT, {
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT}
        }).build(vk);
    }

    void GrayscaleFilter::createResources() {
        this->computeInputImage = std::make_unique<Texture>(
            this->vk, 
            vk.swapChainExtent.width, vk.swapChainExtent.height, 
            VK_FORMAT_R16G16B16A16_SFLOAT, 
            VK_SAMPLE_COUNT_1_BIT,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT
        );

        this->computeOutputImage = std::make_unique<Texture>(
            this->vk, 
            vk.swapChainExtent.width, vk.swapChainExtent.height, 
            VK_FORMAT_R16G16B16A16_SFLOAT, 
            VK_SAMPLE_COUNT_1_BIT,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT
        );

        for(int i=0; i<MAX_FRAMES_IN_FLIGHT; ++i) {
            VkDescriptorImageInfo inputImageInfo{};
            inputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            inputImageInfo.imageView = this->computeInputImage->getImageView();

            VkDescriptorImageInfo outputImageInfo{};
            outputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            outputImageInfo.imageView = this->computeOutputImage->getImageView();

            std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
            descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[0].dstSet = this->descriptorSets[i];
            descriptorWrites[0].dstBinding = 0;
            descriptorWrites[0].dstArrayElement = 0;
            descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].pImageInfo = &inputImageInfo;

            descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[1].dstSet = this->descriptorSets[i];
            descriptorWrites[1].dstBinding = 1;
            descriptorWrites[1].dstArrayElement = 0;
            descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            descriptorWrites[1].descriptorCount = 1;
            descriptorWrites[1].pImageInfo = &outputImageInfo;

            vkUpdateDescriptorSets(vk.device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
        }
    }

    void GrayscaleFilter::applyFilter(VkCommandBuffer commandBuffer, VkImage inputImage, [[maybe_unused]] VkImage outputImage, uint32_t currentFrame) {
        assert(inputImage == outputImage && "Input image is not equal to output image");
        auto swapchainImage = inputImage;

        //swapchain image from color attach to transfer src
        transitionImageLayout(
            commandBuffer, swapchainImage,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_TRANSFER_READ_BIT,
            1, false
        );

        //compute input image from undef to transfer dst
        transitionImageLayout(
            commandBuffer, computeInputImage->getImage(),
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            1, false
        );

        // Copy from swapchain to compute input
        VkImageCopy copyRegion{};
        copyRegion.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copyRegion.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copyRegion.extent = {vk.swapChainExtent.width, vk.swapChainExtent.height, 1};

        vkCmdCopyImage(
            commandBuffer,
            swapchainImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            computeInputImage->getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &copyRegion
        );

        //compute input image from transfer dst to general
        transitionImageLayout(
            commandBuffer, computeInputImage->getImage(),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            1, false
        );

        //compute output image from undef to general
        transitionImageLayout(
            commandBuffer, computeOutputImage->getImage(),
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0,
            VK_ACCESS_SHADER_WRITE_BIT,
            1, false
        );

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
        uint32_t groupCountX = (vk.swapChainExtent.width + 15) / 16;
        uint32_t groupCountY = (vk.swapChainExtent.height + 15) / 16;
        vkCmdDispatch(commandBuffer, groupCountX, groupCountY, 1);

        //compute output image from general to transfer src
        transitionImageLayout(
            commandBuffer, computeOutputImage->getImage(),
            VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_TRANSFER_READ_BIT,
            1, false
        );

        //swapchain image from transfer src to transfer dst
        transitionImageLayout(
            commandBuffer, swapchainImage,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_TRANSFER_READ_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            1, false
        );

        vkCmdCopyImage(
            commandBuffer,
            computeOutputImage->getImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            swapchainImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &copyRegion
        );

        //swapchain image from transfer dst to color attach
        transitionImageLayout(
            commandBuffer, swapchainImage,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            1, false
        );
    }


    //TONEMAP FILTER IMPLEMENTATION
    std::vector<char> TonemapFilter::getShaderCode() {
        return readFile(TONEMAP_SHADER_SRC);
    }

    void TonemapFilter::setUbo(UBO ubo, uint32_t currentFrame) {
        this->uniformBuffer->updateBuffer(ubo, currentFrame);
    }

    DescriptorSetLayout TonemapFilter::createDescriptorSetLayout() {
        return newDescriptorSetBuild(MAX_FRAMES_IN_FLIGHT, {
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT},
        }).build(vk);
    }

    void TonemapFilter::createResources() {
        this->computeInputImage = std::make_unique<Texture>(
            this->vk, 
            vk.swapChainExtent.width, vk.swapChainExtent.height, 
            VK_FORMAT_R16G16B16A16_SFLOAT, 
            VK_SAMPLE_COUNT_1_BIT,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT
        );

        this->computeOutputImage = std::make_unique<Texture>(
            this->vk, 
            vk.swapChainExtent.width, vk.swapChainExtent.height, 
            VK_FORMAT_R8G8B8A8_UNORM, 
            VK_SAMPLE_COUNT_1_BIT,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT
        );

        this->uniformBuffer = std::make_unique<TBuffer<UBO>>(vk, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);


        for(int i=0; i<MAX_FRAMES_IN_FLIGHT; ++i) {
            VkDescriptorImageInfo inputImageInfo{};
            inputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            inputImageInfo.imageView = this->computeInputImage->getImageView();

            VkDescriptorImageInfo outputImageInfo{};
            outputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            outputImageInfo.imageView = this->computeOutputImage->getImageView();

            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = uniformBuffer->getBuffer(i);
            bufferInfo.offset = 0;
            bufferInfo.range = uniformBuffer->getSize();

            std::array<VkWriteDescriptorSet, 3> descriptorWrites{};
            descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[0].dstSet = this->descriptorSets[i];
            descriptorWrites[0].dstBinding = 0;
            descriptorWrites[0].dstArrayElement = 0;
            descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].pImageInfo = &inputImageInfo;

            descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[1].dstSet = this->descriptorSets[i];
            descriptorWrites[1].dstBinding = 1;
            descriptorWrites[1].dstArrayElement = 0;
            descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            descriptorWrites[1].descriptorCount = 1;
            descriptorWrites[1].pImageInfo = &outputImageInfo;

            descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[2].dstSet = this->descriptorSets[i];
            descriptorWrites[2].dstBinding = 2;
            descriptorWrites[2].dstArrayElement = 0;
            descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrites[2].descriptorCount = 1;
            descriptorWrites[2].pBufferInfo = &bufferInfo;

            vkUpdateDescriptorSets(vk.device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
        }
    }

    void TonemapFilter::applyFilter(VkCommandBuffer commandBuffer, VkImage inputImage, VkImage outputImage, uint32_t currentFrame) {
        //input image from color attach to transfer src
        transitionImageLayout(
            commandBuffer, inputImage,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_TRANSFER_READ_BIT,
            1, false
        );

        //compute input image from undef to transfer dst
        transitionImageLayout(
            commandBuffer, computeInputImage->getImage(),
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            1, false
        );

        // Copy from input image to compute input
        VkImageCopy copyRegion{};
        copyRegion.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copyRegion.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copyRegion.extent = {vk.swapChainExtent.width, vk.swapChainExtent.height, 1};

        vkCmdCopyImage(
            commandBuffer,
            inputImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            computeInputImage->getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &copyRegion
        );

        //compute input image from transfer dst to general
        transitionImageLayout(
            commandBuffer, computeInputImage->getImage(),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            1, false
        );

        //compute output image from undef to general
        transitionImageLayout(
            commandBuffer, computeOutputImage->getImage(),
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0,
            VK_ACCESS_SHADER_WRITE_BIT,
            1, false
        );

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
        uint32_t groupCountX = (vk.swapChainExtent.width + 15) / 16;
        uint32_t groupCountY = (vk.swapChainExtent.height + 15) / 16;
        vkCmdDispatch(commandBuffer, groupCountX, groupCountY, 1);

        //compute output image from general to transfer src
        transitionImageLayout(
            commandBuffer, computeOutputImage->getImage(),
            VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_TRANSFER_READ_BIT,
            1, false
        );

        //swapchain image from undef to transfer dst
        transitionImageLayout(
            commandBuffer, outputImage,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            1, false
        );

        vkCmdCopyImage(
            commandBuffer,
            computeOutputImage->getImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            outputImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &copyRegion
        );

        //swapchain image from transfer dst to color attach
        transitionImageLayout(
            commandBuffer, outputImage,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            1, false
        );
    }


    //BLOOM IMPLEMENTATION
    BloomFilter::~BloomFilter() {
        for(auto& s: computeSamplers)
            s.reset();

        for(auto& i: computeImages)
            i.reset();

        vkDestroyPipeline(vk.device, this->upsamplePipeline, nullptr);
        vkDestroyPipelineLayout(vk.device, this->upsamplePipelineLayout, nullptr);
    }

    std::vector<char> BloomFilter::getShaderCode() {
        return readFile(BLOOM_DOWNSAMPLE_SHADER_SRC);
    }

    std::vector<char> BloomFilter::getUpsampleShaderCode() {
        return readFile(BLOOM_UPSAMPLE_SHADER_SRC);
    }

    void BloomFilter::allocate() {
        this->descriptorSetLayout = createDescriptorSetLayout();
        this->descriptorPool = createDescriptorPoolWithLayout(this->descriptorSetLayout, this->vk);
        
        //DOWNSAMPLE
        auto [dPip, dLay] = createComputePipeline(vk, this->descriptorSetLayout.layout, getShaderCode(), sizeof(DownsamplePush));
        this->pipeline = dPip;
        this->pipelineLayout = dLay;
        
        //UPSAMPLE
        auto [uPip, uLay] = createComputePipeline(vk, this->descriptorSetLayout.layout, getUpsampleShaderCode(), sizeof(UpsamplePush));
        this->upsamplePipeline = uPip;
        this->upsamplePipelineLayout = uLay;
        
        for(int i=1; i<BLOOM_LEVELS; ++i)
            this->descriptorSetMap[{i-1, i}] = allocateDescriptorSets(vk, this->descriptorSetLayout.layout, this->descriptorPool);
        
        for(int i=BLOOM_LEVELS-1; i>0; --i)
            this->descriptorSetMap[{i, i-1}] = allocateDescriptorSets(vk, this->descriptorSetLayout.layout, this->descriptorPool);

        createResources();
    }

    void BloomFilter::createResources() {
        int width = vk.swapChainExtent.width, height = vk.swapChainExtent.height;
        for(int i=0; i<BLOOM_LEVELS; ++i) {
            auto usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            if(i == 0)
                usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            
            this->computeImages[i] = std::make_unique<Texture>(
                this->vk, 
                width, height, 
                VK_FORMAT_R16G16B16A16_SFLOAT, 
                VK_SAMPLE_COUNT_1_BIT,
                usage,
                VK_IMAGE_ASPECT_COLOR_BIT
            );

            this->computeSamplers[i] = std::make_unique<TextureSampler>(this->vk, computeImages[i]->getMipLevels());
            this->computeImageSizes[i] = {width, height};
        
            width /= 2;
            height /= 2;
        }

        for(int i=1; i<BLOOM_LEVELS; ++i) {
            updateDescriptorSet(i-1, i);
        }

        for(int i=BLOOM_LEVELS-1; i>0; --i) {
            updateDescriptorSet(i, i-1);
        }
    }

    DescriptorSetLayout BloomFilter::createDescriptorSetLayout() {
        return newDescriptorSetBuild(MAX_FRAMES_IN_FLIGHT * 2 * BLOOM_LEVELS, {
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT}
        }).build(vk);
    }

    void BloomFilter::updateDescriptorSet(int inputLevel, int outputLevel) {
        for(int i=0; i<MAX_FRAMES_IN_FLIGHT; ++i) {
            VkDescriptorImageInfo inputSamplerInfo{};
            inputSamplerInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            inputSamplerInfo.imageView = this->computeImages[inputLevel]->getImageView();
            inputSamplerInfo.sampler = this->computeSamplers[inputLevel]->getSampler();

            VkDescriptorImageInfo outputImageInfo{};
            outputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            outputImageInfo.imageView = this->computeImages[outputLevel]->getImageView();

            std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
            descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[0].dstSet = this->descriptorSetMap[{inputLevel, outputLevel}][i];
            descriptorWrites[0].dstBinding = 0;
            descriptorWrites[0].dstArrayElement = 0;
            descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].pImageInfo = &inputSamplerInfo;

            descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[1].dstSet = this->descriptorSetMap[{inputLevel, outputLevel}][i];
            descriptorWrites[1].dstBinding = 1;
            descriptorWrites[1].dstArrayElement = 0;
            descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            descriptorWrites[1].descriptorCount = 1;
            descriptorWrites[1].pImageInfo = &outputImageInfo;

            vkUpdateDescriptorSets(vk.device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
        }
    }

    void BloomFilter::applyFilter(VkCommandBuffer commandBuffer, VkImage inputImage, [[maybe_unused]] VkImage outputImage, uint32_t currentFrame) {
        assert(inputImage == outputImage && "Input image is not equal to output image");
        auto swapchainImage = inputImage;

        //swapchain image from color attach to transfer src
        transitionImageLayout(
            commandBuffer, swapchainImage,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_TRANSFER_READ_BIT,
            1, false
        );

        //compute base image from undef to transfer dst
        transitionImageLayout(
            commandBuffer, computeImages[0]->getImage(),
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            1, false
        );

        // Copy from swapchain to compute input
        VkImageCopy copyRegion{};
        copyRegion.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copyRegion.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copyRegion.extent = {vk.swapChainExtent.width, vk.swapChainExtent.height, 1};

        vkCmdCopyImage(
            commandBuffer,
            swapchainImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            computeImages[0]->getImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &copyRegion
        );

        //compute base image from transfer dst to general
        transitionImageLayout(
            commandBuffer, computeImages[0]->getImage(),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            1, false
        );


        //DOWNSCALING
        for(int i=1; i<BLOOM_LEVELS; ++i) {
            //compute output image from undef to general
            transitionImageLayout(
                commandBuffer, computeImages[i]->getImage(),
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_GENERAL,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0,
                VK_ACCESS_SHADER_WRITE_BIT,
                1, false
            );
            
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, this->pipeline);
            vkCmdBindDescriptorSets(
                commandBuffer, 
                VK_PIPELINE_BIND_POINT_COMPUTE, 
                this->pipelineLayout, 
                0, 
                1, 
                &this->descriptorSetMap[{i-1, i}][currentFrame], 
                0, 
                nullptr
            );
            
            auto srcTexelSize = glm::vec2(1.0) / this->computeImageSizes[i-1];
            auto invNormCurrResolution = glm::vec2(1.0) / (this->computeImageSizes[i] - glm::vec2(1.0));
            DownsamplePush constants {srcTexelSize, invNormCurrResolution};
            vkCmdPushConstants(
                commandBuffer, 
                this->pipelineLayout, 
                VK_SHADER_STAGE_COMPUTE_BIT, 
                0, sizeof(DownsamplePush), 
                &constants
            );
            
            auto size = this->computeImageSizes[i];
            uint32_t groupCountX = (size.x + 15) / 16;
            uint32_t groupCountY = (size.y + 15) / 16;
            vkCmdDispatch(commandBuffer, groupCountX, groupCountY, 1);
            
            VkMemoryBarrier memoryBarrier{};
            memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                    
            vkCmdPipelineBarrier(
                commandBuffer,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0,
                1, &memoryBarrier,
                0, nullptr,
                0, nullptr
            );
        }


        //UPSCALING
        for(int i=BLOOM_LEVELS-1; i>0; --i) {
            auto size = this->computeImageSizes[i-1];
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, this->upsamplePipeline);
            vkCmdBindDescriptorSets(
                commandBuffer, 
                VK_PIPELINE_BIND_POINT_COMPUTE, 
                this->upsamplePipelineLayout, 
                0, 
                1, 
                &this->descriptorSetMap[{i, i-1}][currentFrame], 
                0, 
                nullptr
            );
            
            auto invNormCurrResolution = glm::vec2(1.0) / (size - glm::vec2(1.0));
            UpsamplePush constants{invNormCurrResolution, filterRadius, bloomIntensity};
            vkCmdPushConstants(
                commandBuffer, 
                this->upsamplePipelineLayout, 
                VK_SHADER_STAGE_COMPUTE_BIT, 
                0, sizeof(UpsamplePush), 
                &constants
            );
            
            uint32_t groupCountX = (size.x + 15) / 16;
            uint32_t groupCountY = (size.y + 15) / 16;
            vkCmdDispatch(commandBuffer, groupCountX, groupCountY, 1);

            VkMemoryBarrier memoryBarrier{};
            memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            vkCmdPipelineBarrier(
                commandBuffer,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0,
                1, &memoryBarrier,
                0, nullptr,
                0, nullptr
            );
        }


        //compute base image from general to transfer src
        transitionImageLayout(
            commandBuffer, computeImages[0]->getImage(),
            VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_TRANSFER_READ_BIT,
            1, false
        );

        //swapchain image from transfer src to transfer dst
        transitionImageLayout(
            commandBuffer, swapchainImage,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_TRANSFER_READ_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            1, false
        );

        vkCmdCopyImage(
            commandBuffer,
            computeImages[0]->getImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            swapchainImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &copyRegion
        );

        //swapchain image from transfer dst to color attach
        transitionImageLayout(
            commandBuffer, swapchainImage,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            1, false
        );
    }

}