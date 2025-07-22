#pragma once

#include <Utils.hpp>

#include <renderer/TGraphicsPipeline.hpp>
#include <renderer/TBuffer.hpp>
#include <renderer/TVertexArray.hpp>
#include <renderer/Texture.hpp>

#include <memory>
#include <unordered_map>
#include <glm/glm.hpp>

static const char* const REND2D_FRAG_SHADER_SRC = "vulkan-engine/shaders/bin/rend2d.frag.spv";
static const char* const REND2D_VERT_SHADER_SRC = "vulkan-engine/shaders/bin/rend2d.vert.spv";

namespace fly {

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
        Renderer2d(const VulkanInstance& vk);
    
        void resize(int width, int height);

        void init(std::unique_ptr<GPipeline2D> pipeline, VkCommandPool commandPool);

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
        
        void render(uint32_t currentFrame, VkCommandPool commandPool);
        GPipeline2D* getPipeline() { return pipeline2d.get(); }

    private:

        void _renderTexture(
            const Texture& texture, 
            const TextureSampler& textureSampler, 
            glm::vec2 origin, 
            glm::vec2 size, 
            bool centre,
            glm::vec4 modColor,
            bool useTexture
        );

        struct TextureRender {
            const Texture& texture; 
            const TextureSampler& textureSampler; 
            UBO2D ubo;
        };
        struct TextureData {
            unsigned meshIdx;
            std::unique_ptr<TBuffer<UBO2D>> uniformBuffer;
        };
        void destroyTextureData(TextureData&& data, uint32_t currentFrame);


    private:
        std::unique_ptr<GPipeline2D> pipeline2d;
        
        //For everyone to use
        std::unique_ptr<Texture> nullTexture;
        std::unique_ptr<TextureSampler> nullTextureSampler;

        std::unordered_map<TextureRef, std::vector<TextureData>> textureData;

        std::unordered_map<TextureRef, std::vector<TextureRender>> textureRenderQueue, oldTextureRenders;
        std::vector<std::pair<uint32_t, std::unique_ptr<TBuffer<UBO2D>>>> ubosToDestroy;

        const VulkanInstance& vk;
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
        GPipeline2D(const VulkanInstance& vk): TGraphicsPipeline{vk, false} {}
        ~GPipeline2D() = default;
    
        void updateDescriptorSet(
            unsigned meshIndex,

            const TBuffer<UBO2D>& uniformBuffer,
            const Texture& texture,
            const TextureSampler& textureSampler
        );

    private:
        std::vector<char> getVertShaderCode() override { return readFile(REND2D_VERT_SHADER_SRC); }
        std::vector<char> getFragShaderCode() override { return readFile(REND2D_FRAG_SHADER_SRC); }
        DescriptorSetLayout createDescriptorSetLayout() override;

    };

}