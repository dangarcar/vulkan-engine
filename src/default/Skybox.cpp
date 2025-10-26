#include "Skybox.hpp"

#include <Engine.hpp>
#include "../renderer/vulkan/Descriptors.hpp"

namespace fly {

    //SKYBOX IMPLEMENTATION
    Skybox::Skybox(VkCommandPool commandPool, std::unique_ptr<Texture> cubemap, std::unique_ptr<TextureSampler> cubemapSampler): cubemapSampler(std::move(cubemapSampler)),cubemap(std::move(cubemap)) {
        FLY_ASSERT(this->cubemap->isCubemap(), "Skybox texture must be a cubemap!");

        this->pipeline = Engine::get().addPipeline<SkyboxPipeline>(true); //Render in the background

        auto vertices = this->vertices;
        auto indices = this->indices;
        this->pipeline->attachModel(std::make_unique<SimpleVertexArray>(Engine::get().getVulkanInstance(), commandPool, std::move(vertices), std::move(indices)));
        this->pipeline->updateDescriptorSet(*this->cubemap, *this->cubemapSampler);
    }

    void Skybox::render(glm::mat4 projection, glm::mat4 view) {
        PushSkybox pc;
        pc.projection = projection;
        pc.view = view;

        this->pipeline->setPushConstant(0, pc);
    }


    //2D PIPELINE IMPLEMENTATION
    void SkyboxPipeline::updateDescriptorSet(
        const Texture& texture,
        const TextureSampler& textureSampler
    ) {
        FLY_ASSERT(this->meshes[0].descriptorSets.size() == MAX_FRAMES_IN_FLIGHT, "Descriptor set vector bad size!");

        for(int i=0; i<MAX_FRAMES_IN_FLIGHT; ++i) {
            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = texture.getImageView();
            imageInfo.sampler = textureSampler.getSampler();

            std::array<VkWriteDescriptorSet, 1> descriptorWrites{};

            descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[0].dstSet = this->meshes[0].descriptorSets[i];
            descriptorWrites[0].dstBinding = 0;
            descriptorWrites[0].dstArrayElement = 0;
            descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].pImageInfo = &imageInfo;

            vkUpdateDescriptorSets(vk->device, descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
        }
    }

    DescriptorSetLayout SkyboxPipeline::createDescriptorSetLayout() {
        return newDescriptorSetBuild(MAX_FRAMES_IN_FLIGHT, {
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