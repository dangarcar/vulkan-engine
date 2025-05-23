#pragma once

#include "../Utils.hpp"

#include "TGraphicsPipeline.hpp"
#include "TUniformBuffer.hpp"
#include "TVertexArray.hpp"
#include "Texture.hpp"
#include "glm/ext/matrix_float4x4.hpp"
#include "renderer/vulkan/VulkanTypes.h"

#include <memory>
#include <unordered_map>

#include <glm/glm.hpp>

static const char* const FRAG2D_SHADER_SRC = "vulkan-engine/shaders/frag2d.spv";
static const char* const VERT2D_SHADER_SRC = "vulkan-engine/shaders/vert2d.spv";

namespace fly {

    class Engine;

    struct UBO2D {
        alignas(16) glm::mat4 proj;
        glm::vec4 modColor;
        int useTexture;
    };

    struct Vertex2D {
        glm::vec2 pos;
    
        static VkVertexInputBindingDescription getBindingDescription();
    
        static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
    
        bool operator==(const Vertex2D& other) const;
    };

    using Vertex2DArray = TVertexArray<Vertex2D>;
    class GPipeline2D;

    class Renderer2d {
    public:
        Renderer2d(Engine& engine);
    
        void resize(int width, int height);

        void renderTexture(
            const Texture& texture, 
            const TextureSampler& textureSampler, 
            glm::vec2 origin, 
            glm::vec2 size, 
            bool centre = false,
            glm::vec4 modColor = {1,1,1,1}
        ) {
            _renderTexture(texture, textureSampler, origin, size, centre, modColor, true);
        }

        void renderQuad(
            glm::vec2 origin, 
            glm::vec2 size, 
            glm::vec4 color,
            bool centre = false
        ) {
            _renderTexture(*this->nullTexture, *this->nullTextureSampler, origin, size, centre, color, false);
        }

    private:
        void render(uint32_t currentFrame);
        friend class Engine;

        void _renderTexture(
            const Texture& texture, 
            const TextureSampler& textureSampler, 
            glm::vec2 origin, 
            glm::vec2 size, 
            bool centre,
            glm::vec4 modColor,
            bool useTexture
        );

    private:
        GPipeline2D* pipeline2d;
        
        //For everyone to use
        std::unique_ptr<Texture> nullTexture;
        std::unique_ptr<TextureSampler> nullTextureSampler;

        struct TextureRender {
            const Texture& texture; 
            const TextureSampler& textureSampler; 
            UBO2D ubo;
        };
        struct TextureData {
            unsigned meshIdx;
            std::unique_ptr<TUniformBuffer<UBO2D>> uniformBuffer;
        };
        std::unordered_map<TextureRef, std::vector<TextureData>> textureData;

        std::unordered_map<TextureRef, std::vector<TextureRender>> textureRenderQueue;

        const VulkanInstance& vk;
        const VkCommandPool commandPool;
        glm::mat4 orthoProj;

        const std::vector<Vertex2D> vertices = {
            {{0.0f, 0.0f}},
            {{1.0f, 0.0f}},
            {{1.0f, 1.0f}},
            {{0.0f, 1.0f}}
        };

        const std::vector<uint32_t> indices { 0, 2, 1, 2, 0, 3 };
    };



    class GPipeline2D: public TGraphicsPipeline<Vertex2D> {
    public:
        GPipeline2D(const VulkanInstance& vk): TGraphicsPipeline{vk} {}
        ~GPipeline2D() = default;
    
        void updateDescriptorSet(
            unsigned meshIndex,

            const TUniformBuffer<UBO2D>& uniformBuffer,
            const Texture& texture,
            const TextureSampler& textureSampler
        );

    private:
    
        std::vector<char> getVertShaderCode() override {
            return readFile(VERT2D_SHADER_SRC);
        }
        std::vector<char> getFragShaderCode() override {
            return readFile(FRAG2D_SHADER_SRC);
        }

        VkDescriptorSetLayout createDescriptorSetLayout() override;
        VkDescriptorPool createDescriptorPool() override;

    };

}