#pragma once

#include <iostream>
#include <functional>
#include <array>
#include <unordered_map>

#include <GLFW/glfw3.h>

namespace BlitzenEngine
{
    /*--------------------------------------------------------------------------------------------------------
    This is class is made so that glfw(or other windowing APIs) is given a pointer to the main instance of it 
    and can call one of its function pointers whenever an input event needs to be handled. This makes it so
    the application can easily change these, according to the desired game logic
    --------------------------------------------------------------------------------------------------------*/
    class Controller
    {
    public:
        #ifndef NDEBUG
            #define DEFAULT_LAMBDA              [](){std::cout << "Event not handled\n";}
        #else
            #define DEFAULT_LAMBDA              [](){}
        #endif
        #define DEFAULT_LAMBDA_ARRAY        {DEFAULT_LAMBDA, DEFAULT_LAMBDA, DEFAULT_LAMBDA}

        //The constructor will create all default function pointers and put them in the hash maps
        Controller();

        //Holds all the function pointers for key presses
        std::unordered_map<int, std::array<std::function<void()>, 3>> m_KeyFunctionPointers;
        inline void SetKeyPressFunction(int key, int action, std::function<void()> pfn) {
            m_KeyFunctionPointers[key][action] = pfn;
        }

        //Holds the function pointer for cursor movement
        #ifndef NDEBUG
            std::function<void(float, float)> m_pfnCursor = [](float, float){std::cout << "Event not handled\n";};
        #else
            std::function<void(float, float)> m_pfnCursor = [](float, float){};
        #endif
        inline void SetCursorFunctionPointer(std::function<void(float, float)> pfn) { m_pfnCursor = pfn; }
    };
}