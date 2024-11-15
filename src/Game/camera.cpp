#include "camera.h"

namespace BlitzenEngine
{
    void Camera::Init(glm::mat4* pMatrix, float* pDeltaTime)
    {
        //Storing the delta time, as it will be constantly used in camera functions, I'll probably do this differently in the future
        m_pDeltaTime = pDeltaTime;
        //Storing the view matrix, so that camera function can change it directly, without communicating with the renderer
        m_pViewMatrix = pMatrix;

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

        *m_pViewMatrix = glm::inverse(translationMatrix * rotationMatrix);
    }
}