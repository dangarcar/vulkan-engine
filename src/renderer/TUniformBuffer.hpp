#pragma once

#include "vulkan/VulkanTypes.h"
#include "vulkan/VulkanConstants.h"
#include "vulkan/VulkanHelpers.hpp"

namespace fly {

    template<typename T>
    class TUniformBuffer {
    public:
        TUniformBuffer(const VulkanInstance& vk): vk{vk} {
            createUniformBuffer();
        }
        TUniformBuffer(T&& ubo, const VulkanInstance& vk): ubo{ubo}, vk{vk} {
            createUniformBuffer();
        }

        ~TUniformBuffer() {
            for(int i=0; i<MAX_FRAMES_IN_FLIGHT; ++i) {
                vkDestroyBuffer(vk.device, this->uniformBuffers[i], nullptr);
                vkFreeMemory(vk.device, this->uniformBuffersMemory[i], nullptr);
            }
        }

        VkBuffer getBuffer(uint32_t currentFrame) const { return uniformBuffers[currentFrame]; }

        size_t getSize() const { return sizeof(ubo); }

        const T& getUBO() const { return ubo; }

        void updateUBO(const T& ubo, uint32_t currentFrame) {
            memcpy(this->uniformBuffersMapped[currentFrame], &ubo, sizeof(ubo));
        }

    
    private:
        T ubo;

        std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT> uniformBuffers;
        std::array<VkDeviceMemory, MAX_FRAMES_IN_FLIGHT> uniformBuffersMemory;
        std::array<void*, MAX_FRAMES_IN_FLIGHT> uniformBuffersMapped;
    
        const VulkanInstance& vk;

    private:
        void createUniformBuffer() {
            VkDeviceSize bufferSize = sizeof(T);
    
            for(int i=0; i<MAX_FRAMES_IN_FLIGHT; ++i) {
                createBuffer(
                    this->vk,
                    bufferSize, 
                    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                    this->uniformBuffers[i], 
                    this->uniformBuffersMemory[i]
                );
    
                vkMapMemory(vk.device, this->uniformBuffersMemory[i], 0, bufferSize, 0, &this->uniformBuffersMapped[i]);
            }
        }

    };

}