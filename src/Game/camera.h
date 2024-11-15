#include "BlitzenVulkan/vulkanRenderer.h"

namespace BlitzenEngine
{
    class Camera
    {
    public:
        void Init(glm::mat4* pMatrix, float* pDeltaTime);

        void MoveCamera(const glm::vec3& velocity, float yawMovement, float pitchMovement);

    private:
        glm::vec3 m_position = glm::vec3(0.f, 0.f, -5.f);

        glm::mat4* m_pViewMatrix;
        float* m_pDeltaTime;

        float m_pitch = 0;
        float m_yaw = 0;

        float m_sensitivity = 10.f;
        float m_speed = 35.f;
    };
}