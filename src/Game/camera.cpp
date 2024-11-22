#include "camera.h"

namespace BlitzenEngine
{
    void Camera::Init(float* pDeltaTime, const int* pWindowWidth, const int* pWindowHeight)
    {
        //Storing the delta time, as it will be constantly used in camera functions, I'll probably do this differently in the future
        m_pDeltaTime = pDeltaTime;
        m_pWindowWidth = pWindowWidth;
        m_pWindowHeight = pWindowHeight;

        MoveCamera(glm::vec3(0.f, 0.f, 0.f), 0.f , 0.f);
    }

    void Camera::MoveCamera(const glm::vec3& velocity, float yawMovement, float pitchMovement)
    {
        /*
        The cursor is active outside the window right now so, it is capable of teleporting the camera with huge values.
        This is undesirable, so it needs to be guarded against with these if statements
        */
        if(yawMovement < 100.f && yawMovement > -100.f)
        {
            m_yaw += (yawMovement * m_sensitivity * (*m_pDeltaTime)) / 100.f;
        }
        if(pitchMovement < 100.f && pitchMovement > -100.f)
        {
            m_pitch -= (pitchMovement * m_sensitivity * (*m_pDeltaTime)) / 100.f;
        }

        glm::quat pitchRotation = glm::angleAxis(m_pitch, glm::vec3(1.0f, 0.f, 0.f));
        glm::quat yawRotation = glm::angleAxis(m_yaw, glm::vec3(0.f, -1.f, 0.f));
        glm::mat4 rotationMatrix = glm::toMat4(yawRotation) * glm::toMat4(pitchRotation);

        m_position += glm::vec3(rotationMatrix * glm::vec4(velocity * (*m_pDeltaTime) * m_speed, 0.f));
        glm::mat4 translationMatrix = glm::translate(glm::mat4(1.f), m_position);

        m_viewMatrix = glm::inverse(translationMatrix * rotationMatrix);
        m_projectionMatrix = glm::perspective(glm::radians(m_fovY), static_cast<float>(*m_pWindowWidth) / static_cast<float>(*m_pWindowHeight), m_zNear, m_zFar);
    }
}