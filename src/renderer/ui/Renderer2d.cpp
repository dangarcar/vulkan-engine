#include "Renderer2d.hpp"

#include <glm/gtc/matrix_transform.hpp>

namespace fly {


    // RENDERER IMPLEMENTATION
    Renderer2d::Renderer2d(const VulkanInstance& vk): vk{vk} {
        this->orthoProj = glm::ortho(0.0f, (float)vk.swapChainExtent.width, 0.0f, (float)vk.swapChainExtent.height);
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
        bool useTexture
    ) {
        if(centre)
            origin -= size / 2.0f;

        auto transform = glm::mat4(1.0f);
        transform = glm::translate(transform, glm::vec3(origin, 0));
        transform = glm::scale(transform, glm::vec3(size, 1));
        
        UBO2D ubo;
        ubo.proj = this->orthoProj * transform;
        ubo.modColor = modColor;
        ubo.useTexture = useTexture;

        this->textureRenderQueue[texture.toRef()].push_back(
            {texture, textureSampler, ubo}
        );
    }

    void Renderer2d::resize(int width, int height) {
        this->orthoProj = glm::ortho(0.0f, (float)width, 0.0f, (float)height);
    }

    void Renderer2d::render(uint32_t currentFrame, VkCommandPool commandPool) {
        //DESTROY OLD UBOS WHEN THEY'RE NOT USED ANYMORE
        std::vector<std::pair<uint32_t, std::unique_ptr<TUniformBuffer<UBO2D>>>> newUbosToDestroy;
        for(auto& p: this->ubosToDestroy) {
            if(currentFrame != p.first) {
                newUbosToDestroy.push_back(std::move(p));
                continue;
            }

            p.second.reset(); //Remove ubo
        }
        this->ubosToDestroy = std::move(newUbosToDestroy);        
        
        //MATCH THE TEXTURE DATA TO RENDER WITH THE ONE THAT THE PIPELINE HOLDS
        for(auto& [k, v]: this->textureRenderQueue) {
            if(this->textureData[k].size() > v.size()) { //Clean up instances of a texture
                while(this->textureData[k].size() != v.size()) {   
                    destroyTextureData(std::move(this->textureData[k].back()), currentFrame);
                    this->textureData[k].pop_back();
                }
            } 
            else if(textureData[k].size() < v.size()) {
                while(this->textureData[k].size() != v.size()) {
                    TextureData data;
                    data.uniformBuffer = std::make_unique<TUniformBuffer<UBO2D>>(this->vk);

                    auto vertices = this->vertices;
                    auto indices = this->indices;
                    data.meshIdx = this->pipeline2d->attachModel(std::make_unique<Vertex2DArray>(
                        this->vk, commandPool, std::move(vertices), std::move(indices)
                    ));
                
                    this->pipeline2d->updateDescriptorSet(
                        data.meshIdx, 
                        *data.uniformBuffer, 
                        v[0].texture, 
                        v[0].textureSampler
                    );
                
                    this->textureData[k].emplace_back(std::move(data));
                }
            }

            for(size_t i=0; i<v.size(); ++i) {
                this->textureData[k][i].uniformBuffer->updateUBO(this->textureRenderQueue[k][i].ubo, currentFrame);
            }
        }

        //CLEAN UP ALL TEXTURES
        for(auto& [k, v]: this->oldTextureRenders) {
            if(!this->textureRenderQueue.contains(k)) {
                for(auto& data: this->textureData[k]) {
                    destroyTextureData(std::move(data), currentFrame);
                }
                this->textureData.erase(k);
            }
        }

        this->oldTextureRenders = this->textureRenderQueue;
        this->textureRenderQueue.clear();
    }

    void Renderer2d::destroyTextureData(TextureData&& data, uint32_t currentFrame) {
        this->pipeline2d->detachModel(data.meshIdx, currentFrame);
        this->ubosToDestroy.emplace_back(std::make_pair(currentFrame, std::move(data.uniformBuffer)));
    }


    //2D PIPELINE IMPLEMENTATION
    void GPipeline2D::updateDescriptorSet(
        unsigned meshIndex,

        const TUniformBuffer<UBO2D>& uniformBuffer,
        const Texture& texture,
        const TextureSampler& textureSampler
    ) {
        assert(this->meshes[meshIndex].descriptorSets.size() == MAX_FRAMES_IN_FLIGHT && "Descriptor set vector bad size!");

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
            descriptorWrites[0].dstSet = this->meshes[meshIndex].descriptorSets[i];
            descriptorWrites[0].dstBinding = 0;
            descriptorWrites[0].dstArrayElement = 0;
            descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].pBufferInfo = &bufferInfo;

            descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[1].dstSet = this->meshes[meshIndex].descriptorSets[i];
            descriptorWrites[1].dstBinding = 1;
            descriptorWrites[1].dstArrayElement = 0;
            descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[1].descriptorCount = 1;
            descriptorWrites[1].pImageInfo = &imageInfo;

            vkUpdateDescriptorSets(vk.device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
        }
    }

    VkDescriptorSetLayout GPipeline2D::createDescriptorSetLayout() {
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

    VkDescriptorPool GPipeline2D::createDescriptorPool() {
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