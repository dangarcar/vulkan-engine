#pragma once

#include "vulkan/VulkanTypes.h"
#include "vulkan/VulkanHelpers.hpp"

#include <vector>

namespace fly {
    
    template<typename Vertex_t>
    class TVertexArray {
    public:
        TVertexArray(std::shared_ptr<VulkanInstance> vk, VkCommandPool commandPool, std::vector<Vertex_t>&& vertices, std::vector<uint32_t>&& indices): 
            vertices{vertices}, indices{indices}, vk{vk}
        {
            createVertexBuffer(commandPool);
            createIndexBuffer(commandPool);
        }
        
        ~TVertexArray() {
            vmaDestroyBuffer(vk->allocator, this->vertexBuffer, this->vertexAlloc);
            vmaDestroyBuffer(vk->allocator, this->indexBuffer, this->indexAlloc);
        }

        VkBuffer getVertexBuffer() const { return vertexBuffer; }
        VkBuffer getIndexBuffer() const { return indexBuffer; }

        const std::vector<Vertex_t>& getVertices() const { return vertices; }
        const std::vector<uint32_t>& getIndices() const { return indices; }

    private:
        std::vector<Vertex_t> vertices;
        std::vector<uint32_t> indices;
        VkBuffer vertexBuffer, indexBuffer;
        VmaAllocation vertexAlloc, indexAlloc;
    
        std::shared_ptr<VulkanInstance> vk;

    private:
        void createVertexBuffer(VkCommandPool commandPool) {            
            VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();
            
            VkBufferCreateInfo bufferInfo{};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = bufferSize;
            bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;


            VkBuffer stagingBuffer;
            VmaAllocation stagingAlloc;
            {
                VmaAllocationCreateInfo allocCreateInfo{};
                allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
                allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
                
                VmaAllocationInfo stagingAllocInfo;
                vmaCreateBuffer(vk->allocator, &bufferInfo, &allocCreateInfo, &stagingBuffer, &stagingAlloc, &stagingAllocInfo);
                memcpy(stagingAllocInfo.pMappedData, vertices.data(), bufferSize);                
            }


            {
                VmaAllocationCreateInfo allocCreateInfo{};
                allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
                
                bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;            

                vmaCreateBuffer(vk->allocator, &bufferInfo, &allocCreateInfo, &this->vertexBuffer, &this->vertexAlloc, nullptr);
            }

    
            {
                auto commandBuffer = beginSingleTimeCommands(vk, commandPool);
                
                copyBuffer(
                    commandBuffer,                    
                    stagingBuffer, 
                    this->vertexBuffer, 
                    bufferSize
                );

                endSingleTimeCommands(vk, commandPool, commandBuffer);
            }
    
            vmaDestroyBuffer(vk->allocator, stagingBuffer, stagingAlloc);
        }    
        
        void createIndexBuffer(VkCommandPool commandPool) {
            VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();
            
            VkBufferCreateInfo bufferInfo{};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = bufferSize;
            bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;


            VkBuffer stagingBuffer;
            VmaAllocation stagingAlloc;
            {
                VmaAllocationCreateInfo allocCreateInfo{};
                allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
                allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
                
                VmaAllocationInfo stagingAllocInfo;
                vmaCreateBuffer(vk->allocator, &bufferInfo, &allocCreateInfo, &stagingBuffer, &stagingAlloc, &stagingAllocInfo);
                memcpy(stagingAllocInfo.pMappedData, indices.data(), bufferSize);                
            }


            {
                VmaAllocationCreateInfo allocCreateInfo{};
                allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
                
                bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;            

                vmaCreateBuffer(vk->allocator, &bufferInfo, &allocCreateInfo, &this->indexBuffer, &this->indexAlloc, nullptr);
            }

    
            {
                auto commandBuffer = beginSingleTimeCommands(vk, commandPool);
                
                copyBuffer(
                    commandBuffer,                    
                    stagingBuffer, 
                    this->indexBuffer, 
                    bufferSize
                );

                endSingleTimeCommands(vk, commandPool, commandBuffer);
            }
    
            vmaDestroyBuffer(vk->allocator, stagingBuffer, stagingAlloc);
        }
    
    };

}

