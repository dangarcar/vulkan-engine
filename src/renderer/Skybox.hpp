#pragma once

#include <Utils.hpp>
#include <glm/glm.hpp>
#include <memory>

#include "TGraphicsPipeline.hpp"
#include "TVertexArray.hpp"
#include "TUniformBuffer.hpp"
#include "Texture.hpp"


static const char* const SKYBOX_FRAG_SHADER_SRC = "vulkan-engine/shaders/skyboxfrag.spv";
static const char* const SKYBOX_VERT_SHADER_SRC = "vulkan-engine/shaders/skyboxvert.spv";

namespace fly {

    struct UBOSkybox {
	    glm::mat4 projection;
	    glm::mat4 view;
    };

    struct SimpleVertex {
        glm::vec3 pos;
    
        static VkVertexInputBindingDescription getBindingDescription();
    
        static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
    
        bool operator==(const SimpleVertex& other) const;
    };

    using SimpleVertexArray = TVertexArray<SimpleVertex>;
    class SkyboxPipeline;
    class Engine;

    class Skybox {
    public:
        Skybox(Engine& engine, std::unique_ptr<Texture> cubemap, std::unique_ptr<TextureSampler> cubemapSampler);
        ~Skybox() = default;
    
        void render(uint32_t currentFrame, glm::mat4 projection, glm::mat4 view);


    private:
        SkyboxPipeline* pipeline;

        std::unique_ptr<TUniformBuffer<UBOSkybox>> uniformBuffer;
        std::unique_ptr<TextureSampler> cubemapSampler;
        std::unique_ptr<Texture> cubemap;

        const std::vector<SimpleVertex> vertices = {
            {{-1.0f,  1.0f, -1.0f}},
            {{-1.0f, -1.0f, -1.0f}},
            {{ 1.0f, -1.0f, -1.0f}},
            {{ 1.0f, -1.0f, -1.0f}},
            {{ 1.0f,  1.0f, -1.0f}},
            {{-1.0f,  1.0f, -1.0f}},

            {{-1.0f, -1.0f,  1.0f}},
            {{-1.0f, -1.0f, -1.0f}},
            {{-1.0f,  1.0f, -1.0f}},
            {{-1.0f,  1.0f, -1.0f}},
            {{-1.0f,  1.0f,  1.0f}},
            {{-1.0f, -1.0f,  1.0f}},

            {{ 1.0f, -1.0f, -1.0f}},
            {{ 1.0f, -1.0f,  1.0f}},
            {{ 1.0f,  1.0f,  1.0f}},
            {{ 1.0f,  1.0f,  1.0f}},
            {{ 1.0f,  1.0f, -1.0f}},
            {{ 1.0f, -1.0f, -1.0f}},

            {{-1.0f, -1.0f,  1.0f}},
            {{-1.0f,  1.0f,  1.0f}},
            {{ 1.0f,  1.0f,  1.0f}},
            {{ 1.0f,  1.0f,  1.0f}},
            {{ 1.0f, -1.0f,  1.0f}},
            {{-1.0f, -1.0f,  1.0f}},

            {{-1.0f,  1.0f, -1.0f}},
            {{ 1.0f,  1.0f, -1.0f}},
            {{ 1.0f,  1.0f,  1.0f}},
            {{ 1.0f,  1.0f,  1.0f}},
            {{-1.0f,  1.0f,  1.0f}},
            {{-1.0f,  1.0f, -1.0f}},

            {{-1.0f, -1.0f, -1.0f}},
            {{-1.0f, -1.0f,  1.0f}},
            {{ 1.0f, -1.0f, -1.0f}},
            {{ 1.0f, -1.0f, -1.0f}},
            {{-1.0f, -1.0f,  1.0f}},
            {{ 1.0f, -1.0f,  1.0f}}
        };

        const std::vector<uint32_t> indices = {
            0, 1, 2,    // Cara trasera
            3, 4, 5,

            6, 7, 8,    // Cara izquierda
            9, 10,11,

            12,13,14,   // Cara derecha
            15,16,17,

            18,19,20,   // Cara frontal
            21,22,23,

            24,25,26,   // Cara superior
            27,28,29,

            30,31,32,   // Cara inferior
            33,34,35
        };
    };

    class Texture;
    class TextureSampler;

    class SkyboxPipeline : public TGraphicsPipeline<SimpleVertex> {
    public:
        SkyboxPipeline(const VulkanInstance& vk): TGraphicsPipeline{vk, false} {}
        ~SkyboxPipeline() = default;

        void updateDescriptorSet(
            const TUniformBuffer<UBOSkybox>& uniformBuffer,
            const Texture& texture,
            const TextureSampler& textureSampler
        );

    private:

        std::vector<char> getVertShaderCode() override {
            return readFile(SKYBOX_VERT_SHADER_SRC);
        }
        std::vector<char> getFragShaderCode() override {
            return readFile(SKYBOX_FRAG_SHADER_SRC);
        }

        VkDescriptorSetLayout createDescriptorSetLayout() override;
        VkDescriptorPool createDescriptorPool() override;

    };

}