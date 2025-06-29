#include "FilterPipeline.hpp"

#include <Utils.hpp>
#include <cassert>

#include "vulkan/VulkanHelpers.hpp"
#include "Texture.hpp"

#include <vulkan/vulkan_core.h>

namespace fly {
    
    FilterPipeline::~FilterPipeline() {
        vkDestroyDescriptorPool(vk.device, this->descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(vk.device, this->descriptorSetLayout, nullptr);
        vkDestroyPipeline(vk.device, this->pipeline, nullptr);
        vkDestroyPipelineLayout(vk.device, this->pipelineLayout, nullptr);
    }

    void FilterPipeline::allocate() {
        this->descriptorPool = createDescriptorPool();
        this->descriptorSetLayout = createDescriptorSetLayout();
        
        auto [pip, lay] = createComputePipeline(vk, this->descriptorSetLayout, getShaderCode(), 0);
        this->pipeline = pip;
        this->pipelineLayout = lay;
        this->descriptorSets = allocateDescriptorSets(vk, this->descriptorSetLayout, this->descriptorPool);

        createResources();
    }

    std::pair<VkPipeline, VkPipelineLayout> FilterPipeline::createComputePipeline(const VulkanInstance& vk, VkDescriptorSetLayout descriptorSetLayout, const std::vector<char>& shaderCode, size_t pushConstantSize) {
        VkShaderModule computeShaderModule = createShaderModule(vk.device, shaderCode);

        VkPipelineShaderStageCreateInfo computeShaderStageInfo{};
        computeShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        computeShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        computeShaderStageInfo.module = computeShaderModule;
        computeShaderStageInfo.pName = "main";

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;

        if(pushConstantSize > 0) {
            VkPushConstantRange pushConstant;
            pushConstant.offset = 0;
	        pushConstant.size = pushConstantSize;
	        pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

            pipelineLayoutInfo.pPushConstantRanges = &pushConstant;
	        pipelineLayoutInfo.pushConstantRangeCount = 1;
        }

        VkPipelineLayout pipelineLayout;
        if(vkCreatePipelineLayout(vk.device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create compute pipeline layout!");
        }

        VkComputePipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.layout = pipelineLayout;
        pipelineInfo.stage = computeShaderStageInfo;

        VkPipeline pipeline;
        if(vkCreateComputePipelines(vk.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
            throw std::runtime_error("failed to create compute pipeline!");
        }

        vkDestroyShaderModule(vk.device, computeShaderModule, nullptr);

        return std::make_pair(pipeline, pipelineLayout);
    }

    std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> FilterPipeline::allocateDescriptorSets(
        const VulkanInstance& vk, 
        VkDescriptorSetLayout descriptorSetLayout,
        VkDescriptorPool descriptorPool
    ) {
        std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        allocInfo.pSetLayouts = layouts.data();
    
        std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> descriptorSets;
        if(vkAllocateDescriptorSets(vk.device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate descriptor sets!");
        }
    
        return descriptorSets;
    } 


    //GRAYSCALE FILTER IMPLEMENTATION
    std::vector<char> GrayscaleFilter::getShaderCode() {
        return readFile(GRAYSCALE_SHADER_SRC);
    }

    VkDescriptorPool GrayscaleFilter::createDescriptorPool() {
        std::array<VkDescriptorPoolSize, 2> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

        VkDescriptorPool descriptorPool;
        if(vkCreateDescriptorPool(vk.device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create compute descriptor pool!");
        }

        return descriptorPool;
    }

    VkDescriptorSetLayout GrayscaleFilter::createDescriptorSetLayout() {
        VkDescriptorSetLayoutBinding inputImageBinding{};
        inputImageBinding.binding = 0;
        inputImageBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        inputImageBinding.descriptorCount = 1;
        inputImageBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutBinding outputImageBinding{};
        outputImageBinding.binding = 1;
        outputImageBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        outputImageBinding.descriptorCount = 1;
        outputImageBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        std::array<VkDescriptorSetLayoutBinding, 2> bindings = {inputImageBinding, outputImageBinding};
        
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        VkDescriptorSetLayout descriptorSetLayout;
        if(vkCreateDescriptorSetLayout(vk.device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create compute descriptor set layout!");
        }

        return descriptorSetLayout;
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

    void GrayscaleFilter::applyFilter(VkCommandBuffer commandBuffer, VkImage inputImage, VkImage outputImage, uint32_t currentFrame) {
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
        uint32_t groupCountX = vk.swapChainExtent.width / 16;
        uint32_t groupCountY = vk.swapChainExtent.height / 16;
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
        this->uniformBuffer->updateUBO(ubo, currentFrame);
    }

    VkDescriptorPool TonemapFilter::createDescriptorPool() {
        std::array<VkDescriptorPoolSize, 3> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        poolSizes[2].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[2].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

        VkDescriptorPool descriptorPool;
        if(vkCreateDescriptorPool(vk.device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create compute descriptor pool!");
        }

        return descriptorPool;
    }

    VkDescriptorSetLayout TonemapFilter::createDescriptorSetLayout() {
        VkDescriptorSetLayoutBinding inputImageBinding{};
        inputImageBinding.binding = 0;
        inputImageBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        inputImageBinding.descriptorCount = 1;
        inputImageBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutBinding outputImageBinding{};
        outputImageBinding.binding = 1;
        outputImageBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        outputImageBinding.descriptorCount = 1;
        outputImageBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutBinding uniformBufferBinding{};
        uniformBufferBinding.binding = 2;
        uniformBufferBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uniformBufferBinding.descriptorCount = 1;
        uniformBufferBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        std::array<VkDescriptorSetLayoutBinding, 3> bindings = {inputImageBinding, outputImageBinding,uniformBufferBinding};
        
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        VkDescriptorSetLayout descriptorSetLayout;
        if(vkCreateDescriptorSetLayout(vk.device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create compute descriptor set layout!");
        }

        return descriptorSetLayout;
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

        this->uniformBuffer = std::make_unique<TUniformBuffer<UBO>>(vk);


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
        uint32_t groupCountX = vk.swapChainExtent.width / 16;
        uint32_t groupCountY = vk.swapChainExtent.height / 16;
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
        this->descriptorPool = createDescriptorPool();
        this->descriptorSetLayout = createDescriptorSetLayout();
        
        //DOWNSAMPLE
        auto [dPip, dLay] = createComputePipeline(vk, this->descriptorSetLayout, getShaderCode(), sizeof(DownsamplePush));
        this->pipeline = dPip;
        this->pipelineLayout = dLay;
        
        //UPSAMPLE
        auto [uPip, uLay] = createComputePipeline(vk, this->descriptorSetLayout, getUpsampleShaderCode(), sizeof(UpsamplePush));
        this->upsamplePipeline = uPip;
        this->upsamplePipelineLayout = uLay;
        
        for(int i=1; i<BLOOM_LEVELS; ++i)
            this->descriptorSetMap[{i-1, i}] = allocateDescriptorSets(vk, this->descriptorSetLayout, this->descriptorPool);
        
        for(int i=BLOOM_LEVELS-1; i>0; --i)
            this->descriptorSetMap[{i, i-1}] = allocateDescriptorSets(vk, this->descriptorSetLayout, this->descriptorPool);

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

    VkDescriptorPool BloomFilter::createDescriptorPool() {
        std::array<VkDescriptorPoolSize, 2> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT) * 2 * BLOOM_LEVELS;
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT) * 2 * BLOOM_LEVELS;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT) * 2 * BLOOM_LEVELS;

        VkDescriptorPool descriptorPool;
        if(vkCreateDescriptorPool(vk.device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create compute descriptor pool!");
        }

        return descriptorPool;
    }

    VkDescriptorSetLayout BloomFilter::createDescriptorSetLayout() {
        VkDescriptorSetLayoutBinding inputSamplerBinding{};
        inputSamplerBinding.binding = 0;
        inputSamplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        inputSamplerBinding.descriptorCount = 1;
        inputSamplerBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutBinding outputImageBinding{};
        outputImageBinding.binding = 1;
        outputImageBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        outputImageBinding.descriptorCount = 1;
        outputImageBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        std::array<VkDescriptorSetLayoutBinding, 2> bindings = {inputSamplerBinding, outputImageBinding};
        
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        VkDescriptorSetLayout descriptorSetLayout;
        if(vkCreateDescriptorSetLayout(vk.device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create compute descriptor set layout!");
        }

        return descriptorSetLayout;
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

    void BloomFilter::applyFilter(VkCommandBuffer commandBuffer, VkImage inputImage, VkImage outputImage, uint32_t currentFrame) {
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