#pragma once

#include <glm/glm.hpp>

namespace fly {

    class Window;

    class BasicCamera {
    public:
        static constexpr glm::vec3 UP = {0, 0, 1};

    public:
        BasicCamera(glm::vec3 pos, float fov = 45, glm::vec3 lookDir = {0, 1, 0});

        void update(Window& window, float dt);

        glm::mat4 getProjection() const { return this->proj; }
        glm::mat4 getView() const { return this->view; } 

    private:
        glm::mat4 proj, view;
        glm::vec3 pos, lookDir;
        float fov, speed = 1.0f, mouseSpeed = 500.0f, scrollSpeed = 1.0f;
        bool used = false;

    };

}
