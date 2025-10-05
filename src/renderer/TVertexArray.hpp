#pragma once

#include "vulkan/VulkanTypes.h"
#include "vulkan/VulkanHelpers.hpp"

#include <vector>
#include <bit>
#include <optional>


namespace fly {

    struct BufferWithStaging {
        size_t count=0, capacity=0;
        VkBuffer buffer=nullptr, stagingBuffer=nullptr;
        VmaAllocation alloc, stagingAlloc;
        VmaAllocationInfo stagingInfo; 
    };


    template<typename T>
    void copyData(
        BufferWithStaging& buffer, 
        std::shared_ptr<VulkanInstance> vk, 
        VkCommandPool commandPool, 
        const std::vector<T>& data
    ) {
        FLY_ASSERT(!data.empty(), "There cannot be zero items");
        
        auto commandBuffer = beginSingleTimeCommands(vk, commandPool);
        
        VkDeviceSize bufferSize = sizeof(T) * data.size();
        memcpy(buffer.stagingInfo.pMappedData, data.data(), bufferSize);          
        copyBuffer(
            commandBuffer,                    
            buffer.stagingBuffer, 
            buffer.buffer, 
            bufferSize
        );
        
        endSingleTimeCommands(vk, commandPool, commandBuffer);
    }
    

    template<typename T>
    void createBuffer(
        BufferWithStaging& buffer, 
        std::shared_ptr<VulkanInstance> vk, 
        VkCommandPool commandPool, 
        const std::vector<T>& data, 
        VkBufferUsageFlags mainUsage, 
        bool constMeshData
    ) {            
        buffer.capacity = std::bit_ceil(data.size());

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = sizeof(T) * buffer.capacity;
        
        {
            VmaAllocationCreateInfo allocCreateInfo{};
            allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
            allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
            
            bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            
            vmaCreateBuffer(vk->allocator, &bufferInfo, &allocCreateInfo, &buffer.stagingBuffer, &buffer.stagingAlloc, &buffer.stagingInfo);
        }
        
        {
            VmaAllocationCreateInfo allocCreateInfo{};
            allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
            
            bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | mainUsage;            
            
            vmaCreateBuffer(vk->allocator, &bufferInfo, &allocCreateInfo, &buffer.buffer, &buffer.alloc, nullptr);
        }
        
        copyData(buffer, vk, commandPool, data);

        if(constMeshData) {
            vmaDestroyBuffer(vk->allocator, buffer.stagingBuffer, buffer.stagingAlloc);
            buffer.stagingBuffer = nullptr;
            buffer.stagingAlloc = nullptr;
            buffer.stagingInfo = {};
        }
    }   


    template<typename T>
    [[nodiscard]] std::optional<BufferWithStaging> updateBuffer(
        BufferWithStaging& buffer, 
        std::shared_ptr<VulkanInstance> vk, 
        VkCommandPool commandPool, 
        VkBufferUsageFlags mainUsage, 
        const std::vector<T>& data
    ) {
        buffer.count = data.size();

        if(buffer.count > buffer.capacity) { //New buffer creation                
            auto tmp = buffer;
            createBuffer(buffer, vk, commandPool, data, mainUsage, false);
            return tmp;
        }

        copyData(buffer, vk, commandPool, data);
        return std::nullopt;
    }



    template<typename Vertex_t>
    class TVertexArray {
        using Index_t = uint32_t;
        
        BufferWithStaging vertex, index;
        std::shared_ptr<VulkanInstance> vk;


    public:
        TVertexArray(std::shared_ptr<VulkanInstance> vk, VkCommandPool commandPool, std::vector<Vertex_t> vertices, std::vector<Index_t> indices, bool constMeshData = true): vk{vk} {
            this->vertex.count = vertices.size();
            this->index.count = indices.size();

            if(vertices.size() != 0)
                createBuffer<Vertex_t>(this->vertex, this->vk, commandPool, vertices, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, constMeshData);
            
            if(indices.size() != 0)
                createBuffer<Index_t>(this->index, this->vk, commandPool, indices, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, constMeshData);
        }
        
        ~TVertexArray() {
            if(vertex.buffer)
                vmaDestroyBuffer(vk->allocator, this->vertex.buffer, this->vertex.alloc);
            if(index.buffer)
                vmaDestroyBuffer(vk->allocator, this->index.buffer, this->index.alloc);

            if(vertex.stagingBuffer)
                vmaDestroyBuffer(vk->allocator, this->vertex.stagingBuffer, this->vertex.stagingAlloc);
            if(index.stagingBuffer)
                vmaDestroyBuffer(vk->allocator, this->index.stagingBuffer, this->index.stagingAlloc);
        }

        
        VkBuffer getVertexBuffer() const { return this->vertex.buffer; }
        VkBuffer getIndexBuffer() const { return this->index.buffer; }

        size_t getIndexCount() const { return this->index.count; }
        size_t getVertexCount() const { return this->vertex.count; }

        
        [[nodiscard]] std::optional<BufferWithStaging> updateVertexBuffer(VkCommandPool commandPool, std::vector<Vertex_t> vertices) {
            return updateBuffer<Vertex_t>(this->vertex, this->vk, commandPool, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertices);
        }

        [[nodiscard]] std::optional<BufferWithStaging> updateIndexBuffer(VkCommandPool commandPool, std::vector<Index_t> indices) {
            return updateBuffer<Index_t>(this->index, this->vk, commandPool, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indices);
        }

    };

}

