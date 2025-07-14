#pragma once

#include <vector>
#include <vulkan/vulkan.h>
#include <bitset>
#include <glm/glm.hpp>

class GLFWwindow;

namespace fly {

    enum class MouseButton {
        LEFT = 0, MIDDLE = 1, RIGHT = 2
    };

    class Window {
    public:
        Window(const char* name);
        Window(const char* name, int width, int height);
        ~Window();
        
        void handleInput();
        void resizeFramebuffer();

        VkSurfaceKHR createSurface(VkInstance instance);

        std::vector<const char*> getRequiredExtensions();
        
        void toggleFullscreen();

        int getWidth() const { return width; }
        int getHeight() const { return height; }
        glm::vec2 getExtent() const { return {width, height}; }

        bool isFramebufferResized() const { return framebufferResized; }
        bool shouldClose() const;

        GLFWwindow* getGlfwWindow() { return window; }

        bool isKeyPressed(int glfwKey) const { return keyStates[glfwKey]; }
        bool keyJustPressed(int glfwKey) const { return oldKeyStates[glfwKey] == false && keyStates[glfwKey] == true; }
        bool keyJustReleased(int glfwKey) const { return oldKeyStates[glfwKey] == true && keyStates[glfwKey] == false; }

        glm::vec2 getMousePos() const { return mousePos; }
        glm::vec2 getMouseDelta() const { return mousePos - oldMousePos; }

        bool isMouseBtnPressed(MouseButton btn) const { return btnPress[static_cast<int>(btn)]; }
        bool mouseClicked(MouseButton btn) const { return oldBtnPress[static_cast<int>(btn)] == false && btnPress[static_cast<int>(btn)] == true; }
        bool mouseReleased(MouseButton btn) const { return oldBtnPress[static_cast<int>(btn)] == true && btnPress[static_cast<int>(btn)] == false; }

        float getScroll() const { return scroll - oldScroll; }

    private:
        GLFWwindow* window;
        const char* name;
        bool framebufferResized = false;
        int width, height;
        int windowedWidth, windowedHeight;
        int posX, posY;

        static constexpr int NUM_KEYS = 1024;
        std::bitset<NUM_KEYS> oldKeyStates = false, keyStates = false;

        glm::vec2 oldMousePos, mousePos;
        float oldScroll = 0, scroll = 0;

        std::bitset<3> btnPress = false, oldBtnPress = false;

        static void framebufferResizeCallback(GLFWwindow* glfwWindow, int width, int height);
        static void keyCallback(GLFWwindow* glfwWindow, int key, int scancode, int action, int mods);
        static void mouseButtonCallback(GLFWwindow* glfwWindow, int button, int action, int mods);
        static void scrollCallback(GLFWwindow* glfwWindow, double xoffset, double yoffset);
    };

    
}