#pragma once

#include <glm/glm.hpp>

#include "../renderer/DeferredShader.hpp"
#include "../renderer/TBuffer.hpp"


namespace fly {

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