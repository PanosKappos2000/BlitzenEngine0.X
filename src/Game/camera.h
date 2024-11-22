#pragma once

#include "Core/math.h"

namespace BlitzenEngine
{
    class Camera
    {
    public:
        void Init(float* pDeltaTime, const int* windowWidth, const int* windowHeight);

        void MoveCamera(const glm::vec3& velocity, float yawMovement, float pitchMovement);

        inline const glm::mat4& GetViewMatrix() const {return m_viewMatrix;}
        inline const glm::mat4& GetProjectionMatrix() const {return m_projectionMatrix;}

        void ChangeProjectionMatrix(float fovY, float zNear, float zFar);
    
    public:

        const int* m_pWindowWidth;
        const int* m_pWindowHeight;

    private:

        //Keeps track of the delta time of the engine so that movement is the same speed no matter the fps
        float* m_pDeltaTime;

        //Used to set the translation of the view matrix
        glm::vec3 m_position = glm::vec3(0.f, 0.f, -5.f);

        //Used to set the rotation of the view matrix
        float m_pitch = 0;
        float m_yaw = 0;

        //Data for how fast the camera should move and change direction
        float m_sensitivity = 10.f;
        float m_speed = 35.f;

        //Used to set up the projection matrix with glm::perspective
        float m_fovY = 45.0f;
        float m_zNear = 10000.f;
        float m_zFar = 0.1f;

        //Transforms all objects to view coordinates
        glm::mat4 m_viewMatrix;
        //Transforms all objects to clip coordinates
        glm::mat4 m_projectionMatrix;
    };
}