#pragma once

#include "vulkan/VulkanTypes.h"
#include <glm/glm.hpp>

#include <vector>
#include <filesystem>

namespace fly {

    struct Vertex {
        glm::vec3 pos;
        glm::vec3 color;
        glm::vec2 texCoord;
    
        static VkVertexInputBindingDescription getBindingDescription();
    
        static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
    
        bool operator==(const Vertex& other) const;
    };
    
    template<typename Vertex_t>
    class TVertexArray {
    public:
        TVertexArray(const VulkanInstance& vk, const VkCommandPool commandPool, std::vector<Vertex_t>&& vertices, std::vector<uint32_t>&& indices);
        
        ~TVertexArray() {
            vkDestroyBuffer(vk.device, this->indexBuffer, nullptr);
            vkFreeMemory(vk.device, this->indexBufferMemory, nullptr);

            vkDestroyBuffer(vk.device, this->vertexBuffer, nullptr);
            vkFreeMemory(vk.device, this->vertexBufferMemory, nullptr);
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
    
        const VulkanInstance& vk;

    private:
        void createVertexBuffer(const VkCommandPool commandPool);
        void createIndexBuffer(const VkCommandPool commandPool);
    };

    using VertexArray = TVertexArray<Vertex>;

    std::unique_ptr<VertexArray> loadModel(const VulkanInstance& vk, const VkCommandPool commandPool, std::filesystem::path filepath);

}

