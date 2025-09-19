#pragma once

#include <glm/glm.hpp>

#include "Texture.hpp"
#include "vulkan/VulkanConstants.h"
#include "TBuffer.hpp"

namespace fly {

    struct DeferredUBO {
        glm::vec4 viewPos;
        glm::vec4 lightPos, lightColor;
    };

    class DeferredShader {
    private:
        static constexpr const char* DEFERRED_SHADER_SRC = "vulkan-engine/shaders/bin/deferred.comp.spv";

    public:
        DeferredShader(std::shared_ptr<VulkanInstance> vk);

        ~DeferredShader();
        
        void updateShader(
            std::shared_ptr<Texture> hdrColorTexture,
            std::shared_ptr<Texture> albedoSpecTexture, 
            std::shared_ptr<Texture> positionsTexture, 
            std::shared_ptr<Texture> normalsTexture
        );

        void run(VkCommandBuffer commandBuffer, uint32_t currentFrame);
        void setUbo(DeferredUBO ubo, uint32_t currentFrame) { uniformBuffer->updateBuffer(ubo, currentFrame); }

    private:
        std::shared_ptr<VulkanInstance> vk;
        std::shared_ptr<Texture> outputTexture;
        std::unique_ptr<fly::TBuffer<DeferredUBO>> uniformBuffer;

        std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> descriptorSets;
        DescriptorSetLayout descriptorSetLayout;
        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        VkPipeline pipeline = VK_NULL_HANDLE;

    };

}