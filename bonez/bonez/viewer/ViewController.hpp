#pragma once

#include <bonez/maths/maths.hpp>

struct GLFWwindow;

namespace BnZ {

class Camera;

class ViewController {
public:
    ViewController(GLFWwindow* window):
        m_pWindow(window) {
    }

    void setSpeed(float speed) {
        m_fSpeed = speed;
    }

    float getSpeed() const {
        return m_fSpeed;
    }

    void increaseSpeed(float delta) {
        m_fSpeed += delta;
        m_fSpeed = max(m_fSpeed, 0.f);
    }

    float getCameraSpeed() const {
        return m_fSpeed;
    }

    bool update(float elapsedTime);

    void setViewMatrix(const glm::mat4& viewMatrix) {
        m_ViewMatrix = viewMatrix;
        m_RcpViewMatrix = glm::inverse(viewMatrix);
    }

    const glm::mat4& getViewMatrix() const {
        return m_ViewMatrix;
    }

private:
    GLFWwindow* m_pWindow = nullptr;
    float m_fSpeed = 0.f;
    bool m_LeftButtonPressed = false;
    Vec2d m_LastCursorPosition;

    glm::mat4 m_ViewMatrix = glm::mat4(1);
    glm::mat4 m_RcpViewMatrix = glm::mat4(1);
};

}
