#pragma once

#include "Renderer2d.hpp"
#include "TextRenderer.hpp"

class GLFWwindow;

namespace fly {

    class UIRenderer {
    public:

        UIRenderer(GLFWwindow* window, std::shared_ptr<VulkanInstance> vk);
        ~UIRenderer();
        
        void renderTexture(
            const Texture& texture, 
            const TextureSampler& textureSampler, 
            glm::vec2 origin, 
            glm::vec2 size, 
            bool centre = false,
            glm::vec4 modColor = {1,1,1,1}
        ) {
            renderer2d.renderTexture(texture, textureSampler, origin, size, centre, modColor);
        }

        void renderQuad(
            glm::vec2 origin, 
            glm::vec2 size, 
            glm::vec4 color,
            bool centre = false
        ) {
            this->renderer2d.renderQuad(origin, size, color, centre);            
        }

        void renderText(
            const std::string& fontName, 
            const std::string& str, 
            glm::vec2 origin, 
            Align align, 
            float size, 
            glm::vec4 color
        ) {
            this->textRenderer.renderText(fontName, str, origin, align, size, color);
        }

        void loadFont(
            const std::string& fontName, 
            std::filesystem::path fontImg, 
            std::filesystem::path fontJson
        ) {
            this->textRenderer.loadFont(fontName, fontImg, fontJson, this->uiCommandPool);
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