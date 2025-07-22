#include "Skybox.hpp"

#include <Engine.hpp>
#include "vulkan/Descriptors.hpp"

namespace fly {

    //SKYBOX IMPLEMENTATION
    Skybox::Skybox(Engine& engine, std::unique_ptr<Texture> cubemap, std::unique_ptr<TextureSampler> cubemapSampler): cubemapSampler(std::move(cubemapSampler)),cubemap(std::move(cubemap)) {
        assert(this->cubemap->isCubemap());

        this->pipeline = engine.addPipeline<SkyboxPipeline>(true); //Render in the background

        this->uniformBuffer = std::make_unique<TBuffer<UBOSkybox>>(engine.getVulkanInstance(), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

        auto vertices = this->vertices;
        auto indices = this->indices;
        this->pipeline->attachModel(std::make_unique<SimpleVertexArray>(engine.getVulkanInstance(), engine.getCommandPool(), std::move(vertices), std::move(indices)));
        this->pipeline->updateDescriptorSet(*this->uniformBuffer, *this->cubemap, *this->cubemapSampler);
    }

    void Skybox::render(uint32_t currentFrame, glm::mat4 projection, glm::mat4 view) {
        UBOSkybox ubo;
        ubo.projection = projection;
        ubo.view = view;

        this->uniformBuffer->updateBuffer(ubo, currentFrame);
    }


    //2D PIPELINE IMPLEMENTATION
    void SkyboxPipeline::updateDescriptorSet(
        const TBuffer<UBOSkybox>& uniformBuffer,
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

    DescriptorSetLayout SkyboxPipeline::createDescriptorSetLayout() {
        return newDescriptorSetBuild(MAX_FRAMES_IN_FLIGHT, {
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT}
        }).build(vk);
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