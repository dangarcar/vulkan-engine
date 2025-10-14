#include "TextRenderer.hpp"

#include <fstream>
#include <memory>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <utility>
#include <vector>

#include "renderer/vulkan/VulkanTypes.h"
#include "renderer/vulkan/Descriptors.hpp"

#include <glm/gtc/matrix_transform.hpp>

namespace fly {

    TextRenderer::TextRenderer(std::shared_ptr<VulkanInstance> vk): vk{vk} {
        this->orthoProj = orthoMatrixByWindowExtent(vk->swapChainExtent.width, vk->swapChainExtent.height);

        VkDeviceSize bufferSize = sizeof(GPUCharacter) * MAX_CHARS;
        for(int i=0; i<MAX_FRAMES_IN_FLIGHT; ++i) {
            VkBufferCreateInfo bufferCreateInfo{};
            bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferCreateInfo.size = bufferSize;
            bufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

            VmaAllocationCreateInfo bufferCreateAllocInfo = {};
            bufferCreateAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            bufferCreateAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
            
            vmaCreateBuffer(
                vk->allocator, 
                &bufferCreateInfo, 
                &bufferCreateAllocInfo, 
                &this->fontBuffers[i], 
                &this->fontBuffersAlloc[i], 
                &this->fontBuffersInfo[i]
            );
        }
    }

    TextRenderer::~TextRenderer() {
        this->newSampler.reset();
        this->newTexture.reset();
        this->fontSampler.reset();
        this->fontTexture.reset();

        for(int i=0; i<MAX_FRAMES_IN_FLIGHT; ++i)
            vmaDestroyBuffer(vk->allocator, this->fontBuffers[i], this->fontBuffersAlloc[i]);
    }

    void TextRenderer::init(std::unique_ptr<TextPipeline> pipeline) {
        this->pipeline = std::move(pipeline);
    }
    

    void TextRenderer::render(uint32_t currentFrame) {
        if(this->newMeshIndex != UINT32_MAX) {
            this->fontChars = std::move(this->newFontChars);
            if(this->meshIndex != UINT32_MAX)
                this->pipeline->detachModel(this->meshIndex, currentFrame);

            this->fontSampler.swap(this->newSampler);
            this->fontTexture.swap(this->newTexture);

            this->meshIndex = this->newMeshIndex;
            this->newMeshIndex = UINT32_MAX;
        }


        //Traverse request queue
        while(!this->requestQueue.empty()) {
            convertText(std::move(this->requestQueue.front()));
            this->requestQueue.pop();
        }


        //Traverse render queue
        if(this->renderQueue.size() > MAX_CHARS)
            throw std::runtime_error("There cannot be that number of characters of the same font!");

        memcpy(this->fontBuffersInfo[currentFrame].pMappedData, &this->renderQueue[0], this->renderQueue.size()*sizeof(GPUCharacter));
        this->pipeline->setInstanceCount(this->meshIndex, this->renderQueue.size());

        this->renderQueue.clear();
    }

    void TextRenderer::renderText(
        const std::string& str, 
        glm::vec2 origin, 
        Align align, 
        float size, 
        glm::vec3 color,
        int zIndex
    ) {
        FLY_ASSERT(0 <= zIndex && zIndex < MAX_Z_INDEX, "Z index must be positive and less than the max z");

        glm::vec4 colorAndZIdx = glm::vec4(color, zIndex - MAX_Z_INDEX + 1);
        this->requestQueue.push(RenderRequest{str, origin, align, size, colorAndZIdx});        
    }

    void TextRenderer::convertText(RenderRequest r) {              
        std::vector<float> advances = {0};
        for(auto c: r.str) {
            FLY_ASSERT(this->fontChars.contains(c), "{} doesn't contain character '{}' in its files", this->fontName, c);

            if(c == '\n')
                advances.push_back(0);
            else
                advances.back() += this->fontChars[c].advance * r.size;
        }

        float advance = 0;
        int line = 0;
        for(auto c: r.str) {
            if(c == '\n') {
                line++;
                advance = 0;
                continue;
            }

            auto& fontChar = this->fontChars[c];
        
            glm::vec2 localOrigin;
            switch(r.align) {
                case Align::LEFT:
                    localOrigin = r.origin;
                break;
                case Align::CENTER:
                    localOrigin = r.origin - glm::vec2(advances[line]/2, 0);
                break;
                case Align::RIGHT:
                    localOrigin = r.origin - glm::vec2(advances[line], 0);
                break;
            }

            glm::vec2 sizeVector {fontChar.width * r.size, fontChar.height * r.size};
            glm::vec2 dorigin {advance - fontChar.originX, 1 - fontChar.originY + line};
            auto transform = glm::mat4(1.0f);
            transform = glm::translate(transform, glm::vec3(localOrigin + dorigin*r.size, 0));
            transform = glm::scale(transform, glm::vec3(sizeVector, 1));
            advance += fontChar.advance;

            GPUCharacter gpuChar;
            gpuChar.proj = this->orthoProj * transform;
            gpuChar.colorAndZIdx = r.colorAndZIdx;
            gpuChar.texCoords = glm::vec4(fontChar.normalizedBegin, fontChar.normalizedEnd);

            this->renderQueue.push_back(gpuChar);
        }
    }

    void TextRenderer::resize(int width, int height) {
        this->orthoProj = orthoMatrixByWindowExtent(width, height);
    }

    void TextRenderer::loadFont(
        const std::string& fontName, 
        std::filesystem::path fontImg, 
        std::filesystem::path fontJson,
        VkCommandPool commandPool
    ) {        
        this->newFontChars.clear();
        this->fontName = fontName;

        //JSON LOADING
        std::ifstream file(fontJson);
        auto data = nlohmann::json::parse(file);

        auto size = data["size"].get<float>();
        auto width = data["width"].get<int>();
        auto height = data["height"].get<int>();
        for(auto [k, e]: data["characters"].items()) {
            FontChar fontChar;
            auto x = e["x"].get<float>();
            auto y = e["y"].get<float>();
            auto fwidth = e["width"].get<float>();
            auto fheight = e["height"].get<float>();

            fontChar.x = x / size;
            fontChar.y = y / size;
            fontChar.width = fwidth / size;
            fontChar.height = fheight / size;
            fontChar.originX = e["originX"].get<float>() / size;
            fontChar.originY = e["originY"].get<float>() / size;
            fontChar.advance = e["advance"].get<float>() / size;

            fontChar.normalizedBegin = { x / width, y / height};
            fontChar.normalizedEnd = { (x + fwidth - 1) / width, (y + fheight - 1) / height};

            this->newFontChars.insert(std::make_pair(k[0], fontChar));
        }

        
        //TEXTURE LOADING
        this->newTexture = std::make_unique<fly::Texture>(
            vk, commandPool, fontImg, 
            fly::STB_Format::STBI_rgb_alpha, VK_FORMAT_R8G8B8A8_SRGB
        );
        this->newSampler = std::make_unique<fly::TextureSampler>(vk, 0); //No mipmapping in the texture atlases
        
        //MODEL LOADING
        auto vertices = this->vertices;
        auto indices = this->indices;
        this->newMeshIndex = this->pipeline->attachModel(std::make_unique<Vertex2DArray>( //TODO:
            vk, commandPool, std::move(vertices), std::move(indices)
        ));

        this->pipeline->updateDescriptorSet(
            this->newMeshIndex, 
            this->fontBuffers, 
            *this->newTexture, 
            *this->newSampler
        );
    }



    //TEXT PIPELINE IMPLEMENTATION
    void TextPipeline::updateDescriptorSet(
        unsigned meshIndex,

        const std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT>& ssbos,
        const Texture& texture,
        const TextureSampler& textureSampler
    ) {
        FLY_ASSERT(this->meshes[meshIndex].descriptorSets.size() == MAX_FRAMES_IN_FLIGHT, "Descriptor set vector bad size!");

        for(int i=0; i<MAX_FRAMES_IN_FLIGHT; ++i) {
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = ssbos[i];
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(GPUCharacter) * TextRenderer::MAX_CHARS;

            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = texture.getImageView();
            imageInfo.sampler = textureSampler.getSampler();

            std::vector<VkWriteDescriptorSet> descriptorWrites(2);

            descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[0].dstSet = this->meshes[meshIndex].descriptorSets[i];
            descriptorWrites[0].dstBinding = 0;
            descriptorWrites[0].dstArrayElement = 0;
            descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].pBufferInfo = &bufferInfo;

            descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[1].dstSet = this->meshes[meshIndex].descriptorSets[i];
            descriptorWrites[1].dstBinding = 1;
            descriptorWrites[1].dstArrayElement = 0;
            descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[1].descriptorCount = 1;
            descriptorWrites[1].pImageInfo = &imageInfo;

            vkUpdateDescriptorSets(vk->device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
        }
    }

    DescriptorSetLayout TextPipeline::createDescriptorSetLayout() {
        return newDescriptorSetBuild(MAX_FRAMES_IN_FLIGHT, {
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT}
        }).build(vk);
    }

}