#pragma once

#include "vulkan/VulkanTypes.h"
#include "vulkan/VulkanConstants.h"
#include "vulkan/VulkanHelpers.hpp"
#include <cstring>

namespace fly {

    template<typename T>
    class TBuffer {
    public:
        TBuffer(std::shared_ptr<VulkanInstance> vk, VkBufferUsageFlagBits usage): vk{vk} {
            VkDeviceSize bufferSize = sizeof(T);

            for(int i=0; i<MAX_FRAMES_IN_FLIGHT; ++i) {
                createBuffer(
                    this->vk,
                    bufferSize, 
                    usage, 
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                    this->buffers[i], 
                    this->buffersMemory[i]
                );
    
                vkMapMemory(vk->device, this->buffersMemory[i], 0, bufferSize, 0, &this->buffersMapped[i]);
            }
        }

        ~TBuffer() {
            for(int i=0; i<MAX_FRAMES_IN_FLIGHT; ++i) {
                vkDestroyBuffer(vk->device, this->buffers[i], nullptr);
                vkFreeMemory(vk->device, this->buffersMemory[i], nullptr);
            }
        }

        VkBuffer getBuffer(uint32_t currentFrame) const { return buffers[currentFrame]; }

        constexpr size_t getSize() const { return sizeof(T); }

        void updateBuffer(const T& value, uint32_t currentFrame) {
            std::memcpy(this->buffersMapped[currentFrame], &value, sizeof(T));
        }

        T getBufferValueAsync(uint32_t currentFrame) {
            T value;
            std::memcpy(&value, this->buffersMapped[currentFrame], sizeof(T));
            return value;
        }


    private:
        std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT> buffers;
        std::array<VkDeviceMemory, MAX_FRAMES_IN_FLIGHT> buffersMemory;
        std::array<void*, MAX_FRAMES_IN_FLIGHT> buffersMapped;
    
        std::shared_ptr<VulkanInstance> vk;

    };

}