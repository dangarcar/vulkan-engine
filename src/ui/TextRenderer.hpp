#pragma once


#include "Renderer2d.hpp" //This is not best practice but it is very convinient
#include "renderer/vulkan/VulkanTypes.h"

#include <memory>


static const char* const FRAG_TEXT_SHADER_SRC = "vulkan-engine/shaders/bin/text.frag.spv";
static const char* const VERT_TEXT_SHADER_SRC = "vulkan-engine/shaders/bin/text.vert.spv";


namespace fly {

    class Engine;

    enum class Align {
        LEFT, RIGHT, CENTER
    };

    struct FontChar {
        float x, y;
        float width, height;
        float originX, originY;
        float advance;

        glm::vec2 normalizedBegin, normalizedEnd;
    };

    struct GPUCharacter {
        alignas(16) glm::mat4 proj;
        alignas(16) glm::vec4 colorAndZIdx;
        alignas(16) glm::vec4 texCoords;
    };

    struct RenderRequest{
        std::string str;
        glm::vec2 origin;
        Align align;
        float size;
        glm::vec4 colorAndZIdx;
    };

    class TextPipeline;

    class TextRenderer {
    public:
        static int const MAX_CHARS = 32768;
    public:
        TextRenderer(std::shared_ptr<VulkanInstance> vk);
        ~TextRenderer();

        void resize(int width, int height);

        void loadFont(
            const std::string& fontName, 
            std::filesystem::path fontImg, 
            std::filesystem::path fontJson,
            VkCommandPool commandPool
        );

        void init(std::unique_ptr<TextPipeline> pipeline);

        void renderText(
            const std::string& str, 
            glm::vec2 origin, 
            Align align, 
            float size, 
            glm::vec3 color,
            int zIndex = 0
        );

        TextPipeline* getPipeline() { return pipeline.get(); }
        void render(uint32_t currentFrame);

    private:
        friend class Engine;

    private:        
        void convertText(RenderRequest r);

        std::queue<RenderRequest> requestQueue;
        std::vector<GPUCharacter> renderQueue;

        
        //FONT
        std::string fontName;
        std::unique_ptr<Texture> fontTexture, newTexture;
        std::unique_ptr<TextureSampler> fontSampler, newSampler;
        std::unordered_map<char, FontChar> fontChars, newFontChars;
        
        unsigned meshIndex = UINT32_MAX, newMeshIndex = UINT32_MAX;
        std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT> fontBuffers;
        std::array<VmaAllocation, MAX_FRAMES_IN_FLIGHT> fontBuffersAlloc;
        std::array<VmaAllocationInfo, MAX_FRAMES_IN_FLIGHT> fontBuffersInfo;

        
        std::unique_ptr<TextPipeline> pipeline;
        glm::mat4 orthoProj;

        std::shared_ptr<VulkanInstance> vk;

        const std::vector<Vertex2D> vertices = {
            {{0.0f, 0.0f}},
            {{1.0f, 0.0f}},
            {{1.0f, 1.0f}},
            {{0.0f, 1.0f}}
        };

        const std::vector<uint32_t> indices { 0, 1, 2, 0, 2, 3 };
    
    };



    class TextPipeline: public TGraphicsPipeline<Vertex2D> {
    public:
        TextPipeline(std::shared_ptr<VulkanInstance> vk): TGraphicsPipeline{vk, DEPTH_TEST_ENABLED} {}
        ~TextPipeline() = default;

        void updateDescriptorSet(
            unsigned meshIndex,

            const std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT>& ssbos,
            const Texture& texture,
            const TextureSampler& textureSampler
        );

    private:
        std::vector<char> getVertShaderCode() override { return readFile(VERT_TEXT_SHADER_SRC); }
        std::vector<char> getFragShaderCode() override { return readFile(FRAG_TEXT_SHADER_SRC); }
        DescriptorSetLayout createDescriptorSetLayout() override;

    };

}