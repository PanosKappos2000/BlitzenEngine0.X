#pragma once

#include "Core/math.h"

namespace BlitzenEngine
{
    class Camera
    {
    public:
        void Init(float deltaTime, const uint32_t* windowWidth, const uint32_t* windowHeight);

        void MoveCamera(float deltaTime);

        void RotateCamera(float yawMovement, float pitchMovement, float deltaTime);

        inline const glm::mat4& GetViewMatrix() const {return m_viewMatrix;}
        inline const glm::mat4& GetProjectionMatrix() const {return m_projectionMatrix;}

        void ChangeProjectionMatrix(float fovY, float zNear, float zFar);

        //The parameter of these function should only take the values -1, 0 and 1 (I should add some protection against undesirable values)
        inline void SetVelocityX(int8_t x) {m_velocity.x = static_cast<float>(x);}
        inline void SetVelocityY(int8_t y) {m_velocity.y = static_cast<float>(y);}
        inline void SetVelocityZ(int8_t z) {m_velocity.z = static_cast<float>(z);}
    
    public:

        const uint32_t* m_pWindowWidth;
        const uint32_t* m_pWindowHeight;

    private:
        
        //Dictates how much the camera should move each frame(if at all)
        glm::vec3 m_velocity = {0.f, 0.f, 0.f};

        //Used to set the translation of the view matrix
        glm::vec3 m_position = glm::vec3(0.f, 0.f, -5.f);

        //Dictate the current direction of the camera and sets the current rotation matrix
        float m_pitch = 0;
        float m_yaw = 0;
        glm::mat4 m_rotationMatrix = glm::mat4(1.f);

        //Data for how fast the camera should move and change direction
        float m_sensitivity = 10.f;
        float m_speed = 100.f;

        //Used to set up the projection matrix with glm::perspective
        float m_fovY = 45.0f;
        float m_zNear = 10000.f;
        float m_zFar = 0.1f;

        //Transforms all objects to view coordinates
        glm::mat4 m_viewMatrix = glm::mat4(1.f);
        //Transforms all objects to clip coordinates
        glm::mat4 m_projectionMatrix = glm::mat4(1.f);
    };
}