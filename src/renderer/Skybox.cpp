#include "Skybox.hpp"

#include <Engine.hpp>

namespace fly {

    //SKYBOX IMPLEMENTATION
    Skybox::Skybox(Engine& engine, std::unique_ptr<Texture> cubemap, std::unique_ptr<TextureSampler> cubemapSampler): cubemapSampler(std::move(cubemapSampler)),cubemap(std::move(cubemap)) {
        assert(this->cubemap->isCubemap());

        this->pipeline = engine.addPipeline<SkyboxPipeline>(true); //Render in the background

        this->uniformBuffer = std::make_unique<TUniformBuffer<UBOSkybox>>(engine.getVulkanInstance());

        auto vertices = this->vertices;
        auto indices = this->indices;
        this->pipeline->attachModel(std::make_unique<SimpleVertexArray>(engine.getVulkanInstance(), engine.getCommandPool(), std::move(vertices), std::move(indices)));
        this->pipeline->updateDescriptorSet(*this->uniformBuffer, *this->cubemap, *this->cubemapSampler);
    }

    void Skybox::render(uint32_t currentFrame, glm::mat4 projection, glm::mat4 view) {
        UBOSkybox ubo;
        ubo.projection = projection;
        ubo.view = view;

        this->uniformBuffer->updateUBO(ubo, currentFrame);
    }


    //2D PIPELINE IMPLEMENTATION
    void SkyboxPipeline::updateDescriptorSet(
        const TUniformBuffer<UBOSkybox>& uniformBuffer,
        const Texture& texture,
        const TextureSampler& textureSampler
    ) {
        assert(this->meshes[0].descriptorSets.size() == MAX_FRAMES_IN_FLIGHT && "Descriptor set vector bad size!");

        for(int i=0; i<MAX_FRAMES_IN_FLIGHT; ++i) {
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = uniformBuffer.getBuffer(i);
            bufferInfo.offset = 0;
            bufferInfo.range = uniformBuffer.getSize();

            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = texture.getImageView();
            imageInfo.sampler = textureSampler.getSampler();

            std::vector<VkWriteDescriptorSet> descriptorWrites(2);

            descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[0].dstSet = this->meshes[0].descriptorSets[i];
            descriptorWrites[0].dstBinding = 0;
            descriptorWrites[0].dstArrayElement = 0;
            descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].pBufferInfo = &bufferInfo;

            descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[1].dstSet = this->meshes[0].descriptorSets[i];
            descriptorWrites[1].dstBinding = 1;
            descriptorWrites[1].dstArrayElement = 0;
            descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[1].descriptorCount = 1;
            descriptorWrites[1].pImageInfo = &imageInfo;

            vkUpdateDescriptorSets(vk.device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
        }
    }

    VkDescriptorSetLayout SkyboxPipeline::createDescriptorSetLayout() {
        VkDescriptorSetLayoutBinding uboLayoutBinding{};
        uboLayoutBinding.binding = 0;
        uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboLayoutBinding.descriptorCount = 1;
        uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        uboLayoutBinding.pImmutableSamplers = nullptr; // Optional

        VkDescriptorSetLayoutBinding samplerLayoutBinding{};
        samplerLayoutBinding.binding = 1;
        samplerLayoutBinding.descriptorCount = 1;
        samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerLayoutBinding.pImmutableSamplers = nullptr;
        samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uboLayoutBinding, samplerLayoutBinding};
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        VkDescriptorSetLayout descriptorSetLayout;
        if (vkCreateDescriptorSetLayout(vk.device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor set layout!");
        }

        return descriptorSetLayout;
    }

    VkDescriptorPool SkyboxPipeline::createDescriptorPool() {
        std::array<VkDescriptorPoolSize, 2> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

        VkDescriptorPool descriptorPool;
        if(vkCreateDescriptorPool(vk.device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor pool!");
        }

        return descriptorPool;
    }


    //VERTEX IMPLEMENTATION
    VkVertexInputBindingDescription SimpleVertex::getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(SimpleVertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescription;
    }
    
    std::vector<VkVertexInputAttributeDescription> SimpleVertex::getAttributeDescriptions() {
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions(1);
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(SimpleVertex, pos);

        return attributeDescriptions;
    }

    bool SimpleVertex::operator==(const SimpleVertex& other) const {
        return pos == other.pos;
    }
}