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

    struct Push2d {
        alignas(16) glm::mat4 proj;
        glm::vec4 modColor;
        glm::vec2 origin, relSize;
        int useTexture;
        float zIndex;
    };

    struct Vertex2D {
        glm::vec2 pos;
    
        static VkVertexInputBindingDescription getBindingDescription();
    
        static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
    
        bool operator==(const Vertex2D& other) const;
    };

    glm::mat4 orthoMatrixByWindowExtent(int width, int height);
    constexpr float MAX_Z_INDEX = 100;

    using Vertex2DArray = TVertexArray<Vertex2D>;
    class GPipeline2D;

    //Everything is private because is managed by the ui renderer
    class Renderer2d {
    private:
        Renderer2d(std::shared_ptr<VulkanInstance> vk);
    
        void resize(int width, int height);

        void init(std::unique_ptr<GPipeline2D> pipeline, VkCommandPool commandPool);
        
        void render(uint32_t currentFrame, VkCommandPool commandPool);
        GPipeline2D* getPipeline() { return pipeline2d.get(); }


    private:
        friend class UIManager;

        void _renderTexture(
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
        );

        struct TextureRender {
            const Texture& texture; 
            const TextureSampler& textureSampler; 
            Push2d pc;
        };


    private:
        std::unique_ptr<GPipeline2D> pipeline2d;
        
        //For everyone to use
        std::unique_ptr<Texture> nullTexture;
        std::unique_ptr<TextureSampler> nullTextureSampler;

        std::unordered_map<TextureRef, std::vector<unsigned>> textureIndices;

        std::unordered_map<TextureRef, std::vector<TextureRender>> textureRenderQueue, oldTextureRenders;

        std::shared_ptr<VulkanInstance> vk;
        glm::mat4 orthoProj;

        const std::vector<Vertex2D> vertices = {
            {{0.0f, 0.0f}},
            {{1.0f, 0.0f}},
            {{1.0f, 1.0f}},
            {{0.0f, 1.0f}}
        };

        const std::vector<uint32_t> indices { 0, 1, 2, 0, 2, 3 };
    };



    class GPipeline2D: public TGraphicsPipeline<Vertex2D, Push2d> {
    public:
        GPipeline2D(std::shared_ptr<VulkanInstance> vk): TGraphicsPipeline{vk, DEPTH_TEST_ENABLED} {}
        ~GPipeline2D() = default;
    
        void updateDescriptorSet(
            unsigned meshIndex,
            const Texture& texture,
            const TextureSampler& textureSampler
        );

    private:
        std::vector<char> getVertShaderCode() override { return readFile(REND2D_VERT_SHADER_SRC); }
        std::vector<char> getFragShaderCode() override { return readFile(REND2D_FRAG_SHADER_SRC); }
        DescriptorSetLayout createDescriptorSetLayout() override;

    };

}