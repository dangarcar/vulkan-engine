#pragma once

#include "Renderer2d.hpp"
#include "TextRenderer.hpp"

class GLFWwindow;

namespace fly {

    class UIManager {
    public:

        UIManager(GLFWwindow* window, std::shared_ptr<VulkanInstance> vk);
        ~UIManager();
        
        void renderTexture(
            const Texture& texture, 
            const TextureSampler& textureSampler, 
            glm::vec2 origin, 
            glm::vec2 size, 
            glm::vec2 relOrigin = {0, 0},
            glm::vec2 relSize = {1, 1},
            bool centre = false,
            glm::vec4 modColor = {1,1,1,1},
            int zIndex = 0
        ) {
            this->renderer2d._renderTexture(texture, textureSampler, origin, size, centre, modColor, relOrigin, relSize, true, zIndex);
        }

        void renderQuad(
            glm::vec2 origin, 
            glm::vec2 size, 
            glm::vec4 color,
            bool centre = false,
            int zIndex = 0
        ) {
            this->renderer2d._renderTexture(*renderer2d.nullTexture, *renderer2d.nullTextureSampler, origin, size, centre, color, {0,0}, {1,1}, false, zIndex);         
        }

        void renderText(
            const std::string& str, 
            glm::vec2 origin, 
            Align align, 
            float size, 
            glm::vec3 color,
            int zIndex = 0
        ) {
            this->textRenderer.renderText(str, origin, align, size, color, zIndex);
        }

        void loadFont(
            const std::string& fontName, 
            std::filesystem::path fontImg, 
            std::filesystem::path fontJson,
            VkCommandPool transferCommandPool
        ) {
            this->textRenderer.loadFont(fontName, fontImg, fontJson, transferCommandPool);
        }

        void cleanupSwapchain();
        void recreateOnNewSwapChain();
        
        void recordCommandBuffer(uint32_t imageIndex, uint32_t currentFrame);
        
        void resize(int width, int height);
        
        void setupFrame();
        void render(uint32_t frame);

        VkCommandBuffer getCommandBuffer(uint32_t currentFrame) const { return uiCommandBuffers[currentFrame]; }
        VkSemaphore getRenderFinishedSemaphore(uint32_t currentFrame) const { return uiRenderFinishedSemaphores[currentFrame]; }
        
    private:
        VkRenderPass uiRenderPass;
        VkDescriptorPool uiDescriptorPool;
        std::vector<VkFramebuffer> uiFramebuffers;
        VkCommandPool uiCommandPool;
        std::vector<VkCommandBuffer> uiCommandBuffers;
        std::vector<VkSemaphore> uiRenderFinishedSemaphores;
        
        std::shared_ptr<VulkanInstance> vk;
        std::unique_ptr<Texture> depthTexture;

        Renderer2d renderer2d;
        TextRenderer textRenderer;
        
    private:    
        void initImgui(GLFWwindow* window);
        
        void createDescriptorPool();
        void createRenderPass();
        void createFramebuffers();
        void createSyncObjects();

    };
    
}