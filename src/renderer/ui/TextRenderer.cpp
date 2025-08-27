#include "TextRenderer.hpp"

#include <fstream>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <utility>
#include <vector>

#include "renderer/vulkan/VulkanTypes.h"
#include "renderer/vulkan/Descriptors.hpp"

#include <glm/gtc/matrix_transform.hpp>

namespace fly {

    TextRenderer::TextRenderer(const VulkanInstance& vk): vk{vk} {
        this->orthoProj = glm::ortho(0.0f, (float)vk.swapChainExtent.width, 0.0f, (float)vk.swapChainExtent.height);
    }

    TextRenderer::~TextRenderer() {
        for(auto& [k, f]: fonts) {
            f.sampler.reset();
            f.texture.reset();

            for(int i=0; i<MAX_FRAMES_IN_FLIGHT; ++i) {
                vkDestroyBuffer(vk.device, f.buffers[i], nullptr);
                vkFreeMemory(vk.device, f.buffersMemory[i], nullptr);
            }
        }
    }

    void TextRenderer::init(std::unique_ptr<TextPipeline> pipeline) {
        this->pipeline = std::move(pipeline);
    }
    

    void TextRenderer::render(uint32_t currentFrame) {
        for(auto& [k, e]: this->oldFontInstances) {
            if(!this->fontRenderQueue.contains(k))
                this->pipeline->setInstanceCount(this->fonts[k].meshIndex, 0);
        }
        this->oldFontInstances.clear();

        for(auto& [k, v]: this->fontRenderQueue) {
            if(v.size() > MAX_CHARS)
                throw std::runtime_error("There cannot be that number of characters of the same font!");
            
            this->oldFontInstances.insert({k, v.size()});

            memcpy(this->fonts[k].buffersMapped[currentFrame], &v[0], v.size()*sizeof(GPUCharacter));
            this->pipeline->setInstanceCount(this->fonts[k].meshIndex, v.size());
        }

        this->fontRenderQueue.clear();
    }

    void TextRenderer::renderText(
        const std::string& fontName, 
        const std::string& str, 
        glm::vec2 origin, 
        Align align, 
        float size, 
        glm::vec4 color
    ) {
        if(!this->fonts.contains(fontName)) {
            std::cerr << fontName << " not found in game files" << std::endl;
            return;
        }

        auto& font = this->fonts.at(fontName);                
        std::vector<float> advances = {0};
        for(auto c: str) {
            FLY_ASSERT(font.fontChars.contains(c), "{} doesn't contain character '{}' in its files", fontName, c);

            if(c == '\n')
                advances.push_back(0);
            else
                advances.back() += font.fontChars[c].advance * size;
        }

        float advance = 0;
        int line = 0;
        this->fontRenderQueue[fontName].reserve(this->fontRenderQueue[fontName].size() + str.size());
        for(auto c: str) {
            if(c == '\n') {
                line++;
                advance = 0;
                continue;
            }

            auto& fontChar = font.fontChars[c];
        
            glm::vec2 localOrigin;
            switch(align) {
                case Align::LEFT:
                    localOrigin = origin;
                break;
                case Align::CENTER:
                    localOrigin = origin - glm::vec2(advances[line]/2, 0);
                break;
                case Align::RIGHT:
                    localOrigin = origin - glm::vec2(advances[line], 0);
                break;
            }

            glm::vec2 sizeVector {fontChar.width * size, fontChar.height * size};
            glm::vec2 dorigin {advance - fontChar.originX, 1 - fontChar.originY + line};
            auto transform = glm::mat4(1.0f);
            transform = glm::translate(transform, glm::vec3(localOrigin + dorigin*size, 0));
            transform = glm::scale(transform, glm::vec3(sizeVector, 1));
            advance += fontChar.advance;

            GPUCharacter gpuChar;
            gpuChar.proj = this->orthoProj * transform;
            gpuChar.color = color;
            gpuChar.texCoords = glm::vec4(fontChar.normalizedBegin, fontChar.normalizedEnd);

            this->fontRenderQueue[fontName].push_back(gpuChar);   
        }
    }

    void TextRenderer::resize(int width, int height) {
        this->orthoProj = glm::ortho(0.0f, (float)width, 0.0f, (float)height);
    }

    void TextRenderer::loadFont(
        const std::string& fontName, 
        std::filesystem::path fontImg, 
        std::filesystem::path fontJson,
        VkCommandPool commandPool
    ) {
        //JSON LOADING
        std::unordered_map<char, FontChar> fontChars;
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

            fontChars.insert(std::make_pair(k[0], fontChar));
        }

        
        //TEXTURE LOADING
        Font font;
        font.texture = std::make_unique<fly::Texture>(
            vk, commandPool, fontImg, 
            fly::STB_Format::STBI_rgb_alpha, VK_FORMAT_R8G8B8A8_SRGB
        );
        font.sampler = std::make_unique<fly::TextureSampler>(vk, 0); //No mipmapping in the texture atlases
        
        //MODEL LOADING
        auto vertices = this->vertices;
        auto indices = this->indices;
        font.meshIndex = this->pipeline->attachModel(std::make_unique<Vertex2DArray>(
            vk, commandPool, std::move(vertices), std::move(indices)
        ));

        VkDeviceSize bufferSize = sizeof(GPUCharacter) * MAX_CHARS;
        for(int i=0; i<MAX_FRAMES_IN_FLIGHT; ++i) {
            createBuffer(
                vk,
                bufferSize, 
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                font.buffers[i], 
                font.buffersMemory[i]
            );

            vkMapMemory(vk.device, font.buffersMemory[i], 0, bufferSize, 0, &font.buffersMapped[i]);
        }

        this->pipeline->updateDescriptorSet(
            font.meshIndex, 
            font.buffers, 
            *font.texture, 
            *font.sampler
        );

        
        //KEEPING IT IN A MAP
        font.name = fontName;
        font.fontChars = std::move(fontChars);
        this->fonts.insert(std::make_pair(fontName, std::move(font)));
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

            vkUpdateDescriptorSets(vk.device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
        }
    }

    DescriptorSetLayout TextPipeline::createDescriptorSetLayout() {
        return newDescriptorSetBuild(MAX_FRAMES_IN_FLIGHT, {
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT}
        }).build(vk);
    }

}