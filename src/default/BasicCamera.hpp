#pragma once

#include <glm/glm.hpp>

namespace fly {

    class Window;

    class BasicCamera {
    public:
        static constexpr glm::vec3 UP = {0, 1, 0};

    public:
        BasicCamera(Window& window, glm::vec3 pos, float fov = 45, glm::vec3 lookDir = {0, 0, 1});
        ~BasicCamera();

        void update(float dt);

        glm::mat4 getProjection() const { return this->proj; }
        glm::mat4 getView() const { return this->view; } 

        glm::vec3 getPos() const { return this->pos; }

    private:
        glm::mat4 proj, view;
        glm::vec3 pos, lookDir;
        float fov, speed = 1.0f, mouseSpeed = 500.0f, scrollSpeed = 600.0f;
        bool used = false;

        Window& window;

    };

}
