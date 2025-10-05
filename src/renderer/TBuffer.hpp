#pragma once

#include "vulkan/VulkanTypes.h"
#include "vulkan/VulkanConstants.h"
#include <cstring>
#include <memory>

namespace fly {

    template<typename T>
    class TBuffer {
    public:
        TBuffer(std::shared_ptr<VulkanInstance> vk, VkBufferUsageFlagBits usage, T value = {}): vk{vk} {
            VkDeviceSize bufferSize = sizeof(T);

            for(int i=0; i<MAX_FRAMES_IN_FLIGHT; ++i) {
                VkBufferCreateInfo bufferCreateInfo{};
                bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
                bufferCreateInfo.size = bufferSize;
                bufferCreateInfo.usage = usage;

                VmaAllocationCreateInfo bufferCreateAllocInfo = {};
                bufferCreateAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
                bufferCreateAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
                
                vmaCreateBuffer(
                    vk->allocator, 
                    &bufferCreateInfo, 
                    &bufferCreateAllocInfo, 
                    &this->buffers[i], 
                    &this->buffersAlloc[i], 
                    &this->buffersInfo[i]
                );

                std::memcpy(this->buffersInfo[i].pMappedData, &value, sizeof(T));
            }
        }

        ~TBuffer() {
            for(int i=0; i<MAX_FRAMES_IN_FLIGHT; ++i) {
                vmaDestroyBuffer(vk->allocator, this->buffers[i], this->buffersAlloc[i]);
            }
        }

        VkBuffer getBuffer(uint32_t currentFrame) const { return buffers[currentFrame]; }

        constexpr size_t getSize() const { return sizeof(T); }

        void updateBuffer(const T& value, uint32_t currentFrame) {
            std::memcpy(this->buffersInfo[currentFrame].pMappedData, &value, sizeof(T));
        }

        void updateBuffer(const T& value, size_t bytes, uint32_t currentFrame) {
            std::memcpy(this->buffersInfo[currentFrame].pMappedData, &value, bytes);
        }


    private:
        std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT> buffers;
        std::array<VmaAllocation, MAX_FRAMES_IN_FLIGHT> buffersAlloc;
        std::array<VmaAllocationInfo, MAX_FRAMES_IN_FLIGHT> buffersInfo;
    
        std::shared_ptr<VulkanInstance> vk;

    };

}