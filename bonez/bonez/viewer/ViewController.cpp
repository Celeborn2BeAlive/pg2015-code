#include "ViewController.hpp"

#include <bonez/maths/maths.hpp>

#include <GLFW/glfw3.h>

namespace BnZ {

bool ViewController::update(float elapsedTime) {

    Vec3f m_FrontVector = -Vec3f(m_RcpViewMatrix[2]);
    Vec3f m_LeftVector = -Vec3f(m_RcpViewMatrix[0]);
    Vec3f m_UpVector = Vec3f(m_RcpViewMatrix[1]);
    Vec3f position = Vec3f(m_RcpViewMatrix[3]);

    bool hasMoved = false;
    Vec3f localTranslationVector(0.f);

    float lateralAngleDelta = 0.f;

    if (glfwGetKey(m_pWindow, GLFW_KEY_W)) {
        localTranslationVector += m_fSpeed * elapsedTime * m_FrontVector;
    }

    if (glfwGetKey(m_pWindow, GLFW_KEY_A)) {
        localTranslationVector += m_fSpeed * elapsedTime * m_LeftVector;
    }

    if (glfwGetKey(m_pWindow, GLFW_KEY_Q)) {
        lateralAngleDelta += 0.01f;
    }

    if (glfwGetKey(m_pWindow, GLFW_KEY_E)) {
        lateralAngleDelta -= 0.01f;
    }

    if (glfwGetKey(m_pWindow, GLFW_KEY_S)) {
        localTranslationVector -= m_fSpeed * elapsedTime * m_FrontVector;
    }
    
    if (glfwGetKey(m_pWindow, GLFW_KEY_D)) {
        localTranslationVector -= m_fSpeed * elapsedTime * m_LeftVector;
    }

    if (glfwGetKey(m_pWindow, GLFW_KEY_UP)) {
        localTranslationVector += m_fSpeed * elapsedTime * m_UpVector;
    }

    if (glfwGetKey(m_pWindow, GLFW_KEY_DOWN)) {
        localTranslationVector -= m_fSpeed * elapsedTime * m_UpVector;
    }

    position += localTranslationVector;

    if (localTranslationVector != Vec3f(0.f)) {
        hasMoved = true;
    }

    if(glfwGetMouseButton(m_pWindow, GLFW_MOUSE_BUTTON_LEFT) && !m_LeftButtonPressed) {
        m_LeftButtonPressed = true;
        glfwGetCursorPos(m_pWindow, &m_LastCursorPosition.x, &m_LastCursorPosition.y);
    } else if(!glfwGetMouseButton(m_pWindow, GLFW_MOUSE_BUTTON_LEFT) && m_LeftButtonPressed) {
        m_LeftButtonPressed = false;
    }

    auto newRcpViewMatrix = m_RcpViewMatrix;

    if(lateralAngleDelta) {
        newRcpViewMatrix = rotate(newRcpViewMatrix, lateralAngleDelta, Vec3f(0, 0, 1));

        hasMoved = true;
    }

    if(m_LeftButtonPressed) {
        Vec2d cursorPosition;
        glfwGetCursorPos(m_pWindow, &cursorPosition.x, &cursorPosition.y);
        Vec2d delta = cursorPosition - m_LastCursorPosition;

        m_LastCursorPosition = cursorPosition;

        if(delta.x || delta.y) {
            newRcpViewMatrix = rotate(newRcpViewMatrix, -0.01f * float(delta.x), Vec3f(0, 1, 0));
            newRcpViewMatrix = rotate(newRcpViewMatrix, -0.01f * float(delta.y), Vec3f(1, 0, 0));

            hasMoved = true;
        }
    }

    m_FrontVector = -Vec3f(newRcpViewMatrix[2]);
    m_LeftVector = -Vec3f(newRcpViewMatrix[0]);
    m_UpVector = cross(m_FrontVector, m_LeftVector);

    if(hasMoved) {
        setViewMatrix(lookAt(position, position + m_FrontVector, m_UpVector));
    }

    return hasMoved;
}

}
