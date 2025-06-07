#include "Window.hpp"

#include "renderer/vulkan/VulkanConstants.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <imgui.h>

#include <stdexcept>

namespace fly {

    Window::Window(int width, int height): width(width), height(height) {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        this->window = glfwCreateWindow(width, height, "Vulkan", nullptr, nullptr);
        glfwSetWindowUserPointer(this->window, this);

        glfwSetFramebufferSizeCallback(this->window, this->framebufferResizeCallback);
        glfwSetKeyCallback(this->window, this->keyCallback);
        glfwSetScrollCallback(this->window, this->scrollCallback);
        glfwSetMouseButtonCallback(this->window, this->mouseButtonCallback);
    }

    Window::~Window() {
        glfwDestroyWindow(this->window);
        glfwTerminate();
    }

    bool Window::shouldClose() const {
        return glfwWindowShouldClose(this->window);
    }

    void Window::resizeFramebuffer() { 
        this->framebufferResized = false; 

        glfwGetFramebufferSize(this->window, &width, &height);
        while(width == 0 || height == 0) {
            glfwGetFramebufferSize(this->window, &width, &height);
            glfwWaitEvents();
        }
    }

    VkSurfaceKHR Window::createSurface(VkInstance instance) {
        VkSurfaceKHR surface;
        if (glfwCreateWindowSurface(instance, this->window, nullptr, &surface) != VK_SUCCESS) {
            throw std::runtime_error("failed to create window surface!");
        }

        return surface;
    }

    void Window::handleInput() {
        this->oldKeyStates = std::move(this->keyStates);        
        this->oldBtnPress = std::move(this->btnPress);
        this->oldScroll = scroll;

        glfwPollEvents();

        this->oldMousePos = this->mousePos;
        double xpos, ypos;
        glfwGetCursorPos(this->window, &xpos, &ypos);
        this->mousePos = glm::vec2(xpos, ypos);
    }


    void Window::framebufferResizeCallback(GLFWwindow* glfwWindow, int width, int height) {
        auto window = reinterpret_cast<Window*>(glfwGetWindowUserPointer(glfwWindow)); //WTF is this? Black magic?
        window->framebufferResized = true;
        window->width = width;
        window->height = height;
    }

    void Window::keyCallback(GLFWwindow* glfwWindow, int key, int scancode, int action, int mods) {
        (void) scancode; (void) mods;
        
        auto window = reinterpret_cast<Window*>(glfwGetWindowUserPointer(glfwWindow));
        
        if(action == GLFW_PRESS || action == GLFW_REPEAT)
            window->keyStates[key] = true;
        else
            window->keyStates[key] = false;
    }

    void Window::mouseButtonCallback(GLFWwindow* glfwWindow, int button, int action, int mods) {
        if(ImGui::GetIO().WantCaptureMouse)
            return;

        (void) mods;

        auto window = reinterpret_cast<Window*>(glfwGetWindowUserPointer(glfwWindow));

        switch(button) {
        case GLFW_MOUSE_BUTTON_RIGHT:
            if(action == GLFW_PRESS) 
                window->btnPress[static_cast<int>(MouseButton::RIGHT)] = true;
            if(action == GLFW_RELEASE) 
                window->btnPress[static_cast<int>(MouseButton::RIGHT)] = false;
        break;
        
        case GLFW_MOUSE_BUTTON_LEFT:
            if(action == GLFW_PRESS) 
                window->btnPress[static_cast<int>(MouseButton::LEFT)] = true;
            if(action == GLFW_RELEASE) 
                window->btnPress[static_cast<int>(MouseButton::LEFT)] = false;
        break;
        
        case GLFW_MOUSE_BUTTON_MIDDLE:
            if(action == GLFW_PRESS) 
                window->btnPress[static_cast<int>(MouseButton::MIDDLE)] = true;
            if(action == GLFW_RELEASE) 
                window->btnPress[static_cast<int>(MouseButton::MIDDLE)] = false;
        break;

        default:
        break;
        }
    }

    void Window::scrollCallback(GLFWwindow* glfwWindow, double xoffset, double yoffset) {
        (void) xoffset;

        auto window = reinterpret_cast<Window*>(glfwGetWindowUserPointer(glfwWindow));
        window->scroll += yoffset;
    }


    std::vector<const char*> Window::getRequiredExtensions() {
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if (enableValidationLayers) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        return extensions;
    }



}