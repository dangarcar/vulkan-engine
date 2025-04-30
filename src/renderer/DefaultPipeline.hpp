#pragma once

#include "GraphicsPipeline.hpp"

#include "VertexArray.hpp"
#include "Texture.hpp"

#include "../Utils.hpp"

static const char* const FRAG_SHADER_SRC = "shaders/frag.spv";
static const char* const VERT_SHADER_SRC = "shaders/vert.spv";

namespace fly {
    
    class DefaultPipeline: public GraphicsPipeline<Vertex> {
    public:
        DefaultPipeline(const VulkanInstance& vk): GraphicsPipeline{vk} {}
        virtual ~DefaultPipeline() {}
    
        void updateDescriptorSet(
            unsigned meshIndex,

            const std::vector<VkBuffer>& uniformBuffers,
            size_t uboSize,
            const Texture& texture,
            const TextureSampler& textureSampler
        );

    private:
    
        std::vector<char> getVertShaderCode() override {
            return readFile(VERT_SHADER_SRC);
        }
        std::vector<char> getFragShaderCode() override {
            return readFile(FRAG_SHADER_SRC);
        }

        VkDescriptorSetLayout createDescriptorSetLayout() override;
    
        VkDescriptorPool createDescriptorPool() override;

    };

}
