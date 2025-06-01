#pragma once

#include "Renderer.hpp"
#include "TGraphicsPipeline.hpp"
#include "Texture.hpp"

#include <filesystem>
#include <memory>
#include <unordered_map>

static const char* const FRAG_TEXT_SHADER_SRC = "vulkan-engine/shaders/textfrag.spv";
static const char* const VERT_TEXT_SHADER_SRC = "vulkan-engine/shaders/textvert.spv";

namespace fly {

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
        TextRenderer(Engine& engine);
        ~TextRenderer();

        void resize(int width, int height);

        void loadFont(const std::string& fontName, std::filesystem::path fontImg, std::filesystem::path fontJson);

        void renderText(
            const std::string& fontName, 
            const std::string& str, 
            glm::vec2 origin, 
            Align align, 
            float size, 
            glm::vec4 color
        );

    private:
        void render(uint32_t currentFrame);
        friend class Engine;

    private:
        Engine& engine;
        std::unordered_map<std::string, Font> fonts;
        std::unordered_map<std::string, std::vector<GPUCharacter>> fontRenderQueue;
        std::unordered_map<std::string, int> oldFontInstances; 

        TextPipeline* pipeline;
        glm::mat4 orthoProj;

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
        TextPipeline(const VulkanInstance& vk): TGraphicsPipeline{vk, false} {}
        ~TextPipeline() = default;

        void updateDescriptorSet(
            unsigned meshIndex,

            const std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT>& ssbos,
            const Texture& texture,
            const TextureSampler& textureSampler
        );

    private:

        std::vector<char> getVertShaderCode() override {
            return readFile(VERT_TEXT_SHADER_SRC);
        }
        std::vector<char> getFragShaderCode() override {
            return readFile(FRAG_TEXT_SHADER_SRC);
        }

        VkDescriptorSetLayout createDescriptorSetLayout() override;
        VkDescriptorPool createDescriptorPool() override;

    };

}