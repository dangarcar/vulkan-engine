#pragma once

#include "../Utils.hpp"

#include "TVertexArray.hpp"
#include "TUniformBuffer.hpp"
#include "renderer/TGraphicsPipeline.hpp"

#include <cstdint>
#include <memory>

#include <glm/glm.hpp>

static const char* const SKYBOX_FRAG_SHADER_SRC = "vulkan-engine/shaders/skyboxfrag.spv";
static const char* const SKYBOX_VERT_SHADER_SRC = "vulkan-engine/shaders/skyboxvert.spv";

namespace fly {

    /*struct UBOSkybox {
	    glm::mat4 projection;
	    glm::mat4 model;
    };

    struct SimpleVertex {
        glm::vec3 pos;
    
        static VkVertexInputBindingDescription getBindingDescription();
    
        static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
    
        bool operator==(const SimpleVertex& other) const;
    };

    using SimpleVertexArray = TVertexArray<SimpleVertex>;
    class SkyboxPipeline;

    class Skybox {

    private:
        SkyboxPipeline* pipeline;

        std::unique_ptr<TUniformBuffer<UBOSkybox>> uniformBuffer;

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
            0, 1, 2, 1, 2, 3
        };
    };

    class Texture;
    class TextureSampler;

    class SkyboxPipeline : public TGraphicsPipeline<SimpleVertex> {
    public:
        SkyboxPipeline(const VulkanInstance& vk): TGraphicsPipeline{vk} {}
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

    };*/

}