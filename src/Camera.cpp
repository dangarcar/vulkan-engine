#include "Camera.hpp"
#include "glm/geometric.hpp"

#include <imgui.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtc/matrix_transform.hpp>

#include <GLFW/glfw3.h>

#include "Window.hpp"

namespace fly {

    BasicCamera::BasicCamera(glm::vec3 pos, float fov, glm::vec3 lookDir): 
        pos{pos}, lookDir{lookDir}, fov{fov}
    {
        
    } 

    void BasicCamera::update(Window& window, float dt) {
        ImGui::SliderFloat("FOV", &this->fov, 10, 170);
        
        ImGui::LabelText("Pos", "%f %f %f", pos.x, pos.y, pos.z);
        ImGui::LabelText("Look dir", "%f %f %f", lookDir.x, lookDir.y, lookDir.z);

        ImGui::SliderFloat("Speed", &this->speed, 0, 10);
        ImGui::SliderFloat("Mouse speed", &this->mouseSpeed, 0, 1000);
        ImGui::SliderFloat("Scroll speed", &this->scrollSpeed, 0, 1000);

        if(window.keyJustPressed(GLFW_KEY_ESCAPE)) used = false; //FIXME:
        if(window.mouseClicked(MouseButton::LEFT)) used = true; //FIXME:

        if(used)
            glfwSetInputMode(window.getGlfwWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        else
            glfwSetInputMode(window.getGlfwWindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);

        auto right = glm::cross(UP, this->lookDir);
        glm::vec3 dpos = {0, 0, 0};
        if(window.isKeyPressed(GLFW_KEY_W)) 
            dpos += glm::normalize(glm::vec3(lookDir.x, lookDir.y, 0));
        else if(window.isKeyPressed(GLFW_KEY_S)) 
            dpos -= glm::normalize(glm::vec3(lookDir.x, lookDir.y, 0));
        if(window.isKeyPressed(GLFW_KEY_A)) 
            dpos += glm::normalize(right);
        else if(window.isKeyPressed(GLFW_KEY_D)) 
            dpos -= glm::normalize(right);
        if(window.isKeyPressed(GLFW_KEY_SPACE)) 
            dpos.z += 1;
        else if(window.isKeyPressed(GLFW_KEY_LEFT_SHIFT)) 
            dpos.z -= 1;

        if(dpos.x != 0 || dpos.y != 0 || dpos.z != 0)
            this->pos += glm::normalize(dpos) * speed * dt;
        
        this->pos += lookDir * window.getScroll() * scrollSpeed * dt;

        if(used) {
            auto dmouse = -window.getMouseDelta() * mouseSpeed * dt * glm::pi<float>();
            auto rotate = glm::mat4(1.0f);
            rotate = glm::rotate(rotate, dmouse.x / window.getWidth(), glm::normalize(glm::cross(lookDir, right)));
            rotate = glm::rotate(rotate, dmouse.y / window.getHeight(), glm::normalize(glm::cross(lookDir, UP)));
            this->lookDir = glm::normalize(glm::mat3(rotate) * lookDir);
        }

        this->view = glm::lookAt(this->pos, this->pos + this->lookDir, UP);
        if(window.getHeight() != 0)  {
            this->proj = glm::perspective(glm::radians(this->fov), window.getWidth() / (float) window.getHeight(), 0.1f, 10.0f);
            this->proj[1][1] *= -1;
        }
    }

}