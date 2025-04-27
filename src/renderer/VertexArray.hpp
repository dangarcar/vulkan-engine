#pragma once

#include "vulkan/VulkanHelpers.hpp"
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

#include <memory>
#include <vector>
#include <filesystem>
#include <array>

namespace fly {

    struct Vertex {
        glm::vec3 pos;
        glm::vec3 color;
        glm::vec2 texCoord;
    
        static VkVertexInputBindingDescription getBindingDescription();
    
        static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions();
    
        bool operator==(const Vertex& other) const;
    };
    
    class VertexArray {
    public:
        VertexArray(const VulkanInstance& vk, const VkCommandPool commandPool, std::vector<Vertex>&& vertices, std::vector<uint32_t>&& indices);
            
        ~VertexArray();

        VkBuffer getVertexBuffer() const { return vertexBuffer; }
        VkBuffer getIndexBuffer() const { return indexBuffer; }

        const std::vector<Vertex>& getVertices() const { return vertices; }
        const std::vector<uint32_t>& getIndices() const { return indices; }

    private:
        std::vector<Vertex> vertices;
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

    std::unique_ptr<VertexArray> loadModel(const VulkanInstance& vk, const VkCommandPool commandPool, std::filesystem::path filepath);

}

