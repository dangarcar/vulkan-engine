#pragma once

#include "vulkan/VulkanTypes.h"
#include "vulkan/VulkanHelpers.hpp"

#include <vector>

namespace fly {
    
    template<typename Vertex_t>
    class TVertexArray {
    public:
        TVertexArray(std::shared_ptr<VulkanInstance> vk, const VkCommandPool commandPool, std::vector<Vertex_t>&& vertices, std::vector<uint32_t>&& indices): 
            vertices{vertices}, indices{indices}, vk{vk}
        {
            createVertexBuffer(commandPool);
            createIndexBuffer(commandPool);
        }
        
        ~TVertexArray() {
            vkDestroyBuffer(vk->device, this->indexBuffer, nullptr);
            vkFreeMemory(vk->device, this->indexBufferMemory, nullptr);

            vkDestroyBuffer(vk->device, this->vertexBuffer, nullptr);
            vkFreeMemory(vk->device, this->vertexBufferMemory, nullptr);
        }

        VkBuffer getVertexBuffer() const { return vertexBuffer; }
        VkBuffer getIndexBuffer() const { return indexBuffer; }

        const std::vector<Vertex_t>& getVertices() const { return vertices; }
        const std::vector<uint32_t>& getIndices() const { return indices; }

    private:
        std::vector<Vertex_t> vertices;
        std::vector<uint32_t> indices;
        VkBuffer vertexBuffer;
        VkDeviceMemory vertexBufferMemory;
        VkBuffer indexBuffer;
        VkDeviceMemory indexBufferMemory;
    
        std::shared_ptr<VulkanInstance> vk;

    private:
        void createVertexBuffer(const VkCommandPool commandPool) {
            VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();
            
            VkBuffer stagingBuffer;
            VkDeviceMemory stagingBufferMemory;
            createBuffer(
                vk,
                bufferSize, 
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                stagingBuffer, 
                stagingBufferMemory
            );
    
            void* data;
            vkMapMemory(vk->device, stagingBufferMemory, 0, bufferSize, 0, &data);
            memcpy(data, vertices.data(), (size_t) bufferSize);
            vkUnmapMemory(vk->device, stagingBufferMemory);
    
            createBuffer(
                vk,
                bufferSize, 
                VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
                this->vertexBuffer, 
                this->vertexBufferMemory
            );
    
            {
                auto commandBuffer = beginSingleTimeCommands(vk, commandPool);
                
                copyBuffer(
                    commandBuffer,                    
                    stagingBuffer, 
                    this->vertexBuffer, 
                    bufferSize
                );

                endSingleTimeCommands(vk, commandPool, commandBuffer, QueueType::TRANSFER);
            }
    
            vkDestroyBuffer(vk->device, stagingBuffer, nullptr);
            vkFreeMemory(vk->device, stagingBufferMemory, nullptr);
        }    
        
        void createIndexBuffer(const VkCommandPool commandPool) {
            VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();
    
            VkBuffer stagingBuffer;
            VkDeviceMemory stagingBufferMemory;
            createBuffer(
                vk,
                bufferSize, 
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                stagingBuffer, 
                stagingBufferMemory
            );
    
            void* data;
            vkMapMemory(vk->device, stagingBufferMemory, 0, bufferSize, 0, &data);
            memcpy(data, indices.data(), (size_t) bufferSize);
            vkUnmapMemory(vk->device, stagingBufferMemory);
    
            createBuffer(
                vk,
                bufferSize, 
                VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, 
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
                this->indexBuffer, 
                this->indexBufferMemory
            );
    
            {
                auto commandBuffer = beginSingleTimeCommands(vk, commandPool);
                copyBuffer(
                    commandBuffer,
                    
                    stagingBuffer, 
                    this->indexBuffer, 
                    bufferSize
                );
                endSingleTimeCommands(vk, commandPool, commandBuffer, QueueType::TRANSFER);
            }
    
            vkDestroyBuffer(vk->device, stagingBuffer, nullptr);
            vkFreeMemory(vk->device, stagingBufferMemory, nullptr);
        }
    
    };

}

