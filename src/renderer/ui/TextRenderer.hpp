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

    struct Font {
        std::string name;
        std::unique_ptr<Texture> texture;
        std::unique_ptr<TextureSampler> sampler;
        std::unordered_map<char, FontChar> fontChars;
        
        unsigned meshIndex;
        std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT> buffers;
        std::array<VkDeviceMemory, MAX_FRAMES_IN_FLIGHT> buffersMemory;
        std::array<void*, MAX_FRAMES_IN_FLIGHT> buffersMapped;
    };

    struct GPUCharacter {
        alignas(16) glm::mat4 proj;
        alignas(16) glm::vec4 color;
        alignas(16) glm::vec4 texCoords;
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
            const std::string& fontName, 
            const std::string& str, 
            glm::vec2 origin, 
            Align align, 
            float size, 
            glm::vec4 color
        );

        TextPipeline* getPipeline() { return pipeline.get(); }
        void render(uint32_t currentFrame);

    private:
        friend class Engine;

    private:
        std::unordered_map<std::string, Font> fonts;
        std::unordered_map<std::string, std::vector<GPUCharacter>> fontRenderQueue;
        std::unordered_map<std::string, int> oldFontInstances; 

        std::unique_ptr<TextPipeline> pipeline;
        glm::mat4 orthoProj;

        std::shared_ptr<VulkanInstance> vk;

        const std::vector<Vertex2D> vertices = {
            {{0.0f, 0.0f}},
            {{1.0f, 0.0f}},
            {{1.0f, 1.0f}},
            {{0.0f, 1.0f}}
        };

        const std::vector<uint32_t> indices { 0, 2, 1, 2, 0, 3 };
    };



    class TextPipeline: public TGraphicsPipeline<Vertex2D> {
    public:
        TextPipeline(std::shared_ptr<VulkanInstance> vk): TGraphicsPipeline{vk, false} {}
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