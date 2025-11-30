#include "Renderer2d.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <renderer/vulkan/Descriptors.hpp>

namespace fly {

    glm::mat4 orthoMatrixByWindowExtent(int width, int height) {
        return glm::ortho(0.0f, (float)width, 0.0f, (float)height, -MAX_Z_INDEX, MAX_Z_INDEX);
    }


    // RENDERER IMPLEMENTATION
    Renderer2d::Renderer2d(std::shared_ptr<VulkanInstance> vk): vk{vk} {
        this->orthoProj = orthoMatrixByWindowExtent(vk->swapChainExtent.width, vk->swapChainExtent.height);
    }

    void Renderer2d::init(std::unique_ptr<GPipeline2D> pipeline, VkCommandPool commandPool) {
        this->pipeline2d = std::move(pipeline);

        this->nullTexture = std::make_unique<Texture>(vk, commandPool);
        this->nullTextureSampler = std::make_unique<TextureSampler>(vk, 1, TextureSampler::Filter::NEAREST);
    }


    void Renderer2d::_renderTexture(
        const Texture& texture, 
        const TextureSampler& textureSampler, 
        glm::vec2 origin, 
        glm::vec2 size, 
        bool centre,
        glm::vec4 modColor,
        glm::vec2 relOrigin,
        glm::vec2 relSize,
        bool useTexture,
        int zIndex
    ) {
        FLY_ASSERT(0 <= zIndex && zIndex < MAX_Z_INDEX, "Z index must be positive and less than the max z");

        if(centre)
            origin -= size / 2.0f;

        auto transform = glm::mat4(1.0f);
        transform = glm::translate(transform, glm::vec3(origin, 0));
        transform = glm::scale(transform, glm::vec3(size, 1));
        
        Push2d pc;
        pc.proj = this->orthoProj * transform;
        pc.modColor = modColor;
        pc.origin = relOrigin;
        pc.relSize = relSize;
        pc.useTexture = useTexture;
        pc.zIndex = zIndex - MAX_Z_INDEX + 1;

        this->textureRenderQueue[texture.toRef()].push_back(
            {texture, textureSampler, pc}
        );
    }

    void Renderer2d::resize(int width, int height) {
        this->orthoProj = orthoMatrixByWindowExtent(width, height);
    }

    void Renderer2d::render(uint32_t currentFrame, VkCommandPool commandPool) {        
        //MATCH THE TEXTURE DATA TO RENDER WITH THE ONE THAT THE PIPELINE HOLDS
        for(auto& [k, v]: this->textureRenderQueue) {
            if(this->textureIndices[k].size() > v.size()) { //Clean up instances of a texture
                while(this->textureIndices[k].size() != v.size()) {   
                    this->pipeline2d->detachModel(this->textureIndices[k].back(), currentFrame);
                    this->textureIndices[k].pop_back();
                }
            } 
            else if(textureIndices[k].size() < v.size()) {
                while(this->textureIndices[k].size() != v.size()) {
                    auto vertices = this->vertices;
                    auto indices = this->indices;
                    auto meshIdx = this->pipeline2d->attachModel(std::make_unique<Vertex2DArray>(
                        this->vk, commandPool, std::move(vertices), std::move(indices)
                    ));
                
                    this->pipeline2d->updateDescriptorSet(meshIdx, v[0].texture, v[0].textureSampler);

                    this->textureIndices[k].emplace_back(meshIdx);
                }
            }
            
            for(size_t i=0; i<v.size(); ++i) {
                auto& renderData = this->textureRenderQueue[k][i];
                this->pipeline2d->setPushConstant(this->textureIndices[k][i], renderData.pc);
            }
        }

        //CLEAN UP ALL TEXTURES
        for(auto& [k, v]: this->oldTextureRenders) {
            if(!this->textureRenderQueue.contains(k)) {
                for(auto meshIdx: this->textureIndices[k]) {
                    this->pipeline2d->detachModel(meshIdx, currentFrame);
                }
                this->textureIndices.erase(k);
            }
        }

        this->oldTextureRenders = this->textureRenderQueue;
        this->textureRenderQueue.clear();
    }


    //2D PIPELINE IMPLEMENTATION
    void GPipeline2D::updateDescriptorSet(
        unsigned meshIndex,
        const Texture& texture,
        const TextureSampler& textureSampler
    ) {
        FLY_ASSERT(this->meshes[meshIndex].descriptorSets.size() == MAX_FRAMES_IN_FLIGHT, "Descriptor set vector bad size!");

        for(int i=0; i<MAX_FRAMES_IN_FLIGHT; ++i) {
            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = texture.getImageView();
            imageInfo.sampler = textureSampler.getSampler();

            std::array<VkWriteDescriptorSet, 1> descriptorWrites{};

            descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[0].dstSet = this->meshes[meshIndex].descriptorSets[i];
            descriptorWrites[0].dstBinding = 0;
            descriptorWrites[0].dstArrayElement = 0;
            descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].pImageInfo = &imageInfo;

            vkUpdateDescriptorSets(vk->device, descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
        }
    }


    DescriptorSetLayout GPipeline2D::createDescriptorSetLayout() {
        return newDescriptorSetBuild(MAX_FRAMES_IN_FLIGHT, {
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT}
        }).build(vk);
    }


    //VERTEX IMPLEMENTATION
    VkVertexInputBindingDescription Vertex2D::getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex2D);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescription;
    }
    
    std::vector<VkVertexInputAttributeDescription> Vertex2D::getAttributeDescriptions() {
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions(1);
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex2D, pos);

        return attributeDescriptions;
    }

    bool Vertex2D::operator==(const Vertex2D& other) const {
        return pos == other.pos;
    }

}