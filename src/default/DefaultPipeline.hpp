#pragma once

#include "../renderer/TGraphicsPipeline.hpp"
#include "../renderer/TBuffer.hpp"
#include "../renderer/TVertexArray.hpp"
#include "../renderer/Texture.hpp"

#include "../Utils.hpp"

#include <glm/glm.hpp>

static const char* const DEFAULT_FRAG_SHADER_SRC = "vulkan-engine/shaders/bin/default.frag.spv";
static const char* const DEFAULT_VERT_SHADER_SRC = "vulkan-engine/shaders/bin/default.vert.spv";

namespace fly {
    
    struct DefaultUBO {
        alignas(16) glm::mat4 model;
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 proj;
    };

    struct Vertex {
        glm::vec3 pos;
        glm::vec3 normal;
        glm::vec2 texCoord;
    
        static VkVertexInputBindingDescription getBindingDescription();
    
        static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
    
        bool operator==(const Vertex& other) const;
    };

    class DefaultPipeline: public TGraphicsPipeline<Vertex> {
    public:
        DefaultPipeline(std::shared_ptr<VulkanInstance> vk): TGraphicsPipeline{vk, DEPTH_TEST_ENABLED | DEFERRED_ENABLED | BACK_CULLING_ENABLED} {}
        ~DefaultPipeline() = default;
    
        void updateDescriptorSet(
            unsigned meshIndex,

            const TBuffer<DefaultUBO>& uniformBuffer,
            const Texture& texture,
            const TextureSampler& textureSampler
        );

    private:
        std::vector<char> getVertShaderCode() override { return readFile(DEFAULT_VERT_SHADER_SRC); }
        std::vector<char> getFragShaderCode() override { return readFile(DEFAULT_FRAG_SHADER_SRC); }
        DescriptorSetLayout createDescriptorSetLayout() override;

    };

    using VertexArray = TVertexArray<Vertex>;
    std::unique_ptr<VertexArray> loadModel(std::shared_ptr<VulkanInstance> vk, const VkCommandPool commandPool, std::filesystem::path filepath);

}
