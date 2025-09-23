#pragma once

#include <glm/glm.hpp>

#include "Texture.hpp"
#include "vulkan/VulkanConstants.h"
#include "TBuffer.hpp"

namespace fly {

    //Deferred shader abstract class with virtual methods
    class DeferredShader {
    public:
        virtual void updateShader(
            std::shared_ptr<Texture> hdrColorTexture,
            std::shared_ptr<Texture> albedoSpecTexture, 
            std::shared_ptr<Texture> positionsTexture, 
            std::shared_ptr<Texture> normalsTexture,
            std::shared_ptr<Texture> pickingTexture
        ) = 0;

        virtual void run(VkCommandBuffer commandBuffer, uint32_t currentFrame);
        virtual ~DeferredShader();
    
    protected:
        DeferredShader(std::shared_ptr<VulkanInstance> vk): vk{vk} {}
        
        std::shared_ptr<VulkanInstance> vk;
        std::shared_ptr<Texture> outputTexture;

        std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> descriptorSets;
        DescriptorSetLayout descriptorSetLayout;
        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        VkPipeline pipeline = VK_NULL_HANDLE;
    };


    //DEFAULT DEFERRED SHADER
    struct DeferredUBO {
        glm::vec4 viewPos;
        glm::vec4 lightPos, lightColor;
    };

    class DefaultDeferredShader: public DeferredShader {
    private:
        static constexpr const char* DEFERRED_SHADER_SRC = "vulkan-engine/shaders/bin/deferred.comp.spv";

    public:
        DefaultDeferredShader(std::shared_ptr<VulkanInstance> vk);

        virtual ~DefaultDeferredShader() = default;
        
        void updateShader(
            std::shared_ptr<Texture> hdrColorTexture,
            std::shared_ptr<Texture> albedoSpecTexture, 
            std::shared_ptr<Texture> positionsTexture, 
            std::shared_ptr<Texture> normalsTexture,
            std::shared_ptr<Texture> pickingTexture
        ) override;

        void setUbo(DeferredUBO ubo, uint32_t currentFrame) { uniformBuffer->updateBuffer(ubo, currentFrame); }

    private:
        std::unique_ptr<fly::TBuffer<DeferredUBO>> uniformBuffer;

    };

}