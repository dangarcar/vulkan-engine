#pragma once

#include "TGraphicsPipeline.hpp"
#include "TUniformBuffer.hpp"
#include "TVertexArray.hpp"

#include "Texture.hpp"

#include "../Utils.hpp"

#include <glm/glm.hpp>

static const char* const DEFAULT_FRAG_SHADER_SRC = "vulkan-engine/shaders/frag.spv";
static const char* const DEFAULT_VERT_SHADER_SRC = "vulkan-engine/shaders/vert.spv";

namespace fly {
    
    struct DefaultUBO {
        alignas(16) glm::mat4 model;
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 proj;
    };

    struct Vertex {
        glm::vec3 pos;
        glm::vec3 color;
        glm::vec2 texCoord;
    
        static VkVertexInputBindingDescription getBindingDescription();
    
        static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
    
        bool operator==(const Vertex& other) const;
    };

    class DefaultPipeline: public TGraphicsPipeline<Vertex> {
    public:
        DefaultPipeline(const VulkanInstance& vk): TGraphicsPipeline{vk, true} {}
        ~DefaultPipeline() = default;
    
        void updateDescriptorSet(
            unsigned meshIndex,

            const TUniformBuffer<DefaultUBO>& uniformBuffer,
            const Texture& texture,
            const TextureSampler& textureSampler
        );

    private:
    
        std::vector<char> getVertShaderCode() override {
            return readFile(DEFAULT_VERT_SHADER_SRC);
        }
        std::vector<char> getFragShaderCode() override {
            return readFile(DEFAULT_FRAG_SHADER_SRC);
        }

        VkDescriptorSetLayout createDescriptorSetLayout() override;
    
        VkDescriptorPool createDescriptorPool() override;

    };

    using VertexArray = TVertexArray<Vertex>;
    std::unique_ptr<VertexArray> loadModel(const VulkanInstance& vk, const VkCommandPool commandPool, std::filesystem::path filepath);

}
