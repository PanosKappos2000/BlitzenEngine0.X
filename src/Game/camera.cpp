#include "camera.h"

namespace BlitzenEngine
{
    void Camera::Init(float deltaTime, const uint32_t* pWindowWidth, const uint32_t* pWindowHeight)
    {
        m_pWindowWidth = pWindowWidth;
        m_pWindowHeight = pWindowHeight;

        RotateCamera(0.f, 0.f, deltaTime);
        MoveCamera(deltaTime);
    }

    void Camera::RotateCamera(float yawMovement, float pitchMovement, float deltaTime)
    {
        if(yawMovement < 100.f && yawMovement > -100.f)
        {
            m_yaw += (yawMovement * m_sensitivity * deltaTime) / 100.f;
        }
        if(pitchMovement < 100.f && pitchMovement > -100.f)
        {
            m_pitch -= (pitchMovement * m_sensitivity * deltaTime) / 100.f;
        }

        glm::quat pitchRotation = glm::angleAxis(m_pitch, glm::vec3(1.0f, 0.f, 0.f));
        glm::quat yawRotation = glm::angleAxis(m_yaw, glm::vec3(0.f, -1.f, 0.f));
        m_rotationMatrix = glm::toMat4(yawRotation) * glm::toMat4(pitchRotation);
    }

    void Camera::MoveCamera(float deltaTime)
    {
        m_position += glm::vec3(m_rotationMatrix * glm::vec4(m_velocity * m_speed * deltaTime, 0.f));
        glm::mat4 translationMatrix = glm::translate(glm::mat4(1.f), m_position);

        m_viewMatrix = glm::inverse(translationMatrix * m_rotationMatrix);
        m_projectionMatrix = glm::perspective(glm::radians(m_fovY), static_cast<float>(*m_pWindowWidth) / static_cast<float>(*m_pWindowHeight), m_zNear, m_zFar);
    }
}